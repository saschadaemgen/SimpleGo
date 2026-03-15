/**
 * SimpleGo - smp_ratchet.c
 * Double Ratchet encryption implementation
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_ratchet.h"
#include <inttypes.h>
#include "smp_x448.h"
#include "smp_crypto.h"
#include "smp_storage.h"
#include "pq_crypto_task.h"  // Session 46 Teil E: PQ crypto operations
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/sha512.h"

static const char *TAG = "SMP_RATCH";

// Constants
#define RATCHET_VERSION         3
#define MSG_HEADER_CONTENT_LEN  80
#define MSG_HEADER_PADDED_LEN   88
#define GCM_IV_LEN              16
#define GCM_TAG_LEN             16
#define AAD_FULL_LEN            200

// ============== Ratchet State ==============

static ratchet_state_t ratchet_state = {0};

// Session 33: Multi-contact PSRAM backing store
static ratchet_state_t *ratchet_states = NULL;  // PSRAM array[MAX_RATCHETS]
static uint8_t active_ratchet_idx = 0;

// Saved X3DH keys (before ratchet_init_sender modifies them)
static uint8_t saved_x3dh_hk[32] = {0};
static uint8_t saved_x3dh_nhk[32] = {0};
static bool saved_x3dh_valid = false;

// Session 46 Teil E: Received PQ params (set by header parser before decrypt_body)
static parsed_msg_header_t s_recv_pq_hdr;
static bool s_recv_pq_valid = false;

// ============== Session 33: Multi-Contact Functions ==============

bool ratchet_multi_init(void) {
    if (ratchet_states) return true;  // Already initialized

    size_t total = sizeof(ratchet_state_t) * MAX_RATCHETS;
    ratchet_states = heap_caps_calloc(MAX_RATCHETS, sizeof(ratchet_state_t), MALLOC_CAP_SPIRAM);
    if (!ratchet_states) {
        ESP_LOGE(TAG, "Failed to allocate ratchet PSRAM array (%zu bytes)", total);
        return false;
    }
    // Copy current working state to slot 0
    memcpy(&ratchet_states[0], &ratchet_state, sizeof(ratchet_state_t));
    active_ratchet_idx = 0;
    ESP_LOGI(TAG, "Ratchet PSRAM array: %d slots, %zu bytes (per slot: %zu, PQ: %zu)",
             MAX_RATCHETS, total, sizeof(ratchet_state_t), sizeof(pq_kem_state_t));
    return true;
}

bool ratchet_set_active(uint8_t idx) {
    if (idx >= MAX_RATCHETS) return false;
    if (!ratchet_states) return false;

    // Save current working state back to PSRAM array
    memcpy(&ratchet_states[active_ratchet_idx], &ratchet_state, sizeof(ratchet_state_t));
    // Load requested slot into working state
    memcpy(&ratchet_state, &ratchet_states[idx], sizeof(ratchet_state_t));
    active_ratchet_idx = idx;

    // 35h: If PSRAM slot was empty, try NVS as fallback
    if (!ratchet_state.initialized) {
        ESP_LOGW(TAG, "Slot [%d] PSRAM empty, trying NVS fallback...", idx);
        if (ratchet_load_state(idx)) {
            // Also update PSRAM array so next swap works without NVS
            memcpy(&ratchet_states[idx], &ratchet_state, sizeof(ratchet_state_t));
            ESP_LOGI(TAG, "Slot [%d] restored from NVS! send=%" PRIu32 " recv=%" PRIu32 "",
                     idx, ratchet_state.msg_num_send, ratchet_state.msg_num_recv);
        } else {
            ESP_LOGD(TAG, "Slot [%d] not in NVS either (fresh contact)", idx);
        }
    }

    return true;
}

uint8_t ratchet_get_active(void) {
    return active_ratchet_idx;
}

// Bug #26: Securely wipe a ratchet slot on contact deletion
void ratchet_wipe_slot(uint8_t idx) {
    if (idx >= MAX_RATCHETS || !ratchet_states) return;

    // If wiping the active slot, zero the working copy first
    if (idx == active_ratchet_idx) {
        sodium_memzero(&ratchet_state, sizeof(ratchet_state_t));
        active_ratchet_idx = 0xFF;  // No valid active slot
        ESP_LOGW(TAG, "Wiped active ratchet slot [%d] (working copy + PSRAM)", idx);
    } else {
        ESP_LOGI(TAG, "Wiped ratchet slot [%d] (PSRAM)", idx);
    }

    // Wipe the PSRAM array slot
    sodium_memzero(&ratchet_states[idx], sizeof(ratchet_state_t));
}

// ============== Helper Functions ==============

static int hkdf_sha512(const uint8_t *salt, size_t salt_len,
                       const uint8_t *ikm, size_t ikm_len,
                       const uint8_t *info, size_t info_len,
                       uint8_t *okm, size_t okm_len) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
                        salt, salt_len, ikm, ikm_len, info, info_len,
                        okm, okm_len);
}

static int aes_gcm_encrypt(const uint8_t *key,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plaintext, size_t pt_len,
                           uint8_t *ciphertext,
                           uint8_t *tag) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) goto cleanup;
    
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                    pt_len, iv, iv_len,
                                    aad, aad_len,
                                    plaintext, ciphertext,
                                    GCM_TAG_LEN, tag);
    
cleanup:
    mbedtls_gcm_free(&gcm);
    return ret;
}

static int aes_gcm_decrypt(const uint8_t *key,
                           const uint8_t *iv, size_t iv_len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *ciphertext, size_t ct_len,
                           const uint8_t *tag,
                           uint8_t *plaintext) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) goto cleanup;
    
    ret = mbedtls_gcm_auth_decrypt(&gcm, ct_len,
                                   iv, iv_len,
                                   aad, aad_len,
                                   tag, GCM_TAG_LEN,
                                   ciphertext, plaintext);
    
cleanup:
    mbedtls_gcm_free(&gcm);
    return ret;
}

// ============== Key Derivation ==============

/**
 * Root KDF with optional PQ KEM shared secret.
 *
 * IKM = dh_out (56 bytes) || kem_secret (32 bytes, optional)
 * Salt = root_key (32 bytes)
 * Info = "SimpleXRootRatchet" (18 bytes)
 * Output = 96 bytes: new_root_key(32) | chain_key(32) | next_header_key(32)
 *
 * When kem_secret is NULL, IKM is just the DH output (unchanged behavior).
 */
static void kdf_root(const uint8_t *root_key, const uint8_t *dh_out,
                     const uint8_t *kem_secret, size_t kem_secret_len,
                     uint8_t *new_root_key, uint8_t *chain_key, uint8_t *next_header_key) {
    uint8_t ikm[88];  /* 56 (DH) + 32 (KEM) max */
    size_t ikm_len = 56;

    memcpy(ikm, dh_out, 56);
    if (kem_secret != NULL && kem_secret_len > 0) {
        if (kem_secret_len > 32) kem_secret_len = 32;
        memcpy(ikm + 56, kem_secret, kem_secret_len);
        ikm_len += kem_secret_len;
    }

    uint8_t kdf_output[96];
    hkdf_sha512(root_key, 32, ikm, ikm_len,
                (const uint8_t *)"SimpleXRootRatchet", 18,
                kdf_output, 96);
    memcpy(new_root_key, kdf_output, 32);
    memcpy(chain_key, kdf_output + 32, 32);
    memcpy(next_header_key, kdf_output + 64, 32);

    /* Wipe intermediate key material */
    sodium_memzero(ikm, sizeof(ikm));
    sodium_memzero(kdf_output, sizeof(kdf_output));
}

static void kdf_chain(const uint8_t *chain_key,
                      uint8_t *next_chain_key, uint8_t *message_key,
                      uint8_t *msg_iv, uint8_t *header_iv) {
    uint8_t kdf_output[96];
    hkdf_sha512(NULL, 0, chain_key, 32,
                (const uint8_t *)"SimpleXChainRatchet", 19,
                kdf_output, 96);
    memcpy(next_chain_key, kdf_output, 32);
    memcpy(message_key, kdf_output + 32, 32);
    memcpy(msg_iv, kdf_output + 64, 16);
    memcpy(header_iv, kdf_output + 80, 16);
}

// ============== X3DH Key Agreement ==============

bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "[LOCK] X3DH Key Agreement (sender)...");
    ESP_LOGI(TAG, "   Note: We are RESPONDER, peer is INITIATOR");

    uint8_t dh1[56], dh2[56], dh3[56];
    if (!x448_dh(peer_key1, our_key2->private_key, dh1)) return false;
    if (!x448_dh(peer_key2, our_key1->private_key, dh2)) return false;
    if (!x448_dh(peer_key2, our_key2->private_key, dh3)) return false;

    uint8_t dh_combined[168];
    memcpy(dh_combined, dh1, 56);
    memcpy(dh_combined + 56, dh2, 56);
    memcpy(dh_combined + 112, dh3, 56);

    uint8_t salt[64] = {0};
    uint8_t kdf_output[96];
    hkdf_sha512(salt, 64, dh_combined, 168,
                (const uint8_t *)"SimpleXX3DH", 11, kdf_output, 96);

    // ================================================================
    // FIX 1: X3DH Output Assignment per Signal spec RatchetInitAliceHE()
    // bytes 0-31:  hk -> HKs (header_key_send) - active send header key
    // bytes 32-63: nhk -> NHKr (next_header_key_recv) - NOT active HKr!
    // bytes 64-95: root_key
    // 
    // Signal: "state.HKr = None; state.NHKr = shared_nhkb"
    // header_key_recv stays 0x00 until first AdvanceRatchet
    // ================================================================
    memcpy(ratchet_state.header_key_send, kdf_output, 32);           // HKs = hk
    memcpy(ratchet_state.next_header_key_recv, kdf_output + 32, 32); // NHKr = nhk (FIX 1!)
    // header_key_recv stays 0x00 - no active HKr before first AdvanceRatchet
    memcpy(ratchet_state.root_key, kdf_output + 64, 32);

    ESP_LOGI(TAG, "X3DH: HKs=%02x%02x.. NHKr=%02x%02x.. RK=%02x%02x..",
             ratchet_state.header_key_send[0], ratchet_state.header_key_send[1],
             ratchet_state.next_header_key_recv[0], ratchet_state.next_header_key_recv[1],
             ratchet_state.root_key[0], ratchet_state.root_key[1]);

    memcpy(ratchet_state.assoc_data, our_key1->public_key, 56);
    memcpy(ratchet_state.assoc_data + 56, peer_key1, 56);

    ESP_LOGI(TAG, "[OK] X3DH complete - RootKey: %02x%02x...", ratchet_state.root_key[0], ratchet_state.root_key[1]);
    
    memcpy(saved_x3dh_hk, ratchet_state.header_key_send, 32);
    memcpy(saved_x3dh_nhk, ratchet_state.next_header_key_recv, 32);
    saved_x3dh_valid = true;
    ESP_LOGI(TAG, "[PIN] Saved X3DH keys: hk=%02x%02x..., nhk=%02x%02x...",
             saved_x3dh_hk[0], saved_x3dh_hk[1], saved_x3dh_nhk[0], saved_x3dh_nhk[1]);
    
    return true;
}

