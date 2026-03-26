/**
 * SimpleGo - smp_rotation.c
 * Queue Rotation: migrate contacts from old server to new server
 *
 * Binary format reference (agent-protocol.md ABNF):
 *
 *   QADD = "QA" sndQueues
 *   sndQueues = length 1*(newQueueUri replacedSndQueue)
 *   newQueueUri = clientVRange smpServer senderId dhPublicKey [sndSecure]
 *   replacedSndQueue = "0" / "1" sndQueueAddr
 *   sndQueueAddr = smpServer senderId
 *   smpServer = hosts port keyHash
 *   hosts = length 1*host
 *   host = shortString
 *   port = shortString
 *   keyHash = shortString
 *   senderId = shortString
 *   clientVRange = version version   (each version = 2 bytes BE)
 *   dhPublicKey = length x509encoded (SPKI: 12B header + 32B key = 44B)
 *   shortString = length *OCTET      (1 byte length prefix)
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_rotation.h"
#include "smp_tasks.h"          // smp_tasks_reset_rotation_guard()
#include "smp_servers.h"
#include "smp_contacts.h"
#include "smp_queue.h"
#include "reply_queue.h"        // Per-contact reply queues (RQ[i])
#include "smp_storage.h"
#include "smp_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"      // heap_caps_calloc for PSRAM
#include "sodium.h"

static const char *TAG = "ROTATION";

// ============== NVS Keys ==============

#define ROT_NVS_CTX     "rot_ctx"       // Global rotation context
#define ROT_NVS_PREFIX  "rot_c"         // Per-contact: "rot_c00" to "rot_c7f"

// ============== SPKI Headers ==============

static const uint8_t X25519_SPKI_HDR[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

// ============== Runtime State ==============

static rotation_context_t s_ctx = {0};
static rotation_contact_data_t *s_contacts = NULL;  // PSRAM, allocated in rotation_init()
static bool s_initialized = false;
static bool s_rq_subs_needed = false;  // Set when rotation completes, cleared after RQ SUBs
static uint8_t s_cq_peer_e2e[32];     // CQ E2E peer key, survives rotation cleanup
static bool s_has_cq_peer_e2e = false;

// ============== NVS Persistence ==============

static void save_context(void)
{
    esp_err_t ret = smp_storage_save_blob_sync(ROT_NVS_CTX, &s_ctx, sizeof(s_ctx));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save rotation context: %s", esp_err_to_name(ret));
    }
}

static void save_contact_data(int idx)
{
    if (!s_contacts || idx < 0 || idx >= 128) return;

    char key[12];
    snprintf(key, sizeof(key), "%s%02x", ROT_NVS_PREFIX, idx);
    esp_err_t ret = smp_storage_save_blob_sync(key, &s_contacts[idx],
                                                sizeof(rotation_contact_data_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save rotation data for contact [%d]: %s",
                 idx, esp_err_to_name(ret));
    }
}

static bool load_context(void)
{
    if (!smp_storage_exists(ROT_NVS_CTX)) return false;

    size_t len = 0;
    esp_err_t ret = smp_storage_load_blob(ROT_NVS_CTX, &s_ctx, sizeof(s_ctx), &len);
    if (ret != ESP_OK || len != sizeof(s_ctx)) {
        memset(&s_ctx, 0, sizeof(s_ctx));
        return false;
    }
    return true;
}

static bool load_contact_data(int idx)
{
    if (!s_contacts || idx < 0 || idx >= 128) return false;

    char key[12];
    snprintf(key, sizeof(key), "%s%02x", ROT_NVS_PREFIX, idx);

    if (!smp_storage_exists(key)) return false;

    size_t len = 0;
    esp_err_t ret = smp_storage_load_blob(key, &s_contacts[idx],
                                           sizeof(rotation_contact_data_t), &len);
    if (ret != ESP_OK || len != sizeof(rotation_contact_data_t)) {
        memset(&s_contacts[idx], 0, sizeof(rotation_contact_data_t));
        return false;
    }
    return true;
}

static void clear_all_rotation_nvs(void)
{
    smp_storage_delete(ROT_NVS_CTX);
    for (int i = 0; i < 128; i++) {
        char key[12];
        snprintf(key, sizeof(key), "%s%02x", ROT_NVS_PREFIX, i);
        smp_storage_delete(key);
    }
}

// ============== Helper: Encode smpServer binary ==============
// Returns bytes written, or -1 on error.
// Format: hosts(length 1*host) port keyHash
//   host = shortString, port = shortString, keyHash = shortString

static int encode_smp_server(uint8_t *buf, int buf_size,
                              const char *host, uint16_t port,
                              const uint8_t *key_hash)
{
    int p = 0;
    int host_len = (int)strlen(host);
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    int port_len = (int)strlen(port_str);

    // Minimum size: 1 + 1 + host_len + 1 + port_len + 1 + 32
    int needed = 1 + 1 + host_len + 1 + port_len + 1 + 32;
    if (p + needed > buf_size) return -1;

    // hosts: [count=1][len][host]
    buf[p++] = 1;                              // 1 host
    buf[p++] = (uint8_t)host_len;
    memcpy(&buf[p], host, host_len);
    p += host_len;

    // port: [len][port_str]
    buf[p++] = (uint8_t)port_len;
    memcpy(&buf[p], port_str, port_len);
    p += port_len;

    // keyHash: [len=32][hash]
    buf[p++] = 32;
    memcpy(&buf[p], key_hash, 32);
    p += 32;

    return p;
}

// ============== Public API ==============

void rotation_init(void)
{
    if (s_initialized) return;

    memset(&s_ctx, 0, sizeof(s_ctx));

    /* Allocate per-contact rotation data in PSRAM (~93 KB for 128 contacts) */
    if (!s_contacts) {
        s_contacts = (rotation_contact_data_t *)heap_caps_calloc(
            128, sizeof(rotation_contact_data_t), MALLOC_CAP_SPIRAM);
        if (!s_contacts) {
            ESP_LOGE(TAG, "Failed to allocate rotation contacts in PSRAM!");
            return;
        }
        ESP_LOGI(TAG, "Rotation contacts allocated: %zu bytes in PSRAM",
                 128 * sizeof(rotation_contact_data_t));
    }

    if (load_context() && s_ctx.state == ROT_GLOBAL_ACTIVE) {
        ESP_LOGW(TAG, "Resuming active rotation to server [%d] %s",
                 s_ctx.target_server_idx, s_ctx.target_host);

        // Restore per-contact data
        int restored = 0;
        for (int i = 0; i < 128; i++) {
            if (load_contact_data(i) && s_contacts[i].state != ROT_IDLE) {
                restored++;
            }
        }
        ESP_LOGI(TAG, "Restored rotation state for %d contacts "
                 "(done=%d, waiting=%d, error=%d)",
                 restored, s_ctx.contacts_done,
                 s_ctx.contacts_waiting, s_ctx.contacts_error);
    } else {
        memset(&s_ctx, 0, sizeof(s_ctx));
    }

    s_initialized = true;
}

