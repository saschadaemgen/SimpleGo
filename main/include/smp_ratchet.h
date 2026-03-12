/**
 * SimpleGo - smp_ratchet.h
 * Double Ratchet encryption state and operations interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_RATCHET_H
#define SMP_RATCHET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smp_x448.h"
#include "sntrup761.h"  // Session 46: PQ KEM key sizes

// ============== Constants ==============

#define RATCHET_KEY_LEN         32
#define RATCHET_IV_LEN          16
#define RATCHET_TAG_LEN         16
#define RATCHET_HEADER_LEN      88
#define X448_PUBLIC_KEY_LEN     56
#define MAX_RATCHETS  128   // Session 33: Multi-contact support (68KB in PSRAM)

// Session 46 Teil C: PQ header wire format constants
#define MSG_HEADER_PQ_PADDED_LEN  2310   // Padded PQ header (hides Proposed vs Accepted)
#define PQ_KEM_NOTHING            0x30   // '0' - no KEM params
#define PQ_KEM_JUST               0x31   // '1' - KEM params present
#define PQ_KEM_PROPOSED           0x50   // 'P' - sending own public key
#define PQ_KEM_ACCEPTED           0x41   // 'A' - sending ciphertext + public key

// ============== PQ KEM State (Session 46: SEC-06) ==============

typedef struct {
    uint8_t pq_active;                                      // 0 = no PQ, 1 = PQ active
    uint8_t pq_kem_state;                                   // 0 = none, 1 = proposed, 2 = accepted

    // Own KEM keypair (for receiving ciphertexts from peer)
    uint8_t own_kem_pk[SNTRUP761_PUBLICKEYBYTES];           // 1158 bytes
    uint8_t own_kem_sk[SNTRUP761_SECRETKEYBYTES];           // 1763 bytes
    uint8_t own_kem_valid;                                  // 1 if keypair is valid

    // Peer's last KEM public key (for encapsulation)
    uint8_t peer_kem_pk[SNTRUP761_PUBLICKEYBYTES];          // 1158 bytes
    uint8_t peer_kem_valid;                                 // 1 if peer key is valid

    // Pending ciphertext (to send in next header)
    uint8_t pending_ct[SNTRUP761_CIPHERTEXTBYTES];          // 1039 bytes
    uint8_t pending_ct_valid;                               // 1 if ciphertext ready to send

    // Pending shared secret (from Encap, waiting for next SEND kdf_root)
    uint8_t pending_ss[SNTRUP761_BYTES];                    // 32 bytes
    uint8_t pending_ss_valid;                               // 1 if ss ready for send-side kdf_root
} pq_kem_state_t;

// ============== Ratchet State ==============

typedef struct {
    // Root key (shared secret from X3DH)
    uint8_t root_key[RATCHET_KEY_LEN];
    
    // Header keys (for encrypting/decrypting message headers)
    uint8_t header_key_send[RATCHET_KEY_LEN];       // HKs - active send header key
    uint8_t header_key_recv[RATCHET_KEY_LEN];        // HKr - active recv header key
    
    // Next header keys (promoted to active on DH ratchet step)
    uint8_t next_header_key_send[RATCHET_KEY_LEN];   // NHKs (Haskell: rcNHKs)
    uint8_t next_header_key_recv[RATCHET_KEY_LEN];    // NHKr (Haskell: rcNHKr)
    
    // Chain keys (for deriving message keys)
    uint8_t chain_key_send[RATCHET_KEY_LEN];
    uint8_t chain_key_recv[RATCHET_KEY_LEN];
    
    // DH keypairs
    x448_keypair_t dh_self;                // Our current DH keypair
    uint8_t        dh_peer[X448_PUBLIC_KEY_LEN];  // Peer's current DH public key
    
    // Message counters
    uint32_t msg_num_send;           // Messages sent in current chain
    uint32_t msg_num_recv;           // Messages received in current chain
    uint32_t prev_chain_len;         // Length of previous sending chain
    
    // State flags
    bool initialized;
    
    // Associated Data for AEAD
    // CRITICAL FIX v0.1.22: rcAD = initiator_key1 || responder_key1
    // Since we are RESPONDER and peer is INITIATOR:
    //   assoc_data = peer_key1 (56 bytes) || our_key1 (56 bytes)
    // This is ABSOLUTE (role-based), NOT relative (our/peer)!
    uint8_t assoc_data[112];         // 56 + 56 bytes = initiator || responder

    // Session 46: Post-quantum KEM state per contact
    // ~5.1 KB per contact. Ignored entirely when pq.pq_active == 0.
    pq_kem_state_t pq;

} ratchet_state_t;

// ============== Parsed MsgHeader (Session 46 Teil C) ==============

typedef struct {
    uint16_t version;
    uint8_t dh_public[X448_PUBLIC_KEY_LEN];              // Raw X448 key (no SPKI)
    uint8_t kem_tag;                                      // PQ_KEM_NOTHING, _PROPOSED, or _ACCEPTED
    uint8_t kem_pk[SNTRUP761_PUBLICKEYBYTES];             // 1158 bytes (valid when kem_pk_valid)
    uint8_t kem_ct[SNTRUP761_CIPHERTEXTBYTES];            // 1039 bytes (valid when kem_ct_valid)
    bool kem_pk_valid;
    bool kem_ct_valid;
    uint32_t pn;
    uint32_t ns;
} parsed_msg_header_t;

// ============== PQ Header Serialization (Session 46 Teil C) ==============

/**
 * Serialize a plaintext MsgHeader with optional PQ KEM fields.
 *
 * Without PQ: 88 bytes (content 80 + padding to 88)
 * With PQ:    2310 bytes (content varies + padding to 2310)
 *
 * @param buf       Output buffer (must be >= 88 or >= 2310 bytes)
 * @param buf_size  Size of output buffer
 * @param dh_public Our X448 DH public key (56 bytes, raw)
 * @param pq        PQ KEM state (NULL = no PQ)
 * @param pn        Previous chain length
 * @param ns        Message number in current chain
 * @return Padded header size (88 or 2310) on success, negative on error
 */
