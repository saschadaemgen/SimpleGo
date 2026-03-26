/**
 * SimpleGo - reply_queue.h
 * Per-contact reply queue management
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef REPLY_QUEUE_H
#define REPLY_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "smp_queue.h"   // QUEUE_ID_SIZE, DH_PUBLIC_SIZE, AUTH_KEY_SIZE

// Forward declare SSL context to avoid mbedtls include here
typedef struct mbedtls_ssl_context mbedtls_ssl_context;

// ============== Per-Contact Reply Queue ==============

typedef struct {
    // IDs from server (IDS response to our NEW)
    uint8_t rcv_id[QUEUE_ID_SIZE];
    int     rcv_id_len;
    uint8_t snd_id[QUEUE_ID_SIZE];
    int     snd_id_len;

    // Server DH public key (from IDS response)
    uint8_t srv_dh_public[DH_PUBLIC_SIZE];

    // Our keypairs for this queue
    uint8_t rcv_auth_public[AUTH_KEY_SIZE];      // Ed25519 public (signing)
    uint8_t rcv_auth_private[64];                 // Ed25519 private
    uint8_t rcv_dh_public[DH_PUBLIC_SIZE];       // X25519 public (server DH)
    uint8_t rcv_dh_private[DH_PUBLIC_SIZE];      // X25519 private (server DH)

    // E2E encryption keys (separate from server DH)
    uint8_t e2e_public[DH_PUBLIC_SIZE];          // X25519 public for E2E
    uint8_t e2e_private[DH_PUBLIC_SIZE];         // X25519 private for E2E

    // Shared secret (rcv_dh_private * srv_dh_public) for server-level decrypt
    uint8_t shared_secret[DH_PUBLIC_SIZE];

    // Peer's E2E public key (from AgentConfirmation/HELLO)
    uint8_t e2e_peer_public[DH_PUBLIC_SIZE];
    bool    e2e_peer_valid;

    // Slot is initialized and has valid server-assigned IDs
    bool valid;

    // Session 49: Old DH keys preserved for peer-send after queue rotation.
    // After rotation, contact->rcv_dh_* gets new values (for decrypt on new server).
    // Peer-send still needs old values (peer server hasn't changed).
    uint8_t peer_dh_secret[DH_PUBLIC_SIZE];     // old rcv_dh_secret for peer send
    uint8_t peer_dh_public[DH_PUBLIC_SIZE];     // old rcv_dh_public for peer send
    bool    has_peer_dh;                         // true if peer_dh fields are valid

    // Session 50: Old auth keys preserved for peer-send after queue rotation.
    // Same pattern as peer_dh: after rotation, rcv_auth_* gets new values
    // (for SUB on new server), but peer-send still needs old values.
    uint8_t peer_auth_private[64];              // old rcv_auth_private for peer send
    uint8_t peer_auth_public[AUTH_KEY_SIZE];    // old rcv_auth_public for peer send
    bool    has_peer_auth;                       // true if peer_auth fields are valid
} reply_queue_t;

// PSRAM-allocated array: one reply_queue_t per contact slot
typedef struct {
    reply_queue_t queues[128];  // Matches MAX_CONTACTS from smp_types.h
} reply_queue_db_t;

// Global PSRAM pointer (allocated by reply_queues_init)
extern reply_queue_db_t *reply_queue_db;

// ============== Lifecycle ==============

/**
 * Allocate reply_queue_db in PSRAM. Safe to call multiple times.
 * Must be called after PSRAM is available (early in app_main).
 * @return true if allocated (or already allocated)
 */
bool reply_queues_init(void);

// ============== Queue Operations ==============

/**
 * Create a new reply queue on the SMP server for a specific contact slot.
 * Sends NEW command on the MAIN SSL connection (no separate TLS).
 * Generates all keypairs, parses IDS response, computes shared secret.
 * Persists to NVS immediately after success (Write-Before-Send).
 *
 * @param ssl         Main SSL context (shared with contact queues)
 * @param block       Scratch buffer, must be >= SMP_BLOCK_SIZE
 * @param session_id  32-byte TLS session ID for SMP signing
 * @param slot        Contact index [0..MAX_CONTACTS-1]
 * @return 0 on success, negative on error
 */
int reply_queue_create(mbedtls_ssl_context *ssl, uint8_t *block,
                       const uint8_t *session_id, int slot);

/**
 * Encode per-contact reply queue as SMPQueueInfo for CONFIRMATION.
 * Uses server host/port/key_hash from global our_queue.
 * Format: clientVersion(4) + smpServer + senderId + dhPublicKey(E2E)
 *
 * @param slot     Contact index
 * @param buf      Output buffer
 * @param max_len  Buffer size
 * @return bytes written, or negative on error
 */
int reply_queue_encode_info(int slot, uint8_t *buf, int max_len);

// ============== Lookup ==============

/**
 * Find which contact slot owns a reply queue by matching rcv_id.
 * Used in MSG routing to dispatch reply queue messages to correct contact.
 *
 * @param entity_id   Entity ID from incoming MSG
 * @param entity_len  Length of entity ID
 * @return contact slot index [0..MAX_CONTACTS-1], or -1 if not found
 */
int find_reply_queue_by_rcv_id(const uint8_t *entity_id, int entity_len);

/**
 * Get pointer to reply queue for a contact slot.
 * @return pointer to reply_queue_t, or NULL if db not initialized or slot invalid
 */
reply_queue_t *reply_queue_get(int slot);

// ============== NVS Persistence ==============

/**
 * Save reply queue credentials to NVS.
 * Key format: "rq_XX" where XX is hex slot number.
 * @return true on success
 */
bool reply_queue_save(int slot);

/**
 * Load reply queue credentials from NVS.
 * @return true on success (queue restored and valid)
 */
bool reply_queue_load(int slot);

/**
 * Load ALL previously saved reply queues from NVS.
 * Called once at boot after reply_queues_init().
 * @return number of queues successfully loaded
 */
int reply_queues_load_all(void);

#endif // REPLY_QUEUE_H
