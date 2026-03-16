/**
 * @file ui_statusbar.c
 * @brief Shared status bar for all screens (26px)
 *
 * Pixel-art functions extracted from ui_main.c and ui_theme.c.
 * Two variants: FULL (clock+WiFi+battery) and CHAT (back+name+PQ).
 * One global LVGL timer updates live data every 10 seconds.
 *
 * Session 48: Created as part of Bug #30 status bar unification.
 * Reference design: Main screen combined bar (Session 40).
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_statusbar.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "smp_storage.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "STATUSBAR";

/* ================================================================
 * State - only one screen visible at a time, so single pointers
 * ================================================================ */

/* FULL variant widgets */
static lv_obj_t *s_title_lbl   = NULL;   /* Screen name (left) */
static lv_obj_t *s_time_lbl    = NULL;   /* Clock label "HH:MM" */
static lv_obj_t *s_wifi_bars[4] = {NULL}; /* 4 ascending bars */

/* CHAT variant widgets */
static lv_obj_t *s_chat_name_lbl = NULL;  /* Contact name */
static lv_obj_t *s_pq_lbl       = NULL;  /* PQ status label */

/* Variant tracking */
static bool s_is_chat = false;

/* Parent screen that owns current widgets.
 * All write functions check lv_scr_act() == s_bar_parent before
 * touching widgets. Prevents crashes when ephemeral screens are
 * destroyed but their pointers linger in the global statics. */
static lv_obj_t *s_bar_parent = NULL;

/* Global update timer */
static lv_timer_t *s_update_timer = NULL;

/** Guard: only touch widgets if owning screen is still active */
static bool statusbar_is_active(void)
{
    return s_bar_parent && (lv_scr_act() == s_bar_parent);
}

/* ================================================================
 * Pixel-Art Helpers (extracted from ui_main.c / ui_theme.c)
 * ================================================================ */

/** Tiny filled rectangle */
static lv_obj_t *px_rect(lv_obj_t *par, int w, int h,
                          lv_color_t c, lv_opa_t opa)
{
    lv_obj_t *r = lv_obj_create(par);
    lv_obj_set_size(r, w, h);
    lv_obj_set_style_bg_color(r, c, 0);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE);
    return r;
}

/* ================================================================
 * Battery Icon (placeholder - no ADC reading yet)
 * Matches Main screen: 20x11 body + 2x5 tip, cyan outlined
 * ================================================================ */

