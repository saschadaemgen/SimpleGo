/*
 * pq_crypto_task.c - Dedicated crypto task for post-quantum operations
 *
 * Architecture:
 *   - 80 KB stack in PSRAM (heap_caps_malloc + xTaskCreateStatic)
 *   - PSRAM is safe here: this task does NOT write NVS (the ESP32-S3
 *     hardware constraint only affects NVS/flash writes, which disable
 *     the cache that PSRAM depends on). SHA-512 hardware accelerator
 *     uses memory-mapped peripheral registers, not DMA.
 *   - Waits on FreeRTOS queue for work items
 *   - Serializes all sntrup761 operations (keygen, encap, decap)
 *   - Supports background keypair pre-computation
 *   - Results returned via per-request semaphore
 *
 * Why PSRAM: Internal SRAM has only ~6 KB free after SimpleGo boots
 * (measured Session 46). 80 KB from internal SRAM is impossible.
 * PSRAM has ~7.9 MB free. Decision documented and approved.
 *
 * NVS persistence of PQ keys is handled by smp_app_task (SRAM stack).
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#include "pq_crypto_task.h"
#include "sntrup761.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <sodium.h>
#include "esp_heap_caps.h"

static const char *TAG = "pq_crypto";

/* ---- Request types ---- */

typedef enum {
    PQ_OP_KEYGEN,
    PQ_OP_ENC,
    PQ_OP_DEC,
    PQ_OP_PRECOMPUTE,  /* background keypair generation */
} pq_op_type_t;

typedef struct {
    pq_op_type_t op;
    /* Pointers to caller's buffers (caller owns memory) */
    union {
        struct {
            uint8_t *pk;
            uint8_t *sk;
        } keygen;
        struct {
            uint8_t *ct;
            uint8_t *ss;
            const uint8_t *pk;
        } enc;
        struct {
            uint8_t *ss;
            const uint8_t *ct;
            const uint8_t *sk;
        } dec;
    };
    /* Result signaling */
    int result;                  /* 0 = success */
    SemaphoreHandle_t done_sem;  /* NULL for background ops */
} pq_request_t;

/* ---- Static state ---- */

static TaskHandle_t s_crypto_task = NULL;
static QueueHandle_t s_request_queue = NULL;

/* xTaskCreateStatic resources - stack allocated from PSRAM at runtime */
static StaticTask_t s_crypto_task_tcb;
static StackType_t *s_crypto_stack = NULL;  /* heap_caps_malloc(PSRAM) */

/* Pre-computed keypair buffer */
static uint8_t s_precomp_pk[SNTRUP761_PUBLICKEYBYTES];
static uint8_t s_precomp_sk[SNTRUP761_SECRETKEYBYTES];
static volatile bool s_precomp_ready = false;
static SemaphoreHandle_t s_precomp_mutex = NULL;

/* ---- Internal: execute operations ---- */

static void handle_keygen(pq_request_t *req) {
    int64_t t0 = esp_timer_get_time();
    req->result = sntrup761_keypair(req->keygen.pk, req->keygen.sk);
    int64_t dt = esp_timer_get_time() - t0;
    ESP_LOGD(TAG, "keygen: %lld us (%d)", dt, req->result);
}

static void handle_enc(pq_request_t *req) {
    int64_t t0 = esp_timer_get_time();
    req->result = sntrup761_enc(req->enc.ct, req->enc.ss, req->enc.pk);
    int64_t dt = esp_timer_get_time() - t0;
    ESP_LOGD(TAG, "enc: %lld us (%d)", dt, req->result);
}

static void handle_dec(pq_request_t *req) {
    int64_t t0 = esp_timer_get_time();
    req->result = sntrup761_dec(req->dec.ss, req->dec.ct, req->dec.sk);
    int64_t dt = esp_timer_get_time() - t0;
    ESP_LOGD(TAG, "dec: %lld us (%d)", dt, req->result);
}

static void handle_precompute(void) {
    if (s_precomp_ready) {
        ESP_LOGD(TAG, "precompute: keypair already available, skipping");
        return;
    }

    uint8_t pk[SNTRUP761_PUBLICKEYBYTES];
    uint8_t sk[SNTRUP761_SECRETKEYBYTES];

    int64_t t0 = esp_timer_get_time();
    int ret = sntrup761_keypair(pk, sk);
    int64_t dt = esp_timer_get_time() - t0;

    if (ret == 0) {
        xSemaphoreTake(s_precomp_mutex, portMAX_DELAY);
        memcpy(s_precomp_pk, pk, sizeof(s_precomp_pk));
        memcpy(s_precomp_sk, sk, sizeof(s_precomp_sk));
        s_precomp_ready = true;
        xSemaphoreGive(s_precomp_mutex);
        ESP_LOGI(TAG, "precomputed keypair ready (%lld us)", dt);
    } else {
        ESP_LOGE(TAG, "precompute keygen failed: %d", ret);
    }

    /* Wipe local copies */
    sodium_memzero(pk, sizeof(pk));
    sodium_memzero(sk, sizeof(sk));
}

