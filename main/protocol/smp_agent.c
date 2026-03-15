/**
 * SimpleGo - smp_agent.c
 * SimpleX agent protocol layer implementation
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_agent.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "mbedtls/gcm.h"
#include "smp_ratchet.h"
#include "smp_peer.h"
#include "mbedtls/sha256.h"
#include "zstd.h"
#include "smp_tasks.h"  // Session 32: UI notification
#include "smp_contacts.h"  // 35e: contacts_db for contact_idx computation
#include "smp_history.h"  // Session 37: encrypted chat history on SD
#include <time.h>          // Session 37: time(NULL) for history timestamps

static const char *TAG = "SMP_AGENT";

// 35e: Current contact being processed (for UI routing)
static int s_agent_contact_idx = 0;

// ============================================================================
// Shared Ratchet Helpers
// ============================================================================

/**
 * Decrypt a ratchet message header using HKr then NHKr.
 *
 * @param em_header      EncMessageHeader data
 * @param em_header_len  Length of em_header
 * @param header_plain   OUT: decrypted header (caller must allocate eh_body_len + 16)
 * @param decrypt_mode   OUT: 0 = SameRatchet (HKr), 1 = AdvanceRatchet (NHKr)
 * @param eh_body_offset OUT: offset to body within em_header
 * @param eh_body_len    OUT: length of encrypted body
 * @return true on success
 */
static bool ratchet_header_try_decrypt(
    const uint8_t *em_header, uint16_t em_header_len,
    uint8_t *header_plain, int *decrypt_mode,
    int *eh_body_offset_out, uint16_t *eh_body_len_out)
{
    if (!ratchet_is_initialized()) {
        ESP_LOGE(TAG, "Ratchet NOT initialized");
        return false;
    }

    ratchet_state_t *rs = ratchet_get_state();

    // Parse EncMessageHeader structure
    uint16_t eh_version = (em_header[0] << 8) | em_header[1];
    const uint8_t *eh_iv   = &em_header[2];
    const uint8_t *eh_tag  = &em_header[18];

    uint16_t eh_body_len;
    int eh_body_offset;
    if (eh_version >= 3) {
        eh_body_len = (em_header[34] << 8) | em_header[35];
        eh_body_offset = 36;
    } else {
        eh_body_len = em_header[34];
        eh_body_offset = 35;
    }
    const uint8_t *eh_body = &em_header[eh_body_offset];

    ESP_LOGD(TAG, "ehVersion=%u, bodyLen=%u, bodyOff=%d", eh_version, eh_body_len, eh_body_offset);

    *eh_body_offset_out = eh_body_offset;
    *eh_body_len_out = eh_body_len;

    mbedtls_gcm_context gcm;
    int ret;
    bool decrypted = false;

    // Try 1: HKr (SameRatchet)
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, rs->header_key_recv, 256);
    ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                eh_iv, 16, rs->assoc_data, 112,
                eh_tag, 16, eh_body, header_plain);
    mbedtls_gcm_free(&gcm);
    if (ret == 0) { decrypted = true; *decrypt_mode = 0; }

    // Try 2: NHKr (AdvanceRatchet)
    if (!decrypted) {
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, rs->next_header_key_recv, 256);
        ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                    eh_iv, 16, rs->assoc_data, 112,
                    eh_tag, 16, eh_body, header_plain);
        mbedtls_gcm_free(&gcm);
        if (ret == 0) { decrypted = true; *decrypt_mode = 1; }
    }

    if (!decrypted) {
        ESP_LOGE(TAG, "Header decrypt FAILED (HKr + NHKr)");
    } else {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        ESP_LOGI(TAG, "      |  [OK] HEADER DECRYPT SUCCESS! (%s)     |",
                 *decrypt_mode == 0 ? "SameRatchet" : "AdvanceRatchet");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
    }

    return decrypted;
}

/**
 * Parse MsgHeader from decrypted header to extract peer DH key, PN, Ns.
 * Handles v2 and v3 formats including KEM field parsing.
 *
 * @param header_plain  Decrypted header plaintext
 * @param peer_dh       OUT: 56-byte peer DH public key
 * @param pn            OUT: previous chain message count
 * @param ns            OUT: current chain message number
 * @return true on success
 */
