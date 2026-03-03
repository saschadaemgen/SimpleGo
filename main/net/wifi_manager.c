/**
 * @file wifi_manager.c
 * @brief Unified WiFi Manager - Init, Scan, Connect, NVS
 *
 * Session 39i: Replaces BOTH smp_wifi.c and old wifi_manager.c.
 * Single event handler, single state machine, no conflicts.
 *
 * Boot: NVS credentials first, Kconfig fallback for dev.
 * Scan: Results cached in SCAN_DONE handler (race-free).
 * Auth: WPA2/WPA3 transition mode (SAE_PWE_BOTH).
 *
 * SimpleGo
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "wifi_manager.h"
#include "smp_types.h"          /* wifi_connected */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

#define NVS_WIFI_NS   "wifi_cfg"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "password"

/* ================================================================
 * State Machine
 *
 * IDLE:       Default. Disconnect -> auto-reconnect (with limit).
 * CONNECTING: User picked a new network. Disconnect -> retry.
 * SCANNING:   Scan active. Disconnect -> ignore (expected).
 * ================================================================ */

typedef enum {
    WM_IDLE,
    WM_CONNECTING,
    WM_SCANNING,
} wm_state_t;

static volatile wm_state_t s_state = WM_IDLE;
static int s_retries = 0;
#define MAX_RETRIES 10

/* ================================================================
 * Cached Scan Results
 *
 * Read from ESP-IDF inside SCAN_DONE handler, BEFORE anything
 * can call esp_wifi_connect() and clear the buffer.
 * UI reads only from this cache via get_scan_results().
 * ================================================================ */

#define SCAN_CACHE_MAX 16

static volatile bool s_scan_done = false;
static wifi_ap_record_t s_cached_aps[SCAN_CACHE_MAX];
static uint16_t s_cached_count = 0;

/* ================================================================
 * Auth Config Helper
 *
 * Fix for WPA3 SAE failure (auth->init 0x600):
 *   - Accept WPA2 as minimum (works with WPA2-only AND transition)
 *   - SAE_PWE_BOTH for WPA3 negotiation
 *   - PMF capable but not required
 * ================================================================ */

static void apply_auth(wifi_config_t *cfg)
{
    /* Session 39k: WPA2_PSK threshold (was WPA_WPA2_PSK).
     * WPA2/WPA3 Transition Mode routers advertise SAE, but ESP32-S3
     * SAE negotiation is fragile -- auth->init 0x600 on every attempt.
     * WPA2_PSK threshold: accepts WPA2 and WPA3, but prefers WPA2-PSK
     * when available. SAE still works if router enforces WPA3-only. */
    cfg->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    cfg->sta.pmf_cfg.capable = true;
    cfg->sta.pmf_cfg.required = false;
}

/* ================================================================
 * Unified Event Handler
 * ================================================================ */

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    /* --- WiFi Events --- */
    if (base == WIFI_EVENT) {

        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_connected = false;

            switch (s_state) {
            case WM_SCANNING:
                /* Expected: we disconnected for scan. Do nothing. */
                break;

            case WM_CONNECTING:
                /* User-initiated connect: retry until limit */
                if (s_retries < MAX_RETRIES) {
                    s_retries++;
                    ESP_LOGI(TAG, "Connect retry %d/%d", s_retries, MAX_RETRIES);
                    esp_wifi_connect();
                } else {
                    ESP_LOGW(TAG, "Connect failed after %d retries", MAX_RETRIES);
                    s_state = WM_IDLE;
                }
                break;

            case WM_IDLE:
            default:
                /* Auto-reconnect with limit */
                if (s_retries < MAX_RETRIES) {
                    s_retries++;
                    ESP_LOGI(TAG, "Reconnecting %d/%d", s_retries, MAX_RETRIES);
                    esp_wifi_connect();
                } else {
                    ESP_LOGW(TAG, "Reconnect exhausted (%d)", MAX_RETRIES);
                }
                break;
            }

        } else if (id == WIFI_EVENT_SCAN_DONE) {
            /* Cache results NOW before anything can clear them */
            s_cached_count = SCAN_CACHE_MAX;
            esp_wifi_scan_get_ap_records(&s_cached_count, s_cached_aps);
            ESP_LOGI(TAG, "Scan done: %d APs", s_cached_count);

            s_state = WM_IDLE;
            s_retries = 0;
            s_scan_done = true;
            /* No reconnect here. UI reads cache, then user decides. */
        }

    /* --- IP Events --- */
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        wifi_connected = true;
        s_state = WM_IDLE;
        s_retries = 0;
    }
}

/* ================================================================
 * Init (replaces smp_wifi_init)
 * ================================================================ */

void wifi_manager_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t iw, ii;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &iw));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &ii));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Boot connect: NVS first, Kconfig fallback */
    wifi_config_t wc = {0};
    bool have_creds = false;

    char ssid[33] = {0}, pass[64] = {0};
    if (wifi_manager_load_credentials(ssid, pass) && ssid[0]) {
        strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
        strncpy((char *)wc.sta.password, pass, sizeof(wc.sta.password) - 1);
        have_creds = true;
        ESP_LOGI(TAG, "Boot: NVS credentials '%s'", ssid);
    }
