/**
 * @file ui_lock.c
 * @brief Lock Screen - SEC-04 Phase 1 (any key to unlock)
 *
 * Displayed after inactivity timeout. All decrypted message data
 * has been wiped from PSRAM and LVGL before this screen appears.
 * Any physical key press unlocks and returns to previous screen.
 *
 * Phase 2 will add PIN-based unlock with attempt limiting.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_lock.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"           /* ui_chat_get_keyboard_indev() */
#include "esp_log.h"

static const char *TAG = "UI_LOCK";

static lv_obj_t *s_lock_screen = NULL;
static lv_obj_t *s_hidden_ta   = NULL;
static lv_group_t *s_lock_group = NULL;

/* ============== Event Handlers ============== */

static void on_any_key(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Key press detected, unlocking");
    ui_manager_unlock();
}

/* ============== Public API ============== */

lv_obj_t *ui_lock_create(void)
{
    s_lock_screen = lv_obj_create(NULL);
    ui_theme_apply(s_lock_screen);

    /* Lock icon */
    lv_obj_t *icon = lv_label_create(s_lock_screen);
    lv_label_set_text(icon, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(icon, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(icon, UI_FONT_MD, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);

    /* "SimpleGo Locked" title */
    lv_obj_t *title = lv_label_create(s_lock_screen);
    lv_label_set_text(title, "SimpleGo Locked");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* "Press any key" hint */
    lv_obj_t *hint = lv_label_create(s_lock_screen);
    lv_label_set_text(hint, "Press any key to unlock");
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 24);

    /* Hidden textarea to capture keyboard input.
     * The T-Deck physical keyboard is an LVGL input device that
     * sends characters to the focused object in the active group.
     * We create a 1x1 invisible textarea and assign the keyboard
     * group to it so ANY key press fires our callback. */
    s_hidden_ta = lv_textarea_create(s_lock_screen);
    lv_obj_set_size(s_hidden_ta, 1, 1);
    lv_obj_set_pos(s_hidden_ta, -10, -10);  /* offscreen */
    lv_obj_set_style_opa(s_hidden_ta, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_hidden_ta, LV_OBJ_FLAG_SCROLLABLE);

    /* Character key -> VALUE_CHANGED, Enter -> READY, special -> KEY */
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_KEY, NULL);

    /* Create group and assign keyboard indev */
    s_lock_group = lv_group_create();
    lv_group_add_obj(s_lock_group, s_hidden_ta);

    lv_indev_t *kb = ui_chat_get_keyboard_indev();
    if (kb) {
        lv_indev_set_group(kb, s_lock_group);
        ESP_LOGI(TAG, "Keyboard assigned to lock screen");
    }

    ESP_LOGI(TAG, "Lock screen created (Phase 1: any key unlock)");
    return s_lock_screen;
}

void ui_lock_cleanup(void)
{
    s_lock_screen = NULL;
    s_hidden_ta   = NULL;

    if (s_lock_group) {
        lv_group_del(s_lock_group);
        s_lock_group = NULL;
    }

    ESP_LOGI(TAG, "Lock screen cleanup");
}