static bool ratchet_parse_msg_header(const uint8_t *header_plain,
                                      uint8_t *peer_dh, uint32_t *pn, uint32_t *ns)
{
    uint8_t key_len = header_plain[4];

    if (key_len != 68) {
        ESP_LOGE(TAG, "Unexpected DH key len %d (expected 68)", key_len);
        return false;
    }

    // DH key starts at offset 5, with 12-byte SPKI header + 56 bytes X448
    memcpy(peer_dh, &header_plain[17], 56);

    int mhp = 5 + key_len;
    uint16_t msg_version = (header_plain[2] << 8) | header_plain[3];

    // v3: KEM field parsing
    if (msg_version >= 3) {
        uint8_t kem_byte = header_plain[mhp];
        if (kem_byte == 0x30) {
            ESP_LOGD(TAG, "KEM: Nothing");
            mhp += 1;
        } else if (kem_byte == 0x31) {
            mhp += 1;
            uint8_t kem_state = header_plain[mhp++];
            ESP_LOGD(TAG, "KEM: Just, state=0x%02x", kem_state);
            if (kem_state == 0x50) {
                uint16_t pk_len = (header_plain[mhp] << 8) | header_plain[mhp + 1];
                mhp += 2 + pk_len;
            } else if (kem_state == 0x41) {
                uint16_t ct_len = (header_plain[mhp] << 8) | header_plain[mhp + 1];
                mhp += 2 + ct_len;
                uint16_t pk_len = (header_plain[mhp] << 8) | header_plain[mhp + 1];
                mhp += 2 + pk_len;
            }
        } else {
            ESP_LOGW(TAG, "Unknown KEM tag: 0x%02x", kem_byte);
            mhp += 1;
        }
    }

    *pn = (header_plain[mhp] << 24) | (header_plain[mhp + 1] << 16) |
          (header_plain[mhp + 2] << 8) | header_plain[mhp + 3];
    mhp += 4;
    *ns = (header_plain[mhp] << 24) | (header_plain[mhp + 1] << 16) |
          (header_plain[mhp + 2] << 8) | header_plain[mhp + 3];

    ESP_LOGD(TAG, "PN=%" PRIu32 ", Ns=%" PRIu32 "", *pn, *ns);
    return true;
}

/**
 * Full ratchet decrypt pipeline: header decrypt -> parse -> body decrypt.
 *
 * @param erm_data      Start of EncRatchetMessage data
 * @param erm_len       Length of EncRatchetMessage
 * @param original_len  Original message length (for bounds checking)
 * @param client_msg    Base pointer of client message (for body offset calc)
 * @param body_out      OUT: decrypted body (caller must free!)
 * @param body_out_len  OUT: decrypted body length
 * @return true on success
 */