// ============== Ratchet Initialization ==============

bool ratchet_init_sender(const uint8_t *peer_dh_public, const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "[SYNC] Initializing initial send ratchet...");

    memcpy(&ratchet_state.dh_self, our_key2, sizeof(x448_keypair_t));
    memcpy(ratchet_state.dh_peer, peer_dh_public, 56);

    uint8_t dh_out[56];
    if (!x448_dh(peer_dh_public, ratchet_state.dh_self.private_key, dh_out)) {
        return false;
    }

    uint8_t new_root_key[32];
    uint8_t next_header_key[32];
    kdf_root(ratchet_state.root_key, dh_out, NULL, 0,
             new_root_key, ratchet_state.chain_key_send, next_header_key);
    memcpy(ratchet_state.root_key, new_root_key, 32);
    
    // ================================================================
    // FIX 2: Save NHKs per Signal spec RatchetInitAliceHE()
    // "state.RK, state.CKs, state.NHKs = KDF_RK_HE(...)"
    // NHKs will be promoted to HKs on first AdvanceRatchet
    // ================================================================
    memcpy(ratchet_state.next_header_key_send, next_header_key, 32);  // FIX 2!

    ESP_LOGI(TAG, "Ratchet init: RK=%02x%02x.. CKs=%02x%02x.. NHKs=%02x%02x..",
             ratchet_state.root_key[0], ratchet_state.root_key[1],
             ratchet_state.chain_key_send[0], ratchet_state.chain_key_send[1],
             ratchet_state.next_header_key_send[0], ratchet_state.next_header_key_send[1]);

    ratchet_state.msg_num_send = 0;
    ratchet_state.prev_chain_len = 0;
    ratchet_state.initialized = true;

    // Auftrag 50b R2: Persist initial ratchet state after X3DH
    // Session 34: Use active slot, not hardcoded 0 (multi-contact fix)
    ratchet_save_state(ratchet_get_active());

    ESP_LOGI(TAG, "[OK] Ratchet initialized");
    return true;
}

static void build_msg_header(uint8_t *header, const uint8_t *dh_public,
                             uint32_t pn, uint32_t ns) {
    memset(header, 0, MSG_HEADER_PADDED_LEN);
    int p = 0;

    header[p++] = 0x00;
    header[p++] = 80;

    header[p++] = 0x00;
    header[p++] = RATCHET_VERSION;

    header[p++] = 68;

    static const uint8_t X448_SPKI_HEADER[12] = {0x30,0x42,0x30,0x05,0x06,0x03,0x2b,0x65,0x6f,0x03,0x39,0x00};
    memcpy(&header[p], X448_SPKI_HEADER, 12); p += 12;
    memcpy(&header[p], dh_public, 56); p += 56;

    header[p++] = 0x30;

    header[p++] = (pn >> 24) & 0xFF;
    header[p++] = (pn >> 16) & 0xFF;
    header[p++] = (pn >> 8)  & 0xFF;
    header[p++] = pn & 0xFF;

    header[p++] = (ns >> 24) & 0xFF;
    header[p++] = (ns >> 16) & 0xFF;
    header[p++] = (ns >> 8)  & 0xFF;
    header[p++] = ns & 0xFF;

    memset(&header[p], '#', 88 - p);
}

// ============== PQ Header Serialization (Session 46 Teil C) ==============

static const uint8_t X448_SPKI_HDR[12] = {
    0x30,0x42,0x30,0x05,0x06,0x03,0x2b,0x65,0x6f,0x03,0x39,0x00
};

int pq_header_serialize(uint8_t *buf, size_t buf_size,
                        const uint8_t *dh_public,
                        const pq_kem_state_t *pq,
                        uint32_t pn, uint32_t ns) {

    bool is_pq = (pq != NULL && pq->pq_active && smp_settings_get_pq_enabled());
    size_t padded_len = is_pq ? MSG_HEADER_PQ_PADDED_LEN : MSG_HEADER_PADDED_LEN;

    if (buf_size < padded_len) {
        ESP_LOGE(TAG, "pq_header_serialize: buf too small (%zu < %zu)", buf_size, padded_len);
        return -1;
    }

    memset(buf, 0, padded_len);
    int p = 2;  /* skip content_len, fill at end */

    /* msgMaxVersion: Word16 BE */
    buf[p++] = 0x00;
    buf[p++] = RATCHET_VERSION;

    /* msgDHRs: 1 byte length + X448 SPKI DER (12 header + 56 key = 68) */
    buf[p++] = 68;
    memcpy(&buf[p], X448_SPKI_HDR, 12); p += 12;
    memcpy(&buf[p], dh_public, 56); p += 56;

    /* msgKEM */
    if (!is_pq || pq->pq_kem_state == 0) {
        /* Nothing: '0' */
        buf[p++] = PQ_KEM_NOTHING;
    } else if (pq->pq_kem_state == 1) {
        /* Proposed: Just('1') + 'P' + pk_len(Word16) + pk(1158) */
        buf[p++] = PQ_KEM_JUST;
        buf[p++] = PQ_KEM_PROPOSED;
        buf[p++] = (SNTRUP761_PUBLICKEYBYTES >> 8) & 0xFF;
        buf[p++] = SNTRUP761_PUBLICKEYBYTES & 0xFF;
        memcpy(&buf[p], pq->own_kem_pk, SNTRUP761_PUBLICKEYBYTES);
        p += SNTRUP761_PUBLICKEYBYTES;
    } else if (pq->pq_kem_state == 2) {
        /* Accepted: Just('1') + 'A' + ct_len(Word16) + ct(1039) + pk_len(Word16) + pk(1158) */
        buf[p++] = PQ_KEM_JUST;
        buf[p++] = PQ_KEM_ACCEPTED;
        buf[p++] = (SNTRUP761_CIPHERTEXTBYTES >> 8) & 0xFF;
        buf[p++] = SNTRUP761_CIPHERTEXTBYTES & 0xFF;
        memcpy(&buf[p], pq->pending_ct, SNTRUP761_CIPHERTEXTBYTES);
        p += SNTRUP761_CIPHERTEXTBYTES;
        buf[p++] = (SNTRUP761_PUBLICKEYBYTES >> 8) & 0xFF;
        buf[p++] = SNTRUP761_PUBLICKEYBYTES & 0xFF;
        memcpy(&buf[p], pq->own_kem_pk, SNTRUP761_PUBLICKEYBYTES);
        p += SNTRUP761_PUBLICKEYBYTES;
    }

    /* msgPN: Word32 BE */
    buf[p++] = (pn >> 24) & 0xFF;
    buf[p++] = (pn >> 16) & 0xFF;
    buf[p++] = (pn >> 8) & 0xFF;
    buf[p++] = pn & 0xFF;

    /* msgNs: Word32 BE */
    buf[p++] = (ns >> 24) & 0xFF;
    buf[p++] = (ns >> 16) & 0xFF;
    buf[p++] = (ns >> 8) & 0xFF;
    buf[p++] = ns & 0xFF;

    /* content_len (bytes 0-1): actual content bytes excluding the 2-byte length field */
    uint16_t content_len = (uint16_t)(p - 2);
    buf[0] = (content_len >> 8) & 0xFF;
    buf[1] = content_len & 0xFF;

    /* Pad with '#' to target size */
    memset(&buf[p], '#', padded_len - p);

    ESP_LOGD(TAG, "pq_header_serialize: content=%u, padded=%zu, pq=%d, kem_state=%d",
             content_len, padded_len, is_pq ? 1 : 0,
             (pq && is_pq) ? pq->pq_kem_state : -1);

    return (int)padded_len;
}

