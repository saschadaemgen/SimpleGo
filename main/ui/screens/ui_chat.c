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
 * Bubbles: asymmetric corners via custom draw callback
 *   After LVGL draws the rounded bubble, our callback paints a
 *   black rectangle OVER the one rounded corner, then redraws
 *   straight border lines on the two outer edges.
 *   No overlay objects — pure draw pipeline manipulation.
 *
 * SimpleGo UI - v2 Redesign
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "UI_CHAT";

/* ============== Layout Constants ============== */

#define HDR_Y               (UI_STATUS_H + 1)               /* 17 */
#define HDR_H               26
#define DIM_Y               (HDR_Y + HDR_H)                  /* 43 */
#define INPUT_H             36
#define INPUT_Y             (UI_SCREEN_H - INPUT_H)          /* 204 */
#define MSG_Y               (DIM_Y + 1)                      /* 44 */
#define MSG_H               (INPUT_Y - MSG_Y)                /* 160 */

/* Bubble styling */
#define BUBBLE_RADIUS       12
#define BUBBLE_PAD_T        5
#define BUBBLE_PAD_B        2
#define BUBBLE_PAD_L        8
#define BUBBLE_PAD_R        8
#define BUBBLE_BORDER_W     1
#define BUBBLE_GAP          5
#define MSG_PAD_SIDE        6

/* Bubble colors — extremely subtle tint, barely visible */
#define BUBBLE_BG_OUT       lv_color_hex(0x000C00)  /* pure dark green, no red */
#define BUBBLE_BG_IN        lv_color_hex(0x000010)  /* pure dark blue, no red  */

/* Border opacities — more neon glow */
#define BORDER_OPA_OUT      ((lv_opa_t)120)  /* ~47% green — more neon */
#define BORDER_OPA_IN       ((lv_opa_t)105)  /* ~41% cyan  — more neon */

/* Input bar */
#define INPUT_FIELD_H       24
#define ACT_BTN_SIZE        24

/* Delivery status text — LVGL symbols rendered at Font 12
 * (Font 10 clips the symbol glyphs at the top) */
#define ST_SENDING          "..."
#define ST_SENT             LV_SYMBOL_OK
#define ST_DELIVERED        LV_SYMBOL_OK " " LV_SYMBOL_OK
#define ST_FAILED           LV_SYMBOL_CLOSE

/* ============== State ============== */

static lv_obj_t *screen        = NULL;
static lv_obj_t *header_label  = NULL;
static lv_obj_t *msg_container = NULL;
static lv_obj_t *input_area    = NULL;
static lv_group_t *input_group = NULL;
static lv_obj_t *s_loading_box = NULL;   // Session 37b: "Loading..." indicator

static ui_chat_send_cb_t send_cb    = NULL;
static lv_indev_t *pending_kb_indev = NULL;

#define MAX_TRACKED_MSGS 16

typedef struct {
    lv_obj_t *status_label;
    uint32_t  msg_seq;
    bool      active;
} tracked_msg_t;

static tracked_msg_t tracked_msgs[MAX_TRACKED_MSGS] = {0};
static uint32_t msg_seq_counter = 0;

// 35e: Active contact for message filtering
static int s_chat_active_contact = 0;

/* ============== Forward Declarations ============== */

static void on_back(lv_event_t *e);
static void on_input_ready(lv_event_t *e);
static void on_send_click(lv_event_t *e);
static void do_send_message(void);

/* ============== Style Helpers ============== */

