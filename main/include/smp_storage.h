/**
 * SimpleGo - smp_storage.h
 * Persistent Storage Module (NVS + SD Card)
 * v0.1.17-alpha
 *
 * Architecture:
 *   NVS (internal flash)  → Crypto-critical data (Ratchet State, Queue Creds, Keys)
 *   SD Card (external)    → Message History (AES-encrypted)
 *
 * CRITICAL - Evgeny's Golden Rule (highest authority):
 *   "Generate key → Persist to flash → THEN send → If response lost → Retry with SAME key"
 *   All state-modifying keys MUST be persisted BEFORE any network send.
 *   Use smp_storage_save_blob_sync() for this pattern.
 *
 * Design Decisions:
 *   - No SQLite: Too heavy for ESP32 (300-500KB code, RAM-hungry)
 *   - NVS Key-Value store: Native ESP-IDF, wear-leveling built-in, perfect for blobs
 *   - SD Card: Simple encrypted files, portable between devices
 *   - Lazy Loading: Ratchet state loaded on demand, not at boot (matches SimpleX pattern)
 *
 * NVS Key Naming Convention:
 *   "ratchet_XX"   → Ratchet state blob for contact index XX
 *   "queue_rcv_XX" → Receive queue credentials for contact XX
 *   "queue_snd_XX" → Send queue credentials for contact XX
 *   "skip_XX_YYYY" → Skipped message key (XX=contact, YYYY=counter)
 *   "contacts"     → Contact database metadata
 *   "pending_key"  → Write-before-send: key awaiting network confirmation
 *
 * SD Card Path Convention:
 *   /sdcard/simplego/msgs/XX/YYYYYYYY.bin  → Message (XX=contact, Y=msg_id)
 *   /sdcard/simplego/export/               → Exportable data
 *
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_STORAGE_H
#define SMP_STORAGE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============== Constants ==============

#define SMP_STORAGE_NVS_NAMESPACE   "simplego"      // Max 15 chars for NVS
#define SMP_STORAGE_SD_MOUNT_POINT  "/sdcard"
#define SMP_STORAGE_SD_BASE_DIR     "/sdcard/simplego"
#define SMP_STORAGE_SD_MSG_DIR      "/sdcard/simplego/msgs"

#define SMP_STORAGE_MAX_KEY_LEN     15              // NVS key length limit
#define SMP_STORAGE_MAX_BLOB_SIZE   4096            // Max single NVS blob (safety limit)

// ============== Lifecycle ==============

/**
 * Initialize storage subsystem — Phase 1: NVS only.
 * No SPI access, no SD card, no bus conflicts.
 * Call BEFORE display init, after nvs_flash_init().
 *
 * @return ESP_OK on success (NVS ready)
 */
esp_err_t smp_storage_init(void);

/**
 * Initialize SD card — Phase 2: Mount on existing SPI bus.
 * Shares SPI bus with display. Call AFTER display init.
 * Non-fatal if no SD card inserted.
 *
 * @return ESP_OK on success (SD mounted), or error code
 */
esp_err_t smp_storage_init_sd(void);

/**
 * Deinitialize storage subsystem.
 * - Closes NVS handle
 * - Unmounts SD card if mounted
 */
void smp_storage_deinit(void);

/**
 * Check if SD card is available and mounted.
 *
 * @return true if SD card is ready for read/write
 */
bool smp_storage_sd_available(void);

// ============== NVS Backend (crypto-critical data) ==============

/**
 * Save a binary blob to NVS.
 * Calls nvs_commit() internally.
 *
 * @param key   NVS key (max 15 chars)
 * @param data  Pointer to data
 * @param len   Data length in bytes
 * @return ESP_OK on success
 */
esp_err_t smp_storage_save_blob(const char *key, const void *data, size_t len);

/**
 * Load a binary blob from NVS.
 *
 * @param key      NVS key
 * @param buf      Output buffer
 * @param buf_len  Buffer capacity
 * @param out_len  Actual bytes read (can be NULL if not needed)
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t smp_storage_load_blob(const char *key, void *buf, size_t buf_len, size_t *out_len);

/**
 * Delete a key from NVS.
 *
 * @param key  NVS key to delete
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t smp_storage_delete(const char *key);

/**
 * Check if a key exists in NVS.
 *
 * @param key  NVS key to check
 * @return true if key exists and has data
 */
bool smp_storage_exists(const char *key);

