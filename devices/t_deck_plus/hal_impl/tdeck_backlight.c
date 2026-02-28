/**
 * @file tdeck_backlight.c
 * @brief T-Deck Plus Display Backlight (Pulse-Counting GPIO 42)
 *
 * The backlight chip uses a one-wire pulse-counting protocol.
 * EXAKT translated from LilyGO factory firmware UnitTest.ino:277-300.
 * 16 brightness levels (0=off, 1=min, 16=max).
 *
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "tdeck_backlight.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TDECK_BL";

#define BL_GPIO         GPIO_NUM_42
#define BL_STEPS        16

static uint8_t bl_level = 0;

void tdeck_backlight_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    /* Start OFF (main.c turns on after screen is cleared) */
    gpio_set_level(BL_GPIO, 0);
    bl_level = 0;

    ESP_LOGI(TAG, "Display backlight init (GPIO %d, %d levels)", BL_GPIO, BL_STEPS);
}

/* EXAKT wie LilyGO Original-Firmware (UnitTest.ino:277-300) */
void tdeck_backlight_set(uint8_t value)
{
    if (value > BL_STEPS) value = BL_STEPS;

    if (value == 0) {
        gpio_set_level(BL_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(3));
        bl_level = 0;
        return;
    }
    if (bl_level == 0) {
        gpio_set_level(BL_GPIO, 1);
        bl_level = BL_STEPS;
        ets_delay_us(30);
    }
    int from = BL_STEPS - bl_level;
    int to = BL_STEPS - value;
    int num = (BL_STEPS + to - from) % BL_STEPS;
    for (int i = 0; i < num; i++) {
        gpio_set_level(BL_GPIO, 0);
        gpio_set_level(BL_GPIO, 1);
    }
    bl_level = value;
}

uint8_t tdeck_backlight_get(void)
{
    return bl_level;
}
