/**
 * @file ui_settings_info.c
 * @brief Settings Screen - INFO Tab (Four-column dual-row design)
 *
 * Session 47: Rewritten to four-column layout (two key-value pairs per row).
 * Security section with NVS mode, eFuse status, PQ toggle.
 * eFuse status read from chip at runtime, not compile-time.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_settings_internal.h"
#include "smp_types.h"
#include "smp_storage.h"
#include "smp_ratchet.h"     // Session 47: PQ toggle (get/set pq_enabled)
#include "smp_servers.h"     // Session 49: Server management overlay
#include "smp_rotation.h"   // Session 49: Queue Rotation for server switch
#include "smp_contacts.h"   // Session 49: count_active for rotation decision
#include "ui_manager.h"    // Session 49: Navigate to contacts after rotation start
#include "esp_efuse.h"        // Session 47: Runtime eFuse status
#include "esp_system.h"       // Session 49: esp_restart() for server switch
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"    // Session 49: vTaskDelay before restart
#include "lvgl.h"

/* Row dimensions */
#define ROW_H           24
#define ROW_GAP         2
#define ROW_BG          lv_color_hex(0x000810)
#define ACCENT_W        3
#define ACCENT_H        16
#define BRAND_H         28

/* Four-column positions (two key-value pairs per row) */
#define L_KEY_X         14      /* Left key */
#define L_VAL_X         78      /* Left value */
#define R_KEY_X         174     /* Right key */
#define R_VAL_X         238     /* Right value */
#define R_ACCENT_X      166     /* Right accent bar */

/* ================================================================
 * State
 * ================================================================ */

static lv_obj_t *s_list         = NULL;
static lv_obj_t *s_heap_val     = NULL;
static lv_obj_t *s_psram_val    = NULL;
static lv_obj_t *s_lvgl_val     = NULL;
static lv_obj_t *s_wifi_val     = NULL;   /* WiFi signal strength */
static lv_obj_t *s_name_val     = NULL;
static lv_obj_t *s_brand_pq     = NULL;   /* PQ status in branding row (clickable) */
static lv_obj_t *s_tz_val       = NULL;   /* Timezone offset label */
static lv_timer_t *s_refresh_timer = NULL;

/* Name editor overlay */
static lv_obj_t *s_name_overlay    = NULL;
static lv_obj_t *s_name_ta         = NULL;
static lv_group_t *s_name_group    = NULL;
static lv_group_t *s_name_prev_group = NULL;

/* Server settings overlay (Session 49) */
static lv_obj_t *s_srv_overlay     = NULL;
static lv_obj_t *s_srv_content     = NULL;

/* Server switch popup (Session 49 rewrite) */
static lv_obj_t *s_srv_popup       = NULL;
static int       s_pending_srv_idx = -1;

/* ================================================================
 * Timezone Offset (Session 48)
 * ================================================================ */

static void update_tz_label(void)
{
    if (!s_tz_val) return;
    char buf[12];
    snprintf(buf, sizeof(buf), "UTC%+d", g_tz_offset_hours);
    lv_label_set_text(s_tz_val, buf);
}

static void on_tz_minus(lv_event_t *e)
{
    (void)e;
    if (g_tz_offset_hours > -12) {
        storage_set_tz_offset(g_tz_offset_hours - 1);
        update_tz_label();
    }
}

static void on_tz_plus(lv_event_t *e)
{
    (void)e;
    if (g_tz_offset_hours < 14) {
        storage_set_tz_offset(g_tz_offset_hours + 1);
        update_tz_label();
    }
}

/* ================================================================
 * Display Lock Mode Toggle (Session 48)
 * ================================================================ */

#define NVS_KEY_DISPLAY_LOCK  "disp_lock"

static lv_obj_t *s_lock_simple = NULL;
static lv_obj_t *s_lock_matrix = NULL;
static lv_obj_t *s_lock_timer_val = NULL;

static uint8_t get_lock_mode(void)
{
    uint8_t mode = 0;
    size_t out_len = 0;
    if (smp_storage_load_blob(NVS_KEY_DISPLAY_LOCK, &mode,
                               sizeof(mode), &out_len) == ESP_OK
        && out_len == 1 && mode == 1) {
        return 1;
    }
    return 0;
}

