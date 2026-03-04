/**
 * SimpleGo - Reply Queue E2E Decryption
 *
 * Consolidates the Reply Queue decrypt pipeline.
 * Now supports both global keys (legacy) and explicit per-contact keys.
 *
 * The _ex() function is the real implementation.
 * The original function is a thin wrapper passing global keys.
 */

#include "smp_e2e.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "sodium.h"
#include "smp_queue.h"        // our_queue, queue_save_credentials
#include "smp_types.h"        // reply_queue_e2e_peer_public/valid
#include "simplex_crypto.h"   // simplex_secretbox_open_debug

// TODO: move to simplex_crypto.h
extern int decrypt_client_msg(const uint8_t *data, int data_len,
                               const uint8_t *sender_pub, const uint8_t *rcv_priv,
                               uint8_t *out);

static const char *TAG = "SMP_E2E";

// X25519 SPKI header (12 bytes before 32-byte raw key)
static const uint8_t X25519_SPKI_HDR[] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

// ============== Internal: Envelope Parsing ==============

/**
 * Parse ClientMsgEnvelope and extract sender's E2E public key.
 * Handles all corrId/e2e combinations and SPKI fallback search.
 *
 * PARAMETERIZED version: writes extracted key to out_sender_pub and
 * optionally to cached_peer_public (if non-NULL).
 * Does NOT touch global reply_queue_e2e_peer_public.
 *
 * @param envelope           Envelope data (after length prefix)
 * @param envelope_len       Envelope length
 * @param cached_peer_public IN: current cached peer key (used when no inline key)
 * @param cached_peer_valid  IN: true if cached_peer_public is valid
 * @param out_sender_pub     OUT: 32-byte sender public key for decrypt
 * @param out_new_peer       OUT: 32-byte newly extracted peer key (if found in envelope)
 * @param out_new_peer_found OUT: true if a NEW peer key was extracted from envelope
 * @param offset             IN/OUT: parse offset (starts at 12, advances past key)
 * @return true if a sender key was determined (either extracted or from cache)
 */
static bool extract_sender_key_ex(const uint8_t *envelope, size_t envelope_len,
                                   const uint8_t *cached_peer_public,
                                   bool cached_peer_valid,
                                   uint8_t *out_sender_pub,
                                   uint8_t *out_new_peer,
                                   bool *out_new_peer_found,
                                   int *offset)
{
    int off = *offset;
    *out_new_peer_found = false;

    uint8_t maybe_corrId = envelope[off++];
    uint8_t maybe_e2e = envelope[off++];

    ESP_LOGD(TAG, "maybe_corrId=0x%02x, maybe_e2e=0x%02x", maybe_corrId, maybe_e2e);

    if (maybe_corrId == '1' && (maybe_e2e == ',' || maybe_e2e == '0')) {
        // corrId SPKI doubles as E2E key
        if (memcmp(&envelope[off], X25519_SPKI_HDR, 12) == 0) {
            memcpy(out_sender_pub, &envelope[off + 12], 32);
            if (out_new_peer) {
                memcpy(out_new_peer, &envelope[off + 12], 32);
            }
            *out_new_peer_found = true;
            ESP_LOGW(TAG, "Extracted peer E2E key from corrId SPKI");
            ESP_LOGW(TAG, "  Key: %02x%02x%02x%02x %02x%02x%02x%02x",
                     out_sender_pub[0], out_sender_pub[1], out_sender_pub[2], out_sender_pub[3],
                     out_sender_pub[4], out_sender_pub[5], out_sender_pub[6], out_sender_pub[7]);
            off += 44;
            *offset = off;
            return true;
        }
        ESP_LOGE(TAG, "SPKI header mismatch at offset %d", off);
        off += 44;

    } else if (maybe_corrId == '1' && maybe_e2e == '1') {
        // Separate E2E key after corrId SPKI
        off += 44;  // skip corrId SPKI
        uint8_t e2e_len = envelope[off++];
        if (e2e_len == 44 && memcmp(&envelope[off], X25519_SPKI_HDR, 12) == 0) {
            memcpy(out_sender_pub, &envelope[off + 12], 32);
            if (out_new_peer) {
                memcpy(out_new_peer, &envelope[off + 12], 32);
            }
            *out_new_peer_found = true;
            ESP_LOGW(TAG, "Extracted peer E2E key from inline SPKI");
            ESP_LOGW(TAG, "  Key: %02x%02x%02x%02x %02x%02x%02x%02x",
                     out_sender_pub[0], out_sender_pub[1], out_sender_pub[2], out_sender_pub[3],
                     out_sender_pub[4], out_sender_pub[5], out_sender_pub[6], out_sender_pub[7]);
            off += 44;
            *offset = off;
            return true;
        }
        off += 44;

    } else if (maybe_corrId == ',' || maybe_corrId == '0') {
        // No inline key: use cached peer key, nonce right after corrId byte
        // Format: [12B header][corrId byte][24B nonce][ciphertext+MAC]
        // maybe_e2e was actually nonce[0], so back up 1 byte
        if (cached_peer_valid && cached_peer_public) {
            memcpy(out_sender_pub, cached_peer_public, 32);
            *offset = off - 1;  // Nonce starts at byte 13, not 14
            ESP_LOGD(TAG, "E2E: corrId='%c', nonce_offset=%d, using cached key", maybe_corrId, off - 1);
            return true;
        }
        ESP_LOGE(TAG, "No pre-shared E2E key available");

    } else {
        // Unknown format: search for SPKI at nearby offsets
        ESP_LOGW(TAG, "Unknown corrId/e2e format, searching for SPKI...");
        for (int try_off = 10; try_off <= 20; try_off++) {
            if ((size_t)(try_off + 44) <= envelope_len &&
                memcmp(&envelope[try_off], X25519_SPKI_HDR, 12) == 0) {
                memcpy(out_sender_pub, &envelope[try_off + 12], 32);
                if (out_new_peer) {
                    memcpy(out_new_peer, &envelope[try_off + 12], 32);
                }
                *out_new_peer_found = true;
                ESP_LOGW(TAG, "Extracted peer E2E key from fallback SPKI search");
                *offset = try_off + 44;
                return true;
            }
        }
    }

    *offset = off;
    return false;
}