int pq_header_deserialize(const uint8_t *buf, size_t buf_len,
                          parsed_msg_header_t *out) {

    memset(out, 0, sizeof(parsed_msg_header_t));

    if (buf_len < MSG_HEADER_PADDED_LEN) {
        ESP_LOGE(TAG, "pq_header_deserialize: buf too small (%zu)", buf_len);
        return -1;
    }

    int p = 0;
    uint16_t content_len = (buf[p] << 8) | buf[p + 1]; p += 2;

    /* Bounds check: content must fit in buffer */
    if ((size_t)(content_len + 2) > buf_len) {
        ESP_LOGE(TAG, "pq_header_deserialize: content_len %u exceeds buf %zu", content_len, buf_len);
        return -2;
    }

    /* msgMaxVersion: Word16 BE */
    out->version = (buf[p] << 8) | buf[p + 1]; p += 2;

    /* msgDHRs: 1 byte length + SPKI DER */
    uint8_t key_len = buf[p++];
    if (key_len != 68) {
        ESP_LOGE(TAG, "pq_header_deserialize: unexpected key_len %u (expected 68)", key_len);
        return -3;
    }
    /* Skip 12-byte SPKI header, extract 56-byte raw X448 key */
    p += 12;
    memcpy(out->dh_public, &buf[p], 56); p += 56;

    /* msgKEM (only if version >= 3) */
    out->kem_tag = PQ_KEM_NOTHING;
    if (out->version >= 3) {
        uint8_t maybe_tag = buf[p++];
        if (maybe_tag == PQ_KEM_NOTHING) {
            out->kem_tag = PQ_KEM_NOTHING;
        } else if (maybe_tag == PQ_KEM_JUST) {
            uint8_t variant = buf[p++];
            if (variant == PQ_KEM_PROPOSED) {
                out->kem_tag = PQ_KEM_PROPOSED;
                uint16_t pk_len = (buf[p] << 8) | buf[p + 1]; p += 2;
                if (pk_len != SNTRUP761_PUBLICKEYBYTES) {
                    ESP_LOGE(TAG, "pq_header_deserialize: bad PK len %u", pk_len);
                    return -4;
                }
                memcpy(out->kem_pk, &buf[p], SNTRUP761_PUBLICKEYBYTES);
                p += SNTRUP761_PUBLICKEYBYTES;
                out->kem_pk_valid = true;
            } else if (variant == PQ_KEM_ACCEPTED) {
                out->kem_tag = PQ_KEM_ACCEPTED;
                uint16_t ct_len = (buf[p] << 8) | buf[p + 1]; p += 2;
                if (ct_len != SNTRUP761_CIPHERTEXTBYTES) {
                    ESP_LOGE(TAG, "pq_header_deserialize: bad CT len %u", ct_len);
                    return -5;
                }
                memcpy(out->kem_ct, &buf[p], SNTRUP761_CIPHERTEXTBYTES);
                p += SNTRUP761_CIPHERTEXTBYTES;
                out->kem_ct_valid = true;
                uint16_t pk_len = (buf[p] << 8) | buf[p + 1]; p += 2;
                if (pk_len != SNTRUP761_PUBLICKEYBYTES) {
                    ESP_LOGE(TAG, "pq_header_deserialize: bad PK len %u in Accepted", pk_len);
                    return -6;
                }
                memcpy(out->kem_pk, &buf[p], SNTRUP761_PUBLICKEYBYTES);
                p += SNTRUP761_PUBLICKEYBYTES;
                out->kem_pk_valid = true;
            } else {
                ESP_LOGE(TAG, "pq_header_deserialize: unknown KEM variant 0x%02x", variant);
                return -7;
            }
        } else {
            ESP_LOGE(TAG, "pq_header_deserialize: unknown maybe tag 0x%02x", maybe_tag);
            return -8;
        }
    }

    /* msgPN: Word32 BE */
    out->pn = ((uint32_t)buf[p] << 24) | ((uint32_t)buf[p + 1] << 16) |
              ((uint32_t)buf[p + 2] << 8) | (uint32_t)buf[p + 3];
    p += 4;

    /* msgNs: Word32 BE */
    out->ns = ((uint32_t)buf[p] << 24) | ((uint32_t)buf[p + 1] << 16) |
              ((uint32_t)buf[p + 2] << 8) | (uint32_t)buf[p + 3];
    p += 4;

    ESP_LOGD(TAG, "pq_header_deserialize: v=%u, kem=0x%02x, pn=%" PRIu32 ", ns=%" PRIu32 ", content=%u",
             out->version, out->kem_tag, out->pn, out->ns, content_len);

    return 0;
}

// ============== PQ Header Test (Session 46 Teil C) ==============

