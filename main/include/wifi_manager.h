/**
 * @file wifi_manager.h
 * @brief Unified WiFi Manager - Init, Scan, Connect, NVS
 *
 * Replaces both smp_wifi.h and old wifi_manager.h.
 * In main.c: replace smp_wifi_init() with wifi_manager_init().
 *
 * SimpleGo
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi_types.h"

extern volatile bool wifi_connected;

typedef struct {
    bool   connected;
    char   ssid[33];
    int8_t rssi;
    char   ip[16];
} wifi_status_t;

/* Init (replaces smp_wifi_init) */
void wifi_manager_init(void);

/* Scan */
void wifi_manager_start_scan(void);
bool wifi_manager_is_scan_done(void);
uint16_t wifi_manager_get_scan_count(void);
const wifi_ap_record_t *wifi_manager_get_scan_results(void);

/* Connect / Disconnect */
void wifi_manager_connect(const char *ssid, const char *password);
void wifi_manager_disconnect(void);
void wifi_manager_resume_auto_reconnect(void);

/* Status */
wifi_status_t wifi_manager_get_status(void);
bool wifi_manager_needs_setup(void);

/* NVS */
bool wifi_manager_load_credentials(char *ssid, char *password);
bool wifi_manager_save_credentials(const char *ssid, const char *password);
bool wifi_manager_forget_network(void);
