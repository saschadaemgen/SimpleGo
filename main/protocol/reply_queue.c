/**
 * SimpleGo - reply_queue.c
 * Per-Contact Reply Queue Management
 * Session 34, Phase 6 - Commit 1
 *
 * Implements per-contact reply queues in PSRAM.
 * Each contact gets its own SMP queue so KEY command
 * binds only that contact's peer, not blocking others.
 */

#include "reply_queue.h"
#include "smp_queue.h"       // our_queue (for server info)
#include "smp_types.h"       // MAX_CONTACTS
#include "smp_contacts.h"    // ED25519_SPKI_HEADER, X25519_SPKI_HEADER
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sodium.h"
#include "mbedtls/ssl.h"
#include "smp_network.h"   // smp_write_command_block, smp_read_block

static const char *TAG = "RQ";

// ============== Global PSRAM Pointer ==============

reply_queue_db_t *reply_queue_db = NULL;

// ============== Lifecycle ==============

bool reply_queues_init(void)
{
    if (reply_queue_db) {
        ESP_LOGD(TAG, "Reply queue DB already initialized");
        return true;
    }

    reply_queue_db = (reply_queue_db_t *)heap_caps_calloc(
        1, sizeof(reply_queue_db_t), MALLOC_CAP_SPIRAM);

    if (!reply_queue_db) {
        ESP_LOGE(TAG, "PSRAM alloc failed! Need %zu bytes", sizeof(reply_queue_db_t));
        return false;
    }

    ESP_LOGI(TAG, "Reply queue DB: %zu bytes PSRAM (%d slots)",
             sizeof(reply_queue_db_t), MAX_CONTACTS);
    return true;
}

// ============== Lookup ==============

reply_queue_t *reply_queue_get(int slot)
{
    if (!reply_queue_db || slot < 0 || slot >= MAX_CONTACTS) {
        return NULL;
    }
    return &reply_queue_db->queues[slot];
}

int find_reply_queue_by_rcv_id(const uint8_t *entity_id, int entity_len)
{
    if (!reply_queue_db || !entity_id || entity_len <= 0) {
        return -1;
    }

    for (int i = 0; i < MAX_CONTACTS; i++) {
        reply_queue_t *rq = &reply_queue_db->queues[i];
        if (!rq->valid) continue;
        if (rq->rcv_id_len == entity_len &&
            memcmp(rq->rcv_id, entity_id, entity_len) == 0) {
            return i;
        }
    }
    return -1;
}

// ============== NVS Persistence ==============

// NVS key format: "rq_XX" where XX = hex slot number
static void make_nvs_key(int slot, char *key_buf, int key_buf_size)
{
    snprintf(key_buf, key_buf_size, "rq_%02x", slot);
}

bool reply_queue_save(int slot)
{
    reply_queue_t *rq = reply_queue_get(slot);
    if (!rq || !rq->valid) {
        ESP_LOGE(TAG, "RQ[%d] save: invalid slot", slot);
        return false;
    }

    char key[16];
    make_nvs_key(slot, key, sizeof(key));

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RQ[%d] NVS open failed: %s", slot, esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(h, key, rq, sizeof(reply_queue_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RQ[%d] NVS set_blob failed: %s", slot, esp_err_to_name(err));
        nvs_close(h);
        return false;
    }

    err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RQ[%d] NVS commit failed: %s", slot, esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "RQ[%d] saved to NVS (%s, %zu bytes)", slot, key, sizeof(reply_queue_t));
    return true;
}

bool reply_queue_load(int slot)
{
    reply_queue_t *rq = reply_queue_get(slot);
    if (!rq) {
        ESP_LOGE(TAG, "RQ[%d] load: invalid slot", slot);
        return false;
    }

    // 35f GUARD: Never overwrite valid PSRAM data with NVS
    // reply_queue_create() sets valid=true in PSRAM but defers NVS save.
    // If subscribe_all_contacts() triggers load before NVS save,
    // the memset below would destroy the valid PSRAM data.
    if (rq->valid) {
        ESP_LOGD(TAG, "RQ[%d] already valid in PSRAM, skip NVS load", slot);
        return true;
    }

    char key[16];
    make_nvs_key(slot, key, sizeof(key));

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return false;  // Normal at first boot, no log spam
    }

    size_t len = sizeof(reply_queue_t);
    err = nvs_get_blob(h, key, rq, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(reply_queue_t)) {
        memset(rq, 0, sizeof(reply_queue_t));
        return false;
    }

    if (!rq->valid) {
        memset(rq, 0, sizeof(reply_queue_t));
        return false;
    }

    ESP_LOGI(TAG, "RQ[%d] loaded from NVS (rcv_id_len=%d)", slot, rq->rcv_id_len);
    return true;
}

