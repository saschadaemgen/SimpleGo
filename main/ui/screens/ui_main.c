/**
 * @file ui_main.c
 * @brief Main Screen — Unread Chats Dashboard
 *
 * Shows contacts with unread messages in compact row style.
 * If no unread messages exist, shows "No new messages" hint.
 *
 * Session 39c: Redesigned to match contacts/settings architecture
 * Session 39e: Row object reduction (6→3), status bar added
 * Session 40:  Combined single bar replaces status bar + header
 *   - One 26px bar: [GO] [unread count] ... [12:00] [WiFi] [Bat]
 *   - "SimpleGo" title removed — only "GO" remains
 *   - Content gains ~19px (177px vs 158px)
 *   - WiFi/Battery pixel-art from ui_create_status_bar() inlined
 * Session 41:  Icons redrawn all-cyan, bottom bar 3 zones, gear long-press WiFi
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_main.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "smp_types.h"
#include "smp_tasks.h"
#include "smp_contacts.h"
#include "smp_history.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_MAIN";
static lv_obj_t *screen         = NULL;
static lv_obj_t *list_container = NULL;
static lv_obj_t *empty_label    = NULL;
static lv_obj_t *hdr_count_lbl  = NULL;
static lv_timer_t *gear_lp_timer = NULL;
static lv_timer_t *s_hdr_refresh_timer = NULL;

/* ================================================================
 * Layout — Session 40: Combined single bar
 * ================================================================
 *
 *  Combined Bar  26px  [GO] 3 unread    [12:00] [WiFi] [Bat]
 *  Glow line      1px  ~~~~~~~~~~~~~~~~
 *  Content      177px  Scrollable unread list
 *  Bottom Bar    36px  CHATS | gear | MEDIA
 *                ----
 *  Total        240px
 */

#define MAIN_BAR_H      26
#define GLOW_Y          MAIN_BAR_H                      /* 26 */
#define CONTENT_Y       (MAIN_BAR_H + 1)                /* 27 */
#define BAR_H           36
#define BAR_Y           (UI_SCREEN_H - BAR_H)           /* 204 */
#define CONTENT_H       (BAR_Y - CONTENT_Y)             /* 177 */

/* Bottom bar: 4 equal buttons */
#define BAR_BTN_W       (UI_SCREEN_W / 3)               /* 106 */
#define BAR_BTN_W_LAST  (UI_SCREEN_W - BAR_BTN_W * 2)  /* 108 remainder */

/* Row styling — matches ui_contacts.c */
#define ROW_H           28
#define ROW_GAP         2
#define ROW_BG          lv_color_hex(0x000810)
#define ROW_BG_PRESS    lv_color_hex(0x001420)
#define NAME_ACTIVE     lv_color_hex(0xD0E8E8)

/* Name truncation */
#define NAME_MAX_DISPLAY  25
#define GEAR_LONGPRESS_MS 5000

/* ================================================================
 * Style Helpers (from ui_contacts.c)
 * ================================================================ */

static void style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* ================================================================
 * Navigation Callbacks
 * ================================================================ */

static void on_chats(lv_event_t *e) { (void)e; ui_manager_show_screen(UI_SCREEN_CONTACTS, LV_SCR_LOAD_ANIM_NONE); }
static void on_new(lv_event_t *e)   { (void)e; ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE); }
static void on_sys(lv_event_t *e)   { (void)e; ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE); }
static void on_wifi_menu(lv_event_t *e) { (void)e; ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE); }

static void gear_longpress_timer_cb(lv_timer_t *t)
{
    (void)t;
    ESP_LOGI(TAG, "Gear long-press 5s -> WiFi menu");
    gear_lp_timer = NULL;
    on_wifi_menu(NULL);
}

static void on_gear_pressed(lv_event_t *e)
{
    (void)e;
    if (gear_lp_timer) lv_timer_delete(gear_lp_timer);
    gear_lp_timer = lv_timer_create(gear_longpress_timer_cb, GEAR_LONGPRESS_MS, NULL);
    lv_timer_set_repeat_count(gear_lp_timer, 1);
}

