/**
 * @file ui_contacts.c
 * @brief Contacts Screen - Matches Chat Layout Pixel-for-Pixel
 *
 * Layout (320x240, identical to ui_chat.c):
 *   +-----------------------------------------+
 *   | [GO]        [12:00] [WiFi] [Bat]  18px  |
 *   | ~~~~~~ glow line ~~~~~~~~~~~~~~~~   1px  |
 *   | [<]  Contacts              2/128  26px  |
 *   | ~~~~~~ dim line ~~~~~~~~~~~~~~~~~   1px  |
 *   | Contact rows (scrollable)        158px  |
 *   | < Back                     + New  36px  |
 *   +-----------------------------------------+
 *   Total: 18+1+26+1+158+36 = 240
 *
 * Session 39f: Split into 3 modules:
 *   - ui_contacts.c      (this file - screen, header, bar, search, events)
 *   - ui_contacts_row.c  (6-object row rendering)
 *   - ui_contacts_popup.c (delete/info popups)
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_contacts.h"
#include "ui_contacts_row.h"
#include "ui_contacts_popup.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "ui_connect.h"
#include "smp_types.h"
#include "smp_tasks.h"
#include "smp_contacts.h"
#include "smp_storage.h"
#include "smp_history.h"
#include <string.h>
#include <stdio.h>


/* ============== Layout (same as ui_chat.c) ============== */

#define HDR_Y           (UI_STATUS_H + 1)
#define HDR_H           26
#define DIM_Y           (HDR_Y + HDR_H)
#define BAR_H           36
#define BAR_Y           (UI_SCREEN_H - BAR_H)
#define LIST_Y          (DIM_Y + 1)
#define SG_LIST_H       (BAR_Y - LIST_Y)

/* Row styling */
#define ROW_GAP         2
#define BTN_W           100

/* ============== State ============== */

static lv_obj_t *screen = NULL;
static lv_obj_t *list_container = NULL;
static lv_obj_t *empty_label = NULL;
static lv_obj_t *count_label = NULL;
static lv_obj_t *bottom_bar = NULL;

/* Search state (37d) */
static lv_obj_t *search_input = NULL;
static lv_obj_t *search_x_btn = NULL;
static lv_group_t *search_group = NULL;
static bool search_active = false;
static char search_query[32] = {0};

/* ============== Forward Declarations ============== */

static void on_go_back(lv_event_t *e);
static void on_hdr_back(lv_event_t *e);
static void on_bar_back(lv_event_t *e);
static void on_bar_new(lv_event_t *e);
static void on_bar_search(lv_event_t *e);
static void on_search_cancel(lv_event_t *e);
static void on_search_input_changed(lv_event_t *e);
static void on_contact_click(lv_event_t *e);
static void on_contact_long_press(lv_event_t *e);
static void rebuild_bottom_bar(void);

/* ============== Style Helpers (from ui_chat.c) ============== */

static void style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* ============== Helpers ============== */

static void generate_contact_name(char *buf, size_t buf_len)
{
    int highest = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) {
            int n = 0;
            if (sscanf(contacts_db.contacts[i].name, "Contact %d", &n) == 1) {
                if (n > highest) highest = n;
            }
        }
    }
    snprintf(buf, buf_len, "Contact %d", highest + 1);
}

static int count_active(void)
{
    int c = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) c++;
    }
    return c;
}

static void update_count_label(void)
{
    if (count_label) {
        int n = count_active();
        if (n > 0) {
            lv_label_set_text_fmt(count_label, "%d/%d", n, MAX_CONTACTS);
        } else {
            lv_label_set_text(count_label, "");
        }
    }
}

/* ============== Header (same structure as chat) ============== */