bool rotation_start(uint8_t target_server_idx)
{
    if (s_ctx.state == ROT_GLOBAL_ACTIVE) {
        ESP_LOGE(TAG, "Rotation already active!");
        return false;
    }

    smp_server_t *srv = smp_servers_get(target_server_idx);
    if (!srv) {
        ESP_LOGE(TAG, "Invalid target server index %d", target_server_idx);
        return false;
    }

    // Count active contacts
    int active_count = 0;
    for (int i = 0; i < 128; i++) {
        if (contacts_db.contacts && contacts_db.contacts[i].active) {
            active_count++;
        }
    }

    if (active_count == 0) {
        ESP_LOGI(TAG, "No active contacts - direct server switch (no rotation needed)");
        smp_servers_set_active(target_server_idx);
        return true;
    }

    // Initialize context
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = ROT_GLOBAL_ACTIVE;
    s_ctx.target_server_idx = target_server_idx;
    strncpy(s_ctx.target_host, srv->host, sizeof(s_ctx.target_host) - 1);
    s_ctx.target_port = srv->port;
    memcpy(s_ctx.target_key_hash, srv->key_hash, 32);
    s_ctx.contacts_total = (uint8_t)active_count;

    /* Fix 5: Save old server for nachzuegler after reboot */
    strncpy(s_ctx.old_host, our_queue.server_host, sizeof(s_ctx.old_host) - 1);
    s_ctx.old_port = our_queue.server_port;

    // Initialize per-contact state (re-allocate if freed after previous rotation)
    if (!s_contacts) {
        s_contacts = (rotation_contact_data_t *)heap_caps_calloc(
            128, sizeof(rotation_contact_data_t), MALLOC_CAP_SPIRAM);
        if (!s_contacts) {
            ESP_LOGE(TAG, "Failed to allocate rotation contacts array");
            return false;
        }
        s_initialized = true;
        ESP_LOGI(TAG, "Rotation contacts re-allocated: %zu bytes in PSRAM",
                 128 * sizeof(rotation_contact_data_t));
    }
    memset(s_contacts, 0, 128 * sizeof(rotation_contact_data_t));

    /* Session 50 Fix A: Reset the live-switch guard so it fires again */
    smp_tasks_reset_rotation_guard();

    // Persist before any network ops (Evgeny's golden rule)
    save_context();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [ROT] QUEUE ROTATION STARTED          |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "  Target: %s:%d", s_ctx.target_host, s_ctx.target_port);
    ESP_LOGI(TAG, "  Contacts to migrate: %d", active_count);
    ESP_LOGI(TAG, "");

    return true;
}

bool rotation_is_active(void)
{
    return s_ctx.state == ROT_GLOBAL_ACTIVE;
}

const rotation_context_t *rotation_get_context(void)
{
    return &s_ctx;
}

rotation_contact_state_t rotation_get_contact_state(int contact_idx)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return ROT_IDLE;
    return s_contacts[contact_idx].state;
}

const rotation_contact_data_t *rotation_get_contact_data(int contact_idx)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return NULL;
    return &s_contacts[contact_idx];
}

// ============== Phase 1: Create Queue on New Server ==============

bool rotation_create_queue_for_contact(int contact_idx)
{
    if (!rotation_is_active()) {
        ESP_LOGE(TAG, "rotation_create_queue: no active rotation");
        return false;
    }
    if (contact_idx < 0 || contact_idx >= 128) return false;
    if (!contacts_db.contacts || !contacts_db.contacts[contact_idx].active) {
        ESP_LOGE(TAG, "rotation_create_queue: contact [%d] not active", contact_idx);
        return false;
    }

    rotation_contact_data_t *rd = &s_contacts[contact_idx];

    // Generate keypairs for MAIN queue (same pattern as queue_create in smp_queue.c)
    ESP_LOGI(TAG, "[ROT][%d] Generating keypairs for new queues...", contact_idx);

    // Main queue: Ed25519 for command signing
    crypto_sign_keypair(rd->new_rcv_auth_public, rd->new_rcv_auth_private);
    // Main queue: X25519 for DH
    crypto_box_keypair(rd->new_rcv_dh_public, rd->new_rcv_dh_private);
    // Main queue: E2E keypair
    crypto_box_keypair(rd->new_e2e_public, rd->new_e2e_private);

    // Reply queue: Ed25519 for command signing
    crypto_sign_keypair(rd->rq_new_rcv_auth_public, rd->rq_new_rcv_auth_private);
    // Reply queue: X25519 for DH
    crypto_box_keypair(rd->rq_new_rcv_dh_public, rd->rq_new_rcv_dh_private);
    // Reply queue: E2E keypair
    crypto_box_keypair(rd->rq_new_e2e_public, rd->rq_new_e2e_private);
    rd->rq_created = false;

    // Persist keys before network operation (golden rule)
    rd->state = ROT_IDLE;  // Not yet created, just keys generated
    save_contact_data(contact_idx);

    ESP_LOGI(TAG, "[ROT][%d] Keys generated. Queue creation requires "
             "active TLS connection to %s:%d",
             contact_idx, s_ctx.target_host, s_ctx.target_port);
    ESP_LOGI(TAG, "[ROT][%d] NEW command must be sent on second TLS connection",
             contact_idx);

    // NOTE: Actual NEW command sending requires the second TLS connection
    // which is managed by smp_tasks.c. This function prepares the keys.
    // The NEW command itself will be sent by the rotation task in Phase 2
    // integration when we wire up the second TLS connection.
    //
    // For Phase 1 testing: mark as QUEUE_CREATED with dummy IDs.
    // In production: the caller sends NEW on the second connection,
    // parses IDS response, and fills in rcv_id/snd_id/srv_dh_public.

    return true;
}

