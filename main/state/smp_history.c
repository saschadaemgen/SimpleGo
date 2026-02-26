/**
 * SimpleGo - smp_history.c
 * Encrypted Chat History on SD Card
 *
 * Architecture:
 *   - Master key in NVS, per-contact keys via HKDF-SHA256
 *   - AES-256-GCM per-message encryption
 *   - Deterministic nonce: slot_index + msg_index (NEVER re-encrypt!)
 *   - Append-only file format with 32-byte header
 *   - Delivery status tracked in unencrypted header (no nonce reuse)
 *   - SD mutex protects ALL file operations against concurrent access
 *
 * All functions MUST be called from App Task (Internal SRAM stack)
 * because NVS writes crash with PSRAM stack (SPI Flash cache conflict).
 *
 * Session 37 — Auftrag von Prinzessin Mausi
 * Session 37b — SD Mutex + DMA hint (Mausi Architektur-Entscheidung)
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_history.h"
#include "smp_storage.h"       // for smp_storage_sd_available()
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tdeck_lvgl.h"        // Session 37b: LVGL recursive mutex = SPI2 bus lock
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "HISTORY";

static uint8_t s_master_key[HISTORY_MASTER_KEY_LEN];
static bool s_initialized = false;

/*
 * SPI2 Bus Lock — Display and SD card share SPI2_HOST.
 *
 * We use the LVGL recursive mutex as the SPI2 bus serialization mechanism:
 *   - LVGL task holds it during lv_timer_handler() → flush_cb → SPI2 display
 *   - App Task takes it before SD file I/O → blocks LVGL rendering
 *   - LVGL context (on_contact_click → smp_history_delete): recursive take OK
 *
 * This replaces the original sd_mutex. No separate mutex needed because
 * ALL SPI2 contention goes through the same LVGL lock.
 *
 * If the T-Deck SD card is ever moved to SPI3 (separate bus), this can
 * be reverted to a simple standalone mutex.
 *
 * Session 37b: Root cause was spi_tx_color failures from bus contention.
 */

/* Timeout for bus lock — generous for slow SD + display flush overlap */
#define SPI2_BUS_TIMEOUT_MS  3000

static inline bool sd_lock(void)
{
    return tdeck_lvgl_lock(SPI2_BUS_TIMEOUT_MS);
}

static inline void sd_unlock(void)
{
    tdeck_lvgl_unlock();
}

/* ============================================================
 * Helper: Build file path for a contact slot.
 * Output: "/sdcard/simplego/msgs/chat_XX.bin"
 * ============================================================ */
static void history_build_path(uint8_t slot, char *path, size_t path_len)
{
    snprintf(path, path_len, "/sdcard/simplego/msgs/chat_%02X.bin", slot);
}

/* ============================================================
 * Helper: Derive per-contact key from master key using HKDF-SHA256.
 * Salt: "simplego-history"
 * Info: single byte slot_index
 * ============================================================ */
