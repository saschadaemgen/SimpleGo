/**
 * @file ui_main.h
 * @brief Main Screen - Chat List
 */

#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_main_create(void);
void ui_main_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MAIN_H */