// Called after successful NEW command on second TLS connection (main queue).
// Fills in the server-assigned IDs and computes shared secret.
// State only advances to ROT_QUEUE_CREATED after BOTH main + RQ are created.
bool rotation_complete_queue_creation(int contact_idx,
                                       const uint8_t *rcv_id, uint8_t rcv_id_len,
                                       const uint8_t *snd_id, uint8_t snd_id_len,
                                       const uint8_t *srv_dh_public)
{
    if (contact_idx < 0 || contact_idx >= 128) return false;
    rotation_contact_data_t *rd = &s_contacts[contact_idx];

    // Store IDs
    rd->new_rcv_id_len = rcv_id_len;
    memcpy(rd->new_rcv_id, rcv_id, rcv_id_len);
    rd->new_snd_id_len = snd_id_len;
    memcpy(rd->new_snd_id, snd_id, snd_id_len);
    memcpy(rd->new_srv_dh_public, srv_dh_public, 32);

    // Compute shared secret
    if (crypto_box_beforenm(rd->new_shared_secret,
                            rd->new_srv_dh_public,
                            rd->new_rcv_dh_private) != 0) {
        ESP_LOGE(TAG, "[ROT][%d] Main queue DH computation failed!", contact_idx);
        rd->state = ROT_ERROR;
        save_contact_data(contact_idx);
        return false;
    }

    // Advance state only if RQ is also created
    if (rd->rq_created) {
        rd->state = ROT_QUEUE_CREATED;
    }
    save_contact_data(contact_idx);

    ESP_LOGI(TAG, "[ROT][%d] Main queue created on %s. rcvId=%02x%02x... sndId=%02x%02x...",
             contact_idx, s_ctx.target_host,
             rd->new_rcv_id[0], rd->new_rcv_id[1],
             rd->new_snd_id[0], rd->new_snd_id[1]);

    return true;
}

// Called after successful NEW command for Reply Queue on second TLS connection.
bool rotation_complete_rq_creation(int contact_idx,
                                    const uint8_t *rcv_id, uint8_t rcv_id_len,
                                    const uint8_t *snd_id, uint8_t snd_id_len,
                                    const uint8_t *srv_dh_public)
{
    if (contact_idx < 0 || contact_idx >= 128) return false;
    rotation_contact_data_t *rd = &s_contacts[contact_idx];

    // Store RQ IDs
    rd->rq_new_rcv_id_len = rcv_id_len;
    memcpy(rd->rq_new_rcv_id, rcv_id, rcv_id_len);
    rd->rq_new_snd_id_len = snd_id_len;
    memcpy(rd->rq_new_snd_id, snd_id, snd_id_len);
    memcpy(rd->rq_new_srv_dh_public, srv_dh_public, 32);

    // Compute RQ shared secret
    if (crypto_box_beforenm(rd->rq_new_shared_secret,
                            rd->rq_new_srv_dh_public,
                            rd->rq_new_rcv_dh_private) != 0) {
        ESP_LOGE(TAG, "[ROT][%d] Reply queue DH computation failed!", contact_idx);
        rd->state = ROT_ERROR;
        save_contact_data(contact_idx);
        return false;
    }

    rd->rq_created = true;

    // Advance state only if main queue is also created (has snd_id)
    if (rd->new_snd_id_len > 0) {
        rd->state = ROT_QUEUE_CREATED;
    }
    save_contact_data(contact_idx);

    ESP_LOGI(TAG, "[ROT][%d] Reply queue created on %s. rcvId=%02x%02x... sndId=%02x%02x...",
             contact_idx, s_ctx.target_host,
             rd->rq_new_rcv_id[0], rd->rq_new_rcv_id[1],
             rd->rq_new_snd_id[0], rd->rq_new_snd_id[1]);

    return true;
}

// ============== QADD Payload Builder ==============

