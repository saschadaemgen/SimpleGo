/**
 * @file ui_settings_wifi.c
 * @brief Settings Screen - WIFI Tab (Scan, Connect, Password Overlay)
 *
 * Session 39c: Extracted from monolithic ui_settings.c
 * Session 39d: Mausi Fixes - lv_async_call, object reduction, timing
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ================================================================
 * WiFi Row Style (same as contact rows)
 * ================================================================ */

#define ROW_H_NET       28
#define ROW_GAP_NET     2
#define ROW_BG          lv_color_hex(0x000810)
#define ROW_BG_PRESS    lv_color_hex(0x001420)
#define WIFI_MAX_APS    6       /* Session 39d: reduced from 10 to save LVGL objects */

/* ================================================================
 * State
 * ================================================================ */

static lv_obj_t *s_wifi_list         = NULL;
static lv_obj_t *s_wifi_empty_hint   = NULL;
static lv_timer_t *s_wifi_scan_timer = NULL;
static lv_timer_t *s_wifi_conn_timer = NULL;
static bool s_wifi_was_connected     = false;
static char s_wifi_was_ssid[33]      = {0};

static char s_selected_ssid[33]      = {0};
static bool s_selected_is_connected  = false;
static wifi_ap_record_t s_scan_results[WIFI_MAX_APS];
static uint16_t s_scan_count         = 0;
static volatile bool s_scan_in_progress = false;

/* Password overlay */
static lv_obj_t *s_pass_overlay     = NULL;
static lv_obj_t *s_pass_ta          = NULL;
static lv_group_t *s_pass_group     = NULL;
static lv_group_t *s_pass_prev_group = NULL;

/* Animation state for scanning/connecting */
static lv_timer_t *s_anim_timer     = NULL;
static int s_anim_phase             = 0;
static bool s_connecting_in_progress = false;
static char s_connecting_ssid[33]   = {0};

/* ================================================================
 * Forward Declarations
 * ================================================================ */

static void on_wifi_scan(void);
static void show_password_overlay(void);
static void close_password_overlay(void);
static void on_wifi_select(lv_event_t *e);
static void wifi_scan_poll_cb(lv_timer_t *t);
static void wifi_conn_poll_cb(lv_timer_t *t);
static void anim_dots_cb(lv_timer_t *t);

/* ================================================================
 * Deferred Tab Rebuild (Session 39d Fix 1: avoids re-entrancy)
 *
 * CRITICAL: Never call settings_switch_tab() from inside a
 * timer callback or event handler - it deletes timers from
 * within their own callback, causing undefined behavior.
 * Always use lv_async_call(deferred_wifi_rebuild, NULL).
 * ================================================================ */

static lv_timer_t *s_rebuild_timer = NULL;

static void rebuild_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_rebuild_timer = NULL;
    /* Guard: If WiFi tab is not displayed (e.g. Name Setup screen after
     * erase-flash), s_wifi_list is NULL. Skip rebuild to avoid crash. */
    if (!s_wifi_list) return;
    /* Session 39k: Don't rebuild while scan is running. Rebuild calls
     * cleanup which resets s_scan_in_progress, so scan results would
     * never be displayed. Reschedule and try again in 300ms. */
    if (s_scan_in_progress) {
        s_rebuild_timer = lv_timer_create(rebuild_timer_cb, 300, NULL);
        lv_timer_set_repeat_count(s_rebuild_timer, 1);
        return;
    }
    settings_switch_tab(TAB_WIFI);
}

static void deferred_wifi_rebuild(void *data)
{
    (void)data;
    /* Guard: WiFi tab not displayed, don't schedule rebuild */
    if (!s_wifi_list) return;
    /* Session 39k: Give WiFi stack 200ms to stabilize status after
     * connect/disconnect. Original used vTaskDelay which BLOCKED the
     * LVGL task for 200ms causing UI freeze. Now uses a one-shot LVGL
     * timer -- non-blocking, fires once then auto-deletes. */
    if (s_rebuild_timer) return;  /* Already pending */
    s_rebuild_timer = lv_timer_create(rebuild_timer_cb, 200, NULL);
    lv_timer_set_repeat_count(s_rebuild_timer, 1);
}

