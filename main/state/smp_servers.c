/**
 * SimpleGo - smp_servers.c
 * Multi-server management with single active server selection
 *
 * 21 preset servers from SimpleX Chat Presets.hs:
 *   - 14 SimpleX Chat (Storage + Proxy)
 *   - 6 Flux (Storage + Proxy)
 *   - 1 SimpleGo (Storage + Proxy, default active)
 *
 * Single active server model (radio-button):
 *   - Exactly one server active at a time
 *   - Server switch stored in NVS, takes effect on next reboot
 *   - Queue Rotation (live migration) planned for future
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_servers.h"
#include "smp_utils.h"      // base64url_decode()
#include "smp_storage.h"    // NVS read/write
#include <string.h>
#include <stdlib.h>         // malloc, free
#include "esp_log.h"

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
    {"smp1.simplexonflux.com", "xQW_ufMkGE20UrTlBl8QqceG1tbuylXhr9VOLPyRJmw=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp2.simplexonflux.com", "LDnWZVlAUInmjmdpQQoIo6FUinRXGe0q3zi5okXDE4s=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp3.simplexonflux.com", "1jne379u7IDJSxAvXbWb_JgoE7iabcslX0LBF22Rej0=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp4.simplexonflux.com", "xmAmqj75I9mWrUihLUlI0ZuNLXlIwFIlHRq5Pb6cHAU=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp5.simplexonflux.com", "rWvBYyTamuRCBYb_KAn-nsejg879ndhiTg5Sq3k0xWA=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},
    {"smp6.simplexonflux.com", "PN7-uqLBToqlf1NxHEaiL35lV2vBpXq8Nj8BW11bU48=", SMP_OP_FLUX, SMP_SERVER_ROLE_ALL},

    // ---- SimpleGo (Operator 2) - Storage + Proxy, DEFAULT ----
    {"smp.simplego.dev", "7qw4hvuS+PvTHbotgtg/xiwrhFUk/s1q2upUQrGIWow=", SMP_OP_SIMPLEGO, SMP_SERVER_ROLE_ALL},
};

#define PRESET_COUNT  (sizeof(PRESETS) / sizeof(PRESETS[0]))

// SimpleGo is the last preset, index 20
#define DEFAULT_ACTIVE_INDEX  ((int)PRESET_COUNT - 1)

// ============== Runtime State ==============

static smp_server_t s_servers[MAX_SMP_SERVERS];
static int s_server_count = 0;
static int s_active_index = DEFAULT_ACTIVE_INDEX;

static smp_operator_t s_operators[MAX_SMP_OPERATORS] = {
    {SMP_OP_SIMPLEX,  "SimpleX",  1},
    {SMP_OP_FLUX,     "Flux",     1},
    {SMP_OP_SIMPLEGO, "SimpleGo", 1},
    {SMP_OP_CUSTOM,   "Custom",   1},
};

static bool s_initialized = false;

// ============== NVS Keys ==============

#define SRV_NVS_KEY      "srv_list"
#define OPS_NVS_KEY      "srv_ops"
#define ACTIVE_NVS_KEY   "active_srv"
#define SRV_NVS_VERSION  3   // V3: server struct unchanged

// ============== Internal: Decode presets ==============

static bool decode_preset(const server_preset_t *preset, smp_server_t *out) {
    memset(out, 0, sizeof(smp_server_t));
    strncpy(out->host, preset->host, sizeof(out->host) - 1);
    out->port = 5223;
    out->op = preset->op;
    out->roles = preset->roles;
    out->preset = 1;
    out->enabled = 1;   // All servers visible in list

    int dec_len = base64url_decode(preset->fingerprint_b64,
                                   out->key_hash, sizeof(out->key_hash));
    if (dec_len != 32) {
        ESP_LOGE(TAG, "Fingerprint decode failed for %s: got %d bytes",
                 preset->host, dec_len);
        return false;
    }
    return true;
}

// ============== Internal: NVS Load/Save ==============

static bool load_servers_from_nvs(void) {
    if (!smp_storage_exists(SRV_NVS_KEY)) return false;

    /* Allocate max possible size and read entire blob at once.
     * smp_storage_load_blob rejects partial reads (buffer < blob). */
    size_t max_size = 4 + sizeof(smp_server_t) * MAX_SMP_SERVERS;
    uint8_t *buf = malloc(max_size);
    if (!buf) return false;

    size_t loaded_len = 0;
    esp_err_t ret = smp_storage_load_blob(SRV_NVS_KEY, buf, max_size, &loaded_len);
    if (ret != ESP_OK || loaded_len < 4) {
        free(buf);
        return false;
    }

    /* Check version */
    if (buf[0] != SRV_NVS_VERSION) {
        ESP_LOGW(TAG, "NVS version mismatch: got %d, expected %d - re-init",
                 buf[0], SRV_NVS_VERSION);
        free(buf);
        return false;
    }

    uint8_t count = buf[1];
    if (count > MAX_SMP_SERVERS) {
        free(buf);
        return false;
    }

    size_t expected = 4 + sizeof(smp_server_t) * (size_t)count;
    if (loaded_len < expected) {
        ESP_LOGW(TAG, "NVS data too short: %zu < %zu", loaded_len, expected);
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
        ESP_LOGW(TAG, "Operator NVS load failed (size mismatch) - using defaults");
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

static void load_active_index(void) {
    uint8_t idx = (uint8_t)DEFAULT_ACTIVE_INDEX;
    size_t out_len = 0;
    if (smp_storage_load_blob(ACTIVE_NVS_KEY, &idx, sizeof(idx), &out_len) == ESP_OK
        && out_len == 1
        && idx < (uint8_t)s_server_count) {
        s_active_index = (int)idx;
    } else {
        s_active_index = DEFAULT_ACTIVE_INDEX;
    }
    ESP_LOGI(TAG, "Active server index: %d (%s)",
             s_active_index,
             s_active_index < s_server_count ? s_servers[s_active_index].host : "???");
}

static void save_active_index(void) {
    uint8_t idx = (uint8_t)s_active_index;
    esp_err_t ret = smp_storage_save_blob_sync(ACTIVE_NVS_KEY, &idx, sizeof(idx));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Active server NVS save failed: %s", esp_err_to_name(ret));
    }
}

