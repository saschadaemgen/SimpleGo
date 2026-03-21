/**
 * SimpleGo - smp_tasks.c
 * FreeRTOS task management for multi-task architecture
 *
 * All heavy allocations use PSRAM (SPIRAM) to preserve internal
 * SRAM for TLS/WiFi operations (~40KB needed).
 */

#include "smp_tasks.h"
#include "smp_frame_pool.h"
#include "smp_events.h"
#include "smp_network.h"   // smp_read_block()
#include "smp_types.h"     // SMP_BLOCK_SIZE
#include "smp_contacts.h"  // find_contact_by_recipient_id, contacts_db
#include "smp_storage.h"   // Session 48: smp_storage_delete for pending contact cleanup
#include "smp_servers.h"   // Session 49: SEC-07 fingerprint verification
#include "smp_queue.h"     // our_queue; T4e: queue_reconnect/subscribe/send_key/read_raw/send_ack
#include "reply_queue.h"   // per-contact reply queues
#include "smp_e2e.h"       // smp_e2e_decrypt_reply_message()
#include "smp_agent.h"     // smp_agent_process_message()
#include "smp_crypto.h"    // decrypt_smp_message()
#include "smp_parser.h"    // parse_agent_message()
#include "sodium.h"        // crypto_box_MACBYTES
#include "smp_ack.h"       // smp_send_ack()
#include "smp_peer.h"      // peer_send_chat_message()
#include "ui_chat.h"       // ui_chat_get_last_seq()
#include "ui_manager.h"    // UI_SCREEN_CHAT
#include "smp_handshake.h" // handshake_get_last_msg_id()
#include "smp_ratchet.h"  // ratchet_set_active()
#include "smp_history.h"  // encrypted chat history on SD
#include "smp_rotation.h" // Session 49: Queue Rotation
#include <string.h>
#include <stdio.h>             // Session 48: snprintf for splash status
#include <time.h>          // time(NULL) for history timestamps
#include <sys/socket.h>
#include <unistd.h>            // Session 48: close() for reconnect
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"      // Session 47: Timing analysis (us precision)
#include <inttypes.h>       // PRId64
#include "wifi_manager.h"      // Session 48: wifi_connected for reconnect guard
extern volatile bool wifi_connected;  // Session 48: from wifi_manager.c
#include "mbedtls/entropy.h"   // Session 48: reconnect TLS context
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"


static const char *TAG = "SMP_TASKS";
static const char *TAG_APP = "SMP_APP";  // App Task logging

// Task handles
TaskHandle_t network_task_handle = NULL;
TaskHandle_t ui_task_handle = NULL;

// Ring buffer handles
RingbufHandle_t net_to_app_buf = NULL;
RingbufHandle_t app_to_net_buf = NULL;

// Stored SSL context (set by smp_tasks_start, used by network task later)
static mbedtls_ssl_context *s_ssl = NULL;
static int s_sock_fd = -1;  // Socket FD for timeout control
static uint8_t s_session_id[32];  // session ID for ACK signing

/* Session 48: Auto-reconnect state */
typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int sock;
} reconnect_ctx_t;

static reconnect_ctx_t *s_rctx = NULL;
static volatile bool s_reconnect_needed = false;
static volatile bool s_reconnect_done   = false;

/* Session 49: Queue Rotation - second TLS connection to target server */
static reconnect_ctx_t *s_rot_ctx = NULL;
static uint8_t s_rot_session_id[32] = {0};
static bool s_rot_connected = false;

/* SPKI headers (defined in smp_contacts.c) */
extern const uint8_t ED25519_SPKI_HEADER[12];
extern const uint8_t X25519_SPKI_HEADER[12];

// Post-confirmation state (set by Reply Queue decrypt, read by 42d handler later)
static uint8_t s_peer_sender_auth_key[44];
static uint8_t s_cq_e2e_peer_public[32];   // CQ E2E peer key (after rotation)
static bool s_cq_e2e_peer_valid = false;
static bool s_has_peer_sender_auth = false;
// Per-contact 42d completion: bit N = contact[N] completed handshake
static uint8_t s_42d_bitmap[16] = {0};  // 128 bits = MAX_CONTACTS
// Session 47 2d: Per-contact "scanned notification sent" bitmap
static uint8_t s_scanned_notified[16] = {0};

static inline bool is_42d_done(int idx) {
    if (idx < 0 || idx >= 128) return true;  // Out of range = skip
    return (s_42d_bitmap[idx / 8] >> (idx % 8)) & 1;
}
static inline void mark_42d_done(int idx) {
    if (idx >= 0 && idx < 128) {
        s_42d_bitmap[idx / 8] |= (1 << (idx % 8));
    }
}
// Public API to reset 42d bit on contact delete
void smp_clear_42d(int idx) {
    if (idx >= 0 && idx < 128) {
        s_42d_bitmap[idx / 8] &= ~(1 << (idx % 8));
        s_scanned_notified[idx / 8] &= ~(1 << (idx % 8));
        ESP_LOGI("SMP_TASKS", "42d + scanned bitmap cleared for slot [%d]", idx);
    }
}

// UI Event Queue (protocol -> LVGL thread)
QueueHandle_t app_to_ui_queue = NULL;

// seq <-> msg_id mapping for delivery receipt matching
#define MAX_MSG_MAPPINGS 16

typedef struct {
    uint32_t ui_seq;
    uint64_t msg_id;
    bool active;
} msg_mapping_t;

static msg_mapping_t msg_mappings[MAX_MSG_MAPPINGS] = {0};

// Active contact for sending (replaces hardcoded contacts[0])
static int s_active_contact_idx = 0;

// Deferred NVS save: network task sets this, app task (Internal SRAM) writes
static volatile bool s_contacts_dirty = false;
static volatile int  s_rq_save_pending = -1;   // slot to save, -1 = none

// Bug #30: De-duplication guard - prevents subscribe_all from being queued multiple times
static volatile bool s_subscribe_all_pending = false;

// History load request flag (UI -> App Task)
static volatile int s_history_load_pending = -1;  // slot to load, -1 = none

// Session 48: Pending contact delete (UI -> App Task, NVS-safe)
static volatile int s_pending_delete_slot = -1;

// Shared buffer for history batch (App Task writes, UI timer reads)
history_message_t *smp_history_batch      = NULL;
int                smp_history_batch_count = 0;
int                smp_history_batch_slot  = -1;

// Track which contact slot is currently in handshake (set by add-contact flow)
static volatile int s_handshake_contact_idx = -1;

// Net Task signals KEY completion to App Task
static TaskHandle_t s_app_task_handle = NULL;
#define NOTIFY_KEY_DONE  (1 << 0)

