/**
 * @file ui_developer.h
 * @brief Developer Screen - Debug Info, Logs, Tests
 */

#ifndef UI_DEVELOPER_H
#define UI_DEVELOPER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_developer_create(void);
void ui_developer_log(const char *msg);
void ui_developer_update_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_DEVELOPER_H */
