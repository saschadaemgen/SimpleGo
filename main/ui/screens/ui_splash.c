/**
 * @file ui_splash.c
 * @brief Animated Splash Screen with Live Boot Progress
 *
 * Layout (320x240):
 *   y=75   "Simple" (white) + "Go" (cyan), Montserrat 28
 *   y=108  "private by design" (dim)
 *   y=185  Progress bar (4px, cyan on dark track, 260px wide)
 *   y=200  Status text (dim, live boot updates)
 *   y=228  Version (nearly invisible, bottom-right)
 *
 * Session 48: Complete rewrite with animations and boot integration.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_splash.h"
#include "lvgl.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_SPLASH";

/* ============== LVGL Widgets ============== */

static lv_obj_t *screen         = NULL;
static lv_obj_t *s_progress_bg  = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_status_lbl   = NULL;
static lv_timer_t *s_poll_timer = NULL;

/* ============== Thread-Safe State (Core 0 writes, Core 1 reads) ============== */

static volatile int  s_target_progress = 0;
static volatile bool s_status_dirty    = false;
static volatile bool s_done_triggered  = false;
static char s_status_buf[64] = "";

/* ============== Constants ============== */

#define PROGRESS_MAX_W   260
#define PROGRESS_H       4
#define PROGRESS_X       30
#define PROGRESS_Y       185
#define STATUS_Y         200
#define LOGO_Y           75
#define TAGLINE_Y        108

/* ============== Fade-In Animation ============== */

