/**
 * @file ui_contacts_popup.c
 * @brief Contact Popup - Delete, Info, Actions
 *
 * Session 39f: Extracted from ui_contacts.c lines 385-655
 * All popup rendering and action logic in one module.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_contacts_popup.h"
#include "ui_theme.h"
#include "ui_chat.h"
#include "ui_connect.h"
#include "smp_types.h"
#include "smp_contacts.h"
#include "smp_storage.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

extern void smp_clear_42d(int idx);

static const char *TAG = "UI_POPUP";

/* ============== State ============== */

static lv_obj_t *popup_overlay = NULL;
static int popup_contact_idx = -1;
static ui_contacts_popup_refresh_cb_t s_refresh_cb = NULL;

/* ============== Forward Declarations ============== */

static void on_popup_delete(lv_event_t *e);
static void on_popup_info(lv_event_t *e);
static void on_popup_cancel(lv_event_t *e);

/* ============== Public API ============== */

bool ui_contacts_popup_active(void)
{
    return (popup_overlay != NULL);
}

void ui_contacts_popup_close(void)
{
    if (popup_overlay) {
        lv_obj_delete(popup_overlay);
        popup_overlay = NULL;
    }
    popup_contact_idx = -1;
}

void ui_contacts_popup_set_refresh_cb(ui_contacts_popup_refresh_cb_t cb)
{
    s_refresh_cb = cb;
}

/* ============== Popup Action Row Helper ============== */

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
    lv_obj_set_style_radius(row, 8, 0);
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

/* ============== Show Delete/Info Popup ============== */

void ui_contacts_popup_show(lv_obj_t *screen, int contact_idx)
{
    if (popup_overlay) ui_contacts_popup_close();
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
        c->have_srv_dh ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
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

    /* Cancel - plain text at bottom */
    lv_obj_t *cancel = lv_label_create(popup_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_popup_cancel, LV_EVENT_CLICKED, NULL);
}

/* ============== Info Popup ============== */

void ui_contacts_popup_show_info(lv_obj_t *screen, int idx)
{
    contact_t *c = &contacts_db.contacts[idx];
    ui_contacts_popup_close();
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
    lv_obj_set_style_text_color(s1, c->have_srv_dh ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
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

/* ============== Event Handlers ============== */

static void on_popup_delete(lv_event_t *e)
{
    (void)e;
    if (popup_contact_idx < 0 || popup_contact_idx >= MAX_CONTACTS) {
        ui_contacts_popup_close();
        return;
    }

    int idx = popup_contact_idx;
    contact_t *c = &contacts_db.contacts[idx];
    ESP_LOGW(TAG, "DELETE [%d] '%s'", idx, c->name);

    memset(c, 0, sizeof(contact_t));

    // Clean up ALL orphaned NVS keys for this contact
    {
        const char *prefixes[] = {"rat_", "peer_", "hand_", "rq_"};
        char nkey[16];
        for (int k = 0; k < 4; k++) {
            snprintf(nkey, sizeof(nkey), "%s%02x", prefixes[k], idx);
            smp_storage_delete(nkey);
        }
        ESP_LOGI(TAG, "NVS keys cleaned: rat/peer/hand/rq_%02x", idx);
    }

    smp_clear_42d(idx);
    ui_chat_clear_contact(idx);
    ui_connect_reset();

    int n = 0;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) n++;
    }
    contacts_db.num_contacts = n;
    save_contacts_to_nvs();

    ui_contacts_popup_close();
    if (s_refresh_cb) s_refresh_cb();
}

static void on_popup_info(lv_event_t *e)
{
    (void)e;
    int idx = popup_contact_idx;
    if (idx < 0 || idx >= MAX_CONTACTS) { ui_contacts_popup_close(); return; }
    /* Get screen from popup_overlay's parent */
    lv_obj_t *scr = lv_obj_get_parent(popup_overlay);
    ui_contacts_popup_show_info(scr, idx);
}

static void on_popup_cancel(lv_event_t *e)
{
    (void)e;
    ui_contacts_popup_close();
}
