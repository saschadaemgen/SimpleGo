/**
 * @file ui_chat.c
 * @brief Chat Screen - Cyberpunk Messenger (Mockup v5)
 *
 * Layout (320×240, edge-to-edge):
 *   ┌──────────────────────────────────┐
 *   │ Status Bar  16px                 │
 *   │ Glow Line    1px                 │
 *   │ Chat Header 26px                 │
 *   │ Dim Line     1px                 │
 *   │ Messages   160px (scrollable)    │
 *   │ Input Bar   36px                 │
 *   └──────────────────────────────────┘
 *   Total: 16+1+26+1+160+36 = 240 ✓
 *
 * Session 39f: Bubble rendering extracted to ui_chat_bubble.c
 *              Style helpers centralized in ui_theme.c
 *
 * SimpleGo UI - v2 Redesign
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat.h"
#include "ui_chat_bubble.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "tdeck_keyboard.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_CHAT";

/* ============== Layout Constants ============== */

#define HDR_Y               (UI_STATUS_H + 1)               /* 17 */
#define HDR_H               26
#define DIM_Y               (HDR_Y + HDR_H)                  /* 43 */
#define INPUT_H             36
#define INPUT_Y             (UI_SCREEN_H - INPUT_H)          /* 204 */
#define MSG_Y               (DIM_Y + 1)                      /* 44 */
#define MSG_H               (INPUT_Y - MSG_Y)                /* 160 */
#define MSG_PAD_SIDE        6

/* Input bar */
#define INPUT_FIELD_H       24
#define ACT_BTN_SIZE        24

/* ============== State ============== */

static lv_obj_t *screen        = NULL;
static lv_obj_t *header_label  = NULL;
static lv_obj_t *msg_container = NULL;
static lv_obj_t *input_area    = NULL;
static lv_group_t *input_group = NULL;
static lv_obj_t *s_loading_box = NULL;   /* Session 37b: "Loading..." indicator */
static lv_obj_t *s_settings_icon = NULL; /* Session 38j: Settings button in header */

static ui_chat_send_cb_t send_cb    = NULL;
static lv_indev_t *pending_kb_indev = NULL;

/* 35e: Active contact for message filtering */
static int s_chat_active_contact = 0;

/* ============== Forward Declarations ============== */

static void on_back(lv_event_t *e);
static void on_input_ready(lv_event_t *e);
static void on_send_click(lv_event_t *e);
static void do_send_message(void);

/* ============== Action Button ============== */

