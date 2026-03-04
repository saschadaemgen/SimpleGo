/**
 * @file ui_chat_bubble.c
 * @brief Chat Bubble Rendering & Delivery Status Tracking
 *
 * Bubble creation with asymmetric corners (custom draw callback),
 * delivery status tracking, timestamps, and sequence numbering.
 *
 * Session 39f: Extracted from monolithic ui_chat.c.
 * Merges duplicate code from ui_chat_add_message() and
 * ui_chat_add_history_message() into single create_bubble_internal().
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat_bubble.h"
#include "ui_theme.h"
#include "smp_history.h"       /* Session 40b: HISTORY_DISPLAY_TEXT for bubble truncation */
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_BUBBLE";

/* ============== Bubble Styling Constants ============== */

#define BUBBLE_RADIUS       12
#define BUBBLE_PAD_T        5
#define BUBBLE_PAD_B        2
#define BUBBLE_PAD_L        8
#define BUBBLE_PAD_R        8
#define BUBBLE_BORDER_W     1

/* Bubble colors — extremely subtle tint, barely visible */
#define BUBBLE_BG_OUT       lv_color_hex(0x000C00)  /* pure dark green, no red */
#define BUBBLE_BG_IN        lv_color_hex(0x000010)  /* pure dark blue, no red  */

/* Border opacities — more neon glow */
#define BORDER_OPA_OUT      ((lv_opa_t)120)  /* ~47% green — more neon */
#define BORDER_OPA_IN       ((lv_opa_t)105)  /* ~41% cyan  — more neon */

/* Delivery status text — LVGL symbols rendered at Font 12
 * (Font 10 clips the symbol glyphs at the top) */
#define ST_SENDING          "..."
#define ST_SENT             LV_SYMBOL_OK
#define ST_DELIVERED        LV_SYMBOL_OK " " LV_SYMBOL_OK
#define ST_FAILED           LV_SYMBOL_CLOSE

/* ============== Delivery Tracking State ============== */

#define MAX_TRACKED_MSGS 16

typedef struct {
    lv_obj_t *status_label;
    uint32_t  msg_seq;
    bool      active;
} tracked_msg_t;

static tracked_msg_t tracked_msgs[MAX_TRACKED_MSGS] = {0};
static uint32_t msg_seq_counter = 0;

/* ============== Session 40b: Bubble Count Tracking ============== */

static int s_bubble_count = 0;   /* Active bubbles in LVGL 64KB pool */

/* 42f: Reset all state when chat screen is destroyed */
void chat_bubble_cleanup(void)
{
    memset(tracked_msgs, 0, sizeof(tracked_msgs));
    msg_seq_counter = 0;
    s_bubble_count = 0;
}

/* LVGL pool safety: refuse new bubbles when free memory drops below this.
 * Must account for: (a) the bubble about to be created (~1400 bytes worst case)
 * plus (b) minimum LVGL working memory for layout/render (~3000 bytes).
 * Session 42e: Raised from 4096 to 4500 to prevent freeze when pool is
 * tight after contact switch (bubble passes old check at 4116 free, costs
 * 1348, leaving only 2768 which freezes LVGL). */
#define LVGL_POOL_SAFETY_MARGIN  4500

/* ============== Timestamp Helpers ============== */

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

    /* --- L-shaped fill: two strips that ONLY cover the
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

/* ============================================================
 * UNIFIED BUBBLE CREATION
 *
 * Merges the duplicate code from ui_chat_add_message() and
 * ui_chat_add_history_message() into a single function.
 *
 * delivery_status controls behavior:
 *   -1  = LIVE    → show "...", track for updates
 *    0  = HISTORY → show single check (sent)
 *    1+ = HISTORY → show double check (delivered)
 * ============================================================ */

