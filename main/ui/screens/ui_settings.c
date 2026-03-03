/**
 * @file ui_settings.c
 * @brief Settings Screen - Tabbed layout with BRIGHT / WIFI / INFO tabs
 *
 * Session 39: Complete rewrite from brightness-only to tabbed architecture.
 * Layout identical to ui_contacts.c (header, content area, bottom bar).
 *
 * Tab content modules:
 *   ui_settings_bright.c  - Display/Keyboard brightness sliders
 *   ui_settings_wifi.c    - WiFi scan, connect, NVS persistence
 *   ui_settings_info.c    - Device info, heap, server status
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"

static const char *TAG = "UI_SETTINGS";

/* ================================================================
 * Shared State (extern'd in ui_settings_internal.h)
 * ================================================================ */

settings_tab_t active_tab   = TAB_BRIGHT;
lv_obj_t *content_area      = NULL;
lv_obj_t *s_settings_scr    = NULL;

/* Header dynamic elements */
lv_obj_t *hdr_title_lbl     = NULL;
lv_obj_t *hdr_action_btn    = NULL;
lv_obj_t *hdr_action_lbl    = NULL;

/* Bottom bar tab buttons */
static lv_obj_t *s_tab_btns[3] = {NULL, NULL, NULL};

/* ================================================================
 * Style Helper (used by all tab modules)
 * ================================================================ */

void settings_style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* ================================================================
 * Header (contacts-style: back arrow + dynamic title + action btn)
 * ================================================================ */

static void on_hdr_action(lv_event_t *e)
{
    (void)e;
    if (active_tab == TAB_WIFI) {
        settings_on_wifi_hdr_action();
    }
}

static void update_tab_button_styles(void)
{
    const char *normal[3]  = {"BRIGHT", "WIFI", "INFO"};
    const char *active[3]  = {"(BRIGHT)", "(WIFI)", "(INFO)"};

    for (int i = 0; i < 3; i++) {
        if (!s_tab_btns[i]) continue;
        lv_obj_t *lbl = lv_obj_get_child(s_tab_btns[i], 0);
        /* No background highlight -- text only */
        lv_obj_set_style_bg_opa(s_tab_btns[i], LV_OPA_TRANSP, 0);
        if (i == (int)active_tab) {
            if (lbl) {
                lv_label_set_text(lbl, active[i]);
                lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_WHITE, 0);
            }
        } else {
            if (lbl) {
                lv_label_set_text(lbl, normal[i]);
                lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
            }
        }
    }
}

