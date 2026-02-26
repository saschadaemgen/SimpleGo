/**
 * SimpleGo - WiFi Initialization
 * Extracted from main.c (Auftrag 46b)
 */

#pragma once

/**
 * Initialize WiFi in STA mode and connect.
 * Sets wifi_connected = true when IP is obtained.
 * Uses CONFIG_SIMPLEGO_WIFI_SSID/PASSWORD from Kconfig.
 */
void smp_wifi_init(void);