static void create_bubble_internal(lv_obj_t *container, const char *text,
                                   bool is_outgoing, int contact_idx,
                                   int active_contact, const char *ts,
                                   int delivery_status, bool auto_scroll)
{
    if (!container || !text) return;

    /* Session 40b: LVGL pool safety check.
     * Refuse new bubbles when the 64KB built-in pool is critically low.
     * Prevents display freeze from pool exhaustion. */
    lv_mem_monitor_t mon_pre;
    lv_mem_monitor(&mon_pre);
    if (mon_pre.free_size < LVGL_POOL_SAFETY_MARGIN) {
        ESP_LOGW(TAG, "LVGL pool low: %u bytes free (need %u), skipping bubble #%d",
                 (unsigned)mon_pre.free_size, LVGL_POOL_SAFETY_MARGIN, s_bubble_count);
        return;
    }

    /* Session 40b: Truncate long messages to protect LVGL 64KB pool.
     * A single 4KB message label would consume significant pool space.
     * Full text remains in PSRAM cache, only display portion in LVGL. */
    char display_text[HISTORY_DISPLAY_TEXT + 4];  /* +4 for "..." + null */
    const char *label_text = text;
    size_t text_strlen = strlen(text);
    if (text_strlen > HISTORY_DISPLAY_TEXT) {
        memcpy(display_text, text, HISTORY_DISPLAY_TEXT);
        memcpy(display_text + HISTORY_DISPLAY_TEXT, "...", 4);
        label_text = display_text;
        ESP_LOGD(TAG, "Truncated %u -> %u chars for LVGL pool safety",
                 (unsigned)text_strlen, HISTORY_DISPLAY_TEXT);
    }

    lv_coord_t cont_inner = lv_obj_get_content_width(container);
    lv_coord_t bubble_max = cont_inner * 72 / 100;
    lv_coord_t margin_push = cont_inner - bubble_max;

    lv_color_t bg      = is_outgoing ? BUBBLE_BG_OUT : BUBBLE_BG_IN;
    lv_color_t bdr_col = is_outgoing ? UI_COLOR_SECONDARY : UI_COLOR_PRIMARY;
    lv_opa_t   bdr_opa = is_outgoing ? BORDER_OPA_OUT : BORDER_OPA_IN;

    /* ===== Bubble ===== */
    lv_obj_t *bubble = lv_obj_create(container);
    /* 35e: Tag bubble with contact index for filtering (+1 so 0 means "untagged") */
    lv_obj_set_user_data(bubble, (void *)(intptr_t)(contact_idx + 1));
    /* Hide if not active contact */
    if (contact_idx != active_contact) {
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
    lv_label_set_text(label, label_text);  /* Session 40b: uses truncated text */
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);

    /* ---- Meta row ---- */
    lv_obj_t *meta = lv_obj_create(bubble);
    lv_obj_set_width(meta, LV_PCT(100));
    lv_obj_set_height(meta, LV_SIZE_CONTENT);
    ui_style_reset(meta);
    lv_obj_add_flag(meta, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_min_height(meta, 14, 0);

    if (is_outgoing) {
        /* Timestamp left-aligned */
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        /* Delivery checkmarks: floating in top-right of bubble */
        lv_obj_t *st = lv_label_create(bubble);
        lv_obj_set_style_text_font(st, UI_FONT_MD, 0);
        lv_obj_add_flag(st, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(st, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(st, LV_ALIGN_TOP_RIGHT, 0, 1);

        if (delivery_status < 0) {
            /* LIVE: show "...", track for updates */
            lv_label_set_text(st, ST_SENDING);
            lv_obj_set_style_text_color(st, UI_COLOR_META_DIM, 0);

            uint32_t seq = msg_seq_counter;
            for (int i = 0; i < MAX_TRACKED_MSGS; i++) {
                if (!tracked_msgs[i].active) {
                    tracked_msgs[i].status_label = st;
                    tracked_msgs[i].msg_seq = seq;
                    tracked_msgs[i].active = true;
                    break;
                }
            }
        } else if (delivery_status >= 1) {
            /* HISTORY: delivered */
            lv_label_set_text(st, ST_DELIVERED);
            lv_obj_set_style_text_color(st, UI_COLOR_SECONDARY, 0);
        } else {
            /* HISTORY: sent */
            lv_label_set_text(st, ST_SENT);
            lv_obj_set_style_text_color(st, UI_COLOR_TEXT_DIM, 0);
        }
    } else {
        /* Timestamp right-aligned for incoming */
        lv_obj_t *time_lbl = lv_label_create(meta);
        lv_label_set_text(time_lbl, ts);
        lv_obj_set_style_text_color(time_lbl, UI_COLOR_META_DIM, 0);
        lv_obj_set_style_text_font(time_lbl, UI_FONT_SM, 0);
        lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    /* Session 40b: LVGL pool monitoring -- measure actual cost per bubble */
    s_bubble_count++;
    lv_mem_monitor_t mon_post;
    lv_mem_monitor(&mon_post);
    size_t bubble_cost = (mon_pre.free_size > mon_post.free_size)
                        ? (mon_pre.free_size - mon_post.free_size) : 0;
    ESP_LOGI(TAG, "LVGL Pool: bubble #%d cost=%u bytes, free=%u/%u (%u%% used), frags=%u%%",
             s_bubble_count,
             (unsigned)bubble_cost,
             (unsigned)mon_post.free_size,
             (unsigned)mon_post.total_size,
             (unsigned)mon_post.used_pct,
             (unsigned)mon_post.frag_pct);

    /* Scroll + refresh (only for live messages) */
    if (auto_scroll) {
        lv_obj_update_layout(container);
        lv_obj_scroll_to_y(container, LV_COORD_MAX, LV_ANIM_ON);
        lv_obj_t *scr = lv_obj_get_screen(container);
        if (scr) {
            lv_obj_invalidate(scr);
        }
        lv_refr_now(NULL);
    }

    ESP_LOGD(TAG, "%s %s: \"%.*s%s\"",
             delivery_status < 0 ? "Live" : "History",
             is_outgoing ? "OUT" : "IN",
             20, text, strlen(text) > 20 ? "..." : "");
}

/* ============== Public API ============== */

void chat_bubble_add_live(lv_obj_t *container, const char *text,
                          bool is_outgoing, int contact_idx,
                          int active_contact)
{
    char ts[20];
    get_timestamp(ts, sizeof(ts));
    create_bubble_internal(container, text, is_outgoing,
                           contact_idx, active_contact,
                           ts, -1, true);
}

void chat_bubble_add_history(lv_obj_t *container, const char *text,
                             bool is_outgoing, int contact_idx,
                             int active_contact, int64_t timestamp,
                             uint8_t delivery_status)
{
    char ts[20];
    format_history_timestamp(timestamp, ts, sizeof(ts));
    create_bubble_internal(container, text, is_outgoing,
                           contact_idx, active_contact,
                           ts, (int)delivery_status, false);
}

void chat_bubble_update_status(uint32_t msg_seq, int status)
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

uint32_t chat_bubble_next_seq(void)
{
    return ++msg_seq_counter;
}

uint32_t chat_bubble_get_last_seq(void)
{
    return msg_seq_counter;
}

/* ============== Session 40b: Bubble Count API ============== */

int chat_bubble_get_count(void)
{
    return s_bubble_count;
}

void chat_bubble_reset_count(void)
{
    s_bubble_count = 0;
    ESP_LOGD(TAG, "Bubble count reset to 0");
}

void chat_bubble_decrement_count(int n)
{
    s_bubble_count -= n;
    if (s_bubble_count < 0) s_bubble_count = 0;
    ESP_LOGD(TAG, "Bubble count decremented by %d, now %d", n, s_bubble_count);
}

/* ============== Session 40c: Bubble Remove Helpers ============== */

int chat_bubble_remove_oldest(lv_obj_t *container, int count, int contact_idx)
{
    if (!container || count <= 0) return 0;

    int removed = 0;
    uint32_t i = 0;
    while (i < lv_obj_get_child_count(container) && removed < count) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        int tag = (int)(intptr_t)lv_obj_get_user_data(child);
        if (tag != 0 && (tag - 1) == contact_idx) {
            lv_obj_delete(child);
            removed++;
            /* Don't increment i: next child shifts into this position */
        } else {
            i++;
        }
    }
    s_bubble_count -= removed;
    if (s_bubble_count < 0) s_bubble_count = 0;
    ESP_LOGD(TAG, "Removed %d oldest bubbles (contact %d), count now %d",
             removed, contact_idx, s_bubble_count);
    return removed;
}

int chat_bubble_remove_newest(lv_obj_t *container, int count, int contact_idx)
{
    if (!container || count <= 0) return 0;

    int removed = 0;
    /* Iterate from last child backward */
    int32_t i = (int32_t)lv_obj_get_child_count(container) - 1;
    while (i >= 0 && removed < count) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        int tag = (int)(intptr_t)lv_obj_get_user_data(child);
        if (tag != 0 && (tag - 1) == contact_idx) {
            lv_obj_delete(child);
            removed++;
        }
        i--;
    }
    s_bubble_count -= removed;
    if (s_bubble_count < 0) s_bubble_count = 0;
    ESP_LOGD(TAG, "Removed %d newest bubbles (contact %d), count now %d",
             removed, contact_idx, s_bubble_count);
    return removed;
}