static void style_reset(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_black(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* ============== Timestamp ============== */

static void get_timestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    if (ti.tm_year > 70) {
        char day[8], hm[8];
        strftime(day, sizeof(day), "%a", &ti);
        strftime(hm, sizeof(hm), "%H:%M", &ti);
        snprintf(buf, len, "%s | %s", day, hm);
    } else {
        snprintf(buf, len, "--:--");
    }
}

/* ============================================================
 * CUSTOM DRAW CALLBACK — sharp corner via draw pipeline
 *
 * Strategy:
 *   1. LVGL draws the bubble as a fully-rounded rectangle (all 4 corners)
 *   2. Our callback fires for each draw task on this bubble
 *   3. We detect the main rectangle draw task (LV_PART_MAIN)
 *   4. We add extra draw tasks AFTER it:
 *      a) Black filled rect covering the one rounded corner
 *      b) 1px border line on the bottom edge of that corner
 *      c) 1px border line on the side edge of that corner
 *
 * Result: 3 rounded corners + 1 sharp corner with continuous border.
 * Zero overlay objects, zero clipping issues.
 * ============================================================ */

static void bubble_draw_cb(lv_event_t *e)
{
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);

    if (base->part != LV_PART_MAIN) return;

    lv_obj_t *bubble = lv_event_get_target_obj(e);
    bool is_out = (bool)(uintptr_t)lv_event_get_user_data(e);

    lv_area_t a;
    lv_obj_get_coords(bubble, &a);

    lv_color_t bdr_col = is_out ? UI_COLOR_SECONDARY : UI_COLOR_PRIMARY;
    lv_opa_t   bdr_opa = is_out ? BORDER_OPA_OUT : BORDER_OPA_IN;

    /* Read the bubble's actual bg color so the patch matches exactly */
    lv_color_t bg_color = lv_obj_get_style_bg_color(bubble, LV_PART_MAIN);

    int R = BUBBLE_RADIUS;
    /* How far the padding+border zone extends inward */
    int pad_b = BUBBLE_PAD_B + BUBBLE_BORDER_W;   /* 3px from bottom edge */
    int pad_side = BUBBLE_BORDER_W + 2;            /* 3px — covers border curve */

    /* --- L-shaped black fill: two strips that ONLY cover the
     *     border/padding zone, never the content area ---
     *
     *  For outgoing (bottom-right):
     *     +---------+----+
     *     | content | S  |  <- Side strip (padding zone on right)
     *     |  area   | I  |
     *     |         | D  |
     *     +---------+ E  |
     *     | BOTTOM STRIP  |  <- Bottom strip (padding zone on bottom)
     *     +---------------+
     *
     *  The curved border passes through these zones,
     *  so the L-shape fully erases the rounded corner arc.
     */

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color  = bg_color;
    fill.bg_opa    = LV_OPA_COVER;
    fill.radius    = 0;

    lv_area_t strip;

    if (is_out) {
        /* Bottom strip: full corner width, only pad_b tall */
        strip.x1 = a.x2 - R;
        strip.y1 = a.y2 - pad_b;
        strip.x2 = a.x2;
        strip.y2 = a.y2;
        lv_draw_rect(base->layer, &fill, &strip);

        /* Side strip: only pad_side wide, from top of radius to bottom strip */
        strip.x1 = a.x2 - pad_side;
        strip.y1 = a.y2 - R;
        strip.x2 = a.x2;
        strip.y2 = a.y2 - pad_b;
        lv_draw_rect(base->layer, &fill, &strip);
    } else {
        /* Bottom strip */
        strip.x1 = a.x1;
        strip.y1 = a.y2 - pad_b;
        strip.x2 = a.x1 + R;
        strip.y2 = a.y2;
        lv_draw_rect(base->layer, &fill, &strip);

        /* Side strip */
        strip.x1 = a.x1;
        strip.y1 = a.y2 - R;
        strip.x2 = a.x1 + pad_side;
        strip.y2 = a.y2 - pad_b;
        lv_draw_rect(base->layer, &fill, &strip);
    }

    /* --- Straight border lines on the two outer edges --- */
    lv_draw_rect_dsc_t bdr;
    lv_draw_rect_dsc_init(&bdr);
    bdr.bg_color = bdr_col;
    bdr.bg_opa   = bdr_opa;
    bdr.radius   = 0;

    lv_area_t edge;
    if (is_out) {
        /* Bottom edge */
        edge.x1 = a.x2 - R;
        edge.y1 = a.y2;
        edge.x2 = a.x2;
        edge.y2 = a.y2;
        lv_draw_rect(base->layer, &bdr, &edge);

        /* Right edge */
        edge.x1 = a.x2;
        edge.y1 = a.y2 - R;
        edge.x2 = a.x2;
        edge.y2 = a.y2;
        lv_draw_rect(base->layer, &bdr, &edge);
    } else {
        /* Bottom edge */
        edge.x1 = a.x1;
        edge.y1 = a.y2;
        edge.x2 = a.x1 + R;
        edge.y2 = a.y2;
        lv_draw_rect(base->layer, &bdr, &edge);

        /* Left edge */
        edge.x1 = a.x1;
        edge.y1 = a.y2 - R;
        edge.x2 = a.x1;
        edge.y2 = a.y2;
        lv_draw_rect(base->layer, &bdr, &edge);
    }
}

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

