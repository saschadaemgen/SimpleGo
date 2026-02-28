/**
 * @file ui_settings.c
 * @brief Settings Screen - Display and Keyboard brightness
 *
 * Optimized for T-Deck Plus 320x240.
 * Slider knobs flush with button edges at min/max positions.
 *
 * Vertical budget (240px):
 *   Header area    44px  (status 16 + glow 1 + header 26 + dim 1)
 *   Top margin     14px
 *   Group 1        62px  (Display: label + slider + 4 buttons)
 *   Inter-gap      18px
 *   Group 2        62px  (Keyboard: label + slider + 4 buttons)
 *   Gap             8px
 *   Hint text      ~16px
 *   Bottom         ~16px
 *                 ------
 *                 240px
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "tdeck_backlight.h"
#include "tdeck_keyboard.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "UI_SETTINGS";

/* ============== Layout (320x240 T-Deck Plus) ============== */

#define HDR_Y           (UI_STATUS_H + 1)       /* 17 */
#define HDR_H           26
#define DIM_Y           (HDR_Y + HDR_H)          /* 43 */
#define CONTENT_Y       (DIM_Y + 1)              /* 44 */

/*
 * Visual row = 240px centered (x 40..280).
 * Buttons span full row width.
 * Slider track is inset so knob edges align with button edges.
 *
 * Knob geometry: slider_h=10, knob_pad=3 -> knob=16px -> overhang=8px
 */
#define ROW_W           240
#define ROW_X           ((320 - ROW_W) / 2)      /* 40 */

#define KNOB_PAD        3
#define SLIDER_H        10
#define KNOB_SIZE       (SLIDER_H + 2 * KNOB_PAD) /* 16 */
#define KNOB_OVERHANG   (KNOB_SIZE / 2)            /* 8 */

#define SLIDER_W        (ROW_W - 2 * KNOB_OVERHANG) /* 224 */
#define SLIDER_X        (ROW_X + KNOB_OVERHANG)     /* 48 */

/* Group offsets from group base Y */
#define LABEL_OFS       -5
#define SLIDER_OFS      18
#define BTN_OFS         40

/* Preset buttons — 4 buttons spanning ROW_W exactly */
#define PBTN_H          22
#define PBTN_GAP        8
#define PBTN_W          ((ROW_W - 3 * PBTN_GAP) / 4)  /* 54 */

/* Group positions */
#define GROUP1_Y        (CONTENT_Y + 14)         /* 58 */
#define GROUP2_Y        (GROUP1_Y + 62 + 18)     /* 138 */

/* ============== State ============== */

static lv_obj_t *s_disp_pct    = NULL;
static lv_obj_t *s_kbd_pct     = NULL;
static lv_obj_t *s_disp_slider = NULL;
static lv_obj_t *s_kbd_slider  = NULL;

/* ============== Style ============== */

static void style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_slider(lv_obj_t *slider)
{
    lv_obj_set_size(slider, SLIDER_W, SLIDER_H);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x002030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UI_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, KNOB_PAD, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, SLIDER_H / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, SLIDER_H / 2, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, KNOB_SIZE / 2, LV_PART_KNOB);
}

/* ============== Helpers ============== */

static void update_pct(lv_obj_t *label, int val, int max)
{
    if (!label) return;
    int pct = (val * 100) / (max > 0 ? max : 1);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(label, buf);
}

/* ============== Events ============== */

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_chat_update_settings_icon();
    ui_manager_go_back();
}

static void on_display_slider(lv_event_t *e)
{
    int v = lv_slider_get_value(lv_event_get_target(e));
    tdeck_backlight_set((uint8_t)v);
    update_pct(s_disp_pct, v, 16);
}

static void on_keyboard_slider(lv_event_t *e)
{
    int v = lv_slider_get_value(lv_event_get_target(e));
    tdeck_kbd_backlight_set((uint8_t)v);
    update_pct(s_kbd_pct, v, 255);
}

/* Display presets (min=1, screen must stay visible) */
static void set_disp(int v)
{
    tdeck_backlight_set((uint8_t)v);
    if (s_disp_slider) lv_slider_set_value(s_disp_slider, v, LV_ANIM_OFF);
    update_pct(s_disp_pct, v, 16);
}
static void on_disp_0(lv_event_t *e)   { (void)e; set_disp(1);  }
static void on_disp_25(lv_event_t *e)  { (void)e; set_disp(4);  }
static void on_disp_50(lv_event_t *e)  { (void)e; set_disp(8);  }
static void on_disp_100(lv_event_t *e) { (void)e; set_disp(16); }

