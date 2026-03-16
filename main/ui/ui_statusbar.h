/**
 * @file ui_statusbar.h
 * @brief Shared status bar for all screens (26px)
 *
 * Two variants:
 *   FULL - Screen name left, live clock + WiFi + battery right
 *   CHAT - Back arrow + contact name left, PQ status right
 *
 * One global LVGL timer updates clock and WiFi every 10 seconds.
 *
 * Session 48: Extracted from ui_main.c, ui_theme.c, ui_chat.c
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_STATUSBAR_H
#define UI_STATUSBAR_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Status bar height (both variants) */
#define UI_STATUSBAR_HEIGHT  26

/**
 * @brief FULL variant: screen name left, clock + WiFi + battery right
 * @param parent  Screen object to attach to
 * @param screen_name  Name shown on the left (e.g. "SimpleGo", "Contacts")
 * @return The bar container object
 *
 * Used by: Main, Contacts, Settings, Connect, Developer, Name Setup
 */
lv_obj_t *ui_statusbar_create(lv_obj_t *parent, const char *screen_name);

/**
 * @brief CHAT variant: back arrow + contact name left, PQ status right
 * @param parent  Screen object to attach to
 * @param contact_name  Contact name shown after back arrow
 * @param pq_status  PQ status text (e.g. "E2EE Standard")
 * @param pq_color  Color for PQ status label
 * @param back_cb  Click callback for back arrow button
 * @return The name label object (for later updates)
 *
 * No clock, no WiFi, no battery - maximizes message space.
 */
lv_obj_t *ui_statusbar_create_chat(lv_obj_t *parent,
                                    const char *contact_name,
                                    const char *pq_status,
                                    lv_color_t pq_color,
                                    lv_event_cb_t back_cb);

/** Update live data (clock, WiFi RSSI). No effect on CHAT variant. */
void ui_statusbar_update(void);

/** Set screen name on FULL variant (e.g. tab switch in Settings) */
void ui_statusbar_set_title(const char *title);

/** Set PQ status text and color on CHAT variant */
void ui_statusbar_set_pq_status(const char *status, lv_color_t color);

/** Set contact name on CHAT variant */
void ui_statusbar_set_contact_name(const char *name);

/** Start global 10-second update timer (call once at app start) */
void ui_statusbar_timer_start(void);

/** Stop the global timer */
void ui_statusbar_timer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_STATUSBAR_H */
