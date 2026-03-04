/**
 * @file ui_manager.c
 * @brief Screen Manager - Manual Fade Control
 *
 * Session 39k: First-boot WiFi detection via wifi_manager_needs_setup().
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
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "UI_MGR";

static lv_obj_t *screens[UI_SCREEN_COUNT] = {NULL};
static ui_screen_t current_screen = UI_SCREEN_SPLASH;

// Session 33: Navigation stack (replaces single prev_screen)
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

esp_err_t ui_manager_init(void)
{
    ESP_LOGI(TAG, "Init...");
    ui_theme_init();
    
    screens[UI_SCREEN_SPLASH] = ui_splash_create();
    lv_scr_load(screens[UI_SCREEN_SPLASH]);
    
    current_screen = UI_SCREEN_SPLASH;
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
    
    // Session 33: Push current to stack before navigating
    nav_stack_push(current_screen);
    ui_screen_t prev = current_screen;
    current_screen = screen;
    
    // Immer direkt laden - Animationen machen die Screens selbst
    lv_scr_load(screens[screen]);

    /* 42f: Delete previous screen to free LVGL pool (MAIN stays permanent) */
    if (prev != UI_SCREEN_SPLASH && prev != UI_SCREEN_MAIN) {
        if (screens[prev] != NULL) {
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
    
    // Alten Splash loeschen
    if (prev == UI_SCREEN_SPLASH && screens[UI_SCREEN_SPLASH]) {
        lv_obj_del(screens[UI_SCREEN_SPLASH]);
        screens[UI_SCREEN_SPLASH] = NULL;
        // Remove splash from stack too
        if (nav_stack_top >= 0 && nav_stack[nav_stack_top] == UI_SCREEN_SPLASH) {
            nav_stack_top--;
        }

        /* Session 39k: First-boot WiFi detection.
         * Uses wifi_manager_needs_setup() which checks NVS namespace
         * "wifi_cfg" key "ssid" AND Kconfig fallback. If neither has
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
}

void ui_manager_go_back(void)
{
    // Session 33: Pop from navigation stack
    ui_screen_t target = nav_stack_pop();
    
    ESP_LOGI(TAG, "<- back to screen %d (stack depth: %d)", target, nav_stack_top + 1);
    
    if (target == current_screen) {
        target = UI_SCREEN_MAIN;
    }
    
    if (!screens[target]) {
        screens[target] = screen_creators[target]();
    }
    
    ui_screen_t old = current_screen;  /* 42f: save before overwrite */
    current_screen = target;

    lv_scr_load(screens[target]);

    /* 42f: Delete old screen to free LVGL pool (MAIN stays permanent) */
    if (old != UI_SCREEN_SPLASH && old != UI_SCREEN_MAIN) {
        if (screens[old] != NULL) {
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
    // NOTE: go_back does NOT push to stack
}

ui_screen_t ui_manager_get_current(void)
{
    return current_screen;
}
