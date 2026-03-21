/**
 * SimpleGo - smp_rotation.h
 * Queue Rotation: migrate contacts from old server to new server
 *
 * Uses QADD/QKEY/QUSE/QTEST agent messages over existing Double Ratchet.
 * No new wire format - rotation messages are normal agent messages with
 * different payload tags (QA/QK/QU/QT instead of M for chat).
 *
 * State machine per contact, persisted to NVS at every step.
 * Dual-TLS: old server stays connected for receive + sending rotation
 * messages, new server connection for NEW/KEY commands.
 *
 * Reference: simplexmq/protocol/agent-protocol.md "Rotating messaging queue"
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_ROTATION_H
#define SMP_ROTATION_H

#include <stdint.h>
#include <stdbool.h>

// ============== Per-Contact Migration State ==============

typedef enum {
    ROT_IDLE            = 0,   // Not migrating
    ROT_QUEUE_CREATED   = 1,   // NEW on new server done, have rcvId/sndId
    ROT_QADD_SENT       = 2,   // QADD sent to contact via Double Ratchet
    ROT_QKEY_RECEIVED   = 3,   // Contact replied with QKEY (sender key)
    ROT_KEY_SENT        = 4,   // KEY command sent on new queue
    ROT_QUSE_SENT       = 5,   // QUSE sent to contact
    ROT_QTEST_SENT      = 6,   // QTEST sent on new queue
    ROT_DONE            = 7,   // Migration complete for this contact
    ROT_ERROR           = 8,   // Migration failed for this contact
    ROT_WAITING         = 9,   // Contact offline, waiting for QKEY reply
} rotation_contact_state_t;

// ============== Global Rotation State ==============

typedef enum {
    ROT_GLOBAL_IDLE     = 0,   // No rotation active
    ROT_GLOBAL_ACTIVE   = 1,   // Rotation in progress
    ROT_GLOBAL_COMPLETE = 2,   // All contacts done
} rotation_global_state_t;

// ============== Per-Contact Rotation Data ==============
// Stored in NVS per contact during migration.
// Each contact needs TWO new queues on the target server:
//   1. Main receive queue (contact sends messages to us here)
//   2. Reply queue RQ[i] (used for handshake/confirmation traffic)

typedef struct {
    rotation_contact_state_t state;

    // --- Main Queue (new, on target server) ---
    uint8_t new_rcv_id[24];
    uint8_t new_rcv_id_len;
    uint8_t new_snd_id[24];
    uint8_t new_snd_id_len;
    uint8_t new_rcv_auth_public[32];
    uint8_t new_rcv_auth_private[64];
    uint8_t new_rcv_dh_public[32];
    uint8_t new_rcv_dh_private[32];
    uint8_t new_e2e_public[32];
    uint8_t new_e2e_private[32];
    uint8_t new_srv_dh_public[32];
    uint8_t new_shared_secret[32];

    // --- Reply Queue RQ[i] (new, on target server) ---
    uint8_t rq_new_rcv_id[24];
    uint8_t rq_new_rcv_id_len;
    uint8_t rq_new_snd_id[24];
    uint8_t rq_new_snd_id_len;
    uint8_t rq_new_rcv_auth_public[32];
    uint8_t rq_new_rcv_auth_private[64];
    uint8_t rq_new_rcv_dh_public[32];
    uint8_t rq_new_rcv_dh_private[32];
    uint8_t rq_new_e2e_public[32];
    uint8_t rq_new_e2e_private[32];
    uint8_t rq_new_srv_dh_public[32];
    uint8_t rq_new_shared_secret[32];
    bool    rq_created;             // True after RQ NEW command succeeded

    // --- Sender key received from contact (QKEY response) ---
    uint8_t peer_sender_key[44];    // Ed25519 SPKI (12 hdr + 32 key)
    bool    has_peer_sender_key;
    uint8_t peer_e2e_public[32];    // X25519 E2E DH from QKEY dhPublicKey
    bool    has_peer_e2e_public;

    // --- Retry tracking ---
    uint32_t retry_count;
    uint32_t last_retry_time;       // Epoch seconds
} rotation_contact_data_t;

// ============== Global Rotation Context ==============

typedef struct {
    rotation_global_state_t state;
    uint8_t target_server_idx;      // Index in smp_servers list
    char    target_host[64];        // Resolved hostname
    uint16_t target_port;           // Resolved port
    uint8_t target_key_hash[32];    // Resolved fingerprint
    uint8_t contacts_total;         // Total contacts to migrate
    uint8_t contacts_done;          // Contacts finished (DONE)
    uint8_t contacts_waiting;       // Contacts waiting (offline)
    uint8_t contacts_error;         // Contacts failed
    char    old_host[64];           // Fix 5: Old server for nachzuegler subscribe
    uint16_t old_port;              // Fix 5: Old server port
} rotation_context_t;

// ============== API ==============

/**
 * Initialize rotation module. Call once at boot.
 * Checks NVS for in-progress rotation and restores state.
 */
void rotation_init(void);

/**
 * Start rotation to a new server.
 * Sets global state to ACTIVE, saves target server info to NVS.
 * Does NOT reboot - caller handles that.
 *
 * @param target_server_idx  Index in smp_servers list
 * @return true if rotation started successfully
 */
bool rotation_start(uint8_t target_server_idx);

/**
 * Check if a rotation is currently active.
 */
bool rotation_is_active(void);

/**
 * Get current rotation context (read-only).
 */
