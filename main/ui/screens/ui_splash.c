/**
 * @file ui_splash.c
 * @brief Splash - Simple, No Animation
 */

#include "ui_splash.h"
#include "lvgl.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"

static const char *TAG = "UI_SPLASH";
static lv_obj_t *screen = NULL;

static void go_to_main(lv_timer_t *t)
{
    lv_timer_del(t);
    ui_manager_show_screen(UI_SCREEN_MAIN, LV_SCR_LOAD_ANIM_NONE);
}

lv_obj_t *ui_splash_create(void)
{
    ESP_LOGI(TAG, "Creating splash...");
    
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Logo
    lv_obj_t *logo = lv_label_create(screen);
    lv_label_set_text(logo, "SimpleGo");
    lv_obj_set_style_text_color(logo, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_28, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -13);
    
    // Tagline
    lv_obj_t *tagline = lv_label_create(screen);
    lv_label_set_text(tagline, "private by design");
    lv_obj_set_style_text_color(tagline, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 18);
    
    // Version
    lv_obj_t *ver = lv_label_create(screen);
    lv_label_set_text(ver, UI_VERSION);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x080808), 0);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_RIGHT, -3, -3);
    
    // Nach 2s zu Main
    lv_timer_create(go_to_main, 2000, NULL);
    
    return screen;
}

void ui_splash_set_status(const char *s) {}
void ui_splash_set_progress(int p) {}
