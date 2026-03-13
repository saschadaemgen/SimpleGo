/**
 * SimpleGo - smp_handshake.h
 * SMP connection handshake protocol interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_HANDSHAKE_H
#define SMP_HANDSHAKE_H

#include <stdint.h>
#include <stdbool.h>
#include "mbedtls/ssl.h"
#include "smp_ratchet.h"
#include "smp_types.h"

/**
 * Send SKEY command to secure peer's queue
 * (must be called BEFORE AgentConfirmation)
 */
bool send_skey_command(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *our_auth_public
);

/**
 * Send HELLO message to complete the connection handshake.
 *
 * @param ssl               TLS context for peer connection
 * @param block             Work buffer (SMP_BLOCK_SIZE)
 * @param session_id        Current session ID (32 bytes)
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer's queue ID
 * @param peer_dh_public    Peer's X25519 DH public key (32 bytes)
 * @param our_dh_private    Our X25519 DH private key (32 bytes)
 * @param our_dh_public     Our X25519 DH public key (32 bytes)
 * @param ratchet           Ratchet state for encryption
 * @param snd_auth_private  Ed25519 private key for signing SEND (64 bytes)
 * @return true on success
 */
bool send_hello_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private
);

/**
 * Send a chat message (A_MSG) to peer.
 * Auftrag 44a: First bidirectional chat message.
 *
 * Uses same encrypt chain as HELLO (Ratchet → E2E → SEND),
 * but with A_MSG payload instead of HELLO tag.
 *
 * @param ssl               TLS context for peer connection
 * @param block             Work buffer (SMP_BLOCK_SIZE)
 * @param session_id        Current session ID (32 bytes)
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer's queue ID
 * @param peer_dh_public    Peer's X25519 DH public key (32 bytes)
 * @param our_dh_private    Our X25519 DH private key (32 bytes)
 * @param our_dh_public     Our X25519 DH public key (32 bytes)
 * @param ratchet           Ratchet state for encryption
 * @param snd_auth_private  Ed25519 private key for signing SEND (64 bytes)
 * @param message           UTF-8 message text to send
 * @return true on success
 */
bool send_chat_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    const char *message
);

/**
 * Send a delivery receipt (A_RCVD) to peer.
 * Auftrag 49b: Enables double-check marks in SimpleX app.
 *
 * Uses same encrypt chain as A_MSG but with:
 *   - Receipt payload ('V' tag) instead of message ('M' tag)
 *   - Silent flag (no push notification)
 *   - CorrId = 'R'
 *
 * @param ssl               TLS context for peer connection
 * @param block             Work buffer (SMP_BLOCK_SIZE)
 * @param session_id        Current session ID (32 bytes)
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer's queue ID
 * @param peer_dh_public    Peer's X25519 DH public key (32 bytes)
 * @param our_dh_private    Our X25519 DH private key (32 bytes)
 * @param our_dh_public     Our X25519 DH public key (32 bytes)
 * @param ratchet           Ratchet state for encryption
 * @param snd_auth_private  Ed25519 private key for signing SEND (64 bytes)
 * @param peer_snd_msg_id   The sndMsgId from the received message
 * @param msg_hash          SHA256 hash of the decrypted body (32 bytes)
 * @return true on success
 */
bool send_receipt_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    uint64_t peer_snd_msg_id,
    const uint8_t *msg_hash
);

/**
 * Parse a decrypted message to check if it's HELLO.
 *
 * @param data       Decrypted message data
 * @param len        Length of data
 * @param msg_id_out Output: message ID (optional, can be NULL)
 * @return true if message is HELLO
 */
bool parse_hello_message(const uint8_t *data, int len, uint64_t *msg_id_out);

/**
 * Complete the duplex connection handshake after sending AgentConfirmation.
 *
 * Flow:
 * 1. (AgentConfirmation already sent)
 * 2. (Fast duplex: skip waiting for confirmation)
 * 3. Send HELLO message
 * 4. Wait for HELLO back (optional in fast mode)
 *
 * @param peer_ssl          TLS context for peer connection
 * @param block             Work buffer
 * @param peer_session_id   Peer's session ID
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer queue ID
 * @param peer_dh_public    Peer's DH public key
 * @param our_dh_private    Our DH private key
 * @param our_dh_public     Our DH public key
 * @param ratchet           Ratchet state
 * @param snd_auth_private  Ed25519 private key for signing SEND (64 bytes)
 * @return true if handshake initiated successfully
 */
bool complete_handshake(
    mbedtls_ssl_context *peer_ssl,
    uint8_t *block,
    const uint8_t *peer_session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private
);

// Status getters
bool is_handshake_complete(void);
bool is_hello_sent(void);
bool is_hello_received(void);
bool is_connected(void);
void reset_handshake_state(void);

// Session 33: Multi-contact handshake support
bool handshake_multi_init(void);
void handshake_set_active(uint8_t idx);

// ============== Persistence (Auftrag 50d) ==============

/**
 * Save handshake send-state (msg_id + prev_msg_hash) to NVS.
 * NVS key: "hand_00" (contact 0, expandable later).
 * Must be called after every successful SEND (Evgeny's Rule).
 *
 * @return true on success
 */
bool handshake_save_state(void);

/**
 * Load handshake send-state from NVS.
 * Restores msg_id and prev_msg_hash so next send has correct sequence.
 *
 * @return true on success
 */
bool handshake_load_state(void);

/**
 * Get the msg_id used in the most recent send operation.
 * Used by delivery status tracking to map UI seq -> protocol msg_id.
 *
 * @return Last used msg_id (post-increment value from build_chat_message)
 */
uint64_t handshake_get_last_msg_id(void);

// ============== Retry After Write Failure (Bug #20) ==============

/**
 * Retry sending the last failed message after reconnect.
 * Re-signs with new session_id - no re-encryption, no ratchet mutation.
 *
 * @param ssl            New SSL context (after reconnect)
 * @param block          SMP block buffer (SMP_BLOCK_SIZE)
 * @param new_session_id New 32-byte session ID from reconnected peer
 * @return true if retry succeeded
 */
bool handshake_retry_send(mbedtls_ssl_context *ssl, uint8_t *block,
                          const uint8_t *new_session_id);

/**
 * Check if a retry buffer is pending (write failed, waiting for reconnect).
 */
bool handshake_has_retry_pending(void);

/**
 * Clear the retry buffer and securely zero all sensitive data.
 */
void handshake_clear_retry(void);

#endif // SMP_HANDSHAKE_H