/**
 * @file ui_developer.c
 * @brief Developer Screen - SimpleX Console
 */
#include "ui_developer.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "UI_DEV";

static lv_obj_t *screen = NULL;
static lv_obj_t *content_area = NULL;
static lv_obj_t *stats_container = NULL;
static lv_obj_t *console_container = NULL;
static lv_obj_t *info_container = NULL;

static lv_obj_t *heap_val, *heap_dma_val, *heap_psram_val;
static lv_obj_t *uptime_val, *wifi_rssi_val, *wifi_ch_val;
static lv_obj_t *console_ta = NULL;
static lv_obj_t *pause_lbl = NULL;
static lv_obj_t *sep_lbl = NULL;
static lv_obj_t *reset_lbl = NULL;
static bool log_enabled = true;

static lv_timer_t *update_tmr = NULL;
static int current_tab = 0;

static lv_obj_t *btn_back, *btn_stats, *btn_console, *btn_info;

static void show_tab(int tab);
static void on_back(lv_event_t *e) { ui_manager_go_back(); }
static void on_tab_stats(lv_event_t *e) { show_tab(0); }
static void on_tab_console(lv_event_t *e) { show_tab(1); }
static void on_tab_info(lv_event_t *e) { show_tab(2); }

static void on_pause(lv_event_t *e) {
    log_enabled = !log_enabled;
    lv_label_set_text(pause_lbl, log_enabled ? "PAUSE" : "RUN");
}

static void on_reset(lv_event_t *e) {
    if (console_ta) {
        lv_textarea_set_text(console_ta, "");
        ui_developer_log("Console cleared");
    }
}

