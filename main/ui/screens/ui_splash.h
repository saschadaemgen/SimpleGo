/**
 * @file ui_splash.h
 * @brief SimpleGo Splash Screen
 */

#ifndef UI_SPLASH_H
#define UI_SPLASH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_splash_create(void);
void ui_splash_set_status(const char *status);
void ui_splash_set_progress(int percent);

#ifdef __cplusplus
}
#endif

#endif /* UI_SPLASH_H */
