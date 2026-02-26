/**
 * SimpleGo - Reply Queue E2E Decryption
 * Extracted from main.c (Auftrag 46c)
 * Extended: Session 34 Phase 6 - per-contact reply queue support
 *
 * Handles the full Reply Queue message decrypt pipeline:
 * Server-level decrypt -> Envelope parse -> E2E decrypt
 *
 * Two variants:
 * - smp_e2e_decrypt_reply_message()     = original, uses global our_queue keys
 * - smp_e2e_decrypt_reply_message_ex()  = new, takes explicit per-contact keys
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Decrypt a Reply Queue message using GLOBAL keys (legacy).
 * Uses our_queue.shared_secret, our_queue.e2e_private, reply_queue_e2e_peer_public.
 * Kept for backward compatibility (Contact 0 during transition).
 *
 * @param encrypted      Server-encrypted data (from MSG command)
 * @param encrypted_len  Length of encrypted data
 * @param msg_id         SMP message ID (used as server nonce)
 * @param msg_id_len     Length of msg_id
 * @param out_plain      OUT: allocated plaintext buffer (caller must free!)
 * @param out_plain_len  OUT: length of decrypted data (includes 2-byte unPad prefix)
 * @return 0 on success, negative on error:
 *         -1 = malloc failed
 *         -2 = server decrypt failed
 *         -3 = no sender key found
 *         -4 = message too short
 *         -5 = E2E decrypt failed (all methods)
 */
int smp_e2e_decrypt_reply_message(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    uint8_t **out_plain, size_t *out_plain_len);

/**
 * Decrypt a Reply Queue message using EXPLICIT per-contact keys.
 * For per-contact reply queue architecture (Session 34 Phase 6).
 *
 * @param encrypted        Server-encrypted data (from MSG command)
 * @param encrypted_len    Length of encrypted data
 * @param msg_id           SMP message ID (used as server nonce)
 * @param msg_id_len       Length of msg_id
 * @param shared_secret    32-byte server-level shared secret (rcv_dh_priv * srv_dh_pub)
 * @param e2e_private      32-byte X25519 private key for E2E layer
 * @param e2e_peer_public  32-byte peer's X25519 public key (NULL if not yet known)
 * @param e2e_peer_valid   true if e2e_peer_public contains a valid key
 * @param out_peer_public  OUT: if peer key extracted from envelope, written here (32B).
 *                         Caller uses this to update reply_queue_t.e2e_peer_public.
 *                         Pass NULL if not needed.
 * @param out_peer_found   OUT: true if new peer key was extracted. Pass NULL if not needed.
 * @param out_plain        OUT: allocated plaintext buffer (caller must free!)
 * @param out_plain_len    OUT: length of decrypted data
 * @return 0 on success, negative on error (same codes as above)
 */
int smp_e2e_decrypt_reply_message_ex(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    const uint8_t *shared_secret,
    const uint8_t *e2e_private,
    const uint8_t *e2e_peer_public,
    bool e2e_peer_valid,
    uint8_t *out_peer_public,
    bool *out_peer_found,
    uint8_t **out_plain, size_t *out_plain_len);