static void show_tab(int tab) {
    current_tab = tab;
    
    lv_obj_set_style_text_color(lv_obj_get_child(btn_stats, 0), tab == 0 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_console, 0), tab == 1 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_info, 0), tab == 2 ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
    
    if (stats_container) lv_obj_add_flag(stats_container, LV_OBJ_FLAG_HIDDEN);
    if (console_container) lv_obj_add_flag(console_container, LV_OBJ_FLAG_HIDDEN);
    if (info_container) lv_obj_add_flag(info_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *containers[] = {stats_container, console_container, info_container};
    if (containers[tab]) lv_obj_clear_flag(containers[tab], LV_OBJ_FLAG_HIDDEN);
    
    // Header Labels nur bei Console
    if (tab == 1) {
        lv_obj_clear_flag(pause_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(sep_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(pause_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(sep_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_stat_row(lv_obj_t *parent, const char *label, int y) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lbl, 8, y);
    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(val, 140, y);
    return val;
}

static void create_stats_tab(void) {
    stats_container = lv_obj_create(content_area);
    lv_obj_set_size(stats_container, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(stats_container, 0, 0);
    lv_obj_set_style_bg_opa(stats_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_container, 0, 0);
    lv_obj_set_style_pad_all(stats_container, 0, 0);
    lv_obj_set_scrollbar_mode(stats_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(stats_container, UI_COLOR_PRIMARY, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(stats_container, LV_OPA_40, LV_PART_SCROLLBAR);
    
    int y = 4, rh = 22;
    
    lv_obj_t *h1 = lv_label_create(stats_container);
    lv_label_set_text(h1, "[ MEMORY ]");
    lv_obj_set_style_text_color(h1, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(h1, 4, y); y += rh;
    
    heap_val = create_stat_row(stats_container, "Heap Free:", y); y += rh;
    heap_dma_val = create_stat_row(stats_container, "DMA Free:", y); y += rh;
    heap_psram_val = create_stat_row(stats_container, "PSRAM Free:", y); y += rh + 6;
    
    lv_obj_t *h2 = lv_label_create(stats_container);
    lv_label_set_text(h2, "[ SYSTEM ]");
    lv_obj_set_style_text_color(h2, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(h2, 4, y); y += rh;
    
    uptime_val = create_stat_row(stats_container, "Uptime:", y); y += rh + 6;
    
    lv_obj_t *h3 = lv_label_create(stats_container);
    lv_label_set_text(h3, "[ WIFI ]");
    lv_obj_set_style_text_color(h3, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(h3, 4, y); y += rh;
    
    wifi_rssi_val = create_stat_row(stats_container, "Signal:", y); y += rh;
    wifi_ch_val = create_stat_row(stats_container, "Channel:", y);
}

static void create_console_tab(void) {
    console_container = lv_obj_create(content_area);
    lv_obj_set_size(console_container, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(console_container, 0, 0);
    lv_obj_set_style_bg_opa(console_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(console_container, 0, 0);
    lv_obj_set_style_pad_all(console_container, 0, 0);
    lv_obj_clear_flag(console_container, LV_OBJ_FLAG_SCROLLABLE);
    
    console_ta = lv_textarea_create(console_container);
    lv_obj_set_size(console_ta, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(console_ta, 0, 0);
    lv_textarea_set_text(console_ta, "");
    lv_obj_set_style_bg_color(console_ta, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(console_ta, 0, 0);
    lv_obj_set_style_text_color(console_ta, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(console_ta, 0, 0);
    lv_obj_set_style_pad_all(console_ta, 4, 0);
    lv_obj_set_scrollbar_mode(console_ta, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(console_ta, UI_COLOR_PRIMARY, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(console_ta, LV_OPA_40, LV_PART_SCROLLBAR);
    
    lv_obj_add_flag(console_container, LV_OBJ_FLAG_HIDDEN);
}

static void create_info_tab(void) {
    info_container = lv_obj_create(content_area);
    lv_obj_set_size(info_container, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(info_container, 0, 0);
    lv_obj_set_style_bg_opa(info_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(info_container, 0, 0);
    lv_obj_set_style_pad_all(info_container, 0, 0);
    lv_obj_set_scrollbar_mode(info_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(info_container, UI_COLOR_PRIMARY, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(info_container, LV_OPA_40, LV_PART_SCROLLBAR);
    
    int y = 4, rh = 20;
    const char *lines[] = {
        "[ BUILD ]", "Version:  " UI_VERSION, "ESP-IDF:  v5.5.2", "LVGL:     9.x", NULL,
        "[ HARDWARE ]", "Board:    T-Deck Plus", "CPU:      ESP32-S3 @ 240MHz",
        "RAM:      512KB + 8MB PSRAM", "Display:  320x240 ST7789", "Touch:    GT911"
    };
    for (int i = 0; i < 11; i++) {
        if (!lines[i]) { y += 6; continue; }
        lv_obj_t *l = lv_label_create(info_container);
        lv_label_set_text(l, lines[i]);
        bool hdr = (lines[i][0] == '[');
        lv_obj_set_style_text_color(l, hdr ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_pos(l, hdr ? 4 : 8, y);
        y += rh;
    }
    lv_obj_add_flag(info_container, LV_OBJ_FLAG_HIDDEN);
}

static void update_stats(lv_timer_t *t) {
    if (current_tab != 0 || !heap_val) return;
    char b[32];
    snprintf(b, 32, "%lu KB", (unsigned long)(esp_get_free_heap_size()/1024));
    lv_label_set_text(heap_val, b);
    snprintf(b, 32, "%lu KB", (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_DMA)/1024));
    lv_label_set_text(heap_dma_val, b);
    snprintf(b, 32, "%lu KB", (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024));
    lv_label_set_text(heap_psram_val, b);
    int s = (int)(esp_timer_get_time()/1000000);
    snprintf(b, 32, "%02d:%02d:%02d", s/3600, (s%3600)/60, s%60);
    lv_label_set_text(uptime_val, b);
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        snprintf(b, 32, "%d dBm", ap.rssi);
        lv_label_set_text(wifi_rssi_val, b);
        snprintf(b, 32, "%d", ap.primary);
        lv_label_set_text(wifi_ch_val, b);
    }
}

lv_obj_t *ui_developer_create(void) {
    ESP_LOGI(TAG, "Creating developer screen...");
    
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    
    // Header: Title links
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Developer");
    lv_obj_set_style_text_color(title, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(title, 4, 2);
    
    // Header: RESET rechts (wird zuerst erstellt als Anker)
    reset_lbl = lv_label_create(screen);
    lv_label_set_text(reset_lbl, "RESET");
    lv_obj_set_style_text_color(reset_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_align(reset_lbl, LV_ALIGN_TOP_RIGHT, -4, 2);
    lv_obj_add_flag(reset_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(reset_lbl, on_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN);
    
    // Header: | Trenner links von RESET
    sep_lbl = lv_label_create(screen);
    lv_label_set_text(sep_lbl, "|");
    lv_obj_set_style_text_color(sep_lbl, UI_COLOR_LINE_DIM, 0);
    lv_obj_align_to(sep_lbl, reset_lbl, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    lv_obj_add_flag(sep_lbl, LV_OBJ_FLAG_HIDDEN);
    
    // Header: PAUSE links vom Trenner
    pause_lbl = lv_label_create(screen);
    lv_label_set_text(pause_lbl, "PAUSE");
    lv_obj_set_style_text_color(pause_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_align_to(pause_lbl, sep_lbl, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    lv_obj_add_flag(pause_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pause_lbl, on_pause, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(pause_lbl, LV_OBJ_FLAG_HIDDEN);
    
    ui_create_line(screen, UI_HEADER_H);
    
    // Content Area
    content_area = lv_obj_create(screen);
    lv_obj_set_size(content_area, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(content_area, 0, UI_CONTENT_Y);
    lv_obj_set_style_bg_opa(content_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_area, 0, 0);
    lv_obj_set_style_pad_all(content_area, 0, 0);
    lv_obj_clear_flag(content_area, LV_OBJ_FLAG_SCROLLABLE);
    
    create_stats_tab();
    create_console_tab();
    create_info_tab();
    
    // Nav Bar
    ui_create_nav_bar(screen);
    
    btn_back = ui_create_nav_btn(screen, "BACK", 0);
    btn_stats = ui_create_nav_btn(screen, "STATS", 1);
    btn_console = ui_create_nav_btn(screen, "CONS", 2);
    btn_info = ui_create_nav_btn(screen, "INFO", 3);
    
    lv_obj_set_style_text_color(lv_obj_get_child(btn_console, 0), UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_info, 0), UI_COLOR_TEXT_DIM, 0);
    
    lv_obj_add_event_cb(btn_back, on_back, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_stats, on_tab_stats, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_console, on_tab_console, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_info, on_tab_info, LV_EVENT_CLICKED, NULL);
    
    update_tmr = lv_timer_create(update_stats, 1000, NULL);
    update_stats(NULL);
    
    ui_developer_log("SimpleX Console ready");
    
    return screen;
}

void ui_developer_log(const char *msg) {
    if (console_ta && log_enabled && msg) {
        lv_textarea_add_text(console_ta, "> ");
        lv_textarea_add_text(console_ta, msg);
        lv_textarea_add_text(console_ta, "\n");
    }
}

void ui_developer_update_stats(void) { update_stats(NULL); }