// Helper: log both internal and PSRAM heap
static void log_heap(const char *label)
{
    ESP_LOGI(TAG, "  [%s] Internal: %lu, PSRAM: %lu",
             label,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// --- Task functions ---
// Network task: SSL read loop
// App/UI tasks: command loops

/**
 * Drain all pending frames from SSL socket before sending commands.
 * Without this, pending MSG/END/OK frames cause ERR BLOCK on writes.
 * Reads with short timeout until no more data, forwards everything to ring buffer.
 *
 * @param ssl      SSL context (sock 54 main connection)
 * @param block    Read buffer (SMP_BLOCK_SIZE, PSRAM)
 * @param max_rounds  Max frames to drain (safety limit)
 * @return Number of frames drained
 */
static int drain_pending_frames(mbedtls_ssl_context *ssl, uint8_t *block, int max_rounds)
{
    int drained = 0;
    for (int i = 0; i < max_rounds; i++) {
        int content_len = smp_read_block(ssl, block, 100);  // 100ms short timeout
        if (content_len <= 0) break;  // Timeout or error = socket is clean

        // Forward to App Task via ring buffer (same as main loop)
        // But skip PONG frames (handled inline by network task)
        bool is_pong = false;
        {
            uint8_t *resp = block + 2;
            int p = 0;
            if (p + 3 <= content_len) {
                p += 3;
                if (p < content_len) {
                    int authLen = resp[p++];
                    if (p + authLen <= content_len) {
                        p += authLen;
                        if (p < content_len) {
                            int corrLen = resp[p++];
                            if (p + corrLen <= content_len) {
                                p += corrLen;
                                if (p < content_len) {
                                    int entLen = resp[p++];
                                    if (p + entLen <= content_len) {
                                        p += entLen;
                                        if (p + 4 <= content_len &&
                                            resp[p] == 'P' && resp[p+1] == 'O' &&
                                            resp[p+2] == 'N' && resp[p+3] == 'G') {
                                            is_pong = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (!is_pong) {
            BaseType_t sent = xRingbufferSend(net_to_app_buf, block,
                                               content_len + 2,
                                               pdMS_TO_TICKS(100));
            if (sent != pdTRUE) {
                ESP_LOGW(TAG, "DRAIN: Ring buffer full, frame dropped!");
            }
        }

        drained++;
        ESP_LOGD(TAG, "DRAIN: forwarded frame %d (%d bytes, pong=%d)",
                 drained, content_len, is_pong);
    }
    if (drained > 0) {
        ESP_LOGD(TAG, "DRAIN: %d frames drained before command", drained);
    }
    return drained;
}

static void network_task(void *arg)
{
    ESP_LOGI(TAG, "Network task running on core %d", xPortGetCoreID());
    log_heap("net_task");

    // Allocate block buffer in PSRAM (preserve internal SRAM for TLS/WiFi)
    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    if (!block) {
        ESP_LOGE(TAG, "Network task: Failed to allocate read buffer in PSRAM!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Network task: SSL read loop starting...");

    // Reduce socket timeout for responsive read loop
    {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(s_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ESP_LOGI(TAG, "Network task: socket timeout set to 1s");
    }

    // PING keepalive state
    TickType_t last_ping_tick = 0;
    TickType_t last_pong_tick = 0;
    bool first_pong_logged = false;
    bool first_ping_sent = false;
    bool pong_timeout_warned = false;
    const TickType_t ping_interval_ticks = pdMS_TO_TICKS(30000);  // 30s
    const TickType_t pong_timeout_ticks = pdMS_TO_TICKS(60000);   // 60s warning

    while (1) {
        // Heartbeat (every ~5 min at 1s loop)
        static int loop_count = 0;
        loop_count++;
        if (loop_count % 300 == 0) {
            ESP_LOGI(TAG, "NET: heartbeat #%d", loop_count / 300);
        }

        // === 1. SSL READ (T1, timeout reduced from 5000 to 1000 for T4c) ===
        int content_len = smp_read_block(s_ssl, block, 1000);

        if (content_len > 0) {
            // PONG detection: parse frame before forwarding
            bool is_pong = false;
            {
                uint8_t *resp = block + 2;
                int p = 0;
                if (p + 3 <= content_len) {
                    p += 3;  // txCount + 2 skip bytes
                    if (p < content_len) {
                        int authLen = resp[p++];
                        if (p + authLen <= content_len) {
                            p += authLen;
                            // v7: no sessLen
                            if (p < content_len) {
                                int corrLen = resp[p++];
                                if (p + corrLen <= content_len) {
                                    p += corrLen;
                                    if (p < content_len) {
                                        int entLen = resp[p++];
                                        if (p + entLen <= content_len) {
                                            p += entLen;
                                            if (p + 4 <= content_len &&
                                                resp[p] == 'P' && resp[p+1] == 'O' &&
                                                resp[p+2] == 'N' && resp[p+3] == 'G') {
                                                is_pong = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (is_pong) {
                last_pong_tick = xTaskGetTickCount();
                pong_timeout_warned = false;
                if (!first_pong_logged) {
                    first_pong_logged = true;
                    ESP_LOGI("PING", "First PONG received on sock %d", s_sock_fd);
                }

                // Do NOT forward PONG to App Task
            } else {
                // Forward data frame to App Task
                if (first_ping_sent) {
                    ESP_LOGD("PING", "Data frame on sock %d (content_len=%d)",
                             s_sock_fd, content_len);
                }

                BaseType_t sent = xRingbufferSend(net_to_app_buf, block,
                                                   content_len + 2,
                                                   pdMS_TO_TICKS(1000));
                if (sent != pdTRUE) {
                    ESP_LOGW(TAG, "Network task: Ring buffer full, frame dropped!");
                }
            }
        } else if (content_len == -2) {
            // Timeout - log every 30th
            if (loop_count % 30 == 0) {
                ESP_LOGD(TAG, "NET: timeout (loop %d)", loop_count);
            }
        } else {
            /* Session 48: Connection lost - request reconnect from App Task */
            ESP_LOGW(TAG, "NET: Connection lost (error %d), requesting reconnect...", content_len);
            s_reconnect_needed = true;
            s_reconnect_done   = false;

            /* Poll until App Task completes reconnect */
            while (!s_reconnect_done) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            s_reconnect_needed = false;
            s_reconnect_done   = false;

            /* Reset PING state for fresh connection */
            last_ping_tick     = 0;
            last_pong_tick     = 0;
            first_pong_logged  = false;
            first_ping_sent    = false;
            pong_timeout_warned = false;
            loop_count         = 0;

            /* Set socket timeout on new connection */
            struct timeval tv_new;
            tv_new.tv_sec  = 1;
            tv_new.tv_usec = 0;
            setsockopt(s_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_new, sizeof(tv_new));

            ESP_LOGI(TAG, "NET: Reconnected, resuming SSL read loop on sock %d", s_sock_fd);
            continue;
        }

        // === 2. COMMAND CHANNEL: Process App Task commands (T4c) ===
        size_t cmd_size = 0;
        void *cmd_item = xRingbufferReceive(app_to_net_buf, &cmd_size, 0);  // Non-blocking!

        while (cmd_item) {
            // Drain pending frames BEFORE every write to prevent ERR BLOCK
            drain_pending_frames(s_ssl, block, 10);

            if (cmd_size == sizeof(net_cmd_t)) {
                net_cmd_t *cmd = (net_cmd_t *)cmd_item;

                switch (cmd->cmd) {
                    case NET_CMD_SEND_ACK:
                        ESP_LOGI(TAG, "NET: Executing ACK command");
                        smp_send_ack(s_ssl, block, s_session_id,
                                     cmd->recipient_id, cmd->recipient_id_len,
                                     cmd->msg_id, cmd->msg_id_len,
                                     cmd->rcv_auth_secret);
                        break;

                    case NET_CMD_SUBSCRIBE_ALL:
                        ESP_LOGI(TAG, "NET: Executing SUBSCRIBE_ALL command");
                        subscribe_all_contacts(s_ssl, block, s_session_id);
                        s_subscribe_all_pending = false;  // Bug #30: reset dedup guard
                        break;

                    case NET_CMD_ADD_CONTACT: {
                        ESP_LOGI(TAG, "NET: Creating contact '%s'", cmd->contact_name);
                        contacts_nvs_deferred = true;
                        int slot = add_contact(s_ssl, block, s_session_id,
                                               cmd->contact_name);
                        contacts_nvs_deferred = false;
                        if (slot >= 0) {
                            s_contacts_dirty = true;  // App task will save
                            s_handshake_contact_idx = slot;  // Track for 42d
                            // Session 47 2d: Reset scanned notification for this slot
                            s_scanned_notified[slot / 8] &= ~(1 << (slot % 8));
                            ESP_LOGI(TAG, "NET: Contact '%s' created in slot [%d]",
                                     cmd->contact_name, slot);
                            // Session 49 Bug #32: Restored subscribe_all after contact creation
                            subscribe_all_contacts(s_ssl, block, s_session_id);

                            // Create per-contact reply queue
                            int rq_ret = reply_queue_create(s_ssl, block, s_session_id, slot);
                            if (rq_ret == 0) {
                                ESP_LOGI(TAG, "NET: Reply queue created for slot [%d]", slot);
                                s_rq_save_pending = slot;  // deferred NVS save from app task
                                // Session 49 Bug #32: Restored subscribe_all after RQ creation
                                subscribe_all_contacts(s_ssl, block, s_session_id);
                            } else {
                                ESP_LOGE(TAG, "NET: Reply queue creation FAILED for [%d]! ret=%d", slot, rq_ret);
                            }

                            // Show QR for new contact
                            char invite[1024];
                            if (get_invite_link_for_slot(slot,
                                    our_queue.server_key_hash,
                                    our_queue.server_host,
                                    our_queue.server_port,
                                    invite, sizeof(invite))) {
                                smp_notify_ui_show_qr(invite);
                                smp_notify_ui_status("Scan to connect");
                                // Fix 1: Log invite link for desktop app connection
                                // Use printf (no tag prefix) for clean copy-paste
                                printf("\n");
                                printf("========== INVITE LINK ==========\n");
                                printf("%s\n", invite);
                                printf("=================================\n");
                                printf("\n");
                            }
                        } else {
                            ESP_LOGE(TAG, "NET: Failed to create contact '%s'",
                                     cmd->contact_name);
                            smp_notify_ui_status("Contact creation failed");
                        }
                        break;
                    }

                    case NET_CMD_SEND_KEY: {
                        ESP_LOGI(TAG, "NET: Executing SEND_KEY for contact [%d]", cmd->rq_slot);

                        // === 35c Fix: KEY secures the REPLY QUEUE, not Contact Queue ===
                        // The Confirmation tells the phone: "send to this Reply Queue".
                        // KEY registers the phone's sender_auth_key on that Reply Queue.
                        // Without KEY on the Reply Queue, phone cannot send HELLO there.
                        const uint8_t *key_rcv_id;
                        int key_rcv_id_len;
                        const uint8_t *key_auth_private;

                        if (cmd->rq_slot < 0) {
                            // Legacy: use global our_queue (reply queue for slot 0)
                            ESP_LOGI(TAG, "NET: SEND_KEY using legacy our_queue (reply queue)");
                            key_rcv_id = our_queue.rcv_id;
                            key_rcv_id_len = our_queue.rcv_id_len;
                            key_auth_private = our_queue.rcv_auth_private;
                        } else {
                            // Use REPLY QUEUE credentials for this contact
                            reply_queue_t *key_rq = reply_queue_get(cmd->rq_slot);
                            if (!key_rq || !key_rq->valid) {
                                ESP_LOGE(TAG, "NET: RQ[%d] not valid for KEY!", cmd->rq_slot);
                                break;
                            }
                            key_rcv_id = key_rq->rcv_id;
                            key_rcv_id_len = key_rq->rcv_id_len;
                            key_auth_private = key_rq->rcv_auth_private;
                            ESP_LOGI(TAG, "NET: SEND_KEY using REPLY QUEUE RQ[%d] (rcvId len=%d, rcvId=%02x%02x%02x%02x...)",
                                     cmd->rq_slot, key_rcv_id_len,
                                     key_rcv_id[0], key_rcv_id[1], key_rcv_id[2], key_rcv_id[3]);
                        }

                        // Build KEY command body: corrId + entityId(rcvId) + "KEY " + lenPrefix + peer_auth_SPKI
                        uint8_t key_body[128];
                        int kp = 0;

                        // corrId = 24 random bytes
                        key_body[kp++] = 24;
                        uint8_t key_corr[24];
                        esp_fill_random(key_corr, 24);
                        memcpy(&key_body[kp], key_corr, 24);
                        kp += 24;

                        // entityId = CONTACT queue rcvId (Bug 1 fix!)
                        key_body[kp++] = (uint8_t)key_rcv_id_len;
                        memcpy(&key_body[kp], key_rcv_id, key_rcv_id_len);
                        kp += key_rcv_id_len;

                        // Command: "KEY "
                        key_body[kp++] = 'K';
                        key_body[kp++] = 'E';
                        key_body[kp++] = 'Y';
                        key_body[kp++] = ' ';

                        // === Bug 2 Fix: smpEncode @ByteString = 1-byte length prefix + data ===
                        // Haskell: smpEncode senderKey = lenPrefix(1B) + SPKI(44B)
                        // Wire: [0x2C][44 bytes SPKI] = 45 bytes total
                        key_body[kp++] = (uint8_t)cmd->peer_auth_key_len;  // Length prefix (should be 44 = 0x2C)
                        memcpy(&key_body[kp], cmd->peer_auth_key, cmd->peer_auth_key_len);
                        kp += cmd->peer_auth_key_len;

                        int key_body_len = kp;

                        // Sign: sessIdLen(1) + sessionId + body (1-byte prefix, matches working SUB)
                        uint8_t key_sign[1 + 32 + 128];
                        int ks = 0;
                        key_sign[ks++] = 32;
                        memcpy(&key_sign[ks], s_session_id, 32);
                        ks += 32;
                        memcpy(&key_sign[ks], key_body, key_body_len);
                        ks += key_body_len;

                        uint8_t key_sig[64];
                        crypto_sign_detached(key_sig, NULL, key_sign, ks,
                                             key_auth_private);

                        // Build transmission: sigLen + sig + body (v7: no sessionId on wire)
                        uint8_t key_trans[256];
                        int kt = 0;
                        key_trans[kt++] = 64;  // sigLen
                        memcpy(&key_trans[kt], key_sig, 64);
                        kt += 64;
                        memcpy(&key_trans[kt], key_body, key_body_len);
                        kt += key_body_len;

                        int ret = smp_write_command_block(s_ssl, block, key_trans, kt);
                        if (ret != 0) {
                            ESP_LOGE(TAG, "NET: KEY send failed for contact [%d]!", cmd->rq_slot);
                        } else {
                            // Drain for OK
                            int resp_len = smp_read_block(s_ssl, block, 5000);
                            if (resp_len > 0) {
                                uint8_t *kr = block + 2;
                                int krp = 0;
                                krp++;    // txCount
                                krp += 2; // skip
                                int kaLen = kr[krp++]; krp += kaLen;
                                int kcLen = kr[krp++]; krp += kcLen;
                                int keLen = kr[krp++]; krp += keLen;
                                if (krp + 1 < resp_len &&
                                    kr[krp] == 'O' && kr[krp+1] == 'K') {
                                    ESP_LOGI(TAG, "");
                                    ESP_LOGI(TAG, "      +----------------------------------------------+");
                                    ESP_LOGI(TAG, "      |  KEY ACCEPTED for contact [%d]!              |", cmd->rq_slot);
                                    ESP_LOGI(TAG, "      +----------------------------------------------+");
                                    // Signal App Task that KEY was accepted
                                    if (s_app_task_handle) {
                                        xTaskNotify(s_app_task_handle, NOTIFY_KEY_DONE, eSetBits);
                                    }
                                } else {
                                    // Signal even on failure (App Task must not hang)
                                    if (s_app_task_handle) {
                                        xTaskNotify(s_app_task_handle, NOTIFY_KEY_DONE, eSetBits);
                                    }
                                }
                            } else {
                                ESP_LOGW(TAG, "NET: No KEY response for contact [%d] (timeout)", cmd->rq_slot);
                                // Signal even on failure (App Task must not hang)
                                if (s_app_task_handle) {
                                    xTaskNotify(s_app_task_handle, NOTIFY_KEY_DONE, eSetBits);
                                }
                            }
                        }
                        break;
                    }
                }
            } else {
                ESP_LOGW(TAG, "NET: Invalid command size %d (expected %d)",
                         (int)cmd_size, (int)sizeof(net_cmd_t));
            }
            vRingbufferReturnItem(app_to_net_buf, cmd_item);

            // Check for more commands
            cmd_item = xRingbufferReceive(app_to_net_buf, &cmd_size, 0);
        }

        // === 3. PING KEEPALIVE ===
        {
            TickType_t now = xTaskGetTickCount();

            if (now - last_ping_tick >= ping_interval_ticks) {
                // Drain pending frames before PING write
                drain_pending_frames(s_ssl, block, 10);

                // SMP PING: [sigLen=0][corrIdLen=24][24B random corrId][entityIdLen=0]["PING"]
                uint8_t ping_trans[30];
                int pp = 0;
                ping_trans[pp++] = 0;     // no signature
                ping_trans[pp++] = 24;    // corrId length = 0x18
                uint8_t ping_corr[24];
                esp_fill_random(ping_corr, 24);
                memcpy(&ping_trans[pp], ping_corr, 24);
                pp += 24;
                ping_trans[pp++] = 0;     // no entityId
                ping_trans[pp++] = 'P';
                ping_trans[pp++] = 'I';
                ping_trans[pp++] = 'N';
                ping_trans[pp++] = 'G';

                smp_write_command_block(s_ssl, block, ping_trans, pp);
                last_ping_tick = now;
                first_ping_sent = true;
                pong_timeout_warned = false;

                ESP_LOGI("PING", "PING sent on sock %d (tick=%lu)",
                         s_sock_fd, (unsigned long)now);
            }

            // Warn once if no PONG within 60s
            if (first_ping_sent && !pong_timeout_warned &&
                last_pong_tick < last_ping_tick &&
                now - last_ping_tick >= pong_timeout_ticks) {
                ESP_LOGW("PING", "No PONG for 60s! Server may have dropped sub. sock=%d",
                         s_sock_fd);
                pong_timeout_warned = true;
            }
        }
    }

    ESP_LOGW(TAG, "Network task: SSL read loop ended, cleaning up");
    heap_caps_free(block);
    vTaskDelete(NULL);
}

// === App Task command helpers ===

// Helper: Send ACK via Ring Buffer to Network Task
static void app_send_ack(const uint8_t *recipient_id, int recipient_id_len,
                         const uint8_t *msg_id, int msg_id_len,
                         const uint8_t *rcv_auth_secret)
{
    net_cmd_t cmd = {0};
    cmd.cmd = NET_CMD_SEND_ACK;
    memcpy(cmd.recipient_id, recipient_id, recipient_id_len);
    cmd.recipient_id_len = recipient_id_len;
    memcpy(cmd.msg_id, msg_id, msg_id_len);
    cmd.msg_id_len = msg_id_len;
    memcpy(cmd.rcv_auth_secret, rcv_auth_secret, 64);

    if (xRingbufferSend(app_to_net_buf, &cmd, sizeof(cmd), pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG_APP, "APP: Failed to send ACK command to Network Task!");
    } else {
        ESP_LOGI(TAG_APP, "APP: ACK command queued for Network Task");
    }
}

/* Session 49 Bug #32: Restored app_request_subscribe_all().
 * Removing this in Session 48 broke multi-contact handshakes.
 * Boot-subscribe stays removed (smp_connect handles it).
 * SUB on already-subscribed queue is noop (Evgeny confirmed). */
static void app_request_subscribe_all(void)
{
    net_cmd_t cmd = {0};
    cmd.cmd = NET_CMD_SUBSCRIBE_ALL;

    if (xRingbufferSend(app_to_net_buf, &cmd, sizeof(cmd), pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG_APP, "APP: Failed to send SUBSCRIBE command!");
    } else {
        ESP_LOGI(TAG_APP, "APP: SUBSCRIBE_ALL command queued");
    }
}

// Request new contact creation via Network Task
int smp_request_add_contact(const char *name)
{
    if (!name || !name[0]) {
        ESP_LOGE(TAG_APP, "APP: add_contact: empty name");
        return -1;
    }

    int free_count = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) free_count++;
    }
    if (free_count == 0) {
        ESP_LOGE(TAG_APP, "APP: All %d contact slots full!", MAX_CONTACTS);
        return -1;
    }

    net_cmd_t cmd = {0};
    cmd.cmd = NET_CMD_ADD_CONTACT;
    strncpy(cmd.contact_name, name, sizeof(cmd.contact_name) - 1);

    if (!app_to_net_buf) {
        ESP_LOGE(TAG_APP, "APP: Task infrastructure not ready - cannot add contact yet");
        return -1;
    }

    if (xRingbufferSend(app_to_net_buf, &cmd, sizeof(cmd), pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG_APP, "APP: Failed to send ADD_CONTACT command!");
        return -1;
    }

    ESP_LOGI(TAG_APP, "APP: ADD_CONTACT '%s' queued (%d slots free)", name, free_count);
    return 0;
}

/* Session 48: Pending contact abort (back from Connect without scan) */

int smp_get_pending_contact_slot(void)
{
    int slot = s_handshake_contact_idx;
    if (slot < 0 || slot >= MAX_CONTACTS) return -1;
    if (is_42d_done(slot)) return -1;
    if ((s_scanned_notified[slot / 8] >> (slot % 8)) & 1) return -1;
    return slot;
}

void smp_abort_pending_contact(void)
{
    int slot = smp_get_pending_contact_slot();
    if (slot < 0) {
        ESP_LOGI(TAG_APP, "APP: No pending contact to abort");
        return;
    }
    ESP_LOGI(TAG_APP, "APP: Aborting pending contact [%d] '%s'",
             slot, contacts_db.contacts[slot].name);

    /* Deactivate in memory immediately so contacts list refresh
     * (triggered by go_back) never shows the dead contact. */
    contacts_db.contacts[slot].active = false;
    if (contacts_db.num_contacts > 0) contacts_db.num_contacts--;

    /* NVS cleanup deferred to App Task (Internal SRAM stack required) */
    s_pending_delete_slot = slot;
    s_handshake_contact_idx = -1;
}

// ============== UI Notification Helpers ==============

void smp_notify_ui_message(const char *text, bool is_outgoing, uint32_t msg_seq, int contact_idx)
{
    if (!app_to_ui_queue || !text) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_MESSAGE;
    evt.is_outgoing = is_outgoing;
    evt.msg_seq = msg_seq;
    evt.contact_idx = contact_idx;
    evt.status = is_outgoing ? MSG_STATUS_SENDING : MSG_STATUS_DELIVERED;
    strncpy(evt.text, text, sizeof(evt.text) - 1);
    xQueueSend(app_to_ui_queue, &evt, 0);
}

void smp_notify_ui_navigate(uint8_t screen)
{
    if (!app_to_ui_queue) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_NAVIGATE;
    evt.screen = screen;
    xQueueSend(app_to_ui_queue, &evt, 0);
}

void smp_notify_ui_contact(const char *name)
{
    if (!app_to_ui_queue || !name) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_SET_CONTACT;
    strncpy(evt.text, name, sizeof(evt.text) - 1);
    xQueueSend(app_to_ui_queue, &evt, 0);
}

void smp_notify_ui_delivery_status(uint32_t msg_seq, msg_delivery_status_t status)
{
    if (!app_to_ui_queue) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_DELIVERY_STATUS;
    evt.msg_seq = msg_seq;
    evt.status = status;
    xQueueSend(app_to_ui_queue, &evt, 0);
}

// Show QR code on connect screen
void smp_notify_ui_show_qr(const char *invite_link)
{
    if (!app_to_ui_queue || !invite_link) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_SHOW_QR;
    int len = strlen(invite_link);
    if (len >= (int)sizeof(evt.text)) {
        ESP_LOGW("QR", "Invite link truncated: %d > %d", len, (int)sizeof(evt.text) - 1);
        len = sizeof(evt.text) - 1;
    }
    memcpy(evt.text, invite_link, len);
    evt.text[len] = '\0';
    xQueueSend(app_to_ui_queue, &evt, 0);
}

void smp_notify_ui_status(const char *status_text)
{
    if (!app_to_ui_queue || !status_text) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_UPDATE_STATUS;
    strncpy(evt.text, status_text, sizeof(evt.text) - 1);
    xQueueSend(app_to_ui_queue, &evt, 0);
}

// Switch chat view to a different contact (thread-safe)
void smp_notify_ui_switch_contact(int contact_idx, const char *name)
{
    if (!app_to_ui_queue) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_SWITCH_CONTACT;
    evt.contact_idx = contact_idx;
    if (name) strncpy(evt.text, name, sizeof(evt.text) - 1);
    xQueueSend(app_to_ui_queue, &evt, 0);
}

// Session 47 2d: Someone scanned our QR code (first reply queue MSG)
void smp_notify_ui_connect_scanned(int contact_idx)
{
    if (!app_to_ui_queue) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_CONNECT_SCANNED;
    evt.contact_idx = contact_idx;
    xQueueSend(app_to_ui_queue, &evt, 0);

    // TIMING: Messpunkt 1 - Event gefeuert
    int64_t t1 = esp_timer_get_time();
    ESP_LOGI("TIMING", "[1] CONNECT_SCANNED fired at %" PRId64 " us (contact [%d])", t1, contact_idx);
}

// Session 47 2d: Peer display name received during handshake
void smp_notify_ui_connect_name(int contact_idx, const char *name)
{
    if (!app_to_ui_queue || !name) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_CONNECT_NAME;
    evt.contact_idx = contact_idx;
    strncpy(evt.text, name, sizeof(evt.text) - 1);
    xQueueSend(app_to_ui_queue, &evt, 0);
    ESP_LOGI("CONNECT", "UI_EVT_CONNECT_NAME fired for contact [%d]: '%s'", contact_idx, name);
}

// Session 47 2d: Contact handshake fully complete
void smp_notify_ui_connect_done(int contact_idx)
{
    if (!app_to_ui_queue) return;
    ui_event_t evt = {0};
    evt.type = UI_EVT_CONNECT_DONE;
    evt.contact_idx = contact_idx;
    xQueueSend(app_to_ui_queue, &evt, 0);
    ESP_LOGI("CONNECT", "UI_EVT_CONNECT_DONE fired for contact [%d]", contact_idx);
}

// ============== Message ID Mapping for Receipts ==============

void smp_register_msg_mapping(uint32_t ui_seq, uint64_t protocol_msg_id)
{
    // Find free slot (oldest gets overwritten if full)
    int oldest = 0;
    for (int i = 0; i < MAX_MSG_MAPPINGS; i++) {
        if (!msg_mappings[i].active) {
            msg_mappings[i].ui_seq = ui_seq;
            msg_mappings[i].msg_id = protocol_msg_id;
            msg_mappings[i].active = true;
            ESP_LOGI("MAP", "Registered seq=%lu -> msg_id=%llu",
                     (unsigned long)ui_seq, (unsigned long long)protocol_msg_id);
            return;
        }
        oldest = i;
    }
    // Overwrite oldest if full
    msg_mappings[oldest].ui_seq = ui_seq;
    msg_mappings[oldest].msg_id = protocol_msg_id;
    msg_mappings[oldest].active = true;
    ESP_LOGW("MAP", "Mapping table full, overwrote slot %d: seq=%lu -> msg_id=%llu",
             oldest, (unsigned long)ui_seq, (unsigned long long)protocol_msg_id);
}

// Internal: lookup seq by msg_id (for receipt matching, one-shot)
static uint32_t find_seq_by_msg_id(uint64_t protocol_msg_id)
{
    for (int i = 0; i < MAX_MSG_MAPPINGS; i++) {
        if (msg_mappings[i].active && msg_mappings[i].msg_id == protocol_msg_id) {
            uint32_t seq = msg_mappings[i].ui_seq;
            msg_mappings[i].active = false;  // One-shot: free after match
            return seq;
        }
    }
    return 0;  // Not found
}

void smp_notify_receipt_received(uint64_t protocol_msg_id)
{
    uint32_t seq = find_seq_by_msg_id(protocol_msg_id);
    if (seq > 0) {
        ESP_LOGI("RCPT", "vv Receipt matched! msg_id=%llu -> seq=%lu",
                 (unsigned long long)protocol_msg_id, (unsigned long)seq);
        smp_notify_ui_delivery_status(seq, MSG_STATUS_DELIVERED);
    } else {
        ESP_LOGW("RCPT", "Receipt for unknown msg_id=%llu (no mapping)",
                 (unsigned long long)protocol_msg_id);
    }
}

// ============== Active Contact Management ==============

void smp_set_active_contact(int idx)
{
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        s_active_contact_idx = idx;
        // Switch ratchet+handshake so send uses correct keys
        ratchet_set_active(idx);
        handshake_set_active(idx);
        ESP_LOGI(TAG, "Active contact set to [%d] %s (ratchet+handshake switched)",
                 idx, contacts_db.contacts[idx].name);
        // Notify UI to switch chat filter
        smp_notify_ui_switch_contact(idx, contacts_db.contacts[idx].name);
    } else {
        ESP_LOGW(TAG, "Invalid contact index %d (max=%d), keeping [%d]",
                 idx, MAX_CONTACTS, s_active_contact_idx);
    }
}

int smp_get_active_contact(void)
{
    return s_active_contact_idx;
}

// Request history load from App Task (called from UI context)
void smp_request_load_history(int slot)
{
    if (slot >= 0 && slot < 128) {
        s_history_load_pending = slot;
        ESP_LOGD("HISTORY", "Load requested for slot %d", slot);
    }
}

// App logic runs on Main Task (64KB Internal SRAM stack)
// Required because NVS writes crash with PSRAM stack (SPI Flash disables cache)

// --- smp_app_run helpers (static, defined before smp_app_run) ---

static void app_init_run(QueueHandle_t kbd_queue, uint8_t **local_block_out)
{
    s_app_task_handle = xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG_APP, "App logic running on main task, core %d", xPortGetCoreID());
    log_heap("app_run");

    esp_err_t hist_ret = smp_history_init();
    if (hist_ret != ESP_OK)
        ESP_LOGW(TAG_APP, "History init failed -- SD not available?");

    *local_block_out = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE + 2, MALLOC_CAP_SPIRAM);
    if (!*local_block_out) {
        ESP_LOGE(TAG_APP, "Failed to allocate parse buffer in PSRAM!");
        return;
    }

    ESP_LOGI(TAG_APP, "App logic: parse loop starting...");

    // Bug #30: Mark all existing contacts as 42d-done on boot.
    // Without this, s_42d_bitmap is all zeros after reboot and every
    // Reply Queue MSG (including delivery receipts) triggers CONNECT_SCANNED.
    {
        int marked = 0;
        for (int i = 0; i < MAX_CONTACTS; i++) {
            if (contacts_db.contacts[i].active) {
                mark_42d_done(i);
                marked++;
            }
        }
        if (marked > 0)
            ESP_LOGI(TAG_APP, "APP: Marked %d existing contacts as 42d-done (boot)", marked);
    }

    /* Session 48: Removed redundant re-subscribe. smp_connect() already
     * subscribed all queues on the same SSL socket that Network Task
     * now reads from. SUB on an already-subscribed queue is a noop
     * (confirmed by Evgeny). Saves ~5 seconds of boot time. */

    if (contacts_db.num_contacts > 0) {
        contact_t *c = &contacts_db.contacts[s_active_contact_idx];
        ESP_LOGI(TAG_APP, "APP: Sending wildcard ACK for [%s] to clear delivery state", c->name);
        uint8_t empty_msg_id[1] = {0};
        app_send_ack(c->recipient_id, c->recipient_id_len,
                     empty_msg_id, 0, c->rcv_auth_secret);
    }

    ESP_LOGI(TAG_APP, "APP: Boot complete, entering main loop");

    /* Session 49: Initialize rotation module (restores in-progress rotation from NVS) */
    rotation_init();
}

/* ================================================================
 * Session 48: Auto-Reconnect (runs on App Task, Internal SRAM stack)
 *
 * Called from app_process_deferred_work() when Network Task signals
 * s_reconnect_needed. Does full TCP + TLS + SMP handshake + subscribe.
 * Updates s_ssl, s_session_id, s_sock_fd for Network Task to resume.
 * ================================================================ */

static bool app_do_reconnect(void)
{
    ESP_LOGW(TAG_APP, "=== RECONNECT: Starting ===");

    /* Close old socket (server already reset it) */
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }

    /* Free previous reconnect context (2nd+ reconnect) */
    if (s_rctx) {
        mbedtls_ssl_free(&s_rctx->ssl);
        mbedtls_ssl_config_free(&s_rctx->conf);
        mbedtls_ctr_drbg_free(&s_rctx->ctr_drbg);
        mbedtls_entropy_free(&s_rctx->entropy);
        heap_caps_free(s_rctx);
        s_rctx = NULL;
        s_ssl = NULL;
    }

    /* Allocate new context in Internal SRAM (mbedTLS needs DMA-capable memory) */
    s_rctx = heap_caps_calloc(1, sizeof(reconnect_ctx_t), MALLOC_CAP_INTERNAL);
    if (!s_rctx) {
        ESP_LOGE(TAG_APP, "RECONNECT: Failed to allocate TLS context!");
        return false;
    }

    mbedtls_ssl_init(&s_rctx->ssl);
    mbedtls_ssl_config_init(&s_rctx->conf);
    mbedtls_entropy_init(&s_rctx->entropy);
    mbedtls_ctr_drbg_init(&s_rctx->ctr_drbg);
    s_rctx->sock = -1;

    int ret;

    ret = mbedtls_ctr_drbg_seed(&s_rctx->ctr_drbg, mbedtls_entropy_func,
                                 &s_rctx->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG_APP, "RECONNECT: DRBG seed failed: -0x%04x", -ret);
        goto fail;
    }

    /* Step 1: TCP Connect */
    ESP_LOGI(TAG_APP, "RECONNECT: TCP to %s:%d...",
             our_queue.server_host, our_queue.server_port);
    s_rctx->sock = smp_tcp_connect(our_queue.server_host, our_queue.server_port);
    if (s_rctx->sock < 0) {
        ESP_LOGE(TAG_APP, "RECONNECT: TCP connect failed");
        goto fail;
    }

    /* Step 2: TLS 1.3 Handshake */
    ret = mbedtls_ssl_config_defaults(&s_rctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto fail;

    mbedtls_ssl_conf_min_tls_version(&s_rctx->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&s_rctx->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&s_rctx->conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&s_rctx->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&s_rctx->conf, mbedtls_ctr_drbg_random, &s_rctx->ctr_drbg);

    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&s_rctx->conf, alpn_list);

    ret = mbedtls_ssl_setup(&s_rctx->ssl, &s_rctx->conf);
    if (ret != 0) goto fail;

    mbedtls_ssl_set_hostname(&s_rctx->ssl, our_queue.server_host);
    mbedtls_ssl_set_bio(&s_rctx->ssl, &s_rctx->sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&s_rctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG_APP, "RECONNECT: TLS handshake failed: -0x%04x", -ret);
            goto fail;
        }
    }
    ESP_LOGI(TAG_APP, "RECONNECT: TLS OK");

    /* Step 3: SMP ServerHello -> extract session_id */
    {
        uint8_t *hblock = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
        if (!hblock) { ESP_LOGE(TAG_APP, "RECONNECT: block alloc failed"); goto fail; }

        int content_len = smp_read_block(&s_rctx->ssl, hblock, 15000);
        if (content_len < 37) {
            ESP_LOGE(TAG_APP, "RECONNECT: No ServerHello (len=%d)", content_len);
            heap_caps_free(hblock);
            goto fail;
        }

        uint8_t *hello = hblock + 2;
        uint8_t sessIdLen = hello[4];
        if (sessIdLen != 32) {
            ESP_LOGE(TAG_APP, "RECONNECT: Bad sessIdLen %d", sessIdLen);
            heap_caps_free(hblock);
            goto fail;
        }
        memcpy(s_session_id, &hello[5], 32);

        /* Step 4: SMP ClientHello (keyHash) */
        int cert1_off, cert1_len, cert2_off, cert2_len;
        parse_cert_chain(hello, content_len, &cert1_off, &cert1_len,
                         &cert2_off, &cert2_len);

        uint8_t ca_hash[32];
        if (cert2_off >= 0) {
            mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
        } else {
            mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
        }

        // SEC-07: Verify TLS certificate fingerprint against known server list
        smp_server_t *expected_srv = smp_servers_find_by_host(our_queue.server_host);
        if (expected_srv) {
            if (memcmp(ca_hash, expected_srv->key_hash, 32) != 0) {
                ESP_LOGE(TAG_APP, "SEC-07: RECONNECT FINGERPRINT MISMATCH for %s!",
                         our_queue.server_host);
                heap_caps_free(hblock);
                goto fail;
            }
            ESP_LOGI(TAG_APP, "SEC-07: Reconnect fingerprint verified");
        }

        uint8_t client_hello[35];
        int pos = 0;
        client_hello[pos++] = 0x00;
        client_hello[pos++] = 0x07;
        client_hello[pos++] = 32;
        memcpy(&client_hello[pos], ca_hash, 32);
        pos += 32;

        ret = smp_write_handshake_block(&s_rctx->ssl, hblock, client_hello, pos);
        heap_caps_free(hblock);
        if (ret != 0) {
            ESP_LOGE(TAG_APP, "RECONNECT: ClientHello send failed");
            goto fail;
        }
    }
    ESP_LOGI(TAG_APP, "RECONNECT: SMP handshake OK, sessionId=%02x%02x%02x%02x...",
             s_session_id[0], s_session_id[1], s_session_id[2], s_session_id[3]);

    /* Step 5: Update globals for Network Task */
    s_ssl = &s_rctx->ssl;
    s_sock_fd = s_rctx->sock;

    /* Step 6: Re-subscribe all contacts */
    {
        uint8_t *sblock = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
        if (sblock) {
            subscribe_all_contacts(&s_rctx->ssl, sblock, s_session_id);
            heap_caps_free(sblock);
        } else {
            ESP_LOGE(TAG_APP, "RECONNECT: subscribe block alloc failed");
        }
    }

    ESP_LOGI(TAG_APP, "=== RECONNECT: Success (sock %d) ===", s_sock_fd);
    return true;

fail:
    if (s_rctx) {
        if (s_rctx->sock >= 0) close(s_rctx->sock);
        mbedtls_ssl_free(&s_rctx->ssl);
        mbedtls_ssl_config_free(&s_rctx->conf);
        mbedtls_ctr_drbg_free(&s_rctx->ctr_drbg);
        mbedtls_entropy_free(&s_rctx->entropy);
        heap_caps_free(s_rctx);
        s_rctx = NULL;
    }
    s_ssl = NULL;
    s_sock_fd = -1;
    return false;
}

/* ================================================================
 * Session 49: Queue Rotation - Second TLS Connection
 *
 * Establishes a second SMP connection to the target server for
 * NEW/KEY commands during queue rotation. The first (main) connection
 * stays active for receiving messages and sending QADD/QUSE/QTEST.
 *
 * Pattern mirrors app_do_reconnect() but targets a different server.
 * ================================================================ */

extern const int ciphersuites[];  // defined in main.c

static void app_rotation_disconnect(void)
{
    if (s_rot_ctx) {
        if (s_rot_ctx->sock >= 0) close(s_rot_ctx->sock);
        mbedtls_ssl_free(&s_rot_ctx->ssl);
        mbedtls_ssl_config_free(&s_rot_ctx->conf);
        mbedtls_ctr_drbg_free(&s_rot_ctx->ctr_drbg);
        mbedtls_entropy_free(&s_rot_ctx->entropy);
        heap_caps_free(s_rot_ctx);
        s_rot_ctx = NULL;
    }
    s_rot_connected = false;
    sodium_memzero(s_rot_session_id, 32);
    ESP_LOGI(TAG_APP, "[ROT] Second TLS connection closed");
}

static bool app_rotation_connect(void)
{
    const rotation_context_t *ctx = rotation_get_context();
    if (!ctx || ctx->state != ROT_GLOBAL_ACTIVE) return false;

    ESP_LOGI(TAG_APP, "[ROT] Connecting to target server %s:%d...",
             ctx->target_host, ctx->target_port);

    /* Free previous rotation context if any */
    if (s_rot_ctx) {
        app_rotation_disconnect();
    }

    /* Allocate in Internal SRAM (mbedTLS DMA requirement) */
    s_rot_ctx = heap_caps_calloc(1, sizeof(reconnect_ctx_t), MALLOC_CAP_INTERNAL);
    if (!s_rot_ctx) {
        ESP_LOGE(TAG_APP, "[ROT] Failed to allocate TLS context!");
        return false;
    }

    mbedtls_ssl_init(&s_rot_ctx->ssl);
    mbedtls_ssl_config_init(&s_rot_ctx->conf);
    mbedtls_entropy_init(&s_rot_ctx->entropy);
    mbedtls_ctr_drbg_init(&s_rot_ctx->ctr_drbg);
    s_rot_ctx->sock = -1;

    int ret;

    ret = mbedtls_ctr_drbg_seed(&s_rot_ctx->ctr_drbg, mbedtls_entropy_func,
                                 &s_rot_ctx->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG_APP, "[ROT] DRBG seed failed: -0x%04x", -ret);
        goto rot_fail;
    }

    /* Step 1: TCP Connect to target server */
    s_rot_ctx->sock = smp_tcp_connect(ctx->target_host, ctx->target_port);
    if (s_rot_ctx->sock < 0) {
        ESP_LOGE(TAG_APP, "[ROT] TCP connect to %s:%d failed",
                 ctx->target_host, ctx->target_port);
        goto rot_fail;
    }

    /* Step 2: TLS 1.3 Handshake */
    ret = mbedtls_ssl_config_defaults(&s_rot_ctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto rot_fail;

    mbedtls_ssl_conf_min_tls_version(&s_rot_ctx->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&s_rot_ctx->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&s_rot_ctx->conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&s_rot_ctx->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&s_rot_ctx->conf, mbedtls_ctr_drbg_random, &s_rot_ctx->ctr_drbg);

    static const char *rot_alpn[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&s_rot_ctx->conf, rot_alpn);

    ret = mbedtls_ssl_setup(&s_rot_ctx->ssl, &s_rot_ctx->conf);
    if (ret != 0) goto rot_fail;

    mbedtls_ssl_set_hostname(&s_rot_ctx->ssl, ctx->target_host);
    mbedtls_ssl_set_bio(&s_rot_ctx->ssl, &s_rot_ctx->sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&s_rot_ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG_APP, "[ROT] TLS handshake failed: -0x%04x", -ret);
            goto rot_fail;
        }
    }
    ESP_LOGI(TAG_APP, "[ROT] TLS OK to %s", ctx->target_host);

    /* Step 3: SMP ServerHello -> session_id + ClientHello (keyHash) */
    {
        uint8_t *hblock = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
        if (!hblock) { ESP_LOGE(TAG_APP, "[ROT] block alloc failed"); goto rot_fail; }

        int content_len = smp_read_block(&s_rot_ctx->ssl, hblock, 15000);
        if (content_len < 37) {
            ESP_LOGE(TAG_APP, "[ROT] No ServerHello (len=%d)", content_len);
            heap_caps_free(hblock);
            goto rot_fail;
        }

        uint8_t *hello = hblock + 2;
        uint8_t sessIdLen = hello[4];
        if (sessIdLen != 32) {
            ESP_LOGE(TAG_APP, "[ROT] Bad sessIdLen %d", sessIdLen);
            heap_caps_free(hblock);
            goto rot_fail;
        }
        memcpy(s_rot_session_id, &hello[5], 32);

        /* Parse cert chain and compute keyHash */
        int cert1_off, cert1_len, cert2_off, cert2_len;
        parse_cert_chain(hello, content_len, &cert1_off, &cert1_len,
                         &cert2_off, &cert2_len);

        uint8_t ca_hash[32];
        if (cert2_off >= 0) {
            mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
        } else {
            mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
        }

        /* SEC-07: Verify fingerprint against expected target server */
        if (memcmp(ca_hash, ctx->target_key_hash, 32) != 0) {
            ESP_LOGE(TAG_APP, "[ROT] SEC-07: FINGERPRINT MISMATCH for %s!",
                     ctx->target_host);
            heap_caps_free(hblock);
            goto rot_fail;
        }
        ESP_LOGI(TAG_APP, "[ROT] SEC-07: Target server fingerprint verified");

        /* Send ClientHello: version(2B) + keyHash(1B len + 32B) */
        uint8_t client_hello[35];
        int pos = 0;
        client_hello[pos++] = 0x00;
        client_hello[pos++] = 0x07;  // SMP v7
        client_hello[pos++] = 32;
        memcpy(&client_hello[pos], ca_hash, 32);
        pos += 32;

        ret = smp_write_handshake_block(&s_rot_ctx->ssl, hblock, client_hello, pos);
        heap_caps_free(hblock);
        if (ret != 0) {
            ESP_LOGE(TAG_APP, "[ROT] ClientHello send failed");
            goto rot_fail;
        }
    }

    s_rot_connected = true;
    ESP_LOGI(TAG_APP, "[ROT] SMP handshake OK! sessId=%02x%02x%02x%02x...",
             s_rot_session_id[0], s_rot_session_id[1],
             s_rot_session_id[2], s_rot_session_id[3]);
    return true;

rot_fail:
    app_rotation_disconnect();
    return false;
}

/**
 * Send NEW command on second (rotation) TLS connection.
 * Parses IDS response and fills rotation data.
 *
 * @param contact_idx  Contact slot
 * @param is_rq        true = Reply Queue, false = Main Queue
 * @return true on success
 */
static bool app_rotation_send_new(int contact_idx, bool is_rq)
{
    if (!s_rot_connected || !s_rot_ctx) return false;

    const rotation_contact_data_t *rd = rotation_get_contact_data(contact_idx);
    if (!rd) return false;

    /* Select correct keypair based on main/RQ */
    const uint8_t *auth_public = is_rq ? rd->rq_new_rcv_auth_public : rd->new_rcv_auth_public;
    const uint8_t *auth_private = is_rq ? rd->rq_new_rcv_auth_private : rd->new_rcv_auth_private;
    const uint8_t *dh_public = is_rq ? rd->rq_new_rcv_dh_public : rd->new_rcv_dh_public;

    ESP_LOGI(TAG_APP, "[ROT][%d] Sending NEW for %s queue...",
             contact_idx, is_rq ? "Reply" : "Main");

    /* Build NEW command body: corrId + entityId(empty) + "NEW " + keys + subMode */
    uint8_t trans_body[256];
    int pos = 0;

    trans_body[pos++] = 1;     // corrId length
    trans_body[pos++] = 'R';   // corrId = 'R' (rotation)
    trans_body[pos++] = 0;     // entityId = empty (new queue)

    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';

    /* rcvAuthKey = Ed25519 SPKI */
    trans_body[pos++] = 44;
    memcpy(&trans_body[pos], ED25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], auth_public, 32);
    pos += 32;

    /* rcvDhKey = X25519 SPKI */
    trans_body[pos++] = 44;
    memcpy(&trans_body[pos], X25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], dh_public, 32);
    pos += 32;

    /* subMode = 'S' (subscribe immediately) */
    trans_body[pos++] = 'S';

    int body_len = pos;

    /* Sign: sessionId + body */
    uint8_t to_sign[1 + 32 + 256];
    int sp = 0;
    to_sign[sp++] = 32;
    memcpy(&to_sign[sp], s_rot_session_id, 32);
    sp += 32;
    memcpy(&to_sign[sp], trans_body, body_len);
    sp += body_len;

    uint8_t signature[64];
    crypto_sign_detached(signature, NULL, to_sign, sp, auth_private);

    /* Build transmission: sigLen + sig + body
     * NOTE: SMP v7 signs WITH sessionId but does NOT put it on the wire.
     * This matches the KEY command format in network_task. */
    uint8_t transmission[256];
    int tp = 0;
    transmission[tp++] = 64;
    memcpy(&transmission[tp], signature, 64);
    tp += 64;
    memcpy(&transmission[tp], trans_body, body_len);
    tp += body_len;

    /* Send on second connection */
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    if (!block) return false;

    int ret = smp_write_command_block(&s_rot_ctx->ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG_APP, "[ROT][%d] NEW send failed!", contact_idx);
        heap_caps_free(block);
        return false;
    }

    /* Parse IDS response */
    int content_len = smp_read_block(&s_rot_ctx->ssl, block, 10000);
    if (content_len < 10) {
        ESP_LOGE(TAG_APP, "[ROT][%d] No IDS response (len=%d)", contact_idx, content_len);
        heap_caps_free(block);
        return false;
    }

    uint8_t *resp = block + 2;
    int p = 0;

    /* Skip transport header: txCount + 2 skip + authLen + corrLen + entLen
     * NOTE: SMP v7 has NO sessLen field in responses (unlike queue_create's v6) */
    p++;       // txCount
    p += 2;    // skip bytes
    int authLen = resp[p++]; p += authLen;
    int corrLen = resp[p++]; p += corrLen;
    int entLen  = resp[p++]; p += entLen;

    /* Check for IDS */
    if (p + 3 > content_len || resp[p] != 'I' || resp[p+1] != 'D' || resp[p+2] != 'S') {
        ESP_LOGE(TAG_APP, "[ROT][%d] Expected IDS, got %c%c%c",
                 contact_idx, resp[p], resp[p+1], resp[p+2]);
        heap_caps_free(block);
        return false;
    }
    p += 3;
    if (p < content_len && resp[p] == ' ') p++;

    /* Parse: rcvId + sndId + dhKey */
    uint8_t rcv_id[24], snd_id[24];
    uint8_t rcv_id_len = resp[p++];
    if (rcv_id_len > 24) { heap_caps_free(block); return false; }
    memcpy(rcv_id, &resp[p], rcv_id_len);
    p += rcv_id_len;

    uint8_t snd_id_len = resp[p++];
    if (snd_id_len > 24) { heap_caps_free(block); return false; }
    memcpy(snd_id, &resp[p], snd_id_len);
    p += snd_id_len;

    uint8_t dh_len = resp[p++];
    if (dh_len != 44) {
        ESP_LOGE(TAG_APP, "[ROT][%d] Unexpected DH key length: %d", contact_idx, dh_len);
        heap_caps_free(block);
        return false;
    }
    /* Skip 12-byte SPKI header, copy 32-byte raw key */
    uint8_t srv_dh_public[32];
    memcpy(srv_dh_public, &resp[p + 12], 32);

    heap_caps_free(block);

    /* Complete queue creation in rotation state machine */
    bool ok;
    if (is_rq) {
        ok = rotation_complete_rq_creation(contact_idx,
                rcv_id, rcv_id_len, snd_id, snd_id_len, srv_dh_public);
    } else {
        ok = rotation_complete_queue_creation(contact_idx,
                rcv_id, rcv_id_len, snd_id, snd_id_len, srv_dh_public);
    }

    if (ok) {
        ESP_LOGI(TAG_APP, "[ROT][%d] %s queue IDS parsed: rcvId=%02x%02x sndId=%02x%02x",
                 contact_idx, is_rq ? "RQ" : "Main",
                 rcv_id[0], rcv_id[1], snd_id[0], snd_id[1]);
    }
    return ok;
}

/**
 * Phase 2: Send KEY command on new queue via second TLS connection.
 * Registers the peer's sender key (from QKEY response) on our new queue,
 * so the peer can authenticate their SEND commands after switching.
 *
 * Format is identical to NET_CMD_SEND_KEY but uses:
 * - s_rot_ctx->ssl (second TLS connection to target server)
 * - s_rot_session_id (session from second TLS handshake)
 * - new queue's rcv_id and rcv_auth_private (from rotation_contact_data_t)
 */
static bool app_rotation_send_key(int contact_idx)
{
    if (!s_rot_ctx || !s_rot_connected) {
        ESP_LOGE(TAG_APP, "[ROT][%d] KEY: second TLS not connected!", contact_idx);
        return false;
    }

    const rotation_contact_data_t *rd = rotation_get_contact_data(contact_idx);
    if (!rd || !rd->has_peer_sender_key) {
        ESP_LOGE(TAG_APP, "[ROT][%d] KEY: no peer sender key!", contact_idx);
        return false;
    }

    ESP_LOGI(TAG_APP, "[ROT][%d] Sending KEY on new queue (rcvId=%02x%02x...)...",
             contact_idx, rd->new_rcv_id[0], rd->new_rcv_id[1]);

    /* Build KEY command body: corrId + entityId(rcvId) + "KEY " + [len]senderKey */
    uint8_t body[128];
    int bp = 0;

    /* corrId = 24 random bytes */
    body[bp++] = 24;
    uint8_t corr[24];
    esp_fill_random(corr, 24);
    memcpy(&body[bp], corr, 24);
    bp += 24;

    /* entityId = new queue rcvId */
    body[bp++] = (uint8_t)rd->new_rcv_id_len;
    memcpy(&body[bp], rd->new_rcv_id, rd->new_rcv_id_len);
    bp += rd->new_rcv_id_len;

    /* Command: "KEY " */
    body[bp++] = 'K';
    body[bp++] = 'E';
    body[bp++] = 'Y';
    body[bp++] = ' ';

    /* senderKey = length-prefixed SPKI (44 bytes) */
    body[bp++] = 44;
    memcpy(&body[bp], rd->peer_sender_key, 44);
    bp += 44;

    /* Sign: [sessIdLen=32][sessionId] + body (SMP v7: sign with sessId, don't put on wire) */
    uint8_t to_sign[1 + 32 + 128];
    int sp = 0;
    to_sign[sp++] = 32;
    memcpy(&to_sign[sp], s_rot_session_id, 32);
    sp += 32;
    memcpy(&to_sign[sp], body, bp);
    sp += bp;

    uint8_t signature[64];
    crypto_sign_detached(signature, NULL, to_sign, sp, rd->new_rcv_auth_private);

    /* Build transmission: sigLen + sig + body (v7: no sessionId on wire) */
    uint8_t transmission[256];
    int tp = 0;
    transmission[tp++] = 64;
    memcpy(&transmission[tp], signature, 64);
    tp += 64;
    memcpy(&transmission[tp], body, bp);
    tp += bp;

    /* Send on second TLS connection */
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    if (!block) return false;

    int ret = smp_write_command_block(&s_rot_ctx->ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG_APP, "[ROT][%d] KEY send failed!", contact_idx);
        heap_caps_free(block);
        return false;
    }

    /* Wait for OK response */
    int content_len = smp_read_block(&s_rot_ctx->ssl, block, 5000);
    if (content_len < 5) {
        ESP_LOGE(TAG_APP, "[ROT][%d] KEY: no response (len=%d)", contact_idx, content_len);
        heap_caps_free(block);
        return false;
    }

    uint8_t *resp = block + 2;
    int p = 0;
    p++;       // txCount
    p += 2;    // skip
    int authLen = resp[p++]; p += authLen;
    int corrLen = resp[p++]; p += corrLen;
    int entLen  = resp[p++]; p += entLen;

    bool ok = false;
    if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
        ESP_LOGI(TAG_APP, "");
        ESP_LOGI(TAG_APP, "      +----------------------------------------------+");
        ESP_LOGI(TAG_APP, "      |  [ROT][%d] KEY ACCEPTED on new queue!         |", contact_idx);
        ESP_LOGI(TAG_APP, "      +----------------------------------------------+");
        ok = true;
    } else {
        ESP_LOGE(TAG_APP, "[ROT][%d] KEY response: %c%c%c (not OK)",
                 contact_idx, resp[p], resp[p+1], resp[p+2]);
    }

    heap_caps_free(block);
    return ok;
}

/**
 * Send QADD/QUSE to contact via peer (first) connection.
 * Uses the existing Double Ratchet + NaCl encrypt pipeline.
 *
 * Declared in smp_peer.h, implemented in smp_peer.c.
 * Signature: peer_send_raw_agent_msg(contact_t*, const uint8_t*, int, const char*)
 */
extern bool peer_send_raw_agent_msg(contact_t *contact,
                                     const uint8_t *a_message, int a_message_len,
                                     const char *label);

/**
 * One rotation step per app loop iteration. Processes one contact.
 * Called from app_process_deferred_work() when rotation is active.
 */
static void app_rotation_step(void)
{
    if (!rotation_is_active()) return;

    /* NOTE: No eager "Step 0" connection here. The rotation TLS is connected
     * on demand in ROT_IDLE (for NEW) and ROT_QKEY_RECEIVED (for KEY), then
     * disconnected immediately after. This prevents 3 simultaneous TLS
     * connections (main + rotation + peer) which exhaust SRAM for sdmmc DMA. */

    /* Find next contact that needs work */
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;

        rotation_contact_state_t state = rotation_get_contact_state(i);

        switch (state) {
            case ROT_IDLE: {
                /* Generate keys for this contact */
                ESP_LOGI(TAG_APP, "[ROT] Phase 1 for contact [%d] '%s'",
                         i, contacts_db.contacts[i].name);

                rotation_create_queue_for_contact(i);

                /* Connect rotation TLS for NEW commands */
                if (!s_rot_connected) {
                    if (!app_rotation_connect()) {
                        ESP_LOGW(TAG_APP, "[ROT][%d] Cannot connect for NEWs, retry next loop", i);
                        return;
                    }
                }

                /* Send NEW for main queue */
                if (!app_rotation_send_new(i, false)) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] Main queue NEW failed!", i);
                    return;  /* Retry next loop */
                }

                /* Send NEW for reply queue */
                if (!app_rotation_send_new(i, true)) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] Reply queue NEW failed!", i);
                    return;
                }

                /* State should now be ROT_QUEUE_CREATED (both done) */
                ESP_LOGI(TAG_APP, "[ROT][%d] Both queues created! State: %s",
                         i, rotation_state_name(rotation_get_contact_state(i)));

                /* Fix 3: Close rotation TLS after NEWs to free SRAM.
                 * Will reconnect on demand for KEY command in Phase 2.
                 * This prevents 3 simultaneous TLS (main + rotation + peer). */
                app_rotation_disconnect();
                ESP_LOGI(TAG_APP, "[ROT][%d] Rotation TLS closed (SRAM reclaim after NEWs)", i);

                return;  /* One contact per loop iteration */
            }

            case ROT_QUEUE_CREATED: {
                /* Validate peer state before attempting QADD */
                if (!peer_load_state(i)) {
                    ESP_LOGW(TAG_APP, "[ROT][%d] No valid peer state - skipping contact", i);
                    rotation_mark_qadd_sent(i);
                    continue;
                }

                /* Build and send QADD via peer connection */
                uint8_t qadd_buf[512];
                int qadd_len = rotation_build_qadd_payload(i, qadd_buf, sizeof(qadd_buf));
                if (qadd_len < 0) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] Failed to build QADD payload!", i);
                    return;
                }

                ESP_LOGI(TAG_APP, "[ROT][%d] QADD payload: %d bytes", i, qadd_len);

                /* Set ratchet + handshake to this contact before sending */
                ratchet_set_active(i);
                handshake_set_active(i);

                /* Send QADD via peer connection (Double Ratchet encrypted) */
                if (peer_send_raw_agent_msg(&contacts_db.contacts[i],
                                             qadd_buf, qadd_len, "QADD")) {
                    rotation_mark_qadd_sent(i);
                    ESP_LOGI(TAG_APP, "[ROT][%d] QADD sent! Waiting for QKEY...", i);
                } else {
                    ESP_LOGW(TAG_APP, "[ROT][%d] QADD send failed! Will retry.", i);
                }

                /* Close peer connection - never run 3 TLS simultaneously */
                peer_disconnect();

                return;  /* One contact per loop */
            }

            case ROT_QADD_SENT:
            case ROT_WAITING:
                /* Waiting for QKEY response (handled in smp_agent.c) */
                /* TODO: Add timeout and retry logic */
                continue;

            case ROT_QKEY_RECEIVED: {
                /* Phase 2a: Send KEY on new queue (register peer's sender key) */
                ESP_LOGI(TAG_APP, "[ROT] Phase 2 for contact [%d] '%s'",
                         i, contacts_db.contacts[i].name);

                /* Ensure second TLS is still connected (may have timed out) */
                if (!s_rot_connected) {
                    if (!app_rotation_connect()) {
                        ESP_LOGW(TAG_APP, "[ROT][%d] Second TLS reconnect failed, retry next loop", i);
                        return;
                    }
                }

                if (!app_rotation_send_key(i)) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] KEY on new queue failed!", i);
                    return;  /* Retry next loop */
                }
                rotation_mark_key_sent(i);

                /* Fix 3: Close rotation TLS after KEY - QUSE goes via peer.
                 * This ensures max 2 TLS at any time (main + peer for QUSE). */
                app_rotation_disconnect();
                ESP_LOGI(TAG_APP, "[ROT][%d] Rotation TLS closed (SRAM reclaim after KEY)", i);

                /* Phase 2b: Send QUSE via peer connection (Double Ratchet) */
                ESP_LOGI(TAG_APP, "[ROT][%d] Sending QUSE (switch to new queue)...", i);

                /* Load peer state for this contact (needed for peer_send_raw_agent_msg) */
                if (!peer_load_state(i)) {
                    ESP_LOGW(TAG_APP, "[ROT][%d] No peer state for QUSE - skipping", i);
                    rotation_mark_quse_sent(i);  /* Mark done anyway - KEY was sent */
                    continue;
                }

                uint8_t quse_buf[128];
                int quse_len = rotation_build_quse_payload(i, quse_buf, sizeof(quse_buf));
                if (quse_len < 0) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] Failed to build QUSE payload!", i);
                    return;
                }

                ratchet_set_active(i);
                handshake_set_active(i);

                if (peer_send_raw_agent_msg(&contacts_db.contacts[i],
                                             quse_buf, quse_len, "QUSE")) {
                    rotation_mark_quse_sent(i);  /* -> ROT_DONE */
                    ESP_LOGI(TAG_APP, "[ROT][%d] QUSE sent! Waiting for QTEST phase.", i);
                } else {
                    ESP_LOGW(TAG_APP, "[ROT][%d] QUSE send failed! Will retry.", i);
                }

                peer_disconnect();
                ESP_LOGI(TAG_APP, "[ROT][%d] Peer disconnected (SRAM reclaim)", i);

                return;  /* One contact per loop */
            }

            case ROT_KEY_SENT: {
                /* KEY was sent but QUSE not yet - resume from here */
                ESP_LOGI(TAG_APP, "[ROT][%d] Resuming Phase 2b: sending QUSE...", i);

                if (!peer_load_state(i)) {
                    ESP_LOGW(TAG_APP, "[ROT][%d] No peer state for QUSE - skipping", i);
                    rotation_mark_quse_sent(i);
                    continue;
                }

                uint8_t quse_buf[128];
                int quse_len = rotation_build_quse_payload(i, quse_buf, sizeof(quse_buf));
                if (quse_len < 0) {
                    ESP_LOGE(TAG_APP, "[ROT][%d] Failed to build QUSE payload!", i);
                    return;
                }

                ratchet_set_active(i);
                handshake_set_active(i);

                if (peer_send_raw_agent_msg(&contacts_db.contacts[i],
                                             quse_buf, quse_len, "QUSE")) {
                    rotation_mark_quse_sent(i);
                    ESP_LOGI(TAG_APP, "[ROT][%d] QUSE sent! Waiting for QTEST phase.", i);
                } else {
                    ESP_LOGW(TAG_APP, "[ROT][%d] QUSE send failed! Will retry.", i);
                }

                peer_disconnect();
                return;
            }

            case ROT_QUSE_SENT:
                /* Waiting for QTEST from App (App sends it on the NEW queue).
                 * When QTEST arrives, smp_agent.c handles it and marks DONE. */
                continue;

            case ROT_QTEST_SENT:
                /* Legacy state - should not occur in new flow */
                continue;

            case ROT_DONE:
            case ROT_ERROR:
                continue;

            default:
                continue;
        }
    }

    /* If we get here, all contacts have been processed (or are waiting) */
    /* Check if all contacts have at least sent QUSE (ready for live-switch).
     * QTEST will arrive on the NEW server AFTER we reconnect there. */
    int ready_count = 0;
    int active_count = 0;
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        active_count++;
        rotation_contact_state_t st = rotation_get_contact_state(i);
        if (st == ROT_QUSE_SENT || st == ROT_DONE) ready_count++;
    }

    if (ready_count == active_count && active_count > 0) {
        /* Only trigger the live-switch once */
        static bool s_complete_logged = false;
        if (!s_complete_logged) {
            s_complete_logged = true;

            /* Check if we're already on the target server (reboot after live-switch).
             * If so, credentials are already migrated - just wait for QTEST. */
            const char *target = rotation_get_target_host();
            if (target && strcmp(our_queue.server_host, target) == 0) {
                ESP_LOGI(TAG_APP, "[ROT] Already on target server %s - waiting for QTEST",
                         our_queue.server_host);
                return;
            }

            ESP_LOGI(TAG_APP, "");
            ESP_LOGI(TAG_APP, "+----------------------------------------+");
            ESP_LOGI(TAG_APP, "|  [ROT] ALL QUSE SENT - LIVE SWITCH     |");
            ESP_LOGI(TAG_APP, "+----------------------------------------+");

            app_rotation_disconnect();

            /* Credential swap + NVS save + set our_queue.server_host to new server */
            rotation_complete();

            /* Force reconnect to new server.
             * After reconnect, subscribe on new queues.
             * QTEST from the App will arrive on the new queue there. */
            ESP_LOGI(TAG_APP, "[ROT] Forcing reconnect to new server %s:%d...",
                     our_queue.server_host, our_queue.server_port);
            if (s_sock_fd >= 0) {
                shutdown(s_sock_fd, SHUT_RDWR);
            }
        }
    }
}