int rotation_build_qadd_payload(int contact_idx, uint8_t *buf, int buf_size)
{
    if (contact_idx < 0 || contact_idx >= 128) return -1;

    rotation_contact_data_t *rd = &s_contacts[contact_idx];
    if (rd->state != ROT_QUEUE_CREATED) {
        ESP_LOGE(TAG, "build_qadd: contact [%d] state is %d, need %d",
                 contact_idx, rd->state, ROT_QUEUE_CREATED);
        return -1;
    }

    // Need old queue info for replacedSndQueue
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "build_qadd: our_queue not valid");
        return -1;
    }

    int p = 0;

    // ---- QADD tag "QA" ----
    if (p + 2 > buf_size) return -1;
    buf[p++] = 'Q';
    buf[p++] = 'A';

    // ---- sndQueues = length 1*(newQueueUri replacedSndQueue) ----
    // Session 50: Two queues (Main + Reply Queue)
    if (p + 1 > buf_size) return -1;
    buf[p++] = 2;   // Two queues: Main (primary) + Reply Queue (secondary)

    // ==== Queue 1: Main receive queue ====

    // clientVRange - must match invite link format (SMP Client Version, not Server Version)
    // initialSMPClientVersion=1, shortLinksSMPClientVersion=4
    if (p + 4 > buf_size) return -1;
    buf[p++] = 0x00; buf[p++] = 0x01;   // min = v1 (initialSMPClientVersion)
    buf[p++] = 0x00; buf[p++] = 0x04;   // max = v4 (shortLinksSMPClientVersion)

    // smpServer (target)
    int srv_len = encode_smp_server(&buf[p], buf_size - p,
                                     s_ctx.target_host,
                                     s_ctx.target_port,
                                     s_ctx.target_key_hash);
    if (srv_len < 0) return -1;
    p += srv_len;

    // senderId (new main queue)
    if (p + 1 + rd->new_snd_id_len > buf_size) return -1;
    buf[p++] = rd->new_snd_id_len;
    memcpy(&buf[p], rd->new_snd_id, rd->new_snd_id_len);
    p += rd->new_snd_id_len;

    // dhPublicKey (X25519 SPKI, 44 bytes)
    if (p + 1 + 44 > buf_size) return -1;
    buf[p++] = 44;
    memcpy(&buf[p], X25519_SPKI_HDR, 12);
    p += 12;
    memcpy(&buf[p], rd->new_e2e_public, 32);
    p += 32;

    // queueMode: Nothing = 0 bytes after dhPublicKey
    // shortLinksSMPClientVersion = 4, our minV=8 >= 4, so parser expects queueMode.
    // maybe "" smpEncode queueMode -> Nothing = empty, Just QMMessaging = "M", Just QMContact = "C"
    // We send Nothing (no extra byte) for a standard messaging queue.

    // replacedSndQueue = "1" (Just) + SndQAddr(old server, old senderId)
    // The App does findQ(addr, sndQueues) - addr must match what the App stored.
    if (p + 1 > buf_size) return -1;
    buf[p++] = '1';

    int old_main_srv = encode_smp_server(&buf[p], buf_size - p,
                                          our_queue.server_host,
                                          our_queue.server_port,
                                          our_queue.server_key_hash);
    if (old_main_srv < 0) return -1;
    p += old_main_srv;

    // Per-contact old senderId (from Reply Queue, not global our_queue)
    reply_queue_t *old_rq = reply_queue_get(contact_idx);
    if (!old_rq || !old_rq->valid) {
        ESP_LOGE(TAG, "[ROT][%d] No valid reply queue for old senderId!", contact_idx);
        return -1;
    }

    ESP_LOGI(TAG, "[ROT][%d] replacedSndQueue: %s:%d sndId=%02x%02x%02x%02x... (%d bytes)",
             contact_idx, our_queue.server_host, our_queue.server_port,
             old_rq->snd_id[0], old_rq->snd_id[1],
             old_rq->snd_id[2], old_rq->snd_id[3], old_rq->snd_id_len);

    if (p + 1 + old_rq->snd_id_len > buf_size) return -1;
    buf[p++] = (uint8_t)old_rq->snd_id_len;
    memcpy(&buf[p], old_rq->snd_id, old_rq->snd_id_len);
    p += old_rq->snd_id_len;

    // ==== Queue 2: Reply queue RQ[contact_idx] ====
    // Session 50: Activated. The App's secondary sndQueue sends to our CQ.
    // findQ in Haskell matches (SMPServer, SenderId) tuples against sqs.

    // clientVRange
    if (p + 4 > buf_size) return -1;
    buf[p++] = 0x00; buf[p++] = 0x01;   // min = v1
    buf[p++] = 0x00; buf[p++] = 0x04;   // max = v4

    // smpServer (same target)
    srv_len = encode_smp_server(&buf[p], buf_size - p,
                                 s_ctx.target_host,
                                 s_ctx.target_port,
                                 s_ctx.target_key_hash);
    if (srv_len < 0) return -1;
    p += srv_len;

    // senderId (new RQ)
    if (p + 1 + rd->rq_new_snd_id_len > buf_size) return -1;
    buf[p++] = rd->rq_new_snd_id_len;
    memcpy(&buf[p], rd->rq_new_snd_id, rd->rq_new_snd_id_len);
    p += rd->rq_new_snd_id_len;

    // dhPublicKey (RQ E2E, X25519 SPKI)
    if (p + 1 + 44 > buf_size) return -1;
    buf[p++] = 44;
    memcpy(&buf[p], X25519_SPKI_HDR, 12);
    p += 12;
    memcpy(&buf[p], rd->rq_new_e2e_public, 32);
    p += 32;

    // replacedSndQueue for Queue 2: "1" + sndQueueAddr(old CQ on old server)
    // The App's SECONDARY sndQueue sends to our CQ with senderId = our_queue.snd_id.
    // This is the snd_id from the initial queue creation (invite link).
    // findQ in Haskell matches (SMPServer, SenderId) - must match what App stored.
    if (p + 1 > buf_size) return -1;
    buf[p++] = '1';

    int old_rq_srv = encode_smp_server(&buf[p], buf_size - p,
                                        our_queue.server_host,
                                        our_queue.server_port,
                                        our_queue.server_key_hash);
    if (old_rq_srv < 0) return -1;
    p += old_rq_srv;

    // SenderId = our_queue.snd_id (what the App uses to SEND to our CQ)
    if (our_queue.snd_id_len > 0) {
        if (p + 1 + our_queue.snd_id_len > buf_size) return -1;
        buf[p++] = (uint8_t)our_queue.snd_id_len;
        memcpy(&buf[p], our_queue.snd_id, our_queue.snd_id_len);
        p += our_queue.snd_id_len;
        ESP_LOGI(TAG, "[ROT][%d] Queue 2 replacedSndQueue: %s:%d sndId=%02x%02x%02x%02x... (%d bytes)",
                 contact_idx, our_queue.server_host, our_queue.server_port,
                 our_queue.snd_id[0], our_queue.snd_id[1],
                 our_queue.snd_id[2], our_queue.snd_id[3], our_queue.snd_id_len);
    } else {
        ESP_LOGW(TAG, "[ROT][%d] No our_queue.snd_id, using empty", contact_idx);
        buf[p++] = 0;
    }

    ESP_LOGI(TAG, "[ROT][%d] QADD payload: %d bytes (count=2), new=%s:%d sndId=%02x%02x...",
             contact_idx, p, s_ctx.target_host, s_ctx.target_port,
             rd->new_snd_id[0], rd->new_snd_id[1]);

    return p;
}

