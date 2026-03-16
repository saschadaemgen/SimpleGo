/**
 * @file ui_settings_info.c
 * @brief Settings Screen - INFO Tab (Four-column dual-row design)
 *
 * Session 47: Rewritten to four-column layout (two key-value pairs per row).
 * Security section with NVS mode, eFuse status, PQ toggle.
 * eFuse status read from chip at runtime, not compile-time.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"
#include "smp_types.h"
#include "smp_storage.h"
#include "smp_ratchet.h"     // Session 47: PQ toggle (get/set pq_enabled)
#include "esp_efuse.h"        // Session 47: Runtime eFuse status
#include "lvgl.h"

/* Row dimensions */
#define ROW_H           24
#define ROW_GAP         2
#define ROW_BG          lv_color_hex(0x000810)
#define ACCENT_W        3
#define ACCENT_H        16
#define BRAND_H         28

/* Four-column positions (two key-value pairs per row) */
#define L_KEY_X         14      /* Left key */
#define L_VAL_X         78      /* Left value */
#define R_KEY_X         174     /* Right key */
#define R_VAL_X         238     /* Right value */
#define R_ACCENT_X      166     /* Right accent bar */

/* ================================================================
 * State
 * ================================================================ */

static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_heap_val     = NULL;
static lv_obj_t *s_psram_val    = NULL;
static lv_obj_t *s_lvgl_val     = NULL;
static lv_obj_t *s_wifi_val     = NULL;   /* WiFi signal strength */
static lv_obj_t *s_name_val     = NULL;
static lv_obj_t *s_brand_pq     = NULL;   /* PQ status in branding row (clickable) */
static lv_timer_t *s_refresh_timer = NULL;

/* Name editor overlay */
static lv_obj_t *s_name_overlay    = NULL;
static lv_obj_t *s_name_ta         = NULL;
static lv_group_t *s_name_group    = NULL;
static lv_group_t *s_name_prev_group = NULL;

/* ================================================================
 * Helpers
 * ================================================================ */

static bool is_efuse_burned(void)
{
    esp_efuse_purpose_t purpose = esp_efuse_get_key_purpose(EFUSE_BLK_KEY1);
    return (purpose != ESP_EFUSE_KEY_PURPOSE_USER);
}

static void refresh_values(void)
{
    if (s_name_val) {
        char name[32];
        storage_get_display_name(name, sizeof(name));
        lv_label_set_text(s_name_val, name);
    }
    if (s_heap_val) {
        uint32_t fh = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        lv_label_set_text_fmt(s_heap_val, "%luB", (unsigned long)fh);
    }
    if (s_psram_val) {
        uint32_t fp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        lv_label_set_text_fmt(s_psram_val, "%luKB", (unsigned long)(fp / 1024));
    }
    if (s_lvgl_val) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        lv_label_set_text_fmt(s_lvgl_val, "%d%%", mon.used_pct);
    }
    if (s_wifi_val) {
        if (wifi_connected) {
            wifi_status_t ws = wifi_manager_get_status();
            lv_label_set_text_fmt(s_wifi_val, "%ddBm", ws.rssi);
            lv_obj_set_style_text_color(s_wifi_val,
                ws.rssi > -60 ? UI_COLOR_PRIMARY : UI_COLOR_PRIMARY, 0);
        } else {
            lv_label_set_text(s_wifi_val, "Offline");
            lv_obj_set_style_text_color(s_wifi_val, UI_COLOR_PRIMARY, 0);
        }
    }
    /* Update PQ status in branding row */
    if (s_brand_pq) {
        uint8_t pq = smp_settings_get_pq_enabled();
        if (pq) {
            lv_label_set_text(s_brand_pq, "E2EE Quantum-Resistant");
        } else {
            lv_label_set_text(s_brand_pq, "E2EE Standard");
        }
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh_values(); }