/* ---- Task function ---- */

static void crypto_task(void *arg) {
    pq_request_t req;

    ESP_LOGI(TAG, "crypto task started on core %d, stack %d bytes",
             xPortGetCoreID(), PQ_CRYPTO_TASK_STACK_SIZE);

    /* Report initial stack */
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "initial stack free: %u bytes",
             (unsigned int)(hwm * sizeof(StackType_t)));

    while (1) {
        if (xQueueReceive(s_request_queue, &req, portMAX_DELAY) == pdTRUE) {
            switch (req.op) {
                case PQ_OP_KEYGEN:
                    handle_keygen(&req);
                    break;
                case PQ_OP_ENC:
                    handle_enc(&req);
                    break;
                case PQ_OP_DEC:
                    handle_dec(&req);
                    break;
                case PQ_OP_PRECOMPUTE:
                    handle_precompute();
                    break;
            }

            /* Signal completion for blocking ops */
            if (req.done_sem != NULL) {
                xSemaphoreGive(req.done_sem);
            }
        }
    }
}

/* ---- Public API: init/deinit ---- */

esp_err_t pq_crypto_task_init(void) {
    if (s_crypto_task != NULL) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    /* Create pre-computation mutex */
    s_precomp_mutex = xSemaphoreCreateMutex();
    if (s_precomp_mutex == NULL) {
        ESP_LOGE(TAG, "failed to create precomp mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create request queue */
    s_request_queue = xQueueCreate(PQ_CRYPTO_QUEUE_DEPTH, sizeof(pq_request_t));
    if (s_request_queue == NULL) {
        ESP_LOGE(TAG, "failed to create request queue");
        vSemaphoreDelete(s_precomp_mutex);
        s_precomp_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Allocate 80 KB stack from PSRAM explicitly.
     *
     * Why PSRAM is safe for this task:
     *   1. No NVS writes (flash ops disable cache, PSRAM depends on cache)
     *   2. SHA-512 peripheral is memory-mapped, not DMA-based
     *   3. All crypto is pure computation, no SPI/flash interaction
     *
     * Internal SRAM budget: only ~6 KB free after boot (Session 46 measurement).
     * PSRAM budget: ~7.9 MB free. 80 KB is negligible.
     */
    size_t stack_words = PQ_CRYPTO_TASK_STACK_SIZE / sizeof(StackType_t);
    s_crypto_stack = (StackType_t *)heap_caps_malloc(
        PQ_CRYPTO_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);

    if (s_crypto_stack == NULL) {
        ESP_LOGE(TAG, "failed to allocate %d bytes from PSRAM for crypto stack",
                 PQ_CRYPTO_TASK_STACK_SIZE);
        ESP_LOGE(TAG, "free PSRAM: %u bytes",
                 (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
        vSemaphoreDelete(s_precomp_mutex);
        s_precomp_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Create task with static stack (PSRAM) and static TCB (BSS/SRAM).
     * xTaskCreateStatic never fails - it returns the task handle directly.
     * Core affinity set via separate API call after creation. */
    s_crypto_task = xTaskCreateStaticPinnedToCore(
        crypto_task,
        "pq_crypto",
        stack_words,
        NULL,
        PQ_CRYPTO_TASK_PRIORITY,
        s_crypto_stack,
        &s_crypto_task_tcb,
        PQ_CRYPTO_TASK_CORE
    );

    if (s_crypto_task == NULL) {
        /* Should never happen with xTaskCreateStatic, but be defensive */
        ESP_LOGE(TAG, "xTaskCreateStaticPinnedToCore returned NULL");
        heap_caps_free(s_crypto_stack);
        s_crypto_stack = NULL;
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
        vSemaphoreDelete(s_precomp_mutex);
        s_precomp_mutex = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "initialized (stack %d KB PSRAM, core %d)",
             PQ_CRYPTO_TASK_STACK_SIZE / 1024, PQ_CRYPTO_TASK_CORE);
    ESP_LOGI(TAG, "free PSRAM: %u bytes, free SRAM: %u bytes",
             (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    return ESP_OK;
}

void pq_crypto_task_deinit(void) {
    if (s_crypto_task != NULL) {
        vTaskDelete(s_crypto_task);
        s_crypto_task = NULL;
    }
    if (s_crypto_stack != NULL) {
        /* Wipe stack memory before freeing (may contain key material) */
        sodium_memzero(s_crypto_stack, PQ_CRYPTO_TASK_STACK_SIZE);
        heap_caps_free(s_crypto_stack);
        s_crypto_stack = NULL;
    }
    if (s_request_queue != NULL) {
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
    }
    if (s_precomp_mutex != NULL) {
        /* Wipe pre-computed keys before destroying */
        xSemaphoreTake(s_precomp_mutex, portMAX_DELAY);
        sodium_memzero(s_precomp_pk, sizeof(s_precomp_pk));
        sodium_memzero(s_precomp_sk, sizeof(s_precomp_sk));
        s_precomp_ready = false;
        xSemaphoreGive(s_precomp_mutex);
        vSemaphoreDelete(s_precomp_mutex);
        s_precomp_mutex = NULL;
    }
    ESP_LOGI(TAG, "deinitialized");
}

/* ---- Internal: submit blocking request ---- */

static esp_err_t submit_request(pq_request_t *req, uint32_t timeout_ms) {
    if (s_request_queue == NULL || s_crypto_task == NULL) {
        ESP_LOGE(TAG, "crypto task not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create per-request completion semaphore */
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (done == NULL) {
        ESP_LOGE(TAG, "failed to create done semaphore");
        return ESP_ERR_NO_MEM;
    }
    req->done_sem = done;
    req->result = -1;

    /* Queue the request */
    if (xQueueSend(s_request_queue, req, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "request queue full");
        vSemaphoreDelete(done);
        return ESP_ERR_TIMEOUT;
    }

    /* Wait for completion */
    TickType_t wait = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(done, wait) != pdTRUE) {
        ESP_LOGE(TAG, "operation timed out after %lu ms", (unsigned long)timeout_ms);
        vSemaphoreDelete(done);
        return ESP_ERR_TIMEOUT;
    }

    vSemaphoreDelete(done);
    return (req->result == 0) ? ESP_OK : ESP_FAIL;
}

/* ---- Public API: blocking operations ---- */

esp_err_t pq_crypto_keygen(uint8_t *pk, uint8_t *sk, uint32_t timeout_ms) {
    pq_request_t req = {
        .op = PQ_OP_KEYGEN,
        .keygen = { .pk = pk, .sk = sk }
    };
    return submit_request(&req, timeout_ms);
}

esp_err_t pq_crypto_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk,
                         uint32_t timeout_ms) {
    pq_request_t req = {
        .op = PQ_OP_ENC,
        .enc = { .ct = ct, .ss = ss, .pk = pk }
    };
    return submit_request(&req, timeout_ms);
}

esp_err_t pq_crypto_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk,
                         uint32_t timeout_ms) {
    pq_request_t req = {
        .op = PQ_OP_DEC,
        .dec = { .ss = ss, .ct = ct, .sk = sk }
    };
    return submit_request(&req, timeout_ms);
}

/* ---- Public API: pre-computation ---- */

esp_err_t pq_crypto_precompute_keypair(void) {
    if (s_precomp_ready) {
        return ESP_ERR_INVALID_STATE;  /* Already have one */
    }
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    pq_request_t req = {
        .op = PQ_OP_PRECOMPUTE,
        .done_sem = NULL,  /* Non-blocking - no completion signal */
    };

    if (xQueueSend(s_request_queue, &req, 0) != pdTRUE) {
        ESP_LOGW(TAG, "precompute: queue full, will retry later");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "precompute: keygen queued");
    return ESP_OK;
}

bool pq_crypto_get_precomputed(uint8_t *pk, uint8_t *sk) {
    if (!s_precomp_ready || s_precomp_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_precomp_mutex, portMAX_DELAY);
    if (!s_precomp_ready) {
        xSemaphoreGive(s_precomp_mutex);
        return false;
    }

    memcpy(pk, s_precomp_pk, SNTRUP761_PUBLICKEYBYTES);
    memcpy(sk, s_precomp_sk, SNTRUP761_SECRETKEYBYTES);

    /* Wipe and mark consumed */
    sodium_memzero(s_precomp_pk, sizeof(s_precomp_pk));
    sodium_memzero(s_precomp_sk, sizeof(s_precomp_sk));
    s_precomp_ready = false;

    xSemaphoreGive(s_precomp_mutex);

    ESP_LOGD(TAG, "precomputed keypair consumed");
    return true;
}

bool pq_crypto_has_precomputed(void) {
    return s_precomp_ready;
}

void pq_crypto_get_stats(uint32_t *stack_free, uint32_t *sram_free,
                          bool *precomp_ready) {
    if (stack_free != NULL) {
        if (s_crypto_task != NULL) {
            *stack_free = uxTaskGetStackHighWaterMark(s_crypto_task)
                          * sizeof(StackType_t);
        } else {
            *stack_free = 0;
        }
    }
    if (sram_free != NULL) {
        /* Report PSRAM free since that's where crypto stack lives */
        *sram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }
    if (precomp_ready != NULL) {
        *precomp_ready = s_precomp_ready;
    }
}
