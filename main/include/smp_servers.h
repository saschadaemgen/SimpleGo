/**
 * SimpleGo - smp_servers.h
 * Multi-server management with single active server selection
 *
 * Three operator categories (visual grouping only):
 *   - SimpleGo (default active)
 *   - SimpleX Chat
 *   - Flux
 *   - Custom (user-added)
 *
 * One server active at a time (radio-button model).
 * Server switch takes effect on next reboot until Queue Rotation
 * is implemented.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_SERVERS_H
#define SMP_SERVERS_H

#include <stdint.h>
#include <stdbool.h>

// ============== Constants ==============

#define MAX_SMP_SERVERS     32
#define MAX_SMP_OPERATORS   4

// Server capability bitmask (what a server CAN do)
#define SMP_SERVER_ROLE_STORAGE  0x01
#define SMP_SERVER_ROLE_PROXY    0x02
#define SMP_SERVER_ROLE_ALL      0x03

// Operator IDs (visual grouping for UI)
#define SMP_OP_SIMPLEX   0   // SimpleX Chat Ltd
#define SMP_OP_FLUX      1   // InFlux Technologies Limited
#define SMP_OP_SIMPLEGO  2   // IT and More Systems
#define SMP_OP_CUSTOM    3   // User-added

// ============== Operator Structure ==============

typedef struct {
    uint8_t id;             // SMP_OP_SIMPLEX / FLUX / SIMPLEGO / CUSTOM
    char name[16];          // "SimpleX", "Flux", "SimpleGo", "Custom"
    uint8_t enabled;        // Visual grouping: 1 = show in UI
} smp_operator_t;

// ============== Server Structure ==============

typedef struct {
    char host[64];              // e.g. "smp8.simplex.im"
    uint16_t port;              // Default 5223
    uint8_t key_hash[32];       // SHA256 of TLS certificate (for SEC-07)
    uint8_t op;                 // SMP_OP_SIMPLEX / FLUX / SIMPLEGO / CUSTOM
    uint8_t roles;              // Capability bitmask: STORAGE, PROXY
    uint8_t enabled;            // 1 = visible in server list
    uint8_t preset;             // 1 = not deletable (factory preset)
} smp_server_t;

// ============== Server API ==============

void smp_servers_init(void);
int smp_servers_count(void);
smp_server_t *smp_servers_get(int index);
smp_server_t *smp_servers_find_by_host(const char *host);
int smp_servers_add_custom(const char *host, uint16_t port, const uint8_t *key_hash);
void smp_servers_save(void);

// Active server (radio-button model: exactly one at a time)
smp_server_t *smp_servers_get_active(void);
int smp_servers_get_active_index(void);
void smp_servers_set_active(int index);

// Per-operator server counts (for UI)
int smp_servers_count_for_operator(uint8_t op_id);

// ============== Operator API ==============

int smp_operators_count(void);
smp_operator_t *smp_operators_get(uint8_t op_id);
const char *smp_operators_get_name(uint8_t op_id);

#endif // SMP_SERVERS_H