/* ================================================================
 * Name Editor Overlay (Session 43 pattern, unchanged)
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

    lv_obj_t *title = lv_label_create(s_name_overlay);
    lv_label_set_text(title, "Display Name");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_SM, 0);
    lv_obj_set_pos(title, 20, 24);

    lv_obj_t *hint = lv_label_create(s_name_overlay);
    lv_label_set_text(hint, "Visible to your contacts");
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_set_pos(hint, 20, 42);

    lv_obj_t *sep = lv_obj_create(s_name_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 62);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

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

    char current_name[32];
    storage_get_display_name(current_name, sizeof(current_name));
    lv_textarea_set_text(s_name_ta, current_name);

    lv_obj_t *save_btn = lv_label_create(s_name_overlay);
    lv_label_set_text(save_btn, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(save_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(save_btn, UI_FONT, 0);
    lv_obj_set_pos(save_btn, 20, 124);
    lv_obj_add_flag(save_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(save_btn, 10);
    lv_obj_add_event_cb(save_btn, on_name_save, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel = lv_label_create(s_name_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_name_cancel, LV_EVENT_CLICKED, NULL);

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

/* ================================================================
 * PQ Toggle Event
 * ================================================================ */

static void on_pq_toggle(lv_event_t *e)
{
    (void)e;
    uint8_t current = smp_settings_get_pq_enabled();
    uint8_t next = current ? 0 : 1;
    smp_settings_set_pq_enabled(next);
    ESP_LOGI("UI_INFO", "Post-Quantum %s", next ? "ON" : "OFF");
    refresh_values();
}

/* ================================================================
 * Row Builders
 * ================================================================ */

/* Create a base row container */
static lv_obj_t *create_row_base(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return row;
}

/* Create accent bar at given x position */
static void create_accent(lv_obj_t *row, int x, lv_color_t clr)
{
    lv_obj_t *a = lv_obj_create(row);
    lv_obj_set_size(a, ACCENT_W, ACCENT_H);
    lv_obj_set_pos(a, x, (ROW_H - ACCENT_H) / 2);
    lv_obj_set_style_bg_color(a, clr, 0);
    lv_obj_set_style_bg_opa(a, (lv_opa_t)180, 0);
    lv_obj_set_style_radius(a, 1, 0);
    lv_obj_set_style_border_width(a, 0, 0);
    lv_obj_set_style_pad_all(a, 0, 0);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

/* Create key label */
static void create_key(lv_obj_t *row, int x, const char *text)
{
    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, text);
    lv_obj_set_style_text_color(k, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, x, 0);
}

/* Create value label */
static lv_obj_t *create_val(lv_obj_t *row, int x, const char *text, lv_color_t clr)
{
    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, text);
    lv_obj_set_style_text_color(v, clr, 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
    lv_obj_align(v, LV_ALIGN_LEFT_MID, x, 0);
    return v;
}

/* Full dual-column row: left pair + right pair */
static void create_dual_row(lv_obj_t *parent,
                             const char *lkey, const char *lval, lv_color_t lclr,
                             const char *rkey, const char *rval, lv_color_t rclr,
                             lv_obj_t **lval_out, lv_obj_t **rval_out)
{
    lv_obj_t *row = create_row_base(parent);

    /* Left side */
    create_accent(row, 6, lclr);
    create_key(row, L_KEY_X, lkey);
    lv_obj_t *lv = create_val(row, L_VAL_X, lval, UI_COLOR_PRIMARY);
    if (lval_out) *lval_out = lv;

    /* Right side */
    create_accent(row, R_ACCENT_X, rclr);
    create_key(row, R_KEY_X, rkey);
    lv_obj_t *rv = create_val(row, R_VAL_X, rval, UI_COLOR_PRIMARY);
    if (rval_out) *rval_out = rv;
}

