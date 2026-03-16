/**
 * SimpleGo - ui_theme.c
 * Cyberpunk theme implementation
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_theme.h"
#include "esp_log.h"

static const char *TAG = "THEME";

void ui_theme_init(void) {
    ESP_LOGI(TAG, "Cyberpunk theme");
}

void ui_theme_apply(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_create_header(lv_obj_t *parent, const char *title, const char *right_text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(lbl, 4, 2);

    if (right_text) {
        lv_obj_t *r = lv_label_create(parent);
        lv_label_set_text(r, right_text);
        lv_obj_set_style_text_color(r, UI_COLOR_TEXT_DIM, 0);
        lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -4, 2);
    }

    ui_create_line(parent, UI_HEADER_H);
    return lbl;
}

lv_obj_t *ui_create_back_btn(lv_obj_t *parent) {
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 56, UI_BTN_H);
    lv_obj_set_pos(btn, 2, y);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "< BACK");
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *ui_create_nav_bar(lv_obj_t *parent) {
    ui_create_line(parent, UI_SCREEN_H - UI_NAV_H);
    return NULL;
}

lv_obj_t *ui_create_nav_btn(lv_obj_t *parent, const char *text, int index) {
    int btn_w = 79;
    int x = index * 80;
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    if (index > 0) {
        lv_obj_t *sep = lv_obj_create(parent);
        lv_obj_set_size(sep, 1, UI_BTN_H);
        lv_obj_set_pos(sep, x, y);
        lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_w, UI_BTN_H);
    lv_obj_set_pos(btn, x + 1, y);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *ui_create_tab_btn(lv_obj_t *parent, const char *text, int index, int total, bool active) {
    int tab_w = 60;
    int start_x = UI_SCREEN_W - (total * tab_w) - 4;
    int x = start_x + (index * tab_w);
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, tab_w - 2, UI_BTN_H);
    lv_obj_set_pos(btn, x, y);

    if (active) {
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_30, 0);
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_style_bg_opa(btn, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, active ? UI_COLOR_TEXT_WHITE : UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *ui_create_switch(lv_obj_t *parent, const char *label, lv_coord_t x, lv_coord_t y, bool state) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 150, 28);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(cont);
    lv_obj_set_size(sw, 40, 20);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_style_bg_color(sw, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_KNOB | LV_STATE_CHECKED);

    if (state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    return sw;
}

lv_obj_t *ui_create_line(lv_obj_t *parent, lv_coord_t y) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_width(line, LV_PCT(100));
    lv_obj_set_height(line, 1);
    lv_obj_set_pos(line, 0, y);
    lv_obj_set_style_bg_color(line, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

lv_obj_t *ui_create_btn(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, UI_BTN_H);
    lv_obj_set_pos(btn, x, y);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);

    return btn;
}

/* Session 48: ui_create_status_bar() removed. All screens now use
 * ui_statusbar.c (shared module with live clock, WiFi RSSI, battery). */

/* ============== Session 39f: Centralized Style Helpers ============== */

void ui_style_reset(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
