/**
 * SimpleGo - smp_ratchet.h
 * Double Ratchet Encryption with CORRECT Wire Format
 * v0.1.22-alpha - Updated 2026-02-03
 * 
 * CRITICAL: rcAD = initiator_pub || responder_pub (ABSOLUTE order!)
 * We are RESPONDER, peer is INITIATOR
 * Therefore: rcAD = peer_key1 || our_key1
 */

#ifndef SMP_RATCHET_H
#define SMP_RATCHET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smp_x448.h"

// ============== Constants ==============

#define RATCHET_KEY_LEN         32
#define RATCHET_IV_LEN          16
#define RATCHET_TAG_LEN         16
#define RATCHET_HEADER_LEN      88
#define X448_PUBLIC_KEY_LEN     56
#define MAX_RATCHETS  128   // Session 33: Multi-contact support (68KB in PSRAM)

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

} ratchet_state_t;

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