void pq_header_test(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== PQ Header Wire Format Test ===");

    /* Fake DH key for testing */
    uint8_t fake_dh[56];
    memset(fake_dh, 0xAA, 56);

    parsed_msg_header_t parsed;
    int ret;
    uint16_t cl;
    bool pass = true;

    /* ---- Test 1: Non-PQ header (88 bytes) ---- */
    uint8_t hdr_nopq[MSG_HEADER_PADDED_LEN];
    ret = pq_header_serialize(hdr_nopq, sizeof(hdr_nopq), fake_dh, NULL, 5, 10);
    cl = (hdr_nopq[0] << 8) | hdr_nopq[1];
    ESP_LOGI(TAG, "[1] Non-PQ: size=%d, content_len=%u, kem=0x%02x",
             ret, cl, hdr_nopq[73]);
    ESP_LOGI(TAG, "    first 16:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, hdr_nopq, 16, ESP_LOG_INFO);

    ret = pq_header_deserialize(hdr_nopq, sizeof(hdr_nopq), &parsed);
    if (ret != 0 || parsed.version != RATCHET_VERSION || parsed.pn != 5 ||
        parsed.ns != 10 || parsed.kem_tag != PQ_KEM_NOTHING || cl != 80) {
        ESP_LOGE(TAG, "    FAIL: deser ret=%d v=%u pn=%" PRIu32 " ns=%" PRIu32 " kem=0x%02x cl=%u",
                 ret, parsed.version, parsed.pn, parsed.ns, parsed.kem_tag, cl);
        pass = false;
    } else {
        ESP_LOGI(TAG, "    PASS: round-trip OK (v=%u pn=%" PRIu32 " ns=%" PRIu32 ")", parsed.version, parsed.pn, parsed.ns);
    }

    /* ---- Test 2: PQ Proposed (2310 bytes, content=1241) ---- */
    pq_kem_state_t pq_prop = {0};
    pq_prop.pq_active = 1;
    pq_prop.pq_kem_state = 1;
    memset(pq_prop.own_kem_pk, 0xBB, SNTRUP761_PUBLICKEYBYTES);

    uint8_t *hdr_prop = heap_caps_malloc(MSG_HEADER_PQ_PADDED_LEN, MALLOC_CAP_SPIRAM);
    if (!hdr_prop) { ESP_LOGE(TAG, "malloc failed"); return; }

    ret = pq_header_serialize(hdr_prop, MSG_HEADER_PQ_PADDED_LEN, fake_dh, &pq_prop, 3, 7);
    cl = (hdr_prop[0] << 8) | hdr_prop[1];
    ESP_LOGI(TAG, "[2] Proposed: size=%d, content_len=%u", ret, cl);
    ESP_LOGI(TAG, "    KEM area [73..77]: %02x %02x %02x %02x %02x",
             hdr_prop[73], hdr_prop[74], hdr_prop[75], hdr_prop[76], hdr_prop[77]);

    ret = pq_header_deserialize(hdr_prop, MSG_HEADER_PQ_PADDED_LEN, &parsed);
    if (ret != 0 || parsed.kem_tag != PQ_KEM_PROPOSED || !parsed.kem_pk_valid ||
        parsed.pn != 3 || parsed.ns != 7 || cl != 1241 ||
        parsed.kem_pk[0] != 0xBB) {
        ESP_LOGE(TAG, "    FAIL: ret=%d kem=0x%02x pk_v=%d pk[0]=0x%02x cl=%u",
                 ret, parsed.kem_tag, parsed.kem_pk_valid, parsed.kem_pk[0], cl);
        pass = false;
    } else {
        ESP_LOGI(TAG, "    PASS: round-trip OK (kem=P, pk[0]=0xBB, pn=%" PRIu32 " ns=%" PRIu32 ")", parsed.pn, parsed.ns);
    }
    free(hdr_prop);

    /* ---- Test 3: PQ Accepted (2310 bytes, content=2282) ---- */
    pq_kem_state_t pq_acc = {0};
    pq_acc.pq_active = 1;
    pq_acc.pq_kem_state = 2;
    memset(pq_acc.own_kem_pk, 0xCC, SNTRUP761_PUBLICKEYBYTES);
    memset(pq_acc.pending_ct, 0xDD, SNTRUP761_CIPHERTEXTBYTES);

    uint8_t *hdr_acc = heap_caps_malloc(MSG_HEADER_PQ_PADDED_LEN, MALLOC_CAP_SPIRAM);
    if (!hdr_acc) { ESP_LOGE(TAG, "malloc failed"); return; }

    ret = pq_header_serialize(hdr_acc, MSG_HEADER_PQ_PADDED_LEN, fake_dh, &pq_acc, 1, 0);
    cl = (hdr_acc[0] << 8) | hdr_acc[1];
    ESP_LOGI(TAG, "[3] Accepted: size=%d, content_len=%u", ret, cl);
    ESP_LOGI(TAG, "    KEM area [73..77]: %02x %02x %02x %02x %02x",
             hdr_acc[73], hdr_acc[74], hdr_acc[75], hdr_acc[76], hdr_acc[77]);

    ret = pq_header_deserialize(hdr_acc, MSG_HEADER_PQ_PADDED_LEN, &parsed);
    if (ret != 0 || parsed.kem_tag != PQ_KEM_ACCEPTED || !parsed.kem_pk_valid ||
        !parsed.kem_ct_valid || parsed.pn != 1 || parsed.ns != 0 || cl != 2282 ||
        parsed.kem_pk[0] != 0xCC || parsed.kem_ct[0] != 0xDD) {
        ESP_LOGE(TAG, "    FAIL: ret=%d kem=0x%02x pk_v=%d ct_v=%d cl=%u",
                 ret, parsed.kem_tag, parsed.kem_pk_valid, parsed.kem_ct_valid, cl);
        pass = false;
    } else {
        ESP_LOGI(TAG, "    PASS: round-trip OK (kem=A, pk[0]=0xCC, ct[0]=0xDD, pn=%" PRIu32 " ns=%" PRIu32 ")",
                 parsed.pn, parsed.ns);
    }
    free(hdr_acc);

    /* ---- Test 4: PQ active but kem_state=0 (Nothing, still 2310 padding) ---- */
    pq_kem_state_t pq_none = {0};
    pq_none.pq_active = 1;
    pq_none.pq_kem_state = 0;

    uint8_t *hdr_pqnone = heap_caps_malloc(MSG_HEADER_PQ_PADDED_LEN, MALLOC_CAP_SPIRAM);
    if (!hdr_pqnone) { ESP_LOGE(TAG, "malloc failed"); return; }

    ret = pq_header_serialize(hdr_pqnone, MSG_HEADER_PQ_PADDED_LEN, fake_dh, &pq_none, 0, 0);
    cl = (hdr_pqnone[0] << 8) | hdr_pqnone[1];
    ESP_LOGI(TAG, "[4] PQ-active Nothing: size=%d, content_len=%u, kem=0x%02x",
             ret, cl, hdr_pqnone[73]);

    if (ret != MSG_HEADER_PQ_PADDED_LEN || cl != 80 || hdr_pqnone[73] != PQ_KEM_NOTHING) {
        ESP_LOGE(TAG, "    FAIL: size or content mismatch");
        pass = false;
    } else {
        ESP_LOGI(TAG, "    PASS: Nothing with PQ padding (2310 bytes, anti-downgrade)");
    }
    free(hdr_pqnone);

    /* ---- Summary ---- */
    if (pass) {
        ESP_LOGI(TAG, "=== PQ Header Test: ALL PASS ===");
    } else {
        ESP_LOGE(TAG, "=== PQ Header Test: FAILURES ===");
    }
    ESP_LOGI(TAG, "");
}

// ============== PQ State Machine (Session 46 Teil E) ==============

void ratchet_set_recv_pq(const parsed_msg_header_t *hdr) {
    if (hdr) {
        memcpy(&s_recv_pq_hdr, hdr, sizeof(parsed_msg_header_t));
        s_recv_pq_valid = true;
    } else {
        s_recv_pq_valid = false;
    }
}

/**
 * PQ receive state machine. Processes KEM params from incoming header.
 * Returns 0 on success. If *kem_ss_valid is true, kem_ss contains 32 bytes
 * that must be fed into kdf_root alongside the DH shared secret.
 *
 * Uses pq_crypto_task for keygen (too heavy for main task stack).
 * Uses sntrup761_enc/dec directly (small enough, ~5-10 KB stack).
 */
static int pq_recv_process(pq_kem_state_t *pq,
                           uint8_t kem_tag,
                           const uint8_t *kem_pk,
                           const uint8_t *kem_ct,
                           uint8_t *kem_ss,
                           bool *kem_ss_valid) {
    *kem_ss_valid = false;
    bool pq_enabled = smp_settings_get_pq_enabled();

    /* ---- Fall 1: Nothing received ---- */
    if (kem_tag == PQ_KEM_NOTHING) {
        if (pq_enabled && pq->pq_kem_state == 0) {
            /* Want PQ, no state yet: generate keypair and propose */
            ESP_LOGI(TAG, "PQ-SM: Nothing received, generating keypair to propose...");
            if (!pq_crypto_get_precomputed(pq->own_kem_pk, pq->own_kem_sk)) {
                ESP_LOGW(TAG, "PQ-SM: No precomputed keypair, blocking keygen (~1.8s)...");
                if (pq_crypto_keygen(pq->own_kem_pk, pq->own_kem_sk, 5000) != ESP_OK) {
                    ESP_LOGE(TAG, "PQ-SM: keygen FAILED");
                    return -1;
                }
            }
            pq->own_kem_valid = 1;
            pq->pq_kem_state = 1;  /* proposed */
            pq->pq_active = 1;
            ESP_LOGI(TAG, "PQ-SM: State -> proposed (pk[0]=%02x)", pq->own_kem_pk[0]);
            pq_nvs_save(ratchet_get_active());  /* Write-Before-Send */
            pq_crypto_precompute_keypair();
        }
        /* Anti-downgrade: if already active, stay active even on Nothing */
        return 0;
    }

    /* ---- Fall 2: Proposed(peer_key) received ---- */
    if (kem_tag == PQ_KEM_PROPOSED) {
        ESP_LOGI(TAG, "PQ-SM: Proposed received, accepting...");

        /* Store peer's PQ public key */
        memcpy(pq->peer_kem_pk, kem_pk, SNTRUP761_PUBLICKEYBYTES);
        pq->peer_kem_valid = 1;

        /* Generate own keypair */
        if (!pq_crypto_get_precomputed(pq->own_kem_pk, pq->own_kem_sk)) {
            ESP_LOGW(TAG, "PQ-SM: No precomputed keypair, blocking keygen (~1.8s)...");
            if (pq_crypto_keygen(pq->own_kem_pk, pq->own_kem_sk, 5000) != ESP_OK) {
                ESP_LOGE(TAG, "PQ-SM: keygen FAILED in Proposed");
                return -2;
            }
        }
        pq->own_kem_valid = 1;

        /* Encapsulate to peer's key -> shared_secret + ciphertext.
         * The secret goes into pending_ss, NOT into kem_ss.
         * Reason: The sender used DH-only for this message's kdf_root.
         * Our encap secret will be sent as Accepted(ct) and fed into
         * our SEND kdf_root on the next outgoing ratchet step. */
        if (pq_crypto_enc(pq->pending_ct, pq->pending_ss, pq->peer_kem_pk, 3000) != ESP_OK) {
            ESP_LOGE(TAG, "PQ-SM: encap FAILED");
            return -3;
        }
        pq->pending_ct_valid = 1;
        pq->pending_ss_valid = 1;
        *kem_ss_valid = false;  /* NOT in current recv kdf_root */

        pq->pq_kem_state = 2;  /* accepted */
        pq->pq_active = 1;

        pq_crypto_precompute_keypair();
        ESP_LOGI(TAG, "PQ-SM: State -> accepted (pending_ss[0]=%02x, ct[0]=%02x, kem_ss=DEFERRED)",
                 pq->pending_ss[0], pq->pending_ct[0]);
        pq_nvs_save(ratchet_get_active());  /* Write-Before-Send */
        return 0;
    }

    /* ---- Fall 3: Accepted(ciphertext, peer_key) received ---- */
    if (kem_tag == PQ_KEM_ACCEPTED) {
        ESP_LOGI(TAG, "PQ-SM: Accepted received, decapsulating...");

        /* Must have a valid own keypair to decapsulate */
        if (!pq->own_kem_valid) {
            ESP_LOGE(TAG, "PQ-SM: CERatchetKEMState - no valid own keypair!");
            ESP_LOGW(TAG, "PQ-SM: Continuing without PQ for this round");
            return 0;  /* Not fatal, just no KEM secret */
        }

        /* Decapsulate -> shared_secret (ss1).
         * This goes into the current recv kdf_root because the SENDER
         * used this same secret in their send kdf_root. */
        if (pq_crypto_dec(kem_ss, kem_ct, pq->own_kem_sk, 3000) != ESP_OK) {
            ESP_LOGE(TAG, "PQ-SM: decap FAILED");
            return -4;
        }
        *kem_ss_valid = true;  /* ss1 goes into current recv kdf_root */

        /* Wipe consumed secret key immediately */
        sodium_memzero(pq->own_kem_sk, SNTRUP761_SECRETKEYBYTES);

        /* Store new peer key */
        memcpy(pq->peer_kem_pk, kem_pk, SNTRUP761_PUBLICKEYBYTES);
        pq->peer_kem_valid = 1;

        /* Generate new own keypair */
        if (!pq_crypto_get_precomputed(pq->own_kem_pk, pq->own_kem_sk)) {
            ESP_LOGW(TAG, "PQ-SM: No precomputed keypair, blocking keygen (~1.8s)...");
            if (pq_crypto_keygen(pq->own_kem_pk, pq->own_kem_sk, 5000) != ESP_OK) {
                ESP_LOGE(TAG, "PQ-SM: keygen FAILED after decap");
                pq->own_kem_valid = 0;
                return -5;
            }
        }
        pq->own_kem_valid = 1;

        /* Encapsulate with new peer key -> ss2 + ct2.
         * ss2 goes into pending_ss (for next SEND kdf_root).
         * ct2 goes into pending_ct (for next outgoing header). */
        if (pq_crypto_enc(pq->pending_ct, pq->pending_ss, pq->peer_kem_pk, 3000) != ESP_OK) {
            ESP_LOGE(TAG, "PQ-SM: encap with new peer key FAILED");
            return -6;
        }
        pq->pending_ct_valid = 1;
        pq->pending_ss_valid = 1;

        pq->pq_kem_state = 2;  /* accepted */
        pq->pq_active = 1;

        pq_crypto_precompute_keypair();
        ESP_LOGI(TAG, "PQ-SM: State -> accepted (decap ss[0]=%02x, new pending_ss[0]=%02x, ct[0]=%02x)",
                 kem_ss[0], pq->pending_ss[0], pq->pending_ct[0]);
        pq_nvs_save(ratchet_get_active());  /* Write-Before-Send */
        return 0;
    }

    ESP_LOGE(TAG, "PQ-SM: Unknown KEM tag 0x%02x", kem_tag);
    return -10;
}

// ============== HKDF KAT Test (Session 46 Teil D) ==============

void pq_hkdf_kat_test(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== PQ HKDF Root Key Derivation KAT ===");

    /* Fixed test vectors */
    uint8_t root_key[32];
    memset(root_key, 0x11, 32);

    uint8_t dh_out[56];
    memset(dh_out, 0x22, 56);

    uint8_t kem_secret[32];
    memset(kem_secret, 0x33, 32);

    uint8_t rk1[32], ck1[32], nhk1[32];
    uint8_t rk2[32], ck2[32], nhk2[32];
    uint8_t rk3[32], ck3[32], nhk3[32];
    uint8_t rk4[32], ck4[32], nhk4[32];
    bool pass = true;

    /* Test 1: DH-only, run twice for determinism */
    kdf_root(root_key, dh_out, NULL, 0, rk1, ck1, nhk1);
    kdf_root(root_key, dh_out, NULL, 0, rk2, ck2, nhk2);

    if (memcmp(rk1, rk2, 32) != 0 || memcmp(ck1, ck2, 32) != 0 || memcmp(nhk1, nhk2, 32) != 0) {
        ESP_LOGE(TAG, "[1] FAIL: DH-only not deterministic!");
        pass = false;
    } else {
        ESP_LOGI(TAG, "[1] PASS: DH-only deterministic");
        ESP_LOGI(TAG, "    RK:  %02x%02x%02x%02x...", rk1[0], rk1[1], rk1[2], rk1[3]);
        ESP_LOGI(TAG, "    CK:  %02x%02x%02x%02x...", ck1[0], ck1[1], ck1[2], ck1[3]);
        ESP_LOGI(TAG, "    NHK: %02x%02x%02x%02x...", nhk1[0], nhk1[1], nhk1[2], nhk1[3]);
    }

    /* Test 2: DH+KEM, run twice for determinism */
    kdf_root(root_key, dh_out, kem_secret, 32, rk3, ck3, nhk3);
    kdf_root(root_key, dh_out, kem_secret, 32, rk4, ck4, nhk4);

    if (memcmp(rk3, rk4, 32) != 0 || memcmp(ck3, ck4, 32) != 0 || memcmp(nhk3, nhk4, 32) != 0) {
        ESP_LOGE(TAG, "[2] FAIL: DH+KEM not deterministic!");
        pass = false;
    } else {
        ESP_LOGI(TAG, "[2] PASS: DH+KEM deterministic");
        ESP_LOGI(TAG, "    RK:  %02x%02x%02x%02x...", rk3[0], rk3[1], rk3[2], rk3[3]);
        ESP_LOGI(TAG, "    CK:  %02x%02x%02x%02x...", ck3[0], ck3[1], ck3[2], ck3[3]);
        ESP_LOGI(TAG, "    NHK: %02x%02x%02x%02x...", nhk3[0], nhk3[1], nhk3[2], nhk3[3]);
    }

    /* Test 3: DH-only vs DH+KEM must differ */
    if (memcmp(rk1, rk3, 32) == 0) {
        ESP_LOGE(TAG, "[3] FAIL: DH-only and DH+KEM produce same root key!");
        pass = false;
    } else {
        ESP_LOGI(TAG, "[3] PASS: KEM changes output (DH-only RK != DH+KEM RK)");
    }

    /* Test 4: KEM with zero-length = same as DH-only */
    uint8_t rk5[32], ck5[32], nhk5[32];
    kdf_root(root_key, dh_out, kem_secret, 0, rk5, ck5, nhk5);

    if (memcmp(rk1, rk5, 32) != 0) {
        ESP_LOGE(TAG, "[4] FAIL: kem_len=0 should equal DH-only!");
        pass = false;
    } else {
        ESP_LOGI(TAG, "[4] PASS: kem_len=0 equals DH-only (no regression)");
    }

    /* Wipe */
    sodium_memzero(rk1, 32); sodium_memzero(ck1, 32); sodium_memzero(nhk1, 32);
    sodium_memzero(rk3, 32); sodium_memzero(ck3, 32); sodium_memzero(nhk3, 32);

    if (pass) {
        ESP_LOGI(TAG, "=== PQ HKDF KAT: ALL PASS ===");
    } else {
        ESP_LOGE(TAG, "=== PQ HKDF KAT: FAILURES ===");
    }
    ESP_LOGI(TAG, "");
}

// ============== Encrypt Message ==============

int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len,
                    size_t padded_msg_len) {
    
    if (!ratchet_state.initialized) return -1;

    /* Determine PQ mode for this message */
    bool is_pq = (ratchet_state.pq.pq_active && smp_settings_get_pq_enabled());
    size_t hdr_padded = is_pq ? MSG_HEADER_PQ_PADDED_LEN : MSG_HEADER_PADDED_LEN;
    size_t em_hdr_total = 2 + GCM_IV_LEN + GCM_TAG_LEN + 2 + hdr_padded;
    /* Non-PQ: 2+16+16+2+88 = 124, PQ: 2+16+16+2+2310 = 2346 */

    /* Bug 2 Fix: PQ header is 2222 bytes larger. To keep total output the same
     * size (so all caller buffers still fit), reduce body padding by the
     * header overhead difference. The peer reads content_len from bytes [0:2]
     * and doesn't care about the amount of '#' padding after the content. */
    if (is_pq) {
        size_t pq_overhead = MSG_HEADER_PQ_PADDED_LEN - MSG_HEADER_PADDED_LEN;  /* 2222 */
        if (padded_msg_len > pq_overhead + 256) {  /* Ensure enough room for content */
            padded_msg_len -= pq_overhead;
            ESP_LOGD(TAG, "PQ: padded_msg_len reduced by %zu -> %zu", pq_overhead, padded_msg_len);
        } else {
            ESP_LOGE(TAG, "PQ: padded_msg_len %zu too small for PQ overhead %zu!", padded_msg_len, pq_overhead);
            return -1;
        }
    }

    ESP_LOGD(TAG, "Encrypt: msg_num=%" PRIu32 " pq=%d kem_state=%d hdr=%zu",
             ratchet_state.msg_num_send, is_pq ? 1 : 0,
             ratchet_state.pq.pq_kem_state, hdr_padded);

    int ret = -1;
    uint8_t *hdr_buf = NULL;
    uint8_t *enc_hdr = NULL;
    uint8_t *em_hdr = NULL;
    uint8_t *p_aad = NULL;
    uint8_t *padded_payload = NULL;
    uint8_t *encrypted_payload = NULL;

    /* 1. Derive keys & IVs from chain */
    uint8_t message_key[32], next_chain_key[32], msg_iv[16], header_iv[16];
    kdf_chain(ratchet_state.chain_key_send, next_chain_key, message_key, msg_iv, header_iv);
    memcpy(ratchet_state.chain_key_send, next_chain_key, 32);

    /* 2. Build plaintext header */
    hdr_buf = heap_caps_malloc(hdr_padded, MALLOC_CAP_SPIRAM);
    if (!hdr_buf) { ESP_LOGE(TAG, "Encrypt: hdr_buf alloc failed"); goto cleanup; }

    if (is_pq) {
        int ser_ret = pq_header_serialize(hdr_buf, hdr_padded,
                                          ratchet_state.dh_self.public_key,
                                          &ratchet_state.pq,
                                          ratchet_state.prev_chain_len,
                                          ratchet_state.msg_num_send);
        if (ser_ret < 0) { ESP_LOGE(TAG, "Encrypt: pq_header_serialize failed"); goto cleanup; }
    } else {
        build_msg_header(hdr_buf, ratchet_state.dh_self.public_key,
                         ratchet_state.prev_chain_len, ratchet_state.msg_num_send);
    }

    /* 3. Encrypt header with AES-256-GCM (AAD = assoc_data) */
    enc_hdr = heap_caps_malloc(hdr_padded, MALLOC_CAP_SPIRAM);
    if (!enc_hdr) { ESP_LOGE(TAG, "Encrypt: enc_hdr alloc failed"); goto cleanup; }

    uint8_t header_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        hdr_buf, hdr_padded,
                        enc_hdr, header_tag) != 0) {
        ESP_LOGE(TAG, "Encrypt: header AES-GCM failed");
        goto cleanup;
    }

    /* 4. Build emHeader: version + IV + tag + body_len + encrypted_body */
    em_hdr = heap_caps_malloc(em_hdr_total, MALLOC_CAP_SPIRAM);
    if (!em_hdr) { ESP_LOGE(TAG, "Encrypt: em_hdr alloc failed"); goto cleanup; }

    int hp = 0;
    em_hdr[hp++] = 0x00; em_hdr[hp++] = RATCHET_VERSION;
    memcpy(&em_hdr[hp], header_iv, 16); hp += 16;
    memcpy(&em_hdr[hp], header_tag, 16); hp += 16;
    em_hdr[hp++] = (hdr_padded >> 8) & 0xFF;
    em_hdr[hp++] = hdr_padded & 0xFF;
    memcpy(&em_hdr[hp], enc_hdr, hdr_padded); hp += hdr_padded;

    ESP_LOGD(TAG, "emHeader: v%d, %zu bytes (body=%zu)", RATCHET_VERSION, em_hdr_total, hdr_padded);

    /* 5. Encrypt body (AAD = assoc_data + emHeader) */
    size_t p_aad_len = 112 + em_hdr_total;
    p_aad = heap_caps_malloc(p_aad_len, MALLOC_CAP_SPIRAM);
    if (!p_aad) { ESP_LOGE(TAG, "Encrypt: p_aad alloc failed"); goto cleanup; }
    memcpy(p_aad, ratchet_state.assoc_data, 112);
    memcpy(p_aad + 112, em_hdr, em_hdr_total);

    padded_payload = malloc(padded_msg_len);
    if (!padded_payload) { ESP_LOGE(TAG, "Encrypt: padded_payload alloc failed"); goto cleanup; }

    padded_payload[0] = (pt_len >> 8) & 0xFF;
    padded_payload[1] = pt_len & 0xFF;
    memcpy(&padded_payload[2], plaintext, pt_len);
    memset(&padded_payload[2 + pt_len], '#', padded_msg_len - 2 - pt_len);

    encrypted_payload = malloc(padded_msg_len);
    if (!encrypted_payload) { ESP_LOGE(TAG, "Encrypt: encrypted_payload alloc failed"); goto cleanup; }

    uint8_t payload_tag[GCM_TAG_LEN];
    if (aes_gcm_encrypt(message_key, msg_iv, GCM_IV_LEN,
                        p_aad, p_aad_len,
                        padded_payload, padded_msg_len,
                        encrypted_payload, payload_tag) != 0) {
        ESP_LOGE(TAG, "Encrypt: body AES-GCM failed");
        goto cleanup;
    }

    /* 6. Assemble output: emHeader_len(2) + emHeader + payload_tag(16) + encrypted_payload */
    int op = 0;
    output[op++] = (em_hdr_total >> 8) & 0xFF;
    output[op++] = em_hdr_total & 0xFF;
    memcpy(&output[op], em_hdr, em_hdr_total); op += em_hdr_total;
    memcpy(&output[op], payload_tag, 16); op += 16;
    memcpy(&output[op], encrypted_payload, padded_msg_len); op += padded_msg_len;
    *out_len = op;

    ratchet_state.msg_num_send++;

    /* Evgeny's Rule: persist BEFORE caller sends over network */
    ratchet_save_state(ratchet_get_active());

    ESP_LOGI(TAG, "Encrypted %zu bytes (padded to %zu) -> %zu bytes, pq=%s",
             pt_len, padded_msg_len, *out_len, is_pq ? "YES" : "no");
    ret = 0;