static void create_contacts_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, HDR_H);
    lv_obj_set_pos(hdr, 0, HDR_Y);
    style_black(hdr);

    /* Back arrow */
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
    lv_obj_add_event_cb(back_btn, on_hdr_back, LV_EVENT_CLICKED, NULL);

    /* "Contacts" title */
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Contacts");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 28, 0);

    /* Contact count */
    count_label = lv_label_create(hdr);
    lv_obj_set_style_text_color(count_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(count_label, UI_FONT_SM, 0);
    lv_obj_align(count_label, LV_ALIGN_RIGHT_MID, -4, 0);

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

/* Thin vertical separator between nav buttons */
static void create_bar_separator(lv_obj_t *parent, lv_coord_t x)
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

/* ============== Bottom Bar (37d: proper buttons + search) ============== */

static void create_bottom_bar(lv_obj_t *parent)
{
    bottom_bar = lv_obj_create(parent);
    lv_obj_set_width(bottom_bar, LV_PCT(100));
    lv_obj_set_height(bottom_bar, BAR_H);
    lv_obj_set_pos(bottom_bar, 0, BAR_Y);
    style_black(bottom_bar);

    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(bottom_bar, (lv_opa_t)38, 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);

    /* --- Back Button (left) --- */
    lv_obj_t *back_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(back_btn, BTN_W, BAR_H);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(back_lbl, UI_FONT, 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_bar_back, LV_EVENT_CLICKED, NULL);

    create_bar_separator(bottom_bar, BTN_W + 3);

    /* --- Search Button (center) --- */
    lv_obj_t *search_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(search_btn, BTN_W, BAR_H);
    lv_obj_align(search_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(search_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(search_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(search_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(search_btn, 0, 0);
    lv_obj_set_style_radius(search_btn, 0, 0);
    lv_obj_set_style_shadow_width(search_btn, 0, 0);

    lv_obj_t *search_lbl = lv_label_create(search_btn);
    lv_label_set_text(search_lbl, LV_SYMBOL_LIST " Search");
    lv_obj_set_style_text_color(search_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(search_lbl, UI_FONT, 0);
    lv_obj_center(search_lbl);
    lv_obj_add_event_cb(search_btn, on_bar_search, LV_EVENT_CLICKED, NULL);

    create_bar_separator(bottom_bar, UI_SCREEN_W - BTN_W - 3);

    /* --- New Button (right) --- */
    lv_obj_t *new_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(new_btn, BTN_W, BAR_H);
    lv_obj_align(new_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(new_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(new_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(new_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(new_btn, 0, 0);
    lv_obj_set_style_radius(new_btn, 0, 0);
    lv_obj_set_style_shadow_width(new_btn, 0, 0);

    lv_obj_t *new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, "New " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(new_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(new_lbl, UI_FONT, 0);
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, on_bar_new, LV_EVENT_CLICKED, NULL);
}

/* ============== Inline Search (37d v2) ============== */

static bool name_matches(const char *name, const char *query)
{
    if (!query[0]) return true;
    size_t qlen = strlen(query);
    for (size_t j = 0; name[j]; j++) {
        bool match = true;
        for (size_t k = 0; k < qlen && match; k++) {
            char nc = name[j + k];
            char qc = query[k];
            if (nc >= 'A' && nc <= 'Z') nc += 32;
            if (qc >= 'A' && qc <= 'Z') qc += 32;
            if (nc != qc) match = false;
        }
        if (match) return true;
    }
    return false;
}

static void filter_contact_list(const char *query)
{
    if (!list_container) return;
    uint32_t child_count = lv_obj_get_child_count(list_container);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *row = lv_obj_get_child(list_container, i);
        if (row == empty_label) continue;
        int idx = (int)(intptr_t)lv_obj_get_user_data(row);
        if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
            if (name_matches(contacts_db.contacts[idx].name, query)) {
                lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if (empty_label && count_active() > 0) {
        lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void rebuild_bottom_bar(void)
{
    if (!bottom_bar) return;

    /* Restore keyboard to chat group */
    lv_indev_t *kb = ui_chat_get_keyboard_indev();
    if (kb && search_group) {
        lv_indev_set_group(kb, NULL);
    }

    if (search_group) {
        lv_group_delete(search_group);
        search_group = NULL;
    }

    search_active = false;
    search_query[0] = '\0';
    search_input = NULL;
    search_x_btn = NULL;

    lv_obj_clean(bottom_bar);

    /* --- Back Button (left) --- */
    lv_obj_t *back_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(back_btn, BTN_W, BAR_H);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(back_lbl, UI_FONT, 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_bar_back, LV_EVENT_CLICKED, NULL);

    create_bar_separator(bottom_bar, BTN_W + 3);

    /* --- Search Button (center) --- */
    lv_obj_t *search_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(search_btn, BTN_W, BAR_H);
    lv_obj_align(search_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(search_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(search_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(search_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(search_btn, 0, 0);
    lv_obj_set_style_radius(search_btn, 0, 0);
    lv_obj_set_style_shadow_width(search_btn, 0, 0);

    lv_obj_t *search_lbl = lv_label_create(search_btn);
    lv_label_set_text(search_lbl, LV_SYMBOL_LIST " Search");
    lv_obj_set_style_text_color(search_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(search_lbl, UI_FONT, 0);
    lv_obj_center(search_lbl);
    lv_obj_add_event_cb(search_btn, on_bar_search, LV_EVENT_CLICKED, NULL);

    create_bar_separator(bottom_bar, UI_SCREEN_W - BTN_W - 3);

    /* --- New Button (right) --- */
    lv_obj_t *new_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(new_btn, BTN_W, BAR_H);
    lv_obj_align(new_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(new_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(new_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(new_btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(new_btn, 0, 0);
    lv_obj_set_style_radius(new_btn, 0, 0);
    lv_obj_set_style_shadow_width(new_btn, 0, 0);

    lv_obj_t *new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, "New " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(new_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(new_lbl, UI_FONT, 0);
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, on_bar_new, LV_EVENT_CLICKED, NULL);

    /* Un-hide all rows */
    filter_contact_list("");
}

static void on_search_input_changed(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    const char *text = lv_textarea_get_text(ta);
    if (text) {
        strncpy(search_query, text, sizeof(search_query) - 1);
        search_query[sizeof(search_query) - 1] = '\0';
    }
    filter_contact_list(search_query);
}

static void on_search_cancel(lv_event_t *e)
{
    (void)e;
    rebuild_bottom_bar();
}

static void on_bar_search(lv_event_t *e)
{
    (void)e;
    if (ui_contacts_popup_active() || !bottom_bar) return;
    search_active = true;

    lv_obj_clean(bottom_bar);

    /* Full-width textarea */
    search_input = lv_textarea_create(bottom_bar);
    lv_obj_set_size(search_input, UI_SCREEN_W, BAR_H);
    lv_obj_align(search_input, LV_ALIGN_CENTER, 0, 0);
    lv_textarea_set_one_line(search_input, true);
    lv_textarea_set_placeholder_text(search_input, "Search contacts...");
    lv_textarea_set_max_length(search_input, 24);
    lv_obj_set_style_bg_color(search_input, lv_color_hex(0x000818), 0);
    lv_obj_set_style_bg_opa(search_input, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(search_input, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(search_input, UI_FONT, 0);
    lv_obj_set_style_border_width(search_input, 0, 0);
    lv_obj_set_style_radius(search_input, 0, 0);
    lv_obj_set_style_pad_left(search_input, 6, 0);
    lv_obj_set_style_pad_right(search_input, 28, 0);
    lv_obj_add_event_cb(search_input, on_search_input_changed,
                        LV_EVENT_VALUE_CHANGED, NULL);

    /* Neon X button */
    search_x_btn = lv_label_create(bottom_bar);
    lv_label_set_text(search_x_btn, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(search_x_btn, lv_color_hex(0xFF0040), 0);
    lv_obj_set_style_text_font(search_x_btn, UI_FONT, 0);
    lv_obj_align(search_x_btn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_add_flag(search_x_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(search_x_btn, 14);
    lv_obj_add_event_cb(search_x_btn, on_search_cancel, LV_EVENT_CLICKED, NULL);

    /* Input group for T-Deck keyboard */
    search_group = lv_group_create();
    lv_group_add_obj(search_group, search_input);
    lv_group_focus_obj(search_input);

    lv_indev_t *kb = ui_chat_get_keyboard_indev();
    if (kb) {
        lv_indev_set_group(kb, search_group);
    }
    lv_obj_add_state(search_input, LV_STATE_FOCUSED);
}

/* ============== Event Handlers ============== */

static void on_go_back(lv_event_t *e)
{
    (void)e;
    if (ui_contacts_popup_active()) { ui_contacts_popup_close(); return; }
    if (search_active) rebuild_bottom_bar();
    ui_manager_go_back();
}

static void on_hdr_back(lv_event_t *e)
{
    (void)e;
    if (ui_contacts_popup_active()) { ui_contacts_popup_close(); return; }
    if (search_active) rebuild_bottom_bar();
    ui_manager_go_back();
}

static void on_bar_back(lv_event_t *e)
{
    (void)e;
    if (ui_contacts_popup_active()) { ui_contacts_popup_close(); return; }
    if (search_active) rebuild_bottom_bar();
    ui_manager_go_back();
}

static void on_bar_new(lv_event_t *e)
{
    (void)e;
    if (ui_contacts_popup_active()) return;

    char name[32];
    generate_contact_name(name, sizeof(name));

    ui_connect_reset();

    int ret = smp_request_add_contact(name);
    if (ret == 0) {
        ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE);
    }
}

static void on_contact_click(lv_event_t *e)
{
    if (ui_contacts_popup_active()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        if (search_active) rebuild_bottom_bar();
        smp_set_active_contact(idx);
        ui_chat_set_contact(contacts_db.contacts[idx].name);
        smp_request_load_history(idx);
        ui_manager_show_screen(UI_SCREEN_CHAT, LV_SCR_LOAD_ANIM_NONE);
        ui_chat_show_loading();
    }
}

static void on_contact_long_press(lv_event_t *e)
{
    if (ui_contacts_popup_active()) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        ui_contacts_popup_show(screen, idx);
    }
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_contacts_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* Register popup refresh callback */
    ui_contacts_popup_set_refresh_cb(ui_contacts_refresh);

    /* Status Bar (16px) */
    lv_obj_t *go_btn = ui_create_status_bar(screen);
    lv_obj_add_event_cb(go_btn, on_go_back, LV_EVENT_CLICKED, NULL);

    /* Header (26px) */
    create_contacts_header(screen);

    /* List area */
    list_container = lv_obj_create(screen);
    lv_obj_set_width(list_container, LV_PCT(100));
    lv_obj_set_height(list_container, SG_LIST_H);
    lv_obj_set_pos(list_container, 0, LIST_Y);
    style_black(list_container);
    lv_obj_set_style_pad_left(list_container, 0, 0);
    lv_obj_set_style_pad_right(list_container, 0, 0);
    lv_obj_set_style_pad_top(list_container, 4, 0);
    lv_obj_set_style_pad_bottom(list_container, 2, 0);
    lv_obj_set_style_pad_row(list_container, ROW_GAP, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_OFF);

    /* Empty state */
    empty_label = lv_label_create(list_container);
    lv_label_set_text(empty_label, "No contacts yet");
    lv_obj_set_style_text_color(empty_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(empty_label, UI_FONT, 0);
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(empty_label, 50, 0);
    lv_obj_set_width(empty_label, LV_PCT(100));

    /* Bottom bar (36px) */
    create_bottom_bar(screen);

    /* Populate */
    ui_contacts_refresh();
    return screen;
}

/* ============== Refresh ============== */

void ui_contacts_refresh(void)
{
    if (!list_container) return;

    if (search_active && bottom_bar) {
        rebuild_bottom_bar();
    }

    /* Remove old rows (keep empty_label) */
    uint32_t cnt = lv_obj_get_child_count(list_container);
    for (int i = cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(list_container, i);
        if (child != empty_label) {
            lv_obj_delete(child);
        }
    }

    int active = count_active();
    update_count_label();

    if (active == 0) {
        lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

    /* Pre-fetch SD card counts BEFORE entering LVGL render loop */
    uint16_t totals[MAX_CONTACTS] = {0};
    uint16_t unreads[MAX_CONTACTS] = {0};
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        smp_history_get_counts(i, &totals[i], &unreads[i]);
    }

    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        ui_contacts_row_create(list_container, i, &contacts_db.contacts[i],
                               totals[i], unreads[i],
                               on_contact_click, on_contact_long_press);
    }
}
