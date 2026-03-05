/**
 * SimpleGo - smp_ack.h
 * SMP ACK command for all queue types
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
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
