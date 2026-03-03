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
static lv_timer_t *s_refresh_timer = NULL;

static void refresh_values(void)
{
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
        extern volatile bool wifi_connected;
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
}

void settings_nullify_info_pointers(void)
{
    s_list = NULL;
    s_heap_val = s_psram_val = s_lvgl_val = s_status_val = NULL;
}

void settings_info_refresh(void) { refresh_values(); }