int pq_header_serialize(uint8_t *buf, size_t buf_size,
                        const uint8_t *dh_public,
                        const pq_kem_state_t *pq,
                        uint32_t pn, uint32_t ns);

/**
 * Deserialize a decrypted plaintext MsgHeader.
 *
 * Handles both non-PQ (88 bytes) and PQ (2310 bytes) headers.
 * The content_len field at offset 0 determines how much is content vs padding.
 *
 * @param buf      Decrypted header buffer
 * @param buf_len  Buffer length (88 or 2310)
 * @param out      Parsed header output
 * @return 0 on success, negative on error
 */
int pq_header_deserialize(const uint8_t *buf, size_t buf_len,
                          parsed_msg_header_t *out);

/**
 * Run PQ header serialization round-trip test.
 * Logs hex dumps to serial monitor for verification.
 */
void pq_header_test(void);

/**
 * Run HKDF KAT (Known-Answer-Test) for PQ root key derivation.
 * Verifies:
 *   1. DH-only produces deterministic output
 *   2. DH+KEM produces different output from DH-only
 *   3. DH+KEM is deterministic
 */
void pq_hkdf_kat_test(void);

// ============== Decrypt Mode (SameRatchet vs AdvanceRatchet) ==============

typedef enum {
    RATCHET_MODE_ADVANCE,    // New DH key from peer → full DH ratchet step (recv + send)
    RATCHET_MODE_SAME        // Same DH key → chain step only, no DH, no new keypair
} ratchet_decrypt_mode_t;

// ============== Session 33: Multi-Contact Init ==============

/**
 * Allocate ratchet array in PSRAM and set active index.
 * Must be called once at startup, before any ratchet operations.
 * @return true on success
 */
bool ratchet_multi_init(void);

/**
 * Set active ratchet index. All subsequent encrypt/decrypt/save
 * operations work on this contact's ratchet state.
 * @param idx  Contact index (0 to MAX_RATCHETS-1)
 * @return true if valid index
 */
bool ratchet_set_active(uint8_t idx);

/**
 * Get current active ratchet index.
 */
uint8_t ratchet_get_active(void);

