/**
 * SimpleGo - ui_name_setup.c
 * First-boot display name setup screen
 *
 * Minimal screen shown on first boot when no display name is configured.
 * After the user enters a name and taps Save, navigates to Main screen.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "ui_name_setup.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "smp_storage.h"
#include "tdeck_keyboard.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_NAME";

static lv_obj_t *s_screen   = NULL;
static lv_obj_t *s_ta       = NULL;
static lv_group_t *s_group  = NULL;

static void on_save(lv_event_t *e)
{
    (void)e;
    if (!s_ta) return;

    const char *text = lv_textarea_get_text(s_ta);
    if (!text || strlen(text) == 0) return;

    esp_err_t ret = storage_set_display_name(text);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "First-boot name saved: %s", text);
    } else {
        ESP_LOGW(TAG, "Name rejected (invalid chars or length)");
        return;
    }

    /* Clean up keyboard group before navigating away */
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev && s_group) {
        lv_indev_set_group(kb_indev, NULL);
    }
    if (s_group) {
        lv_group_delete(s_group);
        s_group = NULL;
    }
    s_ta = NULL;

    ui_manager_show_screen(UI_SCREEN_MAIN, LV_SCR_LOAD_ANIM_NONE);
}

static void on_skip(lv_event_t *e)
{
    (void)e;
    /* Use default "SimpleGo" -- just navigate to Main */
    ESP_LOGI(TAG, "First-boot name skipped, using default");

    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev && s_group) {
        lv_indev_set_group(kb_indev, NULL);
    }
    if (s_group) {
        lv_group_delete(s_group);
        s_group = NULL;
    }
    s_ta = NULL;

    ui_manager_show_screen(UI_SCREEN_MAIN, LV_SCR_LOAD_ANIM_NONE);
}

lv_obj_t *ui_name_setup_create(void)
{
    ESP_LOGI(TAG, "Creating first-boot name setup...");

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Welcome title */
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Welcome to SimpleGo");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_set_pos(title, 20, 30);

    /* Subtitle */
    lv_obj_t *sub = lv_label_create(s_screen);
    lv_label_set_text(sub, "Choose a display name");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_SM, 0);
    lv_obj_set_pos(sub, 20, 52);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(s_screen);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 72);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Name textarea */
    s_ta = lv_textarea_create(s_screen);
    lv_obj_set_size(s_ta, 280, 32);
    lv_obj_set_pos(s_ta, 20, 86);
    lv_textarea_set_one_line(s_ta, true);
    lv_textarea_set_placeholder_text(s_ta, "Your name...");
    lv_textarea_set_max_length(s_ta, 31);
    lv_obj_set_style_bg_color(s_ta, lv_color_hex(0x000810), 0);
    lv_obj_set_style_bg_opa(s_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ta, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_ta, 1, 0);
    lv_obj_set_style_border_opa(s_ta, (lv_opa_t)60, 0);
    lv_obj_set_style_text_color(s_ta, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(s_ta, UI_FONT, 0);
    lv_obj_set_style_radius(s_ta, 4, 0);
    lv_obj_set_style_pad_left(s_ta, 8, 0);

    /* Save button */
    lv_obj_t *save = lv_label_create(s_screen);
    lv_label_set_text(save, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(save, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(save, UI_FONT, 0);
    lv_obj_set_pos(save, 20, 134);
    lv_obj_add_flag(save, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(save, 10);
    lv_obj_add_event_cb(save, on_save, LV_EVENT_CLICKED, NULL);

    /* Skip (use default) */
    lv_obj_t *skip = lv_label_create(s_screen);
    lv_label_set_text(skip, "Skip (use \"SimpleGo\")");
    lv_obj_set_style_text_color(skip, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(skip, UI_FONT_SM, 0);
    lv_obj_set_pos(skip, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(skip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(skip, 10);
    lv_obj_add_event_cb(skip, on_skip, LV_EVENT_CLICKED, NULL);

    /* Keyboard focus to name field */
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev) {
        s_group = lv_group_create();
        lv_group_add_obj(s_group, s_ta);
        lv_indev_set_group(kb_indev, s_group);
        lv_group_focus_obj(s_ta);
        lv_obj_add_state(s_ta, LV_STATE_FOCUSED);
    }

    return s_screen;
}