cleanup:
    if (hdr_buf) { sodium_memzero(hdr_buf, hdr_padded); heap_caps_free(hdr_buf); }
    if (enc_hdr) heap_caps_free(enc_hdr);
    if (em_hdr) heap_caps_free(em_hdr);
    if (p_aad) heap_caps_free(p_aad);
    if (padded_payload) { sodium_memzero(padded_payload, padded_msg_len); free(padded_payload); }
    if (encrypted_payload) free(encrypted_payload);
    return ret;
}

// ============== Self-Decrypt Test ==============

int ratchet_self_decrypt_test(const uint8_t *ciphertext, size_t ct_len,
                              uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGI(TAG, "🔬 Self-decrypt test (header only)...");
    
    int p = 0;
    uint16_t em_hdr_len = (ciphertext[0] << 8) | ciphertext[1];
    if (em_hdr_len != 124) {
        ESP_LOGE(TAG, "   [FAIL] Expected emHeader len 124 (0x007C), got %u (0x%04x)", em_hdr_len, em_hdr_len);
        return -1;
    }
    p = 2;
    
    uint16_t version = (ciphertext[p] << 8) | ciphertext[p + 1]; p += 2;
    uint8_t header_iv[16];
    memcpy(header_iv, &ciphertext[p], 16); p += 16;
    uint8_t header_tag[16];
    memcpy(header_tag, &ciphertext[p], 16); p += 16;
    uint16_t eh_body_len = (ciphertext[p] << 8) | ciphertext[p + 1]; p += 2;
    const uint8_t *encrypted_header = &ciphertext[p];
    
    ESP_LOGI(TAG, "   Version: %d, ehBody len: %d", version, eh_body_len);
    
    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    
    if (aes_gcm_decrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        encrypted_header, eh_body_len,
                        header_tag, decrypted_header) != 0) {
        ESP_LOGE(TAG, "[FAIL] Self-decrypt FAILED!");
        ESP_LOGI(TAG, "   (This is expected - sender can't decrypt own message)");
        ESP_LOGI(TAG, "   Header encrypted with HKs, but for receiver it needs HKr");
        return -1;
    }
    
    ESP_LOGI(TAG, "[OK] Self-decrypt SUCCESS!");
    ESP_LOGI(TAG, "   Header bytes: %02x %02x %02x %02x...",
             decrypted_header[0], decrypted_header[1], 
             decrypted_header[2], decrypted_header[3]);
    
    return 0;
}