static void on_gear_released(lv_event_t *e)
{
    (void)e;
    if (gear_lp_timer) { lv_timer_delete(gear_lp_timer); gear_lp_timer = NULL; }
}

/* ================================================================
 * Helpers
 * ================================================================ */

static void truncate_name(const char *src, char *dst, size_t dst_len)
{
    size_t len = strlen(src);
    if (len <= NAME_MAX_DISPLAY) {
        strncpy(dst, src, dst_len);
        dst[dst_len - 1] = '\0';
    } else {
        size_t copy = NAME_MAX_DISPLAY - 3;
        if (copy >= dst_len) copy = dst_len - 4;
        memcpy(dst, src, copy);
        dst[copy] = '.';
        dst[copy + 1] = '.';
        dst[copy + 2] = '.';
        dst[copy + 3] = '\0';
    }
}

/* ================================================================
 * Row Click -> Open Chat
 * ================================================================ */

static void on_row_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        smp_set_active_contact(idx);
        ui_chat_set_contact(contacts_db.contacts[idx].name);
        smp_request_load_history(idx);
        ui_manager_show_screen(UI_SCREEN_CHAT, LV_SCR_LOAD_ANIM_NONE);
        ui_chat_show_loading();
    }
}

/* ================================================================
 * Unread Contact Row (Session 39e: 3 objects per row)
 * ================================================================ */

static lv_obj_t *create_unread_row(lv_obj_t *parent, int idx, contact_t *c,
                                     uint16_t total, uint16_t unread)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, ROW_BG_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Child 0: Name + checkmark (green — these all have unread) */
    char display_name[40];
    truncate_name(c->name, display_name, sizeof(display_name));
    strncat(display_name, " " LV_SYMBOL_OK,
            sizeof(display_name) - strlen(display_name) - 1);

    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, display_name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(name_lbl, UI_FONT, 0);
    lv_obj_set_style_text_color(name_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 2, 0);

    /* Child 1: Count + E2EE (combined) */
    lv_obj_t *info_lbl = lv_label_create(row);
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_10, 0);
    lv_label_set_text_fmt(info_lbl, "%u " LV_SYMBOL_ENVELOPE " %u  E2EE",
                          total, unread);
    lv_obj_set_style_text_color(info_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_align(info_lbl, LV_ALIGN_RIGHT_MID, -2, 0);

    /* Click event */
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_row_click, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    return row;
}

/* ================================================================
 * Pixel-Art Helpers (from ui_create_status_bar)
 * ================================================================ */