/* ================================================================
 * Animated Chevrons (Scanning / Connecting)
 * ================================================================ */

static const char *s_anim_frames[] = {
    ">",
    ">>",
    ">>>",
    ">>",
};
#define ANIM_FRAME_COUNT 4

static void start_anim(const char *title)
{
    s_anim_phase = 0;
    if (hdr_title_lbl) lv_label_set_text(hdr_title_lbl, title);
    if (hdr_action_lbl) {
        lv_label_set_text(hdr_action_lbl, s_anim_frames[0]);
        lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_WARNING, 0);
    }
    if (hdr_action_btn) lv_obj_clear_flag(hdr_action_btn, LV_OBJ_FLAG_HIDDEN);

    if (!s_anim_timer) {
        s_anim_timer = lv_timer_create(anim_dots_cb, 300, NULL);
    }
}

static void stop_anim(void)
{
    if (s_anim_timer) {
        lv_timer_del(s_anim_timer);
        s_anim_timer = NULL;
    }
    s_anim_phase = 0;
}

static void anim_dots_cb(lv_timer_t *t)
{
    (void)t;
    s_anim_phase = (s_anim_phase + 1) % ANIM_FRAME_COUNT;
    if (hdr_action_lbl) {
        lv_label_set_text(hdr_action_lbl, s_anim_frames[s_anim_phase]);
    }
}

/* ================================================================
 * Shared Row Creator (Session 39d Fix 2: reduced objects per row)
 *
 * Each row now has exactly 2 children: SSID label + RSSI label.
 * No accent bar, no separate check label.
 * Connected = green SSID with embedded checkmark symbol.
 * Selected = cyan SSID. Normal = dim gray.
 * ================================================================ */

static lv_obj_t *create_wifi_row(lv_obj_t *parent, const char *ssid,
                                  int rssi, bool is_connected, int user_idx)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H_NET);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, ROW_BG_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Child 0: SSID label (with embedded checkmark if connected) */
    lv_obj_t *ssid_lbl = lv_label_create(row);
    char ssid_text[36];
    if (is_connected) {
        snprintf(ssid_text, sizeof(ssid_text), "%.24s " LV_SYMBOL_OK, ssid);
    } else {
        snprintf(ssid_text, sizeof(ssid_text), "%.24s", ssid);
    }
    lv_label_set_text(ssid_lbl, ssid_text);
    lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(ssid_lbl, UI_FONT, 0);
    lv_obj_set_style_text_color(ssid_lbl,
        is_connected ? UI_COLOR_SECONDARY : lv_color_hex(0x607878), 0);
    lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    /* Child 1: RSSI label */
    lv_obj_t *rssi_lbl = lv_label_create(row);
    char rssi_text[12];
    snprintf(rssi_text, sizeof(rssi_text), "%ddBm", rssi);
    lv_label_set_text(rssi_lbl, rssi_text);
    lv_obj_set_style_text_font(rssi_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(rssi_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(rssi_lbl, LV_ALIGN_RIGHT_MID, -2, 0);

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(row, (void *)(intptr_t)user_idx);
    lv_obj_add_event_cb(row, on_wifi_select, LV_EVENT_CLICKED, NULL);

    return row;
}

/* ================================================================
 * Network Row Selection (Session 39d Fix 2: highlight via text color)
 * ================================================================ */

static void on_wifi_select(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(row);

    /* Check if this is the connected-network row (user_data = -1) */
    if (idx == -1) {
        /* Clicking the connected network -> prepare DISCONNECT */
        wifi_status_t ws = wifi_manager_get_status();
        if (ws.connected) {
            strncpy(s_selected_ssid, ws.ssid, 32);
            s_selected_ssid[32] = '\0';
            s_selected_is_connected = true;
            settings_update_header_for_tab();
        }
        return;
    }

    if (idx < 0 || idx >= s_scan_count) return;

    strncpy(s_selected_ssid, (char *)s_scan_results[idx].ssid, 32);
    s_selected_ssid[32] = '\0';

    /* Check if this scanned SSID is the connected one */
    wifi_status_t ws = wifi_manager_get_status();
    s_selected_is_connected = ws.connected &&
        (strcmp(s_selected_ssid, ws.ssid) == 0);

    /* Highlight selected row by SSID text color (no accent bars) */
    if (s_wifi_list) {
        uint32_t cnt = lv_obj_get_child_count(s_wifi_list);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(s_wifi_list, i);
            if (child == s_wifi_empty_hint) continue;
            int row_idx = (int)(intptr_t)lv_obj_get_user_data(child);
            /* Child 0 = SSID label */
            lv_obj_t *ssid_lbl = lv_obj_get_child(child, 0);
            if (!ssid_lbl) continue;
            if (row_idx == idx) {
                /* Selected: cyan */
                lv_obj_set_style_text_color(ssid_lbl, UI_COLOR_PRIMARY, 0);
            } else if (row_idx == -1) {
                /* Connected row stays green */
                lv_obj_set_style_text_color(ssid_lbl, UI_COLOR_SECONDARY, 0);
            } else {
                /* Normal: dim */
                lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0x607878), 0);
            }
        }
    }

    settings_update_header_for_tab();
}

