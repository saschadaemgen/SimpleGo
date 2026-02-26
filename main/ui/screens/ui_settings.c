/**
 * @file ui_settings.c
 */
#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_system.h"

static lv_obj_t *screen = NULL;

static void on_back(lv_event_t *e) { ui_manager_go_back(); }
static void on_reboot(lv_event_t *e) { esp_restart(); }

lv_obj_t *ui_settings_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "System", NULL);
    
    const char *info[] = {"DEVICE", "T-Deck Plus", "CRYPTO", "X448+AES", "PROTO", "SMP 1.0"};
    for (int i = 0; i < 6; i += 2) {
        lv_obj_t *l = lv_label_create(screen);
        lv_label_set_text(l, info[i]);
        lv_obj_set_style_text_color(l, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(l, 10, 25 + (i/2)*20);
        lv_obj_t *v = lv_label_create(screen);
        lv_label_set_text(v, info[i+1]);
        lv_obj_set_style_text_color(v, UI_COLOR_PRIMARY, 0);
        lv_obj_set_pos(v, 100, 25 + (i/2)*20);
    }
    
    ui_create_line(screen, 90);
    
    lv_obj_t *reboot = ui_create_btn(screen, "REBOOT", 110, 105, 100);
    lv_obj_set_style_border_color(reboot, UI_COLOR_ERROR, 0);
    lv_obj_t *lbl = lv_obj_get_child(reboot, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ERROR, 0);
    lv_obj_add_event_cb(reboot, on_reboot, LV_EVENT_CLICKED, NULL);
    
    // Nav Bar - wie Main Screen
    ui_create_nav_bar(screen);
    lv_obj_t *back = ui_create_nav_btn(screen, "BACK", 0);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    
    return screen;
}
