/**
 * SimpleGo - smp_x448.h
 * X448 Elliptic Curve Diffie-Hellman for E2E Ratchet
 * Requires wolfSSL with HAVE_CURVE448 enabled
 * v0.1.15-alpha
 */

#ifndef SMP_X448_H
#define SMP_X448_H

#include <stdint.h>
#include <stdbool.h>

// X448 key sizes
#define X448_KEY_SIZE       56
#define X448_SPKI_SIZE      68  // 12 header + 56 key

// X448 SPKI header: 30 42 30 05 06 03 2b 65 6f 03 39 00
extern const uint8_t X448_SPKI_HEADER[12];

// Key pair structure
typedef struct {
    uint8_t public_key[X448_KEY_SIZE];
    uint8_t private_key[X448_KEY_SIZE];
} x448_keypair_t;

// E2E Ratchet params we send back
typedef struct {
    uint8_t version_min;        // E2E version range min (2)
    uint8_t version_max;        // E2E version range max (2, not 3 to avoid PQ)
    x448_keypair_t key1;        // First X448 keypair (identity key)
    x448_keypair_t key2;        // Second X448 keypair (ratchet key)
    // Kyber1024 KEM keys (Post-Quantum)
    uint8_t kem_public_key[1568];   // KYBER_PUBLICKEYBYTES for K=4
    uint8_t kem_secret_key[3168];   // KYBER_SECRETKEYBYTES for K=4
    bool has_kem;                    // true if KEM keys generated
} e2e_params_t;

/**
 * Initialize X448 crypto (wolfSSL RNG)
 * Returns true on success
 */
bool x448_init(void);

/**
 * Generate X448 keypair
 * Returns true on success
 */
bool x448_generate_keypair(x448_keypair_t *keypair);

/**
 * Perform X448 Diffie-Hellman
 * shared_secret must be X448_KEY_SIZE bytes
 * Returns true on success
 */
bool x448_dh(const uint8_t *their_public, 
             const uint8_t *my_private,
             uint8_t *shared_secret);

/**
 * Encode X448 public key to SPKI format
 * output must be X448_SPKI_SIZE bytes
 */
void x448_encode_spki(const uint8_t *public_key, uint8_t *output);

/**
 * Encode X448 public key to Base64URL for URI
 * output must be at least 100 bytes
 * Returns length of encoded string
 */
int x448_encode_base64url(const uint8_t *public_key, char *output);

/**
 * Generate E2E ratchet parameters to send in AgentConfirmation
 * Generates two X448 keypairs
 */
bool e2e_generate_params(e2e_params_t *params);

/**
 * Encode E2E params for sending (SMP binary format)
 * Format: version(2 BE) + key1_spki(68) + key2_spki(68)
 * output must be at least 140 bytes
 * Returns encoded length (138 bytes)
 */
int e2e_encode_params(const e2e_params_t *params, uint8_t *output);

#endif // SMP_X448_H