void settings_update_header_for_tab(void)
{
    if (!hdr_title_lbl || !hdr_action_btn || !hdr_action_lbl) return;

    switch (active_tab) {
    case TAB_BRIGHT:
        lv_label_set_text(hdr_title_lbl, "Brightness");
        lv_obj_add_flag(hdr_action_btn, LV_OBJ_FLAG_HIDDEN);
        break;
    case TAB_WIFI:
        settings_update_wifi_header();
        break;
    case TAB_INFO:
        lv_label_set_text(hdr_title_lbl, "Device Info");
        lv_obj_add_flag(hdr_action_btn, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

static void create_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, HDR_H);
    lv_obj_set_pos(hdr, 0, HDR_Y);
    settings_style_black(hdr);

    /* Title (dynamic, set per-tab) */
    hdr_title_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_title_lbl, "Settings");
    lv_obj_set_style_text_color(hdr_title_lbl, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(hdr_title_lbl, UI_FONT, 0);
    lv_obj_align(hdr_title_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    /* Action button (SCAN / CONNECT / DISCONNECT for WiFi tab) */
    hdr_action_btn = lv_btn_create(hdr);
    lv_obj_set_height(hdr_action_btn, HDR_H);
    lv_obj_set_style_bg_opa(hdr_action_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(hdr_action_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(hdr_action_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(hdr_action_btn, 0, 0);
    lv_obj_set_style_radius(hdr_action_btn, 0, 0);
    lv_obj_set_style_shadow_width(hdr_action_btn, 0, 0);
    lv_obj_set_style_pad_left(hdr_action_btn, 8, 0);
    lv_obj_set_style_pad_right(hdr_action_btn, 4, 0);
    lv_obj_align(hdr_action_btn, LV_ALIGN_RIGHT_MID, 0, 0);

    hdr_action_lbl = lv_label_create(hdr_action_btn);
    lv_label_set_text(hdr_action_lbl, "SCAN");
    lv_obj_set_style_text_color(hdr_action_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(hdr_action_lbl, UI_FONT, 0);
    lv_obj_center(hdr_action_lbl);

    lv_obj_add_event_cb(hdr_action_btn, on_hdr_action, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(hdr_action_btn, LV_OBJ_FLAG_HIDDEN); /* shown only on WIFI tab */

    /* Dim line below header */
    lv_obj_t *dim = lv_obj_create(parent);
    lv_obj_set_width(dim, LV_PCT(100));
    lv_obj_set_height(dim, 1);
    lv_obj_set_pos(dim, 0, DIM_Y);
    lv_obj_set_style_bg_color(dim, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(dim, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dim, 0, 0);
    lv_obj_set_style_radius(dim, 0, 0);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_CLICKABLE);
}

/* ================================================================
 * Content Area
 * ================================================================ */

static void create_content_area(lv_obj_t *parent)
{
    content_area = lv_obj_create(parent);
    lv_obj_set_size(content_area, UI_SCREEN_W, CONTENT_H);
    lv_obj_set_pos(content_area, 0, CONTENT_Y);
    settings_style_black(content_area);
}

/* ================================================================
 * Tab Switching
 * ================================================================ */

void settings_switch_tab(settings_tab_t tab)
{
    /* Cleanup old tab timers */
    switch (active_tab) {
    case TAB_BRIGHT: settings_cleanup_bright_timers(); break;
    case TAB_WIFI:   settings_cleanup_wifi_timers();   break;
    case TAB_INFO:   settings_cleanup_info_timers();   break;
    }

    active_tab = tab;

    /* Clear content area */
    if (content_area) {
        lv_obj_clean(content_area);
    }

    /* Nullify stale pointers in ALL tab modules */
    settings_nullify_bright_pointers();
    settings_nullify_wifi_pointers();
    settings_nullify_info_pointers();

    /* Build new tab content */
    switch (tab) {
    case TAB_BRIGHT: settings_create_bright(content_area); break;
    case TAB_WIFI:   settings_create_wifi(content_area);   break;
    case TAB_INFO:   settings_create_info(content_area);   break;
    }

    /* Update header and tab button styles */
    settings_update_header_for_tab();
    update_tab_button_styles();
}

/* ================================================================
 * Bottom Bar (BACK + 3 tab buttons, contacts-style)
 * ================================================================ */

/* Thin vertical separator */
static void create_bar_sep(lv_obj_t *parent, lv_coord_t x)
{
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 1, BAR_H - 10);
    lv_obj_set_pos(sep, x, 5);
    lv_obj_set_style_bg_color(sep, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(sep, (lv_opa_t)50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static void on_back(lv_event_t *e)
{
    (void)e;
    /* Cleanup active tab timers */
    switch (active_tab) {
    case TAB_BRIGHT: settings_cleanup_bright_timers(); break;
    case TAB_WIFI:   settings_cleanup_wifi_timers();   break;
    case TAB_INFO:   settings_cleanup_info_timers();   break;
    }
    wifi_manager_resume_auto_reconnect();
    ui_chat_update_settings_icon();
    ui_manager_go_back();
}

static void on_tab_bright(lv_event_t *e) { (void)e; settings_switch_tab(TAB_BRIGHT); }
static void on_tab_wifi(lv_event_t *e)   { (void)e; settings_switch_tab(TAB_WIFI); }
static void on_tab_info(lv_event_t *e)   { (void)e; settings_switch_tab(TAB_INFO); }

static lv_obj_t *create_bar_btn(lv_obj_t *parent, const char *text,
                                 lv_coord_t x, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BAR_BTN_W, BAR_H);
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
    settings_style_black(bar);

    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(bar, (lv_opa_t)38, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);

    /* BACK | BRIGHT | WIFI | INFO  (4 x 80px = 320px) */
    lv_obj_t *back_btn = create_bar_btn(bar, LV_SYMBOL_LEFT " Back",
                                         0, on_back);
    (void)back_btn;

    create_bar_sep(bar, BAR_BTN_W);
    s_tab_btns[0] = create_bar_btn(bar, "BRIGHT",
                                    BAR_BTN_W, on_tab_bright);

    create_bar_sep(bar, BAR_BTN_W * 2);
    s_tab_btns[1] = create_bar_btn(bar, "WIFI",
                                    BAR_BTN_W * 2, on_tab_wifi);

    create_bar_sep(bar, BAR_BTN_W * 3);
    s_tab_btns[2] = create_bar_btn(bar, "INFO",
                                    BAR_BTN_W * 3, on_tab_info);
}

/* ================================================================
 * Screen Creation
 * ================================================================ */

lv_obj_t *ui_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_theme_apply(scr);
    s_settings_scr = scr;

    /* Status bar */
    lv_obj_t *go_btn = ui_create_status_bar(scr);
    lv_obj_add_event_cb(go_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* Header with dynamic title + action button */
    create_header(scr);

    /* Content area (tab content goes here) */
    create_content_area(scr);

    /* Bottom bar: BACK + 3 tabs */
    create_bottom_bar(scr);

    /* Start on BRIGHT tab */
    active_tab = TAB_BRIGHT;
    settings_create_bright(content_area);
    settings_update_header_for_tab();
    update_tab_button_styles();

    ESP_LOGI(TAG, "Settings screen created (tabbed: BRIGHT/WIFI/INFO)");
    return scr;
}

void ui_settings_show_wifi_tab(void)
{
    settings_switch_tab(TAB_WIFI);
}