int reply_queues_load_all(void)
{
    if (!reply_queue_db) {
        ESP_LOGE(TAG, "Cannot load: DB not initialized");
        return 0;
    }

    int loaded = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (reply_queue_load(i)) {
            loaded++;
        }
    }

    ESP_LOGI(TAG, "Loaded %d reply queues from NVS", loaded);
    return loaded;
}

// ============== Queue Creation (NEW command on main SSL) ==============

int reply_queue_create(mbedtls_ssl_context *ssl, uint8_t *block,
                       const uint8_t *session_id, int slot)
{
    if (!reply_queue_db || slot < 0 || slot >= MAX_CONTACTS) {
        ESP_LOGE(TAG, "RQ create: invalid params (slot=%d)", slot);
        return -1;
    }

    reply_queue_t *rq = &reply_queue_db->queues[slot];
    memset(rq, 0, sizeof(reply_queue_t));

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== RQ[%d] Creating reply queue on main SSL ===", slot);

    // --- Generate keypairs ---

    // Ed25519 auth keypair
    crypto_sign_keypair(rq->rcv_auth_public, rq->rcv_auth_private);

    // X25519 DH keypair (for server-level encryption)
    crypto_box_keypair(rq->rcv_dh_public, rq->rcv_dh_private);

    // X25519 E2E keypair (separate, for per-queue E2E encryption)
    crypto_box_keypair(rq->e2e_public, rq->e2e_private);

    ESP_LOGI(TAG, "RQ[%d] keypairs generated (auth+DH+E2E)", slot);

    // --- Build NEW command body (SMP v7 format, matching add_contact pattern) ---
    // Format: corrIdLen(1) + corrId + entityIdLen(0) + "NEW " + authKeyLen + authSPKI + dhKeyLen + dhSPKI + subMode

    // Build auth SPKI (12-byte header + 32-byte raw key = 44 bytes)
    uint8_t rcv_auth_spki[44];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, rq->rcv_auth_public, 32);

    // Build DH SPKI
    uint8_t rcv_dh_spki[44];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, rq->rcv_dh_public, 32);

    uint8_t trans_body[256];
    int pos = 0;

    // corrId: 1-byte length + simple identifier
    trans_body[pos++] = 1;
    trans_body[pos++] = 'r';  // 'r' for reply queue (distinct from add_contact '0'+slot)

    // entityId: empty
    trans_body[pos++] = 0;

    // Command: "NEW "
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';

    // Auth key: length prefix + SPKI (matching add_contact)
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_auth_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;

    // DH key: length prefix + SPKI (matching add_contact)
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_dh_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;

    // subMode: 'S' (subscribe immediately)
    trans_body[pos++] = 'S';

    int trans_body_len = pos;

    ESP_LOGI(TAG, "RQ[%d] NEW body: %d bytes", slot, trans_body_len);

    // --- Sign: [sessIdLen=32][sessionId][body] (matching add_contact) ---
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;

    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, rq->rcv_auth_private);

    // --- Build transmission: [sigLen=64][sig 64B][body] (matching add_contact) ---
    uint8_t transmission[256];
    int tpos = 0;

    transmission[tpos++] = crypto_sign_BYTES;
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;

    // v7: no sessionId on wire (only in signature)
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;

    // --- Send via smp_write_command_block (NOT direct ssl_write!) ---
    ESP_LOGI(TAG, "RQ[%d] Sending NEW command...", slot);
    int ret = smp_write_command_block(ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "RQ[%d] NEW send failed: %d", slot, ret);
        return -2;
    }
    ESP_LOGI(TAG, "RQ[%d] NEW sent (%d bytes)", slot, tpos);

    // --- Read IDS response via smp_read_block (NOT direct ssl_read!) ---
    int content_len = smp_read_block(ssl, block, 15000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "RQ[%d] IDS read failed: %d", slot, content_len);
        return -3;
    }

    // Parse IDS using linear scan (matching add_contact pattern)
    // smp_read_block returns block with [content_len 2B][txCount][txLen 2B][transmission...]
    // Linear scan is robust regardless of header structure
    uint8_t *resp = block + 2;
    int ids_found = 0;

    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
            int p = i + 4;

            // rcvId: length-prefixed
            if (p < content_len) {
                rq->rcv_id_len = resp[p++];
                if (rq->rcv_id_len > QUEUE_ID_SIZE) rq->rcv_id_len = QUEUE_ID_SIZE;
                if (p + rq->rcv_id_len <= content_len) {
                    memcpy(rq->rcv_id, &resp[p], rq->rcv_id_len);
                    p += rq->rcv_id_len;
                }
            }

            // sndId: length-prefixed
            if (p < content_len) {
                rq->snd_id_len = resp[p++];
                if (rq->snd_id_len > QUEUE_ID_SIZE) rq->snd_id_len = QUEUE_ID_SIZE;
                if (p + rq->snd_id_len <= content_len) {
                    memcpy(rq->snd_id, &resp[p], rq->snd_id_len);
                    p += rq->snd_id_len;
                }
            }

            // srvDhPub: length-prefixed SPKI (44 = 12 header + 32 key)
            if (p < content_len) {
                uint8_t srv_dh_len = resp[p++];
                if (srv_dh_len == 44 && p + srv_dh_len <= content_len) {
                    memcpy(rq->srv_dh_public, &resp[p + 12], 32);
                }
            }

            ids_found = 1;
            break;
        }

        // Check for ERR
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
            ESP_LOGE(TAG, "RQ[%d] Server error in IDS response", slot);
            return -4;
        }
    }

    if (!ids_found) {
        ESP_LOGE(TAG, "RQ[%d] IDS not found in response (%d bytes)", slot, content_len);
        return -4;
    }

    // --- Compute shared secret (rcv_dh_private * srv_dh_public) ---
    if (crypto_box_beforenm(rq->shared_secret, rq->srv_dh_public, rq->rcv_dh_private) != 0) {
        ESP_LOGE(TAG, "RQ[%d] DH shared secret computation failed", slot);
        return -7;
    }

    rq->valid = true;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+--------------------------------------------------+");
    ESP_LOGI(TAG, "|  RQ[%d] CREATED SUCCESSFULLY                      |", slot);
    ESP_LOGI(TAG, "+--------------------------------------------------+");
    ESP_LOGI(TAG, "  rcv_id (%d): %02x%02x%02x%02x...",
             rq->rcv_id_len, rq->rcv_id[0], rq->rcv_id[1], rq->rcv_id[2], rq->rcv_id[3]);
    ESP_LOGI(TAG, "  snd_id (%d): %02x%02x%02x%02x...",
             rq->snd_id_len, rq->snd_id[0], rq->snd_id[1], rq->snd_id[2], rq->snd_id[3]);

    // --- NVS persist deferred (network_task runs on PSRAM stack, ---
    // --- NVS/SPI flash disables cache = crash if stack in PSRAM) ---
    // Data is valid in PSRAM. Save from internal-SRAM task later.
    ESP_LOGI(TAG, "RQ[%d] NVS save deferred (PSRAM stack)", slot);

    return 0;
}