static esp_err_t history_derive_key(uint8_t slot, uint8_t *out_key)
{
    const uint8_t salt[] = "simplego-history";
    uint8_t info[1] = { slot };

    int ret = mbedtls_hkdf(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        salt, sizeof(salt) - 1,    // salt (without null terminator)
        s_master_key, HISTORY_MASTER_KEY_LEN,  // input key material
        info, 1,                   // info = slot index
        out_key, HISTORY_MASTER_KEY_LEN  // output: 32 bytes
    );

    if (ret != 0) {
        ESP_LOGE(TAG, "HKDF failed: -0x%04X", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ============================================================
 * Helper: Build GCM nonce (12 bytes):
 * Bytes 0-1:  slot_index (big-endian uint16)
 * Bytes 2-7:  reserved (0x00)
 * Bytes 8-11: msg_index (big-endian uint32)
 *
 * msg_index stored as uint32 gives headroom for >65535 messages
 * even though header.msg_count is currently uint16.
 * ============================================================ */
static void history_build_nonce(uint8_t slot, uint16_t msg_index, uint8_t *iv)
{
    memset(iv, 0, HISTORY_GCM_IV_LEN);
    iv[0] = 0x00;
    iv[1] = slot;
    iv[8]  = (uint8_t)((msg_index >> 24) & 0xFF);
    iv[9]  = (uint8_t)((msg_index >> 16) & 0xFF);
    iv[10] = (uint8_t)((msg_index >> 8) & 0xFF);
    iv[11] = (uint8_t)(msg_index & 0xFF);
}

/* ============================================================
 * Init: Load or generate master key from NVS, create SD mutex
 * ============================================================ */
esp_err_t smp_history_init(void)
{
    if (s_initialized) return ESP_OK;

    // Session 37b: No mutex creation needed — we use tdeck_lvgl_lock()
    // which is already initialized before smp_history_init() is called.

    // Load master key from NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("simplego", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t key_len = HISTORY_MASTER_KEY_LEN;
    err = nvs_get_blob(nvs, HISTORY_NVS_KEY, s_master_key, &key_len);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First start: generate master key
        ESP_LOGI(TAG, "Generating new master key");
        esp_fill_random(s_master_key, HISTORY_MASTER_KEY_LEN);

        err = nvs_set_blob(nvs, HISTORY_NVS_KEY, s_master_key, HISTORY_MASTER_KEY_LEN);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS write master key failed: %s", esp_err_to_name(err));
            nvs_close(nvs);
            return err;
        }
        err = nvs_commit(nvs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
            nvs_close(nvs);
            return err;
        }
        ESP_LOGI(TAG, "Master key generated and persisted");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS read master key failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    } else {
        ESP_LOGI(TAG, "Master key loaded from NVS");
    }

    nvs_close(nvs);

    // Ensure message directory exists (mutex-protected)
    sd_lock();
    mkdir("/sdcard/simplego", 0755);
    mkdir("/sdcard/simplego/msgs", 0755);
    sd_unlock();

    s_initialized = true;
    return ESP_OK;
}

/* ============================================================
 * Append: Encrypt and append one message to the history file
 * ============================================================ */
esp_err_t smp_history_append(uint8_t slot, const history_message_t *msg)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!smp_storage_sd_available()) return ESP_ERR_NOT_FOUND;
    if (slot >= 128 || !msg) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    // Take SD mutex for entire file operation
    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (append slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    // Read or create header
    history_header_t header;
    bool new_file = false;
    FILE *f = fopen(path, "rb");
    if (f) {
        if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
            ESP_LOGE(TAG, "Corrupt header in %s", path);
            fclose(f);
            sd_unlock();
            return ESP_FAIL;
        }
        fclose(f);
        // Validate magic
        if (header.magic[0] != HISTORY_MAGIC_0 || header.magic[1] != HISTORY_MAGIC_1 ||
            header.magic[2] != HISTORY_MAGIC_2 || header.magic[3] != HISTORY_MAGIC_3) {
            ESP_LOGE(TAG, "Invalid magic in %s", path);
            sd_unlock();
            return ESP_FAIL;
        }
    } else {
        // New file
        new_file = true;
        memset(&header, 0, sizeof(header));
        header.magic[0] = HISTORY_MAGIC_0;
        header.magic[1] = HISTORY_MAGIC_1;
        header.magic[2] = HISTORY_MAGIC_2;
        header.magic[3] = HISTORY_MAGIC_3;
        header.version = HISTORY_VERSION;
        header.slot_index = slot;
        header.msg_count = 0;
        header.unread_count = 0;
        header.last_delivered_idx = 0;
    }

    // Derive per-contact key (no SD access, no mutex needed for this)
    uint8_t contact_key[HISTORY_MASTER_KEY_LEN];
    esp_err_t err = history_derive_key(slot, contact_key);
    if (err != ESP_OK) {
        sd_unlock();
        return err;
    }

    // Build plaintext payload: direction(1) + timestamp(8) + text_len(2) + text(N)
    uint16_t text_len = msg->text_len;
    if (text_len > HISTORY_MAX_TEXT) text_len = HISTORY_MAX_TEXT;
    size_t plaintext_len = 1 + 8 + 2 + text_len;
    uint8_t *plaintext = malloc(plaintext_len);
    if (!plaintext) {
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;
    plaintext[offset++] = msg->direction;
    // timestamp big-endian
    int64_t ts = msg->timestamp;
    for (int i = 7; i >= 0; i--) {
        plaintext[offset++] = (uint8_t)((ts >> (i * 8)) & 0xFF);
    }
    plaintext[offset++] = (uint8_t)((text_len >> 8) & 0xFF);
    plaintext[offset++] = (uint8_t)(text_len & 0xFF);
    memcpy(&plaintext[offset], msg->text, text_len);

    // Build nonce
    uint8_t iv[HISTORY_GCM_IV_LEN];
    history_build_nonce(slot, header.msg_count, iv);

    // Encrypt with AES-256-GCM
    uint8_t *ciphertext = malloc(plaintext_len);
    uint8_t tag[HISTORY_GCM_TAG_LEN];
    if (!ciphertext) {
        free(plaintext);
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES,
                                  contact_key, 256);
    if (ret != 0) {
        ESP_LOGE(TAG, "GCM setkey failed: -0x%04X", (unsigned)-ret);
        mbedtls_gcm_free(&gcm);
        free(plaintext);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_FAIL;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len,
                                     iv, HISTORY_GCM_IV_LEN,
                                     NULL, 0,  // no AAD
                                     plaintext, ciphertext,
                                     HISTORY_GCM_TAG_LEN, tag);
    mbedtls_gcm_free(&gcm);
    free(plaintext);

    if (ret != 0) {
        ESP_LOGE(TAG, "GCM encrypt failed: -0x%04X", (unsigned)-ret);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_FAIL;
    }

    // Build record: record_len(2) + iv(12) + ciphertext(N) + tag(16)
    uint16_t record_len = 2 + HISTORY_GCM_IV_LEN + plaintext_len + HISTORY_GCM_TAG_LEN;

    // Write to file (still inside SD mutex)
    f = fopen(path, new_file ? "wb" : "r+b");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s for writing", path);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_FAIL;
    }

    if (new_file) {
        // Write header first
        fwrite(&header, 1, HISTORY_HEADER_SIZE, f);
    }

    // Seek to end for append
    fseek(f, 0, SEEK_END);

    // Write record
    uint8_t rec_len_bytes[2] = {
        (uint8_t)((record_len >> 8) & 0xFF),
        (uint8_t)(record_len & 0xFF)
    };
    fwrite(rec_len_bytes, 1, 2, f);
    fwrite(iv, 1, HISTORY_GCM_IV_LEN, f);
    fwrite(ciphertext, 1, plaintext_len, f);
    fwrite(tag, 1, HISTORY_GCM_TAG_LEN, f);

    free(ciphertext);

    // Update header counters
    header.msg_count++;
    if (msg->direction == HISTORY_DIR_RECEIVED) {
        header.unread_count++;
    }

    // Seek back to header and overwrite
    fseek(f, 0, SEEK_SET);
    fwrite(&header, 1, HISTORY_HEADER_SIZE, f);

    fflush(f);
    fclose(f);

    // Release SD mutex
    sd_unlock();

    ESP_LOGI(TAG, "Appended msg #%u to slot %u (%u bytes)",
             header.msg_count - 1, slot, record_len);

    // Clear derived key from stack
    memset(contact_key, 0, sizeof(contact_key));

    return ESP_OK;
}