// ============== Internal: Multi-Method E2E Decrypt ==============

/**
 * Try multiple E2E decrypt methods against the ciphertext.
 * Methods: decrypt_client_msg, crypto_box, crypto_secretbox, simplex_secretbox
 *
 * PARAMETERIZED: takes explicit e2e_private instead of our_queue.e2e_private.
 */
static int try_e2e_decrypt_ex(const uint8_t *envelope, size_t envelope_len,
                               int nonce_offset, int cipher_offset,
                               const uint8_t *sender_pub,
                               const uint8_t *e2e_private,
                               uint8_t *e2e_plain, size_t cipher_len)
{
    const uint8_t *cm_nonce = &envelope[nonce_offset];
    const uint8_t *ciphertext = &envelope[cipher_offset];

    // DH secret for methods 2-3
    uint8_t dh_secret[32];
    if (crypto_scalarmult(dh_secret, e2e_private, sender_pub) != 0) {
        ESP_LOGE(TAG, "DH computation failed");
        return -1;
    }

    int ret = -1;

    // Method 0: decrypt_client_msg (Contact Queue style)
    ESP_LOGD(TAG, "Trying decrypt_client_msg...");
    {
        int block_len = envelope_len - nonce_offset;
        uint8_t *m0 = malloc(block_len);
        if (m0) {
            int dec = decrypt_client_msg(&envelope[nonce_offset], block_len,
                                          sender_pub, e2e_private, m0);
            if (dec > 0) {
                memcpy(e2e_plain, m0, dec < (int)cipher_len ? dec : (int)cipher_len);
                ret = 0;
            }
            free(m0);
        }
    }

    // Method 1: crypto_box_open_easy
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying crypto_box_open_easy...");
        ret = crypto_box_open_easy(e2e_plain, ciphertext, cipher_len,
                                    cm_nonce, sender_pub, e2e_private);
    }

    // Method 2: crypto_secretbox_open_easy with DH secret
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying crypto_secretbox_open_easy...");
        ret = crypto_secretbox_open_easy(e2e_plain, ciphertext, cipher_len,
                                          cm_nonce, dh_secret);
    }

    // Method 3: Custom simplex_secretbox
    if (ret != 0) {
        ESP_LOGD(TAG, "Trying simplex_secretbox_open_debug...");
        ret = simplex_secretbox_open_debug(e2e_plain, ciphertext, cipher_len,
                                            cm_nonce, dh_secret, "REPLY_E2E");
    }

    sodium_memzero(dh_secret, 32);
    return ret;
}

// ============== Public API: Parameterized (_ex) ==============