// ============== Public API: Init ==============

void smp_servers_init(void) {
    if (s_initialized) return;

    // Load operators (separate from servers, survives server list re-init)
    load_operators_from_nvs();

    // Try loading servers from NVS
    if (load_servers_from_nvs()) {
        load_active_index();
        s_initialized = true;

        ESP_LOGI(TAG, "Server list: %d total, active: %s",
                 s_server_count, s_servers[s_active_index].host);
        return;
    }

    // First boot: decode all presets
    ESP_LOGI(TAG, "First boot: initializing %d preset servers", (int)PRESET_COUNT);

    s_server_count = 0;

    for (int i = 0; i < (int)PRESET_COUNT && s_server_count < MAX_SMP_SERVERS; i++) {
        if (!decode_preset(&PRESETS[i], &s_servers[s_server_count])) {
            ESP_LOGW(TAG, "Skipping preset %d (%s)", i, PRESETS[i].host);
            continue;
        }
        s_server_count++;
    }

    // Default active: SimpleGo (last preset)
    s_active_index = s_server_count - 1;

    save_servers_to_nvs();
    save_operators_to_nvs();
    save_active_index();

    s_initialized = true;

    // Log final state
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SMP Server List (%d servers) ===", s_server_count);
    for (int i = 0; i < s_server_count; i++) {
        const char *op_name = smp_operators_get_name(s_servers[i].op);
        ESP_LOGI(TAG, "  [%d] %s%-24s %s  fp=%02x%02x...",
                 i,
                 i == s_active_index ? "*" : " ",
                 s_servers[i].host,
                 op_name,
                 s_servers[i].key_hash[0], s_servers[i].key_hash[1]);
    }
    ESP_LOGI(TAG, "  (* = active)");
    ESP_LOGI(TAG, "");
}

// ============== Public API: Server Queries ==============

int smp_servers_count(void) {
    return s_server_count;
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

// ============== Public API: Active Server ==============

smp_server_t *smp_servers_get_active(void) {
    if (s_active_index < 0 || s_active_index >= s_server_count) {
        ESP_LOGE(TAG, "Active index %d out of range (count=%d)!",
                 s_active_index, s_server_count);
        return NULL;
    }
    return &s_servers[s_active_index];
}

int smp_servers_get_active_index(void) {
    return s_active_index;
}

void smp_servers_set_active(int index) {
    if (index < 0 || index >= s_server_count) {
        ESP_LOGE(TAG, "Cannot set active: index %d out of range", index);
        return;
    }
    s_active_index = index;
    save_active_index();
    ESP_LOGI(TAG, "Active server changed to [%d] %s (effective on next reboot)",
             index, s_servers[index].host);
}

// ============== Public API: Server Modification ==============

smp_server_t *smp_servers_find_by_host(const char *host) {
    if (!host || !host[0]) return NULL;
    for (int i = 0; i < s_server_count; i++) {
        if (strcmp(s_servers[i].host, host) == 0) {
            return &s_servers[i];
        }
    }
    return NULL;
}

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

const char *smp_operators_get_name(uint8_t op_id) {
    if (op_id >= MAX_SMP_OPERATORS) return "???";
    return s_operators[op_id].name;
}