static bool ratchet_decrypt_message(
    const uint8_t *client_msg, size_t original_len,
    size_t erm_offset,
    uint8_t **body_out, size_t *body_out_len)
{
    *body_out = NULL;
    *body_out_len = 0;

    const uint8_t *erm = client_msg + erm_offset;

    // Parse EncRatchetMessage structure
    // v3: 2-byte emHeader length prefix
    uint16_t em_header_len = (erm[0] << 8) | erm[1];
    const uint8_t *em_header = &erm[2];

    ESP_LOGD(TAG, "EncRatchetMessage: emHeader len=%u", em_header_len);

    // Pre-parse eh_body_len for correct allocation size
    int eh_body_offset, decrypt_mode;
    uint16_t eh_body_len;
    {
        uint16_t eh_ver = (em_header[0] << 8) | em_header[1];
        if (eh_ver >= 3) {
            eh_body_len = (em_header[34] << 8) | em_header[35];
        } else {
            eh_body_len = em_header[34];
        }
    }

    uint8_t *header_plain = malloc(eh_body_len + 16);
    if (!header_plain) return false;

    if (!ratchet_header_try_decrypt(em_header, em_header_len,
                                     header_plain, &decrypt_mode,
                                     &eh_body_offset, &eh_body_len)) {
        free(header_plain);
        return false;
    }

    // Parse MsgHeader
    uint8_t peer_dh[56];
    uint32_t hdr_pn, hdr_ns;
    if (!ratchet_parse_msg_header(header_plain, peer_dh, &hdr_pn, &hdr_ns)) {
        free(header_plain);
        return false;
    }

    // Session 46 Teil E: Parse PQ KEM fields from decrypted header
    // and feed to ratchet state machine BEFORE body decrypt.
    {
        parsed_msg_header_t pq_hdr;
        if (pq_header_deserialize(header_plain, eh_body_len, &pq_hdr) == 0) {
            ratchet_set_recv_pq(&pq_hdr);
            if (pq_hdr.kem_tag != PQ_KEM_NOTHING) {
                ESP_LOGI(TAG, "PQ: KEM tag=0x%02x pk_valid=%d ct_valid=%d",
                         pq_hdr.kem_tag, pq_hdr.kem_pk_valid, pq_hdr.kem_ct_valid);
            }
        } else {
            ratchet_set_recv_pq(NULL);
        }
    }

    // Body decrypt
    const uint8_t *body_auth_tag = em_header + em_header_len;
    const uint8_t *body_data = em_header + em_header_len + 16;

    size_t body_offset_in_msg = (size_t)(body_data - client_msg);
    if (body_offset_in_msg >= original_len) {
        ESP_LOGE(TAG, "emBody exceeds message bounds");
        free(header_plain);
        return false;
    }
    size_t body_data_len = original_len - body_offset_in_msg;

    // Verify auth tag is also in bounds
    size_t auth_offset_in_msg = (size_t)(body_auth_tag - client_msg);
    if (auth_offset_in_msg + 16 > original_len) {
        ESP_LOGE(TAG, "emAuthTag exceeds message bounds");
        free(header_plain);
        return false;
    }

    ESP_LOGD(TAG, "Body decrypt: len=%zu", body_data_len);

    uint8_t *body_plain = malloc(body_data_len + 16);
    if (!body_plain) {
        free(header_plain);
        return false;
    }

    size_t body_plain_len = 0;
    int ret = ratchet_decrypt_body(
        decrypt_mode == 0 ? RATCHET_MODE_SAME : RATCHET_MODE_ADVANCE,
        peer_dh, hdr_pn, hdr_ns,
        em_header, (size_t)em_header_len,
        body_auth_tag, body_data, body_data_len,
        body_plain, &body_plain_len);

    free(header_plain);

    if (ret != 0) {
        ESP_LOGE(TAG, "Ratchet body decrypt FAILED (%d)", ret);
        free(body_plain);
        return false;
    }

    *body_out = body_plain;
    *body_out_len = body_plain_len;
    return true;
}

// ============================================================================
// ConnInfo Handlers
// ============================================================================

/**
 * Handle 'D' tag (AgentConnInfoReply): find and log JSON.
 */
static void handle_conninfo_reply(const uint8_t *body, size_t body_len)
{
    ESP_LOGI(TAG, "Tag: 'D' = AgentConnInfoReply");
    for (size_t i = 1; i < body_len && i < 200; i++) {
        if (body[i] == '{') {
            int json_end = (body_len - i > 200) ? 200 : (int)(body_len - i);
            ESP_LOGI(TAG, "JSON at offset %zu: %.*s", i, json_end, &body[i]);
            break;
        }
    }
}

/**
 * Handle 'I' tag (ConnInfo): Zstd decompress and log JSON.
 */