/* Keyboard presets (0 = off) */
static void set_kbd(int v)
{
    tdeck_kbd_backlight_set((uint8_t)v);
    if (s_kbd_slider) lv_slider_set_value(s_kbd_slider, v, LV_ANIM_OFF);
    update_pct(s_kbd_pct, v, 255);
}
static void on_kbd_0(lv_event_t *e)    { (void)e; set_kbd(0);   }
static void on_kbd_25(lv_event_t *e)   { (void)e; set_kbd(64);  }
static void on_kbd_50(lv_event_t *e)   { (void)e; set_kbd(128); }
static void on_kbd_100(lv_event_t *e)  { (void)e; set_kbd(255); }

/* ============== Header ============== */

static void create_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, HDR_H);
    lv_obj_set_pos(hdr, 0, HDR_Y);
    style_black(hdr);

    lv_obj_t *back_btn = lv_btn_create(hdr);
    lv_obj_set_size(back_btn, 24, HDR_H);
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
    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Brightness Settings");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 28, 0);

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

/* ============== Preset Buttons ============== */

static void create_preset_btn(lv_obj_t *parent, const char *text,
                               int x, int y, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, PBTN_W, PBTN_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x001418), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x003344), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_SM, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
}

static void create_preset_row(lv_obj_t *parent, int y,
                               lv_event_cb_t cb0, lv_event_cb_t cb25,
                               lv_event_cb_t cb50, lv_event_cb_t cb100)
{
    int x = ROW_X;
    int step = PBTN_W + PBTN_GAP;
    create_preset_btn(parent, "0%",   x,            y, cb0);
    create_preset_btn(parent, "25%",  x + step,     y, cb25);
    create_preset_btn(parent, "50%",  x + step * 2, y, cb50);
    create_preset_btn(parent, "100%", x + step * 3, y, cb100);
}

/* ============== Slider Group ============== */

static lv_obj_t *create_slider_group(
    lv_obj_t *parent,
    const char *label_text,
    int y_base,
    int range_min,
    int range_max,
    int initial_val,
    lv_event_cb_t slider_cb,
    lv_obj_t **slider_out)
{
    /* Label left, aligned with button row */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_pos(label, ROW_X, y_base + LABEL_OFS);

    /* Percentage right, aligned with button row end */
    lv_obj_t *pct = lv_label_create(parent);
    int pct_val = (initial_val * 100) / (range_max > 0 ? range_max : 1);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct_val);
    lv_label_set_text(pct, buf);
    lv_obj_set_style_text_color(pct, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(pct, UI_FONT, 0);
    lv_obj_set_pos(pct, ROW_X + ROW_W - 36, y_base + LABEL_OFS);

    /* Slider — inset so knob edges align with button row edges */
    lv_obj_t *slider = lv_slider_create(parent);
    style_slider(slider);
    lv_obj_set_pos(slider, SLIDER_X, y_base + SLIDER_OFS);
    lv_slider_set_range(slider, range_min, range_max);
    lv_slider_set_value(slider, initial_val, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (slider_out) *slider_out = slider;
    return pct;
}

/* ============== Content ============== */

static void create_content(lv_obj_t *parent)
{
    /* Group 1: Display backlight */
    uint8_t disp_val = tdeck_backlight_get();
    if (disp_val == 0) disp_val = 8;
    s_disp_pct = create_slider_group(parent, "Display",
        GROUP1_Y, 1, 16, disp_val, on_display_slider, &s_disp_slider);
    create_preset_row(parent, GROUP1_Y + BTN_OFS,
        on_disp_0, on_disp_25, on_disp_50, on_disp_100);

    /* Group 2: Keyboard backlight */
    uint8_t kbd_val = tdeck_kbd_backlight_get_current();
    s_kbd_pct = create_slider_group(parent, "Keyboard",
        GROUP2_Y, 0, 255, kbd_val, on_keyboard_slider, &s_kbd_slider);
    create_preset_row(parent, GROUP2_Y + BTN_OFS,
        on_kbd_0, on_kbd_25, on_kbd_50, on_kbd_100);

    /* Battery hint at bottom */
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "(Brightness affects power and battery life)");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x304050), 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_theme_apply(scr);

    lv_obj_t *go_btn = ui_create_status_bar(scr);
    lv_obj_add_event_cb(go_btn, on_back, LV_EVENT_CLICKED, NULL);

    create_header(scr);
    create_content(scr);

    ESP_LOGI(TAG, "Settings screen created");
    return scr;
}
