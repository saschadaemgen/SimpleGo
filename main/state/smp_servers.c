/**
 * SimpleGo - smp_servers.c
 * Multi-server management with operator-level role control
 *
 * 21 preset servers from SimpleX Chat Presets.hs:
 *   - 14 SimpleX Chat (Storage + Proxy, 4 randomly enabled on first boot)
 *   - 6 Flux (Storage + Proxy, 3 randomly enabled on first boot)
 *   - 1 SimpleGo (Storage + Proxy, always enabled, default)
 *
 * Roles controlled at operator level (matching SimpleX App behavior):
 *   - Operator "enabled" = master toggle
 *   - Operator "use_for_recv" = allow Storage ("To receive")
 *   - Operator "use_for_proxy" = allow Proxy ("For private routing")
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_servers.h"
#include "smp_utils.h"      // base64url_decode()
#include "smp_storage.h"    // NVS read/write
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "SMP_SRV";

// ============== Preset Definition ==============

typedef struct {
    const char *host;
    const char *fingerprint_b64;
    uint8_t op;
    uint8_t roles;
} server_preset_t;

// Source: simplex-chat/src/Simplex/Chat/Operators/Presets.hs
static const server_preset_t PRESETS[] = {
    // ---- SimpleX Chat (Operator 0) - Storage + Proxy ----
    // enabledSimplexChatSMPServers (11 servers)
    {"smp8.simplex.im",  "0YuTwO05YJWS8rkjn9eLJDjQhFKvIYd8d4xG8X1blIU=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp9.simplex.im",  "SkIkI6EPd2D63F4xFKfHk7I1UGZVNn6k1QWZ5rcyr6w=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp10.simplex.im", "6iIcWT_dF2zN_w5xzZEY7HI2Prbh3ldP07YTyDexPjE=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp11.simplex.im", "1OwYGt-yqOfe2IyVHhxz3ohqo3aCCMjtB-8wn4X_aoY=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp12.simplex.im", "UkMFNAXLXeAAe0beCa4w6X_zp18PwxSaSjY17BKUGXQ=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp14.simplex.im", "enEkec4hlR3UtKx2NMpOUK_K4ZuDxjWBO1d9Y4YXVaA=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp15.simplex.im", "h--vW7ZSkXPeOUpfxlFGgauQmXNFOzGoizak7Ult7cw=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp16.simplex.im", "hejn2gVIqNU6xjtGM3OwQeuk8ZEbDXVJXAlnSBJBWUA=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp17.simplex.im", "ZKe4uxF4Z_aLJJOEsC-Y6hSkXgQS5-oc442JQGkyP8M=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp18.simplex.im", "PtsqghzQKU83kYTlQ1VKg996dW4Cw4x_bvpKmiv8uns=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp19.simplex.im", "N_McQS3F9TGoh4ER0QstUf55kGnNSd-wXfNPZ7HukcM=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    // disabledSimplexChatSMPServers (3 servers)
    {"smp4.simplex.im",  "u2dS9sG8nMNURyZwqASV4yROM28Er0luVTx5X1CsMrU=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp5.simplex.im",  "hpq7_4gGJiilmz5Rf-CswuU5kZGkm_zOIooSw6yALRg=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},
    {"smp6.simplex.im",  "PQUV2eL0t7OStZOoAsPEV2QYWt4-xilbakvGUGOItUo=", SMP_OP_SIMPLEX, SMP_SERVER_ROLE_ALL},

    // ---- Flux (Operator 1) - Storage + Proxy ----
    // Verified in SimpleX App: Flux has "To receive" AND "For private routing"
    {"smp1.simplexonflux.com", "xQW_ufMkGE20UrTlBl8QqceG1tbuylXhr9VOLPyRJmw=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp2.simplexonflux.com", "LDnWZVlAUInmjmdpQQoIo6FUinRXGe0q3zi5okXDE4s=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp3.simplexonflux.com", "1jne379u7IDJSxAvXbWb_JgoE7iabcslX0LBF22Rej0=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp4.simplexonflux.com", "xmAmqj75I9mWrUihLUlI0ZuNLXlIwFIlHRq5Pb6cHAU=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp5.simplexonflux.com", "rWvBYyTamuRCBYb_KAn-nsejg879ndhiTg5Sq3k0xWA=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp6.simplexonflux.com", "PN7-uqLBToqlf1NxHEaiL35lV2vBpXq8Nj8BW11bU48=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},

    // ---- SimpleGo (Operator 2) - Storage + Proxy, DEFAULT ----
    {"smp.simplego.dev", "XfTKGkd9rBkebeTnXMKSnLMAh82tHjNubpJRylz7KXg=", SMP_OP_SIMPLEGO, SMP_SERVER_ROLE_ALL},
};

#define PRESET_COUNT  (sizeof(PRESETS) / sizeof(PRESETS[0]))

// ============== Runtime State ==============

static smp_server_t s_servers[MAX_SMP_SERVERS];
static int s_server_count = 0;

static smp_operator_t s_operators[MAX_SMP_OPERATORS] = {
    {SMP_OP_SIMPLEX,  "SimpleX",  1, 1, 1},
    {SMP_OP_FLUX,     "Flux",     1, 1, 1},
    {SMP_OP_SIMPLEGO, "SimpleGo", 1, 1, 1},
    {SMP_OP_CUSTOM,   "Custom",   1, 1, 1},
};

static bool s_initialized = false;
static uint8_t s_last_pick_op = 0xFF;

// ============== NVS Keys ==============

#define SRV_NVS_KEY      "srv_list"
#define OPS_NVS_KEY      "srv_ops"
#define SRV_NVS_VERSION  2   // Bumped: Flux fix (PROXY -> ALL)

// ============== Internal: Decode presets ==============

static bool decode_preset(const server_preset_t *preset, smp_server_t *out) {
    memset(out, 0, sizeof(smp_server_t));
    strncpy(out->host, preset->host, sizeof(out->host) - 1);
    out->port = 5223;
    out->op = preset->op;
    out->roles = preset->roles;
    out->preset = 1;
    out->enabled = 0;

    int dec_len = base64url_decode(preset->fingerprint_b64,
                                   out->key_hash, sizeof(out->key_hash));
    if (dec_len != 32) {
        ESP_LOGE(TAG, "Fingerprint decode failed for %s: got %d bytes",
                 preset->host, dec_len);
        return false;
    }
    return true;
}

// ============== Internal: Random subset selection ==============

static void random_enable_subset(int start, int total, int want) {
    if (want >= total) {
        for (int i = start; i < start + total; i++) {
            s_servers[i].enabled = 1;
        }
        return;
    }

    int indices[MAX_SMP_SERVERS];
    for (int i = 0; i < total; i++) {
        indices[i] = start + i;
    }

    for (int i = 0; i < want; i++) {
        int remaining = total - i;
        int pick = (int)(esp_random() % (uint32_t)remaining);
        int idx = indices[i + pick];
        indices[i + pick] = indices[i];
        indices[i] = idx;
        s_servers[idx].enabled = 1;
    }
}

// ============== Internal: NVS Load/Save ==============

static bool load_servers_from_nvs(void) {
    if (!smp_storage_exists(SRV_NVS_KEY)) return false;

    // Read header to check version
    uint8_t header[4];
    size_t hdr_len = 0;
    esp_err_t ret = smp_storage_load_blob(SRV_NVS_KEY, header, 4, &hdr_len);
    if (ret != ESP_OK || hdr_len < 4) return false;

    if (header[0] != SRV_NVS_VERSION) {
        ESP_LOGW(TAG, "NVS version mismatch: got %d, expected %d - re-init",
                 header[0], SRV_NVS_VERSION);
        return false;
    }

    uint8_t count = header[1];
    if (count > MAX_SMP_SERVERS) return false;

    size_t total_size = 4 + sizeof(smp_server_t) * (size_t)count;
    uint8_t *buf = malloc(total_size);
    if (!buf) return false;

    size_t loaded_len = 0;
    ret = smp_storage_load_blob(SRV_NVS_KEY, buf, total_size, &loaded_len);
    if (ret != ESP_OK) {
        free(buf);
        return false;
    }

    memcpy(s_servers, buf + 4, sizeof(smp_server_t) * count);
    s_server_count = count;
    free(buf);

    ESP_LOGI(TAG, "Loaded %d servers from NVS", s_server_count);
    return true;
}

static void save_servers_to_nvs(void) {
    size_t save_size = 4 + sizeof(smp_server_t) * (size_t)s_server_count;
    uint8_t *buf = malloc(save_size);
    if (!buf) {
        ESP_LOGE(TAG, "NVS save: malloc failed");
        return;
    }

    buf[0] = SRV_NVS_VERSION;
    buf[1] = (uint8_t)s_server_count;
    buf[2] = 0;
    buf[3] = 0;
    memcpy(buf + 4, s_servers, sizeof(smp_server_t) * (size_t)s_server_count);

    esp_err_t ret = smp_storage_save_blob_sync(SRV_NVS_KEY, buf, save_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %s (size=%zu)", esp_err_to_name(ret), save_size);
    } else {
        ESP_LOGI(TAG, "Saved %d servers to NVS (%zu bytes)", s_server_count, save_size);
    }
    free(buf);
}

static bool load_operators_from_nvs(void) {
    if (!smp_storage_exists(OPS_NVS_KEY)) return false;

    size_t loaded_len = 0;
    esp_err_t ret = smp_storage_load_blob(OPS_NVS_KEY, s_operators,
                                           sizeof(s_operators), &loaded_len);
    if (ret != ESP_OK || loaded_len != sizeof(s_operators)) {
        ESP_LOGW(TAG, "Operator NVS load failed - using defaults");
        return false;
    }

    ESP_LOGI(TAG, "Loaded operators from NVS");
    return true;
}

static void save_operators_to_nvs(void) {
    esp_err_t ret = smp_storage_save_blob_sync(OPS_NVS_KEY, s_operators,
                                                sizeof(s_operators));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Operator NVS save failed: %s", esp_err_to_name(ret));
    }
}

// ============== Public API: Init ==============

void smp_servers_init(void) {
    if (s_initialized) return;

    // Load operators (separate from servers, survives server list re-init)
    load_operators_from_nvs();

    // Try loading servers from NVS
    if (load_servers_from_nvs()) {
        s_initialized = true;

        int en_storage = smp_servers_count_enabled(SMP_SERVER_ROLE_STORAGE);
        int en_proxy = smp_servers_count_enabled(SMP_SERVER_ROLE_PROXY);
        ESP_LOGI(TAG, "Server list: %d total, %d storage, %d proxy",
                 s_server_count, en_storage, en_proxy);
        return;
    }

    // First boot: decode presets and randomly enable subsets
    ESP_LOGI(TAG, "First boot: initializing %d preset servers", (int)PRESET_COUNT);

    s_server_count = 0;
    int sx_start = -1, sx_count = 0;
    int fx_start = -1, fx_count = 0;

    for (int i = 0; i < (int)PRESET_COUNT && s_server_count < MAX_SMP_SERVERS; i++) {
        if (!decode_preset(&PRESETS[i], &s_servers[s_server_count])) {
            ESP_LOGW(TAG, "Skipping preset %d (%s)", i, PRESETS[i].host);
            continue;
        }

        if (PRESETS[i].op == SMP_OP_SIMPLEX) {
            if (sx_start < 0) sx_start = s_server_count;
            sx_count++;
        } else if (PRESETS[i].op == SMP_OP_FLUX) {
            if (fx_start < 0) fx_start = s_server_count;
            fx_count++;
        } else if (PRESETS[i].op == SMP_OP_SIMPLEGO) {
            s_servers[s_server_count].enabled = 1;
        }

        s_server_count++;
    }

    if (sx_start >= 0 && sx_count > 0) {
        random_enable_subset(sx_start, sx_count, 4);
        ESP_LOGI(TAG, "SimpleX: enabled 4 of %d servers", sx_count);
    }
    if (fx_start >= 0 && fx_count > 0) {
        random_enable_subset(fx_start, fx_count, 3);
        ESP_LOGI(TAG, "Flux: enabled 3 of %d servers", fx_count);
    }

    save_servers_to_nvs();
    save_operators_to_nvs();

    s_initialized = true;

    // Log final state
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SMP Server List (%d servers) ===", s_server_count);
    for (int i = 0; i < s_server_count; i++) {
        const char *op_name = "???";
        switch (s_servers[i].op) {
            case SMP_OP_SIMPLEX:  op_name = "SimpleX"; break;
            case SMP_OP_FLUX:     op_name = "Flux"; break;
            case SMP_OP_SIMPLEGO: op_name = "SimpleGo"; break;
            case SMP_OP_CUSTOM:   op_name = "Custom"; break;
        }
        const char *role_str = "???";
        switch (s_servers[i].roles) {
            case SMP_SERVER_ROLE_STORAGE: role_str = "Storage"; break;
            case SMP_SERVER_ROLE_PROXY:   role_str = "Proxy"; break;
            case SMP_SERVER_ROLE_ALL:     role_str = "Storage+Proxy"; break;
        }
        ESP_LOGI(TAG, "  [%d] %s%-24s %s  %s  fp=%02x%02x...",
                 i,
                 s_servers[i].enabled ? "*" : " ",
                 s_servers[i].host,
                 op_name, role_str,
                 s_servers[i].key_hash[0], s_servers[i].key_hash[1]);
    }
    ESP_LOGI(TAG, "  (* = enabled)");

    // Log operator state
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Operators ===");
    for (int i = 0; i < MAX_SMP_OPERATORS; i++) {
        ESP_LOGI(TAG, "  %s: %s  recv=%d  proxy=%d",
                 s_operators[i].name,
                 s_operators[i].enabled ? "ON" : "OFF",
                 s_operators[i].use_for_recv,
                 s_operators[i].use_for_proxy);
    }
    ESP_LOGI(TAG, "");
}

// ============== Public API: Server Queries ==============

int smp_servers_count(void) {
    return s_server_count;
}

int smp_servers_count_enabled(uint8_t role_mask) {
    int count = 0;
    for (int i = 0; i < s_server_count; i++) {
        if (s_servers[i].enabled && (s_servers[i].roles & role_mask)) {
            smp_operator_t *op = smp_operators_get(s_servers[i].op);
            if (op && op->enabled) {
                if ((role_mask & SMP_SERVER_ROLE_STORAGE) && !op->use_for_recv) continue;
                if ((role_mask & SMP_SERVER_ROLE_PROXY) && !op->use_for_proxy) continue;
                count++;
            }
        }
    }
    return count;
}

smp_server_t *smp_servers_get(int index) {
    if (index < 0 || index >= s_server_count) return NULL;
    return &s_servers[index];
}

int smp_servers_count_for_operator(uint8_t op_id) {
    int count = 0;
    for (int i = 0; i < s_server_count; i++) {
        if (s_servers[i].op == op_id) count++;
    }
    return count;
}

int smp_servers_count_enabled_for_operator(uint8_t op_id) {
    int count = 0;
    for (int i = 0; i < s_server_count; i++) {
        if (s_servers[i].op == op_id && s_servers[i].enabled) count++;
    }
    return count;
}

// ============== Public API: Server Selection ==============

smp_server_t *smp_servers_pick_storage(void) {
    // Filter: server enabled + capable of storage +
    //         operator enabled + operator allows recv
    int candidates[MAX_SMP_SERVERS];
    int n_candidates = 0;

    for (int i = 0; i < s_server_count; i++) {
        if (!s_servers[i].enabled) continue;
        if (!(s_servers[i].roles & SMP_SERVER_ROLE_STORAGE)) continue;

        smp_operator_t *op = smp_operators_get(s_servers[i].op);
        if (!op || !op->enabled || !op->use_for_recv) continue;

        candidates[n_candidates++] = i;
    }

    if (n_candidates == 0) {
        ESP_LOGE(TAG, "No enabled Storage servers available!");
        return NULL;
    }

    // Prefer operators we haven't used recently
    int preferred[MAX_SMP_SERVERS];
    int n_preferred = 0;

    for (int i = 0; i < n_candidates; i++) {
        if (s_servers[candidates[i]].op != s_last_pick_op) {
            preferred[n_preferred++] = candidates[i];
        }
    }

    int *pool = n_preferred > 0 ? preferred : candidates;
    int pool_size = n_preferred > 0 ? n_preferred : n_candidates;

    int pick = pool[(int)(esp_random() % (uint32_t)pool_size)];
    s_last_pick_op = s_servers[pick].op;

    ESP_LOGI(TAG, "Picked storage server: %s (op=%d, pool=%d/%d)",
             s_servers[pick].host, s_servers[pick].op,
             pool_size, n_candidates);

    return &s_servers[pick];
}

smp_server_t *smp_servers_find_by_host(const char *host) {
    if (!host || !host[0]) return NULL;
    for (int i = 0; i < s_server_count; i++) {
        if (strcmp(s_servers[i].host, host) == 0) {
            return &s_servers[i];
        }
    }
    return NULL;
}

// ============== Public API: Server Modification ==============

int smp_servers_add_custom(const char *host, uint16_t port,
                           const uint8_t *key_hash) {
    if (s_server_count >= MAX_SMP_SERVERS) {
        ESP_LOGE(TAG, "Server list full (%d)", MAX_SMP_SERVERS);
        return -1;
    }

    smp_server_t *srv = &s_servers[s_server_count];
    memset(srv, 0, sizeof(smp_server_t));
    strncpy(srv->host, host, sizeof(srv->host) - 1);
    srv->port = port;
    memcpy(srv->key_hash, key_hash, 32);
    srv->op = SMP_OP_CUSTOM;
    srv->roles = SMP_SERVER_ROLE_ALL;
    srv->enabled = 1;
    srv->preset = 0;

    int idx = s_server_count;
    s_server_count++;
    save_servers_to_nvs();

    ESP_LOGI(TAG, "Added custom server [%d]: %s:%d", idx, host, port);
    return idx;
}

void smp_servers_set_enabled(int index, bool enabled) {
    if (index < 0 || index >= s_server_count) return;
    s_servers[index].enabled = enabled ? 1 : 0;
    save_servers_to_nvs();
    ESP_LOGI(TAG, "Server [%d] %s: %s",
             index, s_servers[index].host,
             enabled ? "enabled" : "disabled");
}

void smp_servers_set_roles(int index, uint8_t roles) {
    if (index < 0 || index >= s_server_count) return;
    s_servers[index].roles = roles;
    save_servers_to_nvs();
    ESP_LOGI(TAG, "Server [%d] %s: roles=0x%02x",
             index, s_servers[index].host, roles);
}

void smp_servers_save(void) {
    save_servers_to_nvs();
}

// ============== Public API: Operators ==============

int smp_operators_count(void) {
    return MAX_SMP_OPERATORS;
}

smp_operator_t *smp_operators_get(uint8_t op_id) {
    if (op_id >= MAX_SMP_OPERATORS) return NULL;
    return &s_operators[op_id];
}

void smp_operators_set_enabled(uint8_t op_id, bool enabled) {
    if (op_id >= MAX_SMP_OPERATORS) return;
    s_operators[op_id].enabled = enabled ? 1 : 0;
    save_operators_to_nvs();
    ESP_LOGI(TAG, "Operator %s: %s",
             s_operators[op_id].name,
             enabled ? "enabled" : "disabled");
}

void smp_operators_set_recv(uint8_t op_id, bool recv) {
    if (op_id >= MAX_SMP_OPERATORS) return;
    s_operators[op_id].use_for_recv = recv ? 1 : 0;
    save_operators_to_nvs();
    ESP_LOGI(TAG, "Operator %s: recv=%d",
             s_operators[op_id].name, s_operators[op_id].use_for_recv);
}

void smp_operators_set_proxy(uint8_t op_id, bool proxy) {
    if (op_id >= MAX_SMP_OPERATORS) return;
    s_operators[op_id].use_for_proxy = proxy ? 1 : 0;
    save_operators_to_nvs();
    ESP_LOGI(TAG, "Operator %s: proxy=%d",
             s_operators[op_id].name, s_operators[op_id].use_for_proxy);
}