// ============== Decrypt Message ==============

int ratchet_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGW(TAG, "[WARN] DEPRECATED: ratchet_decrypt() called - use ratchet_decrypt_body() instead");
    ESP_LOGI(TAG, "🔓 Decrypting message (%zu bytes)...", ct_len);
    
    int p = 0;
    uint16_t em_header_len;

    em_header_len = (ciphertext[0] << 8) | ciphertext[1];
    p = 2;
    
    if (em_header_len == 124) {
        ESP_LOGI(TAG, "   ✓ v3 format (2-byte prefix, emHeader=124)");
    } else if (em_header_len == 123) {
        ESP_LOGI(TAG, "   [WARN] Detected v2 format (0x7B prefix) - reparsing");
        em_header_len = 123;
        p = 1;
    } else {
        ESP_LOGW(TAG, "   [WARN] Unexpected emHeader len: %u (0x%04x) - trying as v3", em_header_len, em_header_len);
    }

    ESP_LOGI(TAG, "   emHeader length: %u, starting at offset %d", em_header_len, p);
    
    const uint8_t *em_header = &ciphertext[p];
    p += em_header_len;
    
    int hp = 0;
    uint16_t version = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    ESP_LOGI(TAG, "   Version: %d", version);
    
    uint8_t header_iv[16];
    memcpy(header_iv, &em_header[hp], 16); hp += 16;
    
    uint8_t header_tag[16];
    memcpy(header_tag, &em_header[hp], 16); hp += 16;
    
    uint16_t eh_body_len;
    if (version >= 3) {
        eh_body_len = (em_header[hp] << 8) | em_header[hp + 1]; hp += 2;
    } else {
        eh_body_len = em_header[hp++];
    }
    ESP_LOGI(TAG, "   ehBody length: %d (v%d format)", eh_body_len, version);
    
    const uint8_t *encrypted_header = &em_header[hp];
    
    const uint8_t *payload_tag = &ciphertext[p]; p += 16;
    const uint8_t *encrypted_payload = &ciphertext[p];
    size_t payload_len = ct_len - p;
    
    ESP_LOGI(TAG, "   Payload length: %zu", payload_len);
    
    uint8_t decrypted_header[MSG_HEADER_PADDED_LEN];
    
    ESP_LOGI(TAG, "   rcAD (same for both directions): %02x%02x...||%02x%02x...",
             ratchet_state.assoc_data[0], ratchet_state.assoc_data[1],
             ratchet_state.assoc_data[56], ratchet_state.assoc_data[57]);

    ESP_LOGI(TAG, "   Trying header decrypt with header_key_recv...");
    ESP_LOGI(TAG, "   header_key_recv: %02x%02x%02x%02x...",
             ratchet_state.header_key_recv[0], ratchet_state.header_key_recv[1],
             ratchet_state.header_key_recv[2], ratchet_state.header_key_recv[3]);
    
    if (aes_gcm_decrypt(ratchet_state.header_key_recv, header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,
                        encrypted_header, eh_body_len,
                        header_tag, decrypted_header) != 0) {
        ESP_LOGE(TAG, "   [FAIL] Header decryption failed with header_key_recv!");
        
        ESP_LOGI(TAG, "   Trying with header_key_send...");
        if (aes_gcm_decrypt(ratchet_state.header_key_send, header_iv, GCM_IV_LEN,
                            ratchet_state.assoc_data, 112,
                            encrypted_header, eh_body_len,
                            header_tag, decrypted_header) != 0) {
            ESP_LOGE(TAG, "   [FAIL] Header decryption also failed with header_key_send!");
            return -1;
        }
        ESP_LOGI(TAG, "   [OK] Header decrypted with header_key_send");
    } else {
        ESP_LOGI(TAG, "   [OK] Header decrypted with header_key_recv");
    }
    
    ESP_LOGD(TAG, "MsgHeader: %02x%02x %02x%02x %02x...",
             decrypted_header[0], decrypted_header[1],
             decrypted_header[2], decrypted_header[3], decrypted_header[4]);
    
    int mhp = 0;
    uint16_t content_len = (decrypted_header[mhp] << 8) | decrypted_header[mhp + 1]; mhp += 2;
    uint16_t msg_version = (decrypted_header[mhp] << 8) | decrypted_header[mhp + 1]; mhp += 2;
    uint8_t key_len = decrypted_header[mhp++];
    
    ESP_LOGI(TAG, "   Content len: %d, Version: %d, Key len: %d", content_len, msg_version, key_len);
    
    if (key_len != 68) {
        ESP_LOGE(TAG, "   [FAIL] Unexpected key length: %d", key_len);
        return -1;
    }
    
    uint8_t peer_new_dh[56];
    memcpy(peer_new_dh, &decrypted_header[mhp + 12], 56);
    mhp += 68;
    
    if (msg_version >= 3) {
        uint8_t kem_tag = decrypted_header[mhp];
        ESP_LOGI(TAG, "   KEM tag: 0x%02x '%c' %s", kem_tag, kem_tag,
                 kem_tag == 0x30 ? "(Nothing)" : "(Just - PQ!)");
        if (kem_tag == 0x30) {
            mhp += 1;
        } else if (kem_tag == 0x31) {
            ESP_LOGW(TAG, "   [WARN] KEM = Just (PQ mode) - not implemented!");
            return -1;
        }
    }
    
    uint32_t msg_pn = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    mhp += 4;
    uint32_t msg_ns = (decrypted_header[mhp] << 24) | (decrypted_header[mhp+1] << 16) |
                      (decrypted_header[mhp+2] << 8) | decrypted_header[mhp+3];
    
    ESP_LOGI(TAG, "   PN: %" PRIu32 ", Ns: %" PRIu32 "", msg_pn, msg_ns);
    ESP_LOGI(TAG, "   Peer new DH: %02x%02x%02x%02x...",
             peer_new_dh[0], peer_new_dh[1], peer_new_dh[2], peer_new_dh[3]);
    
    bool dh_changed = (memcmp(peer_new_dh, ratchet_state.dh_peer, 56) != 0);
    
    if (dh_changed) {
        ESP_LOGI(TAG, "   [SYNC] New DH key detected - doing ratchet step...");
        
        uint8_t dh_out[56];
        if (!x448_dh(peer_new_dh, ratchet_state.dh_self.private_key, dh_out)) {
            ESP_LOGE(TAG, "   [FAIL] DH failed!");
            return -1;
        }
        
        ESP_LOGI(TAG, "   DH output: %02x%02x%02x%02x...",
                 dh_out[0], dh_out[1], dh_out[2], dh_out[3]);
        
        uint8_t new_root_key[32], new_chain_key[32], new_header_key[32];
        kdf_root(ratchet_state.root_key, dh_out, NULL, 0, new_root_key, new_chain_key, new_header_key);
        
        memcpy(ratchet_state.root_key, new_root_key, 32);
        memcpy(ratchet_state.chain_key_recv, new_chain_key, 32);
        memcpy(ratchet_state.header_key_recv, new_header_key, 32);
        memcpy(ratchet_state.dh_peer, peer_new_dh, 56);
        ratchet_state.msg_num_recv = 0;
        
        ESP_LOGI(TAG, "   New chain_key_recv: %02x%02x%02x%02x...",
                 ratchet_state.chain_key_recv[0], ratchet_state.chain_key_recv[1],
                 ratchet_state.chain_key_recv[2], ratchet_state.chain_key_recv[3]);
    }
    
    uint8_t message_key[32], next_chain_key[32], msg_iv[16], unused_iv[16];
    uint8_t temp_ck[32];
    memcpy(temp_ck, ratchet_state.chain_key_recv, 32);
    
    for (uint32_t i = ratchet_state.msg_num_recv; i < msg_ns; i++) {
        kdf_chain(temp_ck, next_chain_key, message_key, msg_iv, unused_iv);
        memcpy(temp_ck, next_chain_key, 32);
        ESP_LOGI(TAG, "   Skipped to msg %" PRIu32 "", i + 1);
    }
    
    kdf_chain(temp_ck, next_chain_key, message_key, msg_iv, unused_iv);
    memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
    ratchet_state.msg_num_recv = msg_ns + 1;
    
    ESP_LOGI(TAG, "   message_key: %02x%02x%02x%02x...",
             message_key[0], message_key[1], message_key[2], message_key[3]);
    
    uint8_t *payload_aad = malloc(112 + em_header_len);
    if (!payload_aad) {
        ESP_LOGE(TAG, "   [FAIL] malloc failed");
        return -1;
    }
    memcpy(payload_aad, ratchet_state.assoc_data, 112);
    memcpy(payload_aad + 112, em_header, em_header_len);
    
    if (aes_gcm_decrypt(message_key, msg_iv, GCM_IV_LEN,
                        payload_aad, 112 + em_header_len,
                        encrypted_payload, payload_len,
                        payload_tag, plaintext) != 0) {
        ESP_LOGE(TAG, "   [FAIL] Payload decryption failed!");
        free(payload_aad);
        return -1;
    }
    
    free(payload_aad);
    
    uint16_t actual_len = (plaintext[0] << 8) | plaintext[1];
    ESP_LOGI(TAG, "   Padded: %zu bytes, actual: %d bytes", payload_len, actual_len);
    
    memmove(plaintext, plaintext + 2, actual_len);
    *pt_len = actual_len;
    
    ESP_LOGI(TAG, "   [OK] Decrypted message: %zu bytes", *pt_len);
    ESP_LOGI(TAG, "   First bytes: %02x %02x %02x %02x",
             plaintext[0], plaintext[1], plaintext[2], plaintext[3]);
    
    return 0;
}

// ============== Decrypt Incoming Message ==============

int ratchet_decrypt_incoming(const uint8_t *ciphertext, size_t ct_len,
                             uint8_t *plaintext, size_t *pt_len) {
    ESP_LOGI(TAG, "🔓 Decrypting INCOMING message from peer (%zu bytes)...", ct_len);
    return ratchet_decrypt(ciphertext, ct_len, plaintext, pt_len);
}

// ============== Decrypt Body (Phase 2b) ==============