/* ================================================================
 * Header Action (SCAN / CONNECT / DISCONNECT)
 * ================================================================ */

void settings_on_wifi_hdr_action(void)
{
    if (active_tab != TAB_WIFI) return;

    if (s_selected_ssid[0] && s_selected_is_connected) {
        /* DISCONNECT */
        ESP_LOGI("UI_SET", "Disconnecting from '%s'", s_selected_ssid);
        wifi_manager_disconnect();
        s_selected_ssid[0] = '\0';
        s_selected_is_connected = false;
        /* Session 39d Fix 1: deferred rebuild via lv_async_call */
        lv_async_call(deferred_wifi_rebuild, NULL);
    } else if (s_selected_ssid[0]) {
        /* CONNECT -> show password overlay */
        show_password_overlay();
    } else {
        /* SCAN */
        on_wifi_scan();
    }
}

void settings_update_wifi_header(void)
{
    wifi_status_t ws = wifi_manager_get_status();
    if (s_selected_ssid[0]) {
        lv_label_set_text(hdr_title_lbl, s_selected_ssid);
    } else if (ws.connected) {
        char buf[36];
        snprintf(buf, sizeof(buf), "%.24s " LV_SYMBOL_OK, ws.ssid);
        lv_label_set_text(hdr_title_lbl, buf);
    } else {
        lv_label_set_text(hdr_title_lbl, "Networks");
    }
    lv_obj_clear_flag(hdr_action_btn, LV_OBJ_FLAG_HIDDEN);
    if (s_selected_ssid[0] && s_selected_is_connected) {
        /* Selected the connected network -> DISCONNECT */
        lv_label_set_text(hdr_action_lbl, "DISCONNECT");
        lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_ERROR, 0);
    } else if (s_selected_ssid[0]) {
        /* Selected a different network -> CONNECT */
        lv_label_set_text(hdr_action_lbl, "CONNECT");
        lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_SECONDARY, 0);
    } else {
        lv_label_set_text(hdr_action_lbl, "SCAN");
        lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_PRIMARY, 0);
    }
}

/* ================================================================
 * WiFi Scan
 * ================================================================ */

static void on_wifi_scan(void)
{
    if (s_scan_in_progress) return;
    s_scan_in_progress = true;
    s_selected_ssid[0] = '\0';
    s_selected_is_connected = false;

    start_anim("Scanning");

    wifi_manager_start_scan();
}

