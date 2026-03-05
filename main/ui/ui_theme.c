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

/* ============================================================
 * v2 Status Bar (16px) — hand-drawn indicators
 *
 * Matches mockup v5:
 *   [GO]          [12:42] [▂▃▅▇] [▯▯▯|]
 *   Font10        Font10   4bars   outlined battery
 * ============================================================ */

/** Helper: make a tiny invisible container */
static void _sty_reset(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

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

lv_obj_t *ui_create_status_bar(lv_obj_t *parent)
{
    /* ---- Bar background (14px, black) ---- */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_height(bar, UI_STATUS_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- "GO" button — left side, Font 10 ---- */
    lv_obj_t *go_btn = lv_btn_create(bar);
    lv_obj_set_size(go_btn, 28, UI_STATUS_H);
    lv_obj_set_pos(go_btn, 2, 0);
    lv_obj_set_style_bg_opa(go_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(go_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(go_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(go_btn, 0, 0);
    lv_obj_set_style_radius(go_btn, 0, 0);
    lv_obj_set_style_shadow_width(go_btn, 0, 0);
    lv_obj_set_style_pad_all(go_btn, 0, 0);

    lv_obj_t *go_lbl = lv_label_create(go_btn);
    lv_label_set_text(go_lbl, "GO");
    lv_obj_set_style_text_color(go_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(go_lbl, UI_FONT_SM, 0);
    lv_obj_center(go_lbl);

    /* ---- Battery tip (rightmost element) ---- */
    lv_obj_t *bat_tip = _pixel_rect(bar, 1, 3,
                                     UI_COLOR_TEXT_DIM, LV_OPA_COVER);
    lv_obj_align(bat_tip, LV_ALIGN_RIGHT_MID, -4, 0);

    /* ---- Battery body: 14×7, outlined ---- */
    lv_obj_t *bat_body = lv_obj_create(bar);
    lv_obj_set_size(bat_body, 14, 7);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bat_body, 1, 0);
    lv_obj_set_style_border_color(bat_body, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(bat_body, 1, 0);
    lv_obj_set_style_pad_all(bat_body, 1, 0);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(bat_body, bat_tip, LV_ALIGN_OUT_LEFT_MID, -1, 0);

    /* Battery fill: ~70% green inside body */
    lv_obj_t *bat_fill = _pixel_rect(bat_body, 7, 3,
                                      UI_COLOR_SECONDARY, LV_OPA_COVER);
    lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);

    /* ---- WiFi: 4 ascending bars ---- */
    /* Container: 11 x 7 px */
    lv_obj_t *wifi = lv_obj_create(bar);
    lv_obj_set_size(wifi, 11, 7);
    _sty_reset(wifi);
    lv_obj_align_to(wifi, bat_body, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    /* Bars: widths 2px, heights 2/3/5/7, spaced 3px apart, bottom-aligned */
    static const int bar_h[] = {2, 3, 5, 7};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *wb = _pixel_rect(wifi, 2, bar_h[i],
                                    UI_COLOR_PRIMARY,
                                    (i == 3) ? LV_OPA_30 : LV_OPA_COVER);
        lv_obj_set_pos(wb, i * 3, 7 - bar_h[i]);
    }

    /* ---- Time: dim cyan, Font 10 ---- */
    lv_obj_t *time_lbl = lv_label_create(bar);
    lv_label_set_text(time_lbl, "12:00");
    lv_obj_set_style_text_color(time_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
    lv_obj_align_to(time_lbl, wifi, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    /* ---- Glow line below status bar ---- */
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_set_width(glow, LV_PCT(100));
    lv_obj_set_height(glow, 1);
    lv_obj_set_pos(glow, 0, UI_STATUS_H);
    lv_obj_set_style_bg_color(glow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(glow, (lv_opa_t)64, 0);  /* ~25% */
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_radius(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE);

    ESP_LOGI(TAG, "Status bar created (16px, hand-drawn)");
    return go_btn;
}

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
