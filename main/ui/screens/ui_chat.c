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
 * Bubble rendering extracted to ui_chat_bubble.c
 *              Style helpers centralized in ui_theme.c
 *
 * SimpleGo UI - v2 Redesign
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat.h"
#include "ui_chat_bubble.h"

/* Cleanup tracked message state when screen is destroyed.
 * TODO: Move this declaration to ui_chat_bubble.h */
extern void chat_bubble_cleanup(void);
#include "ui_theme.h"
#include "ui_manager.h"
#include "tdeck_keyboard.h"
#include "smp_history.h"       /* history_message_t for PSRAM cache */
#include "esp_heap_caps.h"     /* PSRAM allocation */
#include "esp_log.h"
#include <string.h>
#include <time.h>             /* time() for live message timestamp */

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
static lv_obj_t *s_loading_box = NULL;   /* "Loading..." indicator */
static lv_obj_t *s_settings_icon = NULL; /* Settings button in header */

static ui_chat_send_cb_t send_cb    = NULL;
static lv_indev_t *pending_kb_indev = NULL;

/* Active contact for message filtering */
static int s_chat_active_contact = 0;

/* ============== Sliding Window Chat History ============== */

#define MSG_CACHE_SIZE      30   /* Max messages in PSRAM ring cache */
#define BUBBLE_WINDOW_SIZE   5   /* Max simultaneous bubbles in LVGL pool */
#define SCROLL_LOAD_COUNT    2   /* Bubbles to add/remove per scroll trigger */
#define SCROLL_TOP_THRESHOLD 10  /* Pixels from top to trigger older load */
#define SCROLL_BTM_THRESHOLD 10  /* Pixels from bottom to trigger newer load */

static history_message_t *s_msg_cache = NULL;  /* PSRAM, allocated on first use */
static int  s_cache_count    = 0;    /* Messages currently in cache */
static int  s_cache_slot     = -1;   /* Contact slot for current cache */

/* Window tracks which cache indices are currently rendered as LVGL bubbles.
 * s_window_start = cache index of the OLDEST visible bubble (top of screen)
 * s_window_end   = cache index AFTER the NEWEST visible bubble (bottom)
 * Invariant: s_window_end - s_window_start <= BUBBLE_WINDOW_SIZE */
static int  s_window_start   = 0;
static int  s_window_end     = 0;
static bool s_loading_older  = false; /* Debounce guard for scroll-up loading */
static bool s_loading_newer  = false; /* Debounce guard for scroll-down loading */
static bool s_window_busy    = false; /* Re-entrancy guard: blocks ALL scroll triggers while loading */
static bool s_window_setup   = false; /* Guard: block scroll-cb during cache+render setup */
static bool s_scroll_cb_registered = false;

/* Forward declaration for scroll handler */
static void on_scroll_cb(lv_event_t *e);

/* ============== Screen Cleanup ============== */

void ui_chat_cleanup(void)
{
    /* Null all LVGL object pointers BEFORE lv_obj_del(screen).
     * lv_obj_del destroys the objects, but these static pointers
     * would still reference freed memory (dangling pointers).
     * Background tasks (switch_contact, add_message) check these
     * and would crash without this cleanup. */
    screen          = NULL;
    header_label    = NULL;
    msg_container   = NULL;
    input_area      = NULL;
    s_loading_box   = NULL;
    s_settings_icon = NULL;

    /* input_group is NOT a child of the screen, must be freed separately */
    if (input_group) {
        lv_group_del(input_group);
        input_group = NULL;
    }

    /* Reset window state */
    s_window_start  = 0;
    s_window_end    = 0;
    s_cache_count   = 0;
    s_cache_slot    = -1;
    s_loading_older = false;
    s_loading_newer = false;
    s_window_busy   = false;
    s_window_setup  = false;
    s_scroll_cb_registered = false;

    /* Reset bubble tracking (status_label pointers are dangling too) */
    chat_bubble_cleanup();

    ESP_LOGI(TAG, "42f: Chat screen cleanup (all pointers nulled)");
}

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