int smp_e2e_decrypt_reply_message_ex(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    const uint8_t *shared_secret,
    const uint8_t *e2e_private,
    const uint8_t *e2e_peer_public,
    bool e2e_peer_valid,
    uint8_t *out_peer_public,
    bool *out_peer_found,
    uint8_t **out_plain, size_t *out_plain_len)
{
    *out_plain = NULL;
    *out_plain_len = 0;
    if (out_peer_found) *out_peer_found = false;

    // === Layer 1: Server-level decrypt ===
    uint8_t server_nonce[24];
    memset(server_nonce, 0, 24);
    memcpy(server_nonce, msg_id, msg_id_len > 24 ? 24 : msg_id_len);

    uint8_t *server_plain = malloc(encrypted_len);
    if (!server_plain) return -1;

    if (crypto_box_open_easy_afternm(server_plain, encrypted, encrypted_len,
                                      server_nonce, shared_secret) != 0) {
        ESP_LOGE(TAG, "Server-level decrypt FAILED");
        free(server_plain);
        return -2;
    }
    int plain_len = encrypted_len - crypto_box_MACBYTES;
    ESP_LOGI(TAG, "Server decrypt OK (%d bytes)", plain_len);

    // Debug diagnostics
    ESP_LOGD(TAG, "enc_len=%d, msg_id_len=%d, plain_len=%d", encrypted_len, msg_id_len, plain_len);
    ESP_LOGD(TAG, "shared_secret: %02x%02x%02x%02x %02x%02x%02x%02x",
             shared_secret[0], shared_secret[1], shared_secret[2], shared_secret[3],
             shared_secret[4], shared_secret[5], shared_secret[6], shared_secret[7]);

    // === Length prefix handling ===
    uint16_t raw_len_prefix = (server_plain[0] << 8) | server_plain[1];
    int prefix_len = 0;
    if (plain_len > 2 && (server_plain[0] != 0x00 || server_plain[1] != 0x00)) {
        prefix_len = 2;
    }

    const uint8_t *envelope = server_plain + prefix_len;
    size_t envelope_len = raw_len_prefix;

    // Debug: envelope header
    ESP_LOGD(TAG, "len_prefix=%u, prefix_len=%d, envelope_len=%zu", raw_len_prefix, prefix_len, envelope_len);
    ESP_LOGD(TAG, "envelope first 32 bytes:");
    {
        char hex[128] = {0}; int hx = 0;
        for (int i = 0; i < 32 && i < (int)envelope_len; i++)
            hx += sprintf(&hex[hx], "%02x ", envelope[i]);
        ESP_LOGD(TAG, "  %s", hex);
    }

    // === Layer 2: E2E envelope parse + decrypt ===
    int offset = 12;
    uint8_t sender_pub[32];
    uint8_t new_peer[32];
    bool new_peer_found = false;

    if (!extract_sender_key_ex(envelope, envelope_len,
                                e2e_peer_public, e2e_peer_valid,
                                sender_pub, new_peer, &new_peer_found,
                                &offset)) {
        ESP_LOGE(TAG, "No sender key found");
        free(server_plain);
        return -3;
    }

    // Report extracted peer key to caller
    if (new_peer_found) {
        if (out_peer_public) {
            memcpy(out_peer_public, new_peer, 32);
        }
        if (out_peer_found) {
            *out_peer_found = true;
        }
    }

    // Debug: key summary
    ESP_LOGD(TAG, "E2E keys: sender=%02x%02x%02x%02x, our=%02x%02x%02x%02x",
             sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
             e2e_private[0], e2e_private[1], e2e_private[2], e2e_private[3]);

    // Check bounds for nonce + MAC
    if (envelope_len < (size_t)offset + 24 + 16) {
        ESP_LOGE(TAG, "Message too short for E2E decrypt");
        free(server_plain);
        return -4;
    }

    ESP_LOGD(TAG, "cmNonce at offset %d, cipher at %d, cipher_len=%zu",
             offset, offset + 24, envelope_len - offset - 24);

    int nonce_offset = offset;
    int cipher_offset = offset + 24;
    size_t cipher_len = envelope_len - cipher_offset;

    uint8_t *e2e_plain = malloc(cipher_len);
    if (!e2e_plain) {
        free(server_plain);
        return -1;
    }

    int ret = try_e2e_decrypt_ex(envelope, envelope_len, nonce_offset, cipher_offset,
                                  sender_pub, e2e_private, e2e_plain, cipher_len);

    if (ret != 0) {
        ESP_LOGE(TAG, "E2E decrypt FAILED (all methods)");
        free(e2e_plain);
        free(server_plain);
        return -5;
    }

    size_t result_len = cipher_len - 16;  // Remove Poly1305 MAC
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      |  E2E LAYER 2 DECRYPT SUCCESS!                |");
    ESP_LOGI(TAG, "      +----------------------------------------------+");
    ESP_LOGI(TAG, "      Decrypted %zu bytes! (nonce_offset=%d)", result_len, nonce_offset);

    *out_plain = e2e_plain;
    *out_plain_len = result_len;

    free(server_plain);
    return 0;
}

// ============== Public API: Legacy Wrapper ==============

int smp_e2e_decrypt_reply_message(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    uint8_t **out_plain, size_t *out_plain_len)
{
    // Use global keys (backward compatible)
    uint8_t new_peer[32];
    bool new_peer_found = false;

    int ret = smp_e2e_decrypt_reply_message_ex(
        encrypted, encrypted_len,
        msg_id, msg_id_len,
        our_queue.shared_secret,
        our_queue.e2e_private,
        reply_queue_e2e_peer_public,
        reply_queue_e2e_peer_valid,
        new_peer, &new_peer_found,
        out_plain, out_plain_len);

    // Legacy behavior: update globals when peer key extracted
    if (new_peer_found) {
        memcpy(reply_queue_e2e_peer_public, new_peer, 32);
        reply_queue_e2e_peer_valid = true;
        ESP_LOGW(TAG, "47f: Updated global reply_queue_e2e_peer_public");
        // Persist (legacy path uses queue_save_credentials)
        queue_save_credentials();
    }

    return ret;
}
