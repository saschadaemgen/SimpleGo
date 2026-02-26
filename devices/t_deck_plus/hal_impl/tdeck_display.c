/**
 * @file tdeck_display.c
 * @brief T-Deck Plus ST7789 Display Driver - NO FLASH
 */

#include "tdeck_display.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "TDECK_DISP";

#define TDECK_POWER_ON      GPIO_NUM_10
#define TDECK_SPI_HOST      SPI2_HOST
#define TDECK_SPI_MOSI      GPIO_NUM_41
#define TDECK_SPI_MISO      GPIO_NUM_38
#define TDECK_SPI_SCLK      GPIO_NUM_40
#define TDECK_TFT_CS        GPIO_NUM_12
#define TDECK_TFT_DC        GPIO_NUM_11
#define TDECK_TFT_BL        GPIO_NUM_42
#define TDECK_TFT_RST       GPIO_NUM_NC

#define TFT_WIDTH           320
#define TFT_HEIGHT          240
#define SPI_FREQ_HZ         (40 * 1000 * 1000)

#define BL_LEDC_TIMER       LEDC_TIMER_0
#define BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BL_LEDC_SPEED       LEDC_LOW_SPEED_MODE

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static bool is_initialized = false;

static esp_err_t power_on(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << TDECK_POWER_ON),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(TDECK_POWER_ON, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = BL_LEDC_SPEED,
        .timer_num = BL_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .speed_mode = BL_LEDC_SPEED,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = TDECK_TFT_BL,
        .duty = 0,  // START WITH BACKLIGHT OFF!
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
    return ESP_OK;
}

void tdeck_display_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * 255) / 100;
    ledc_set_duty(BL_LEDC_SPEED, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_SPEED, BL_LEDC_CHANNEL);
}

static esp_err_t spi_bus_init(void)
{
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = TDECK_SPI_SCLK,
        .mosi_io_num = TDECK_SPI_MOSI,
        .miso_io_num = TDECK_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t),
    };
    return spi_bus_initialize(TDECK_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
}

static esp_err_t panel_init(void)
{
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = TDECK_TFT_DC,
        .cs_gpio_num = TDECK_TFT_CS,
        .pclk_hz = SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_err_t ret = esp_lcd_new_panel_io_spi(TDECK_SPI_HOST, &io_cfg, &io_handle);
    if (ret != ESP_OK) return ret;

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = TDECK_TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &panel_handle);
    if (ret != ESP_OK) return ret;

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 0);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    
    return ESP_OK;
}

static void clear_screen_black(void)
{
    uint16_t *black_buf = heap_caps_malloc(320 * 40 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (black_buf) {
        memset(black_buf, 0x00, 320 * 40 * sizeof(uint16_t));
        for (int y = 0; y < 240; y += 40) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 40, black_buf);
        }
        free(black_buf);
    }
}

esp_err_t tdeck_display_init(void)
{
    if (is_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Display init (no flash)...");
    
    esp_err_t ret;

    // 1. Power ON
    ret = power_on();
    if (ret != ESP_OK) return ret;

    // 2. Backlight init BUT KEEP OFF
    ret = backlight_init();
    if (ret != ESP_OK) return ret;

    // 3. SPI bus
    ret = spi_bus_init();
    if (ret != ESP_OK) return ret;

    // 4. Panel init
    ret = panel_init();
    if (ret != ESP_OK) return ret;

    // 5. CLEAR SCREEN TO BLACK (while backlight is still off!)
    clear_screen_black();

    // 6. NOW turn backlight on - screen is already black!
    // Backlight wird spaeter von main.c eingeschaltet

    is_initialized = true;
    ESP_LOGI(TAG, "Display ready!");
    
    return ESP_OK;
}

void tdeck_display_fill(uint16_t color)
{
    if (!is_initialized || !panel_handle) return;
    
    size_t line_size = TFT_WIDTH * sizeof(uint16_t);
    uint16_t *line_buf = heap_caps_malloc(line_size, MALLOC_CAP_DMA);
    if (!line_buf) return;
    
    for (int i = 0; i < TFT_WIDTH; i++) {
        line_buf[i] = color;
    }
    for (int y = 0; y < TFT_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, TFT_WIDTH, y + 1, line_buf);
    }
    free(line_buf);
}

void tdeck_display_test(void)
{
    tdeck_display_fill(0x001F);
    vTaskDelay(pdMS_TO_TICKS(1000));
    tdeck_display_fill(0xF800);
    vTaskDelay(pdMS_TO_TICKS(1000));
    tdeck_display_fill(0x07E0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    tdeck_display_fill(0x0000);
}

esp_lcd_panel_handle_t tdeck_display_get_panel(void)
{
    return panel_handle;
}