/* Settings button handlers */
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
    lv_obj_set_width(name, 148);  /* narrower for settings button */
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, UI_FONT, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 28, 0);

    /* Settings gear button (short=toggle kbd, long=settings) */
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

    /* Scroll handler for sliding window (load older on scroll-up) */
    lv_obj_add_event_cb(msg_container, on_scroll_cb, LV_EVENT_SCROLL, NULL);
    s_scroll_cb_registered = true;

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
    /* If chat screen was destroyed, skip bubble creation.
     * Message is already saved to SD history by the caller. */
    if (!screen) return;

    /* Evict BEFORE creating new bubble to guarantee pool space.
     * Previously eviction ran AFTER creation, causing the pool check
     * inside create_bubble_internal() to reject the new bubble when
     * the pool was near capacity (7808 < 8192 threshold). */
    if (contact_idx == s_chat_active_contact) {
        int count = chat_bubble_get_count();
        if (count >= BUBBLE_WINDOW_SIZE) {
            int removed = chat_bubble_remove_oldest(
                msg_container, 1, s_chat_active_contact);
            s_window_start += removed;
            ESP_LOGI(TAG, "42d: Pre-evict %d oldest, window [%d..%d)",
                     removed, s_window_start, s_window_end);
        }
    }

    chat_bubble_add_live(msg_container, text, is_outgoing,
                         contact_idx, s_chat_active_contact);

    /* Add live message to PSRAM cache so scroll-up has no gaps.
     * Only if cache exists and matches current contact. */
    if (s_msg_cache && contact_idx == s_cache_slot
        && s_cache_count < MSG_CACHE_SIZE) {
        history_message_t *m = &s_msg_cache[s_cache_count];
        memset(m, 0, sizeof(history_message_t));
        m->direction = is_outgoing ? HISTORY_DIR_SENT : HISTORY_DIR_RECEIVED;
        m->delivery_status = 0xFF;  /* live: no SD status yet */
        m->timestamp = (int64_t)time(NULL);
        size_t len = strlen(text);
        if (len > HISTORY_MAX_TEXT - 1) len = HISTORY_MAX_TEXT - 1;
        memcpy(m->text, text, len);
        m->text[len] = '\0';
        m->text_len = (uint16_t)len;
        s_cache_count++;
        s_window_end = s_cache_count;
    }

    ESP_LOGI(TAG, "42d: Live %s, bubbles=%d, cache=%d, window [%d..%d)",
             is_outgoing ? "OUT" : "IN",
             chat_bubble_get_count(), s_cache_count,
             s_window_start, s_window_end);
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
    if (!screen) return;  /* no screen, no labels to update */
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
    /* If chat screen was destroyed, only update contact index.
     * Background tasks may call this after navigation away from chat. */
    s_chat_active_contact = contact_idx;
    if (!screen) return;

    /* Set guard IMMEDIATELY to block stale scroll events. */
    s_window_setup = true;

    /* Update header */
    if (header_label && name) {
        char trunc[CHAT_NAME_MAX + 4];
        truncate_chat_name(trunc, name, sizeof(trunc));
        lv_label_set_text(header_label, trunc);
    }

    /* Delete ALL existing bubbles (not just hide/show).
     * Old code only toggled visibility, leaving stale bubbles consuming
     * LVGL pool memory. Since cache_history will render fresh bubbles
     * anyway, full cleanup is safe and prevents pool fragmentation. */
    if (msg_container) {
        lv_obj_clean(msg_container);
        s_loading_box = NULL;
        chat_bubble_reset_count();
    }

    ESP_LOGI(TAG, "42e: Switched to contact [%d] '%s' (pool cleared, scroll guard ON)",
             contact_idx, name ? name : "?");
}