// ============== QKEY Handler ==============

bool rotation_handle_qkey(int contact_idx, const uint8_t *qkey_data, int qkey_len)
{
    if (contact_idx < 0 || contact_idx >= 128) return false;

    rotation_contact_data_t *rd = &s_contacts[contact_idx];
    if (rd->state != ROT_QADD_SENT && rd->state != ROT_WAITING) {
        ESP_LOGW(TAG, "[ROT][%d] Unexpected QKEY in state %d", contact_idx, rd->state);
        return false;
    }

    /*
     * QKEY = "QK" sndQueueKeys
     * sndQueueKeys = length 1*(newQueueInfo senderKey)
     * newQueueInfo = version smpServer senderId dhPublicKey [sndSecure]
     * senderKey = length x509encoded
     *
     * We already consumed "QK" tag. qkey_data starts at sndQueueKeys.
     * Session 50 Etappe 2: Parse count queues (1 or 2).
     */

    int p = 0;
    if (p >= qkey_len) return false;

    uint8_t count = qkey_data[p++];
    if (count < 1) {
        ESP_LOGE(TAG, "[ROT][%d] QKEY: queue count = 0", contact_idx);
        return false;
    }

    ESP_LOGI(TAG, "[ROT][%d] QKEY: parsing %d queue(s)...", contact_idx, count);

    for (int q = 0; q < count && q < 2; q++) {
        ESP_LOGI(TAG, "[ROT][%d] QKEY Queue %d/%d at offset %d:", contact_idx, q + 1, count, p);

        /* newQueueInfo: version(2) + smpServer + senderId + dhPublicKey */

        /* version = 2 bytes */
        if (p + 2 > qkey_len) {
            ESP_LOGE(TAG, "[ROT][%d] QKEY Q%d: truncated at version", contact_idx, q + 1);
            return false;
        }
        p += 2;

        /* smpServer = hosts port keyHash */
        if (p >= qkey_len) return false;
        uint8_t host_count = qkey_data[p++];
        for (int h = 0; h < host_count; h++) {
            if (p >= qkey_len) return false;
            uint8_t hlen = qkey_data[p++];
            p += hlen;
        }
        /* port */
        if (p >= qkey_len) return false;
        uint8_t port_len = qkey_data[p++];
        p += port_len;
        /* keyHash */
        if (p >= qkey_len) return false;
        uint8_t hash_len = qkey_data[p++];
        p += hash_len;

        /* senderId */
        if (p >= qkey_len) return false;
        uint8_t sid_len = qkey_data[p++];
        p += sid_len;

        /* dhPublicKey (X25519 SPKI: 12 hdr + 32 raw key) */
        if (p >= qkey_len) return false;
        uint8_t dh_len = qkey_data[p++];

        /* Select target fields based on queue index */
        uint8_t *dst_e2e = (q == 0) ? rd->peer_e2e_public : rd->rq_peer_e2e_public;
        bool *dst_e2e_valid = (q == 0) ? &rd->has_peer_e2e_public : &rd->has_rq_peer_e2e_public;
        const char *q_label = (q == 0) ? "Main" : "RQ";

        if (dh_len == 44 && p + 44 <= qkey_len &&
            memcmp(&qkey_data[p], "\x30\x2a\x30\x05\x06\x03\x2b\x65\x6e\x03\x21\x00", 12) == 0) {
            memcpy(dst_e2e, &qkey_data[p + 12], 32);
            *dst_e2e_valid = true;
            ESP_LOGI(TAG, "[ROT][%d] QKEY Q%d (%s) dhPublicKey: %02x%02x%02x%02x...",
                     contact_idx, q + 1, q_label,
                     dst_e2e[0], dst_e2e[1], dst_e2e[2], dst_e2e[3]);
        } else if (dh_len == 32 && p + 32 <= qkey_len) {
            memcpy(dst_e2e, &qkey_data[p], 32);
            *dst_e2e_valid = true;
            ESP_LOGI(TAG, "[ROT][%d] QKEY Q%d (%s) dhPublicKey (raw 32B): %02x%02x%02x%02x...",
                     contact_idx, q + 1, q_label,
                     dst_e2e[0], dst_e2e[1], dst_e2e[2], dst_e2e[3]);
        } else {
            ESP_LOGW(TAG, "[ROT][%d] QKEY Q%d (%s) dhPublicKey: unexpected len=%d",
                     contact_idx, q + 1, q_label, dh_len);
        }
        p += dh_len;

        /* senderKey = length x509encoded (Ed25519 SPKI, 44 bytes) */
        if (p >= qkey_len) {
            ESP_LOGE(TAG, "[ROT][%d] QKEY Q%d: truncated at senderKey", contact_idx, q + 1);
            return false;
        }
        uint8_t sender_key_len = qkey_data[p++];

        uint8_t *dst_sk = (q == 0) ? rd->peer_sender_key : rd->rq_peer_sender_key;
        bool *dst_sk_valid = (q == 0) ? &rd->has_peer_sender_key : &rd->has_rq_peer_sender_key;

        if (sender_key_len == 44 && p + 44 <= qkey_len) {
            memcpy(dst_sk, &qkey_data[p], 44);
            *dst_sk_valid = true;
            ESP_LOGI(TAG, "[ROT][%d] QKEY Q%d (%s) senderKey: %02x%02x%02x%02x...",
                     contact_idx, q + 1, q_label,
                     dst_sk[0], dst_sk[1], dst_sk[2], dst_sk[3]);
        } else {
            ESP_LOGW(TAG, "[ROT][%d] QKEY Q%d (%s) senderKey: unexpected len=%d",
                     contact_idx, q + 1, q_label, sender_key_len);
        }
        p += sender_key_len;
    }

    rd->state = ROT_QKEY_RECEIVED;
    save_contact_data(contact_idx);

    ESP_LOGI(TAG, "[ROT][%d] QKEY received! %d queue(s) parsed, Main=%s RQ=%s",
             contact_idx, count,
             rd->has_peer_sender_key ? "OK" : "MISSING",
             rd->has_rq_peer_sender_key ? "OK" : "MISSING");

    return true;
}

