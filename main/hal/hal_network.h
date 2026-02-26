/**
 * @file hal_network.h
 * @brief Network Hardware Abstraction Layer Interface
 * 
 * Abstracts network connectivity:
 * - ESP32: WiFi + Optional Ethernet
 * - Raspberry Pi: Ethernet + WiFi
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * NETWORK TYPES
 *==========================================================================*/

/**
 * @brief Network interface type
 */
typedef enum {
    HAL_NET_WIFI_STA,   /**< WiFi station */
    HAL_NET_WIFI_AP,    /**< WiFi access point */
    HAL_NET_ETHERNET,   /**< Ethernet */
    HAL_NET_CELLULAR,   /**< Cellular (future) */
} hal_net_type_t;

/**
 * @brief Network state
 */
typedef enum {
    HAL_NET_STATE_DISCONNECTED,
    HAL_NET_STATE_CONNECTING,
    HAL_NET_STATE_CONNECTED,
    HAL_NET_STATE_GOT_IP,
    HAL_NET_STATE_FAILED,
} hal_net_state_t;

/**
 * @brief WiFi auth mode
 */
typedef enum {
    HAL_WIFI_AUTH_OPEN,
    HAL_WIFI_AUTH_WEP,
    HAL_WIFI_AUTH_WPA_PSK,
    HAL_WIFI_AUTH_WPA2_PSK,
    HAL_WIFI_AUTH_WPA_WPA2_PSK,
    HAL_WIFI_AUTH_WPA3_PSK,
    HAL_WIFI_AUTH_WPA2_ENTERPRISE,
} hal_wifi_auth_t;

/**
 * @brief IP address (v4)
 */
typedef struct {
    uint8_t addr[4];
} hal_ip4_t;

/**
 * @brief Network configuration
 */
typedef struct {
    hal_net_type_t type;            /**< Interface type */
    bool use_dhcp;                  /**< Use DHCP */
    hal_ip4_t ip;                   /**< Static IP (if not DHCP) */
    hal_ip4_t netmask;              /**< Netmask */
    hal_ip4_t gateway;              /**< Gateway */
    hal_ip4_t dns1;                 /**< Primary DNS */
    hal_ip4_t dns2;                 /**< Secondary DNS */
} hal_net_config_t;

/**
 * @brief WiFi configuration
 */
typedef struct {
    char ssid[33];                  /**< SSID (max 32 chars) */
    char password[65];              /**< Password (max 64 chars) */
    hal_wifi_auth_t auth;           /**< Auth mode (auto-detect if OPEN) */
    uint8_t channel;                /**< Channel (0 = auto) */
    uint8_t bssid[6];               /**< BSSID (optional, all zeros = any) */
} hal_wifi_config_t;

/**
 * @brief WiFi scan result
 */
typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    hal_wifi_auth_t auth;
} hal_wifi_scan_result_t;

/**
 * @brief Network status
 */
typedef struct {
    hal_net_state_t state;
    hal_ip4_t ip;
    hal_ip4_t netmask;
    hal_ip4_t gateway;
    int8_t rssi;                    /**< WiFi signal strength */
    uint8_t mac[6];                 /**< MAC address */
    char ssid[33];                  /**< Connected SSID (WiFi only) */
} hal_net_status_t;

/**
 * @brief Network event type
 */
typedef enum {
    HAL_NET_EVENT_CONNECTED,
    HAL_NET_EVENT_DISCONNECTED,
    HAL_NET_EVENT_GOT_IP,
    HAL_NET_EVENT_LOST_IP,
    HAL_NET_EVENT_SCAN_DONE,
    HAL_NET_EVENT_AUTH_FAIL,
    HAL_NET_EVENT_NO_AP_FOUND,
} hal_net_event_t;

/**
 * @brief Network event callback
 */
typedef void (*hal_net_event_cb_t)(hal_net_event_t event, void *user_data);

/*============================================================================
 * NETWORK API
 *==========================================================================*/

/**
 * @brief Initialize network HAL
 * @return HAL_OK on success
 */
hal_err_t hal_net_init(void);

/**
 * @brief Deinitialize network HAL
 * @return HAL_OK on success
 */
hal_err_t hal_net_deinit(void);

/**
 * @brief Set network event callback
 * @param cb Callback function
 * @param user_data User data
 */
void hal_net_set_event_cb(hal_net_event_cb_t cb, void *user_data);

/**
 * @brief Get network status
 * @param type Interface type
 * @return Pointer to status (valid until next call)
 */
const hal_net_status_t *hal_net_get_status(hal_net_type_t type);

/**
 * @brief Check if connected with IP
 * @return true if connected and has IP
 */
bool hal_net_is_connected(void);

/*============================================================================
 * WIFI API
 *==========================================================================*/

/**
 * @brief Start WiFi station mode
 * @return HAL_OK on success
 */
hal_err_t hal_wifi_start(void);

/**
 * @brief Stop WiFi
 * @return HAL_OK on success
 */
hal_err_t hal_wifi_stop(void);

/**
 * @brief Connect to WiFi network
 * @param config WiFi configuration
 * @return HAL_OK on success (connection is async)
 */
hal_err_t hal_wifi_connect(const hal_wifi_config_t *config);

/**
 * @brief Disconnect from WiFi
 * @return HAL_OK on success
 */
hal_err_t hal_wifi_disconnect(void);

/**
 * @brief Start WiFi scan
 * @return HAL_OK on success
 */
hal_err_t hal_wifi_scan_start(void);

/**
 * @brief Get scan results
 * @param results Output array
 * @param max_results Array size
 * @return Number of results found
 */
int hal_wifi_scan_get_results(hal_wifi_scan_result_t *results, int max_results);

/**
 * @brief Get current RSSI
 * @return RSSI in dBm, or 0 if not connected
 */
int8_t hal_wifi_get_rssi(void);

/**
 * @brief Set WiFi power save mode
 * @param enable true to enable
 * @return HAL_OK on success
 */
hal_err_t hal_wifi_set_power_save(bool enable);

/*============================================================================
 * ETHERNET API (optional)
 *==========================================================================*/

/**
 * @brief Start Ethernet
 * @return HAL_OK on success, HAL_ERR_NOT_SUPPORTED if not available
 */
hal_err_t hal_ethernet_start(void);

/**
 * @brief Stop Ethernet
 * @return HAL_OK on success
 */
hal_err_t hal_ethernet_stop(void);

/*============================================================================
 * DNS HELPER
 *==========================================================================*/

/**
 * @brief Resolve hostname to IP
 * @param hostname Hostname
 * @param ip Output IP address
 * @return HAL_OK on success
 */
hal_err_t hal_net_gethostbyname(const char *hostname, hal_ip4_t *ip);

/*============================================================================
 * IP STRING HELPERS
 *==========================================================================*/

/**
 * @brief Convert IP to string
 * @param ip IP address
 * @param buf Output buffer (min 16 bytes)
 * @return Pointer to buf
 */
static inline char *hal_ip4_to_str(hal_ip4_t ip, char *buf) {
    extern int sprintf(char *, const char *, ...);
    sprintf(buf, "%d.%d.%d.%d", ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);
    return buf;
}

/**
 * @brief Create IP from bytes
 */
static inline hal_ip4_t hal_ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (hal_ip4_t){{a, b, c, d}};
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_NETWORK_H */