/* ============================================================
 * Load Recent: Read and decrypt the last N messages from file.
 * Delivery status reconstructed from header.last_delivered_idx.
 * ============================================================ */
esp_err_t smp_history_load_recent(uint8_t slot, history_message_t *out,
                                   int count, int *loaded)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!smp_storage_sd_available()) return ESP_ERR_NOT_FOUND;
    if (slot >= 128 || !out || !loaded) return ESP_ERR_INVALID_ARG;

    *loaded = 0;
    char path[64];
    history_build_path(slot, path, sizeof(path));

    // Take SD mutex for entire read+decrypt operation
    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (load slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        sd_unlock();
        return ESP_OK;  // No history yet -- not an error
    }

    // Read header
    history_header_t header;
    if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
        fclose(f);
        sd_unlock();
        return ESP_FAIL;
    }

    if (header.msg_count == 0) {
        fclose(f);
        sd_unlock();
        return ESP_OK;
    }

    // Derive key (pure computation, but keep inside mutex for simplicity)
    uint8_t contact_key[HISTORY_MASTER_KEY_LEN];
    esp_err_t err = history_derive_key(slot, contact_key);
    if (err != ESP_OK) {
        fclose(f);
        sd_unlock();
        return err;
    }

    // Determine start index (skip older messages)
    int start_idx = 0;
    if (header.msg_count > count) {
        start_idx = header.msg_count - count;
    }
    int to_load = header.msg_count - start_idx;
    if (to_load > count) to_load = count;

    // Seek forward through records to reach start_idx
    fseek(f, HISTORY_HEADER_SIZE, SEEK_SET);

    for (int i = 0; i < start_idx; i++) {
        uint8_t len_bytes[2];
        if (fread(len_bytes, 1, 2, f) != 2) {
            ESP_LOGE(TAG, "Unexpected EOF skipping record %d", i);
            fclose(f);
            sd_unlock();
            memset(contact_key, 0, sizeof(contact_key));
            return ESP_FAIL;
        }
        uint16_t rec_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];
        // Skip rest of record (rec_len includes the 2 length bytes)
        fseek(f, rec_len - 2, SEEK_CUR);
    }

    // Now read and decrypt the desired records
    mbedtls_gcm_context gcm;

    for (int i = 0; i < to_load; i++) {
        uint16_t msg_idx = start_idx + i;

        // Read record_len
        uint8_t len_bytes[2];
        if (fread(len_bytes, 1, 2, f) != 2) break;
        uint16_t rec_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];

        // Read IV
        uint8_t iv[HISTORY_GCM_IV_LEN];
        if (fread(iv, 1, HISTORY_GCM_IV_LEN, f) != HISTORY_GCM_IV_LEN) break;

        // Ciphertext length = rec_len - 2(len) - 12(iv) - 16(tag)
        size_t ct_len = rec_len - 2 - HISTORY_GCM_IV_LEN - HISTORY_GCM_TAG_LEN;
        if (ct_len > HISTORY_MAX_TEXT + 11) {  // 1+8+2+text
            ESP_LOGE(TAG, "Record %d too large: %zu", msg_idx, ct_len);
            fseek(f, ct_len + HISTORY_GCM_TAG_LEN, SEEK_CUR);
            continue;
        }

        uint8_t *ciphertext = malloc(ct_len);
        uint8_t tag[HISTORY_GCM_TAG_LEN];
        if (!ciphertext) break;

        if (fread(ciphertext, 1, ct_len, f) != ct_len) { free(ciphertext); break; }
        if (fread(tag, 1, HISTORY_GCM_TAG_LEN, f) != HISTORY_GCM_TAG_LEN) { free(ciphertext); break; }

        // Decrypt
        uint8_t *plaintext = malloc(ct_len);
        if (!plaintext) { free(ciphertext); break; }

        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, contact_key, 256);

        int ret = mbedtls_gcm_auth_decrypt(&gcm, ct_len,
                                            iv, HISTORY_GCM_IV_LEN,
                                            NULL, 0,
                                            tag, HISTORY_GCM_TAG_LEN,
                                            ciphertext, plaintext);
        mbedtls_gcm_free(&gcm);
        free(ciphertext);

        if (ret != 0) {
            ESP_LOGE(TAG, "GCM decrypt failed for msg %d: -0x%04X", msg_idx, (unsigned)-ret);
            free(plaintext);
            continue;
        }

        // Parse plaintext into history_message_t
        history_message_t *m = &out[*loaded];
        memset(m, 0, sizeof(history_message_t));

        size_t p = 0;
        m->direction = plaintext[p++];
        m->delivery_status = 0;  // Default: sent (v)

        // Reconstruct delivery status from header high-water mark
        if (m->direction == HISTORY_DIR_SENT && msg_idx <= header.last_delivered_idx) {
            m->delivery_status = 1;  // delivered (vv)
        }

        m->timestamp = 0;
        for (int b = 7; b >= 0; b--) {
            m->timestamp |= ((int64_t)plaintext[p++]) << (b * 8);
        }

        m->text_len = ((uint16_t)plaintext[p] << 8) | plaintext[p + 1];
        p += 2;

        if (m->text_len > HISTORY_MAX_TEXT) m->text_len = HISTORY_MAX_TEXT;
        memcpy(m->text, &plaintext[p], m->text_len);
        m->text[m->text_len] = '\0';  // Null-terminate

        free(plaintext);
        (*loaded)++;
    }

    fclose(f);

    // Release SD mutex
    sd_unlock();

    memset(contact_key, 0, sizeof(contact_key));

    ESP_LOGI(TAG, "Loaded %d messages from slot %u", *loaded, slot);
    return ESP_OK;
}