// ============== Mark QADD Sent ==============

void rotation_mark_qadd_sent(int contact_idx)
{
    if (contact_idx < 0 || contact_idx >= 128) return;
    s_contacts[contact_idx].state = ROT_QADD_SENT;
    save_contact_data(contact_idx);
    ESP_LOGI(TAG, "[ROT][%d] QADD sent, waiting for QKEY...", contact_idx);
}

// ============== Phase 2: QUSE Payload Builder ==============

int rotation_build_quse_payload(int contact_idx, uint8_t *buf, int buf_size)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return -1;

    const rotation_contact_data_t *rd = &s_contacts[contact_idx];
    int p = 0;

    /* QUSE = "QU" + count + (SndQAddr + Bool)* 
     * SndQAddr = SMPServer + SenderId
     * Bool = 'T' (primary) or 'F' (not primary) */

    /* Tag: "QU" */
    if (p + 2 > buf_size) return -1;
    buf[p++] = 'Q';
    buf[p++] = 'U';

    /* count = 1 (single queue, matching QADD Test A) */
    if (p + 1 > buf_size) return -1;
    buf[p++] = 1;

    /* SndQAddr: SMPServer(new) + SenderId(new_snd_id) */
    int srv_len = encode_smp_server(&buf[p], buf_size - p,
                                     s_ctx.target_host,
                                     s_ctx.target_port,
                                     s_ctx.target_key_hash);
    if (srv_len < 0) return -1;
    p += srv_len;

    /* SenderId: new_snd_id (length-prefixed) */
    if (p + 1 + rd->new_snd_id_len > buf_size) return -1;
    buf[p++] = rd->new_snd_id_len;
    memcpy(&buf[p], rd->new_snd_id, rd->new_snd_id_len);
    p += rd->new_snd_id_len;

    /* Primary flag: 'T' = True */
    if (p + 1 > buf_size) return -1;
    buf[p++] = 'T';

    ESP_LOGI(TAG, "[ROT][%d] QUSE payload: %d bytes, server=%s:%d sndId=%02x%02x...",
             contact_idx, p, s_ctx.target_host, s_ctx.target_port,
             rd->new_snd_id[0], rd->new_snd_id[1]);
    return p;
}

// ============== Phase 2: State Transitions ==============

void rotation_mark_key_sent(int contact_idx)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return;
    s_contacts[contact_idx].state = ROT_KEY_SENT;
    save_contact_data(contact_idx);
    ESP_LOGI(TAG, "[ROT][%d] KEY sent on new queue, ready for QUSE", contact_idx);
}

void rotation_mark_quse_sent(int contact_idx)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return;
    /* QUSE sent - now need QTEST before contact is truly DONE */
    s_contacts[contact_idx].state = ROT_QUSE_SENT;
    save_contact_data(contact_idx);
    ESP_LOGI(TAG, "[ROT][%d] QUSE sent, need QTEST to finalize", contact_idx);
}

// ============== Phase 3: QTEST Received ==============
// We do NOT send QTEST. The App sends QTEST to us on the NEW queue
// after it receives our QUSE. When we receive QTEST, rotation is done.

void rotation_mark_qtest_received(int contact_idx)
{
    if (!s_contacts || contact_idx < 0 || contact_idx >= 128) return;
    s_contacts[contact_idx].state = ROT_DONE;
    save_contact_data(contact_idx);

    /* Update global counters */
    s_ctx.contacts_done++;
    save_context();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [ROT][%d] ROTATION COMPLETE!           |", contact_idx);
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "  Contact migrated to %s:%d",
             s_ctx.target_host, s_ctx.target_port);
    ESP_LOGI(TAG, "  Progress: %d/%d contacts done",
             s_ctx.contacts_done, s_ctx.contacts_total);

    /* Check if ALL contacts are now DONE - if so, clean up rotation */
    bool all_done = true;
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        if (s_contacts[i].state != ROT_DONE && s_contacts[i].state != ROT_IDLE) {
            all_done = false;
            break;
        }
    }
    if (all_done) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "+========================================+");
        ESP_LOGI(TAG, "|  ALL QTEST RECEIVED - ROTATION DONE    |");
        ESP_LOGI(TAG, "+========================================+");

        /* Log SRAM before cleanup */
        ESP_LOGI(TAG, "  SRAM before cleanup: Internal=%zu, PSRAM=%zu",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        /* Zero all rotation data and clear NVS */
        for (int i = 0; i < 128; i++) {
            if (s_contacts[i].state != ROT_IDLE) {
                sodium_memzero(&s_contacts[i], sizeof(rotation_contact_data_t));
            }
        }
        memset(&s_ctx, 0, sizeof(s_ctx));
        clear_all_rotation_nvs();

        /* Free the 94 KB PSRAM rotation contacts array */
        heap_caps_free(s_contacts);
        s_contacts = NULL;
        s_initialized = false;

        ESP_LOGI(TAG, "  SRAM after cleanup: Internal=%zu, PSRAM=%zu",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, "  Rotation state cleared. Server switch fully complete.");
        s_rq_subs_needed = true;
    }
}

// ============== Complete (live-switch, no reboot) ==============