// ============== X3DH Key Agreement ==============

/**
 * Perform X3DH key agreement as sender (initiator)
 * This establishes the initial root key for the ratchet
 * 
 * @param peer_key1           Peer's X448 identity key (56 bytes) - INITIATOR
 * @param peer_key2           Peer's X448 signed prekey (56 bytes)
 * @param our_key1            Our X448 identity keypair - RESPONDER
 * @param our_key2            Our ephemeral X448 keypair (used for DH)
 * @return true on success
 */
bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2);

// ============== Ratchet Initialization ==============

/**
 * Initialize the ratchet for sending (after X3DH)
 * 
 * @param peer_dh_public  Peer's initial DH public key (56 bytes)
 * @param our_key2        Our ephemeral keypair used in X3DH
 * @return true on success
 */
bool ratchet_init_sender(const uint8_t *peer_dh_public, 
                        const x448_keypair_t *our_key2);

// ============== Encryption/Decryption ==============

/**
 * Encrypt a message using the Double Ratchet
 * 
 * Output format (Version 3, non-PQ):
 *   [0-1]     emHeader-len = 0x007C (124, Word16 BE)
 *   [2-125]   emHeader (124 bytes)
 *   [126-141] payload AuthTag (16 bytes)
 *   [142-...] encrypted payload
 * 
 * emHeader structure (v3):
 *   [0-1]     ehVersion (Word16 BE) = 3
 *   [2-17]    ehIV (16 bytes)
 *   [18-33]   ehAuthTag (16 bytes)
 *   [34-35]   ehBody-len = 0x0058 (88, Word16 BE Large encoding)
 *   [36-123]  ehBody (88 bytes encrypted MsgHeader)
 * 
 * MsgHeader (v3, non-PQ, padded to 88 bytes):
 *   [0-1]     Word16 content length = 80
 *   [2-3]     msgMaxVersion = 3 (Word16 BE)
 *   [4]       DH key length = 68
 *   [5-72]    msgDHRs (X448 SPKI, 68 bytes)
 *   [73]      msgKEM = '0' (0x30 = Nothing)
 *   [74-77]   msgPN (Word32 BE)
 *   [78-81]   msgNs (Word32 BE)
 *   [82-87]   '#' padding (6 bytes)
 * 
 * @param plaintext       Input data to encrypt
 * @param pt_len          Length of plaintext
 * @param output          Output buffer (must be large enough: pt_len + ~160 bytes)
 * @param out_len         Output: actual length written
 * @param padded_msg_len  Target padded message length
 * @return 0 on success, negative on error
 */
int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len,
                    size_t padded_msg_len);

/**
 * Decrypt a message using the Double Ratchet
 * 
 * @param ciphertext  Encrypted data (EncRatchetMessage format)
 * @param ct_len      Length of ciphertext
 * @param plaintext   Output buffer for decrypted message
 * @param pt_len      Output: actual length written
 * @return 0 on success, negative on error
 */
int ratchet_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    uint8_t *plaintext, size_t *pt_len);

/**
 * Self-decrypt test (for debugging)
 * Attempts to decrypt a message we just encrypted (header only)
 * Note: This is expected to fail because sender uses HKs but receiver needs HKr
 */
int ratchet_self_decrypt_test(const uint8_t *ciphertext, size_t ct_len,
                              uint8_t *plaintext, size_t *pt_len);

/**
 * Decrypt the body of an incoming EncRatchetMessage (Phase 2b)
 * 
 * Called AFTER header has been successfully decrypted and MsgHeader parsed.
 * 
 * ADVANCE mode: Full DH Ratchet (recv + send) → Chain KDF → AES-GCM → unPad
 *   Used when peer's DH key is NEW (header decrypted with NHKr)
 * 
 * SAME mode: Chain KDF only → AES-GCM → unPad  
 *   Used when peer's DH key is SAME (header decrypted with HKr)
 * 
 * @param mode             RATCHET_MODE_ADVANCE or RATCHET_MODE_SAME
 * @param peer_new_pub     Peer's DH public key from MsgHeader (56 bytes, raw X448)
 * @param msg_pn           PN (previous chain length) from MsgHeader
 * @param msg_ns           Ns (message number) from MsgHeader
 * @param em_header_raw    Raw emHeader bytes as received (for AAD, NOT decrypted)
 * @param em_header_len    Length of emHeader (124 for v3, 123 for v2)
 * @param em_auth_tag      emAuthTag (16 bytes)
 * @param em_body          Encrypted body (emBody)
 * @param em_body_len      Length of emBody
 * @param plaintext        Output buffer (must be >= em_body_len)
 * @param pt_len           Output: actual plaintext length after unPad
 * @return 0 on success, negative on error
 */