/* ============================================================
 * Get Counts: Read header without loading messages
 * ============================================================ */
esp_err_t smp_history_get_counts(uint8_t slot, uint16_t *total, uint16_t *unread)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (slot >= 128) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (get_counts slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        sd_unlock();
        if (total) *total = 0;
        if (unread) *unread = 0;
        return ESP_OK;
    }

    history_header_t header;
    if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
        fclose(f);
        sd_unlock();
        return ESP_FAIL;
    }
    fclose(f);
    sd_unlock();

    if (total) *total = header.msg_count;
    if (unread) *unread = header.unread_count;
    return ESP_OK;
}

/* ============================================================
 * Mark Read: Set unread_count to 0 in file header
 * ============================================================ */
esp_err_t smp_history_mark_read(uint8_t slot)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (slot >= 128) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (mark_read slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "r+b");
    if (!f) {
        sd_unlock();
        return ESP_OK;  // No file, nothing to do
    }

    history_header_t header;
    if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
        fclose(f);
        sd_unlock();
        return ESP_FAIL;
    }

    header.unread_count = 0;

    fseek(f, 0, SEEK_SET);
    fwrite(&header, 1, HISTORY_HEADER_SIZE, f);
    fflush(f);
    fclose(f);
    sd_unlock();

    return ESP_OK;
}