static void handle_conninfo(const uint8_t *body, size_t body_len)
{
    ESP_LOGI(TAG, "Tag: 'I' = AgentMessage INFO (peer ConnInfo)");

    // Search for Zstd magic bytes (0x28B52FFD)
    int zstd_offset = -1;
    for (int i = 1; i < 16 && i + 3 < (int)body_len; i++) {
        if (body[i] == 0x28 && body[i + 1] == 0xb5 &&
            body[i + 2] == 0x2f && body[i + 3] == 0xfd) {
            zstd_offset = i;
            break;
        }
    }

    if (zstd_offset < 0) {
        ESP_LOGW(TAG, "No Zstd magic in first 16 bytes - trying raw body");

        // === Auftrag 35g Bug E Fallback: try displayName from raw body ===
        // Search body as string (may contain JSON without Zstd compression)
        char raw_search[256];
        size_t search_len = (body_len > 255) ? 255 : body_len;
        memcpy(raw_search, body, search_len);
        raw_search[search_len] = '\0';

        char *dn_raw = strstr(raw_search, "\"displayName\":\"");
        if (dn_raw) {
            dn_raw += 15;
            char *dn_raw_end = strchr(dn_raw, '"');
            if (dn_raw_end && (dn_raw_end - dn_raw) > 0 && (dn_raw_end - dn_raw) < 31) {
                char peer_name[32];
                int name_len = (int)(dn_raw_end - dn_raw);
                memcpy(peer_name, dn_raw, name_len);
                peer_name[name_len] = '\0';

                if (s_agent_contact_idx >= 0 && s_agent_contact_idx < MAX_CONTACTS &&
                    contacts_db.contacts[s_agent_contact_idx].active) {
                    strncpy(contacts_db.contacts[s_agent_contact_idx].name, peer_name, 31);
                    contacts_db.contacts[s_agent_contact_idx].name[31] = '\0';

                    ESP_LOGI(TAG, "Bug E FIX (raw): Contact [%d] name updated: '%s'",
                             s_agent_contact_idx, peer_name);

                    save_contact_single(s_agent_contact_idx);
                    smp_notify_ui_contact(peer_name);

                    // Session 47 2d: Notify UI of peer name for connection progress
                    smp_notify_ui_connect_name(s_agent_contact_idx, peer_name);
                }
            }
        }
        return;
    }

    const uint8_t *zstd_frame = &body[zstd_offset];
    size_t zstd_frame_len = body_len - zstd_offset;

    unsigned long long decomp_size = ZSTD_getFrameContentSize(zstd_frame, zstd_frame_len);
    if (decomp_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        decomp_size = 16384;
    } else if (decomp_size == ZSTD_CONTENTSIZE_ERROR) {
        ESP_LOGE(TAG, "Zstd: not valid compressed data");
        return;
    }

    if (decomp_size >= 65536) {
        ESP_LOGE(TAG, "Zstd: decompressed size too large (%llu)", decomp_size);
        return;
    }

    char *json_buf = (char *)malloc(decomp_size + 1);
    if (!json_buf) {
        ESP_LOGE(TAG, "malloc json_buf failed");
        return;
    }

    size_t result = ZSTD_decompress(json_buf, decomp_size, zstd_frame, zstd_frame_len);
    if (ZSTD_isError(result)) {
        ESP_LOGE(TAG, "Zstd decompress failed: %s", ZSTD_getErrorName(result));
        free(json_buf);
        return;
    }

    json_buf[result] = '\0';

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      |  [OK] ConnInfo JSON DECOMPRESSED!               |");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      Decompressed: %zu bytes", result);

    // Print in chunks to avoid log truncation
    size_t printed = 0;
    while (printed < result) {
        size_t chunk = result - printed;
        if (chunk > 200) chunk = 200;
        ESP_LOGI(TAG, "      %.*s", (int)chunk, &json_buf[printed]);
        printed += chunk;
    }

    // === Auftrag 35g Bug E Fix: Extract displayName and update contact ===
    char *dn = strstr(json_buf, "\"displayName\":\"");
    if (dn) {
        dn += 15;  // Skip past "displayName":"
        char *dn_end = strchr(dn, '"');
        if (dn_end && (dn_end - dn) > 0 && (dn_end - dn) < 31) {
            char peer_name[32];
            int name_len = (int)(dn_end - dn);
            memcpy(peer_name, dn, name_len);
            peer_name[name_len] = '\0';

            // Update contact in PSRAM
            if (s_agent_contact_idx >= 0 && s_agent_contact_idx < MAX_CONTACTS &&
                contacts_db.contacts[s_agent_contact_idx].active) {
                strncpy(contacts_db.contacts[s_agent_contact_idx].name, peer_name, 31);
                contacts_db.contacts[s_agent_contact_idx].name[31] = '\0';

                ESP_LOGI(TAG, "Bug E FIX: Contact [%d] name updated: '%s'",
                         s_agent_contact_idx, peer_name);

                // Persist to NVS
                save_contact_single(s_agent_contact_idx);

                // Update UI chat header
                smp_notify_ui_contact(peer_name);

                // Session 47 2d: Notify UI of peer name for connection progress
                smp_notify_ui_connect_name(s_agent_contact_idx, peer_name);
            }
        }
    }

    free(json_buf);
}

/**
 * Dispatch ratchet body plaintext based on tag byte.
 */