static void anim_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void fade_in(lv_obj_t *obj, uint32_t duration, uint32_t delay)
{
    lv_obj_set_style_opa(obj, 0, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* ============== Transition to Main ============== */

static void go_to_main(lv_timer_t *t)
{
    lv_timer_del(t);
    if (ui_manager_get_current() != UI_SCREEN_SPLASH) {
        ESP_LOGI(TAG, "Already navigated away from splash, skipping");
        return;
    }
    ui_manager_show_screen(UI_SCREEN_MAIN, LV_SCR_LOAD_ANIM_FADE_ON);
}

/* ============== Poll Timer (100ms) ============== */

static void splash_poll_cb(lv_timer_t *t)
{
    (void)t;

    if (s_status_dirty && s_status_lbl) {
        s_status_dirty = false;
        lv_label_set_text(s_status_lbl, s_status_buf);
    }

    if (s_progress_bar) {
        int target_w = (PROGRESS_MAX_W * s_target_progress) / 100;
        if (target_w < 1 && s_target_progress > 0) target_w = 1;
        int cur_w = lv_obj_get_width(s_progress_bar);
        if (cur_w < target_w) {
            int step = (target_w - cur_w + 3) / 4;
            if (step < 2) step = 2;
            int new_w = cur_w + step;
            if (new_w > target_w) new_w = target_w;
            lv_obj_set_width(s_progress_bar, new_w);
        }
    }

    if (s_target_progress >= 100 && !s_done_triggered) {
        s_done_triggered = true;
        if (s_status_lbl) {
            lv_obj_set_style_text_color(s_status_lbl,
                UI_COLOR_SECONDARY, 0);
        }
        if (s_progress_bar) {
            lv_obj_set_width(s_progress_bar, PROGRESS_MAX_W);
            lv_obj_set_style_bg_color(s_progress_bar,
                UI_COLOR_SECONDARY, 0);
        }
        if (s_poll_timer) {
            lv_timer_delete(s_poll_timer);
            s_poll_timer = NULL;
        }
        lv_timer_create(go_to_main, 800, NULL);
    }
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_splash_create(void)
{
    ESP_LOGI(TAG, "Creating splash...");

    s_target_progress = 0;
    s_status_dirty = false;
    s_done_triggered = false;
    s_status_buf[0] = '\0';

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Logo: "Simple" white + "Go" cyan, centered row --- */
    lv_obj_t *logo_box = lv_obj_create(screen);
    lv_obj_set_size(logo_box, UI_SCREEN_W, LV_SIZE_CONTENT);
    lv_obj_set_pos(logo_box, 0, LOGO_Y);
    lv_obj_set_style_bg_opa(logo_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(logo_box, 0, 0);
    lv_obj_set_style_pad_all(logo_box, 0, 0);
    lv_obj_set_style_pad_column(logo_box, 2, 0);
    lv_obj_set_flex_flow(logo_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(logo_box, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(logo_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *logo_simple = lv_label_create(logo_box);
    lv_label_set_text(logo_simple, "Simple");
    lv_obj_set_style_text_color(logo_simple, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(logo_simple, &lv_font_montserrat_28, 0);

    lv_obj_t *logo_go = lv_label_create(logo_box);
    lv_label_set_text(logo_go, "Go");
    lv_obj_set_style_text_color(logo_go, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(logo_go, &lv_font_montserrat_28, 0);

    /* --- Tagline --- */
    lv_obj_t *tagline = lv_label_create(screen);
    lv_label_set_text(tagline, "private by design");
    lv_obj_set_style_text_color(tagline, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(tagline, UI_FONT, 0);
    lv_obj_set_width(tagline, UI_SCREEN_W);
    lv_obj_set_style_text_align(tagline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(tagline, 0, TAGLINE_Y);

    /* --- Progress Track (dark, 4px, rounded) --- */
    s_progress_bg = lv_obj_create(screen);
    lv_obj_set_size(s_progress_bg, PROGRESS_MAX_W, PROGRESS_H);
    lv_obj_set_pos(s_progress_bg, PROGRESS_X, PROGRESS_Y);
    lv_obj_set_style_bg_color(s_progress_bg, lv_color_hex(0x162028), 0);
    lv_obj_set_style_bg_opa(s_progress_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_progress_bg, 0, 0);
    lv_obj_set_style_radius(s_progress_bg, 2, 0);
    lv_obj_set_style_pad_all(s_progress_bg, 0, 0);
    lv_obj_clear_flag(s_progress_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_progress_bg, LV_OBJ_FLAG_CLICKABLE);

    /* --- Progress Fill (cyan, starts at 1px) --- */
    s_progress_bar = lv_obj_create(s_progress_bg);
    lv_obj_set_size(s_progress_bar, 1, PROGRESS_H);
    lv_obj_set_pos(s_progress_bar, 0, 0);
    lv_obj_set_style_bg_color(s_progress_bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_progress_bar, 0, 0);
    lv_obj_set_style_radius(s_progress_bar, 2, 0);
    lv_obj_clear_flag(s_progress_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_progress_bar, LV_OBJ_FLAG_CLICKABLE);

    /* --- Status Text (centered, below progress bar) --- */
    s_status_lbl = lv_label_create(screen);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_style_text_color(s_status_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_status_lbl, UI_FONT_SM, 0);
    lv_obj_set_width(s_status_lbl, UI_SCREEN_W);
    lv_obj_set_style_text_align(s_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_lbl, 0, STATUS_Y);

    /* --- Version (barely visible) --- */
    lv_obj_t *ver = lv_label_create(screen);
    lv_label_set_text(ver, UI_VERSION);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_font(ver, UI_FONT_SM, 0);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

    /* --- Animations --- */
    fade_in(logo_box, 800, 0);
    fade_in(tagline, 600, 400);
    fade_in(s_progress_bg, 400, 700);
    fade_in(s_status_lbl, 400, 700);

    /* --- Poll timer (100ms) --- */
    s_poll_timer = lv_timer_create(splash_poll_cb, 100, NULL);

    return screen;
}

/* ============== Public API (Core 0) ============== */

void ui_splash_set_status(const char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    if (len >= sizeof(s_status_buf)) len = sizeof(s_status_buf) - 1;
    memcpy(s_status_buf, s, len);
    s_status_buf[len] = '\0';
    s_status_dirty = true;
}

void ui_splash_set_progress(int p)
{
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    s_target_progress = p;
}