static lv_obj_t *draw_battery(lv_obj_t *bar)
{
    /* Tip (rightmost) */
    lv_obj_t *tip = px_rect(bar, 2, 5, UI_COLOR_PRIMARY, LV_OPA_COVER);
    lv_obj_align(tip, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Body: 20x11, outlined */
    lv_obj_t *body = lv_obj_create(bar);
    lv_obj_set_size(body, 20, 11);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(body, 1, 0);
    lv_obj_set_style_pad_all(body, 1, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(body, tip, LV_ALIGN_OUT_LEFT_MID, -1, 0);

    /* Fill: ~70% */
    lv_obj_t *fill = px_rect(body, 12, 7, UI_COLOR_PRIMARY, LV_OPA_COVER);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);

    return body;  /* Caller chains WiFi alignment to this */
}

/* ================================================================
 * WiFi Bars (4 ascending, live RSSI)
 * Matches Main screen: 15x11 container, bars 3px wide
 * ================================================================ */

static lv_obj_t *draw_wifi(lv_obj_t *bar, lv_obj_t *bat_body)
{
    /* Container: 19x12, holds 4 bars with uniform bottom alignment */
    lv_obj_t *wifi = lv_obj_create(bar);
    lv_obj_set_size(wifi, 19, 11);
    lv_obj_set_style_bg_opa(wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi, 0, 0);
    lv_obj_set_style_radius(wifi, 0, 0);
    lv_obj_set_style_pad_all(wifi, 0, 0);
    lv_obj_clear_flag(wifi, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(wifi, bat_body, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    /* Bars: 3px wide, heights 3/5/8/11, 5px pitch.
     * All bars bottom-aligned at row 11: y = 11 - height */
    static const int heights[] = {3, 5, 8, 11};
    for (int i = 0; i < 4; i++) {
        s_wifi_bars[i] = px_rect(wifi, 3, heights[i],
                                  UI_COLOR_PRIMARY, LV_OPA_COVER);
        lv_obj_set_pos(s_wifi_bars[i], i * 5, 11 - heights[i]);
    }

    return wifi;
}

/* ================================================================
 * WiFi RSSI Update
 * ================================================================ */

static void update_wifi_bars(void)
{
    wifi_ap_record_t ap_info;
    int bars = 0;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        if (ap_info.rssi >= -50)      bars = 4;
        else if (ap_info.rssi >= -65) bars = 3;
        else if (ap_info.rssi >= -75) bars = 2;
        else                          bars = 1;
    }

    for (int i = 0; i < 4; i++) {
        if (!s_wifi_bars[i]) continue;
        lv_obj_set_style_bg_opa(s_wifi_bars[i],
                                 (i < bars) ? LV_OPA_COVER : LV_OPA_30, 0);
    }
}

/* ================================================================
 * Clock Update (UTC + timezone offset for display)
 * ================================================================ */

static void update_clock(void)
{
    if (!s_time_lbl) return;

    time_t now = time(NULL);
    now += (int32_t)g_tz_offset_hours * 3600;  /* Display offset only */
    struct tm ti;
    gmtime_r(&now, &ti);

    char buf[8];
    if (ti.tm_year > 70) {
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    lv_label_set_text(s_time_lbl, buf);
}

/* ================================================================
 * Glow Line (1px, below status bar)
 * ================================================================ */

static void draw_glow(lv_obj_t *parent)
{
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_set_width(glow, LV_PCT(100));
    lv_obj_set_height(glow, 1);
    lv_obj_set_pos(glow, 0, UI_STATUSBAR_HEIGHT);
    lv_obj_set_style_bg_color(glow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(glow, (lv_opa_t)64, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_radius(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE);
}

/* ================================================================
 * FULL Variant: Screen name + Clock + WiFi + Battery
 * ================================================================ */

lv_obj_t *ui_statusbar_create(lv_obj_t *parent, const char *screen_name,
                               statusbar_widgets_t *out)
{
    s_is_chat = false;
    s_chat_name_lbl = NULL;
    s_pq_lbl = NULL;
    s_bar_parent = parent;

    /* Bar background (26px, black) */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUSBAR_HEIGHT);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Screen name (left, cyan) */
    s_title_lbl = lv_label_create(bar);
    lv_label_set_text(s_title_lbl, screen_name ? screen_name : "");
    lv_obj_set_style_text_color(s_title_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_title_lbl, UI_FONT, 0);
    lv_obj_align(s_title_lbl, LV_ALIGN_LEFT_MID, 6, 0);

    /* Battery (rightmost, returns body for alignment chain) */
    lv_obj_t *bat_body = draw_battery(bar);

    /* WiFi bars (left of battery body) */
    lv_obj_t *wifi = draw_wifi(bar, bat_body);

    /* Clock (left of WiFi, with breathing room) */
    s_time_lbl = lv_label_create(bar);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_time_lbl, UI_FONT_MD, 0);
    lv_obj_align_to(s_time_lbl, wifi, LV_ALIGN_OUT_LEFT_MID, -12, 0);

    /* Glow line */
    draw_glow(parent);

    /* Initial data */
    update_clock();
    update_wifi_bars();

    /* Fill output struct so permanent screens can keep local pointers */
    if (out) {
        out->title = s_title_lbl;
        out->time  = s_time_lbl;
        for (int i = 0; i < 4; i++) out->wifi_bars[i] = s_wifi_bars[i];
    }

    return s_title_lbl;
}

/* ================================================================
 * CHAT Variant: Back arrow + Contact name + PQ status
 * ================================================================ */

lv_obj_t *ui_statusbar_create_chat(lv_obj_t *parent,
                                    const char *contact_name,
                                    const char *pq_status,
                                    lv_color_t pq_color,
                                    lv_event_cb_t back_cb)
{
    s_is_chat = true;
    s_title_lbl = NULL;
    s_time_lbl = NULL;
    s_bar_parent = parent;
    for (int i = 0; i < 4; i++) s_wifi_bars[i] = NULL;

    /* Bar background (26px, black) */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUSBAR_HEIGHT);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button (left, 28px wide) */
    lv_obj_t *back_btn = lv_btn_create(bar);
    lv_obj_set_size(back_btn, 28, UI_STATUSBAR_HEIGHT);
    lv_obj_set_pos(back_btn, 0, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);

    lv_obj_t *arrow = lv_label_create(back_btn);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(arrow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(arrow, UI_FONT, 0);
    lv_obj_center(arrow);

    if (back_cb) {
        lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Contact name (left, after back button) */
    s_chat_name_lbl = lv_label_create(bar);
    lv_label_set_text(s_chat_name_lbl, contact_name ? contact_name : "");
    lv_label_set_long_mode(s_chat_name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_chat_name_lbl, 180);
    lv_obj_set_style_text_color(s_chat_name_lbl, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(s_chat_name_lbl, UI_FONT, 0);
    lv_obj_align(s_chat_name_lbl, LV_ALIGN_LEFT_MID, 30, 0);

    /* PQ status (right) */
    s_pq_lbl = lv_label_create(bar);
    lv_label_set_text(s_pq_lbl, pq_status ? pq_status : "E2EE");
    lv_obj_set_style_text_color(s_pq_lbl, pq_color, 0);
    lv_obj_set_style_text_font(s_pq_lbl, UI_FONT_SM, 0);
    lv_obj_align(s_pq_lbl, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Glow line */
    draw_glow(parent);

    return s_chat_name_lbl;
}

/* ================================================================
 * Public Update Functions
 * ================================================================ */

void ui_statusbar_update(void)
{
    if (!statusbar_is_active()) return;
    if (s_is_chat) return;  /* CHAT variant has no live data */
    update_clock();
    update_wifi_bars();
}

void ui_statusbar_set_title(const char *title)
{
    if (!statusbar_is_active()) return;
    if (s_title_lbl && title) {
        lv_label_set_text(s_title_lbl, title);
    }
}

void ui_statusbar_set_pq_status(const char *status, lv_color_t color)
{
    if (!statusbar_is_active()) return;
    if (s_pq_lbl && status) {
        lv_label_set_text(s_pq_lbl, status);
        lv_obj_set_style_text_color(s_pq_lbl, color, 0);
    }
}

void ui_statusbar_set_contact_name(const char *name)
{
    if (!statusbar_is_active()) return;
    if (s_chat_name_lbl && name) {
        char trunc[32];
        size_t len = strlen(name);
        if (len > 25) {
            memcpy(trunc, name, 22);
            trunc[22] = '.';
            trunc[23] = '.';
            trunc[24] = '.';
            trunc[25] = '\0';
        } else {
            strncpy(trunc, name, sizeof(trunc) - 1);
            trunc[sizeof(trunc) - 1] = '\0';
        }
        lv_label_set_text(s_chat_name_lbl, trunc);
    }
}

/* ================================================================
 * Global Update Timer (10 seconds)
 * ================================================================ */

static void statusbar_timer_cb(lv_timer_t *t)
{
    (void)t;

    /* Don't update if lock screen active */
    if (ui_manager_get_current() == UI_SCREEN_LOCK) return;

    ui_statusbar_update();
}

void ui_statusbar_timer_start(void)
{
    if (s_update_timer) return;  /* Already running */
    s_update_timer = lv_timer_create(statusbar_timer_cb, 10000, NULL);
    ESP_LOGI(TAG, "Update timer started (10s interval)");
}

void ui_statusbar_timer_stop(void)
{
    if (s_update_timer) {
        lv_timer_delete(s_update_timer);
        s_update_timer = NULL;
        ESP_LOGI(TAG, "Update timer stopped");
    }
}