static void dispatch_body_tag(const uint8_t *body, size_t body_len)
{
    if (body_len == 0) return;

    uint8_t tag = body[0];
    switch (tag) {
        case 'D':
            handle_conninfo_reply(body, body_len);
            break;
        case 'I':
            if (body_len > 6) handle_conninfo(body, body_len);
            break;
        default:
            ESP_LOGW(TAG, "Unknown body tag: 0x%02X '%c'",
                     tag, (tag >= 0x20 && tag < 0x7f) ? (char)tag : '?');
            break;
    }
}

// ============================================================================
// PHConfirmation Handler ('K')
// ============================================================================

/**
 * Handle PHConfirmation: extract auth key, parse agent fields,
 * perform ratchet decrypt, dispatch ConnInfo.
 */
static bool handle_confirmation(const uint8_t *client_msg, uint16_t original_len,
                                 uint8_t *peer_sender_auth_key, bool *has_peer_sender_auth)
{
    uint8_t key_len = client_msg[1];
    ESP_LOGD(TAG, "PHConfirmation: auth key len=%u", key_len);

    // Extract sender auth key for KEY command
    if (key_len == 44) {
        memcpy(peer_sender_auth_key, &client_msg[2], 44);
        *has_peer_sender_auth = true;
        ESP_LOGI("KEY_DEBUG", "peer_sender_auth_key: len=%d, first4: %02x %02x %02x %02x",
            key_len, client_msg[2], client_msg[3], client_msg[4], client_msg[5]);
        ESP_LOGI(TAG, "Sender auth key saved for KEY command");
    } else {
        ESP_LOGW(TAG, "Unexpected key_len=%u (expected 44)", key_len);
    }

    // Parse AgentConfirmation fields
    size_t cm_offset = 2 + key_len;

    uint16_t agent_version = (client_msg[cm_offset] << 8) | client_msg[cm_offset + 1];
    cm_offset += 2;

    char agent_tag = client_msg[cm_offset++];
    char maybe_tag = client_msg[cm_offset++];

    ESP_LOGD(TAG, "Agent v%u, Tag='%c', e2eEnc='%c'", agent_version, agent_tag, maybe_tag);

    if (maybe_tag == '0') {
        ESP_LOGD(TAG, "e2eEncryption_ = Nothing (X3DH already exchanged)");
    } else if (maybe_tag == '1') {
        ESP_LOGW(TAG, "e2eEncryption_ = Just - X448 Keys follow!");
    }

    // Ratchet decrypt the EncRatchetMessage
    uint8_t *body = NULL;
    size_t body_len = 0;

    if (ratchet_decrypt_message(client_msg, original_len, cm_offset, &body, &body_len)) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        ESP_LOGI(TAG, "      |  [SUCCESS] ConnInfo DECRYPTED! [SUCCESS]          |");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        ESP_LOGI(TAG, "      Plaintext: %zu bytes", body_len);

        dispatch_body_tag(body, body_len);
        free(body);
    }

    return true;  // Auth key may still be valid even if ratchet failed
}

// ============================================================================
// PHEmpty Handler ('_') - A_HELLO / A_MSG
// ============================================================================

/**
 * Extract chat text from ratchet-decrypted A_MSG body.
 * Parses: AgentMessage 'M' -> JSON -> "text":"..."
 */