int ratchet_decrypt_body(ratchet_decrypt_mode_t mode,
                         const uint8_t *peer_new_pub,
                         uint32_t msg_pn, uint32_t msg_ns,
                         const uint8_t *em_header_raw, size_t em_header_len,
                         const uint8_t *em_auth_tag,
                         const uint8_t *em_body, size_t em_body_len,
                         uint8_t *plaintext, size_t *pt_len) {

    ESP_LOGI(TAG, "Decrypt body: mode=%s pn=%" PRIu32 " ns=%" PRIu32 " hdr=%zu body=%zu",
             mode == RATCHET_MODE_SAME ? "same" : "advance",
             msg_pn, msg_ns, em_header_len, em_body_len);

    // Variables needed by both modes
    uint8_t recv_chain_key[32];
    uint8_t new_root_key_2[32] = {0};
    uint8_t send_chain_key[32] = {0};
    uint8_t new_nhk_recv[32] = {0};
    uint8_t new_nhk_send[32] = {0};
    x448_keypair_t new_dh_self = {0};

    if (mode == RATCHET_MODE_ADVANCE) {
    // DH Ratchet Step 1: Receiving Chain
    uint8_t dh_secret_recv[56];
    if (!x448_dh(peer_new_pub, ratchet_state.dh_self.private_key, dh_secret_recv)) {
        ESP_LOGE(TAG, "X448 DH (recv) failed!");
        return -1;
    }

    // PQ State Machine: process KEM params from received header.
    // Fall 2 (Proposed): kem_ss_valid = false (secret deferred to send side)
    // Fall 3 (Accepted): kem_ss_valid = true (sender used this secret)
    uint8_t kem_ss[SNTRUP761_BYTES];
    bool kem_ss_valid = false;
    if (s_recv_pq_valid && smp_settings_get_pq_enabled()) {
        int pq_ret = pq_recv_process(&ratchet_state.pq,
                                     s_recv_pq_hdr.kem_tag,
                                     s_recv_pq_hdr.kem_pk_valid ? s_recv_pq_hdr.kem_pk : NULL,
                                     s_recv_pq_hdr.kem_ct_valid ? s_recv_pq_hdr.kem_ct : NULL,
                                     kem_ss, &kem_ss_valid);
        if (pq_ret != 0) {
            ESP_LOGW(TAG, "PQ-SM returned %d, continuing with DH-only", pq_ret);
            kem_ss_valid = false;
        }
        s_recv_pq_valid = false;
    }

    uint8_t new_root_key_1[32];
    kdf_root(ratchet_state.root_key, dh_secret_recv,
             kem_ss_valid ? kem_ss : NULL, kem_ss_valid ? SNTRUP761_BYTES : 0,
             new_root_key_1, recv_chain_key, new_nhk_recv);

    if (kem_ss_valid) {
        sodium_memzero(kem_ss, sizeof(kem_ss));
    }

    ESP_LOGD(TAG, "DH recv: rk1=%02x%02x.. ck=%02x%02x.. pq_recv=%s",
             new_root_key_1[0], new_root_key_1[1],
             recv_chain_key[0], recv_chain_key[1],
             kem_ss_valid ? "YES" : "no");

    // DH Ratchet Step 2: Sending Chain
    if (!x448_generate_keypair(&new_dh_self)) {
        ESP_LOGE(TAG, "X448 keygen failed!");
        return -2;
    }

    uint8_t dh_secret_send[56];
    if (!x448_dh(peer_new_pub, new_dh_self.private_key, dh_secret_send)) {
        ESP_LOGE(TAG, "X448 DH (send) failed!");
        return -3;
    }

    // Feed pending_ss into send kdf_root if available AND PQ sending is active.
    // pending_ss must only be used when the outgoing header will contain
    // Accepted(ct), so the peer can decap and derive the same chain keys.
    bool pq_send_active = (ratchet_state.pq.pq_active && smp_settings_get_pq_enabled());
    uint8_t *send_kem = NULL;
    size_t send_kem_len = 0;
    if (ratchet_state.pq.pending_ss_valid && pq_send_active) {
        send_kem = ratchet_state.pq.pending_ss;
        send_kem_len = SNTRUP761_BYTES;
    }

    kdf_root(new_root_key_1, dh_secret_send, send_kem, send_kem_len,
             new_root_key_2, send_chain_key, new_nhk_send);

    if (send_kem) {
        ESP_LOGI(TAG, "PQ: pending_ss[0]=%02x fed into send kdf_root", ratchet_state.pq.pending_ss[0]);
        sodium_memzero(ratchet_state.pq.pending_ss, SNTRUP761_BYTES);
        ratchet_state.pq.pending_ss_valid = 0;
    }

    ESP_LOGD(TAG, "DH send: rk2=%02x%02x.. ck=%02x%02x.. pq_send=%s",
             new_root_key_2[0], new_root_key_2[1],
             send_chain_key[0], send_chain_key[1],
             send_kem ? "YES" : "no");

    } else {
        // RATCHET_MODE_SAME: Skip DH ratchet, use existing chain_key_recv
        memcpy(recv_chain_key, ratchet_state.chain_key_recv, 32);
        ESP_LOGD(TAG, "Same ratchet: ck=%02x%02x..",
                 recv_chain_key[0], recv_chain_key[1]);
    }

    // Re-delivery detection: In SAME mode, if msg_ns < msg_num_recv,
    // this message was already processed. Return early without decrypting.
    if (mode == RATCHET_MODE_SAME && msg_ns < ratchet_state.msg_num_recv) {
        ESP_LOGW(TAG, "Re-delivery: msg_ns=%" PRIu32 " < recv=%" PRIu32 ", skipping",
                 msg_ns, ratchet_state.msg_num_recv);
        return -10;  // RE_DELIVERY
    }

    // Chain KDF
    uint8_t temp_ck[32];
    memcpy(temp_ck, recv_chain_key, 32);

    uint8_t message_key[32], next_chain_key[32], iv_body[16], iv_header[16];

    uint32_t skip_from = (mode == RATCHET_MODE_ADVANCE) ? 0 : ratchet_state.msg_num_recv;
    ESP_LOGD(TAG, "Chain KDF: from=%" PRIu32 " to=%" PRIu32 ", skip=%" PRIu32 "",
             skip_from, msg_ns, (msg_ns > skip_from) ? msg_ns - skip_from : 0);

    for (uint32_t i = skip_from; i < msg_ns; i++) {
        kdf_chain(temp_ck, next_chain_key, message_key, iv_body, iv_header);
        memcpy(temp_ck, next_chain_key, 32);
    }

    kdf_chain(temp_ck, next_chain_key, message_key, iv_body, iv_header);

    // AES-256-GCM Body Decrypt
    size_t aad_len = 112 + em_header_len;
    uint8_t *aad = malloc(aad_len);
    if (!aad) {
        ESP_LOGE(TAG, "malloc AAD failed!");
        return -4;
    }
    memcpy(aad, ratchet_state.assoc_data, 112);
    memcpy(aad + 112, em_header_raw, em_header_len);

    ESP_LOGD(TAG, "AES-GCM: aad=%zu bytes, body=%zu bytes", aad_len, em_body_len);

    int ret = aes_gcm_decrypt(message_key, iv_body, GCM_IV_LEN,
                               aad, aad_len,
                               em_body, em_body_len,
                               em_auth_tag, plaintext);
    free(aad);

    if (ret != 0) {
        ESP_LOGE(TAG, "AES-GCM body decrypt failed (ret=%d)", ret);
        return -5;
    }

    // unPad
    uint16_t actual_len = (plaintext[0] << 8) | plaintext[1];

    if (actual_len > em_body_len - 2) {
        ESP_LOGE(TAG, "unPad length invalid! %u > %zu", actual_len, em_body_len - 2);
        return -6;
    }

    memmove(plaintext, plaintext + 2, actual_len);
    *pt_len = actual_len;

    ESP_LOGI(TAG, "Decrypted: %zu bytes (padded=%zu), tag=0x%02X",
             *pt_len, em_body_len, plaintext[0]);

    if (mode == RATCHET_MODE_ADVANCE) {
        // Apply state updates
        memcpy(ratchet_state.root_key, new_root_key_2, 32);
        memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
        memcpy(ratchet_state.chain_key_send, send_chain_key, 32);

        memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32);
        memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
        memcpy(ratchet_state.next_header_key_send, new_nhk_send, 32);
        memcpy(ratchet_state.next_header_key_recv, new_nhk_recv, 32);

        memcpy(ratchet_state.dh_self.private_key, new_dh_self.private_key, 56);
        memcpy(ratchet_state.dh_self.public_key, new_dh_self.public_key, 56);
        memcpy(ratchet_state.dh_peer, peer_new_pub, 56);

        ratchet_state.prev_chain_len = ratchet_state.msg_num_send;
        ratchet_state.msg_num_recv = msg_ns + 1;
        ratchet_state.msg_num_send = 0;

        ESP_LOGI(TAG, "Advance: recv=%" PRIu32 " send=%" PRIu32 " prev=%" PRIu32 "",
                 ratchet_state.msg_num_recv, ratchet_state.msg_num_send,
                 ratchet_state.prev_chain_len);
    } else {
        memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);
        ratchet_state.msg_num_recv = msg_ns + 1;

        ESP_LOGI(TAG, "Same: recv=%" PRIu32 "", ratchet_state.msg_num_recv);
    }

    ratchet_save_state(ratchet_get_active());

    ESP_LOGI(TAG, "Decrypt body OK (%zu bytes)", *pt_len);

    return 0;
}

// ============== Persistence (Auftrag 50b) ==============

/* Session 46: Classical ratchet size = struct BEFORE the pq field.
 * Full struct is 5640 bytes (exceeds NVS blob limit of ~4000 bytes).
 * PQ fields are saved separately via pq_nvs_save() in Teil F. */
#define RATCHET_CLASSICAL_SIZE  offsetof(ratchet_state_t, pq)

bool ratchet_save_state(uint8_t contact_idx) {
    if (contact_idx >= MAX_RATCHETS) {
        ESP_LOGE(TAG, "ratchet_save_state: invalid contact_idx %d", contact_idx);
        return false;
    }
    if (!ratchet_state.initialized) {
        ESP_LOGW(TAG, "ratchet_save_state: state not initialized, skipping");
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "rat_%02u", contact_idx);

    /* Save only classical part - PQ fields saved separately via pq_nvs_save() */
    esp_err_t ret = smp_storage_save_blob_sync(key, &ratchet_state, RATCHET_CLASSICAL_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ratchet_save_state('%s') FAILED: %s", key, esp_err_to_name(ret));
        return false;
    }

    ESP_LOGD(TAG, "Ratchet saved: '%s' (%zu bytes) | send=%" PRIu32 " recv=%" PRIu32 "",
             key, RATCHET_CLASSICAL_SIZE,
             ratchet_state.msg_num_send, ratchet_state.msg_num_recv);
    return true;
}

