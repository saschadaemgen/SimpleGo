/**
 * @file ui_main.c
 * @brief Main Screen - Simple
 */
#include "ui_main.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"

static const char *TAG = "UI_MAIN";
static lv_obj_t *screen = NULL;

static void on_chats(lv_event_t *e) { ui_manager_show_screen(UI_SCREEN_CONTACTS, LV_SCR_LOAD_ANIM_NONE); }
static void on_new(lv_event_t *e) { ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE); }
static void on_dev(lv_event_t *e) { ui_manager_show_screen(UI_SCREEN_DEVELOPER, LV_SCR_LOAD_ANIM_NONE); }
static void on_sys(lv_event_t *e) { ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE); }

lv_obj_t *ui_main_create(void)
{
    ESP_LOGI(TAG, "Creating main...");
    
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    
    // Header
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "SimpleGo");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(title, 0, 2);
    
    lv_obj_t *ver = lv_label_create(screen);
    lv_label_set_text(ver, UI_VERSION);
    lv_obj_set_style_text_color(ver, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(ver, LV_ALIGN_TOP_RIGHT, -2, 2);
    
    ui_create_line(screen, UI_HEADER_H);
    
    // Content
    lv_obj_t *msg = lv_label_create(screen);
    lv_label_set_text(msg, "NO ACTIVE CHATS");
    lv_obj_set_style_text_color(msg, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "[ NEW ] to connect");
    lv_obj_set_style_text_color(hint, UI_COLOR_PRIMARY, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 10);
    
    // Nav bar
    ui_create_nav_bar(screen);
    
    lv_obj_t *b1 = ui_create_nav_btn(screen, "CHATS", 0);
    lv_obj_t *b2 = ui_create_nav_btn(screen, "NEW", 1);
    lv_obj_t *b3 = ui_create_nav_btn(screen, "DEV", 2);
    lv_obj_t *b4 = ui_create_nav_btn(screen, "SYS", 3);
    
    lv_obj_add_event_cb(b1, on_chats, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b2, on_new, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b3, on_dev, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(b4, on_sys, LV_EVENT_CLICKED, NULL);
    
    return screen;
}

void ui_main_refresh(void) {}
