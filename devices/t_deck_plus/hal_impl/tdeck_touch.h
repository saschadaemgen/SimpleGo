/**
 * @file tdeck_touch.h
 * @brief GT911 Touch Driver for T-Deck Plus
 */

#ifndef TDECK_TOUCH_H
#define TDECK_TOUCH_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tdeck_touch_init(void);
bool tdeck_touch_read(int16_t *x, int16_t *y);
lv_indev_t *tdeck_touch_register_lvgl(void);

#ifdef __cplusplus
}
#endif

#endif /* TDECK_TOUCH_H */
