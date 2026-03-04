/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * smp_types.h - Data structures and constants
 * v0.1.14-alpha - Added E2E ratchet key support
 */

#ifndef SMP_TYPES_H
#define SMP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "sodium.h"

// ============== Constants ==============

#define SMP_BLOCK_SIZE      16384
#define MAX_CONTACTS        128   // Session 33: Multi-contact (35KB in PSRAM)
#define NVS_NAMESPACE       "simplego"
#define SPKI_KEY_SIZE       44  // 12 header + 32 key

// X448 key size (for E2E ratchet)
#define X448_KEY_SIZE       56  // 448 / 8 = 56 bytes

// ============== Contact Structure ==============

typedef struct {
    char name[32];                                    // Contact name
    uint8_t rcv_auth_secret[crypto_sign_SECRETKEYBYTES]; // 64 bytes
    uint8_t rcv_auth_public[crypto_sign_PUBLICKEYBYTES]; // 32 bytes
    uint8_t rcv_dh_secret[32];                        // X25519 secret
    uint8_t rcv_dh_public[32];                        // X25519 public
    uint8_t recipient_id[24];                         // Queue recipient ID
    uint8_t recipient_id_len;
    uint8_t sender_id[24];                            // Queue sender ID
    uint8_t sender_id_len;
    uint8_t srv_dh_public[32];                        // Server DH public key
    uint8_t have_srv_dh;                              // 1 if srv_dh_public is valid
    uint8_t active;                                   // 1=active, 0=slot free
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t *contacts;          // Session 33: PSRAM-allocated array
} contacts_db_t;

// ============== Peer Queue Structure ==============

typedef struct {
    char host[64];
    int port;
    uint8_t key_hash[32];
    char key_hash_b64[48];
    uint8_t queue_id[32];
    int queue_id_len;
    uint8_t dh_public[32];              // X25519 for SMP-level encryption
    int has_dh;                         // 1 if dh_public is valid
    
    // E2E Ratchet keys (X448) - received from peer's invitation
    uint8_t e2e_key1[X448_KEY_SIZE];    // First X448 public key for X3DH
    uint8_t e2e_key2[X448_KEY_SIZE];    // Second X448 public key for X3DH
    int has_e2e;                        // 1 if e2e keys are present
    
    int valid;
} peer_queue_t;

// ============== Peer Connection Structure ==============

typedef struct {
    int sock;
    void *ssl;           // mbedtls_ssl_context*
    void *conf;          // mbedtls_ssl_config*
    void *entropy;       // mbedtls_entropy_context*
    void *ctr_drbg;      // mbedtls_ctr_drbg_context*
    uint8_t session_id[32];
    uint8_t server_key_hash[32];
    bool connected;
} peer_connection_t;

#endif // SMP_TYPES_H