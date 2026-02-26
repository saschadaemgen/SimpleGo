/**
 * @file ui_main.c
 * @brief Main Screen — Unread Chats Dashboard
 *
 * Shows contacts with unread messages in compact row style.
 * If no unread messages exist, shows "No new messages" hint.
 *
 * Session 37d: Redesigned from static placeholder to live unread list
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
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_MAIN";
static lv_obj_t *screen = NULL;
static lv_obj_t *list_container = NULL;
static lv_obj_t *empty_label = NULL;

/* ============== Layout ============== */

#define LIST_Y          (UI_HEADER_H + 2)
#define SG_LIST_H          (UI_SCREEN_H - UI_HEADER_H - UI_NAV_H - 4)

/* Row styling — matches ui_contacts.c */
#define ROW_H           28
#define ROW_GAP         2
#define ACCENT_W        3
#define ACCENT_H        18
#define ROW_BG          lv_color_hex(0x000810)
#define ROW_BG_PRESS    lv_color_hex(0x001420)
#define ROW_BORDER_OPA  ((lv_opa_t)40)
#define NAME_ACTIVE     lv_color_hex(0xD0E8E8)

/* Name truncation */
#define NAME_MAX_DISPLAY  25

/* ============== Navigation Callbacks ============== */

static void on_chats(lv_event_t *e) { (void)e; ui_manager_show_screen(UI_SCREEN_CONTACTS, LV_SCR_LOAD_ANIM_NONE); }
static void on_new(lv_event_t *e)   { (void)e; ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE); }
static void on_dev(lv_event_t *e)   { (void)e; ui_manager_show_screen(UI_SCREEN_DEVELOPER, LV_SCR_LOAD_ANIM_NONE); }
static void on_sys(lv_event_t *e)   { (void)e; ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE); }

/* ============== Helpers ============== */

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

/* ============== Row Click → Open Chat ============== */

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

/* ============== Unread Contact Row ============== */

static lv_obj_t *create_unread_row(lv_obj_t *parent, int idx, contact_t *c,
                                     uint16_t total, uint16_t unread)
{
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
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Green accent bar — always green since these are unread */
    lv_obj_t *accent = lv_obj_create(row);
    lv_obj_set_size(accent, ACCENT_W, ACCENT_H);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, -2, 0);
    lv_obj_set_style_bg_color(accent, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_bg_opa(accent, (lv_opa_t)220, 0);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Name */
    char display_name[32];
    truncate_name(c->name, display_name, sizeof(display_name));
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text(name_lbl, display_name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(name_lbl, UI_FONT, 0);
    lv_obj_set_style_text_color(name_lbl, NAME_ACTIVE, 0);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    /* Check mark — green for unread */
    lv_obj_t *check_lbl = lv_label_create(row);
    lv_obj_set_style_text_font(check_lbl, UI_FONT_SM, 0);
    lv_label_set_text(check_lbl, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(check_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_align_to(check_lbl, name_lbl, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* Message count — "total ✉ unread" green */
    lv_obj_t *count_lbl = lv_label_create(row);
    lv_obj_set_style_text_font(count_lbl, &lv_font_montserrat_10, 0);
    lv_label_set_text_fmt(count_lbl, "%u " LV_SYMBOL_ENVELOPE " %u", total, unread);
    lv_obj_set_style_text_color(count_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_align(count_lbl, LV_ALIGN_RIGHT_MID, -34, 0);

    /* E2EE badge — far right */
    lv_obj_t *e2ee_lbl = lv_label_create(row);
    lv_label_set_text(e2ee_lbl, "E2EE");
    lv_obj_set_style_text_font(e2ee_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(e2ee_lbl, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_opa(e2ee_lbl, (lv_opa_t)160, 0);
    lv_obj_align(e2ee_lbl, LV_ALIGN_RIGHT_MID, -2, 0);

    /* Click event */
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_row_click, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    return row;
}

/* ============== Screen Create ============== */

lv_obj_t *ui_main_create(void)
{
    ESP_LOGI(TAG, "Creating main...");

    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* Header */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "SimpleGo");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(title, 4, 2);

    lv_obj_t *ver = lv_label_create(screen);
    lv_label_set_text(ver, UI_VERSION);
    lv_obj_set_style_text_color(ver, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(ver, LV_ALIGN_TOP_RIGHT, -4, 2);

    ui_create_line(screen, UI_HEADER_H);

    /* Scrollable list area */
    list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, LV_PCT(100), SG_LIST_H);
    lv_obj_set_pos(list_container, 0, LIST_Y);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container, 0, 0);
    lv_obj_set_style_pad_all(list_container, 0, 0);
    lv_obj_set_style_pad_row(list_container, ROW_GAP, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_OFF);

    /* Empty state label */
    empty_label = lv_label_create(list_container);
    lv_label_set_text(empty_label, "No new messages");
    lv_obj_set_style_text_color(empty_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(empty_label, UI_FONT, 0);
    lv_obj_set_width(empty_label, LV_PCT(100));
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(empty_label, 60, 0);

    /* Nav bar */
    ui_create_nav_bar(screen);

    lv_obj_t *b1 = ui_create_nav_btn(screen, "CHATS", 0);
    lv_obj_t *b2 = ui_create_nav_btn(screen, "NEW", 1);
    lv_obj_t *b3 = ui_create_nav_btn(screen, "DEV", 2);
    lv_obj_t *b4 = ui_create_nav_btn(screen, "SYS", 3);

    lv_obj_add_event_cb(b1, on_chats, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b2, on_new, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b3, on_dev, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b4, on_sys, LV_EVENT_CLICKED, NULL);

    /* Initial populate */
    ui_main_refresh();

    return screen;
}

/* ============== Refresh — Scan for Unread ============== */

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
}