void rotation_complete(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+========================================+");
    ESP_LOGI(TAG, "|  QUEUE ROTATION - LIVE SWITCH          |");
    ESP_LOGI(TAG, "+========================================+");

    int migrated = 0;
    int pending = 0;

    /* ---- Phase 1: Migrate per-contact credentials ---- */
    /* Migrate for QUSE_SENT (App got our QUSE, will send QTEST on new queue)
     * and ROT_DONE (QTEST already received). */
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;

        if (s_contacts[i].state != ROT_DONE && s_contacts[i].state != ROT_QUSE_SENT) {
            pending++;
            ESP_LOGW(TAG, "  [%d] NOT READY (state=%d) - keeping old credentials",
                     i, s_contacts[i].state);
            continue;
        }

        const rotation_contact_data_t *rd = &s_contacts[i];
        contact_t *c = &contacts_db.contacts[i];
        reply_queue_t *rq = reply_queue_get(i);

        /* Main queue credentials update.
         *
         * DH keys dilemma after rotation:
         * - RECEIVING on new server needs NEW rcv_dh (server encrypted with new DH)
         * - SENDING via peer needs OLD rcv_dh (peer server hasn't changed)
         *
         * Solution: save old DH to rq->peer_dh_* for peer_send, then update
         * contact->rcv_dh_* to new values for server-level decrypt. */

        /* Save old DH keys for peer-send before overwriting.
         * Only on FIRST rotation: peer server never changes, so it always
         * expects the ORIGINAL keys. Second rotation must not overwrite. */
        if (rq && !rq->has_peer_dh) {
            memcpy(rq->peer_dh_secret, c->rcv_dh_secret, 32);
            memcpy(rq->peer_dh_public, c->rcv_dh_public, 32);
            rq->has_peer_dh = true;
        }

        /* Update IDs */
        memcpy(c->recipient_id, rd->new_rcv_id, rd->new_rcv_id_len);
        c->recipient_id_len = rd->new_rcv_id_len;

        /* Update auth keys (SUB on new server needs them) */
        memcpy(c->rcv_auth_public, rd->new_rcv_auth_public, 32);
        memcpy(c->rcv_auth_secret, rd->new_rcv_auth_private, 64);

        /* Update DH keys (decrypt on new server needs them) */
        memcpy(c->rcv_dh_public, rd->new_rcv_dh_public, 32);
        memcpy(c->rcv_dh_secret, rd->new_rcv_dh_private, 32);

        /* Update server DH */
        memcpy(c->srv_dh_public, rd->new_srv_dh_public, 32);
        c->have_srv_dh = true;

        /* Reply queue: update IDs, server DH, shared secret, E2E keys.
         * Auth keys and DH keys need backup before overwriting (peer-send
         * still uses old values on the peer's server). */
        if (rq) {
            memcpy(rq->rcv_id, rd->rq_new_rcv_id, rd->rq_new_rcv_id_len);
            rq->rcv_id_len = rd->rq_new_rcv_id_len;
            /* Session 50 Fix C: rq->snd_id must store the MAIN queue snd_id,
             * not the RQ snd_id. Reason: rotation_build_qadd_payload() reads
             * rq->snd_id for the replacedSndQueue field, and the App's findQ()
             * matches against the snd_id from QUSE - which is the Main queue. */
            memcpy(rq->snd_id, rd->new_snd_id, rd->new_snd_id_len);
            rq->snd_id_len = rd->new_snd_id_len;

            /* Session 50 Fix B: Save old auth keys for peer-send before overwriting.
             * Only on FIRST rotation: peer server never changes, so it always
             * expects the ORIGINAL keys. Second rotation must not overwrite. */
            if (!rq->has_peer_auth) {
                memcpy(rq->peer_auth_private, rq->rcv_auth_private, 64);
                memcpy(rq->peer_auth_public, rq->rcv_auth_public, 32);
                rq->has_peer_auth = true;
            }

            /* NOW update auth keys (SUB on new server needs them) */
            memcpy(rq->rcv_auth_public, rd->rq_new_rcv_auth_public, 32);
            memcpy(rq->rcv_auth_private, rd->rq_new_rcv_auth_private, 64);

            /* rcv_dh_public/private: already backed up to peer_dh above */
            memcpy(rq->e2e_public, rd->rq_new_e2e_public, 32);
            memcpy(rq->e2e_private, rd->rq_new_e2e_private, 32);
            memcpy(rq->srv_dh_public, rd->rq_new_srv_dh_public, 32);
            memcpy(rq->shared_secret, rd->rq_new_shared_secret, 32);
            rq->valid = true;
        }

        ESP_LOGI(TAG, "  [%d] '%s' credentials migrated (rcvId=%02x%02x...)",
                 i, c->name, c->recipient_id[0], c->recipient_id[1]);
        migrated++;
    }

    ESP_LOGI(TAG, "  Migrated: %d, Pending: %d", migrated, pending);

    /* ---- Phase 1b: CQ E2E key update after rotation ---- */
    /* Session 50: BEIDE Operationen AKTIV + Cache-Invalidierung.
     * Haskell-Analyse bestaetigt: DH(new_e2e_private, peer_e2e_public) korrekt. */
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        rotation_contact_data_t *rd = &s_contacts[i];
        if (rd->state != ROT_DONE && rd->state != ROT_QUSE_SENT) continue;

        /* Operation 1: Update our E2E key to the one generated in QADD */
        memcpy(our_queue.e2e_public, rd->new_e2e_public, 32);
        memcpy(our_queue.e2e_private, rd->new_e2e_private, 32);
        ESP_LOGI(TAG, "  Phase1b Op1: our_queue.e2e updated from contact [%d]", i);

        /* Operation 2: Peer E2E Key aus QKEY */
        if (rd->has_peer_e2e_public) {
            memcpy(s_cq_peer_e2e, rd->peer_e2e_public, 32);
            s_has_cq_peer_e2e = true;
            smp_storage_save_blob_sync("cq_e2e_peer", s_cq_peer_e2e, 32);
            ESP_LOGI(TAG, "  Phase1b Op2: CQ E2E peer key saved");
        }
        break;  /* Single-contact MVP: first migrated contact */
    }

    /* Session 50: Invalidate CQ E2E peer cache in smp_tasks.c so next
     * CQ MSG reloads the fresh key written above. Without this, second
     * rotation uses stale peer key from first rotation. */
    smp_tasks_reset_rotation_guard();

    /* ---- Phase 2: Update our_queue server info ---- */
    strncpy(our_queue.server_host, s_ctx.target_host, sizeof(our_queue.server_host) - 1);
    our_queue.server_host[sizeof(our_queue.server_host) - 1] = '\0';
    our_queue.server_port = s_ctx.target_port;
    memcpy(our_queue.server_key_hash, s_ctx.target_key_hash, 32);

    ESP_LOGI(TAG, "  our_queue.server_host = %s:%d", our_queue.server_host, our_queue.server_port);

    /* ---- Phase 3: Persist everything to NVS ---- */
    ESP_LOGI(TAG, "  Saving contacts to NVS...");
    save_contacts_to_nvs();

    ESP_LOGI(TAG, "  Saving reply queues to NVS...");
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        if (s_contacts[i].state == ROT_DONE || s_contacts[i].state == ROT_QUSE_SENT) {
            reply_queue_save(i);
        }
    }

    ESP_LOGI(TAG, "  Saving our_queue to NVS...");
    smp_storage_save_blob_sync("queue_our", &our_queue, sizeof(our_queue_t));

    /* ---- Phase 4: Switch active server ---- */
    uint8_t target_idx = s_ctx.target_server_idx;
    smp_servers_set_active(target_idx);
    ESP_LOGI(TAG, "  Active server set to [%d] %s", target_idx, s_ctx.target_host);

    /* ---- Phase 5: Clean up ---- */
    /* Check how many contacts still need QTEST */
    int awaiting_qtest = 0;
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        if (s_contacts[i].state == ROT_QUSE_SENT) awaiting_qtest++;
    }

    if (pending == 0 && awaiting_qtest == 0) {
        /* All contacts truly DONE - clean slate */
        ESP_LOGI(TAG, "  All contacts fully migrated - clearing rotation state");

        /* Zero all rotation keys in RAM */
        for (int i = 0; i < 128; i++) {
            if (s_contacts[i].state != ROT_IDLE) {
                sodium_memzero(&s_contacts[i], sizeof(rotation_contact_data_t));
            }
        }
        memset(&s_ctx, 0, sizeof(s_ctx));
        clear_all_rotation_nvs();
    } else {
        /* Rotation stays active: waiting for QTEST or offline contacts */
        ESP_LOGW(TAG, "  %d awaiting QTEST, %d pending - rotation stays active",
                 awaiting_qtest, pending);
        save_context();
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+========================================+");
    ESP_LOGI(TAG, "|  LIVE SWITCH COMPLETE                  |");
    ESP_LOGI(TAG, "+========================================+");
    ESP_LOGI(TAG, "  Caller must now reconnect to %s", our_queue.server_host);
}