/** Helper: tiny filled rect */
static lv_obj_t *_pixel_rect(lv_obj_t *par, int w, int h,
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

/** Helper: reset obj to invisible container */
static void _sty_reset(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

/* ================================================================
 * Combined Bar (Session 40)
 *
 * 26px: [GO] [3 unread]          [12:00] [WiFi▂▃▅▇] [▯▯|]
 *
 * Replaces separate status bar (16px) + header (26px) = 44px
 * Now only 26px + 1px glow = 27px total → +17px for content
 * ================================================================ */

static void create_main_bar(lv_obj_t *parent)
{
    /* ---- Bar background (26px, black) ---- */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, MAIN_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- WiFi SSID / Unread count (left, updated by refresh) ---- */
    hdr_count_lbl = lv_label_create(bar);
    lv_label_set_text(hdr_count_lbl, "");
    lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hdr_count_lbl, UI_FONT, 0);
    lv_obj_align(hdr_count_lbl, LV_ALIGN_LEFT_MID, 6, 0);

    /* ---- Battery tip (rightmost element) ---- */
    lv_obj_t *bat_tip = _pixel_rect(bar, 2, 5,
                                     UI_COLOR_PRIMARY, LV_OPA_COVER);
    lv_obj_align(bat_tip, LV_ALIGN_RIGHT_MID, -4, 0);

    /* ---- Battery body: 18×9, outlined ---- */
    lv_obj_t *bat_body = lv_obj_create(bar);
    lv_obj_set_size(bat_body, 20, 11);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bat_body, 1, 0);
    lv_obj_set_style_border_color(bat_body, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(bat_body, 1, 0);
    lv_obj_set_style_pad_all(bat_body, 1, 0);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(bat_body, bat_tip, LV_ALIGN_OUT_LEFT_MID, -1, 0);

    /* Battery fill: ~70% green inside body */
    lv_obj_t *bat_fill = _pixel_rect(bat_body, 12, 7,
                                      UI_COLOR_PRIMARY, LV_OPA_COVER);
    lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);

    /* ---- WiFi: 4 ascending bars ---- */
    lv_obj_t *wifi = lv_obj_create(bar);
    lv_obj_set_size(wifi, 15, 11);
    _sty_reset(wifi);
    lv_obj_align_to(wifi, bat_body, LV_ALIGN_OUT_LEFT_MID, -6, 0);

    /* Bars: 3px wide, heights 3/5/8/11, all solid cyan */
    static const int bar_h[] = {3, 5, 8, 11};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *wb = _pixel_rect(wifi, 3, bar_h[i],
                                    UI_COLOR_PRIMARY, LV_OPA_COVER);
        lv_obj_set_pos(wb, i * 4, 11 - bar_h[i]);
    }

    /* ---- Time: dim cyan ---- */
    lv_obj_t *time_lbl = lv_label_create(bar);
    lv_label_set_text(time_lbl, "12:00");
    lv_obj_set_style_text_color(time_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(time_lbl, UI_FONT_MD, 0);
    lv_obj_align_to(time_lbl, wifi, LV_ALIGN_OUT_LEFT_MID, -6, 0);

    /* ---- Glow line below combined bar ---- */
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_set_width(glow, LV_PCT(100));
    lv_obj_set_height(glow, 1);
    lv_obj_set_pos(glow, 0, GLOW_Y);
    lv_obj_set_style_bg_color(glow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(glow, (lv_opa_t)64, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_radius(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE);
}

/* ================================================================
 * Bottom Bar (contacts-style: CHATS | NEW | DEV | gear)
 * ================================================================ */

static void create_bar_separator(lv_obj_t *parent, int x)
{
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 1, BAR_H - 8);
    lv_obj_set_pos(sep, x, 4);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_bar_btn(lv_obj_t *parent, const char *text,
                                  int x, int w, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, BAR_H);
    lv_obj_set_pos(btn, x, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static void create_bottom_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_height(bar, BAR_H);
    lv_obj_set_pos(bar, 0, BAR_Y);
    style_black(bar);

    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(bar, (lv_opa_t)38, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);

    /* Zone 1: CHATS (left) */
    create_bar_btn(bar, "CHATS", 0, BAR_BTN_W, on_chats);
    create_bar_separator(bar, BAR_BTN_W);

    /* Zone 2: gear (center) - short=settings, 5s long=WiFi */
    lv_obj_t *gear_btn = create_bar_btn(bar, LV_SYMBOL_SETTINGS,
                                         BAR_BTN_W, BAR_BTN_W, on_sys);
    lv_obj_add_event_cb(gear_btn, on_gear_pressed,  LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(gear_btn, on_gear_released, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(gear_btn, on_gear_released, LV_EVENT_PRESS_LOST, NULL);
    create_bar_separator(bar, BAR_BTN_W * 2);

    /* Zone 3: MEDIA (right, placeholder -> CONNECT) */
    create_bar_btn(bar, "MEDIA", BAR_BTN_W * 2, BAR_BTN_W_LAST, on_new);
}

/* ================================================================
 * Header Bar Periodic Refresh (Session 39k)
 *
 * Updates SSID / unread indicator every 3s so WiFi SSID appears
 * on the main screen even if connection happens after boot.
 * ================================================================ */

static void hdr_refresh_cb(lv_timer_t *t)
{
    (void)t;
    if (!hdr_count_lbl) return;

    /* Count unreads (lightweight -- just count, no SD reads) */
    int unread_count = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        uint16_t total, unread;
        smp_history_get_counts(i, &total, &unread);
        if (unread > 0) unread_count++;
    }

    if (unread_count > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_ENVELOPE " %d", unread_count);
        lv_label_set_text(hdr_count_lbl, buf);
        lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_PRIMARY, 0);
    } else {
        wifi_status_t ws = wifi_manager_get_status();
        if (ws.connected) {
            char ssid_buf[28];
            snprintf(ssid_buf, sizeof(ssid_buf), "%.24s", ws.ssid);
            lv_label_set_text(hdr_count_lbl, ssid_buf);
            lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_PRIMARY, 0);
        } else {
            lv_label_set_text(hdr_count_lbl, "No WiFi");
            lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_TEXT_DIM, 0);
        }
    }
}

