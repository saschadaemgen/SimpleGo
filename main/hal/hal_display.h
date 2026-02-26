/**
 * @file hal_display.h
 * @brief Display Hardware Abstraction Layer Interface
 * 
 * Abstracts display hardware across different devices:
 * - T-Deck Plus: 320x240 ST7789 SPI
 * - T-Embed CC1101: 170x320 ST7789 SPI
 * - Raspberry Pi: Framebuffer/SDL
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DISPLAY TYPES
 *==========================================================================*/

/**
 * @brief Display orientation
 */
typedef enum {
    HAL_DISPLAY_ORIENTATION_0   = 0,    /**< Portrait */
    HAL_DISPLAY_ORIENTATION_90  = 90,   /**< Landscape (rotated CW) */
    HAL_DISPLAY_ORIENTATION_180 = 180,  /**< Portrait (upside down) */
    HAL_DISPLAY_ORIENTATION_270 = 270,  /**< Landscape (rotated CCW) */
} hal_display_orientation_t;

/**
 * @brief Display color depth
 */
typedef enum {
    HAL_DISPLAY_COLOR_1BIT  = 1,   /**< Monochrome */
    HAL_DISPLAY_COLOR_8BIT  = 8,   /**< 256 colors */
    HAL_DISPLAY_COLOR_16BIT = 16,  /**< RGB565 */
    HAL_DISPLAY_COLOR_24BIT = 24,  /**< RGB888 */
    HAL_DISPLAY_COLOR_32BIT = 32,  /**< ARGB8888 */
} hal_display_color_depth_t;

/**
 * @brief Display capabilities flags
 */
typedef enum {
    HAL_DISPLAY_CAP_NONE        = 0,
    HAL_DISPLAY_CAP_TOUCH       = (1 << 0),  /**< Has touch overlay */
    HAL_DISPLAY_CAP_BACKLIGHT   = (1 << 1),  /**< Adjustable backlight */
    HAL_DISPLAY_CAP_ROTATION    = (1 << 2),  /**< Hardware rotation */
    HAL_DISPLAY_CAP_VSYNC       = (1 << 3),  /**< V-sync available */
    HAL_DISPLAY_CAP_DMA         = (1 << 4),  /**< DMA transfers */
    HAL_DISPLAY_CAP_DOUBLE_BUF  = (1 << 5),  /**< Double buffering */
} hal_display_caps_t;

/**
 * @brief Display information structure
 */
typedef struct {
    const char *name;                       /**< Device name */
    hal_size_t native_size;                 /**< Native resolution */
    hal_size_t current_size;                /**< Current resolution (after rotation) */
    hal_display_orientation_t orientation;  /**< Current orientation */
    hal_display_color_depth_t color_depth;  /**< Color depth */
    uint32_t capabilities;                  /**< Capability flags */
    uint8_t backlight_level;                /**< Current backlight 0-100 */
} hal_display_info_t;

/**
 * @brief Display configuration
 */
typedef struct {
    hal_display_orientation_t orientation;  /**< Initial orientation */
    uint8_t backlight_level;                /**< Initial backlight 0-100 */
    bool invert_colors;                     /**< Invert display colors */
    void *platform_config;                  /**< Platform-specific config */
} hal_display_config_t;

/**
 * @brief Flush callback for LVGL integration
 */
typedef void (*hal_display_flush_cb_t)(void *disp_drv, const hal_rect_t *area, 
                                        uint8_t *color_data);

/*============================================================================
 * DISPLAY API
 *==========================================================================*/

/**
 * @brief Initialize display HAL
 * @param config Display configuration (NULL for defaults)
 * @return HAL_OK on success
 */
hal_err_t hal_display_init(const hal_display_config_t *config);

/**
 * @brief Deinitialize display HAL
 * @return HAL_OK on success
 */
hal_err_t hal_display_deinit(void);

/**
 * @brief Get display information
 * @return Pointer to display info structure
 */
const hal_display_info_t *hal_display_get_info(void);

/**
 * @brief Set display orientation
 * @param orientation New orientation
 * @return HAL_OK on success
 */
hal_err_t hal_display_set_orientation(hal_display_orientation_t orientation);

/**
 * @brief Set backlight level
 * @param level Brightness 0-100 (0 = off)
 * @return HAL_OK on success
 */
hal_err_t hal_display_set_backlight(uint8_t level);

/**
 * @brief Get current backlight level
 * @return Brightness 0-100
 */
uint8_t hal_display_get_backlight(void);

/**
 * @brief Flush rectangle to display
 * @param area Area to update
 * @param color_data Color data (format depends on color_depth)
 * @return HAL_OK on success
 */
hal_err_t hal_display_flush(const hal_rect_t *area, const uint8_t *color_data);

/**
 * @brief Wait for flush to complete
 * @param timeout_ms Maximum wait time
 * @return HAL_OK on success, HAL_ERR_TIMEOUT on timeout
 */
hal_err_t hal_display_flush_wait(uint32_t timeout_ms);

/**
 * @brief Signal flush complete (call from DMA ISR)
 */
void hal_display_flush_ready(void);

/**
 * @brief Turn display on/off
 * @param on true to turn on
 * @return HAL_OK on success
 */
hal_err_t hal_display_power(bool on);

/**
 * @brief Invert display colors
 * @param invert true to invert
 * @return HAL_OK on success
 */
hal_err_t hal_display_invert(bool invert);

/*============================================================================
 * LVGL INTEGRATION
 *==========================================================================*/

/**
 * @brief Register flush callback for LVGL
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void hal_display_set_flush_cb(hal_display_flush_cb_t cb, void *user_data);

/**
 * @brief Get display buffer for LVGL
 * @param buf1 First buffer pointer (output)
 * @param buf2 Second buffer pointer (output, NULL if single buffer)
 * @param size Buffer size in pixels (output)
 * @return HAL_OK on success
 */
hal_err_t hal_display_get_buffers(void **buf1, void **buf2, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DISPLAY_H */
