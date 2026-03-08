/**
 * SimpleGo - ui_connect.c
 * Connect screen with QR code display
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_connect.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_CONNECT";

static lv_obj_t *screen = NULL;
static lv_obj_t *qr_code = NULL;
static lv_obj_t *placeholder = NULL;
static lv_obj_t *status_lbl = NULL;

static void on_back(lv_event_t *e) { ui_manager_go_back(); }

lv_obj_t *ui_connect_create(void)
{
    ESP_LOGI(TAG, "Creating connect screen...");

    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    // Session 33 Phase 4A: Centered title instead of left-aligned header
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Scan with SimpleGo\nor SimpleX App");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    // QR Code - Session 33 Phase 4A: Standard BLACK on WHITE (ISO 18004)
    // Inverted QR codes are NOT readable by SimpleX App scanner!
    qr_code = lv_qrcode_create(screen);
    lv_qrcode_set_size(qr_code, 140);
    lv_qrcode_set_dark_color(qr_code, lv_color_black());
    lv_qrcode_set_light_color(qr_code, lv_color_white());
    lv_qrcode_set_quiet_zone(qr_code, false);
    lv_obj_set_style_pad_all(qr_code, 0, 0);
    lv_obj_set_style_border_width(qr_code, 0, 0);
    lv_obj_set_style_outline_width(qr_code, 0, 0);
    lv_obj_align(qr_code, LV_ALIGN_CENTER, 0, 6);
    lv_obj_add_flag(qr_code, LV_OBJ_FLAG_HIDDEN);

    // Placeholder (hidden when QR is shown)
    placeholder = lv_label_create(screen);
    lv_label_set_text(placeholder, "Generating...");
    lv_obj_set_style_text_color(placeholder, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, 0);

    // Status Label unter QR
    status_lbl = lv_label_create(screen);
    lv_label_set_text(status_lbl, "");
    lv_obj_set_style_text_color(status_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_align(status_lbl, LV_ALIGN_BOTTOM_RIGHT, -5, -8);

    // Nav Bar
    ui_create_nav_bar(screen);
    lv_obj_t *back = ui_create_nav_btn(screen, "BACK", 0);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);

    return screen;
}

void ui_connect_set_invite_link(const char *link) {
    if (!link || !qr_code) return;

    ESP_LOGI(TAG, "Setting QR code for link (%d chars)", strlen(link));

    lv_result_t res = lv_qrcode_update(qr_code, link, strlen(link));
    
    if (res == LV_RESULT_OK) {
        ESP_LOGI(TAG, "QR code updated successfully");
        lv_obj_clear_flag(qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(status_lbl, "Waiting for connection...");
    } else {
        ESP_LOGE(TAG, "QR code update failed: %d", res);
        lv_label_set_text(placeholder, "QR Error!");
        lv_label_set_text(status_lbl, "Link too long?");
    }
}

// Session 33 Phase 4A: Update status text on connect screen
void ui_connect_set_status(const char *text) {
    if (status_lbl && text) {
        lv_label_set_text(status_lbl, text);
    }
}

// 36d: Reset QR code and status for clean state
void ui_connect_reset(void)
{
    /* Guard: if screen was deleted, pointers are dangling */
    if (!screen) return;

    if (qr_code) {
        lv_obj_add_flag(qr_code, LV_OBJ_FLAG_HIDDEN);
    }
    if (placeholder) {
        lv_obj_clear_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_lbl) {
        lv_label_set_text(status_lbl, "Generating...");
    }
    ESP_LOGI("UI_CONN", "36d: QR reset");
}

// Session 43: Null all static pointers before screen deletion
void ui_connect_cleanup(void)
{
    screen = NULL;
    qr_code = NULL;
    placeholder = NULL;
    status_lbl = NULL;
}