static void app_process_deferred_work(void)
{
    /* Session 48: Auto-reconnect with exponential backoff */
    if (s_reconnect_needed && !s_reconnect_done) {
        static uint32_t backoff_ms = 0;

        /* Wait for WiFi before attempting TCP */
        if (!wifi_connected) {
            ESP_LOGW(TAG_APP, "RECONNECT: No WiFi, waiting...");
            return;  /* Will retry next loop iteration */
        }

        if (backoff_ms > 0) {
            ESP_LOGI(TAG_APP, "RECONNECT: Backoff %lu ms...", (unsigned long)backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        }

        if (app_do_reconnect()) {
            backoff_ms = 0;
            s_reconnect_done = true;
            ESP_LOGI(TAG_APP, "RECONNECT: Signalling Network Task to resume");
        } else {
            if (backoff_ms == 0) backoff_ms = 2000;
            else if (backoff_ms < 60000) backoff_ms *= 2;
            ESP_LOGW(TAG_APP, "RECONNECT: Failed, next attempt in %lu ms",
                     (unsigned long)backoff_ms);
        }
        return;  /* Don't process other work during reconnect */
    }

    if (s_contacts_dirty) {
        s_contacts_dirty = false;
        save_contacts_to_nvs();
    }

    /* Session 48: NVS cleanup for aborted pending contact.
     * Memory deactivation already happened in smp_abort_pending_contact().
     * This just cleans up the NVS keys (requires Internal SRAM stack). */
    if (s_pending_delete_slot >= 0) {
        int slot = s_pending_delete_slot;
        s_pending_delete_slot = -1;

        if (slot >= 0 && slot < MAX_CONTACTS) {
            ESP_LOGI(TAG_APP, "APP: NVS cleanup for aborted contact [%d]", slot);

            /* Clean NVS keys for this slot */
            uint8_t s = (uint8_t)slot;
            char key[20];
            snprintf(key, sizeof(key), "ratchet_%02u", s);
            smp_storage_delete(key);
            snprintf(key, sizeof(key), "queue_rcv_%02u", s);
            smp_storage_delete(key);
            snprintf(key, sizeof(key), "queue_snd_%02u", s);
            smp_storage_delete(key);
            snprintf(key, sizeof(key), "rq_%02u", s);
            smp_storage_delete(key);

            /* Clear 42d + scanned bitmaps */
            smp_clear_42d(slot);

            /* Persist contact list (slot now inactive) */
            save_contacts_to_nvs();

            ESP_LOGI(TAG_APP, "APP: Pending contact [%d] NVS cleanup done (%d remaining)",
                     slot, contacts_db.num_contacts);
        }
    }

    if (s_rq_save_pending >= 0) {
        int rq_slot = s_rq_save_pending;
        s_rq_save_pending = -1;
        if (reply_queue_save(rq_slot))
            ESP_LOGI(TAG_APP, "RQ[%d] NVS save completed (deferred)", rq_slot);
        else
            ESP_LOGE(TAG_APP, "RQ[%d] NVS save FAILED!", rq_slot);
    }

    if (s_history_load_pending >= 0) {
        int slot = s_history_load_pending;
        s_history_load_pending = -1;

        if (!smp_history_batch) {
            smp_history_batch = heap_caps_malloc(
                HISTORY_MAX_LOAD * sizeof(history_message_t), MALLOC_CAP_SPIRAM);
        }

        if (smp_history_batch) {
            int loaded = 0;
            esp_err_t err = smp_history_load_recent(
                (uint8_t)slot, smp_history_batch, HISTORY_MAX_LOAD, &loaded);

            ui_event_t evt = {0};
            evt.type = UI_EVT_HISTORY_BATCH;
            evt.contact_idx = slot;

            if (err == ESP_OK && loaded > 0) {
                smp_history_batch_count = loaded;
                smp_history_batch_slot = slot;
                smp_history_mark_read((uint8_t)slot);
                ESP_LOGI(TAG_APP, "History: %d messages loaded for slot %d", loaded, slot);
            } else {
                smp_history_batch_count = 0;
                smp_history_batch_slot = -1;
                ESP_LOGD(TAG_APP, "History: no messages for slot %d", slot);
            }
            xQueueSend(app_to_ui_queue, &evt, 0);
        } else {
            ESP_LOGE(TAG_APP, "History: PSRAM alloc failed for batch buffer");
        }
    }

    /* Session 49: Queue Rotation processing */
    if (rotation_is_active()) {
        app_rotation_step();
    }
}

static void app_process_keyboard_queue(QueueHandle_t kbd_queue)
{
    char kbd_msg[256];
    while (kbd_queue && xQueueReceive(kbd_queue, kbd_msg, 0) == pdTRUE) {
        ESP_LOGI(TAG_APP, "Sending: \"%s\"", kbd_msg);
        uint32_t seq = ui_chat_get_last_seq();
        contact_t *msg_contact = &contacts_db.contacts[s_active_contact_idx];

        if (!msg_contact->active) {
            ESP_LOGE(TAG_APP, "   [FAIL] Active contact [%d] is not active!", s_active_contact_idx);
            smp_notify_ui_delivery_status(seq, MSG_STATUS_FAILED);
            continue;
        }

        if (peer_send_chat_message(msg_contact, kbd_msg)) {
            ESP_LOGI(TAG_APP, "   [OK] Message sent! seq=%lu", (unsigned long)seq);
            smp_notify_ui_delivery_status(seq, MSG_STATUS_SENT);
            uint64_t sent_msg_id = handshake_get_last_msg_id();
            smp_register_msg_mapping(seq, sent_msg_id);

            history_message_t hist_msg = {
                .direction = HISTORY_DIR_SENT,
                .delivery_status = HISTORY_STATUS_SENT,
                .timestamp = time(NULL),
                .text_len = (uint16_t)strlen(kbd_msg),
            };
            if (hist_msg.text_len > HISTORY_MAX_TEXT)
                hist_msg.text_len = HISTORY_MAX_TEXT;
            memcpy(hist_msg.text, kbd_msg, hist_msg.text_len);
            hist_msg.text[hist_msg.text_len] = '\0';
            smp_history_append((uint8_t)s_active_contact_idx, &hist_msg);
        } else {
            ESP_LOGE(TAG_APP, "   Send failed! seq=%lu", (unsigned long)seq);
            smp_notify_ui_delivery_status(seq, MSG_STATUS_FAILED);

            // Persist failed message to SD - survives restart with red X
            history_message_t hist_msg = {
                .direction = HISTORY_DIR_SENT,
                .delivery_status = HISTORY_STATUS_FAILED,
                .timestamp = time(NULL),
                .text_len = (uint16_t)strlen(kbd_msg),
            };
            if (hist_msg.text_len > HISTORY_MAX_TEXT)
                hist_msg.text_len = HISTORY_MAX_TEXT;
            memcpy(hist_msg.text, kbd_msg, hist_msg.text_len);
            hist_msg.text[hist_msg.text_len] = '\0';
            smp_history_append((uint8_t)s_active_contact_idx, &hist_msg);
        }
    }
}

static void app_handle_reply_queue_msg(
    int rq_contact, bool is_legacy_rq,
    uint8_t *resp, int p, int enc_len,
    uint8_t *msg_id, uint8_t msgIdLen)
{
    ESP_LOGI(TAG_APP, "   Decrypting REPLY QUEUE message for contact [%d] (legacy=%d)...", rq_contact, is_legacy_rq);
    // Session 47: Only show status during active handshake, not normal messages
    if (!is_42d_done(rq_contact))
        smp_notify_ui_status("Decrypting...");

    uint8_t *e2e_plain = NULL;
    size_t e2e_plain_len = 0;
    int e2e_ret = -99;

    reply_queue_t *rq = reply_queue_get(rq_contact);
    if (!is_legacy_rq && rq && rq->valid) {
        uint8_t new_peer[32];
        bool new_peer_found = false;

        e2e_ret = smp_e2e_decrypt_reply_message_ex(
            &resp[p], enc_len, msg_id, msgIdLen,
            rq->shared_secret,
            rq->e2e_private,
            rq->e2e_peer_public,
            rq->e2e_peer_valid,
            new_peer, &new_peer_found,
            &e2e_plain, &e2e_plain_len);

        if (new_peer_found) {
            memcpy(rq->e2e_peer_public, new_peer, 32);
            rq->e2e_peer_valid = true;
            reply_queue_save(rq_contact);
            ESP_LOGI(TAG_APP, "   RQ[%d] peer E2E key updated + saved", rq_contact);
        }
    } else {
        ESP_LOGW(TAG_APP, "   Using legacy our_queue for decrypt (legacy=%d, rq_valid=%d)", is_legacy_rq, rq ? rq->valid : -1);
        e2e_ret = smp_e2e_decrypt_reply_message(
            &resp[p], enc_len, msg_id, msgIdLen,
            &e2e_plain, &e2e_plain_len);
    }

    if (e2e_ret == 0 && e2e_plain) {
        int hs_contact = rq_contact;
        ESP_LOGI(TAG_APP, "   Reply Queue: handshake for contact [%d]", hs_contact);

        ratchet_set_active(hs_contact);
        handshake_set_active(hs_contact);
        ESP_LOGI(TAG_APP, "   35a: Active slot set to [%d] BEFORE agent_process", hs_contact);

        smp_agent_process_message(e2e_plain, e2e_plain_len,
               &contacts_db.contacts[hs_contact],
               s_peer_sender_auth_key, &s_has_peer_sender_auth);
        free(e2e_plain);
    } else {
        ESP_LOGE(TAG_APP, "   Reply Queue decrypt failed! ret=%d", e2e_ret);
    }

    // === Post-Confirmation Block (per-contact) ===
    int hs_contact = rq_contact;
    if (s_has_peer_sender_auth && !is_42d_done(hs_contact)) {
        mark_42d_done(hs_contact);
        ratchet_set_active(hs_contact);
        handshake_set_active(hs_contact);
        ESP_LOGI(TAG_APP, "APP: 42d - Starting for contact [%d] (ratchet+handshake set)", hs_contact);

        // 1. Send KEY via Network Task (async, on main SSL)
        {
            net_cmd_t key_cmd = {0};
            key_cmd.cmd = NET_CMD_SEND_KEY;
            key_cmd.rq_slot = is_legacy_rq ? -1 : hs_contact;
            memcpy(key_cmd.peer_auth_key, s_peer_sender_auth_key, 44);
            key_cmd.peer_auth_key_len = 44;

            if (xRingbufferSend(app_to_net_buf, &key_cmd, sizeof(key_cmd),
                                pdMS_TO_TICKS(5000)) != pdTRUE) {
                ESP_LOGE(TAG_APP, "APP: 42d -- Failed to queue SEND_KEY!");
                goto skip_42d_app;
            }
            ESP_LOGI(TAG_APP, "APP: 42d -- SEND_KEY queued (slot=%d)", key_cmd.rq_slot);
            smp_notify_ui_status("Securing channel...");
            {
                uint32_t notify_val = 0;
                BaseType_t got = xTaskNotifyWait(0, NOTIFY_KEY_DONE, &notify_val,
                                                  pdMS_TO_TICKS(5000));
                if (got == pdTRUE) {
                    ESP_LOGI(TAG_APP, "APP: 42d -- KEY confirmed by Net Task!");
                } else {
                    ESP_LOGW(TAG_APP, "APP: 42d -- KEY timeout (5s)! Proceeding anyway...");
                }
            }
        }

        // 2. HELLO auf Contact Queue
        {
            contact_t *hello_contact = &contacts_db.contacts[hs_contact];
            smp_notify_ui_status("Sending HELLO...");
            if (peer_send_hello(hello_contact)) {
                ESP_LOGI(TAG_APP, "APP: 42d -- HELLO sent!");
            } else {
                ESP_LOGE(TAG_APP, "APP: 42d -- HELLO failed!");
            }
        }

        // 3. Brief wait for peer response
        smp_notify_ui_status("Waiting for peer...");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 4. Connection complete - update status, stay on connect screen
        s_active_contact_idx = hs_contact;
        smp_notify_ui_status("Connected!");
        ESP_LOGI(TAG_APP, "APP: 42d -- Contact [%d] connected!", hs_contact);

        // Session 47 2d: Notify UI that handshake is fully complete
        smp_notify_ui_connect_done(hs_contact);

        // Session 49 Bug #32: Re-subscribe after handshake completion
        app_request_subscribe_all();

        s_has_peer_sender_auth = false;

        skip_42d_app: ;
    }

    // ACK Reply Queue with per-contact auth keys
    {
        reply_queue_t *ack_rq = reply_queue_get(rq_contact);
        if (!is_legacy_rq && ack_rq && ack_rq->valid) {
            app_send_ack(ack_rq->rcv_id, ack_rq->rcv_id_len,
                         msg_id, msgIdLen, ack_rq->rcv_auth_private);
        } else {
            app_send_ack(our_queue.rcv_id, our_queue.rcv_id_len,
                         msg_id, msgIdLen, our_queue.rcv_auth_private);
        }
    }

    // Bug #30: removed subscribe_all after RQ MSG - subscriptions remain active
    ESP_LOGI(TAG_APP, "APP: skip subscribe_all after RQ MSG - subscriptions active");
    ESP_LOGI(TAG_APP, "APP: Reply Queue processing complete.");
}

static void app_handle_contact_queue_msg(
    contact_t *contact, bool is_reply_queue,
    uint8_t *resp, int p, int enc_len,
    uint8_t *msg_id, uint8_t msgIdLen)
{
    if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
        int cq_contact_idx = (int)(contact - contacts_db.contacts);
        if (cq_contact_idx >= 0 && cq_contact_idx < MAX_CONTACTS) {
            ratchet_set_active(cq_contact_idx);
            handshake_set_active(cq_contact_idx);
        }

        ESP_LOGW(TAG_APP, "APP: Contact Queue decrypt for [%s] (slot %d), enc_len=%d",
                 contact->name, cq_contact_idx, enc_len);

        /* Session 49: During rotation QUSE_SENT, any MSG = QTEST */
        if (rotation_is_active() && cq_contact_idx >= 0 && cq_contact_idx < MAX_CONTACTS) {
            rotation_contact_state_t rs = rotation_get_contact_state(cq_contact_idx);
            if (rs == ROT_QUSE_SENT) {
                ESP_LOGI(TAG_APP, "");
                ESP_LOGI(TAG_APP, "      +----------------------------------------------+");
                ESP_LOGI(TAG_APP, "      |  [ROT] QTEST received from contact [%d]      |", cq_contact_idx);
                ESP_LOGI(TAG_APP, "      +----------------------------------------------+");
                rotation_mark_qtest_received(cq_contact_idx);
                /* ACK the QTEST message, but skip all decrypt */
                app_send_ack(contact->recipient_id, contact->recipient_id_len,
                             msg_id, msgIdLen, contact->rcv_auth_secret);
                return;
            }
        }

        /* Post-handshake messages need full pipeline: Layer 3+2+1.
         * Pre-handshake (AgentConfirmation) only need Layer 3.
         * Detection: is_42d_done means handshake completed -> full pipeline. */
        if (is_42d_done(cq_contact_idx)) {
            /* FULL PIPELINE: Layer 3 (Server) + Layer 2 (E2E) via smp_e2e,
             * then Layer 1 (Ratchet) via smp_agent. */

            /* Ensure CQ E2E peer key is populated (from rotation QKEY) */
            if (!s_cq_e2e_peer_valid) {
                if (rotation_get_cq_peer_e2e(s_cq_e2e_peer_public)) {
                    s_cq_e2e_peer_valid = true;
                    ESP_LOGI(TAG_APP, "   CQ E2E peer key loaded from rotation");
                }
            }

            uint8_t cq_shared[32];
            if (crypto_box_beforenm(cq_shared, contact->srv_dh_public,
                                     contact->rcv_dh_secret) != 0) {
                ESP_LOGE(TAG_APP, "   CQ shared secret computation failed");
                goto fallback;
            }

            uint8_t *e2e_plain = NULL;
            size_t e2e_plain_len = 0;
            uint8_t new_peer[32];
            bool new_peer_found = false;

            int e2e_ret = smp_e2e_decrypt_reply_message_ex(
                &resp[p], enc_len, msg_id, msgIdLen,
                cq_shared,
                our_queue.e2e_private,
                s_cq_e2e_peer_public,
                s_cq_e2e_peer_valid,
                new_peer, &new_peer_found,
                &e2e_plain, &e2e_plain_len);

            sodium_memzero(cq_shared, 32);

            if (new_peer_found) {
                memcpy(s_cq_e2e_peer_public, new_peer, 32);
                s_cq_e2e_peer_valid = true;
                ESP_LOGI(TAG_APP, "   CQ E2E peer key cached");
            }

            if (e2e_ret == 0 && e2e_plain) {
                ESP_LOGI(TAG_APP, "   CQ Full pipeline: Layer 3+2 OK (%zu bytes)", e2e_plain_len);
                smp_agent_process_message(e2e_plain, e2e_plain_len,
                    contact, s_peer_sender_auth_key, &s_has_peer_sender_auth);
                free(e2e_plain);

                app_send_ack(contact->recipient_id, contact->recipient_id_len,
                             msg_id, msgIdLen, contact->rcv_auth_secret);
                return;
            }
            ESP_LOGW(TAG_APP, "   CQ E2E failed (ret=%d), falling back to Layer 3 only", e2e_ret);
            if (e2e_plain) free(e2e_plain);
        }

fallback:;
        /* FALLBACK: Layer 3 only (handshake messages, AgentConfirmation) */
        uint8_t *plain = heap_caps_malloc(enc_len, MALLOC_CAP_SPIRAM);
        if (plain) {
            int plain_len = 0;
            if (decrypt_smp_message(contact, &resp[p], enc_len,
                                    msg_id, msgIdLen, plain, &plain_len)) {
                ESP_LOGI(TAG_APP, "   SMP Decrypt OK! (%d bytes)", plain_len);
                parse_agent_message(contact, plain, plain_len);
            } else {
                ESP_LOGE(TAG_APP, "   Decrypt FAILED!");
            }
            heap_caps_free(plain);
        } else {
            ESP_LOGE(TAG_APP, "   Failed to allocate decrypt buffer!");
        }

        app_send_ack(contact->recipient_id, contact->recipient_id_len,
                     msg_id, msgIdLen, contact->rcv_auth_secret);
    } else if (!is_reply_queue) {
        ESP_LOGW(TAG_APP, "   Cannot decrypt - contact=%s, have_srv_dh=%d, enc_len=%d",
                 contact ? contact->name : "NULL",
                 contact ? contact->have_srv_dh : -1,
                 enc_len);
    }
}

