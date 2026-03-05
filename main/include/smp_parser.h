/**
 * SimpleGo - smp_parser.h
 * Agent message parser interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_PARSER_H
#define SMP_PARSER_H

#include <stdbool.h>
#include "smp_types.h"

// Parse agent message (after SMP decryption)
void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len);

// Message type constants
#define AGENT_MSG_CONFIRMATION  'C'
#define AGENT_MSG_INVITATION    'I'
#define AGENT_MSG_ENVELOPE      'M'
#define AGENT_MSG_RATCHET_KEY   'R'

/**
 * Parse AgentConfirmation and extract Reply Queue E2E key
 * Called after per-queue E2E decrypt on Contact Queue
 * 
 * @param cm_plain      Decrypted ClientMessage
 * @param cm_plain_len  Length
 * @return 0 on success, -1 on error
 */
int parse_agent_confirmation(const uint8_t *cm_plain, int cm_plain_len);

#endif // SMP_PARSER_H