void ui_chat_clear_contact(int contact_idx)
{
    if (!msg_container) return;

    /* -1 = clear ALL bubbles */
    if (contact_idx < 0) {
        lv_obj_clean(msg_container);
        s_loading_box = NULL;  /* was a child of msg_container, now deleted */
        chat_bubble_reset_count();  /* reset bubble tracking */
        ESP_LOGI(TAG, "36d: Cleared ALL bubbles");
        return;
    }

    int removed = 0;
    uint32_t i = 0;
    while (i < lv_obj_get_child_count(msg_container)) {
        lv_obj_t *child = lv_obj_get_child(msg_container, i);
        int tag = (int)(intptr_t)lv_obj_get_user_data(child);
        if (tag != 0 && (tag - 1) == contact_idx) {
            lv_obj_delete(child);
            removed++;
        } else {
            i++;
        }
    }
    if (removed > 0) {
        chat_bubble_decrement_count(removed);
    }
    ESP_LOGI(TAG, "36d: Cleared %d bubbles for contact [%d]", removed, contact_idx);
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

/* ============== Sliding Window Implementation ============== */

void ui_chat_cache_history(const history_message_t *batch, int count, int slot)
{
    /* If chat screen was destroyed, skip entirely.
     * Data will be loaded fresh when screen is recreated. */
    if (!screen) return;

    /* GUARD ON: Block scroll-cb from firing on stale content during setup.
     * Without this, setting window vars while old bubbles exist causes the
     * scroll event to trigger load_older_messages() and corrupt the window. */
    s_window_setup = true;

    /* Clear ALL existing bubbles BEFORE setting window state.
     * This prevents scroll events from seeing stale content.
     * Force layout update after cleanup to help LVGL reclaim
     * freed pool memory before new bubbles are created. */
    if (msg_container) {
        lv_obj_clean(msg_container);
        s_loading_box = NULL;
        chat_bubble_reset_count();
        lv_obj_update_layout(msg_container);

        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        ESP_LOGI(TAG, "42e: Pool after cleanup: free=%u/%u (%u%% used), frags=%u%%",
                 (unsigned)mon.free_size, (unsigned)mon.total_size,
                 (unsigned)mon.used_pct, (unsigned)mon.frag_pct);
    }

    /* Allocate PSRAM cache on first use */
    if (!s_msg_cache) {
        s_msg_cache = heap_caps_malloc(
            sizeof(history_message_t) * MSG_CACHE_SIZE, MALLOC_CAP_SPIRAM);
        if (!s_msg_cache) {
            ESP_LOGE(TAG, "40c: PSRAM cache alloc failed (%u bytes)",
                     (unsigned)(sizeof(history_message_t) * MSG_CACHE_SIZE));
            s_window_setup = false;
            return;
        }
        ESP_LOGI(TAG, "40c: PSRAM cache allocated (%u bytes)",
                 (unsigned)(sizeof(history_message_t) * MSG_CACHE_SIZE));
    }

    /* Copy last MSG_CACHE_SIZE messages into cache */
    int to_copy = count;
    int src_start = 0;
    if (to_copy > MSG_CACHE_SIZE) {
        src_start = count - MSG_CACHE_SIZE;
        to_copy = MSG_CACHE_SIZE;
    }
    memcpy(s_msg_cache, &batch[src_start],
           (size_t)to_copy * sizeof(history_message_t));

    s_cache_count = to_copy;
    s_cache_slot = slot;

    /* Calculate initial window: show last N messages where N depends
     * on available LVGL pool. Pool may be fragmented/tight after
     * contact switches. Check actual free memory and reduce window if
     * the pool can't hold BUBBLE_WINDOW_SIZE bubbles safely.
     * Each bubble costs ~1200-1400 bytes. Reserve 4500 for LVGL. */
    lv_mem_monitor_t win_mon;
    lv_mem_monitor(&win_mon);
    int pool_budget = (int)win_mon.free_size - 4500;
    int max_safe = pool_budget / 1200;  /* avg bubble cost from measurements */
    if (max_safe < 1) max_safe = 1;
    if (max_safe > BUBBLE_WINDOW_SIZE) max_safe = BUBBLE_WINDOW_SIZE;

    s_window_end = s_cache_count;
    s_window_start = s_cache_count - max_safe;
    if (s_window_start < 0) s_window_start = 0;

    if (max_safe < BUBBLE_WINDOW_SIZE) {
        ESP_LOGW(TAG, "42e: Pool tight (%d free), window reduced to %d bubbles",
                 (int)win_mon.free_size, max_safe);
    }

    s_loading_older = false;
    s_loading_newer = false;
    s_window_busy = false;

    /* Guard stays ON until ui_chat_window_render_done() is called by main.c
     * after progressive render completes. */
    ESP_LOGI(TAG, "40c: Cached %d msgs (slot %d), window [%d..%d), guard ON",
             s_cache_count, slot, s_window_start, s_window_end);
}

int ui_chat_get_window_start(void)
{
    return s_window_start;
}

int ui_chat_get_window_end(void)
{
    return s_window_end;
}

lv_obj_t *ui_chat_get_msg_container(void)
{
    return msg_container;
}

int ui_chat_get_active_contact(void)
{
    return s_chat_active_contact;
}

/**
 * Render one history message from cache as an LVGL bubble.
 * Used by both initial render (from main.c timer) and scroll-up loading.
 * Detects if create_bubble_internal skipped due to pool safety,
 * and adjusts window state to prevent inconsistency.
 */
static void render_cache_bubble(int cache_idx, bool at_top)
{
    if (cache_idx < 0 || cache_idx >= s_cache_count || !msg_container) return;

    int count_before = chat_bubble_get_count();

    history_message_t *m = &s_msg_cache[cache_idx];

    if (at_top) {
        /* Create bubble normally, then move to top of container */
        chat_bubble_add_history(msg_container, m->text,
                                m->direction == HISTORY_DIR_SENT,
                                s_cache_slot, s_chat_active_contact,
                                m->timestamp, m->delivery_status);

        /* Move the just-created bubble (last child) to position 0 */
        uint32_t last = lv_obj_get_child_count(msg_container) - 1;
        lv_obj_t *bubble = lv_obj_get_child(msg_container, last);
        if (bubble) {
            lv_obj_move_to_index(bubble, 0);
        }
    } else {
        /* Append at bottom (normal order) */
        chat_bubble_add_history(msg_container, m->text,
                                m->direction == HISTORY_DIR_SENT,
                                s_cache_slot, s_chat_active_contact,
                                m->timestamp, m->delivery_status);
    }

    /* If bubble count didn't increase, creation was skipped (pool safety).
     * Adjust window to match actual rendered state. */
    if (chat_bubble_get_count() == count_before) {
        if (at_top) {
            s_window_start = cache_idx + 1;
        } else {
            s_window_end = cache_idx;
        }
        ESP_LOGW(TAG, "42e: Bubble skipped at cache[%d], window adjusted to [%d..%d)",
                 cache_idx, s_window_start, s_window_end);
    }
}

static void load_older_messages(void)
{
    if (s_window_start <= 0) {
        ESP_LOGD(TAG, "40c: No older messages in cache");
        s_loading_older = false;
        s_window_busy = false;
        return;
    }

    /* Calculate how many to add (don't exceed cache bounds) */
    int to_add = SCROLL_LOAD_COUNT;
    if (to_add > s_window_start) to_add = s_window_start;

    /* Remove newest bubbles from bottom to stay within BUBBLE_WINDOW_SIZE */
    int current_bubbles = s_window_end - s_window_start;
    int need_remove = (current_bubbles + to_add) - BUBBLE_WINDOW_SIZE;
    if (need_remove > 0) {
        int removed = chat_bubble_remove_newest(
            msg_container, need_remove, s_cache_slot);
        s_window_end -= removed;
    }

    /* Measure content height BEFORE inserting */
    lv_obj_update_layout(msg_container);
    lv_coord_t old_scroll_y = lv_obj_get_scroll_y(msg_container);
    lv_coord_t old_content_h = lv_obj_get_scroll_top(msg_container)
                             + lv_obj_get_height(msg_container)
                             + lv_obj_get_scroll_bottom(msg_container);

    /* Insert older messages at TOP of container.
     * Insert from newest-of-batch to oldest-of-batch, each at index 0.
     * This produces correct chronological order:
     *   insert cache[start-1] at 0 -> [start-1]
     *   insert cache[start-2] at 0 -> [start-2, start-1]
     *   insert cache[start-3] at 0 -> [start-3, start-2, start-1]  */
    int new_start = s_window_start - to_add;
    for (int i = s_window_start - 1; i >= new_start; i--) {
        render_cache_bubble(i, true);
    }
    s_window_start = new_start;

    /* Measure content height AFTER inserting and correct scroll position.
     * New bubbles at top pushed content down. We scroll down by the added
     * height so the user stays looking at the same messages. LV_ANIM_OFF
     * prevents visible jump. */
    lv_obj_update_layout(msg_container);
    lv_coord_t new_content_h = lv_obj_get_scroll_top(msg_container)
                             + lv_obj_get_height(msg_container)
                             + lv_obj_get_scroll_bottom(msg_container);
    lv_coord_t height_diff = new_content_h - old_content_h;
    if (height_diff > 0) {
        lv_obj_scroll_to_y(msg_container, old_scroll_y + height_diff, LV_ANIM_OFF);
    }

    s_loading_older = false;
    s_window_busy = false;
    ESP_LOGI(TAG, "40c: Scroll-load %d older, window [%d..%d), bubbles=%d",
             to_add, s_window_start, s_window_end, chat_bubble_get_count());
}

static void load_newer_messages(void)
{
    if (s_window_end >= s_cache_count) {
        ESP_LOGD(TAG, "40c: No newer messages in cache");
        s_loading_newer = false;
        s_window_busy = false;
        return;
    }

    /* Calculate how many to add (don't exceed cache bounds) */
    int to_add = SCROLL_LOAD_COUNT;
    if (s_window_end + to_add > s_cache_count)
        to_add = s_cache_count - s_window_end;

    /* Remove oldest bubbles from top to stay within BUBBLE_WINDOW_SIZE.
     * Measure scroll BEFORE removing so we can compensate for the lost height. */
    lv_obj_update_layout(msg_container);
    lv_coord_t old_scroll_y = lv_obj_get_scroll_y(msg_container);
    lv_coord_t old_content_h = lv_obj_get_scroll_top(msg_container)
                             + lv_obj_get_height(msg_container)
                             + lv_obj_get_scroll_bottom(msg_container);

    int current_bubbles = s_window_end - s_window_start;
    int need_remove = (current_bubbles + to_add) - BUBBLE_WINDOW_SIZE;
    if (need_remove > 0) {
        int removed = chat_bubble_remove_oldest(
            msg_container, need_remove, s_cache_slot);
        s_window_start += removed;
    }

    /* Append newer messages at BOTTOM (normal order, no reorder needed) */
    for (int i = s_window_end; i < s_window_end + to_add; i++) {
        render_cache_bubble(i, false);
    }
    s_window_end += to_add;

    /* Correct scroll position: removing bubbles at top shifted content up.
     * Adjust scroll_y downward by the height that was removed so the user
     * keeps looking at the same messages. */
    lv_obj_update_layout(msg_container);
    lv_coord_t new_content_h = lv_obj_get_scroll_top(msg_container)
                             + lv_obj_get_height(msg_container)
                             + lv_obj_get_scroll_bottom(msg_container);
    lv_coord_t height_diff = old_content_h - new_content_h;
    if (height_diff > 0) {
        lv_coord_t corrected = old_scroll_y - height_diff;
        if (corrected < 0) corrected = 0;
        lv_obj_scroll_to_y(msg_container, corrected, LV_ANIM_OFF);
    }

    s_loading_newer = false;
    s_window_busy = false;
    ESP_LOGI(TAG, "40c: Scroll-load %d newer, window [%d..%d), bubbles=%d",
             to_add, s_window_start, s_window_end, chat_bubble_get_count());
}

static void on_scroll_cb(lv_event_t *e)
{
    lv_obj_t *container = lv_event_get_target(e);
    if (!container || !s_msg_cache || s_cache_count == 0) return;

    /* Block during cache+render setup to prevent race condition */
    if (s_window_setup) return;

    /* Block re-entrant calls: lv_obj_scroll_to_y() inside load_older/newer
     * fires another LV_EVENT_SCROLL before the load function returns.
     * Without this, scroll-up triggers scroll-down in the same frame. */
    if (s_window_busy) return;

    lv_coord_t scroll_y = lv_obj_get_scroll_y(container);

    /* Near top of scroll area AND there are older messages? */
    if (scroll_y <= SCROLL_TOP_THRESHOLD
        && s_window_start > 0
        && !s_loading_older) {
        s_loading_older = true;
        s_window_busy = true;
        ESP_LOGI(TAG, "40c: Scroll-up trigger (scroll_y=%d, window [%d..%d))",
                 (int)scroll_y, s_window_start, s_window_end);
        load_older_messages();
        return;  /* Don't check bottom in same event */
    }

    /* Near bottom of scroll area AND there are newer messages?
     * lv_obj_get_scroll_bottom() returns pixels remaining below viewport. */
    lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(container);
    if (scroll_bottom <= SCROLL_BTM_THRESHOLD
        && s_window_end < s_cache_count
        && !s_loading_newer) {
        s_loading_newer = true;
        s_window_busy = true;
        ESP_LOGI(TAG, "40c: Scroll-down trigger (scroll_btm=%d, window [%d..%d))",
                 (int)scroll_bottom, s_window_start, s_window_end);
        load_newer_messages();
    }
}

void ui_chat_window_render_done(void)
{
    s_window_setup = false;
    ESP_LOGI(TAG, "40c: Render done, guard OFF, window [%d..%d), bubbles=%d",
             s_window_start, s_window_end, chat_bubble_get_count());
}

/* ============== Settings Icon Update ============== */

void ui_chat_update_settings_icon(void)
{
    if (s_settings_icon) {
        lv_obj_set_style_text_color(s_settings_icon,
            tdeck_kbd_backlight_is_on() ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    }
}