/* ================================================================
 * Content Area
 * ================================================================ */

static void create_content_area(lv_obj_t *parent)
{
    list_container = lv_obj_create(parent);
    lv_obj_set_size(list_container, UI_SCREEN_W, CONTENT_H);
    lv_obj_set_pos(list_container, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container, 0, 0);
    lv_obj_set_style_pad_all(list_container, 0, 0);
    lv_obj_set_style_pad_top(list_container, 4, 0);
    lv_obj_set_style_pad_row(list_container, ROW_GAP, 0);
    lv_obj_set_style_radius(list_container, 0, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_OFF);
}

/* ================================================================
 * Screen Create
 * ================================================================ */

lv_obj_t *ui_main_create(void)
{
    ESP_LOGI(TAG, "Creating main...");

    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* Session 40: Combined bar replaces status bar + header */
    create_main_bar(screen);
    create_content_area(screen);
    create_bottom_bar(screen);

    /* Empty state label (inside content area) */
    empty_label = lv_label_create(list_container);
    lv_label_set_text(empty_label, "No new messages");
    lv_obj_set_style_text_color(empty_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(empty_label, UI_FONT, 0);
    lv_obj_set_width(empty_label, LV_PCT(100));
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(empty_label, 50, 0);

    /* Initial populate */
    ui_main_refresh();

    /* Session 39k: Periodic header refresh for WiFi SSID after boot */
    s_hdr_refresh_timer = lv_timer_create(hdr_refresh_cb, 3000, NULL);

    return screen;
}

/* ================================================================
 * Refresh — Scan for Unread
 * ================================================================ */

void ui_main_refresh(void)
{
    if (!list_container) return;

    /* Remove old rows (keep empty_label) */
    uint32_t cnt = lv_obj_get_child_count(list_container);
    for (int i = cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(list_container, i);
        if (child != empty_label) {
            lv_obj_delete(child);
        }
    }

    /* Pre-fetch SD card counts BEFORE LVGL rendering
     * to avoid SPI mutex deadlock with display task */
    uint16_t totals[MAX_CONTACTS] = {0};
    uint16_t unreads[MAX_CONTACTS] = {0};
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        smp_history_get_counts(i, &totals[i], &unreads[i]);
    }

    /* Build rows only for contacts with unread messages */
    int unread_count = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;

        if (unreads[i] > 0) {
            create_unread_row(list_container, i, &contacts_db.contacts[i],
                              totals[i], unreads[i]);
            unread_count++;
        }
    }

    /* Show/hide empty label */
    if (unread_count > 0) {
        lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    /* Update combined bar info: unread count OR WiFi SSID */
    if (hdr_count_lbl) {
        if (unread_count > 0) {
            /* Unread: blue mail icon + count */
            char buf[24];
            snprintf(buf, sizeof(buf), LV_SYMBOL_ENVELOPE " %d", unread_count);
            lv_label_set_text(hdr_count_lbl, buf);
            lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_PRIMARY, 0);
        } else {
            /* No unread: show connected WiFi SSID */
            wifi_status_t ws = wifi_manager_get_status();
            if (ws.connected) {
                char ssid_buf[28];
                snprintf(ssid_buf, sizeof(ssid_buf), "%.24s", ws.ssid);
                lv_label_set_text(hdr_count_lbl, ssid_buf);
                lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_PRIMARY, 0);
            } else {
                lv_label_set_text(hdr_count_lbl, "No WiFi");
                lv_obj_set_style_text_color(hdr_count_lbl, UI_COLOR_TEXT_DIM, 0);
            }
        }
    }
}