/*
 * test_sntrup761.c - Standalone verification test for sntrup761 KEM
 *
 * Tests: keypair generation, encapsulation, decapsulation.
 * Verifies shared secrets match and logs timing for each operation.
 *
 * This test MUST run on a task with >= 80 KB stack (internal SRAM).
 * Do NOT call from app_main directly - the default main task stack
 * is too small.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#include "sntrup761.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sodium.h>  /* for sodium_memzero */

static const char *TAG = "sntrup761_test";

/* Test result - set by test task, read by caller */
static volatile int s_test_result = -1;
static volatile bool s_test_done = false;

static void test_task(void *arg) {
    uint8_t pk[SNTRUP761_PUBLICKEYBYTES];
    uint8_t sk[SNTRUP761_SECRETKEYBYTES];
    uint8_t ct[SNTRUP761_CIPHERTEXTBYTES];
    uint8_t ss_enc[SNTRUP761_BYTES];
    uint8_t ss_dec[SNTRUP761_BYTES];
    int64_t t_start, t_keygen, t_enc, t_dec;
    int ret;

    ESP_LOGI(TAG, "=== sntrup761 KEM Verification Test ===");
    ESP_LOGI(TAG, "PK: %d bytes, SK: %d bytes, CT: %d bytes, SS: %d bytes",
             SNTRUP761_PUBLICKEYBYTES, SNTRUP761_SECRETKEYBYTES,
             SNTRUP761_CIPHERTEXTBYTES, SNTRUP761_BYTES);

    /* Report stack high water mark before heavy operations */
    UBaseType_t stack_before = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Stack high water mark before keygen: %u bytes free",
             (unsigned int)(stack_before * sizeof(StackType_t)));

    /* ---- Key Generation ---- */
    t_start = esp_timer_get_time();
    ret = sntrup761_keypair(pk, sk);
    t_keygen = esp_timer_get_time() - t_start;

    if (ret != 0) {
        ESP_LOGE(TAG, "FAIL: keypair returned %d", ret);
        s_test_result = 1;
        s_test_done = true;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "keygen: %lld us (%.1f ms)", t_keygen, t_keygen / 1000.0);

    /* Report stack after keygen (deepest point) */
    UBaseType_t stack_after_keygen = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Stack high water mark after keygen: %u bytes free",
             (unsigned int)(stack_after_keygen * sizeof(StackType_t)));
    ESP_LOGI(TAG, "Stack consumed by keygen: ~%u bytes",
             (unsigned int)((stack_before - stack_after_keygen) * sizeof(StackType_t)));

    /* ---- Encapsulation ---- */
    t_start = esp_timer_get_time();
    ret = sntrup761_enc(ct, ss_enc, pk);
    t_enc = esp_timer_get_time() - t_start;

    if (ret != 0) {
        ESP_LOGE(TAG, "FAIL: enc returned %d", ret);
        s_test_result = 2;
        s_test_done = true;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "enc:    %lld us (%.1f ms)", t_enc, t_enc / 1000.0);

    /* ---- Decapsulation ---- */
    t_start = esp_timer_get_time();
    ret = sntrup761_dec(ss_dec, ct, sk);
    t_dec = esp_timer_get_time() - t_start;

    if (ret != 0) {
        ESP_LOGE(TAG, "FAIL: dec returned %d", ret);
        s_test_result = 3;
        s_test_done = true;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "dec:    %lld us (%.1f ms)", t_dec, t_dec / 1000.0);

    /* ---- Verify shared secrets match ---- */
    if (memcmp(ss_enc, ss_dec, SNTRUP761_BYTES) != 0) {
        ESP_LOGE(TAG, "FAIL: shared secrets do NOT match!");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, ss_enc, SNTRUP761_BYTES, ESP_LOG_ERROR);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, ss_dec, SNTRUP761_BYTES, ESP_LOG_ERROR);
        s_test_result = 4;
        s_test_done = true;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "PASS: shared secrets match (32 bytes)");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ss_enc, SNTRUP761_BYTES, ESP_LOG_INFO);

    /* ---- Summary ---- */
    ESP_LOGI(TAG, "=== Timing Summary ===");
    ESP_LOGI(TAG, "  keygen: %6lld us (%5.1f ms)", t_keygen, t_keygen / 1000.0);
    ESP_LOGI(TAG, "  enc:    %6lld us (%5.1f ms)", t_enc, t_enc / 1000.0);
    ESP_LOGI(TAG, "  dec:    %6lld us (%5.1f ms)", t_dec, t_dec / 1000.0);
    ESP_LOGI(TAG, "  total:  %6lld us (%5.1f ms)",
             t_keygen + t_enc + t_dec,
             (t_keygen + t_enc + t_dec) / 1000.0);

    /* Stack report */
    UBaseType_t stack_final = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Stack high water mark final: %u bytes free (of 81920)",
             (unsigned int)(stack_final * sizeof(StackType_t)));
    ESP_LOGI(TAG, "Peak stack usage: ~%u bytes",
             81920 - (unsigned int)(stack_final * sizeof(StackType_t)));

    /* Secure wipe */
    sodium_memzero(sk, sizeof(sk));
    sodium_memzero(ss_enc, sizeof(ss_enc));
    sodium_memzero(ss_dec, sizeof(ss_dec));

    ESP_LOGI(TAG, "=== Test Complete: PASS ===");
    s_test_result = 0;
    s_test_done = true;
    vTaskDelete(NULL);
}

int sntrup761_run_test(void) {
    s_test_result = -1;
    s_test_done = false;

    /* Create test task with 80 KB stack in internal SRAM */
    BaseType_t ret = xTaskCreatePinnedToCore(
        test_task,
        "pq_test",
        80 * 1024 / sizeof(StackType_t),  /* 80 KB stack */
        NULL,
        5,       /* priority */
        NULL,
        0        /* Core 0 */
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create test task - not enough SRAM?");
        return -1;
    }

    /* Wait for test to complete (timeout 10 seconds) */
    int timeout_ms = 10000;
    while (!s_test_done && timeout_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout_ms -= 100;
    }

    if (!s_test_done) {
        ESP_LOGE(TAG, "Test timed out after 10 seconds");
        return -2;
    }

    return s_test_result;
}
