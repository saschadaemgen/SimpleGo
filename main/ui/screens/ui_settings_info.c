/**
 * @file ui_settings_info.c
 * @brief Settings Screen - INFO Tab (Device info, heap, server status)
 *
 * Session 39: Device information display with live memory values.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"
#include "smp_types.h"
#include "lvgl.h"

/* ================================================================
 * State
 * ================================================================ */

static lv_obj_t *s_heap_lbl   = NULL;
static lv_obj_t *s_psram_lbl  = NULL;
static lv_obj_t *s_lvgl_lbl   = NULL;
static lv_obj_t *s_status_lbl = NULL;
static lv_timer_t *s_refresh_timer = NULL;

/* ================================================================
 * Live Refresh
 * ================================================================ */

static void refresh_values(void)
{
    if (s_heap_lbl) {
        uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        lv_label_set_text_fmt(s_heap_lbl, "%lu bytes", (unsigned long)free_heap);
    }
    if (s_psram_lbl) {
        uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        lv_label_set_text_fmt(s_psram_lbl, "%lu bytes", (unsigned long)free_psram);
    }
    if (s_lvgl_lbl) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        lv_label_set_text_fmt(s_lvgl_lbl, "%lu / 65536 (%d%%)",
                              (unsigned long)mon.free_size, mon.used_pct);
    }
    if (s_status_lbl) {
        /* Use the global wifi_connected from smp_types.h */
        extern volatile bool wifi_connected;
        if (wifi_connected) {
            wifi_status_t ws = wifi_manager_get_status();
            lv_label_set_text_fmt(s_status_lbl, "%s " LV_SYMBOL_OK, ws.ssid);
            lv_obj_set_style_text_color(s_status_lbl, UI_COLOR_SECONDARY, 0);
        } else {
            lv_label_set_text(s_status_lbl, "Disconnected " LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(s_status_lbl, UI_COLOR_ERROR, 0);
        }
    }
}

static void refresh_timer_cb(lv_timer_t *t)
{
    (void)t;
    refresh_values();
}

/* ================================================================
 * Key-Value Row Helper
 * ================================================================ */

static lv_obj_t *create_kv_row(lv_obj_t *parent, const char *key,
                                const char *value, int y)
{
    /* Key label (dim, left) */
    lv_obj_t *k = lv_label_create(parent);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_color(k, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);
    lv_obj_set_pos(k, 20, y);

    /* Value label (white, right of key) */
    lv_obj_t *v = lv_label_create(parent);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_color(v, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
    lv_obj_set_pos(v, 120, y);

    return v;  /* Return value label for live updates */
}

/* ================================================================
 * Public Interface
 * ================================================================ */

void settings_create_info(lv_obj_t *parent)
{
    /* Branding */
    lv_obj_t *brand = lv_label_create(parent);
    lv_label_set_text(brand, "SimpleGo");
    lv_obj_set_style_text_color(brand, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(brand, UI_FONT, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *sub = lv_label_create(parent);
    lv_label_set_text(sub, "Secure Hardware Messenger");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_SM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 24);

    /* Device info rows */
    int y = 42;
    int row_h = 14;

    create_kv_row(parent, "Version",   UI_VERSION,          y); y += row_h;
    create_kv_row(parent, "Build",     "ESP-IDF 5.5.2",     y); y += row_h;
    create_kv_row(parent, "Hardware",  "ESP32-S3 / T-Deck+", y); y += row_h;

    y += 4; /* Small gap before memory section */
    s_heap_lbl  = create_kv_row(parent, "Free Heap",  "...", y); y += row_h;
    s_psram_lbl = create_kv_row(parent, "PSRAM Free", "...", y); y += row_h;
    s_lvgl_lbl  = create_kv_row(parent, "LVGL Pool",  "...", y); y += row_h;

    y += 4; /* Small gap before server section */
    create_kv_row(parent, "Server", "smp1.simplego.chat", y); y += row_h;
    s_status_lbl = create_kv_row(parent, "Status", "...", y);

    /* Footer */
    lv_obj_t *footer = lv_label_create(parent);
    lv_label_set_text(footer, "AGPL-3.0  |  github.com/cannatoshi/SimpleGo");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x304050), 0);
    lv_obj_set_style_text_font(footer, UI_FONT_SM, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* Initial values + 2s refresh timer */
    refresh_values();
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 2000, NULL);
}

void settings_cleanup_info_timers(void)
{
    if (s_refresh_timer) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
}

void settings_nullify_info_pointers(void)
{
    s_heap_lbl = s_psram_lbl = s_lvgl_lbl = s_status_lbl = NULL;
}

void settings_info_refresh(void)
{
    refresh_values();
}