static void extract_chat_text(const uint8_t *body, size_t body_len, int contact_idx)
{
    if (body_len < 11 || body[0] != 'M') return;

    uint8_t pmh_len = body[9];
    int inner_off = 10 + pmh_len;

    if (inner_off >= (int)body_len) return;

    uint8_t inner_tag = body[inner_off];

    if (inner_tag == 'M') {
        // A_MSG body -> JSON with chat text
        int json_off = inner_off + 1;
        int json_len = (int)body_len - json_off;

        char *json = malloc(json_len + 1);
        if (!json) return;

        memcpy(json, &body[json_off], json_len);
        json[json_len] = '\0';

        char *tk = strstr(json, "\"text\":\"");
        if (tk) {
            char *text_start = tk + 8;
            char *text_end = strchr(text_start, '"');
            if (text_end) {
                *text_end = '\0';
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "      +----------------------------------------------+");
                ESP_LOGI(TAG, "      |  [SUCCESS] ESP32 RECEIVED MESSAGE! [SUCCESS]       |");
                ESP_LOGI(TAG, "      |                                              |");
                ESP_LOGI(TAG, "      |  [MSG] \"%s\"", text_start);
                ESP_LOGI(TAG, "      |                                              |");
                ESP_LOGI(TAG, "      |  🏆 BIDIRECTIONAL CHAT ON ESP32! 🏆           |");
                ESP_LOGI(TAG, "      +----------------------------------------------+");
                // Session 32: Push to display
                smp_notify_ui_message(text_start, false, 0, contact_idx);

                // Session 37: Save received message to encrypted SD history
                {
                    history_message_t hist_msg = {
                        .direction = HISTORY_DIR_RECEIVED,
                        .delivery_status = 0,
                        .timestamp = time(NULL),
                        .text_len = (uint16_t)strlen(text_start),
                    };
                    if (hist_msg.text_len > HISTORY_MAX_TEXT)
                        hist_msg.text_len = HISTORY_MAX_TEXT;
                    memcpy(hist_msg.text, text_start, hist_msg.text_len);
                    hist_msg.text[hist_msg.text_len] = '\0';
                    smp_history_append((uint8_t)contact_idx, &hist_msg);
                }
            }
        }
        free(json);

    } else if (inner_tag == 'H') {
        ESP_LOGD(TAG, "HELLO inside ratchet (late)");

    } else if (inner_tag == 'V') {
        // Session 32: Delivery Receipt ('V' = RCVD)
        // Format: 'V' + count(1) + [msg_id(8B BE) + hash_len(1) + hash(N)]...
        int voff = inner_off + 1;
        if (voff >= (int)body_len) return;

        uint8_t receipt_count = body[voff++];
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        ESP_LOGI(TAG, "      |  [MSG] DELIVERY RECEIPT: %d message(s)            |", receipt_count);
        ESP_LOGI(TAG, "      +----------------------------------------------+");

        for (uint8_t r = 0; r < receipt_count && voff + 9 < (int)body_len; r++) {
            // Parse msg_id (8 bytes Big Endian)
            uint64_t rcpt_msg_id = 0;
            for (int b = 0; b < 8; b++) {
                rcpt_msg_id = (rcpt_msg_id << 8) | body[voff + b];
            }
            voff += 8;

            // Parse hash
            uint8_t hash_len = (voff < (int)body_len) ? body[voff++] : 0;
            ESP_LOGI(TAG, "      Receipt[%d]: msg_id=%llu, hash_len=%d, hash=%02x%02x%02x%02x...",
                     r, (unsigned long long)rcpt_msg_id, hash_len,
                     (voff < (int)body_len) ? body[voff] : 0,
                     (voff+1 < (int)body_len) ? body[voff+1] : 0,
                     (voff+2 < (int)body_len) ? body[voff+2] : 0,
                     (voff+3 < (int)body_len) ? body[voff+3] : 0);
            voff += hash_len;

            // Session 32: Push "vv" to UI
            smp_notify_receipt_received(rcpt_msg_id);
        }

        // Session 37: Update delivery high-water mark on SD history
        // SimpleX receipts come in order, so mark all up to current count
        {
            uint16_t total = 0;
            if (smp_history_get_counts((uint8_t)contact_idx, &total, NULL) == ESP_OK
                && total > 0) {
                smp_history_update_delivered((uint8_t)contact_idx, total - 1);
            }
        }
    }
}

/**
 * Handle PHEmpty: dispatch A_HELLO and A_MSG (ratchet decrypt).
 */