const rotation_context_t *rotation_get_context(void);

/**
 * Get migration state for a specific contact.
 */
rotation_contact_state_t rotation_get_contact_state(int contact_idx);

/**
 * Get migration data for a specific contact (read-only).
 */
const rotation_contact_data_t *rotation_get_contact_data(int contact_idx);

/**
 * Execute Phase 1 for one contact: create queue on new server.
 * Requires active TLS connection to target server.
 *
 * @param contact_idx  Contact slot index
 * @return true if queue created successfully (state -> ROT_QUEUE_CREATED)
 */
bool rotation_create_queue_for_contact(int contact_idx);

/**
 * Build QADD agent message payload (the aMessage portion).
 * Caller wraps this in agentMessage envelope and sends via Double Ratchet.
 *
 * @param contact_idx  Contact slot index (must be in ROT_QUEUE_CREATED state)
 * @param buf          Output buffer for QADD binary payload
 * @param buf_size     Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int rotation_build_qadd_payload(int contact_idx, uint8_t *buf, int buf_size);

/**
 * Handle incoming QKEY message from contact.
 * Extracts sender key and advances state to ROT_QKEY_RECEIVED.
 *
 * @param contact_idx  Contact slot index
 * @param qkey_data    Raw QKEY payload (after "QK" tag)
 * @param qkey_len     Length of payload
 * @return true if QKEY processed successfully
 */
bool rotation_handle_qkey(int contact_idx, const uint8_t *qkey_data, int qkey_len);

/**
 * Complete queue creation after NEW response from target server.
 * Fills in server-assigned IDs and computes shared secret.
 * State advances to ROT_QUEUE_CREATED only when BOTH main + RQ are done.
 *
 * @param contact_idx    Contact slot index
 * @param rcv_id         Receiver ID from IDS response
 * @param rcv_id_len     Length of rcv_id
 * @param snd_id         Sender ID from IDS response
 * @param snd_id_len     Length of snd_id
 * @param srv_dh_public  Server DH public key (32 bytes, raw, no SPKI)
 * @return true on success
 */
bool rotation_complete_queue_creation(int contact_idx,
                                       const uint8_t *rcv_id, uint8_t rcv_id_len,
                                       const uint8_t *snd_id, uint8_t snd_id_len,
                                       const uint8_t *srv_dh_public);

/**
 * Complete Reply Queue creation after NEW response from target server.
 * Same as rotation_complete_queue_creation but for the per-contact RQ.
 * State advances to ROT_QUEUE_CREATED only when BOTH main + RQ are done.
 */
bool rotation_complete_rq_creation(int contact_idx,
                                    const uint8_t *rcv_id, uint8_t rcv_id_len,
                                    const uint8_t *snd_id, uint8_t snd_id_len,
                                    const uint8_t *srv_dh_public);

/**
 * Mark QADD as sent for a contact. Advances state to ROT_QADD_SENT.
 * Call after successfully sending the QADD message via Double Ratchet.
 */
void rotation_mark_qadd_sent(int contact_idx);

/**
 * Build QUSE agent message payload (the aMessage portion).
 * QUSE = "QU" (just 2 bytes, no additional data).
 * Tells the contact to switch to the new queue.
 *
 * @param contact_idx  Contact slot index
 * @param buf          Output buffer
 * @param buf_size     Size of output buffer
 * @return Number of bytes written (always 2), or -1 on error
 */
int rotation_build_quse_payload(int contact_idx, uint8_t *buf, int buf_size);

/**
 * Mark KEY as sent on new queue. Advances state to ROT_KEY_SENT.
 */
void rotation_mark_key_sent(int contact_idx);

/**
 * Mark QUSE as sent. Advances state to ROT_QUSE_SENT.
 * Waits for QTEST from App before contact is truly DONE.
 */
void rotation_mark_quse_sent(int contact_idx);

/**
 * Mark QTEST as received from the App.
 * Advances state to ROT_DONE and updates global counters.
 * Call when smp_agent.c receives a "QT" message from the contact.
 */
void rotation_mark_qtest_received(int contact_idx);

/**
 * Complete rotation: migrate credentials in RAM + NVS for DONE contacts.
 * Updates our_queue.server_host to new server. Does NOT reboot.
 * Caller must trigger reconnect to new server after this returns.
 * If all contacts DONE: clears rotation state (rotation_is_active -> false).
 * If nachzuegler pending: keeps rotation active for offline contacts.
 */
void rotation_complete(void);

/**
 * Get target server hostname (for UI display).
 * Returns NULL if no rotation active.
 */
const char *rotation_get_target_host(void);

/**
 * Check if there are pending (not-DONE) contacts from a rotation.
 */
bool rotation_has_pending(void);

/**
 * Check if Reply Queue subscriptions need to be restored after rotation.
 * Returns true once after rotation completes, then resets to false.
 * Caller should subscribe RQs when this returns true.
 */
bool rotation_rq_subs_needed(void);

/**
 * Get the CQ E2E peer public key extracted from QKEY dhPublicKey.
 * Returns true if key was stored (copies 32 bytes to out_key).
 * Survives rotation cleanup - stored in persistent static.
 */
bool rotation_get_cq_peer_e2e(uint8_t *out_key);

/**
 * Abort rotation. Cleans up state, keeps old server active.
 */
void rotation_abort(void);

/**
 * Get human-readable state name (for logging/UI).
 */
const char *rotation_state_name(rotation_contact_state_t state);

#endif // SMP_ROTATION_H