/* Scan poll timer */
static void wifi_scan_poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_scan_in_progress) return;
    if (!wifi_manager_is_scan_done()) return;
    s_scan_in_progress = false;
    stop_anim();

    /* Session 39k Fix: Use cached backend results instead of direct ESP-IDF
     * API. wifi_manager.c caches results in SCAN_DONE handler BEFORE anything
     * can call esp_wifi_connect() and clear the buffer. Old code called
     * esp_wifi_scan_get_ap_records() directly -- race with event handler. */
    uint16_t backend_count = wifi_manager_get_scan_count();
    const wifi_ap_record_t *backend_aps = wifi_manager_get_scan_results();
    s_scan_count = (backend_count < WIFI_MAX_APS) ? backend_count : WIFI_MAX_APS;
    if (s_scan_count > 0) {
        memcpy(s_scan_results, backend_aps, s_scan_count * sizeof(wifi_ap_record_t));
    }

    /* Update header */
    if (s_scan_count > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Networks (%d)", s_scan_count);
        if (hdr_title_lbl) lv_label_set_text(hdr_title_lbl, buf);
    } else {
        if (hdr_title_lbl) lv_label_set_text(hdr_title_lbl, "No networks");
    }
    if (hdr_action_lbl) {
        lv_label_set_text(hdr_action_lbl, "SCAN");
        lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_PRIMARY, 0);
    }

    if (!s_wifi_list) return;

    /* Remove old rows (keep empty hint) */
    uint32_t cnt = lv_obj_get_child_count(s_wifi_list);
    for (int i = cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(s_wifi_list, i);
        if (child == s_wifi_empty_hint) continue;
        lv_obj_delete(child);
    }

    if (s_wifi_empty_hint) lv_obj_add_flag(s_wifi_empty_hint, LV_OBJ_FLAG_HIDDEN);

    wifi_status_t ws = wifi_manager_get_status();

    for (int i = 0; i < s_scan_count; i++) {
        bool is_connected = ws.connected &&
            (strcmp((char *)s_scan_results[i].ssid, ws.ssid) == 0);

        create_wifi_row(s_wifi_list, (char *)s_scan_results[i].ssid,
                        s_scan_results[i].rssi, is_connected, i);
    }
}

/* ================================================================
 * Connection Status Poll (Session 39d Fix 1+3: deferred + SSID tracking)
 * ================================================================ */

static void wifi_conn_poll_cb(lv_timer_t *t)
{
    (void)t;
    /* Don't interfere while scan is running - ESP-IDF channel-hops
     * during scan, which can briefly make us appear disconnected */
    if (s_scan_in_progress) return;
    wifi_status_t ws = wifi_manager_get_status();
    bool now_connected = ws.connected;
    bool state_changed = (now_connected != s_wifi_was_connected);

    /* Detect network SWITCH: both connected but different SSID */
    bool ssid_changed = false;
    if (now_connected && s_wifi_was_connected) {
        ssid_changed = (strcmp(ws.ssid, s_wifi_was_ssid) != 0);
    }

    if (state_changed || ssid_changed) {
        s_wifi_was_connected = now_connected;
        if (now_connected) {
            strncpy(s_wifi_was_ssid, ws.ssid, 32);
            s_wifi_was_ssid[32] = '\0';
        } else {
            s_wifi_was_ssid[0] = '\0';
        }

        ESP_LOGI("UI_SET", "WiFi state changed: %s%s",
                 now_connected ? "connected to " : "disconnected",
                 now_connected ? ws.ssid : "");

        /* Stop any running animation */
        stop_anim();
        s_connecting_in_progress = false;
        s_connecting_ssid[0] = '\0';

        /* Session 39d Fix 1: NEVER call settings_switch_tab() here!
         * We are inside a timer callback - deferred rebuild avoids
         * deleting this timer from its own callback (re-entrancy). */
        lv_async_call(deferred_wifi_rebuild, NULL);
    }
    /* Also detect connect completion while animating */
    else if (s_connecting_in_progress && now_connected) {
        if (strcmp(ws.ssid, s_connecting_ssid) == 0) {
            stop_anim();
            s_connecting_in_progress = false;
            s_connecting_ssid[0] = '\0';
            strncpy(s_wifi_was_ssid, ws.ssid, 32);
            s_wifi_was_ssid[32] = '\0';
            /* Session 39d Fix 1: deferred rebuild */
            lv_async_call(deferred_wifi_rebuild, NULL);
        }
    }
}

/* ================================================================
 * Password Overlay (contacts popup style)
 * ================================================================ */

static void on_pass_connect(lv_event_t *e)
{
    (void)e;
    const char *pass = "";
    if (s_pass_ta) {
        pass = lv_textarea_get_text(s_pass_ta);
        if (!pass) pass = "";
    }

    ESP_LOGI("UI_SET", "Connecting to '%s'", s_selected_ssid);
    wifi_manager_connect(s_selected_ssid, pass);
    close_password_overlay();

    /* Start connecting animation */
    strncpy(s_connecting_ssid, s_selected_ssid, 32);
    s_connecting_ssid[32] = '\0';
    s_connecting_in_progress = true;

    char title_buf[36];
    snprintf(title_buf, sizeof(title_buf), "%.24s", s_selected_ssid);
    start_anim(title_buf);
}

