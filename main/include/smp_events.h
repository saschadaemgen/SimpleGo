/**
 * SimpleGo - smp_events.h
 * Event types and inter-task command protocol
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef SMP_EVENTS_H
#define SMP_EVENTS_H

#include <stdint.h>
#include <stddef.h>

// Event types for task communication
typedef enum {
    SMP_EVENT_FRAME_RECEIVED,   // Network -> App: raw frame from server
    SMP_EVENT_FRAME_SEND,       // App -> Network: frame to send to server
    SMP_EVENT_MSG_DECODED,      // App -> UI: decrypted message ready
    SMP_EVENT_ACK_READY,        // App -> Network: ACK/receipt to send
    SMP_EVENT_RECONNECT,        // Any -> Network: trigger reconnection
    SMP_EVENT_SHUTDOWN          // Any -> All: graceful shutdown
} smp_event_type_t;

// Frame source identifier
typedef enum {
    SMP_SOURCE_NETWORK,
    SMP_SOURCE_APP,
    SMP_SOURCE_UI
} smp_source_t;

// Frame container (4KB data buffer)
typedef struct {
    uint8_t data[4096];
    size_t len;
    smp_source_t source;
} smp_frame_t;

// Event container for inter-task queues
typedef struct {
    smp_event_type_t event_type;
    void *payload;
    size_t payload_len;
    uint32_t timestamp;
} smp_event_t;

// === Network Task Command Protocol (App -> Network via Ring Buffer) ===
// Phase 3 T4: Commands sent from App Task to Network Task

typedef enum {
    NET_CMD_SEND_ACK,          // Send ACK for received message
    NET_CMD_SUBSCRIBE_ALL,     // Re-subscribe all contacts after handshake
    NET_CMD_ADD_CONTACT,       // Create new contact queue on server
    NET_CMD_SEND_KEY,          // Session 34: Send KEY on per-contact reply queue
} net_cmd_type_t;

typedef struct {
    net_cmd_type_t cmd;
    // For NET_CMD_SEND_ACK:
    uint8_t recipient_id[24];
    int recipient_id_len;
    uint8_t msg_id[24];
    int msg_id_len;
    uint8_t rcv_auth_secret[64];
    // For NET_CMD_ADD_CONTACT:
    char contact_name[32];
    // For NET_CMD_SEND_KEY (Session 34 Phase 6):
    int rq_slot;                    // Reply queue contact slot
    uint8_t peer_auth_key[44];      // Peer's Ed25519 SPKI (from PHConfirmation 'K')
    int peer_auth_key_len;          // Must be 44
    // NET_CMD_SUBSCRIBE_ALL needs no extra data
} net_cmd_t;

#endif // SMP_EVENTS_H
