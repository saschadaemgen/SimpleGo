/**
 * SimpleGo - ui_theme.h
 * SimpleGo cyberpunk theme, colors, fonts, and widget helpers
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"
#include "simplego_fonts.h"   // Session 37c: RAM fonts with umlaut fallback

#ifdef __cplusplus
extern "C" {
#endif

#define UI_VERSION "v0.2.0-beta"

/* Colors - Cyberpunk Neon */
#define UI_COLOR_BG           lv_color_hex(0x000000)
#define UI_COLOR_PRIMARY      lv_color_hex(0x00D4FF)  /* Cyan */
#define UI_COLOR_ACCENT       lv_color_hex(0xFF00FF)  /* Magenta */
#define UI_COLOR_SECONDARY    lv_color_hex(0x00FF66)  /* Green */
#define UI_COLOR_WARNING      lv_color_hex(0xFFCC00)  /* Yellow */
#define UI_COLOR_ERROR        lv_color_hex(0xFF3366)  /* Red */

#define UI_COLOR_TEXT         lv_color_hex(0x00D4FF)
#define UI_COLOR_TEXT_DIM     lv_color_hex(0x006680)
#define UI_COLOR_TEXT_WHITE   lv_color_hex(0xFFFFFF)

#define UI_COLOR_LINE         lv_color_hex(0x00D4FF)
#define UI_COLOR_LINE_DIM     lv_color_hex(0x003344)

/* v2 chat colors */
#define UI_COLOR_ENCRYPT      lv_color_hex(0x2a9d5c)  /* Muted green */
#define UI_COLOR_INPUT_BG     lv_color_hex(0x000d14)  /* Input field fill */
#define UI_COLOR_META_DIM     lv_color_hex(0x405060)  /* Bubble timestamps */

/* Device frame border */
#define UI_FRAME_W            2
#define UI_FRAME_COLOR        lv_color_hex(0x003344)

/* Layout - legacy (contacts, settings, developer screens) */
#define UI_SCREEN_W           320
#define UI_SCREEN_H           240
#define UI_HEADER_H           20
#define UI_NAV_H              32
#define UI_BTN_H              28
#define UI_CONTENT_Y          (UI_HEADER_H + 2)
#define UI_CONTENT_H          (UI_SCREEN_H - UI_HEADER_H - UI_NAV_H - 4)

/* v2 status bar */
#define UI_STATUS_H           16

/* Fonts — RAM copies with German umlaut fallback (Session 37c) */
#define UI_FONT               &simplego_font_14
#define UI_FONT_MD            &simplego_font_12
#define UI_FONT_SM            &simplego_font_10

/* Legacy widget helpers */
void ui_theme_init(void);
void ui_theme_apply(lv_obj_t *obj);
lv_obj_t *ui_create_header(lv_obj_t *parent, const char *title, const char *right_text);
lv_obj_t *ui_create_back_btn(lv_obj_t *parent);
lv_obj_t *ui_create_nav_bar(lv_obj_t *parent);
lv_obj_t *ui_create_nav_btn(lv_obj_t *parent, const char *text, int index);
lv_obj_t *ui_create_line(lv_obj_t *parent, lv_coord_t y);
lv_obj_t *ui_create_btn(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w);
lv_obj_t *ui_create_tab_btn(lv_obj_t *parent, const char *text, int index, int total, bool active);
lv_obj_t *ui_create_switch(lv_obj_t *parent, const char *label, lv_coord_t x, lv_coord_t y, bool state);

/* Session 48: ui_create_status_bar removed. Use ui_statusbar.h instead. */

/**
 * @brief Session 39f: Centralized style helpers
 * Replaces identical static copies in ui_chat.c, ui_contacts.c, ui_settings.c
 */
void ui_style_reset(lv_obj_t *obj);
void ui_style_black(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif
