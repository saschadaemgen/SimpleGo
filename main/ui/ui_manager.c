/**
 * SimpleGo - ui_manager.c
 * Screen manager with navigation stack
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_splash.h"
#include "ui_main.h"
#include "ui_chat.h"
#include "ui_contacts.h"
#include "ui_connect.h"
#include "ui_settings.h"
#include "ui_developer.h"
#include "ui_name_setup.h"
#include "ui_lock.h"
#include "wifi_manager.h"
#include "smp_storage.h"
#include "esp_log.h"

/* Cleanup functions called before screen deletion.
 * TODO: move to ui_chat.h */
extern void ui_chat_cleanup(void);
extern void ui_connect_cleanup(void);
extern void ui_lock_cleanup(void);

static const char *TAG = "UI_MGR";

static lv_obj_t *screens[UI_SCREEN_COUNT] = {NULL};
static ui_screen_t current_screen = UI_SCREEN_SPLASH;
static bool s_name_check_done = false;  /* Session 43: one-shot first-boot name check */

/* SEC-04: Inactivity lock timer */
#define LOCK_TIMEOUT_MS      60000   /* 60 seconds inactivity to lock */
#define LOCK_CHECK_INTERVAL  2000    /* Check every 2 seconds */
static lv_timer_t *s_lock_timer = NULL;

// Navigation stack (replaces single prev_screen)
#define NAV_STACK_DEPTH 8
static ui_screen_t nav_stack[NAV_STACK_DEPTH];
static int nav_stack_top = -1;  // -1 = empty

typedef lv_obj_t *(*screen_create_fn)(void);

static const screen_create_fn screen_creators[UI_SCREEN_COUNT] = {
    [UI_SCREEN_SPLASH]    = ui_splash_create,
    [UI_SCREEN_MAIN]      = ui_main_create,
    [UI_SCREEN_CHAT]      = ui_chat_create,
    [UI_SCREEN_CONTACTS]  = ui_contacts_create,
    [UI_SCREEN_CONNECT]   = ui_connect_create,
    [UI_SCREEN_SETTINGS]  = ui_settings_create,
    [UI_SCREEN_DEVELOPER] = ui_developer_create,
    [UI_SCREEN_NAME_SETUP] = ui_name_setup_create,
    [UI_SCREEN_LOCK]       = ui_lock_create,
};

static void nav_stack_push(ui_screen_t screen)
{
    if (screen == UI_SCREEN_SPLASH) return;
    if (nav_stack_top >= 0 && nav_stack[nav_stack_top] == screen) return;
    if (nav_stack_top < NAV_STACK_DEPTH - 1) {
        nav_stack[++nav_stack_top] = screen;
    } else {
        for (int i = 0; i < NAV_STACK_DEPTH - 1; i++) {
            nav_stack[i] = nav_stack[i + 1];
        }
        nav_stack[nav_stack_top] = screen;
    }
}

static ui_screen_t nav_stack_pop(void)
{
    if (nav_stack_top >= 0) {
        return nav_stack[nav_stack_top--];
    }
    return UI_SCREEN_MAIN;
}

/* SEC-04: Inactivity timer callback.
 * Checks lv_disp_get_inactive_time() every LOCK_CHECK_INTERVAL ms.
 * If no input for LOCK_TIMEOUT_MS, locks the device. */
static void lock_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Don't lock if already locked or still on splash */
    if (current_screen == UI_SCREEN_LOCK) return;
    if (current_screen == UI_SCREEN_SPLASH) return;

    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) return;

    uint32_t inactive = lv_disp_get_inactive_time(disp);
    if (inactive >= LOCK_TIMEOUT_MS) {
        ESP_LOGI(TAG, "SEC-04: Inactivity %u ms >= %u ms, locking",
                 (unsigned)inactive, LOCK_TIMEOUT_MS);
        ui_manager_lock();
    }
}

esp_err_t ui_manager_init(void)
{
    ESP_LOGI(TAG, "Init...");
    ui_theme_init();
    
    screens[UI_SCREEN_SPLASH] = ui_splash_create();
    lv_scr_load(screens[UI_SCREEN_SPLASH]);
    
    current_screen = UI_SCREEN_SPLASH;

    /* SEC-04: Start inactivity monitoring for auto-lock */
    s_lock_timer = lv_timer_create(lock_timer_cb, LOCK_CHECK_INTERVAL, NULL);
    ESP_LOGI(TAG, "SEC-04: Lock timer started (%d ms timeout, %d ms check)",
             LOCK_TIMEOUT_MS, LOCK_CHECK_INTERVAL);

    return ESP_OK;
}

void ui_manager_show_screen(ui_screen_t screen, lv_scr_load_anim_t anim)
{
    if (screen >= UI_SCREEN_COUNT) return;
    if (screen == current_screen) return;
    
    ESP_LOGI(TAG, "-> screen %d (stack depth: %d)", screen, nav_stack_top + 1);
    
    if (!screens[screen]) {
        screens[screen] = screen_creators[screen]();
    }
    
    // Push current to stack before navigating
    nav_stack_push(current_screen);
    ui_screen_t prev = current_screen;
    current_screen = screen;
    
    // Always load directly - screens handle their own animations
    lv_scr_load(screens[screen]);

    /* Delete previous screen to free LVGL pool (MAIN stays permanent) */
    if (prev != UI_SCREEN_SPLASH && prev != UI_SCREEN_MAIN) {
        if (screens[prev] != NULL) {
            /* Null dangling pointers BEFORE destroying LVGL objects */
            if (prev == UI_SCREEN_CHAT) ui_chat_cleanup();
            if (prev == UI_SCREEN_CONNECT) ui_connect_cleanup();
            if (prev == UI_SCREEN_LOCK) ui_lock_cleanup();
            lv_obj_del(screens[prev]);
            screens[prev] = NULL;
            ESP_LOGI(TAG, "Screen %d deleted from pool", prev);
        }
    }
    
    // Refresh contacts list when navigating to it
    if (screen == UI_SCREEN_CONTACTS) {
        ui_contacts_refresh();
    }

    // Refresh main screen unread list when navigating to it
    if (screen == UI_SCREEN_MAIN) {
        ui_main_refresh();
    }
    
    // Delete splash screen after first navigation
    if (prev == UI_SCREEN_SPLASH && screens[UI_SCREEN_SPLASH]) {
        lv_obj_del(screens[UI_SCREEN_SPLASH]);
        screens[UI_SCREEN_SPLASH] = NULL;
        // Remove splash from stack too
        if (nav_stack_top >= 0 && nav_stack[nav_stack_top] == UI_SCREEN_SPLASH) {
            nav_stack_top--;
        }

        /* First-boot WiFi detection.
         * Checks NVS "wifi_cfg"/"ssid" AND Kconfig fallback. If neither has
         * credentials, redirect to Settings WiFi tab. */
        if (screen == UI_SCREEN_MAIN && wifi_manager_needs_setup()) {
            ESP_LOGI(TAG, "No WiFi credentials -> WiFi setup");
            if (!screens[UI_SCREEN_SETTINGS]) {
                screens[UI_SCREEN_SETTINGS] = screen_creators[UI_SCREEN_SETTINGS]();
            }
            nav_stack_push(current_screen);
            current_screen = UI_SCREEN_SETTINGS;
            lv_scr_load(screens[UI_SCREEN_SETTINGS]);
            ui_settings_show_wifi_tab();
        }
    }

    /* Session 43: First-boot name check (one-shot, catches ALL paths to MAIN).
     * Triggers after splash, after WiFi setup, after any redirect -- whenever
     * we land on MAIN without a display name configured. The flag prevents
     * infinite loops (Skip button navigates to MAIN without saving). */
    if (screen == UI_SCREEN_MAIN && !s_name_check_done && !storage_has_display_name()) {
        s_name_check_done = true;
        ESP_LOGI(TAG, "No display name -> name setup");
        if (!screens[UI_SCREEN_NAME_SETUP]) {
            screens[UI_SCREEN_NAME_SETUP] = screen_creators[UI_SCREEN_NAME_SETUP]();
        }
        nav_stack_push(current_screen);
        current_screen = UI_SCREEN_NAME_SETUP;
        lv_scr_load(screens[UI_SCREEN_NAME_SETUP]);
    }
}

void ui_manager_go_back(void)
{
    // Pop from navigation stack
    ui_screen_t target = nav_stack_pop();
    
    ESP_LOGI(TAG, "<- back to screen %d (stack depth: %d)", target, nav_stack_top + 1);
    
    if (target == current_screen) {
        target = UI_SCREEN_MAIN;
    }
    
    if (!screens[target]) {
        screens[target] = screen_creators[target]();
    }
    
    ui_screen_t old = current_screen;  /* save before overwrite */
    current_screen = target;

    lv_scr_load(screens[target]);

    /* Delete old screen to free LVGL pool (MAIN stays permanent) */
    if (old != UI_SCREEN_SPLASH && old != UI_SCREEN_MAIN) {
        if (screens[old] != NULL) {
            /* Null dangling pointers BEFORE destroying LVGL objects */
            if (old == UI_SCREEN_CHAT) ui_chat_cleanup();
            if (old == UI_SCREEN_CONNECT) ui_connect_cleanup();
            if (old == UI_SCREEN_LOCK) ui_lock_cleanup();
            lv_obj_del(screens[old]);
            screens[old] = NULL;
            ESP_LOGI(TAG, "Screen %d deleted from pool", old);
        }
    }
    
    // Refresh contacts list when navigating back to it
    if (target == UI_SCREEN_CONTACTS) {
        ui_contacts_refresh();
    }

    // Refresh main screen unread list when navigating back to it
    if (target == UI_SCREEN_MAIN) {
        ui_main_refresh();
    }

    /* Session 43: First-boot name check (same as in show_screen) */
    if (target == UI_SCREEN_MAIN && !s_name_check_done && !storage_has_display_name()) {
        s_name_check_done = true;
        ESP_LOGI(TAG, "No display name -> name setup");
        if (!screens[UI_SCREEN_NAME_SETUP]) {
            screens[UI_SCREEN_NAME_SETUP] = screen_creators[UI_SCREEN_NAME_SETUP]();
        }
        nav_stack_push(current_screen);
        current_screen = UI_SCREEN_NAME_SETUP;
        lv_scr_load(screens[UI_SCREEN_NAME_SETUP]);
    }
    // NOTE: go_back does NOT push to stack
}

ui_screen_t ui_manager_get_current(void)
{
    return current_screen;
}

/* ============== SEC-04: Screen Lock ============== */

void ui_manager_lock(void)
{
    if (current_screen == UI_SCREEN_LOCK) return;
    if (current_screen == UI_SCREEN_SPLASH) return;

    ESP_LOGI(TAG, "SEC-04: Locking device (from screen %d)", current_screen);

    /* Force-wipe decrypted message data regardless of current screen.
     * If chat is active, this wipes labels + cache before navigation
     * destroys the screen. If chat is not active, the cache was already
     * wiped during previous navigation cleanup - this is a no-op. */
    ui_chat_secure_wipe();

    /* Navigate to lock screen. Normal cleanup path handles the rest:
     * if current is CHAT, show_screen calls ui_chat_cleanup() which
     * does a second wipe (harmless, sodium_memzero is idempotent). */
    ui_manager_show_screen(UI_SCREEN_LOCK, LV_SCR_LOAD_ANIM_NONE);
}

void ui_manager_unlock(void)
{
    if (current_screen != UI_SCREEN_LOCK) return;

    ESP_LOGI(TAG, "SEC-04: Unlocking device");

    /* Go back to previous screen. If the previous screen was CHAT,
     * it was destroyed on lock. go_back will pop the stack and land
     * on MAIN or CONTACTS. User re-enters chat normally, which triggers
     * fresh SD history load. */
    ui_manager_go_back();
}