static void update_lock_labels(uint8_t mode)
{
    if (s_lock_simple) {
        lv_obj_set_style_text_color(s_lock_simple,
            mode == 0 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    }
    if (s_lock_matrix) {
        lv_obj_set_style_text_color(s_lock_matrix,
            mode == 1 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    }
}

static void on_lock_simple(lv_event_t *e)
{
    (void)e;
    uint8_t mode = 0;
    smp_storage_save_blob(NVS_KEY_DISPLAY_LOCK, &mode, sizeof(mode));
    update_lock_labels(0);
    ESP_LOGI("UI_INFO", "Display lock mode: Simple");
}

static void on_lock_matrix(lv_event_t *e)
{
    (void)e;
    uint8_t mode = 1;
    smp_storage_save_blob(NVS_KEY_DISPLAY_LOCK, &mode, sizeof(mode));
    update_lock_labels(1);
    ESP_LOGI("UI_INFO", "Display lock mode: Matrix");
}

/* ================================================================
 * Lock Timer +/- (Session 48)
 * ================================================================ */

static void update_lock_timer_label(void)
{
    if (!s_lock_timer_val) return;
    uint8_t idx = ui_manager_get_lock_timer_idx();
    lv_label_set_text(s_lock_timer_val, ui_manager_get_lock_timer_label(idx));
}

static void on_timer_minus(lv_event_t *e)
{
    (void)e;
    uint8_t idx = ui_manager_get_lock_timer_idx();
    if (idx > 0) {
        ui_manager_set_lock_timer_idx(idx - 1);
        update_lock_timer_label();
    }
}

static void on_timer_plus(lv_event_t *e)
{
    (void)e;
    uint8_t idx = ui_manager_get_lock_timer_idx();
    if (idx < ui_manager_get_lock_timer_count() - 1) {
        ui_manager_set_lock_timer_idx(idx + 1);
        update_lock_timer_label();
    }
}

/* ================================================================
 * Helpers
 * ================================================================ */

static bool is_efuse_burned(void)
{
    esp_efuse_purpose_t purpose = esp_efuse_get_key_purpose(EFUSE_BLK_KEY1);
    return (purpose != ESP_EFUSE_KEY_PURPOSE_USER);
}

static void refresh_values(void)
{
    if (s_name_val) {
        char name[32];
        storage_get_display_name(name, sizeof(name));
        lv_label_set_text(s_name_val, name);
    }
    if (s_heap_val) {
        uint32_t fh = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        lv_label_set_text_fmt(s_heap_val, "%luB", (unsigned long)fh);
    }
    if (s_psram_val) {
        uint32_t fp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        lv_label_set_text_fmt(s_psram_val, "%luKB", (unsigned long)(fp / 1024));
    }
    if (s_lvgl_val) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        lv_label_set_text_fmt(s_lvgl_val, "%d%%", mon.used_pct);
    }
    if (s_wifi_val) {
        if (wifi_connected) {
            wifi_status_t ws = wifi_manager_get_status();
            lv_label_set_text_fmt(s_wifi_val, "%ddBm", ws.rssi);
            lv_obj_set_style_text_color(s_wifi_val,
                ws.rssi > -60 ? UI_COLOR_PRIMARY : UI_COLOR_PRIMARY, 0);
        } else {
            lv_label_set_text(s_wifi_val, "Offline");
            lv_obj_set_style_text_color(s_wifi_val, UI_COLOR_PRIMARY, 0);
        }
    }
    /* Update PQ status in branding row */
    if (s_brand_pq) {
        uint8_t pq = smp_settings_get_pq_enabled();
        if (pq) {
            lv_label_set_text(s_brand_pq, "E2EE Quantum-Resistant");
        } else {
            lv_label_set_text(s_brand_pq, "E2EE Standard");
        }
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh_values(); }

/* ================================================================
 * Name Editor Overlay (Session 43 pattern, unchanged)
 * ================================================================ */

static void close_name_overlay(void)
{
    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev && s_name_prev_group) {
        lv_indev_set_group(kb_indev, s_name_prev_group);
    }
    if (s_name_group) {
        lv_group_delete(s_name_group);
        s_name_group = NULL;
    }
    s_name_prev_group = NULL;
    s_name_ta = NULL;

    if (s_name_overlay) {
        lv_obj_delete(s_name_overlay);
        s_name_overlay = NULL;
    }
}

static void on_name_save(lv_event_t *e)
{
    (void)e;
    if (!s_name_ta) return;
    const char *text = lv_textarea_get_text(s_name_ta);
    if (!text || strlen(text) == 0) return;
    esp_err_t ret = storage_set_display_name(text);
    if (ret == ESP_OK) {
        ESP_LOGI("UI_INFO", "Display name saved: %s", text);
    }
    close_name_overlay();
    refresh_values();
}

static void on_name_cancel(lv_event_t *e)
{
    (void)e;
    close_name_overlay();
}

static void show_name_overlay(void)
{
    if (s_name_overlay) close_name_overlay();

    s_name_overlay = lv_obj_create(s_settings_scr);
    lv_obj_set_size(s_name_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_name_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_name_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_name_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_name_overlay, 0, 0);
    lv_obj_set_style_radius(s_name_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_name_overlay, 0, 0);
    lv_obj_clear_flag(s_name_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_name_overlay);
    lv_label_set_text(title, "Display Name");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_SM, 0);
    lv_obj_set_pos(title, 20, 24);

    lv_obj_t *hint = lv_label_create(s_name_overlay);
    lv_label_set_text(hint, "Visible to your contacts");
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    lv_obj_set_pos(hint, 20, 42);

    lv_obj_t *sep = lv_obj_create(s_name_overlay);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 62);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    s_name_ta = lv_textarea_create(s_name_overlay);
    lv_obj_set_size(s_name_ta, 280, 32);
    lv_obj_set_pos(s_name_ta, 20, 76);
    lv_textarea_set_one_line(s_name_ta, true);
    lv_textarea_set_placeholder_text(s_name_ta, "Enter your name...");
    lv_textarea_set_max_length(s_name_ta, 31);
    lv_obj_set_style_bg_color(s_name_ta, lv_color_hex(0x000810), 0);
    lv_obj_set_style_bg_opa(s_name_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_name_ta, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_name_ta, 1, 0);
    lv_obj_set_style_border_opa(s_name_ta, (lv_opa_t)60, 0);
    lv_obj_set_style_text_color(s_name_ta, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(s_name_ta, UI_FONT, 0);
    lv_obj_set_style_radius(s_name_ta, 4, 0);
    lv_obj_set_style_pad_left(s_name_ta, 8, 0);

    char current_name[32];
    storage_get_display_name(current_name, sizeof(current_name));
    lv_textarea_set_text(s_name_ta, current_name);

    lv_obj_t *save_btn = lv_label_create(s_name_overlay);
    lv_label_set_text(save_btn, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_color(save_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(save_btn, UI_FONT, 0);
    lv_obj_set_pos(save_btn, 20, 124);
    lv_obj_add_flag(save_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(save_btn, 10);
    lv_obj_add_event_cb(save_btn, on_name_save, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel = lv_label_create(s_name_overlay);
    lv_label_set_text(cancel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 20, UI_SCREEN_H - 30);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cancel, on_name_cancel, LV_EVENT_CLICKED, NULL);

    lv_indev_t *kb_indev = tdeck_keyboard_get_indev();
    if (kb_indev) {
        s_name_prev_group = lv_indev_get_group(kb_indev);
        s_name_group = lv_group_create();
        lv_group_add_obj(s_name_group, s_name_ta);
        lv_indev_set_group(kb_indev, s_name_group);
        lv_group_focus_obj(s_name_ta);
        lv_obj_add_state(s_name_ta, LV_STATE_FOCUSED);
    }
}

static void on_name_row_click(lv_event_t *e)
{
    (void)e;
    show_name_overlay();
}

/* ================================================================
 * PQ Toggle Event
 * ================================================================ */

static void on_pq_toggle(lv_event_t *e)
{
    (void)e;
    uint8_t current = smp_settings_get_pq_enabled();
    uint8_t next = current ? 0 : 1;
    smp_settings_set_pq_enabled(next);
    ESP_LOGI("UI_INFO", "Post-Quantum %s", next ? "ON" : "OFF");
    refresh_values();
}

/* ================================================================
 * Server Selection Overlay (Session 49 rewrite)
 *
 * Single active server model (radio-button).
 * Flat scrollable list grouped by operator.
 * Server switch stored in NVS, effective on next reboot.
 * Private Message Routing: info-only popup (future feature).
 * ================================================================ */

static void close_srv_popup(void)
{
    if (s_srv_popup) {
        lv_obj_delete(s_srv_popup);
        s_srv_popup = NULL;
    }
    s_pending_srv_idx = -1;
}

static void close_srv_overlay(void)
{
    close_srv_popup();
    if (s_srv_overlay) {
        lv_obj_delete(s_srv_overlay);
        s_srv_overlay = NULL;
    }
    s_srv_content = NULL;
}

static void srv_rebuild_list(void);

/* --- Event handlers --- */

static void on_srv_back(lv_event_t *e)
{
    (void)e;
    close_srv_overlay();
}

static void on_srv_confirm(lv_event_t *e)
{
    (void)e;
    if (s_pending_srv_idx >= 0) {
        /* Count active contacts to decide: rotation or direct switch */
        int active = 0;
        for (int i = 0; i < MAX_CONTACTS; i++) {
            if (contacts_db.contacts[i].active) active++;
        }

        if (active > 0) {
            /* Contacts exist - start queue rotation (no reboot) */
            ESP_LOGI("UI_INFO", "Starting queue rotation to server [%d] (%d contacts)",
                     s_pending_srv_idx, active);
            if (rotation_start(s_pending_srv_idx)) {
                close_srv_overlay();
                /* Navigate to contacts - user sees live rotation status per contact */
                ui_manager_show_screen(UI_SCREEN_CONTACTS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
                return;
            }
            /* rotation_start failed - fall through to direct switch */
            ESP_LOGW("UI_INFO", "rotation_start failed, falling back to direct switch");
        }

        /* No contacts or rotation failed - direct switch + reboot */
        smp_servers_set_active(s_pending_srv_idx);
        ESP_LOGI("UI_INFO", "Server switched to [%d] - rebooting...", s_pending_srv_idx);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }
    close_srv_popup();
}

static void on_srv_cancel(lv_event_t *e)
{
    (void)e;
    close_srv_popup();
}

static void show_srv_switch_popup(int new_idx)
{
    close_srv_popup();

    smp_server_t *srv = smp_servers_get(new_idx);
    if (!srv) return;

    s_pending_srv_idx = new_idx;

    /* Full-screen semi-transparent backdrop */
    s_srv_popup = lv_obj_create(s_srv_overlay);
    lv_obj_set_size(s_srv_popup, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_srv_popup, 0, 0);
    lv_obj_set_style_bg_color(s_srv_popup, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_srv_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_srv_popup, 0, 0);
    lv_obj_set_style_radius(s_srv_popup, 0, 0);
    lv_obj_set_style_pad_all(s_srv_popup, 0, 0);
    lv_obj_clear_flag(s_srv_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(s_srv_popup);
    lv_label_set_text(title, "Switch Server?");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_set_pos(title, 20, 24);

    /* Explanation text */
    lv_obj_t *info1 = lv_label_create(s_srv_popup);
    lv_label_set_text(info1, "Contacts will be migrated to\nthe new server automatically.");
    lv_obj_set_style_text_color(info1, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(info1, UI_FONT_SM, 0);
    lv_obj_set_pos(info1, 20, 50);

    lv_obj_t *info2 = lv_label_create(s_srv_popup);
    lv_label_set_text(info2, "Chat stays available during\nmigration. No messages lost.");
    lv_obj_set_style_text_color(info2, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(info2, UI_FONT_SM, 0);
    lv_obj_set_pos(info2, 20, 82);

    /* New server hostname */
    lv_obj_t *srv_label = lv_label_create(s_srv_popup);
    lv_label_set_text(srv_label, "New server:");
    lv_obj_set_style_text_color(srv_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(srv_label, UI_FONT_SM, 0);
    lv_obj_set_pos(srv_label, 20, 116);

    lv_obj_t *srv_name = lv_label_create(s_srv_popup);
    lv_label_set_text(srv_name, srv->host);
    lv_obj_set_style_text_color(srv_name, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(srv_name, UI_FONT, 0);
    lv_obj_set_pos(srv_name, 20, 132);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(s_srv_popup);
    lv_obj_set_size(sep, 280, 1);
    lv_obj_set_pos(sep, 20, 158);
    lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Cancel button */
    lv_obj_t *cancel = lv_label_create(s_srv_popup);
    lv_label_set_text(cancel, "Cancel");
    lv_obj_set_style_text_color(cancel, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(cancel, UI_FONT, 0);
    lv_obj_set_pos(cancel, 40, 172);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cancel, 10);
    lv_obj_add_event_cb(cancel, on_srv_cancel, LV_EVENT_CLICKED, NULL);

    /* Switch button */
    lv_obj_t *confirm = lv_label_create(s_srv_popup);
    lv_label_set_text(confirm, "Migrate");
    lv_obj_set_style_text_color(confirm, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(confirm, UI_FONT, 0);
    lv_obj_set_pos(confirm, 220, 172);
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(confirm, 10);
    lv_obj_add_event_cb(confirm, on_srv_confirm, LV_EVENT_CLICKED, NULL);
}

static void on_srv_select(lv_event_t *e)
{
    int srv_idx = (int)(intptr_t)lv_event_get_user_data(e);
    int current = smp_servers_get_active_index();

    /* Already active - no action */
    if (srv_idx == current) return;

    show_srv_switch_popup(srv_idx);
}

/* --- Private Message Routing info popup --- */

static void on_pmr_ok(lv_event_t *e)
{
    (void)e;
    close_srv_popup();
}

static void on_pmr_info(lv_event_t *e)
{
    (void)e;
    close_srv_popup();

    s_srv_popup = lv_obj_create(s_srv_overlay);
    lv_obj_set_size(s_srv_popup, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_srv_popup, 0, 0);
    lv_obj_set_style_bg_color(s_srv_popup, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_srv_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_srv_popup, 0, 0);
    lv_obj_set_style_radius(s_srv_popup, 0, 0);
    lv_obj_set_style_pad_all(s_srv_popup, 0, 0);
    lv_obj_clear_flag(s_srv_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_srv_popup);
    lv_label_set_text(title, "Private Message Routing");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_set_pos(title, 20, 60);

    lv_obj_t *info = lv_label_create(s_srv_popup);
    lv_label_set_text(info, "Private message routing will be\navailable in a future version.");
    lv_obj_set_style_text_color(info, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(info, UI_FONT_SM, 0);
    lv_obj_set_pos(info, 20, 90);

    lv_obj_t *ok = lv_label_create(s_srv_popup);
    lv_label_set_text(ok, LV_SYMBOL_OK " OK");
    lv_obj_set_style_text_color(ok, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(ok, UI_FONT, 0);
    lv_obj_set_pos(ok, 130, 140);
    lv_obj_add_flag(ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(ok, 10);
    lv_obj_add_event_cb(ok, on_pmr_ok, LV_EVENT_CLICKED, NULL);
}

/* --- Build the flat list --- */

static void srv_rebuild_list(void)
{
    if (!s_srv_content) return;
    lv_obj_clean(s_srv_content);

    int active_idx = smp_servers_get_active_index();

    /* Order: SimpleGo first, then SimpleX, then Flux */
    int op_order[3] = {SMP_OP_SIMPLEGO, SMP_OP_SIMPLEX, SMP_OP_FLUX};

    for (int oi = 0; oi < 3; oi++) {
        int op_id = op_order[oi];
        const char *op_name = smp_operators_get_name((uint8_t)op_id);

        /* === Operator header (bare label, no container) === */
        lv_obj_t *hdr = lv_label_create(s_srv_content);
        lv_label_set_text_fmt(hdr, "  %s", op_name);
        lv_obj_set_style_text_color(hdr, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(hdr, UI_FONT, 0);
        lv_obj_set_style_pad_top(hdr, 6, 0);
        lv_obj_set_style_pad_bottom(hdr, 2, 0);

        /* === Server rows === */
        int total = smp_servers_count();
        for (int i = 0; i < total; i++) {
            smp_server_t *srv = smp_servers_get(i);
            if (!srv || srv->op != (uint8_t)op_id) continue;

            bool is_active = (i == active_idx);

            /* Row container (clickable = select server) */
            lv_obj_t *row = lv_obj_create(s_srv_content);
            lv_obj_set_size(row, LV_PCT(100), 20);
            lv_obj_set_style_bg_color(row, ROW_BG, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, on_srv_select, LV_EVENT_CLICKED,
                                 (void *)(intptr_t)i);

            /* Hostname with active marker */
            lv_obj_t *host_lbl = lv_label_create(row);
            lv_label_set_text_fmt(host_lbl, " %s %s",
                is_active ? "*" : " ", srv->host);
            lv_obj_set_style_text_color(host_lbl,
                is_active ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_WHITE, 0);
            lv_obj_set_style_text_font(host_lbl, UI_FONT_SM, 0);
            lv_obj_align(host_lbl, LV_ALIGN_LEFT_MID, 4, 0);

            /* Route label (clickable, shows PMR info popup) */
            lv_obj_t *route_lbl = lv_label_create(row);
            lv_label_set_text(route_lbl, "Route");
            lv_obj_set_style_text_color(route_lbl, UI_COLOR_TEXT_DIM, 0);
            lv_obj_set_style_text_font(route_lbl, UI_FONT_SM, 0);
            lv_obj_align(route_lbl, LV_ALIGN_RIGHT_MID, -8, 0);
            lv_obj_add_flag(route_lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(route_lbl, 6);
            lv_obj_add_event_cb(route_lbl, on_pmr_info, LV_EVENT_CLICKED, NULL);
        }
    }
}

/* --- Show overlay --- */

static void show_srv_overlay(void)
{
    if (s_srv_overlay) close_srv_overlay();

    s_srv_overlay = lv_obj_create(s_settings_scr);
    lv_obj_set_size(s_srv_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_srv_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_srv_overlay, lv_color_hex(0x000408), 0);
    lv_obj_set_style_bg_opa(s_srv_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_srv_overlay, 0, 0);
    lv_obj_set_style_radius(s_srv_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_srv_overlay, 0, 0);
    lv_obj_clear_flag(s_srv_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(s_srv_overlay);
    lv_label_set_text(title, "Server");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_set_pos(title, 10, 6);

    /* Active server name in title bar */
    smp_server_t *active = smp_servers_get_active();
    if (active) {
        lv_obj_t *active_lbl = lv_label_create(s_srv_overlay);
        lv_label_set_text(active_lbl, active->host);
        lv_obj_set_style_text_color(active_lbl, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(active_lbl, UI_FONT_SM, 0);
        lv_obj_align(active_lbl, LV_ALIGN_TOP_RIGHT, -10, 9);
    }

    /* Glow line */
    lv_obj_t *glow = lv_obj_create(s_srv_overlay);
    lv_obj_set_size(glow, UI_SCREEN_W, 1);
    lv_obj_set_pos(glow, 0, 25);
    lv_obj_set_style_bg_color(glow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(glow, (lv_opa_t)64, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_radius(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Scrollable content */
    s_srv_content = lv_obj_create(s_srv_overlay);
    lv_obj_set_size(s_srv_content, UI_SCREEN_W, UI_SCREEN_H - 52);
    lv_obj_set_pos(s_srv_content, 0, 26);
    lv_obj_set_style_bg_opa(s_srv_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_srv_content, 0, 0);
    lv_obj_set_style_pad_all(s_srv_content, 0, 0);
    lv_obj_set_style_pad_top(s_srv_content, 2, 0);
    lv_obj_set_style_pad_row(s_srv_content, 1, 0);
    lv_obj_set_style_radius(s_srv_content, 0, 0);
    lv_obj_set_flex_flow(s_srv_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_srv_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_srv_content, LV_SCROLLBAR_MODE_OFF);

    /* Back button */
    lv_obj_t *back = lv_label_create(s_srv_overlay);
    lv_label_set_text(back, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(back, UI_FONT, 0);
    lv_obj_set_pos(back, 10, UI_SCREEN_H - 22);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 10);
    lv_obj_add_event_cb(back, on_srv_back, LV_EVENT_CLICKED, NULL);

    srv_rebuild_list();
    ESP_LOGI("UI_INFO", "Server selection overlay opened");
}

static void on_server_row_click(lv_event_t *e)
{
    (void)e;
    show_srv_overlay();
}
/* ================================================================
 * Row Builders
 * ================================================================ */

/* Create a base row container */
static lv_obj_t *create_row_base(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_bg_color(row, ROW_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return row;
}

/* Create accent bar at given x position */
static void create_accent(lv_obj_t *row, int x, lv_color_t clr)
{
    lv_obj_t *a = lv_obj_create(row);
    lv_obj_set_size(a, ACCENT_W, ACCENT_H);
    lv_obj_set_pos(a, x, (ROW_H - ACCENT_H) / 2);
    lv_obj_set_style_bg_color(a, clr, 0);
    lv_obj_set_style_bg_opa(a, (lv_opa_t)180, 0);
    lv_obj_set_style_radius(a, 1, 0);
    lv_obj_set_style_border_width(a, 0, 0);
    lv_obj_set_style_pad_all(a, 0, 0);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

/* Create key label */
static void create_key(lv_obj_t *row, int x, const char *text)
{
    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, text);
    lv_obj_set_style_text_color(k, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, x, 0);
}

/* Create value label */
static lv_obj_t *create_val(lv_obj_t *row, int x, const char *text, lv_color_t clr)
{
    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, text);
    lv_obj_set_style_text_color(v, clr, 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
    lv_obj_align(v, LV_ALIGN_LEFT_MID, x, 0);
    return v;
}

/* Full dual-column row: left pair + right pair */
static void create_dual_row(lv_obj_t *parent,
                             const char *lkey, const char *lval, lv_color_t lclr,
                             const char *rkey, const char *rval, lv_color_t rclr,
                             lv_obj_t **lval_out, lv_obj_t **rval_out)
{
    lv_obj_t *row = create_row_base(parent);

    /* Left side */
    create_accent(row, 6, lclr);
    create_key(row, L_KEY_X, lkey);
    lv_obj_t *lv = create_val(row, L_VAL_X, lval, UI_COLOR_PRIMARY);
    if (lval_out) *lval_out = lv;

    /* Right side */
    create_accent(row, R_ACCENT_X, rclr);
    create_key(row, R_KEY_X, rkey);
    lv_obj_t *rv = create_val(row, R_VAL_X, rval, UI_COLOR_PRIMARY);
    if (rval_out) *rval_out = rv;
}

/* ================================================================
 * Screen Build
 * ================================================================ */

void settings_create_info(lv_obj_t *parent)
{
    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, UI_SCREEN_W, CONTENT_H);
    lv_obj_set_pos(s_list, 0, 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_top(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, ROW_GAP, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    /* === Row 0: Name (clickable) + PQ status === */
    {
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_size(row, LV_PCT(100), BRAND_H);
        lv_obj_set_style_bg_color(row, ROW_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x001420), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_name_row_click, LV_EVENT_CLICKED, NULL);

        char name[32];
        storage_get_display_name(name, sizeof(name));
        s_name_val = lv_label_create(row);
        lv_label_set_text(s_name_val, name);
        lv_obj_set_style_text_color(s_name_val, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(s_name_val, UI_FONT, 0);
        lv_obj_align(s_name_val, LV_ALIGN_LEFT_MID, 14, 0);

        lv_obj_t *edit = lv_label_create(row);
        lv_label_set_text(edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(edit, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(edit, UI_FONT_SM, 0);
        lv_obj_set_pos(edit, 120, (BRAND_H - 10) / 2);

        s_brand_pq = lv_label_create(row);
        lv_obj_set_style_text_font(s_brand_pq, UI_FONT_SM, 0);
        lv_obj_set_style_text_color(s_brand_pq, UI_COLOR_PRIMARY, 0);
        lv_obj_align(s_brand_pq, LV_ALIGN_RIGHT_MID, -20, 0);
        lv_obj_add_flag(s_brand_pq, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_brand_pq, 10);
        lv_obj_add_event_cb(s_brand_pq, on_pq_toggle, LV_EVENT_CLICKED, NULL);

        /* Edit icon - same style as Name edit (dim, small) */
        lv_obj_t *pq_edit = lv_label_create(row);
        lv_label_set_text(pq_edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(pq_edit, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(pq_edit, UI_FONT_SM, 0);
        lv_obj_align(pq_edit, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_flag(pq_edit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(pq_edit, 10);
        lv_obj_add_event_cb(pq_edit, on_pq_toggle, LV_EVENT_CLICKED, NULL);
        /* Color and text set in refresh_values() */
    }

    /* === Row 1: Version | Hardware === */
    create_dual_row(s_list,
        "Version", "0.1.17-a", UI_COLOR_PRIMARY,
        "Hardware", "ESP32-S3", UI_COLOR_PRIMARY,
        NULL, NULL);

    /* === Row 2: Build | NVS Mode === */
    {
        bool vault =
#if defined(CONFIG_NVS_ENCRYPTION)
            true;
#else
            false;
#endif

        lv_obj_t *row = create_row_base(s_list);
        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "Build");
        create_val(row, L_VAL_X, "IDF 5.5.2", UI_COLOR_PRIMARY);
        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "NVS");
        create_val(row, R_VAL_X, vault ? "VAULT" : "OPEN",
                   vault ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM);
    }

    /* === Row 3: eFuse Status | License === */
    {
        bool burned = is_efuse_burned();

        lv_obj_t *row = create_row_base(s_list);

        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "eFuse");
        create_val(row, L_VAL_X, burned ? "BURNED" : "NONE",
                   burned ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM);

        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "License");
        create_val(row, R_VAL_X, "AGPL-3.0", UI_COLOR_TEXT_DIM);
    }

    /* === Row 4: WiFi Signal | Heap === */
    create_dual_row(s_list,
        "WiFi", "...", UI_COLOR_PRIMARY,
        "Heap", "...", UI_COLOR_PRIMARY,
        &s_wifi_val, &s_heap_val);

    /* === Row 5: PSRAM | LVGL === */
    create_dual_row(s_list,
        "PSRAM", "...", UI_COLOR_PRIMARY,
        "LVGL", "...", UI_COLOR_PRIMARY,
        &s_psram_val, &s_lvgl_val);

    /* === Row 6: Server (clickable) | Time [-] UTC+1 [+] === */
    {
        lv_obj_t *row = create_row_base(s_list);

        /* Left: Server (clickable to open server settings overlay) */
        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "Server");
        lv_obj_t *srv_val = create_val(row, L_VAL_X, "Manage", UI_COLOR_PRIMARY);
        lv_obj_add_flag(srv_val, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(srv_val, 10);
        lv_obj_add_event_cb(srv_val, on_server_row_click, LV_EVENT_CLICKED, NULL);

        /* Right: Time with [-] value [+] */
        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "Time");

        /* Minus button (14x14 square) */
        lv_obj_t *btn_m = lv_btn_create(row);
        lv_obj_set_size(btn_m, 14, 14);
        lv_obj_align(btn_m, LV_ALIGN_LEFT_MID, R_VAL_X, 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn_m, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn_m, 1, 0);
        lv_obj_set_style_border_color(btn_m, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_radius(btn_m, 1, 0);
        lv_obj_set_style_shadow_width(btn_m, 0, 0);
        lv_obj_set_style_pad_all(btn_m, 0, 0);
        lv_obj_t *ml = lv_label_create(btn_m);
        lv_label_set_text(ml, "-");
        lv_obj_set_style_text_color(ml, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(ml, UI_FONT_SM, 0);
        lv_obj_center(ml);
        lv_obj_add_event_cb(btn_m, on_tz_minus, LV_EVENT_CLICKED, NULL);

        /* UTC offset value */
        s_tz_val = lv_label_create(row);
        char tz_buf[12];
        snprintf(tz_buf, sizeof(tz_buf), "UTC%+d", g_tz_offset_hours);
        lv_label_set_text(s_tz_val, tz_buf);
        lv_obj_set_style_text_color(s_tz_val, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(s_tz_val, UI_FONT_SM, 0);
        lv_obj_align(s_tz_val, LV_ALIGN_LEFT_MID, R_VAL_X + 18, 0);

        /* Plus button (14x14 square) */
        lv_obj_t *btn_p = lv_btn_create(row);
        lv_obj_set_size(btn_p, 14, 14);
        lv_obj_align(btn_p, LV_ALIGN_LEFT_MID, R_VAL_X + 58, 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn_p, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn_p, 1, 0);
        lv_obj_set_style_border_color(btn_p, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_radius(btn_p, 1, 0);
        lv_obj_set_style_shadow_width(btn_p, 0, 0);
        lv_obj_set_style_pad_all(btn_p, 0, 0);
        lv_obj_t *pl = lv_label_create(btn_p);
        lv_label_set_text(pl, "+");
        lv_obj_set_style_text_color(pl, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(pl, UI_FONT_SM, 0);
        lv_obj_center(pl);
        lv_obj_add_event_cb(btn_p, on_tz_plus, LV_EVENT_CLICKED, NULL);
    }

    /* === Row 7: Lock Sim|Mtx | Timeout [-] 60s [+] === */
    {
        uint8_t cur_mode = get_lock_mode();
        uint8_t cur_idx = ui_manager_get_lock_timer_idx();

        lv_obj_t *row = create_row_base(s_list);

        /* Left: Lock mode toggle */
        create_accent(row, 6, UI_COLOR_PRIMARY);
        create_key(row, L_KEY_X, "Lock");

        s_lock_simple = lv_label_create(row);
        lv_label_set_text(s_lock_simple, "Sim");
        lv_obj_set_style_text_color(s_lock_simple,
            cur_mode == 0 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(s_lock_simple, UI_FONT_SM, 0);
        lv_obj_align(s_lock_simple, LV_ALIGN_LEFT_MID, L_VAL_X, 0);
        lv_obj_add_flag(s_lock_simple, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_lock_simple, 6);
        lv_obj_add_event_cb(s_lock_simple, on_lock_simple, LV_EVENT_CLICKED, NULL);

        lv_obj_t *sep = lv_label_create(row);
        lv_label_set_text(sep, "|");
        lv_obj_set_style_text_color(sep, UI_COLOR_LINE_DIM, 0);
        lv_obj_set_style_text_font(sep, UI_FONT_SM, 0);
        lv_obj_align(sep, LV_ALIGN_LEFT_MID, L_VAL_X + 22, 0);

        s_lock_matrix = lv_label_create(row);
        lv_label_set_text(s_lock_matrix, "Mtx");
        lv_obj_set_style_text_color(s_lock_matrix,
            cur_mode == 1 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(s_lock_matrix, UI_FONT_SM, 0);
        lv_obj_align(s_lock_matrix, LV_ALIGN_LEFT_MID, L_VAL_X + 30, 0);
        lv_obj_add_flag(s_lock_matrix, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(s_lock_matrix, 6);
        lv_obj_add_event_cb(s_lock_matrix, on_lock_matrix, LV_EVENT_CLICKED, NULL);

        /* Right: Timeout with [-] value [+] */
        create_accent(row, R_ACCENT_X, UI_COLOR_PRIMARY);
        create_key(row, R_KEY_X, "Timer");

        lv_obj_t *btn_m = lv_btn_create(row);
        lv_obj_set_size(btn_m, 14, 14);
        lv_obj_align(btn_m, LV_ALIGN_LEFT_MID, R_VAL_X, 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn_m, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn_m, 1, 0);
        lv_obj_set_style_border_color(btn_m, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_radius(btn_m, 1, 0);
        lv_obj_set_style_shadow_width(btn_m, 0, 0);
        lv_obj_set_style_pad_all(btn_m, 0, 0);
        lv_obj_t *ml = lv_label_create(btn_m);
        lv_label_set_text(ml, "-");
        lv_obj_set_style_text_color(ml, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(ml, UI_FONT_SM, 0);
        lv_obj_center(ml);
        lv_obj_add_event_cb(btn_m, on_timer_minus, LV_EVENT_CLICKED, NULL);

        s_lock_timer_val = lv_label_create(row);
        lv_label_set_text(s_lock_timer_val,
            ui_manager_get_lock_timer_label(cur_idx));
        lv_obj_set_style_text_color(s_lock_timer_val, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(s_lock_timer_val, UI_FONT_SM, 0);
        lv_obj_align(s_lock_timer_val, LV_ALIGN_LEFT_MID, R_VAL_X + 18, 0);

        lv_obj_t *btn_p = lv_btn_create(row);
        lv_obj_set_size(btn_p, 14, 14);
        lv_obj_align(btn_p, LV_ALIGN_LEFT_MID, R_VAL_X + 58, 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn_p, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn_p, 1, 0);
        lv_obj_set_style_border_color(btn_p, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_radius(btn_p, 1, 0);
        lv_obj_set_style_shadow_width(btn_p, 0, 0);
        lv_obj_set_style_pad_all(btn_p, 0, 0);
        lv_obj_t *pl = lv_label_create(btn_p);
        lv_label_set_text(pl, "+");
        lv_obj_set_style_text_color(pl, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(pl, UI_FONT_SM, 0);
        lv_obj_center(pl);
        lv_obj_add_event_cb(btn_p, on_timer_plus, LV_EVENT_CLICKED, NULL);
    }

    /* Initial refresh + timer */
    refresh_values();
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 2000, NULL);
}

void settings_cleanup_info_timers(void)
{
    if (s_refresh_timer) { lv_timer_del(s_refresh_timer); s_refresh_timer = NULL; }
    close_name_overlay();
    close_srv_overlay();
}


void settings_nullify_info_pointers(void)
{
    s_list = NULL;
    s_heap_val = s_psram_val = s_lvgl_val = s_wifi_val = s_name_val = NULL;
    s_brand_pq = NULL;
    s_tz_val = NULL;
    s_lock_simple = NULL;
    s_lock_matrix = NULL;
    s_lock_timer_val = NULL;
    s_srv_overlay = NULL;
    s_srv_content = NULL;
    s_srv_popup = NULL;
    s_pending_srv_idx = -1;
}

void settings_info_refresh(void) { refresh_values(); }
