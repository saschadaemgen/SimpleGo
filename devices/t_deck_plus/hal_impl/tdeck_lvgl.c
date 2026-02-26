/**
 * @file tdeck_lvgl.c
 * @brief LVGL Integration - Task starts AFTER UI init
 */

#include "tdeck_lvgl.h"
#include "tdeck_display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "lvgl.h"

static const char *TAG = "TDECK_LVGL";

#define LVGL_BUFFER_LINES       20
#define LVGL_TICK_PERIOD_MS     1

static lv_display_t *lvgl_display = NULL;
static uint8_t *draw_buf1 = NULL;
static uint8_t *draw_buf2 = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static TaskHandle_t lvgl_task_handle = NULL;
static bool is_initialized = false;
static bool task_running = false;

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = tdeck_display_get_panel();
    if (panel) {
        esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    }
    lv_display_flush_ready(disp);
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *pvParameter)
{
    ESP_LOGI(TAG, "LVGL task running");
    
    // Reconfigure WDT: keep it active but don't watch idle tasks
    // This prevents "task not found" errors when LVGL starves IDLE1
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,  // Don't watch ANY idle task
        .trigger_panic = false
    };
    esp_err_t err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WDT reconfigured: idle core monitoring disabled");
    } else {
        ESP_LOGW(TAG, "WDT reconfigure failed: %s", esp_err_to_name(err));
    }
    
    while (1) {
        if (xSemaphoreTakeRecursive(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGiveRecursive(lvgl_mutex);
            if (time_till_next < 1) time_till_next = 1;
            if (time_till_next > 10) time_till_next = 10;
            vTaskDelay(pdMS_TO_TICKS(time_till_next));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

esp_err_t tdeck_lvgl_init(void)
{
    if (is_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "LVGL Init...");

    /*
     * Session 37b: Recursive mutex — allows nested locking.
     *
     * WHY recursive? Display and SD card share SPI2_HOST. All SD file
     * operations take this mutex via tdeck_lvgl_lock() to prevent
     * SPI bus contention with display rendering. But smp_history_delete()
     * is called from LVGL context (on_contact_click → ui_poll_timer_cb)
     * where the mutex is already held. Recursive mutex avoids deadlock.
     */
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (!lvgl_mutex) {
        ESP_LOGE(TAG, "Mutex failed!");
        return ESP_FAIL;
    }

    lv_init();

    size_t buf_size = TDECK_DISPLAY_WIDTH * LVGL_BUFFER_LINES * sizeof(lv_color_t);
    
    /*
     * Session 37b: Draw buffer placement for smooth scrolling.
     *
     * buf1 in internal DMA-capable SRAM: fast SPI2 transfers, no tearing.
     * buf2 in PSRAM: LVGL renders here while buf1 is being DMA'd to display.
     *
     * Cost: ~12.8KB internal SRAM (320 × 20 lines × 2 bytes RGB565).
     * Benefit: eliminates scroll "Schlieren" (tearing artifacts) because
     * DMA from internal SRAM is ~4x faster than from PSRAM via cache.
     *
     * If internal SRAM is too tight, fall back to both in PSRAM.
     */
    draw_buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    draw_buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    if (!draw_buf1) {
        // Fallback: both in PSRAM (slower but works)
        ESP_LOGW(TAG, "Internal DMA buf failed (%u bytes), falling back to PSRAM", (unsigned)buf_size);
        draw_buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    }
    if (!draw_buf1) {
        ESP_LOGE(TAG, "Buffer alloc failed!");
        return ESP_ERR_NO_MEM;
    }

    lvgl_display = lv_display_create(TDECK_DISPLAY_WIDTH, TDECK_DISPLAY_HEIGHT);
    if (!lvgl_display) {
        ESP_LOGE(TAG, "Display create failed!");
        return ESP_FAIL;
    }

    lv_display_set_flush_cb(lvgl_display, lvgl_flush_cb);
    lv_display_set_color_format(lvgl_display, LV_COLOR_FORMAT_RGB565);

    if (draw_buf2) {
        lv_display_set_buffers(lvgl_display, draw_buf1, draw_buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        lv_display_set_buffers(lvgl_display, draw_buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }

    // Set default screen to BLACK immediately
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Tick timer - needed for animations
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_create(&tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);

    // DON'T start task yet - wait for UI to be ready
    is_initialized = true;
    ESP_LOGI(TAG, "LVGL initialized (task not started yet)");

    return ESP_OK;
}

// Call this AFTER ui_manager_init() to start rendering
void tdeck_lvgl_start(void)
{
    if (!is_initialized || task_running) return;
    
    xTaskCreatePinnedToCore(lvgl_task, "LVGL", 16384, NULL, 5, &lvgl_task_handle, 1);
    task_running = true;
    ESP_LOGI(TAG, "LVGL task started");
}

bool tdeck_lvgl_lock(uint32_t timeout_ms)
{
    if (!lvgl_mutex) return false;
    return xSemaphoreTakeRecursive(lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void tdeck_lvgl_unlock(void)
{
    if (lvgl_mutex) {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}
