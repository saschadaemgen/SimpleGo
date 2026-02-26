/**
 * @file hal_system.h
 * @brief System Hardware Abstraction Layer Interface
 * 
 * Abstracts system functions:
 * - Power management (sleep, wake)
 * - Battery monitoring
 * - Watchdog
 * - System info
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_SYSTEM_H
#define HAL_SYSTEM_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * SYSTEM TYPES
 *==========================================================================*/

/**
 * @brief System capabilities
 */
typedef enum {
    HAL_SYS_CAP_NONE        = 0,
    HAL_SYS_CAP_BATTERY     = (1 << 0),  /**< Has battery */
    HAL_SYS_CAP_CHARGING    = (1 << 1),  /**< Can detect charging */
    HAL_SYS_CAP_DEEP_SLEEP  = (1 << 2),  /**< Deep sleep support */
    HAL_SYS_CAP_LIGHT_SLEEP = (1 << 3),  /**< Light sleep support */
    HAL_SYS_CAP_RTC         = (1 << 4),  /**< Has RTC */
    HAL_SYS_CAP_WATCHDOG    = (1 << 5),  /**< Has watchdog */
    HAL_SYS_CAP_TEMP_SENSOR = (1 << 6),  /**< Internal temp sensor */
} hal_sys_caps_t;

/**
 * @brief Reset reason
 */
typedef enum {
    HAL_RESET_UNKNOWN,
    HAL_RESET_POWER_ON,         /**< Power on reset */
    HAL_RESET_SOFTWARE,         /**< Software reset */
    HAL_RESET_PANIC,            /**< Exception/panic */
    HAL_RESET_WATCHDOG,         /**< Watchdog reset */
    HAL_RESET_DEEP_SLEEP,       /**< Wake from deep sleep */
    HAL_RESET_BROWNOUT,         /**< Brownout */
    HAL_RESET_EXTERNAL,         /**< External reset (button) */
} hal_reset_reason_t;

/**
 * @brief Sleep mode
 */
typedef enum {
    HAL_SLEEP_LIGHT,            /**< Light sleep (fast wake) */
    HAL_SLEEP_DEEP,             /**< Deep sleep (low power) */
} hal_sleep_mode_t;

/**
 * @brief Wakeup source
 */
typedef enum {
    HAL_WAKEUP_TIMER    = (1 << 0),  /**< Timer */
    HAL_WAKEUP_GPIO     = (1 << 1),  /**< GPIO interrupt */
    HAL_WAKEUP_TOUCH    = (1 << 2),  /**< Touch pad */
    HAL_WAKEUP_UART     = (1 << 3),  /**< UART activity */
    HAL_WAKEUP_ULP      = (1 << 4),  /**< ULP coprocessor */
} hal_wakeup_source_t;

/**
 * @brief Battery status
 */
typedef struct {
    uint8_t level;              /**< Battery level 0-100% */
    uint16_t voltage_mv;        /**< Battery voltage in mV */
    bool charging;              /**< Is charging */
    bool usb_connected;         /**< USB power connected */
    int16_t current_ma;         /**< Current in mA (+charging, -discharging) */
    int8_t temperature;         /**< Battery temperature in °C */
} hal_battery_status_t;

/**
 * @brief System information
 */
typedef struct {
    uint32_t capabilities;          /**< Capability flags */
    const char *chip_model;         /**< Chip model (e.g., "ESP32-S3") */
    const char *chip_revision;      /**< Chip revision */
    uint32_t cpu_freq_mhz;          /**< CPU frequency */
    uint32_t heap_free;             /**< Free heap */
    uint32_t heap_total;            /**< Total heap */
    uint32_t psram_free;            /**< Free PSRAM (0 if none) */
    uint32_t psram_total;           /**< Total PSRAM */
    int8_t temperature;             /**< Internal temp in °C (if available) */
    hal_reset_reason_t reset_reason;/**< Last reset reason */
    uint64_t uptime_ms;             /**< Uptime in milliseconds */
    uint8_t mac_wifi[6];            /**< WiFi MAC address */
    uint8_t mac_bt[6];              /**< Bluetooth MAC address */
} hal_sys_info_t;

/**
 * @brief Power event callback
 */
