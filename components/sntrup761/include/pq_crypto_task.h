/*
 * pq_crypto_task.h - Dedicated crypto task for post-quantum operations
 *
 * sntrup761 key generation needs ~60-70 KB stack - far more than any
 * normal FreeRTOS task in SimpleGo. This module provides a dedicated
 * task with 80 KB internal SRAM stack on Core 0 that serializes all
 * PQ crypto operations.
 *
 * Usage:
 *   pq_crypto_task_init();              // Start task at boot
 *   pq_crypto_keygen(pk, sk, timeout);  // Blocking keygen
 *   pq_crypto_enc(ct, ss, pk, timeout); // Blocking encap
 *   pq_crypto_dec(ss, ct, sk, timeout); // Blocking decap
 *
 * All blocking calls are safe from any task - they send a request
 * to the crypto task and wait for the result via a semaphore.
 *
 * Background pre-computation:
 *   pq_crypto_precompute_keypair();     // Trigger background keygen
 *   pq_crypto_get_precomputed(pk, sk);  // Get pre-computed keypair
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#ifndef PQ_CRYPTO_TASK_H
#define PQ_CRYPTO_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Task configuration ---- */

#define PQ_CRYPTO_TASK_STACK_SIZE  (80 * 1024)   /* 80 KB internal SRAM */
#define PQ_CRYPTO_TASK_PRIORITY    6             /* Above app (5), below network (7) */
#define PQ_CRYPTO_TASK_CORE        0             /* Core 0 with network task */
#define PQ_CRYPTO_QUEUE_DEPTH      4             /* Max pending requests */

/* ---- Initialization ---- */

/**
 * Initialize and start the crypto task.
 * Must be called once at boot, before any crypto operations.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if SRAM insufficient
 */
esp_err_t pq_crypto_task_init(void);

/**
 * Stop and destroy the crypto task.
 * Waits for any in-flight operation to complete.
 */
void pq_crypto_task_deinit(void);

/* ---- Blocking operations (safe from any task) ---- */

/**
 * Generate a sntrup761 keypair (blocking).
 *
 * @param pk         Output: public key (1158 bytes)
 * @param sk         Output: secret key (1763 bytes)
 * @param timeout_ms Maximum wait time in ms (0 = no timeout)
 * @return ESP_OK, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_STATE
 */
esp_err_t pq_crypto_keygen(uint8_t *pk, uint8_t *sk, uint32_t timeout_ms);

/**
 * Encapsulate (blocking).
 *
 * @param ct         Output: ciphertext (1039 bytes)
 * @param ss         Output: shared secret (32 bytes)
 * @param pk         Input:  recipient's public key (1158 bytes)
 * @param timeout_ms Maximum wait time in ms (0 = no timeout)
 * @return ESP_OK, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_STATE
 */
esp_err_t pq_crypto_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk,
                         uint32_t timeout_ms);

/**
 * Decapsulate (blocking).
 *
 * @param ss         Output: shared secret (32 bytes)
 * @param ct         Input:  ciphertext (1039 bytes)
 * @param sk         Input:  own secret key (1763 bytes)
 * @param timeout_ms Maximum wait time in ms (0 = no timeout)
 * @return ESP_OK, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_STATE
 */
esp_err_t pq_crypto_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk,
                         uint32_t timeout_ms);

/* ---- Background pre-computation (Aufgabe 3) ---- */

/**
 * Trigger background keypair generation.
 * Non-blocking - queues the request and returns immediately.
 * If a pre-computed keypair already exists, this is a no-op.
 *
 * @return ESP_OK if queued, ESP_ERR_INVALID_STATE if keypair already available
 */
esp_err_t pq_crypto_precompute_keypair(void);

/**
 * Retrieve pre-computed keypair (non-blocking).
 *
 * @param pk  Output: public key (1158 bytes)
 * @param sk  Output: secret key (1763 bytes)
 * @return true if keypair was available (consumed), false if not ready
 */
bool pq_crypto_get_precomputed(uint8_t *pk, uint8_t *sk);

/**
 * Check if a pre-computed keypair is ready.
 */
bool pq_crypto_has_precomputed(void);

/**
 * Get memory usage report for diagnostics.
 *
 * @param stack_free     Output: crypto task stack bytes remaining
 * @param sram_free      Output: total free internal SRAM
 * @param precomp_ready  Output: whether pre-computed keypair is available
 */
void pq_crypto_get_stats(uint32_t *stack_free, uint32_t *sram_free,
                          bool *precomp_ready);

#ifdef __cplusplus
}
#endif

#endif /* PQ_CRYPTO_TASK_H */
