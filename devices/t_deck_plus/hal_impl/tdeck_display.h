/**
 * @file tdeck_display.h
 * @brief T-Deck Plus ST7789 Display Driver
 * 
 * Minimal driver for testing - uses esp_lcd_panel API
 * Based on LilyGo July 2024 configuration
 */

#ifndef TDECK_DISPLAY_H
#define TDECK_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display dimensions (landscape)
#define TDECK_DISPLAY_WIDTH   320
#define TDECK_DISPLAY_HEIGHT  240

/**
 * @brief Initialize T-Deck Plus display
 * 
 * - Enables GPIO10 power
 * - Configures SPI bus
 * - Initializes ST7789 panel
 * - Turns on backlight
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
 * @brief Set backlight brightness
 * @param percent 0-100
 */
void tdeck_display_backlight(uint8_t percent);

/**
 * @brief Run color test (Blue -> Red -> Green -> Black)
 */
void tdeck_display_test(void);

/**
 * @brief Get panel handle for LVGL integration
 * @return Panel handle or NULL if not initialized
 */
esp_lcd_panel_handle_t tdeck_display_get_panel(void);

#ifdef __cplusplus
}
#endif

#endif /* TDECK_DISPLAY_H */