// ============== SMPQueueInfo Encoding ==============

int reply_queue_encode_info(int slot, uint8_t *buf, int max_len)
{
    reply_queue_t *rq = reply_queue_get(slot);
    if (!rq || !rq->valid) {
        ESP_LOGE(TAG, "RQ[%d] encode_info: invalid", slot);
        return -1;
    }

    // Server info from global our_queue (all queues on same server)
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "RQ[%d] encode_info: our_queue not valid (no server info)", slot);
        return -2;
    }

    // Format identical to queue_encode_info() but using per-contact data:
    // clientVersion(1B) + smpServer + senderId + dhPublicKey(E2E)
    //
    // smpServer = host_len(2B) + host + port_str_len(1B) + port_str
    //           + key_hash_len(1B) + key_hash(32B)
    // senderId  = len(1B) + snd_id
    // dhPublicKey = SPKI(44B) using e2e_public

    int p = 0;

    // clientVersion = 4 (2-byte BE, matching queue_encode_info)
    if (p + 1 >= max_len) return -3;
    buf[p++] = 0x00;
    buf[p++] = 0x04;

    // smpServer: host count + host
    int host_len = strlen(our_queue.server_host);
    if (p + 2 + host_len >= max_len) return -3;
    buf[p++] = 0x01;              // host count (WAS MISSING!)
    buf[p++] = (uint8_t)host_len; // 1-byte length (was 2-byte BE)
    memcpy(&buf[p], our_queue.server_host, host_len);
    p += host_len;

    // smpServer: port as string
    char port_str[8];
    int port_len = snprintf(port_str, sizeof(port_str), "%d", our_queue.server_port);
    if (p + 1 + port_len >= max_len) return -3;
    buf[p++] = (uint8_t)port_len;
    memcpy(&buf[p], port_str, port_len);
    p += port_len;

    // smpServer: key hash
    if (p + 1 + 32 >= max_len) return -3;
    buf[p++] = 32;
    memcpy(&buf[p], our_queue.server_key_hash, 32);
    p += 32;

    // senderId (per-contact snd_id)
    if (p + 1 + rq->snd_id_len >= max_len) return -3;
    buf[p++] = (uint8_t)rq->snd_id_len;
    memcpy(&buf[p], rq->snd_id, rq->snd_id_len);
    p += rq->snd_id_len;

    // dhPublicKey: length prefix + E2E public key as SPKI (per-contact)
    if (p + 1 + 44 > max_len) return -3;
    buf[p++] = 44;  // Length prefix (WAS MISSING!)
    memcpy(&buf[p], X25519_SPKI_HEADER, 12);
    memcpy(&buf[p + 12], rq->e2e_public, 32);
    p += 44;

    ESP_LOGI(TAG, "RQ[%d] encode_info: %d bytes (snd_id_len=%d)",
             slot, p, rq->snd_id_len);
    return p;
}