/* ================================================================
 * Screen Build
 * ================================================================ */

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

    /* === Row 0: Name (clickable) + PQ status === */
    {
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_size(row, LV_PCT(100), BRAND_H);
        lv_obj_set_style_bg_color(row, ROW_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x001420), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_name_row_click, LV_EVENT_CLICKED, NULL);

        char name[32];
        storage_get_display_name(name, sizeof(name));
        s_name_val = lv_label_create(row);
        lv_label_set_text(s_name_val, name);
        lv_obj_set_style_text_color(s_name_val, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(s_name_val, UI_FONT, 0);
        lv_obj_align(s_name_val, LV_ALIGN_LEFT_MID, 14, 0);

        lv_obj_t *edit = lv_label_create(row);
        lv_label_set_text(edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(edit, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(edit, UI_FONT_SM, 0);
        lv_obj_set_pos(edit, 120, (BRAND_H - 10) / 2);

        s_brand_pq = lv_label_create(row);
        lv_obj_set_style_text_font(s_brand_pq, UI_FONT_SM, 0);
        lv_obj_set_style_text_color(s_brand_pq, UI_COLOR_PRIMARY, 0);
        lv_obj_align(s_brand_pq, LV_ALIGN_RIGHT_MID, -20, 0);
        lv_obj_add_flag(s_brand_pq, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_brand_pq, 10);
        lv_obj_add_event_cb(s_brand_pq, on_pq_toggle, LV_EVENT_CLICKED, NULL);

        /* Edit icon - same style as Name edit (dim, small) */
        lv_obj_t *pq_edit = lv_label_create(row);
        lv_label_set_text(pq_edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(pq_edit, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(pq_edit, UI_FONT_SM, 0);
        lv_obj_align(pq_edit, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_flag(pq_edit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(pq_edit, 10);
        lv_obj_add_event_cb(pq_edit, on_pq_toggle, LV_EVENT_CLICKED, NULL);
        /* Color and text set in refresh_values() */
    }

    /* === Row 1: Version | Hardware === */
    create_dual_row(s_list,
        "Version", "0.1.17-a", UI_COLOR_PRIMARY,
        "Hardware", "ESP32-S3", UI_COLOR_PRIMARY,
        NULL, NULL);

    /* === Row 2: Build | NVS Mode === */
    {
        bool vault =
#if defined(CONFIG_NVS_ENCRYPTION)
            true;
#else
            false;
#endif

        lv_obj_t *row = create_row_base(s_list);
        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "Build");
        create_val(row, L_VAL_X, "IDF 5.5.2", UI_COLOR_PRIMARY);
        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "NVS");
        create_val(row, R_VAL_X, vault ? "VAULT" : "OPEN",
                   vault ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM);
    }

    /* === Row 3: eFuse Status | License === */
    {
        bool burned = is_efuse_burned();

        lv_obj_t *row = create_row_base(s_list);

        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "eFuse");
        create_val(row, L_VAL_X, burned ? "BURNED" : "NONE",
                   burned ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM);

        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "License");
        create_val(row, R_VAL_X, "AGPL-3.0", UI_COLOR_TEXT_DIM);
    }

    /* === Row 4: WiFi Signal | Heap === */
    create_dual_row(s_list,
        "WiFi", "...", UI_COLOR_PRIMARY,
        "Heap", "...", UI_COLOR_PRIMARY,
        &s_wifi_val, &s_heap_val);

    /* === Row 5: PSRAM | LVGL === */
    create_dual_row(s_list,
        "PSRAM", "...", UI_COLOR_PRIMARY,
        "LVGL", "...", UI_COLOR_PRIMARY,
        &s_psram_val, &s_lvgl_val);

    /* === Row 6: Server === */
    {
        lv_obj_t *row = create_row_base(s_list);
        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "Server");
        create_val(row, L_VAL_X, "simplego", UI_COLOR_PRIMARY);
    }

    /* Initial refresh + timer */
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
    s_heap_val = s_psram_val = s_lvgl_val = s_wifi_val = s_name_val = NULL;
    s_brand_pq = NULL;
}

void settings_info_refresh(void) { refresh_values(); }
