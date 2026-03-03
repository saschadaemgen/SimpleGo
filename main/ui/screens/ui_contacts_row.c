/**
 * @file ui_contacts_row.c
 * @brief Contact Row Rendering - Restored 6-Object Design
 *
 * Session 39f: Extracted from ui_contacts.c with FIXED layout.
 *
 * Row layout (28px height):
 *   [6px gap][3px accent][5px gap][Name 148px][Checkmark @166][Count][E2EE]
 *
 * 6 LVGL objects per row:
 *   0. Row container (lv_obj)
 *   1. Accent bar (lv_obj, 3px colored, radius 1)
 *   2. Name+Check flex box (lv_obj, flex row, 174px max)
 *     2a. Name label (lv_label, max 152px, clips long names)
 *     2b. Status icon (lv_label, checkmark or "...", sits right after name)
 *   3. Message count (lv_label, "total ✉ unread")
 *   4. E2EE badge (lv_label)
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_contacts_row.h"
#include "ui_theme.h"
#include <string.h>

/* Row styling */
#define ROW_H           28
#define ACCENT_W        3
#define ACCENT_H        18

#define ROW_BG          lv_color_hex(0x000810)
#define ROW_BG_PRESS    lv_color_hex(0x001420)
#define ROW_BORDER_OPA  ((lv_opa_t)40)
#define NAME_ACTIVE     lv_color_hex(0xD0E8E8)
#define NAME_PENDING    lv_color_hex(0x607878)

#define NAME_MAX_DISPLAY  25

/* Truncate name with "..." */
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

/* ============== Row Creation ============== */

lv_obj_t *ui_contacts_row_create(lv_obj_t *parent, int idx, contact_t *c,
                                  uint16_t total, uint16_t unread,
                                  lv_event_cb_t on_click,
                                  lv_event_cb_t on_long_press)
{
    bool conn = c->have_srv_dh;
    bool has_unread = (unread > 0);

    /* Status color for accent bar */
    lv_color_t status_clr;
    lv_opa_t accent_opa;
    if (has_unread) {
        status_clr = UI_COLOR_SECONDARY;
        accent_opa = (lv_opa_t)220;
    } else if (conn) {
        status_clr = UI_COLOR_PRIMARY;
        accent_opa = (lv_opa_t)200;
    } else {
        status_clr = UI_COLOR_TEXT_DIM;
        accent_opa = (lv_opa_t)80;
    }

    /* === Row container — zero padding, all positions absolute === */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, ROW_BG_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(row, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_opa(row, ROW_BORDER_OPA, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(row, (void *)(intptr_t)idx);

    /* === Object 1: Accent bar — 3px wide, proper spacing === */
    lv_obj_t *accent = lv_obj_create(row);
    lv_obj_set_size(accent, ACCENT_W, ACCENT_H);
    lv_obj_set_pos(accent, 6, (ROW_H - ACCENT_H) / 2);
    lv_obj_set_style_bg_color(accent, status_clr, 0);
    lv_obj_set_style_bg_opa(accent, accent_opa, 0);
    lv_obj_set_style_radius(accent, 1, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* === Objects 2+3: Name + Checkmark in flex row container === */
    /*    Check sits directly after name with 1 space gap.       */
    /*    Container has max width so it never overruns counts.   */
    lv_obj_t *name_box = lv_obj_create(row);
    lv_obj_set_height(name_box, ROW_H);
    lv_obj_set_width(name_box, 174);
    lv_obj_set_style_bg_opa(name_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_box, 0, 0);
    lv_obj_set_style_pad_all(name_box, 0, 0);
    lv_obj_set_style_pad_column(name_box, 2, 0);
    lv_obj_set_flex_flow(name_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(name_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(name_box, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_clear_flag(name_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    char display_name[32];
    truncate_name(c->name, display_name, sizeof(display_name));
    lv_obj_t *name_lbl = lv_label_create(name_box);
    lv_label_set_text(name_lbl, display_name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_max_width(name_lbl, 152, 0);
    lv_obj_set_style_text_font(name_lbl, UI_FONT, 0);

    if (has_unread)       lv_obj_set_style_text_color(name_lbl, UI_COLOR_SECONDARY, 0);
    else if (conn)        lv_obj_set_style_text_color(name_lbl, NAME_ACTIVE, 0);
    else                  lv_obj_set_style_text_color(name_lbl, NAME_PENDING, 0);

    lv_obj_t *check_lbl = lv_label_create(name_box);
    lv_obj_set_style_text_font(check_lbl, UI_FONT_SM, 0);
    if (conn) {
        lv_label_set_text(check_lbl, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(check_lbl,
            has_unread ? UI_COLOR_SECONDARY : UI_COLOR_PRIMARY, 0);
    } else {
        lv_label_set_text(check_lbl, "...");
        lv_obj_set_style_text_color(check_lbl, UI_COLOR_TEXT_DIM, 0);
    }

    /* === Object 4: Message count — always visible === */
    lv_obj_t *count_lbl = lv_label_create(row);
    lv_obj_set_style_text_font(count_lbl, &lv_font_montserrat_10, 0);
    if (total > 0 || unread > 0) {
        lv_label_set_text_fmt(count_lbl, "%u " LV_SYMBOL_ENVELOPE " %u", total, unread);
    } else {
        lv_label_set_text(count_lbl, "0 " LV_SYMBOL_ENVELOPE " 0");
    }
    if (has_unread) {
        lv_obj_set_style_text_color(count_lbl, UI_COLOR_SECONDARY, 0);
    } else {
        lv_obj_set_style_text_color(count_lbl, UI_COLOR_TEXT_DIM, 0);
    }
    lv_obj_align(count_lbl, LV_ALIGN_RIGHT_MID, -36, 0);

    /* === Object 5: E2EE badge — far right === */
    lv_obj_t *e2ee_lbl = lv_label_create(row);
    lv_label_set_text(e2ee_lbl, "E2EE");
    lv_obj_set_style_text_font(e2ee_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(e2ee_lbl, UI_COLOR_ENCRYPT, 0);
    if (!has_unread) {
        lv_obj_set_style_opa(e2ee_lbl, (lv_opa_t)160, 0);
    }
    lv_obj_align(e2ee_lbl, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Events */
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_click, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
    lv_obj_add_event_cb(row, on_long_press, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)idx);

    return row;
}