// ============== Write-Before-Send (Evgeny's Idempotency Pattern) ==============

/**
 * Save a binary blob to NVS with SYNCHRONOUS flush guarantee.
 *
 * This function guarantees that data is physically written to flash
 * BEFORE it returns. Use this BEFORE any network operation that
 * sends keys or modifies server state.
 *
 * Pattern:
 *   1. Generate key/state
 *   2. smp_storage_save_blob_sync("pending_key", key, len)  ← MUST succeed
 *   3. Send network command
 *   4. On success: delete "pending_key" or update final state
 *   5. On failure/reboot: load "pending_key" and retry with SAME key
 *
 * Includes verify-read-back for critical data integrity.
 *
 * @param key   NVS key (max 15 chars)
 * @param data  Pointer to data
 * @param len   Data length in bytes
 * @return ESP_OK on success (data verified in flash)
 */
esp_err_t smp_storage_save_blob_sync(const char *key, const void *data, size_t len);

// ============== SD Card Backend (Message History) ==============

/**
 * Write data to a file on SD card.
 *
 * @param path  Full path (e.g. "/sdcard/simplego/msgs/00/00000001.bin")
 * @param data  Pointer to data
 * @param len   Data length
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if SD not available
 */
esp_err_t smp_storage_sd_write(const char *path, const void *data, size_t len);

/**
 * Read data from a file on SD card.
 *
 * @param path     Full path
 * @param buf      Output buffer
 * @param buf_len  Buffer capacity
 * @param out_len  Actual bytes read (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if file/SD not available
 */
esp_err_t smp_storage_sd_read(const char *path, void *buf, size_t buf_len, size_t *out_len);

/**
 * Delete a file from SD card.
 *
 * @param path  Full path
 * @return ESP_OK on success
 */
esp_err_t smp_storage_sd_delete(const char *path);

/**
 * Check if a file exists on SD card.
 *
 * @param path  Full path
 * @return true if file exists
 */
bool smp_storage_sd_file_exists(const char *path);

// ============== Storage Info & Diagnostics ==============

/**
 * Log storage diagnostics: NVS usage, SD card info, struct sizes.
 * Call during boot or on demand for debugging.
 */
void smp_storage_print_info(void);

/**
 * Run storage self-test (NVS roundtrip + SD roundtrip + timing).
 * Logs PASS/FAIL for each test.
 *
 * @return ESP_OK if all tests pass
 */
esp_err_t smp_storage_self_test(void);

// ============== Display Name (Session 43) ==============

/**
 * Read the user's display name from NVS.
 * Returns "SimpleGo" if no name has been configured.
 *
 * @param buf       Output buffer
 * @param buf_size  Buffer capacity (recommend 32)
 */
void storage_get_display_name(char *buf, size_t buf_size);

/**
 * Save the user's display name to NVS.
 * Validates input: max 31 chars, no quotes or backslashes (JSON safety).
 *
 * @param name  Display name string (null-terminated)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if validation fails
 */
esp_err_t storage_set_display_name(const char *name);

/**
 * Check if a display name has been configured (for first-boot detection).
 *
 * @return true if NVS key "user_name" exists
 */
bool storage_has_display_name(void);

// ============== Timezone Offset (Session 48) ==============

/** Global UTC offset in hours (-12 to +14). Display only, internal stays UTC. */
extern int8_t g_tz_offset_hours;

/**
 * Load timezone offset from NVS into g_tz_offset_hours.
 * Sets 0 (UTC) if no stored value. Call at boot before first clock display.
 */
void storage_load_tz_offset(void);

/**
 * Save timezone offset to NVS and update g_tz_offset_hours.
 * @param offset  Hours from UTC (-12 to +14)
 */
void storage_set_tz_offset(int8_t offset);

// ============== Future: Typed Convenience Functions (Auftrag 50b) ==============

// esp_err_t smp_storage_save_ratchet(uint8_t contact_idx, const void *blob, size_t len);
// esp_err_t smp_storage_load_ratchet(uint8_t contact_idx, void *buf, size_t buf_len, size_t *out_len);
// esp_err_t smp_storage_save_message(uint8_t contact_idx, uint32_t msg_id, const void *data, size_t len);
// esp_err_t smp_storage_load_messages(uint8_t contact_idx, uint32_t from_id, uint32_t count, ...);

#endif // SMP_STORAGE_H
