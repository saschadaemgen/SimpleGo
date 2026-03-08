/**
 * @file ui_settings_info.c
 * @brief Settings Screen - INFO Tab (Row-based design)
 *
 * Session 39k: Rewritten to row-based design (matches contacts/wifi).
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"
#include "smp_types.h"
#include "smp_storage.h"
#include "lvgl.h"

#define ROW_H           24
#define ROW_GAP         2
#define ROW_BG          lv_color_hex(0x000810)
#define ACCENT_W        3
#define ACCENT_H        16
#define KEY_X           14
#define VAL_X           130

static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_heap_val     = NULL;
static lv_obj_t *s_psram_val    = NULL;
static lv_obj_t *s_lvgl_val     = NULL;
static lv_obj_t *s_status_val   = NULL;
static lv_obj_t *s_name_val     = NULL;
static lv_timer_t *s_refresh_timer = NULL;

/* Name editor overlay (same pattern as WiFi password overlay) */
static lv_obj_t *s_name_overlay    = NULL;
static lv_obj_t *s_name_ta         = NULL;
static lv_group_t *s_name_group    = NULL;
static lv_group_t *s_name_prev_group = NULL;

static void refresh_values(void)
{
    if (s_name_val) {
        char name[32];
        storage_get_display_name(name, sizeof(name));
        lv_label_set_text(s_name_val, name);
    }
    if (s_heap_val) {
        uint32_t fh = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        lv_label_set_text_fmt(s_heap_val, "%lu B", (unsigned long)fh);
    }
    if (s_psram_val) {
        uint32_t fp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        lv_label_set_text_fmt(s_psram_val, "%lu KB", (unsigned long)(fp / 1024));
    }
    if (s_lvgl_val) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        lv_label_set_text_fmt(s_lvgl_val, "%lu / 64K (%d%%)",
                              (unsigned long)mon.free_size, mon.used_pct);
    }
    if (s_status_val) {
        if (wifi_connected) {
            wifi_status_t ws = wifi_manager_get_status();
            lv_label_set_text_fmt(s_status_val, "%s " LV_SYMBOL_OK, ws.ssid);
            lv_obj_set_style_text_color(s_status_val, UI_COLOR_SECONDARY, 0);
        } else {
            lv_label_set_text(s_status_val, "Offline " LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(s_status_val, UI_COLOR_ERROR, 0);
        }
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh_values(); }

/* ================================================================
 * Name Editor Overlay (Session 43, WiFi password overlay pattern)
 * ================================================================ */

static void close_name_overlay(void)
{
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev && s_name_prev_group) {
        lv_indev_set_group(kb_indev, s_name_prev_group);
    }
    if (s_name_group) {
        lv_group_delete(s_name_group);
        s_name_group = NULL;
    }
    s_name_prev_group = NULL;
    s_name_ta = NULL;

    if (s_name_overlay) {
        lv_obj_delete(s_name_overlay);
        s_name_overlay = NULL;
    }
}

static void on_name_save(lv_event_t *e)
{
    (void)e;
    if (!s_name_ta) return;

    const char *text = lv_textarea_get_text(s_name_ta);
    if (!text || strlen(text) == 0) return;

    esp_err_t ret = storage_set_display_name(text);
    if (ret == ESP_OK) {
        ESP_LOGI("UI_INFO", "Display name saved: %s", text);
    } else {
        ESP_LOGW("UI_INFO", "Display name rejected (invalid chars or length)");
    }

    close_name_overlay();
    refresh_values();
}

static void on_name_cancel(lv_event_t *e)
{
    (void)e;
    close_name_overlay();
}