int ratchet_decrypt_body(ratchet_decrypt_mode_t mode,
                         const uint8_t *peer_new_pub,
                         uint32_t msg_pn, uint32_t msg_ns,
                         const uint8_t *em_header_raw, size_t em_header_len,
                         const uint8_t *em_auth_tag,
                         const uint8_t *em_body, size_t em_body_len,
                         uint8_t *plaintext, size_t *pt_len);

// ============== Persistence (Auftrag 50b) ==============

/**
 * Save ratchet state to NVS for the given contact index.
 * Uses smp_storage_save_blob_sync() for Write-Before-Send safety.
 * NVS key: "rat_XX" (XX = contact index 00-7F, hex)
 *
 * @param contact_idx  Contact index (0-127)
 * @return true on success
 */
bool ratchet_save_state(uint8_t contact_idx);

/**
 * Load ratchet state from NVS into the PSRAM array slot.
 * Validates size and initialized flag before accepting.
 * On failure, ratchet stays uninitialized (normal handshake flow).
 *
 * @param contact_idx  Contact index (0-127)
 * @return true on success (ratchet state restored)
 */
bool ratchet_load_state(uint8_t contact_idx);

// ============== PQ NVS Persistence (Session 46 Teil F) ==============

/**
 * Save PQ KEM state to separate NVS keys for Write-Before-Send safety.
 * Keys: pq_XX_act, pq_XX_st, pq_XX_opk, pq_XX_osk, pq_XX_ppk, pq_XX_ct
 * Must run on smp_app_task (SRAM stack, NVS-safe).
 *
 * @param contact_idx  Contact slot (0-127)
 * @return true on success
 */
bool pq_nvs_save(uint8_t contact_idx);

/**
 * Load PQ KEM state from NVS into active ratchet state.
 * Called at boot after ratchet_load_state for active contacts.
 *
 * @param contact_idx  Contact slot (0-127)
 * @return true if PQ state was found and loaded
 */
bool pq_nvs_load(uint8_t contact_idx);

// ============== PQ State Machine (Session 46 Teil E) ==============

/**
 * Set received PQ KEM params from parsed header.
 * Must be called BEFORE ratchet_decrypt_body() when PQ header was parsed.
 * The decrypt_body ADVANCE path reads these to run the PQ state machine.
 *
 * @param hdr  Parsed header with KEM fields (NULL to clear)
 */
void ratchet_set_recv_pq(const parsed_msg_header_t *hdr);

// ============== PQ Settings (Session 46: SEC-06) ==============

/**
 * Get PQ encryption enabled setting from NVS.
 * Default: 1 (enabled). Created on first read if missing.
 * @return 1 = PQ enabled, 0 = PQ disabled
 */
uint8_t smp_settings_get_pq_enabled(void);

/**
 * Set PQ encryption enabled setting in NVS.
 * @param val  1 = enable PQ, 0 = disable PQ
 */
void smp_settings_set_pq_enabled(uint8_t val);

// ============== State Access / Debug ==============

/**
 * Get pointer to global ratchet state (for debugging/inspection/persistence)
 */
ratchet_state_t *ratchet_get_state(void);

/**
 * Check if ratchet has been initialized
 */
bool ratchet_is_initialized(void);

/**
 * Get saved X3DH keys (captured before ratchet_init_sender modifies state)
 * Returns NULL if X3DH hasn't run yet
 */
const uint8_t *ratchet_get_saved_hk(void);
const uint8_t *ratchet_get_saved_nhk(void);

#endif // SMP_RATCHET_H