// ============== Helper APIs ==============

const char *rotation_get_target_host(void)
{
    if (s_ctx.state == ROT_GLOBAL_IDLE) return NULL;
    return s_ctx.target_host;
}

bool rotation_has_pending(void)
{
    if (s_ctx.state == ROT_GLOBAL_IDLE) return false;
    if (!s_contacts) return false;
    for (int i = 0; i < 128; i++) {
        if (!contacts_db.contacts[i].active) continue;
        if (s_contacts[i].state != ROT_IDLE && s_contacts[i].state != ROT_DONE) {
            return true;
        }
    }
    return false;
}

bool rotation_rq_subs_needed(void)
{
    if (s_rq_subs_needed) {
        s_rq_subs_needed = false;
        return true;
    }
    return false;
}

bool rotation_get_cq_peer_e2e(uint8_t *out_key)
{
    if (!s_has_cq_peer_e2e) {
        /* Try loading from NVS (survives reboot) */
        size_t len = 32;
        if (smp_storage_load_blob("cq_e2e_peer", s_cq_peer_e2e, 32, &len) == ESP_OK && len == 32) {
            s_has_cq_peer_e2e = true;
            ESP_LOGI(TAG, "CQ E2E peer key loaded from NVS: %02x%02x%02x%02x...",
                     s_cq_peer_e2e[0], s_cq_peer_e2e[1],
                     s_cq_peer_e2e[2], s_cq_peer_e2e[3]);
        }
    }
    if (s_has_cq_peer_e2e && out_key) {
        memcpy(out_key, s_cq_peer_e2e, 32);
        return true;
    }
    return false;
}

// ============== Abort ==============

void rotation_abort(void)
{
    ESP_LOGW(TAG, "Rotation aborted!");

    // Securely zero all rotation keys
    for (int i = 0; i < 128; i++) {
        if (s_contacts[i].state != ROT_IDLE) {
            sodium_memzero(&s_contacts[i], sizeof(rotation_contact_data_t));
        }
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    clear_all_rotation_nvs();

    ESP_LOGI(TAG, "Rotation state cleared. Old server remains active.");
}

// ============== State Name ==============

const char *rotation_state_name(rotation_contact_state_t state)
{
    switch (state) {
        case ROT_IDLE:            return "Idle";
        case ROT_QUEUE_CREATED:   return "Queue created";
        case ROT_QADD_SENT:       return "QADD sent";
        case ROT_QKEY_RECEIVED:   return "QKEY received";
        case ROT_KEY_SENT:        return "KEY sent";
        case ROT_QUSE_SENT:       return "QUSE sent (need QTEST)";
        case ROT_QTEST_SENT:      return "QTEST sent";
        case ROT_DONE:            return "Done";
        case ROT_ERROR:           return "Error";
        case ROT_WAITING:         return "Waiting (offline)";
        default:                  return "Unknown";
    }
}
