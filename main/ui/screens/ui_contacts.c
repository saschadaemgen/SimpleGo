/**
 * @file ui_contacts.c
 * @brief Contacts Screen - Rectangular Blue Card Design
 *
 * Layout (320x240, identical to ui_chat.c):
 *   +-----------------------------------------+
 *   | [GO]        [12:00] [WiFi] [Bat]  16px  |
 *   | ~~~~~~ glow line ~~~~~~~~~~~~~~~~   1px  |
 *   | [<]  Contacts              2/128  26px  |
 *   | ~~~~~~ dim line ~~~~~~~~~~~~~~~~~   1px  |
 *   | Contact cards (scrollable)        160px  |
 *   | < Back                     + New  36px  |
 *   +-----------------------------------------+
 *   Total: 16+1+26+1+160+36 = 240
 *
 * Contact Card Design (rectangular, no radius):
 *   +------------------------------------------+
 *   |  Alice                    [OK] connected  |  <- Name + Status
 *   |  X3DH + Ratchet              Slot 0       |  <- Encryption + Meta
 *   +------------------------------------------+
 *
 * Session 37: Redesign + encrypted chat history integration
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_contacts.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "ui_connect.h"    // 36d: ui_connect_reset()
#include "smp_types.h"
#include "smp_tasks.h"
#include "smp_contacts.h"
#include "smp_storage.h"   // 36a: smp_storage_delete for NVS key cleanup
#include "smp_history.h"   // Session 37: encrypted chat history on SD
extern void smp_clear_42d(int idx);  // 36b: reset 42d bitmap on delete
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_CONTACTS";

/* ============== Layout (same as ui_chat.c) ============== */

#define HDR_Y           (UI_STATUS_H + 1)       /* 17 */
#define HDR_H           26
#define DIM_Y           (HDR_Y + HDR_H)          /* 43 */
#define BAR_H           36
#define BAR_Y           (UI_SCREEN_H - BAR_H)    /* 204 */
#define LIST_Y          (DIM_Y + 1)              /* 44 */
#define LIST_H          (BAR_Y - LIST_Y)          /* 160 */

/* Card styling — rectangular blue cards, matching incoming bubble palette */
#define CARD_H          44
#define CARD_GAP        3
#define CARD_PAD_L      10
#define CARD_PAD_R      8

/* Colors — matching chat incoming bubble aesthetic */
#define CARD_BG         lv_color_hex(0x000010)   /* same as BUBBLE_BG_IN  */
#define CARD_BG_PRESS   lv_color_hex(0x001420)   /* subtle highlight      */
#define CARD_BORDER_OPA ((lv_opa_t)50)           /* slightly more than bubbles */
#define NAME_ACTIVE     lv_color_hex(0xD0E8E8)   /* light cyan-white      */
#define NAME_PENDING    lv_color_hex(0x607878)   /* muted                 */
#define STATUS_CONN     UI_COLOR_SECONDARY       /* green glow            */
#define STATUS_WAIT     UI_COLOR_TEXT_DIM         /* dim gray              */

/* ============== State ============== */

static lv_obj_t *screen = NULL;
static lv_obj_t *list_container = NULL;
static lv_obj_t *empty_label = NULL;
static lv_obj_t *count_label = NULL;

/* Long-press popup */
static lv_obj_t *popup_overlay = NULL;
static int popup_contact_idx = -1;

/* ============== Forward Declarations ============== */

static void on_go_back(lv_event_t *e);
static void on_hdr_back(lv_event_t *e);
static void on_bar_back(lv_event_t *e);
static void on_bar_new(lv_event_t *e);
static void on_contact_click(lv_event_t *e);
static void on_contact_long_press(lv_event_t *e);
static void popup_close(void);
static void on_popup_delete(lv_event_t *e);
static void on_popup_info(lv_event_t *e);
static void on_popup_cancel(lv_event_t *e);
static void on_popup_overlay_click(lv_event_t *e);

/* ============== Style Helper ============== */

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

    /* Back arrow - same as chat */
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

/* ============== Bottom Bar ============== */

static void create_bottom_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_height(bar, BAR_H);
    lv_obj_set_pos(bar, 0, BAR_Y);
    style_black(bar);

    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(bar, (lv_opa_t)38, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);

    lv_obj_t *back_lbl = lv_label_create(bar);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(back_lbl, UI_FONT, 0);
    lv_obj_align(back_lbl, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_flag(back_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_lbl, on_bar_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t *new_lbl = lv_label_create(bar);
    lv_label_set_text(new_lbl, LV_SYMBOL_PLUS " New");
    lv_obj_set_style_text_color(new_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(new_lbl, UI_FONT, 0);
    lv_obj_align(new_lbl, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_flag(new_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(new_lbl, on_bar_new, LV_EVENT_CLICKED, NULL);
}

/* ============== Contact Card ============== */
/*
 * Rectangular blue card, two-line layout:
 *
 *   +------------------------------------------+
 *   |  Alice                    [OK] connected  |
 *   |  X3DH + Ratchet              Slot 0       |
 *   +------------------------------------------+
 *
 * No border-radius, no accent bar hacks.
 * Clean rectangular card matching incoming bubble palette.
 */

static lv_obj_t *create_contact_card(lv_obj_t *parent, int idx, contact_t *c)
{
    bool conn = c->have_srv_dh;

    /* === Card container === */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), CARD_H);

    /* Rectangular: radius 0, clean blue background */
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_set_style_bg_color(card, CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    /* Subtle border — primary color at low opacity */
    lv_obj_set_style_border_color(card, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, CARD_BORDER_OPA, 0);

    /* Press state */
    lv_obj_set_style_bg_color(card, CARD_BG_PRESS, LV_STATE_PRESSED);

    /* Internal padding */
    lv_obj_set_style_pad_left(card, CARD_PAD_L, 0);
    lv_obj_set_style_pad_right(card, CARD_PAD_R, 0);
    lv_obj_set_style_pad_top(card, 5, 0);
    lv_obj_set_style_pad_bottom(card, 4, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* === Line 1: Contact Name (left) + Status (right) === */

    /* Name — prominent, 14pt */
    lv_obj_t *name_lbl = lv_label_create(card);
    lv_label_set_text(name_lbl, c->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 170);
    lv_obj_set_style_text_font(name_lbl, UI_FONT, 0);
    lv_obj_set_style_text_color(name_lbl, conn ? NAME_ACTIVE : NAME_PENDING, 0);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Status text — right aligned, top line */
    lv_obj_t *status_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(status_lbl, UI_FONT_SM, 0);
    if (conn) {
        lv_label_set_text(status_lbl, LV_SYMBOL_OK " connected");
        lv_obj_set_style_text_color(status_lbl, STATUS_CONN, 0);
    } else {
        lv_label_set_text(status_lbl, "waiting...");
        lv_obj_set_style_text_color(status_lbl, STATUS_WAIT, 0);
    }
    lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, 0, 1);

    /* === Line 2: Encryption badge (left) + Slot info (right) === */

    /* Encryption — the signature look */
    lv_obj_t *enc_lbl = lv_label_create(card);
    if (conn) {
        lv_label_set_text(enc_lbl, "X3DH + Double Ratchet");
    } else {
        lv_label_set_text(enc_lbl, "handshake pending");
    }
    lv_obj_set_style_text_font(enc_lbl, UI_FONT_SM, 0);
    lv_obj_set_style_text_color(enc_lbl, conn ? UI_COLOR_ENCRYPT : UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(enc_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Slot info — dim, right side */
    lv_obj_t *slot_lbl = lv_label_create(card);
    lv_label_set_text_fmt(slot_lbl, "Slot %d", idx);
    lv_obj_set_style_text_font(slot_lbl, UI_FONT_SM, 0);
    lv_obj_set_style_text_color(slot_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(slot_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* === Events === */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, on_contact_click, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
    lv_obj_add_event_cb(card, on_contact_long_press, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)idx);

    return card;
}

/* ============== Fullscreen Popup ============== */

static lv_obj_t *create_popup_action(lv_obj_t *parent, const char *icon,
                                      const char *text, lv_color_t color,
                                      lv_event_cb_t cb)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(row, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x001420), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(row, color, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_opa(row, (lv_opa_t)50, 0);
    lv_obj_set_style_radius(row, 0, 0);   /* rectangular, matching cards */
    lv_obj_set_style_pad_left(row, 16, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_lbl = lv_label_create(row);
    lv_label_set_text(icon_lbl, icon);
    lv_obj_set_style_text_color(icon_lbl, color, 0);
    lv_obj_set_style_text_font(icon_lbl, UI_FONT, 0);
    lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *txt_lbl = lv_label_create(row);
    lv_label_set_text(txt_lbl, text);
    lv_obj_set_style_text_color(txt_lbl, color, 0);
    lv_obj_set_style_text_font(txt_lbl, UI_FONT, 0);
    lv_obj_align(txt_lbl, LV_ALIGN_LEFT_MID, 24, 0);

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);
    return row;
}

static void show_popup(int contact_idx)
{
    if (popup_overlay) popup_close();
    popup_contact_idx = contact_idx;
    contact_t *c = &contacts_db.contacts[contact_idx];

    /* Fullscreen dark background */
    popup_overlay = lv_obj_create(screen);
    lv_obj_set_size(popup_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(popup_overlay, 0, 0);
    lv_obj_set_style_bg_color(popup_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(popup_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup_overlay, 0, 0);
    lv_obj_set_style_radius(popup_overlay, 0, 0);
    lv_obj_set_style_pad_all(popup_overlay, 0, 0);
    lv_obj_clear_flag(popup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Header area - contact name large */
    lv_obj_t *name_lbl = lv_label_create(popup_overlay);
    lv_label_set_text(name_lbl, c->name);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, 280);
    lv_obj_set_style_text_color(name_lbl, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name_lbl, UI_FONT, 0);
    lv_obj_set_pos(name_lbl, 20, 24);

    /* Status line */
    lv_obj_t *status_lbl = lv_label_create(popup_overlay);
    lv_label_set_text_fmt(status_lbl, "%s  |  Slot %d",
        c->have_srv_dh ? "connected" : "waiting...", contact_idx);
    lv_obj_set_style_text_color(status_lbl,
        c->have_srv_dh ? UI_COLOR_SECONDARY : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(status_lbl, UI_FONT_SM, 0);
    lv_obj_set_pos(status_lbl, 20, 44);

    /* Encryption badge */
    lv_obj_t *enc_lbl = lv_label_create(popup_overlay);
    lv_label_set_text(enc_lbl, "X3DH + Double Ratchet");
    lv_obj_set_style_text_color(enc_lbl, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_text_font(enc_lbl, UI_FONT_SM, 0);
    lv_obj_set_pos(enc_lbl, 20, 60);

    /* Separator line */
    lv_obj_t *sep = lv_obj_create(popup_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 82);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Actions container */
    lv_obj_t *actions = lv_obj_create(popup_overlay);
    lv_obj_set_size(actions, 280, 130);
    lv_obj_set_pos(actions, 20, 92);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_pad_row(actions, 6, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    create_popup_action(actions, LV_SYMBOL_TRASH, "Delete contact",
                        UI_COLOR_ERROR, on_popup_delete);
    create_popup_action(actions, LV_SYMBOL_EYE_OPEN, "Contact info",
                        UI_COLOR_PRIMARY, on_popup_info);

    /* Cancel */
    lv_obj_t *cancel = lv_label_create(popup_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_popup_cancel, LV_EVENT_CLICKED, NULL);
}

static void show_info_popup(int idx)
{
    contact_t *c = &contacts_db.contacts[idx];
    popup_close();
    popup_contact_idx = idx;

    /* Fullscreen info */
    popup_overlay = lv_obj_create(screen);
    lv_obj_set_size(popup_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(popup_overlay, 0, 0);
    lv_obj_set_style_bg_color(popup_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(popup_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup_overlay, 0, 0);
    lv_obj_set_style_radius(popup_overlay, 0, 0);
    lv_obj_set_style_pad_all(popup_overlay, 0, 0);
    lv_obj_clear_flag(popup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(popup_overlay);
    lv_label_set_text(title, "Contact Info");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_SM, 0);
    lv_obj_set_pos(title, 20, 20);

    /* Name */
    lv_obj_t *name = lv_label_create(popup_overlay);
    lv_label_set_text(name, c->name);
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, UI_FONT, 0);
    lv_obj_set_pos(name, 20, 38);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(popup_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 62);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Info rows */
    int y = 74;
    const int line_h = 18;

    lv_obj_t *s1 = lv_label_create(popup_overlay);
    lv_label_set_text_fmt(s1, "Status      %s", c->have_srv_dh ? "connected" : "waiting...");
    lv_obj_set_style_text_color(s1, c->have_srv_dh ? UI_COLOR_SECONDARY : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s1, UI_FONT_SM, 0);
    lv_obj_set_pos(s1, 20, y); y += line_h;

    lv_obj_t *s2 = lv_label_create(popup_overlay);
    lv_label_set_text_fmt(s2, "Slot        %d / %d", idx, MAX_CONTACTS);
    lv_obj_set_style_text_color(s2, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s2, UI_FONT_SM, 0);
    lv_obj_set_pos(s2, 20, y); y += line_h;

    lv_obj_t *s3 = lv_label_create(popup_overlay);
    lv_label_set_text(s3, "Protocol    SimpleX SMP v7");
    lv_obj_set_style_text_color(s3, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s3, UI_FONT_SM, 0);
    lv_obj_set_pos(s3, 20, y); y += line_h;

    lv_obj_t *s4 = lv_label_create(popup_overlay);
    lv_label_set_text(s4, "Encryption  X3DH + Double Ratchet");
    lv_obj_set_style_text_color(s4, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_text_font(s4, UI_FONT_SM, 0);
    lv_obj_set_pos(s4, 20, y); y += line_h;

    lv_obj_t *s5 = lv_label_create(popup_overlay);
    lv_label_set_text(s5, "Layers      TLS 1.3 + NaCl + Ratchet");
    lv_obj_set_style_text_color(s5, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_text_font(s5, UI_FONT_SM, 0);
    lv_obj_set_pos(s5, 20, y);

    /* Back */
    lv_obj_t *cancel = lv_label_create(popup_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_popup_cancel, LV_EVENT_CLICKED, NULL);
}

static void popup_close(void)
{
    if (popup_overlay) {
        lv_obj_delete(popup_overlay);
        popup_overlay = NULL;
    }
    popup_contact_idx = -1;
}

/* ============== Popup Actions ============== */

static void on_popup_delete(lv_event_t *e)
{
    (void)e;
    if (popup_contact_idx < 0 || popup_contact_idx >= MAX_CONTACTS) {
        popup_close();
        return;
    }

    int idx = popup_contact_idx;
    contact_t *c = &contacts_db.contacts[idx];
    ESP_LOGW(TAG, "DELETE [%d] '%s'", idx, c->name);

    memset(c, 0, sizeof(contact_t));

    // 36a+36b: Clean up ALL orphaned NVS keys for this contact
    {
        const char *prefixes[] = {"rat_", "peer_", "hand_", "rq_"};
        char nkey[16];
        for (int k = 0; k < 4; k++) {
            snprintf(nkey, sizeof(nkey), "%s%02x", prefixes[k], idx);
            smp_storage_delete(nkey);
        }
        ESP_LOGI(TAG, "NVS keys cleaned: rat/peer/hand/rq_%02x", idx);
    }

    smp_clear_42d(idx);  // 36b: allow 42d re-handshake in this slot

    // 36d: Clear chat bubbles for deleted contact
    ui_chat_clear_contact(idx);

    // Session 37: Delete encrypted chat history from SD
    smp_history_delete(idx);

    // 36d: Reset QR cache
    ui_connect_reset();

    int n = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) n++;
    }
    contacts_db.num_contacts = n;
    save_contacts_to_nvs();

    popup_close();
    ui_contacts_refresh();
}

static void on_popup_info(lv_event_t *e)
{
    (void)e;
    int idx = popup_contact_idx;
    if (idx < 0 || idx >= MAX_CONTACTS) { popup_close(); return; }
    show_info_popup(idx);
}

static void on_popup_cancel(lv_event_t *e)
{
    (void)e;
    popup_close();
}

static void on_popup_overlay_click(lv_event_t *e)
{
    (void)e;
    popup_close();
}

/* ============== Event Handlers ============== */

static void on_go_back(lv_event_t *e)
{
    (void)e;
    if (popup_overlay) { popup_close(); return; }
    ui_manager_go_back();
}

static void on_hdr_back(lv_event_t *e)
{
    (void)e;
    if (popup_overlay) { popup_close(); return; }
    ui_manager_go_back();
}

static void on_bar_back(lv_event_t *e)
{
    (void)e;
    if (popup_overlay) { popup_close(); return; }
    ui_manager_go_back();
}

static void on_bar_new(lv_event_t *e)
{
    (void)e;
    if (popup_overlay) return;

    char name[32];
    generate_contact_name(name, sizeof(name));

    // 36d: Reset QR screen BEFORE navigating (no stale QR flash)
    ui_connect_reset();

    int ret = smp_request_add_contact(name);
    if (ret == 0) {
        ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE);
    }
}

static void on_contact_click(lv_event_t *e)
{
    if (popup_overlay) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        smp_set_active_contact(idx);
        ui_chat_set_contact(contacts_db.contacts[idx].name);

        // Session 37: Clear old bubbles and request history load from App Task
        ui_chat_clear_contact(idx);
        ui_chat_show_loading();  // Session 37b: show "Loading..." until history arrives
        ui_chat_switch_contact(idx, contacts_db.contacts[idx].name);
        smp_request_load_history(idx);

        ui_manager_show_screen(UI_SCREEN_CHAT, LV_SCR_LOAD_ANIM_NONE);
    }
}

static void on_contact_long_press(lv_event_t *e)
{
    if (popup_overlay) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        show_popup(idx);
    }
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_contacts_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* Status Bar (16px) */
    lv_obj_t *go_btn = ui_create_status_bar(screen);
    lv_obj_add_event_cb(go_btn, on_go_back, LV_EVENT_CLICKED, NULL);

    /* Header (26px) */
    create_contacts_header(screen);

    /* List area (160px) */
    list_container = lv_obj_create(screen);
    lv_obj_set_width(list_container, LV_PCT(100));
    lv_obj_set_height(list_container, LIST_H);
    lv_obj_set_pos(list_container, 0, LIST_Y);
    style_black(list_container);
    lv_obj_set_style_pad_left(list_container, 6, 0);
    lv_obj_set_style_pad_right(list_container, 6, 0);
    lv_obj_set_style_pad_top(list_container, 5, 0);
    lv_obj_set_style_pad_bottom(list_container, 4, 0);
    lv_obj_set_style_pad_row(list_container, CARD_GAP, 0);
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

    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        create_contact_card(list_container, i, &contacts_db.contacts[i]);
    }
}