void smp_app_run(QueueHandle_t kbd_queue)
{
    uint8_t *local_block = NULL;
    app_init_run(kbd_queue, &local_block);
    if (!local_block) return;

    while (1) {
        app_process_deferred_work();
        app_process_keyboard_queue(kbd_queue);

        /* Fix 3: Restore Reply Queue subscriptions after rotation completes.
         * During rotation, RQ SUBs are skipped (SRAM conservation).
         * Trigger a clean reconnect which will subscribe everything fresh. */
        if (rotation_rq_subs_needed()) {
            ESP_LOGI(TAG_APP, "[ROT] Rotation complete - triggering reconnect for RQ subscriptions");
            if (s_sock_fd >= 0) {
                shutdown(s_sock_fd, SHUT_RDWR);
            }
        }

        // Read frame from ring buffer (blocking with timeout)
        size_t item_size = 0;
        void *item = xRingbufferReceive(net_to_app_buf, &item_size, pdMS_TO_TICKS(1000));
        if (!item) continue;

        // Copy to local buffer and return ring buffer item immediately
        if (item_size > SMP_BLOCK_SIZE + 2) item_size = SMP_BLOCK_SIZE + 2;
        memcpy(local_block, item, item_size);
        vRingbufferReturnItem(net_to_app_buf, item);

        // Transport-Parsing
        int content_len = (int)item_size - 2;
        if (content_len < 4) { ESP_LOGW(TAG_APP, "Frame too short: %d bytes", content_len); continue; }
        uint8_t *resp = local_block + 2;
        int p = 0;

        // txCount + 2 skip bytes
        if (p + 3 > content_len) { ESP_LOGW(TAG_APP, "Frame truncated at txCount"); continue; }
        uint8_t tx_count = resp[p]; p++;
        (void)tx_count;
        p += 2;

        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at authLen"); continue; }
        int authLen = resp[p++];
        if (p + authLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in auth"); continue; }
        p += authLen;

        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at corrLen"); continue; }
        int corrLen = resp[p++];
        if (p + corrLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in corr"); continue; }
        p += corrLen;

        if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at entLen"); continue; }
        int entLen = resp[p++];
        if (p + entLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in entity"); continue; }
        uint8_t entity_id[24];
        if (entLen > 24) entLen = 24;
        memcpy(entity_id, &resp[p], entLen);
        p += entLen;

        // Contact + Reply Queue Lookup
        int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
        contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;

        int rq_contact = find_reply_queue_by_rcv_id(entity_id, entLen);
        bool is_reply_queue = (rq_contact >= 0);
        bool is_legacy_rq = false;

        if (!is_reply_queue && our_queue.rcv_id_len > 0 &&
            entLen == our_queue.rcv_id_len &&
            memcmp(entity_id, our_queue.rcv_id, entLen) == 0 &&
            s_handshake_contact_idx >= 0) {
            is_reply_queue = true;
            is_legacy_rq = true;
            rq_contact = s_handshake_contact_idx;
            ESP_LOGW(TAG_APP, "Reply Queue matched via legacy our_queue (contact [%d])", rq_contact);
        }

        if (is_reply_queue)
            ESP_LOGI(TAG_APP, "MSG on REPLY_Q for contact [%d]", rq_contact);

        // Session 47 2d: Notify UI at first sign of activity after QR scan.
        // Contact Queue MSG arrives ~1s after scan (invitation response).
        // Reply Queue MSG arrives ~9s later (confirmation).
        // Fire on whichever comes first, but only once per contact.
        {
            int scanned_idx = -1;
            // Path A: Contact Queue MSG for the contact being created
            if (!is_reply_queue && contact &&
                contact_idx >= 0 && s_handshake_contact_idx >= 0 &&
                contact_idx == s_handshake_contact_idx &&
                !is_42d_done(contact_idx)) {
                scanned_idx = contact_idx;
            }
            // Path B: Reply Queue MSG for any new contact (fallback)
            if (scanned_idx < 0 && is_reply_queue &&
                rq_contact >= 0 && !is_42d_done(rq_contact)) {
                scanned_idx = rq_contact;
            }
            if (scanned_idx >= 0) {
                int byte = scanned_idx / 8;
                int bit = scanned_idx % 8;
                if (!(s_scanned_notified[byte] & (1 << bit))) {
                    s_scanned_notified[byte] |= (1 << bit);
                    smp_notify_ui_connect_scanned(scanned_idx);
                }
            }
        }

        // Command dispatch
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG_APP, "APP: OK [%s]",
                     contact ? contact->name : (is_reply_queue ? "reply_q" : "unknown"));
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
            ESP_LOGI(TAG_APP, "APP: END [%s]",
                     contact ? contact->name : (is_reply_queue ? "reply_q" : "unknown"));
        }
        else if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G' && resp[p+3] == ' ') {
            p += 4;

            if (p >= content_len) { ESP_LOGW(TAG_APP, "Frame truncated at msgIdLen"); continue; }
            uint8_t msgIdLen = resp[p++];
            uint8_t msg_id[24];
            memset(msg_id, 0, 24);
            if (msgIdLen > 24) msgIdLen = 24;
            if (p + msgIdLen > content_len) { ESP_LOGW(TAG_APP, "Frame truncated in msgId"); continue; }
            memcpy(msg_id, &resp[p], msgIdLen);
            p += msgIdLen;

            int enc_len = content_len - p;

            ESP_LOGW(TAG_APP, "APP: MSG for [%s] reply_q=%d, enc_len=%d",
                     contact ? contact->name : "unknown", is_reply_queue, enc_len);

            if (is_reply_queue && enc_len > crypto_box_MACBYTES)
                app_handle_reply_queue_msg(rq_contact, is_legacy_rq, resp, p, enc_len, msg_id, msgIdLen);

            app_handle_contact_queue_msg(contact, is_reply_queue, resp, p, enc_len, msg_id, msgIdLen);
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
            ESP_LOGE(TAG_APP, "APP: ERR [%s]",
                     contact ? contact->name : "unknown");
        }
        else {
            ESP_LOGW(TAG_APP, "APP: Unknown cmd=%c%c%c",
                     (p < content_len) ? resp[p] : '?',
                     (p+1 < content_len) ? resp[p+1] : '?',
                     (p+2 < content_len) ? resp[p+2] : '?');
        }
    }

    ESP_LOGW(TAG_APP, "App logic: parse loop ended, cleaning up");
    heap_caps_free(local_block);
}


