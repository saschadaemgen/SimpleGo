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
#include "ui_splash.h"   // Session 48: boot progress updates
#include <string.h>
#include <stdio.h>             // Session 48: snprintf for splash status
#include <time.h>          // time(NULL) for history timestamps
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"      // Session 47: Timing analysis (us precision)
#include <inttypes.h>       // PRId64


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

// Post-confirmation state (set by Reply Queue decrypt, read by 42d handler later)
static uint8_t s_peer_sender_auth_key[44];
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
            ESP_LOGE(TAG, "Network task: SSL read error %d", content_len);
            break;
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
                            // Bug #30: removed subscribe_all - queue already subscribed via subMode='S'
                            ESP_LOGI(TAG, "NET: skip subscribe_all - contact queue subscribed via subMode=S");

                            // Create per-contact reply queue
                            int rq_ret = reply_queue_create(s_ssl, block, s_session_id, slot);
                            if (rq_ret == 0) {
                                ESP_LOGI(TAG, "NET: Reply queue created for slot [%d]", slot);
                                s_rq_save_pending = slot;  // deferred NVS save from app task
                                // Bug #30: removed subscribe_all - RQ already subscribed via subMode='S'
                                ESP_LOGI(TAG, "NET: skip subscribe_all - RQ[%d] subscribed via subMode=S", slot);
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

/* Session 48: app_request_subscribe_all() removed. First subscribe in
 * smp_connect() is sufficient. SUB on already-subscribed queue is noop. */

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

    /* Session 48: Boot is truly done - signal splash screen */
    {
#if defined(CONFIG_NVS_ENCRYPTION)
        bool vault = true;
#else
        bool vault = false;
#endif
        uint8_t pq = smp_settings_get_pq_enabled();
        int layers = pq ? 5 : 4;
        if (vault && pq) {
            char msg[64];
            snprintf(msg, sizeof(msg), "eFuse sealed. %d layers. Quantum-ready.", layers);
            ui_splash_set_status(msg);
        } else if (vault) {
            char msg[64];
            snprintf(msg, sizeof(msg), "eFuse sealed. %d layers active.", layers);
            ui_splash_set_status(msg);
        } else if (pq) {
            char msg[64];
            snprintf(msg, sizeof(msg), "%d layers active. Quantum-ready.", layers);
            ui_splash_set_status(msg);
        } else {
            ui_splash_set_status("All systems go.");
        }
        ui_splash_set_progress(100);
    }
}

static void app_process_deferred_work(void)
{
    if (s_contacts_dirty) {
        s_contacts_dirty = false;
        save_contacts_to_nvs();
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
