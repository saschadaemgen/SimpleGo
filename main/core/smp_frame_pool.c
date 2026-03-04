/**
 * SimpleGo - smp_frame_pool.c
 * Static frame pool for zero-copy inter-task communication
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_frame_pool.h"
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sodium.h"

static const char *TAG = "SMP_POOL";

// Pool allocated dynamically in PSRAM
static smp_frame_t *s_pool = NULL;
static bool s_used[SMP_FRAME_POOL_SIZE];
static SemaphoreHandle_t s_mutex = NULL;

void smp_frame_pool_init(void)
{
    s_pool = heap_caps_calloc(SMP_FRAME_POOL_SIZE, sizeof(smp_frame_t),
                              MALLOC_CAP_SPIRAM);
    if (!s_pool) {
        ESP_LOGE(TAG, "Failed to allocate frame pool in PSRAM!");
        return;
    }

    memset(s_used, 0, sizeof(s_used));

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create pool mutex");
        heap_caps_free(s_pool);
        s_pool = NULL;
        return;
    }

    ESP_LOGI(TAG, "Frame pool initialized (PSRAM): %d frames x %d bytes = %d KB",
             SMP_FRAME_POOL_SIZE, (int)sizeof(smp_frame_t),
             (int)(SMP_FRAME_POOL_SIZE * sizeof(smp_frame_t) / 1024));
}

smp_frame_t *smp_frame_pool_alloc(void)
{
    if (!s_mutex || !s_pool) return NULL;

    smp_frame_t *frame = NULL;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < SMP_FRAME_POOL_SIZE; i++) {
            if (!s_used[i]) {
                s_used[i] = true;
                s_pool[i].len = 0;
                frame = &s_pool[i];
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }

    if (!frame) {
        ESP_LOGW(TAG, "Pool exhausted! (%d/%d in use)",
                 SMP_FRAME_POOL_SIZE - smp_frame_pool_available(),
                 SMP_FRAME_POOL_SIZE);
    }

    return frame;
}

void smp_frame_pool_free(smp_frame_t *frame)
{
    if (!frame || !s_mutex || !s_pool) return;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < SMP_FRAME_POOL_SIZE; i++) {
            if (frame == &s_pool[i]) {
                sodium_memzero(frame->data, sizeof(frame->data));
                frame->len = 0;
                s_used[i] = false;
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }
}

int smp_frame_pool_available(void)
{
    if (!s_mutex || !s_pool) return 0;

    int count = 0;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < SMP_FRAME_POOL_SIZE; i++) {
            if (!s_used[i]) count++;
        }
        xSemaphoreGive(s_mutex);
    }

    return count;
}