static void show_name_overlay(void)
{
    if (s_name_overlay) close_name_overlay();

    s_name_overlay = lv_obj_create(s_settings_scr);
    lv_obj_set_size(s_name_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_name_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_name_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_name_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_name_overlay, 0, 0);
    lv_obj_set_style_radius(s_name_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_name_overlay, 0, 0);
    lv_obj_clear_flag(s_name_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(s_name_overlay);
    lv_label_set_text(title, "Display Name");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_SM, 0);
    lv_obj_set_pos(title, 20, 24);

    /* Hint */
    lv_obj_t *hint = lv_label_create(s_name_overlay);
    lv_label_set_text(hint, "Visible to your contacts");
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_set_pos(hint, 20, 42);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(s_name_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 62);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Name textarea */
    s_name_ta = lv_textarea_create(s_name_overlay);
    lv_obj_set_size(s_name_ta, 280, 32);
    lv_obj_set_pos(s_name_ta, 20, 76);
    lv_textarea_set_one_line(s_name_ta, true);
    lv_textarea_set_placeholder_text(s_name_ta, "Enter your name...");
    lv_textarea_set_max_length(s_name_ta, 31);
    lv_obj_set_style_bg_color(s_name_ta, lv_color_hex(0x000810), 0);
    lv_obj_set_style_bg_opa(s_name_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_name_ta, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_name_ta, 1, 0);
    lv_obj_set_style_border_opa(s_name_ta, (lv_opa_t)60, 0);
    lv_obj_set_style_text_color(s_name_ta, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(s_name_ta, UI_FONT, 0);
    lv_obj_set_style_radius(s_name_ta, 4, 0);
    lv_obj_set_style_pad_left(s_name_ta, 8, 0);

    /* Pre-fill with current name */
    char current_name[32];
    storage_get_display_name(current_name, sizeof(current_name));
    lv_textarea_set_text(s_name_ta, current_name);

    /* Save button */
    lv_obj_t *save_btn = lv_label_create(s_name_overlay);
    lv_label_set_text(save_btn, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(save_btn, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(save_btn, UI_FONT, 0);
    lv_obj_set_pos(save_btn, 20, 124);
    lv_obj_add_flag(save_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(save_btn, 10);
    lv_obj_add_event_cb(save_btn, on_name_save, LV_EVENT_CLICKED, NULL);

    /* Cancel */
    lv_obj_t *cancel = lv_label_create(s_name_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_name_cancel, LV_EVENT_CLICKED, NULL);

    /* Keyboard focus to name field */
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev) {
        s_name_prev_group = lv_indev_get_group(kb_indev);
        s_name_group = lv_group_create();
        lv_group_add_obj(s_name_group, s_name_ta);
        lv_indev_set_group(kb_indev, s_name_group);
        lv_group_focus_obj(s_name_ta);
        lv_obj_add_state(s_name_ta, LV_STATE_FOCUSED);
    }
}

static void on_name_row_click(lv_event_t *e)
{
    (void)e;
    show_name_overlay();
}

static lv_obj_t *create_info_row(lv_obj_t *parent, const char *key,
                                  const char *value, lv_color_t accent_clr)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *accent = lv_obj_create(row);
    lv_obj_set_size(accent, ACCENT_W, ACCENT_H);
    lv_obj_set_pos(accent, 6, (ROW_H - ACCENT_H) / 2);
    lv_obj_set_style_bg_color(accent, accent_clr, 0);
    lv_obj_set_style_bg_opa(accent, (lv_opa_t)180, 0);
    lv_obj_set_style_radius(accent, 1, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_color(k, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, KEY_X, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_color(v, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
    lv_obj_align(v, LV_ALIGN_LEFT_MID, VAL_X, 0);

    return v;
}

void settings_create_info(lv_obj_t *parent)
{
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, UI_SCREEN_W, CONTENT_H);
    lv_obj_set_pos(s_list, 0, 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_top(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, ROW_GAP, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    /* Branding row */
    lv_obj_t *br = lv_obj_create(s_list);
    lv_obj_set_size(br, LV_PCT(100), ROW_H + 4);
    lv_obj_set_style_bg_color(br, ROW_BG, 0);
    lv_obj_set_style_bg_opa(br, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(br, 0, 0);
    lv_obj_set_style_radius(br, 0, 0);
    lv_obj_set_style_pad_all(br, 0, 0);
    lv_obj_clear_flag(br, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *bl = lv_label_create(br);
    lv_label_set_text(bl, "SimpleGo");
    lv_obj_set_style_text_color(bl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(bl, UI_FONT, 0);
    lv_obj_align(bl, LV_ALIGN_LEFT_MID, 14, 0);

    lv_obj_t *sl = lv_label_create(br);
    lv_label_set_text(sl, "E2EE " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(sl, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(sl, UI_FONT_SM, 0);
    lv_obj_align(sl, LV_ALIGN_RIGHT_MID, -8, 0);

    /* Display name (clickable, opens editor overlay) */
    {
        char name[32];
        storage_get_display_name(name, sizeof(name));

        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_size(row, LV_PCT(100), ROW_H);
        lv_obj_set_style_bg_color(row, ROW_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x001420), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *accent = lv_obj_create(row);
        lv_obj_set_size(accent, ACCENT_W, ACCENT_H);
        lv_obj_set_pos(accent, 6, (ROW_H - ACCENT_H) / 2);
        lv_obj_set_style_bg_color(accent, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_bg_opa(accent, (lv_opa_t)180, 0);
        lv_obj_set_style_radius(accent, 1, 0);
        lv_obj_set_style_border_width(accent, 0, 0);
        lv_obj_set_style_pad_all(accent, 0, 0);
        lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *k = lv_label_create(row);
        lv_label_set_text(k, "Name");
        lv_obj_set_style_text_color(k, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(k, UI_FONT_SM, 0);
        lv_obj_align(k, LV_ALIGN_LEFT_MID, KEY_X, 0);

        s_name_val = lv_label_create(row);
        lv_label_set_text(s_name_val, name);
        lv_obj_set_style_text_color(s_name_val, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(s_name_val, UI_FONT_SM, 0);
        lv_obj_align(s_name_val, LV_ALIGN_LEFT_MID, VAL_X, 0);

        /* Edit hint */
        lv_obj_t *edit = lv_label_create(row);
        lv_label_set_text(edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(edit, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(edit, UI_FONT_SM, 0);
        lv_obj_align(edit, LV_ALIGN_RIGHT_MID, -8, 0);

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_name_row_click, LV_EVENT_CLICKED, NULL);
    }

    /* Device info */
    create_info_row(s_list, "Version",  UI_VERSION,           UI_COLOR_PRIMARY);
    create_info_row(s_list, "Build",    "ESP-IDF 5.5.2",      UI_COLOR_PRIMARY);
    create_info_row(s_list, "Hardware", "ESP32-S3 / T-Deck+", UI_COLOR_PRIMARY);

    /* Memory (live) */
    s_heap_val  = create_info_row(s_list, "Free Heap",  "...", UI_COLOR_SECONDARY);
    s_psram_val = create_info_row(s_list, "PSRAM Free", "...", UI_COLOR_SECONDARY);
    s_lvgl_val  = create_info_row(s_list, "LVGL Pool",  "...", UI_COLOR_SECONDARY);

    /* Network */
    create_info_row(s_list, "Server", "smp1.simplego.chat", UI_COLOR_PRIMARY);
    s_status_val = create_info_row(s_list, "Status", "...", UI_COLOR_PRIMARY);

    /* License */
    create_info_row(s_list, "License", "AGPL-3.0", UI_COLOR_TEXT_DIM);

    refresh_values();
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 2000, NULL);
}

void settings_cleanup_info_timers(void)
{
    if (s_refresh_timer) { lv_timer_del(s_refresh_timer); s_refresh_timer = NULL; }
    close_name_overlay();
}

void settings_nullify_info_pointers(void)
{
    s_list = NULL;
    s_heap_val = s_psram_val = s_lvgl_val = s_status_val = s_name_val = NULL;
}

void settings_info_refresh(void) { refresh_values(); }
