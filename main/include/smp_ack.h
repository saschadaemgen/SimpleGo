/**
 * SimpleGo - SMP ACK Command
 * Consolidated ACK sending for all queue types
 *
 * Replaces 3 near-identical ACK implementations:
 * - Contact Queue ACK (main receive loop)
 * - Reply Queue ACK (42d handler)
 * - Main Loop ACK (45n PHEmpty handler)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mbedtls/ssl.h"

/**
 * Send ACK for a received message on any queue.
 *
 * Constructs the signed SMP ACK command:
 *   Transport: [sigLen][signature][sessLen][sessionId][body]
 *   Body:      [version=1]['A'][rcvIdLen][rcvId]["ACK "][msgIdLen][msgId]
 *   Signature: Ed25519 over [sessLen][sessionId][body]
 *
 * @param ssl           Active TLS connection to SMP server
 * @param block         Work buffer (SMP_BLOCK_SIZE)
 * @param session_id    32-byte TLS session ID
 * @param recipient_id  Queue recipient ID
 * @param recipient_id_len  Length of recipient_id
 * @param msg_id        Message ID to acknowledge
 * @param msg_id_len    Length of msg_id
 * @param rcv_auth_secret  64-byte Ed25519 secret key for signing
 * @return true on success
 */
bool smp_send_ack(mbedtls_ssl_context *ssl, uint8_t *block,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret);
