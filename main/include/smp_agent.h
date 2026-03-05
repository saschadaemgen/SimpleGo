/**
 * SimpleGo - smp_agent.h
 * SimpleX agent protocol layer interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "smp_contacts.h"

// Agent message tags
#define AGENT_TAG_CONFIRMATION 'K'  // PHConfirmation (with auth key)
#define AGENT_TAG_EMPTY        '_'  // PHEmpty (HELLO, A_MSG, etc.)
#define AGENT_TAG_HELLO        'H'  // A_HELLO
#define AGENT_TAG_MSG          'M'  // A_MSG (chat message)
#define AGENT_TAG_INFO         'I'  // ConnInfo (Zstd compressed)
#define AGENT_TAG_REPLY        'D'  // AgentConnInfoReply

/**
 * Process a decrypted E2E plaintext (includes 2-byte unPad prefix).
 *
 * Dispatches to appropriate handler based on PrivHeader tag:
 * - 'K' (PHConfirmation): Extracts sender auth key, parses AgentConfirmation,
 *   performs ratchet header/body decrypt, handles ConnInfo with Zstd.
 * - '_' (PHEmpty): Detects A_HELLO, ratchet-decrypts A_MSG chat messages.
 *
 * @param plaintext              E2E decrypted data (with 2-byte unPad length prefix)
 * @param plaintext_len          Length of plaintext (cipher_len - MAC)
 * @param contact                Associated contact (may be NULL for Reply Queue)
 * @param peer_sender_auth_key   OUT: 44-byte auth key (filled for 'K' tag)
 * @param has_peer_sender_auth   OUT: set true when auth key was extracted
 * @return true if message was handled successfully
 */
bool smp_agent_process_message(
    const uint8_t *plaintext, size_t plaintext_len,
    contact_t *contact,
    uint8_t *peer_sender_auth_key,
    bool *has_peer_sender_auth
);