static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *symbol,
                                    lv_color_t icon_color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, ACT_BTN_SIZE, ACT_BTN_SIZE);
    lv_obj_set_style_radius(btn, ACT_BTN_SIZE / 2, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_10, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, icon_color, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_SM, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ============== Chat Header ============== */

/* Session 38j: Settings button handlers */
static void on_settings_short(lv_event_t *e)
{
    (void)e;
    tdeck_kbd_backlight_toggle();
    if (s_settings_icon) {
        lv_obj_set_style_text_color(s_settings_icon,
            tdeck_kbd_backlight_is_on() ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    }
}

static void on_settings_long(lv_event_t *e)
{
    (void)e;
    ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE);
}

static lv_obj_t *create_chat_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_width(hdr, LV_PCT(100));
    lv_obj_set_height(hdr, HDR_H);
    lv_obj_set_pos(hdr, 0, HDR_Y);
    ui_style_black(hdr);

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

    /* Contact name */
    lv_obj_t *name = lv_label_create(hdr);
    lv_label_set_text(name, "Alice");
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 148);  /* Session 38j: narrower for settings button */
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, UI_FONT, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 28, 0);

    /* Session 38j: Settings gear button (short=toggle kbd, long=settings) */
    lv_obj_t *set_btn = lv_btn_create(hdr);
    lv_obj_set_size(set_btn, 28, HDR_H);
    lv_obj_set_style_bg_opa(set_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(set_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(set_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(set_btn, 0, 0);
    lv_obj_set_style_radius(set_btn, 0, 0);
    lv_obj_set_style_shadow_width(set_btn, 0, 0);
    lv_obj_set_style_pad_all(set_btn, 0, 0);
    lv_obj_align(set_btn, LV_ALIGN_RIGHT_MID, -104, 0);

    s_settings_icon = lv_label_create(set_btn);
    lv_label_set_text(s_settings_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(s_settings_icon,
        tdeck_kbd_backlight_is_on() ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_settings_icon, UI_FONT, 0);
    lv_obj_center(s_settings_icon);
    lv_obj_add_event_cb(set_btn, on_settings_short, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(set_btn, on_settings_long, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *enc = lv_label_create(hdr);
    lv_label_set_text(enc, "Post-Quantum E2E");
    lv_obj_set_style_text_color(enc, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_text_font(enc, UI_FONT_SM, 0);
    lv_obj_align(enc, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t *dim = lv_obj_create(parent);
    lv_obj_set_width(dim, LV_PCT(100));
    lv_obj_set_height(dim, 1);
    lv_obj_set_pos(dim, 0, DIM_Y);
    lv_obj_set_style_bg_color(dim, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(dim, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dim, 0, 0);
    lv_obj_set_style_radius(dim, 0, 0);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_CLICKABLE);

    return name;
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_chat_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* Status Bar (16px) */
    lv_obj_t *go_btn = ui_create_status_bar(screen);
    lv_obj_add_event_cb(go_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* Chat Header (26px) */
    header_label = create_chat_header(screen);

    /* Messages (160px) */
    msg_container = lv_obj_create(screen);
    lv_obj_set_width(msg_container, LV_PCT(100));
    lv_obj_set_height(msg_container, MSG_H);
    lv_obj_set_pos(msg_container, 0, MSG_Y);
    ui_style_black(msg_container);
    lv_obj_set_style_pad_left(msg_container, MSG_PAD_SIDE, 0);
    lv_obj_set_style_pad_right(msg_container, MSG_PAD_SIDE, 0);
    lv_obj_set_style_pad_top(msg_container, 8, 0);
    lv_obj_set_style_pad_bottom(msg_container, 5, 0);
    lv_obj_set_style_pad_row(msg_container, CHAT_BUBBLE_GAP, 0);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(msg_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(msg_container, LV_SCROLLBAR_MODE_OFF);

    /* Input Bar (36px) */
    lv_obj_t *input_bar = lv_obj_create(screen);
    lv_obj_set_width(input_bar, LV_PCT(100));
    lv_obj_set_height(input_bar, INPUT_H);
    lv_obj_set_pos(input_bar, 0, INPUT_Y);
    ui_style_black(input_bar);

    lv_obj_set_style_border_width(input_bar, 1, 0);
    lv_obj_set_style_border_color(input_bar, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(input_bar, (lv_opa_t)38, 0);
    lv_obj_set_style_border_side(input_bar, LV_BORDER_SIDE_TOP, 0);

    lv_obj_set_style_pad_left(input_bar, 3, 0);
    lv_obj_set_style_pad_right(input_bar, 3, 0);
    lv_obj_set_style_pad_top(input_bar, 6, 0);
    lv_obj_set_style_pad_bottom(input_bar, 6, 0);
    lv_obj_set_style_pad_column(input_bar, 4, 0);
    lv_obj_set_flex_flow(input_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_bar, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Textarea */
    input_area = lv_textarea_create(input_bar);
    lv_obj_set_flex_grow(input_area, 1);
    lv_obj_set_height(input_area, INPUT_FIELD_H);
    lv_textarea_set_one_line(input_area, true);
    lv_textarea_set_placeholder_text(input_area, "> message...");

    lv_obj_set_style_bg_opa(input_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(input_area, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(input_area, UI_FONT, 0);
    lv_obj_set_style_border_color(input_area, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(input_area, 1, 0);
    lv_obj_set_style_radius(input_area, INPUT_FIELD_H / 2, 0);
    lv_obj_set_style_pad_left(input_area, 8, 0);
    lv_obj_set_style_pad_right(input_area, 8, 0);
    lv_obj_set_style_pad_top(input_area, 3, 0);
    lv_obj_set_style_pad_bottom(input_area, 3, 0);

    lv_obj_set_style_border_color(input_area, UI_COLOR_LINE_DIM,
                                  LV_STATE_FOCUSED);

    /* Cursor: dim cyan, thin */
    lv_obj_set_style_bg_color(input_area, UI_COLOR_TEXT_DIM, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(input_area, LV_OPA_COVER, LV_PART_CURSOR);

    lv_obj_set_style_text_color(input_area, lv_color_hex(0x304050),
                                LV_PART_TEXTAREA_PLACEHOLDER);

    lv_obj_add_event_cb(input_area, on_input_ready, LV_EVENT_READY, NULL);

    /* Buttons: [SEND] [VOICE] [FILE] */
    lv_obj_t *send_btn = create_action_btn(input_bar, LV_SYMBOL_RIGHT,
                                            UI_COLOR_SECONDARY);
    lv_obj_add_event_cb(send_btn, on_send_click, LV_EVENT_CLICKED, NULL);
    create_action_btn(input_bar, LV_SYMBOL_VOLUME_MID, UI_COLOR_TEXT_DIM);
    create_action_btn(input_bar, LV_SYMBOL_FILE,       UI_COLOR_TEXT_DIM);

    /* Keyboard group */
    input_group = lv_group_create();
    lv_group_add_obj(input_group, input_area);

    if (pending_kb_indev) {
        lv_indev_set_group(pending_kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked (deferred)");
    }

    ESP_LOGI(TAG, "Chat screen created (v5 layout)");
    return screen;
}

/* ============== Event Handlers ============== */

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_manager_go_back();
}

static void do_send_message(void)
{
    const char *text = lv_textarea_get_text(input_area);
    if (!text || text[0] == '\0') return;

    ESP_LOGI(TAG, "Send: \"%s\"", text);

    uint32_t seq = chat_bubble_next_seq();
    (void)seq;

    ui_chat_add_message(text, true, s_chat_active_contact);

    if (send_cb) {
        send_cb(text);
    }

    lv_textarea_set_text(input_area, "");
}

static void on_input_ready(lv_event_t *e)
{
    (void)e;
    do_send_message();
}

static void on_send_click(lv_event_t *e)
{
    (void)e;
    do_send_message();
}

/* ============== Name Truncation ============== */

#define CHAT_NAME_MAX  20

static void truncate_chat_name(char *dst, const char *src, size_t dst_size)
{
    if (!src || !dst || dst_size < 4) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len <= CHAT_NAME_MAX) {
        strncpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
    } else {
        size_t copy = (CHAT_NAME_MAX < dst_size - 4) ? CHAT_NAME_MAX : dst_size - 4;
        memcpy(dst, src, copy);
        dst[copy] = '.'; dst[copy+1] = '.'; dst[copy+2] = '.'; dst[copy+3] = '\0';
    }
}

/* ============== Public API — Thin Wrappers ============== */

void ui_chat_set_contact(const char *name)
{
    if (header_label && name) {
        char trunc[CHAT_NAME_MAX + 4];
        truncate_chat_name(trunc, name, sizeof(trunc));
        lv_label_set_text(header_label, trunc);
    }
}

void ui_chat_add_message(const char *text, bool is_outgoing, int contact_idx)
{
    chat_bubble_add_live(msg_container, text, is_outgoing,
                         contact_idx, s_chat_active_contact);
}

void ui_chat_add_history_message(const char *text, bool is_outgoing, int contact_idx,
                                  int64_t timestamp, uint8_t delivery_status)
{
    chat_bubble_add_history(msg_container, text, is_outgoing,
                            contact_idx, s_chat_active_contact,
                            timestamp, delivery_status);
}

void ui_chat_set_send_callback(ui_chat_send_cb_t cb)
{
    send_cb = cb;
}

void ui_chat_set_keyboard_indev(lv_indev_t *kb_indev)
{
    pending_kb_indev = kb_indev;

    if (kb_indev && input_group) {
        lv_indev_set_group(kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked");
    }
}

lv_indev_t *ui_chat_get_keyboard_indev(void)
{
    return pending_kb_indev;
}

void ui_chat_update_status(uint32_t msg_seq, int status)
{
    chat_bubble_update_status(msg_seq, status);
}

uint32_t ui_chat_next_seq(void)
{
    return chat_bubble_next_seq();
}

uint32_t ui_chat_get_last_seq(void)
{
    return chat_bubble_get_last_seq();
}

/* ============== Contact Management ============== */

void ui_chat_switch_contact(int contact_idx, const char *name)
{
    s_chat_active_contact = contact_idx;

    /* Update header */
    if (header_label && name) {
        char trunc[CHAT_NAME_MAX + 4];
        truncate_chat_name(trunc, name, sizeof(trunc));
        lv_label_set_text(header_label, trunc);
    }

    /* Filter bubbles: show matching, hide others */
    if (msg_container) {
        uint32_t child_count = lv_obj_get_child_count(msg_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(msg_container, i);
            int tag = (int)(intptr_t)lv_obj_get_user_data(child);
            if (tag == 0) continue;  /* Untagged, skip */
            int child_contact = tag - 1;
            if (child_contact == contact_idx) {
                lv_obj_clear_flag(child, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        }
        /* Scroll to bottom of visible messages */
        lv_obj_update_layout(msg_container);
        lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_OFF);
    }

    ESP_LOGI(TAG, "Switched to contact [%d] '%s'", contact_idx, name ? name : "?");
}

void ui_chat_clear_contact(int contact_idx)
{
    if (!msg_container) return;

    /* -1 = clear ALL bubbles */
    if (contact_idx < 0) {
        lv_obj_clean(msg_container);
        s_loading_box = NULL;  /* was a child of msg_container, now deleted */
        ESP_LOGI(TAG, "36d: Cleared ALL bubbles");
        return;
    }

    uint32_t i = 0;
    while (i < lv_obj_get_child_count(msg_container)) {
        lv_obj_t *child = lv_obj_get_child(msg_container, i);
        int tag = (int)(intptr_t)lv_obj_get_user_data(child);
        if (tag != 0 && (tag - 1) == contact_idx) {
            lv_obj_delete(child);
        } else {
            i++;
        }
    }
    ESP_LOGI(TAG, "36d: Cleared bubbles for contact [%d]", contact_idx);
}

/* ============== Loading Indicator ============== */

void ui_chat_show_loading(void)
{
    if (!msg_container) return;
    ui_chat_hide_loading();  /* remove stale one if any */

    /* Transparent wrapper fills the empty msg_container via flex_grow */
    s_loading_box = lv_obj_create(msg_container);
    lv_obj_remove_style_all(s_loading_box);
    lv_obj_set_width(s_loading_box, LV_PCT(100));
    lv_obj_set_flex_grow(s_loading_box, 1);
    lv_obj_set_style_bg_opa(s_loading_box, LV_OPA_TRANSP, 0);

    /* Centered label inside wrapper */
    lv_obj_t *lbl = lv_label_create(s_loading_box);
    lv_label_set_text(lbl, "Loading...");
    lv_obj_set_style_text_color(lbl, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_center(lbl);
}

void ui_chat_hide_loading(void)
{
    if (s_loading_box) {
        lv_obj_delete(s_loading_box);  /* deletes child label too */
        s_loading_box = NULL;
    }
}

void ui_chat_scroll_to_bottom(void)
{
    if (msg_container) {
        lv_obj_update_layout(msg_container);
        lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

/* ============== Session 38j: Settings Icon Update ============== */

void ui_chat_update_settings_icon(void)
{
    if (s_settings_icon) {
        lv_obj_set_style_text_color(s_settings_icon,
            tdeck_kbd_backlight_is_on() ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    }
}