/* ============================================================
 * Update Delivered: Set delivery high-water mark in header.
 * All sent messages with index <= msg_index are "delivered" (vv).
 * Only the unencrypted header is modified -- no GCM nonce reuse.
 * ============================================================ */
esp_err_t smp_history_update_delivered(uint8_t slot, uint16_t msg_index)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (slot >= 128) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (update_delivered slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "r+b");
    if (!f) {
        sd_unlock();
        return ESP_OK;  // No file, nothing to do
    }

    history_header_t header;
    if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
        fclose(f);
        sd_unlock();
        return ESP_FAIL;
    }

    // Only update if new index is higher (monotonic)
    if (msg_index > header.last_delivered_idx) {
        header.last_delivered_idx = msg_index;

        fseek(f, 0, SEEK_SET);
        fwrite(&header, 1, HISTORY_HEADER_SIZE, f);
        fflush(f);

        ESP_LOGD(TAG, "Delivery mark updated: slot %u, idx %u", slot, msg_index);
    }

    fclose(f);
    sd_unlock();
    return ESP_OK;
}

/* ============================================================
 * Delete: Remove the entire history file for a contact slot
 * ============================================================ */
esp_err_t smp_history_delete(uint8_t slot)
{
    if (slot >= 128) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    sd_lock();
    if (remove(path) == 0) {
        ESP_LOGI(TAG, "Deleted history for slot %u", slot);
    } else {
        ESP_LOGD(TAG, "No history file for slot %u (already clean)", slot);
    }
    sd_unlock();

    return ESP_OK;
}
