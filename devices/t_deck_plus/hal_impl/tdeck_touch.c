/**
 * @file tdeck_touch.c
 * @brief GT911 Touch Driver - Calibrated for T-Deck Plus
 */

#include "tdeck_touch.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "TOUCH";

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA         18
#define I2C_SCL         8
#define I2C_FREQ        100000

#define GT911_ADDR_1    0x5D
#define GT911_ADDR_2    0x14

#define GT911_PRODUCT_ID    0x8140
#define GT911_POINT_INFO    0x814E
#define GT911_POINT_1       0x8150

#define TOUCH_INT_PIN   16

#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240

// Calibration values from corner measurements
#define RAW_X_MIN       6
#define RAW_X_MAX       233
#define RAW_Y_MIN       5
#define RAW_Y_MAX       314

static bool initialized = false;
static uint8_t gt911_addr = GT911_ADDR_1;
static lv_indev_t *indev = NULL;
static int16_t last_x = 0;
static int16_t last_y = 0;

static esp_err_t i2c_master_init(void)
{
    static bool i2c_initialized = false;
    if (i2c_initialized) return ESP_OK;
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) return ret;
    
    ret = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret == ESP_ERR_INVALID_STATE) {
        i2c_initialized = true;
        return ESP_OK;
    }
    if (ret == ESP_OK) i2c_initialized = true;
    return ret;
}

static esp_err_t gt911_read(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = { (reg >> 8) & 0xFF, reg & 0xFF };
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, reg_buf, 2, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t gt911_write_byte(uint16_t reg, uint8_t value)
{
    uint8_t buf[3] = { (reg >> 8) & 0xFF, reg & 0xFF, value };
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 3, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static bool gt911_probe(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

esp_err_t tdeck_touch_init(void)
{
    if (initialized) return ESP_OK;
    
    ESP_LOGI(TAG, "Initializing GT911 touch controller...");
    
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (gt911_probe(GT911_ADDR_1)) {
        gt911_addr = GT911_ADDR_1;
        ESP_LOGI(TAG, "GT911 found at 0x%02X", gt911_addr);
    } else if (gt911_probe(GT911_ADDR_2)) {
        gt911_addr = GT911_ADDR_2;
        ESP_LOGI(TAG, "GT911 found at 0x%02X", gt911_addr);
    } else {
        ESP_LOGE(TAG, "GT911 not found!");
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t product_id[4] = {0};
    gt911_read(GT911_PRODUCT_ID, product_id, 4);
    ESP_LOGI(TAG, "Product ID: %c%c%c%c", product_id[0], product_id[1], product_id[2], product_id[3]);
    
    initialized = true;
    ESP_LOGI(TAG, "GT911 calibrated and ready!");
    return ESP_OK;
}

bool tdeck_touch_read(int16_t *x, int16_t *y)
{
    if (!initialized) return false;
    
    uint8_t status = 0;
    if (gt911_read(GT911_POINT_INFO, &status, 1) != ESP_OK) {
        return false;
    }
    
    uint8_t points = status & 0x0F;
    bool ready = (status & 0x80) != 0;
    
    gt911_write_byte(GT911_POINT_INFO, 0);
    
    if (!ready || points == 0) {
        return false;
    }
    
    uint8_t data[6];
    if (gt911_read(GT911_POINT_1, data, 6) != ESP_OK) {
        return false;
    }
    
    int16_t raw_x = data[0] | (data[1] << 8);
    int16_t raw_y = data[2] | (data[3] << 8);
    
    // CALIBRATED TRANSFORMATION:
    // Raw Y (5-314) -> Screen X (0-319)
    // Raw X (233-6) -> Screen Y (0-239) [inverted!]
    
    // Screen X = map raw_y from [RAW_Y_MIN, RAW_Y_MAX] to [0, 319]
    int32_t screen_x = ((int32_t)(raw_y - RAW_Y_MIN) * (SCREEN_WIDTH - 1)) / (RAW_Y_MAX - RAW_Y_MIN);
    
    // Screen Y = map raw_x from [RAW_X_MAX, RAW_X_MIN] to [0, 239] (inverted)
    int32_t screen_y = ((int32_t)(RAW_X_MAX - raw_x) * (SCREEN_HEIGHT - 1)) / (RAW_X_MAX - RAW_X_MIN);
    
    // Clamp to screen bounds
    if (screen_x < 0) screen_x = 0;
    if (screen_x >= SCREEN_WIDTH) screen_x = SCREEN_WIDTH - 1;
    if (screen_y < 0) screen_y = 0;
    if (screen_y >= SCREEN_HEIGHT) screen_y = SCREEN_HEIGHT - 1;
    
    *x = (int16_t)screen_x;
    *y = (int16_t)screen_y;
    
    last_x = *x;
    last_y = *y;
    
    return true;
}

static void touch_read_cb(lv_indev_t *drv, lv_indev_data_t *data)
{
    int16_t x, y;
    
    if (tdeck_touch_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_indev_t *tdeck_touch_register_lvgl(void)
{
    if (!initialized) return NULL;
    if (indev) return indev;
    
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    
    ESP_LOGI(TAG, "Touch registered with LVGL");
    return indev;
}