bool ratchet_load_state(uint8_t contact_idx) {
    if (contact_idx >= MAX_RATCHETS) {
        ESP_LOGE(TAG, "ratchet_load_state: invalid contact_idx %d", contact_idx);
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "rat_%02u", contact_idx);

    if (!smp_storage_exists(key)) {
        ESP_LOGI(TAG, "ratchet_load_state: '%s' not found - fresh start", key);
        return false;
    }

    /* Bug #23: ratchet_state_t is ~5.7 KB after PQ integration (Session 46).
     * Stack allocation crashed LVGL task (8 KB stack) when called from
     * on_contact_click -> smp_set_active_contact -> ratchet_set_active.
     * Fix: allocate in PSRAM, zero-wipe before free. */
    ratchet_state_t *loaded = heap_caps_malloc(sizeof(ratchet_state_t), MALLOC_CAP_SPIRAM);
    if (!loaded) {
        ESP_LOGE(TAG, "ratchet_load_state: PSRAM alloc failed (%zu bytes)", sizeof(ratchet_state_t));
        return false;
    }

    /* Zero entire struct (PQ fields default to 0 = inactive) */
    size_t loaded_len = 0;
    memset(loaded, 0, sizeof(ratchet_state_t));

    /* Load classical part only. NVS blob is RATCHET_CLASSICAL_SIZE (~520 bytes).
     * Old pre-PQ blobs (also 520 bytes) load identically. */
    esp_err_t ret = smp_storage_load_blob(key, loaded, RATCHET_CLASSICAL_SIZE, &loaded_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ratchet_load_state('%s') FAILED: %s", key, esp_err_to_name(ret));
        sodium_memzero(loaded, sizeof(ratchet_state_t));
        heap_caps_free(loaded);
        return false;
    }

    /* Accept both old (520) and new (RATCHET_CLASSICAL_SIZE) sizes */
    #define RATCHET_STATE_SIZE_PRE_PQ  520
    if (loaded_len != RATCHET_CLASSICAL_SIZE && loaded_len != RATCHET_STATE_SIZE_PRE_PQ) {
        ESP_LOGE(TAG, "ratchet_load_state: size mismatch! got %zu, expected %zu or %zu",
                 loaded_len, RATCHET_CLASSICAL_SIZE, (size_t)RATCHET_STATE_SIZE_PRE_PQ);
        sodium_memzero(loaded, sizeof(ratchet_state_t));
        heap_caps_free(loaded);
        return false;
    }
    if (!loaded->initialized) {
        ESP_LOGW(TAG, "ratchet_load_state: loaded state has initialized=false!");
        sodium_memzero(loaded, sizeof(ratchet_state_t));
        heap_caps_free(loaded);
        return false;
    }

    /* Accept loaded state (PQ fields stay zeroed, loaded via pq_nvs_load) */
    memcpy(&ratchet_state, loaded, sizeof(ratchet_state_t));

    ESP_LOGI(TAG, "Ratchet restored: '%s' (%zu bytes) send=%" PRIu32 " recv=%" PRIu32 "",
             key, loaded_len,
             ratchet_state.msg_num_send, ratchet_state.msg_num_recv);

    /* Wipe and free temp buffer - keys were in here */
    sodium_memzero(loaded, sizeof(ratchet_state_t));
    heap_caps_free(loaded);
    return true;
}

// ============== PQ NVS Persistence (Session 46 Teil F) ==============

bool pq_nvs_save(uint8_t contact_idx) {
    if (contact_idx >= MAX_RATCHETS) return false;

    pq_kem_state_t *pq = &ratchet_state.pq;
    char key[16];
    esp_err_t ret;
    bool ok = true;

    snprintf(key, sizeof(key), "pq_%02x_act", contact_idx);
    ret = smp_storage_save_blob_sync(key, &pq->pq_active, 1);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }

    snprintf(key, sizeof(key), "pq_%02x_st", contact_idx);
    ret = smp_storage_save_blob_sync(key, &pq->pq_kem_state, 1);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }

    /* Only save key material if there's something valid */
    if (pq->own_kem_valid) {
        snprintf(key, sizeof(key), "pq_%02x_opk", contact_idx);
        ret = smp_storage_save_blob_sync(key, pq->own_kem_pk, SNTRUP761_PUBLICKEYBYTES);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }

        snprintf(key, sizeof(key), "pq_%02x_osk", contact_idx);
        ret = smp_storage_save_blob_sync(key, pq->own_kem_sk, SNTRUP761_SECRETKEYBYTES);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }
    }

    if (pq->peer_kem_valid) {
        snprintf(key, sizeof(key), "pq_%02x_ppk", contact_idx);
        ret = smp_storage_save_blob_sync(key, pq->peer_kem_pk, SNTRUP761_PUBLICKEYBYTES);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }
    }

    if (pq->pending_ct_valid) {
        snprintf(key, sizeof(key), "pq_%02x_ct", contact_idx);
        ret = smp_storage_save_blob_sync(key, pq->pending_ct, SNTRUP761_CIPHERTEXTBYTES);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }
    }

    if (pq->pending_ss_valid) {
        snprintf(key, sizeof(key), "pq_%02x_ss", contact_idx);
        ret = smp_storage_save_blob_sync(key, pq->pending_ss, SNTRUP761_BYTES);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "pq_nvs_save %s FAIL", key); ok = false; }
    }

    if (ok) {
        ESP_LOGI(TAG, "PQ NVS saved: [%02x] active=%u state=%u own=%u peer=%u ct=%u ss=%u",
                 contact_idx, pq->pq_active, pq->pq_kem_state,
                 pq->own_kem_valid, pq->peer_kem_valid, pq->pending_ct_valid,
                 pq->pending_ss_valid);
    }
    return ok;
}

bool pq_nvs_load(uint8_t contact_idx) {
    if (contact_idx >= MAX_RATCHETS) return false;

    pq_kem_state_t *pq = &ratchet_state.pq;
    char key[16];
    size_t len;

    /* Check if PQ state exists for this contact */
    snprintf(key, sizeof(key), "pq_%02x_act", contact_idx);
    if (!smp_storage_exists(key)) {
        ESP_LOGD(TAG, "pq_nvs_load [%02x]: no PQ state in NVS", contact_idx);
        return false;
    }

    len = 0;
    smp_storage_load_blob(key, &pq->pq_active, 1, &len);

    snprintf(key, sizeof(key), "pq_%02x_st", contact_idx);
    len = 0;
    smp_storage_load_blob(key, &pq->pq_kem_state, 1, &len);

    snprintf(key, sizeof(key), "pq_%02x_opk", contact_idx);
    if (smp_storage_exists(key)) {
        len = 0;
        smp_storage_load_blob(key, pq->own_kem_pk, SNTRUP761_PUBLICKEYBYTES, &len);
        snprintf(key, sizeof(key), "pq_%02x_osk", contact_idx);
        len = 0;
        smp_storage_load_blob(key, pq->own_kem_sk, SNTRUP761_SECRETKEYBYTES, &len);
        pq->own_kem_valid = 1;
    }

    snprintf(key, sizeof(key), "pq_%02x_ppk", contact_idx);
    if (smp_storage_exists(key)) {
        len = 0;
        smp_storage_load_blob(key, pq->peer_kem_pk, SNTRUP761_PUBLICKEYBYTES, &len);
        pq->peer_kem_valid = 1;
    }

    snprintf(key, sizeof(key), "pq_%02x_ct", contact_idx);
    if (smp_storage_exists(key)) {
        len = 0;
        smp_storage_load_blob(key, pq->pending_ct, SNTRUP761_CIPHERTEXTBYTES, &len);
        pq->pending_ct_valid = 1;
    }

    snprintf(key, sizeof(key), "pq_%02x_ss", contact_idx);
    if (smp_storage_exists(key)) {
        len = 0;
        smp_storage_load_blob(key, pq->pending_ss, SNTRUP761_BYTES, &len);
        pq->pending_ss_valid = 1;
    }

    ESP_LOGI(TAG, "PQ NVS loaded: [%02x] active=%u state=%u own=%u peer=%u ct=%u ss=%u",
             contact_idx, pq->pq_active, pq->pq_kem_state,
             pq->own_kem_valid, pq->peer_kem_valid, pq->pending_ct_valid,
             pq->pending_ss_valid);
    return true;
}

// ============== PQ Settings (Session 46: SEC-06) ==============

static uint8_t s_pq_enabled = 1;       // Cached value, default ON
static bool s_pq_loaded = false;        // True after first NVS read

uint8_t smp_settings_get_pq_enabled(void) {
    if (s_pq_loaded) return s_pq_enabled;

    // First read: try NVS
    uint8_t val = 1;  // Default: PQ ON
    size_t len = 0;
    esp_err_t ret = smp_storage_load_blob("pq_enabled", &val, sizeof(val), &len);
    if (ret != ESP_OK || len != sizeof(val)) {
        // Key doesn't exist (first boot) - create with default
        val = 1;
        smp_storage_save_blob_sync("pq_enabled", &val, sizeof(val));
        ESP_LOGI(TAG, "PQ setting: created with default ON");
    } else {
        ESP_LOGI(TAG, "PQ setting: loaded from NVS = %u", val);
    }

    s_pq_enabled = val;
    s_pq_loaded = true;
    return s_pq_enabled;
}

void smp_settings_set_pq_enabled(uint8_t val) {
    val = val ? 1 : 0;  // Normalize
    s_pq_enabled = val;
    s_pq_loaded = true;
    smp_storage_save_blob_sync("pq_enabled", &val, sizeof(val));
    ESP_LOGI(TAG, "PQ setting: saved to NVS = %u", val);
}

// ============== Getters ==============

ratchet_state_t *ratchet_get_state(void) { return &ratchet_state; }
bool ratchet_is_initialized(void) { return ratchet_state.initialized; }

const uint8_t *ratchet_get_saved_hk(void) { return saved_x3dh_valid ? saved_x3dh_hk : NULL; }
const uint8_t *ratchet_get_saved_nhk(void) { return saved_x3dh_valid ? saved_x3dh_nhk : NULL; }