static void ui_task(void *arg)
{
    ESP_LOGI(TAG, "UI task running on core %d", xPortGetCoreID());
    log_heap("ui_task");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Public API ---

int smp_tasks_init(void)
{
    ESP_LOGI(TAG, "Initializing task infrastructure...");
    log_heap("before_init");

    // Initialize frame pool (4 x 4KB = 16KB in PSRAM)
    smp_frame_pool_init();
    ESP_LOGI(TAG, "  Frame pool: %d frames available (PSRAM)",
             smp_frame_pool_available());
    log_heap("after_pool");

    // Create ring buffers in PSRAM (NOSPLIT = items not split across wrap)
    net_to_app_buf = xRingbufferCreateWithCaps(NET_TO_APP_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!net_to_app_buf) {
        ESP_LOGE(TAG, "Failed to create net_to_app ring buffer");
        return -1;
    }

    app_to_net_buf = xRingbufferCreateWithCaps(APP_TO_NET_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!app_to_net_buf) {
        ESP_LOGE(TAG, "Failed to create app_to_net ring buffer");
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
        return -1;
    }

    ESP_LOGI(TAG, "  Ring buffers (PSRAM): net->app %dB, app->net %dB",
             NET_TO_APP_BUF_SIZE, APP_TO_NET_BUF_SIZE);

    // UI event queue (8 events, non-blocking push)
    app_to_ui_queue = xQueueCreate(8, sizeof(ui_event_t));
    if (!app_to_ui_queue) {
        ESP_LOGW(TAG, "Failed to create UI event queue (non-fatal)");
    }

    log_heap("after_init");

    return 0;
}

int smp_tasks_start(mbedtls_ssl_context *ssl_context, const uint8_t *session_id, int sock_fd)
{
    if (!ssl_context || !session_id) {
        ESP_LOGE(TAG, "SSL context or session_id is NULL");
        return -1;
    }

    s_ssl = ssl_context;
    memcpy(s_session_id, session_id, 32);
    s_sock_fd = sock_fd;  // Store for network task timeout

    ESP_LOGI(TAG, "Starting tasks (all in PSRAM)...");
    log_heap("before_tasks");

    // Network task on Core 0 (stack in PSRAM)
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        network_task, "net_task",
        NETWORK_TASK_STACK, NULL,
        NETWORK_TASK_PRIO, &network_task_handle, 0,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        return -1;
    }

    // App logic: NOT a separate task anymore!
    // Runs on main task (64KB Internal SRAM stack) because NVS writes
    // crash with PSRAM stack (SPI Flash disables cache = PSRAM inaccessible)
    // Call smp_app_run() from main.c after smp_tasks_start()

    // UI task on Core 1 (stack in PSRAM)
    ret = xTaskCreatePinnedToCoreWithCaps(
        ui_task, "ui_task",
        UI_TASK_STACK, NULL,
        UI_TASK_PRIO, &ui_task_handle, 1,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        smp_tasks_stop();
        return -1;
    }

    ESP_LOGI(TAG, "Tasks started (net+ui PSRAM, app on main task)");
    log_heap("after_tasks");

    return 0;
}

void smp_tasks_stop(void)
{
    ESP_LOGW(TAG, "Stopping tasks...");

    if (network_task_handle) {
        vTaskDeleteWithCaps(network_task_handle);
        network_task_handle = NULL;
    }
    if (ui_task_handle) {
        vTaskDeleteWithCaps(ui_task_handle);
        ui_task_handle = NULL;
    }

    if (net_to_app_buf) {
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
    }
    if (app_to_net_buf) {
        vRingbufferDeleteWithCaps(app_to_net_buf);
        app_to_net_buf = NULL;
    }

    s_ssl = NULL;

    ESP_LOGW(TAG, "All tasks stopped");
}
