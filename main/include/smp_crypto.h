/**
 * SimpleGo - smp_crypto.h
 * Cryptographic functions for SMP protocol
 */

#ifndef SMP_CRYPTO_H
#define SMP_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include "smp_types.h"

// X25519 (32-byte keys) - for SMP envelope encryption
#define X25519_KEY_SIZE 32

// X448 (56-byte keys) - for E2E Ratchet encryption
#define X448_KEY_SIZE 56
#define X448_SHARED_SIZE 56

// Decrypt SMP message (Layer 3 - server DH)
bool decrypt_smp_message(contact_t *c, const uint8_t *encrypted, int enc_len,
                         const uint8_t *nonce, uint8_t nonce_len,
                         uint8_t *plain, int *plain_len);

// Decrypt client message (Layer 5 - contact DH, X25519)
int decrypt_client_msg(const uint8_t *enc, int enc_len,
                       const uint8_t *sender_dh_pub,
                       const uint8_t *our_dh_priv,
                       uint8_t *plain);

// Encrypt message for peer (X25519)
int encrypt_for_peer(const uint8_t *plain, int plain_len,
                     const uint8_t *peer_dh_pub,
                     const uint8_t *our_dh_priv,
                     uint8_t *encrypted);

// ============== X448 Functions (for E2E Ratchet) ==============

// Generate X448 keypair
int smp_x448_keygen(uint8_t *private_key,    // 56 bytes output
                    uint8_t *public_key);     // 56 bytes output

// X448 Diffie-Hellman
int smp_x448_dh(const uint8_t *our_private,   // 56 bytes
                const uint8_t *their_public,  // 56 bytes
                uint8_t *shared_secret);      // 56 bytes output

// Parse X448 public key from SPKI format
int smp_x448_parse_spki(const uint8_t *spki, int spki_len,
                        uint8_t *raw_pubkey);  // 56 bytes output

#endif // SMP_CRYPTO_H