#ifdef CONFIG_SIMPLEGO_WIFI_SSID
    else if (strlen(CONFIG_SIMPLEGO_WIFI_SSID) > 0) {
        strncpy((char *)wc.sta.ssid, CONFIG_SIMPLEGO_WIFI_SSID,
                sizeof(wc.sta.ssid) - 1);
        strncpy((char *)wc.sta.password, CONFIG_SIMPLEGO_WIFI_PASSWORD,
                sizeof(wc.sta.password) - 1);
        have_creds = true;
        ESP_LOGI(TAG, "Boot: Kconfig credentials '%s'",
                 CONFIG_SIMPLEGO_WIFI_SSID);
    }
#endif

    if (have_creds) {
        apply_auth(&wc);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "No saved network — waiting for setup");
    }
}

/* ================================================================
 * Scan
 * ================================================================ */

void wifi_manager_start_scan(void)
{
    s_scan_done = false;
    s_cached_count = 0;
    s_state = WM_SCANNING;
    s_retries = MAX_RETRIES;   /* Block auto-reconnect during scan */

    esp_wifi_scan_stop();

    /* Abort any pending connect attempt; handler sees WM_SCANNING, ignores */
    if (!wifi_connected) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    wifi_scan_config_t sc = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active = { .min = 120, .max = 300 },
    };

    esp_err_t ret = esp_wifi_scan_start(&sc, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(ret));
        s_scan_done = true;
        s_state = WM_IDLE;
        s_retries = 0;
    } else {
        ESP_LOGI(TAG, "Scan started");
    }
}

bool wifi_manager_is_scan_done(void)
{
    return s_scan_done;
}

uint16_t wifi_manager_get_scan_count(void)
{
    return s_cached_count;
}

const wifi_ap_record_t *wifi_manager_get_scan_results(void)
{
    return s_cached_aps;
}

/* ================================================================
 * Connect
 * ================================================================ */

void wifi_manager_connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) return;

    /* Persist first (Evgeny's Golden Rule) */
    wifi_manager_save_credentials(ssid, password);

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    if (password && password[0]) {
        strncpy((char *)wc.sta.password, password, sizeof(wc.sta.password) - 1);
    }
    apply_auth(&wc);

    if (esp_wifi_set_config(WIFI_IF_STA, &wc) != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed");
        return;
    }

    s_state = WM_CONNECTING;
    s_retries = 0;
    ESP_LOGI(TAG, "Connecting to '%s'", ssid);

    /* Session 39k: Always disconnect first to reset ESP-IDF driver state.
     * After exhausted retries, the driver can be stuck. A clean
     * disconnect + short delay ensures fresh connect attempt. */
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_connect();
}

void wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Manual disconnect");
    s_state = WM_IDLE;
    s_retries = MAX_RETRIES;   /* Block auto-reconnect */
    esp_wifi_disconnect();
}

/* ================================================================
 * Resume (call when leaving settings screen)
 * ================================================================ */

void wifi_manager_resume_auto_reconnect(void)
{
    s_retries = 0;
    s_state = WM_IDLE;
    if (!wifi_connected) {
        char ssid[33] = {0};
        if (wifi_manager_load_credentials(ssid, NULL) && ssid[0]) {
            ESP_LOGI(TAG, "Resuming reconnect to '%s'", ssid);
            esp_wifi_connect();
        }
    }
}

/* ================================================================
 * Status
 * ================================================================ */

wifi_status_t wifi_manager_get_status(void)
{
    wifi_status_t st = {0};
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        st.connected = true;
        strncpy(st.ssid, (char *)ap.ssid, 32);
        st.rssi = ap.rssi;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(netif, &ip) == ESP_OK)
                snprintf(st.ip, sizeof(st.ip), IPSTR, IP2STR(&ip.ip));
        }
    }
    return st;
}

bool wifi_manager_needs_setup(void)
{
    char ssid[33] = {0};
    if (wifi_manager_load_credentials(ssid, NULL) && ssid[0])
        return false;
#ifdef CONFIG_SIMPLEGO_WIFI_SSID
    if (strlen(CONFIG_SIMPLEGO_WIFI_SSID) > 0)
        return false;
#endif
    return true;
}

/* ================================================================
 * NVS
 * ================================================================ */

bool wifi_manager_load_credentials(char *ssid, char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READONLY, &h) != ESP_OK) return false;
    bool found = false;
    size_t len = 33;
    if (nvs_get_str(h, NVS_KEY_SSID, ssid, &len) == ESP_OK && len > 1)
        found = true;
    if (found && password) {
        len = 64;
        if (nvs_get_str(h, NVS_KEY_PASS, password, &len) != ESP_OK)
            password[0] = '\0';
    }
    nvs_close(h);
    return found;
}

bool wifi_manager_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = (nvs_set_str(h, NVS_KEY_SSID, ssid) == ESP_OK);
    if (ok && password && password[0])
        ok = (nvs_set_str(h, NVS_KEY_PASS, password) == ESP_OK);
    if (ok) nvs_commit(h);
    nvs_close(h);
    if (ok) ESP_LOGI(TAG, "Saved: '%s'", ssid);
    return ok;
}

bool wifi_manager_forget_network(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_WIFI_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_erase_key(h, NVS_KEY_SSID);
    nvs_erase_key(h, NVS_KEY_PASS);
    nvs_commit(h);
    nvs_close(h);
    wifi_manager_disconnect();
    ESP_LOGI(TAG, "Network forgotten");
    return true;
}