static bool handle_empty(const uint8_t *client_msg, uint16_t original_len,
                          contact_t *contact)
{
    ESP_LOGD(TAG, "PHEmpty - AgentMsgEnvelope");

    const uint8_t *agent = client_msg + 1;
    int agent_len = original_len - 1;

    if (agent_len < 3) {
        ESP_LOGW(TAG, "Agent message too short (%d)", agent_len);
        return false;
    }

    uint16_t agent_ver = (agent[0] << 8) | agent[1];
    char agent_tag = agent[2];

    if (agent_tag == 'H') {
        // A_HELLO - Connection established!
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        ESP_LOGI(TAG, "      |  [SUCCESS] A_HELLO received! CONNECTED! [SUCCESS] |");
        ESP_LOGI(TAG, "      +----------------------------------------------+");
        return true;

    } else if (agent_tag == 'M') {
        // A_MSG - Ratchet decrypt -> Chat text
        ESP_LOGI(TAG, "A_MSG - ratchet decrypt starting...");

        const uint8_t *erm = agent + 3;
        int erm_len = agent_len - 3;

        if (erm_len < 20 || !ratchet_is_initialized()) {
            ESP_LOGW(TAG, "Cannot ratchet decrypt (len=%d, init=%d)",
                     erm_len, ratchet_is_initialized());
            return false;
        }

        // Use the shared ratchet pipeline
        // Build a virtual "client_msg" starting from agent+3 so offsets work
        // erm_offset=0 because erm IS the EncRatchetMessage start
        uint8_t *body = NULL;
        size_t body_len = 0;

        if (ratchet_decrypt_message(erm, erm_len, 0, &body, &body_len)) {
            ESP_LOGD(TAG, "Ratchet decrypt OK (%zu bytes)", body_len);
            extract_chat_text(body, body_len, s_agent_contact_idx);
            
            // Auftrag 49b: Trigger delivery receipt for chat messages
            // Parse peer's sndMsgId and send receipt for ✓✓
            if (body_len >= 11 && body[0] == 'M') {
                // Parse peer_snd_msg_id from bytes [1-8] (Int64 BE)
                uint64_t peer_snd_msg_id = 0;
                for (int i = 1; i <= 8; i++) {
                    peer_snd_msg_id = (peer_snd_msg_id << 8) | body[i];
                }
                
                // Check inner tag: only send receipt for actual chat messages ('M'), not HELLO ('H')
                uint8_t pmh_len = body[9];
                int inner_off = 10 + pmh_len;
                if (inner_off < (int)body_len && body[inner_off] == 'M') {
                    // Compute SHA256 hash of the decrypted body
                    uint8_t msg_hash[32];
                    mbedtls_sha256(body, body_len, msg_hash, 0);
                    
                    ESP_LOGI(TAG, "[MSG] Receipt data: peer_sndMsgId=%llu, hash=%02x%02x%02x%02x...",
                             (unsigned long long)peer_snd_msg_id,
                             msg_hash[0], msg_hash[1], msg_hash[2], msg_hash[3]);
                    
                    // Send receipt (reconnects if needed)
                    if (contact) {
                        peer_send_receipt(contact, peer_snd_msg_id, msg_hash);
                    } else {
                        ESP_LOGW(TAG, "[WARN]  Cannot send receipt: contact is NULL");
                    }
                }
            }
            
            free(body);
            return true;
        }
        return false;

    } else {
        ESP_LOGD(TAG, "Agent v%u, tag='%c'", agent_ver, agent_tag);
        return true;
    }
}

// ============================================================================
// Public API
// ============================================================================

bool smp_agent_process_message(
    const uint8_t *plaintext, size_t plaintext_len,
    contact_t *contact,
    uint8_t *peer_sender_auth_key,
    bool *has_peer_sender_auth)
{
    if (plaintext_len < 3) {
        ESP_LOGW(TAG, "Plaintext too short (%zu bytes)", plaintext_len);
        return false;
    }

    // 35e: Compute contact index from pointer for UI routing
    if (contact) {
        s_agent_contact_idx = (int)(contact - contacts_db.contacts);
        if (s_agent_contact_idx < 0 || s_agent_contact_idx >= MAX_CONTACTS) {
            s_agent_contact_idx = 0;  // Safety fallback
        }
    }

    // unPad: 2-byte length prefix
    uint16_t original_len = (plaintext[0] << 8) | plaintext[1];
    const uint8_t *client_msg = plaintext + 2;

    ESP_LOGD(TAG, "unPad: originalLength=%u, padding=%zu",
             original_len, plaintext_len - 2 - original_len);

    uint8_t priv_tag = client_msg[0];
    ESP_LOGD(TAG, "PrivHeader tag: 0x%02x '%c'", priv_tag, priv_tag);

    switch (priv_tag) {
        case AGENT_TAG_CONFIRMATION:
            return handle_confirmation(client_msg, original_len,
                                        peer_sender_auth_key, has_peer_sender_auth);

        case AGENT_TAG_EMPTY:
            return handle_empty(client_msg, original_len, contact);

        default:
            ESP_LOGE(TAG, "Unknown PrivHeader: 0x%02x '%c'",
                     priv_tag, (priv_tag >= 0x20 && priv_tag < 0x7f) ? priv_tag : '?');
            return false;
    }
}