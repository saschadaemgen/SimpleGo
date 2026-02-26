/**
 * @file hal_common.h
 * @brief Common HAL types, error codes and macros
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * VERSION
 *==========================================================================*/

#define HAL_VERSION_MAJOR   1
#define HAL_VERSION_MINOR   0
#define HAL_VERSION_PATCH   0
#define HAL_VERSION_STRING  "1.0.0"

/*============================================================================
 * ERROR CODES
 *==========================================================================*/

typedef enum {
    HAL_OK              = 0,     /**< Success */
    HAL_ERR_GENERIC     = -1,    /**< Generic error */
    HAL_ERR_INVALID_ARG = -2,    /**< Invalid argument */
    HAL_ERR_NO_MEM      = -3,    /**< Out of memory */
    HAL_ERR_NOT_FOUND   = -4,    /**< Resource not found */
    HAL_ERR_TIMEOUT     = -5,    /**< Operation timed out */
    HAL_ERR_BUSY        = -6,    /**< Resource busy */
    HAL_ERR_NOT_INIT    = -7,    /**< Not initialized */
    HAL_ERR_ALREADY     = -8,    /**< Already done/exists */
    HAL_ERR_IO          = -9,    /**< I/O error */
    HAL_ERR_NOT_SUPPORTED = -10, /**< Not supported on this device */
} hal_err_t;

/*============================================================================
 * COMMON TYPES
 *==========================================================================*/

/**
 * @brief Point in 2D space
 */
typedef struct {
    int16_t x;
    int16_t y;
} hal_point_t;

/**
 * @brief Size (width x height)
 */
typedef struct {
    uint16_t width;
    uint16_t height;
} hal_size_t;

/**
 * @brief Rectangle
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} hal_rect_t;

/**
 * @brief Color (ARGB8888)
 */
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} hal_color_t;

/**
 * @brief RGB565 color (for displays)
 */
typedef uint16_t hal_color16_t;

/**
 * @brief Convert ARGB8888 to RGB565
 */
static inline hal_color16_t hal_color_to_rgb565(hal_color_t c) {
    return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
}

/**
 * @brief Create color from RGB values
 */
static inline hal_color_t hal_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (hal_color_t){.r = r, .g = g, .b = b, .a = 255};
}

/*============================================================================
 * COMMON MACROS
 *==========================================================================*/

#define HAL_MIN(a, b)       ((a) < (b) ? (a) : (b))
#define HAL_MAX(a, b)       ((a) > (b) ? (a) : (b))
#define HAL_CLAMP(x, lo, hi) HAL_MIN(HAL_MAX(x, lo), hi)
#define HAL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define HAL_UNUSED(x)       (void)(x)

/*============================================================================
 * LOGGING (maps to platform-specific logging)
 *==========================================================================*/

#ifndef HAL_LOG_LEVEL
#define HAL_LOG_LEVEL 3  // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
#endif

#ifdef CONFIG_IDF_TARGET
    // ESP-IDF
    #include "esp_log.h"
    #define HAL_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
    #define HAL_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
    #define HAL_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
    #define HAL_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
    #define HAL_LOGV(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)
#else
    // Generic (e.g., Raspberry Pi)
    #include <stdio.h>
    #define HAL_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define HAL_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define HAL_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define HAL_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define HAL_LOGV(tag, fmt, ...) printf("[V][%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_COMMON_H */