typedef void (*hal_power_callback_t)(void *user_data);

/*============================================================================
 * SYSTEM API
 *==========================================================================*/

/**
 * @brief Initialize system HAL
 * @return HAL_OK on success
 */
hal_err_t hal_sys_init(void);

/**
 * @brief Get system information
 * @return Pointer to system info (refreshed on each call)
 */
const hal_sys_info_t *hal_sys_get_info(void);

/**
 * @brief Get uptime in milliseconds
 * @return Uptime in ms
 */
uint64_t hal_sys_uptime(void);

/**
 * @brief Get current tick count
 * @return Tick count in ms
 */
uint32_t hal_sys_ticks(void);

/**
 * @brief Delay in milliseconds
 * @param ms Delay time
 */
void hal_sys_delay(uint32_t ms);

/**
 * @brief Delay in microseconds
 * @param us Delay time
 */
void hal_sys_delay_us(uint32_t us);

/*============================================================================
 * RESET API
 *==========================================================================*/

/**
 * @brief Software reset
 */
void hal_sys_reset(void) __attribute__((noreturn));

/**
 * @brief Get reset reason
 * @return Reset reason
 */
hal_reset_reason_t hal_sys_get_reset_reason(void);

/**
 * @brief Get reset reason as string
 * @return Reset reason string
 */
const char *hal_sys_get_reset_reason_str(void);

/*============================================================================
 * POWER MANAGEMENT API
 *==========================================================================*/

/**
 * @brief Enter sleep mode
 * @param mode Sleep mode
 * @param wakeup_sources Bitmask of wakeup sources
 * @param duration_ms Sleep duration (for TIMER wakeup, 0 = indefinite)
 * @return HAL_OK on wakeup, or error
 */
hal_err_t hal_sys_sleep(hal_sleep_mode_t mode, uint32_t wakeup_sources, 
                         uint32_t duration_ms);

/**
 * @brief Configure GPIO wakeup
 * @param gpio_num GPIO number
 * @param level Wakeup on high (true) or low (false)
 * @return HAL_OK on success
 */
hal_err_t hal_sys_set_gpio_wakeup(int gpio_num, bool level);

/**
 * @brief Get wakeup cause after sleep
 * @return Wakeup source that triggered wake
 */
hal_wakeup_source_t hal_sys_get_wakeup_cause(void);

/**
 * @brief Set CPU frequency
 * @param mhz Frequency in MHz (device-specific valid values)
 * @return HAL_OK on success
 */
hal_err_t hal_sys_set_cpu_freq(uint32_t mhz);

/*============================================================================
 * BATTERY API
 *==========================================================================*/

/**
 * @brief Get battery status
 * @return Pointer to battery status (or NULL if no battery)
 */
const hal_battery_status_t *hal_sys_get_battery(void);

/**
 * @brief Check if running on battery
 * @return true if on battery (not charging)
 */
bool hal_sys_on_battery(void);

/**
 * @brief Set low battery callback
 * @param threshold Battery level threshold (0-100)
 * @param cb Callback function
 * @param user_data User data
 */
void hal_sys_set_low_battery_cb(uint8_t threshold, hal_power_callback_t cb, 
                                 void *user_data);

/*============================================================================
 * WATCHDOG API
 *==========================================================================*/

/**
 * @brief Initialize watchdog
 * @param timeout_ms Timeout in milliseconds
 * @return HAL_OK on success
 */
hal_err_t hal_wdt_init(uint32_t timeout_ms);

/**
 * @brief Feed/reset watchdog
 */
void hal_wdt_feed(void);

/**
 * @brief Disable watchdog
 */
void hal_wdt_disable(void);

/*============================================================================
 * UNIQUE ID API
 *==========================================================================*/

/**
 * @brief Get unique device ID
 * @param id Output buffer (min 8 bytes)
 * @param len Buffer size (in), actual size (out)
 * @return HAL_OK on success
 */
hal_err_t hal_sys_get_unique_id(uint8_t *id, size_t *len);

/**
 * @brief Get unique ID as hex string
 * @param buf Output buffer (min 17 bytes for 8-byte ID)
 * @param len Buffer size
 * @return Pointer to buf
 */
char *hal_sys_get_unique_id_str(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SYSTEM_H */