static lv_obj_t *create_chat_header(lv_obj_t *parent)
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

    /* Contact name — placeholder until data binding */
    lv_obj_t *name = lv_label_create(hdr);
    lv_label_set_text(name, "Alice");
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 180);
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, UI_FONT, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 28, 0);

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
    style_black(msg_container);
    lv_obj_set_style_pad_left(msg_container, MSG_PAD_SIDE, 0);
    lv_obj_set_style_pad_right(msg_container, MSG_PAD_SIDE, 0);
    lv_obj_set_style_pad_top(msg_container, 8, 0);
    lv_obj_set_style_pad_bottom(msg_container, 5, 0);
    lv_obj_set_style_pad_row(msg_container, BUBBLE_GAP, 0);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(msg_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(msg_container, LV_SCROLLBAR_MODE_OFF);

    /* Input Bar (36px) */
    lv_obj_t *input_bar = lv_obj_create(screen);
    lv_obj_set_width(input_bar, LV_PCT(100));
    lv_obj_set_height(input_bar, INPUT_H);
    lv_obj_set_pos(input_bar, 0, INPUT_Y);
    style_black(input_bar);

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

    uint32_t seq = ui_chat_next_seq();
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

/* ============== Public API ============== */

void ui_chat_set_contact(const char *name)
{
    if (header_label && name) {
        lv_label_set_text(header_label, name);
    }
}

void ui_chat_add_message(const char *text, bool is_outgoing, int contact_idx)
{
    if (!msg_container || !text) return;

    char ts[20];
    get_timestamp(ts, sizeof(ts));

    lv_coord_t cont_inner = lv_obj_get_content_width(msg_container);
    lv_coord_t bubble_max = cont_inner * 72 / 100;
    lv_coord_t margin_push = cont_inner - bubble_max;

    lv_color_t bg      = is_outgoing ? BUBBLE_BG_OUT : BUBBLE_BG_IN;
    lv_color_t bdr_col  = is_outgoing ? UI_COLOR_SECONDARY : UI_COLOR_PRIMARY;
    lv_opa_t   bdr_opa  = is_outgoing ? BORDER_OPA_OUT : BORDER_OPA_IN;

    /* ===== Bubble ===== */
    lv_obj_t *bubble = lv_obj_create(msg_container);
    // 35e: Tag bubble with contact index for filtering
    lv_obj_set_user_data(bubble, (void *)(intptr_t)(contact_idx + 1));  // +1 so 0 means "untagged"
    // Hide if not active contact
    if (contact_idx != s_chat_active_contact) {
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_width(bubble, bubble_max);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(bubble, BUBBLE_PAD_T, 0);
    lv_obj_set_style_pad_bottom(bubble, BUBBLE_PAD_B, 0);
    lv_obj_set_style_pad_left(bubble, BUBBLE_PAD_L, 0);
    lv_obj_set_style_pad_right(bubble, BUBBLE_PAD_R, 0);
    lv_obj_set_style_pad_row(bubble, 2, 0);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    /* Rounded corners + neon border (LVGL draws all 4 corners rounded) */
    lv_obj_set_style_radius(bubble, BUBBLE_RADIUS, 0);
    lv_obj_set_style_bg_color(bubble, bg, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bubble, BUBBLE_BORDER_W, 0);
    lv_obj_set_style_border_color(bubble, bdr_col, 0);
    lv_obj_set_style_border_opa(bubble, bdr_opa, 0);

    if (is_outgoing) {
        lv_obj_set_style_margin_left(bubble, margin_push, 0);
    } else {
        lv_obj_set_style_margin_left(bubble, 0, 0);
    }

    /* ===== Custom draw callback to flatten one corner ===== */
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(bubble, bubble_draw_cb,
                        LV_EVENT_DRAW_TASK_ADDED,
                        (void *)(uintptr_t)is_outgoing);

    /* ---- Message text (Font 12 — between SM and LG) ---- */
    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);

    /* ---- Meta row ---- */
    lv_obj_t *meta = lv_obj_create(bubble);
    lv_obj_set_width(meta, LV_PCT(100));
    lv_obj_set_height(meta, LV_SIZE_CONTENT);
    style_reset(meta);
    lv_obj_add_flag(meta, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_min_height(meta, 14, 0);

    if (is_outgoing) {
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        /* Checkmarks: floating in top-right of bubble (away from corner patch) */
        lv_obj_t *st = lv_label_create(bubble);
        lv_label_set_text(st, ST_SENDING);
        lv_obj_set_style_text_color(st, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(st, UI_FONT_MD, 0);
        lv_obj_add_flag(st, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(st, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(st, LV_ALIGN_TOP_RIGHT, 0, 1);

        uint32_t seq = msg_seq_counter;
        for (int i = 0; i < MAX_TRACKED_MSGS; i++) {
            if (!tracked_msgs[i].active) {
                tracked_msgs[i].status_label = st;
                tracked_msgs[i].msg_seq = seq;
                tracked_msgs[i].active = true;
                break;
            }
        }
    } else {
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    /* Scroll + refresh */
    lv_obj_update_layout(msg_container);
    lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_ON);
    if (screen) {
        lv_obj_invalidate(screen);
    }
    lv_refr_now(NULL);

    ESP_LOGD(TAG, "%s: \"%s\"", is_outgoing ? "OUT" : "IN", text);
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

/* ============== Delivery Status API ============== */

void ui_chat_update_status(uint32_t msg_seq, int status)
{
    for (int i = 0; i < MAX_TRACKED_MSGS; i++) {
        if (tracked_msgs[i].active && tracked_msgs[i].msg_seq == msg_seq) {
            if (status >= 0 && status <= 3) {
                static const char *st_text[] = {
                    ST_SENDING, ST_SENT, ST_DELIVERED, ST_FAILED
                };
                lv_label_set_text(tracked_msgs[i].status_label,
                                  st_text[status]);

                lv_color_t color;
                switch (status) {
                    case 1:  color = UI_COLOR_TEXT_DIM;   break;
                    case 2:  color = UI_COLOR_SECONDARY;  break;
                    case 3:  color = UI_COLOR_ERROR;      break;
                    default: color = UI_COLOR_TEXT_DIM;    break;
                }
                lv_obj_set_style_text_color(
                    tracked_msgs[i].status_label, color, 0);
            }

            if (status == 2 || status == 3) {
                tracked_msgs[i].active = false;
            }
            return;
        }
    }
}

uint32_t ui_chat_next_seq(void)
{
    return ++msg_seq_counter;
}

uint32_t ui_chat_get_last_seq(void)
{
    return msg_seq_counter;
}

void ui_chat_switch_contact(int contact_idx, const char *name)
{
    s_chat_active_contact = contact_idx;

    // Update header
    if (header_label && name) {
        lv_label_set_text(header_label, name);
    }

    // Filter bubbles: show matching, hide others
    if (msg_container) {
        uint32_t child_count = lv_obj_get_child_count(msg_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(msg_container, i);
            int tag = (int)(intptr_t)lv_obj_get_user_data(child);
            if (tag == 0) continue;  // Untagged, skip
            int child_contact = tag - 1;
            if (child_contact == contact_idx) {
                lv_obj_clear_flag(child, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // Scroll to bottom of visible messages
        lv_obj_update_layout(msg_container);
        lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_OFF);
    }

    ESP_LOGI("UI_CHAT", "Switched to contact [%d] '%s'", contact_idx, name ? name : "?");
}

void ui_chat_clear_contact(int contact_idx)
{
    if (!msg_container) return;

    /* -1 = clear ALL bubbles (used on contact switch) */
    if (contact_idx < 0) {
        lv_obj_clean(msg_container);
        s_loading_box = NULL;  /* was a child of msg_container, now deleted */
        ESP_LOGI("UI_CHAT", "36d: Cleared ALL bubbles");
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
    ESP_LOGI("UI_CHAT", "36d: Cleared bubbles for contact [%d]", contact_idx);
}

/* ============== Session 37: History Message Loading ============== */

/**
 * Format a stored timestamp (Unix seconds) into "Day | HH:MM"
 */
static void format_history_timestamp(int64_t ts, char *buf, size_t len)
{
    if (ts <= 0) {
        snprintf(buf, len, "--:--");
        return;
    }
    time_t t = (time_t)ts;
    struct tm ti;
    localtime_r(&t, &ti);
    if (ti.tm_year > 70) {
        char day[8], hm[8];
        strftime(day, sizeof(day), "%a", &ti);
        strftime(hm, sizeof(hm), "%H:%M", &ti);
        snprintf(buf, len, "%s | %s", day, hm);
    } else {
        snprintf(buf, len, "--:--");
    }
}

void ui_chat_add_history_message(const char *text, bool is_outgoing, int contact_idx,
                                  int64_t timestamp, uint8_t delivery_status)
{
    if (!msg_container || !text) return;

    char ts[20];
    format_history_timestamp(timestamp, ts, sizeof(ts));

    lv_coord_t cont_inner = lv_obj_get_content_width(msg_container);
    lv_coord_t bubble_max = cont_inner * 72 / 100;
    lv_coord_t margin_push = cont_inner - bubble_max;

    lv_color_t bg      = is_outgoing ? BUBBLE_BG_OUT : BUBBLE_BG_IN;
    lv_color_t bdr_col  = is_outgoing ? UI_COLOR_SECONDARY : UI_COLOR_PRIMARY;
    lv_opa_t   bdr_opa  = is_outgoing ? BORDER_OPA_OUT : BORDER_OPA_IN;

    /* Bubble */
    lv_obj_t *bubble = lv_obj_create(msg_container);
    lv_obj_set_user_data(bubble, (void *)(intptr_t)(contact_idx + 1));
    if (contact_idx != s_chat_active_contact) {
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_width(bubble, bubble_max);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(bubble, BUBBLE_PAD_T, 0);
    lv_obj_set_style_pad_bottom(bubble, BUBBLE_PAD_B, 0);
    lv_obj_set_style_pad_left(bubble, BUBBLE_PAD_L, 0);
    lv_obj_set_style_pad_right(bubble, BUBBLE_PAD_R, 0);
    lv_obj_set_style_pad_row(bubble, 2, 0);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(bubble, BUBBLE_RADIUS, 0);
    lv_obj_set_style_bg_color(bubble, bg, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bubble, BUBBLE_BORDER_W, 0);
    lv_obj_set_style_border_color(bubble, bdr_col, 0);
    lv_obj_set_style_border_opa(bubble, bdr_opa, 0);

    if (is_outgoing) {
        lv_obj_set_style_margin_left(bubble, margin_push, 0);
    }

    /* Custom draw callback for sharp corner */
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(bubble, bubble_draw_cb,
                        LV_EVENT_DRAW_TASK_ADDED,
                        (void *)(uintptr_t)is_outgoing);

    /* Message text */
    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);

    /* Meta row */
    lv_obj_t *meta = lv_obj_create(bubble);
    lv_obj_set_width(meta, LV_PCT(100));
    lv_obj_set_height(meta, LV_SIZE_CONTENT);
    style_reset(meta);
    lv_obj_add_flag(meta, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_min_height(meta, 14, 0);

    if (is_outgoing) {
        /* Timestamp left */
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        /* Delivery checkmark — already resolved from history */
        lv_obj_t *st = lv_label_create(bubble);
        if (delivery_status >= 1) {
            lv_label_set_text(st, ST_DELIVERED);
            lv_obj_set_style_text_color(st, UI_COLOR_SECONDARY, 0);
        } else {
            lv_label_set_text(st, ST_SENT);
            lv_obj_set_style_text_color(st, UI_COLOR_TEXT_DIM, 0);
        }
        lv_obj_set_style_text_font(st, UI_FONT_MD, 0);
        lv_obj_add_flag(st, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(st, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(st, LV_ALIGN_TOP_RIGHT, 0, 1);
        /* No tracking — history messages don't need live status updates */
    } else {
        /* Timestamp right for incoming */
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    /* No individual scroll/refresh — caller scrolls once after loading batch */
    ESP_LOGD(TAG, "History %s: \"%.*s%s\"",
             is_outgoing ? "OUT" : "IN",
             20, text, strlen(text) > 20 ? "..." : "");
}

void ui_chat_scroll_to_bottom(void)
{
    if (msg_container) {
        lv_obj_update_layout(msg_container);
        lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

/* ============== Session 37b: Loading Indicator ============== */

void ui_chat_show_loading(void)
{
    if (!msg_container) return;
    ui_chat_hide_loading();  // remove stale one if any

    // Transparent wrapper fills the empty msg_container via flex_grow
    s_loading_box = lv_obj_create(msg_container);
    lv_obj_remove_style_all(s_loading_box);
    lv_obj_set_width(s_loading_box, LV_PCT(100));
    lv_obj_set_flex_grow(s_loading_box, 1);
    lv_obj_set_style_bg_opa(s_loading_box, LV_OPA_TRANSP, 0);

    // Centered label inside wrapper
    lv_obj_t *lbl = lv_label_create(s_loading_box);
    lv_label_set_text(lbl, "Loading...");
    lv_obj_set_style_text_color(lbl, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_center(lbl);
}

void ui_chat_hide_loading(void)
{
    if (s_loading_box) {
        lv_obj_delete(s_loading_box);  // deletes child label too
        s_loading_box = NULL;
    }
}
