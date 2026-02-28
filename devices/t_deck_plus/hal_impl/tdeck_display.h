/**
 * @file tdeck_display.h
 * @brief T-Deck Plus ST7789 Display Driver
 *
 * Minimal driver for testing - uses esp_lcd_panel API
 * Based on LilyGo July 2024 configuration
 *
 * Display backlight is handled by tdeck_backlight.h (pulse-counting GPIO 42).
 */

#ifndef TDECK_DISPLAY_H
#define TDECK_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Display dimensions (landscape) */
#define TDECK_DISPLAY_WIDTH   320
#define TDECK_DISPLAY_HEIGHT  240

/**
 * @brief Initialize T-Deck Plus display
 *
 * - Enables GPIO10 power
 * - Configures SPI bus
 * - Initializes ST7789 panel
 * - Clears screen to black
 * - Backlight is NOT turned on here (caller uses tdeck_backlight_set)
 *
 * @return ESP_OK on success
 */
esp_err_t tdeck_display_init(void);

/**
 * @brief Fill entire screen with color
 * @param color RGB565 color value
 */
void tdeck_display_fill(uint16_t color);

/**
 * @brief Run color test (Blue -> Red -> Green -> Black)
 */
void tdeck_display_test(void);

/**
 * @brief Get panel handle for LVGL integration
 * @return Panel handle or NULL if not initialized
 */
esp_lcd_panel_handle_t tdeck_display_get_panel(void);

/**
 * @brief Get panel IO handle for DMA callback registration
 * @return Panel IO handle or NULL if not initialized
 */
esp_lcd_panel_io_handle_t tdeck_display_get_io_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* TDECK_DISPLAY_H */
