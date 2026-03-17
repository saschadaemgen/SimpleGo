/**
 * SimpleGo - smp_servers.h
 * Multi-server management with preset lists and SEC-07 fingerprint verification
 *
 * Server list with three operator categories:
 *   - SimpleX Chat (Storage + Proxy)
 *   - Flux (Proxy only, no Storage)
 *   - SimpleGo (Storage + Proxy, default)
 *   - Custom (user-added)
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_SERVERS_H
#define SMP_SERVERS_H

#include <stdint.h>
#include <stdbool.h>

// ============== Constants ==============

#define MAX_SMP_SERVERS     32

// Role bitmask: what a server can be used for
#define SMP_SERVER_ROLE_STORAGE  0x01   // Can store messages (queue host)
#define SMP_SERVER_ROLE_PROXY    0x02   // Can relay/proxy messages
#define SMP_SERVER_ROLE_ALL      0x03   // Storage + Proxy

// Operator IDs
#define SMP_OP_SIMPLEX   0   // SimpleX Chat Ltd
#define SMP_OP_FLUX      1   // InFlux Technologies Limited
#define SMP_OP_SIMPLEGO  2   // IT and More Systems
#define SMP_OP_CUSTOM    3   // User-added

// ============== Server Structure ==============

typedef struct {
    char host[64];              // e.g. "smp8.simplex.im"
    uint16_t port;              // Default 5223
    uint8_t key_hash[32];       // SHA256 of TLS certificate (for SEC-07)
    uint8_t op;                 // SMP_OP_SIMPLEX / FLUX / SIMPLEGO / CUSTOM
    uint8_t roles;              // Bitmask: STORAGE, PROXY
    uint8_t enabled;            // 1 = active, 0 = disabled
    uint8_t preset;             // 1 = not deletable (factory preset)
} smp_server_t;

// ============== API ==============

void smp_servers_init(void);
int smp_servers_count(void);
int smp_servers_count_enabled(uint8_t role_mask);
smp_server_t *smp_servers_get(int index);
smp_server_t *smp_servers_pick_storage(void);
smp_server_t *smp_servers_find_by_host(const char *host);
int smp_servers_add_custom(const char *host, uint16_t port, const uint8_t *key_hash);
void smp_servers_set_enabled(int index, bool enabled);
void smp_servers_set_roles(int index, uint8_t roles);
void smp_servers_save(void);

#endif // SMP_SERVERS_H