static void on_pass_cancel(lv_event_t *e)
{
    (void)e;
    close_password_overlay();
}

static void show_password_overlay(void)
{
    if (s_pass_overlay) close_password_overlay();

    /* Fullscreen overlay (same pattern as contacts popup) */
    s_pass_overlay = lv_obj_create(s_settings_scr);
    lv_obj_set_size(s_pass_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_pass_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_pass_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_pass_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pass_overlay, 0, 0);
    lv_obj_set_style_radius(s_pass_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_pass_overlay, 0, 0);
    lv_obj_clear_flag(s_pass_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title: "Connect to" */
    lv_obj_t *title = lv_label_create(s_pass_overlay);
    lv_label_set_text(title, "Connect to");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_SM, 0);
    lv_obj_set_pos(title, 20, 24);

    /* SSID name (large) */
    lv_obj_t *name = lv_label_create(s_pass_overlay);
    lv_label_set_text(name, s_selected_ssid);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 280);
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, UI_FONT, 0);
    lv_obj_set_pos(name, 20, 42);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(s_pass_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 68);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Password label */
    lv_obj_t *pass_lbl = lv_label_create(s_pass_overlay);
    lv_label_set_text(pass_lbl, "Password");
    lv_obj_set_style_text_color(pass_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(pass_lbl, UI_FONT_SM, 0);
    lv_obj_set_pos(pass_lbl, 20, 80);

    /* Password textarea */
    s_pass_ta = lv_textarea_create(s_pass_overlay);
    lv_obj_set_size(s_pass_ta, 280, 32);
    lv_obj_set_pos(s_pass_ta, 20, 96);
    lv_textarea_set_one_line(s_pass_ta, true);
    lv_textarea_set_password_mode(s_pass_ta, true);
    lv_textarea_set_placeholder_text(s_pass_ta, "Enter password...");
    lv_textarea_set_max_length(s_pass_ta, 63);
    lv_obj_set_style_bg_color(s_pass_ta, ROW_BG, 0);
    lv_obj_set_style_bg_opa(s_pass_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_pass_ta, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_pass_ta, 1, 0);
    lv_obj_set_style_border_opa(s_pass_ta, (lv_opa_t)60, 0);
    lv_obj_set_style_text_color(s_pass_ta, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(s_pass_ta, UI_FONT, 0);
    lv_obj_set_style_radius(s_pass_ta, 4, 0);
    lv_obj_set_style_pad_left(s_pass_ta, 8, 0);

    /* Connect button */
    lv_obj_t *conn_btn = lv_label_create(s_pass_overlay);
    lv_label_set_text(conn_btn, LV_SYMBOL_WIFI " Connect");
    lv_obj_set_style_text_color(conn_btn, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(conn_btn, UI_FONT, 0);
    lv_obj_set_pos(conn_btn, 20, 144);
    lv_obj_add_flag(conn_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(conn_btn, 10);
    lv_obj_add_event_cb(conn_btn, on_pass_connect, LV_EVENT_CLICKED, NULL);

    /* Cancel */
    lv_obj_t *cancel = lv_label_create(s_pass_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_pass_cancel, LV_EVENT_CLICKED, NULL);

    /* Keyboard focus to password field */
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev) {
        s_pass_prev_group = lv_indev_get_group(kb_indev);
        s_pass_group = lv_group_create();
        lv_group_add_obj(s_pass_group, s_pass_ta);
        lv_indev_set_group(kb_indev, s_pass_group);
        lv_group_focus_obj(s_pass_ta);
        lv_obj_add_state(s_pass_ta, LV_STATE_FOCUSED);
    }
}

static void close_password_overlay(void)
{
    /* Restore keyboard */
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev && s_pass_prev_group) {
        lv_indev_set_group(kb_indev, s_pass_prev_group);
    }
    if (s_pass_group) {
        lv_group_delete(s_pass_group);
        s_pass_group = NULL;
    }
    s_pass_prev_group = NULL;
    s_pass_ta = NULL;

    if (s_pass_overlay) {
        lv_obj_delete(s_pass_overlay);
        s_pass_overlay = NULL;
    }
}

/* ================================================================
 * Public Interface
 * ================================================================ */

void settings_create_wifi(lv_obj_t *parent)
{
    s_selected_ssid[0] = '\0';
    s_selected_is_connected = false;

    /* Scrollable list */
    s_wifi_list = lv_obj_create(parent);
    lv_obj_set_width(s_wifi_list, LV_PCT(100));
    lv_obj_set_height(s_wifi_list, CONTENT_H);
    lv_obj_set_pos(s_wifi_list, 0, 0);
    settings_style_black(s_wifi_list);
    lv_obj_set_style_pad_top(s_wifi_list, 4, 0);
    lv_obj_set_style_pad_bottom(s_wifi_list, 2, 0);
    lv_obj_set_style_pad_row(s_wifi_list, ROW_GAP_NET, 0);
    lv_obj_set_flex_flow(s_wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_wifi_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_wifi_list, LV_SCROLLBAR_MODE_OFF);

    /* Show current connected network (Session 39d: using shared row creator) */
    wifi_status_t ws = wifi_manager_get_status();
    if (ws.connected) {
        create_wifi_row(s_wifi_list, ws.ssid, ws.rssi, true, -1);
    }

    /* Empty hint */
    s_wifi_empty_hint = lv_label_create(s_wifi_list);
    lv_label_set_text(s_wifi_empty_hint,
        ws.connected ? "Tap SCAN for other networks" : "Tap SCAN to find networks");
    lv_obj_set_style_text_color(s_wifi_empty_hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_wifi_empty_hint, UI_FONT, 0);
    lv_obj_set_style_text_align(s_wifi_empty_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_wifi_empty_hint, ws.connected ? 20 : 50, 0);
    lv_obj_set_width(s_wifi_empty_hint, LV_PCT(100));

    /* Session 39k: If cached scan results exist (from a previous scan
     * before a connection-triggered rebuild), show them immediately.
     * Without this, a rebuild after connect wipes scan results from view. */
    if (s_scan_count > 0) {
        if (s_wifi_empty_hint) lv_obj_add_flag(s_wifi_empty_hint, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < s_scan_count; i++) {
            bool is_conn = ws.connected &&
                (strcmp((char *)s_scan_results[i].ssid, ws.ssid) == 0);
            /* Skip connected network -- already shown as dedicated row above */
            if (is_conn) continue;
            create_wifi_row(s_wifi_list, (char *)s_scan_results[i].ssid,
                            s_scan_results[i].rssi, false, i);
        }
    }

    s_wifi_scan_timer = lv_timer_create(wifi_scan_poll_cb, 100, NULL);

    /* Poll connection status every 2s - detects reconnect and network switch */
    wifi_status_t ws2 = wifi_manager_get_status();
    s_wifi_was_connected = ws2.connected;
    if (ws2.connected) {
        strncpy(s_wifi_was_ssid, ws2.ssid, 32);
        s_wifi_was_ssid[32] = '\0';
    } else {
        s_wifi_was_ssid[0] = '\0';
    }
    s_wifi_conn_timer = lv_timer_create(wifi_conn_poll_cb, 2000, NULL);

    ESP_LOGI("UI_SET", "WIFI tab created (3 objs/row, async rebuild)");
}

void settings_cleanup_wifi_timers(void)
{
    if (s_wifi_scan_timer) {
        lv_timer_del(s_wifi_scan_timer);
        s_wifi_scan_timer = NULL;
    }
    if (s_wifi_conn_timer) {
        lv_timer_del(s_wifi_conn_timer);
        s_wifi_conn_timer = NULL;
    }
    stop_anim();
    /* Session 39k: Cancel pending rebuild timer */
    if (s_rebuild_timer) {
        lv_timer_del(s_rebuild_timer);
        s_rebuild_timer = NULL;
    }
    /* Close password overlay if open */
    close_password_overlay();
    s_scan_in_progress = false;
    s_connecting_in_progress = false;
    s_connecting_ssid[0] = '\0';
    s_wifi_was_ssid[0] = '\0';
}

void settings_nullify_wifi_pointers(void)
{
    s_wifi_list = s_wifi_empty_hint = NULL;
}
