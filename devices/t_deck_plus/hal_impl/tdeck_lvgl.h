/**
 * @file tdeck_lvgl.h
 * @brief LVGL Integration for T-Deck Plus
 */

#ifndef TDECK_LVGL_H
#define TDECK_LVGL_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDECK_DISPLAY_WIDTH  320
#define TDECK_DISPLAY_HEIGHT 240

esp_err_t tdeck_lvgl_init(void);
void tdeck_lvgl_start(void);  // Call after UI is ready!
bool tdeck_lvgl_lock(uint32_t timeout_ms);
void tdeck_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
