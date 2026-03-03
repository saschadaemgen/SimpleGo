/**
 * @file ui_settings_bright.c
 * @brief Settings Screen - BRIGHT Tab (Display + Keyboard brightness)
 *
 * Session 39: Extracted from monolithic ui_settings.c into tab module.
 * Content positions are relative to content_area (Y starts at 0).
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"

/* ================================================================
 * Layout (relative to content_area, not screen)
 *
 * Content area: 320 x 160px (starts at screen Y=44)
 * Visual row = 240px centered (x 40..280)
 * ================================================================ */

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

/* Preset buttons */
#define PBTN_H          22
#define PBTN_GAP        8
#define PBTN_W          ((ROW_W - 3 * PBTN_GAP) / 4)  /* 54 */

/* Group positions (relative to content_area top = 0) */
#define GROUP1_Y        14
#define GROUP2_Y        (GROUP1_Y + 62 + 18)     /* 94 */

/* ================================================================
 * State
 * ================================================================ */

static lv_obj_t *s_disp_pct    = NULL;
static lv_obj_t *s_kbd_pct     = NULL;
static lv_obj_t *s_disp_slider = NULL;
static lv_obj_t *s_kbd_slider  = NULL;

/* ================================================================
 * Helpers
 * ================================================================ */

static void update_pct(lv_obj_t *label, int val, int max)
{
    if (!label) return;
    int pct = (val * 100) / (max > 0 ? max : 1);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(label, buf);
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

/* ================================================================
 * Events
 * ================================================================ */

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

/* ================================================================
 * Preset Buttons
 * ================================================================ */

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

/* ================================================================
 * Slider Group
 * ================================================================ */

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
    /* Label left */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_pos(label, ROW_X, y_base + LABEL_OFS);

    /* Percentage right */
    lv_obj_t *pct = lv_label_create(parent);
    int pct_val = (initial_val * 100) / (range_max > 0 ? range_max : 1);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct_val);
    lv_label_set_text(pct, buf);
    lv_obj_set_style_text_color(pct, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(pct, UI_FONT, 0);
    lv_obj_set_pos(pct, ROW_X + ROW_W - 36, y_base + LABEL_OFS);

    /* Slider */
    lv_obj_t *slider = lv_slider_create(parent);
    style_slider(slider);
    lv_obj_set_pos(slider, SLIDER_X, y_base + SLIDER_OFS);
    lv_slider_set_range(slider, range_min, range_max);
    lv_slider_set_value(slider, initial_val, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (slider_out) *slider_out = slider;
    return pct;
}

/* ================================================================
 * Public Interface
 * ================================================================ */

void settings_create_bright(lv_obj_t *parent)
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

    /* Battery hint */
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "(Brightness affects battery life)");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x304050), 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void settings_cleanup_bright_timers(void)
{
    /* Brightness tab has no timers */
}

void settings_nullify_bright_pointers(void)
{
    s_disp_pct = s_kbd_pct = NULL;
    s_disp_slider = s_kbd_slider = NULL;
}
