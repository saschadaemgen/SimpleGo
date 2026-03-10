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
#include "esp_heap_caps.h"     // Session 40a: PSRAM allocation for raw record buffer
#include "esp_mac.h"           // SEC-05: Device-bound HKDF via chip MAC
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
 * Info: "simplego-slot-XX-AABBCCDDEEFF" (slot + chip MAC)
 *
 * SEC-05: The chip's unique MAC address binds derived keys to this
 * specific device. Same master key on a different ESP32-S3 produces
 * completely different per-contact keys, preventing SD card theft
 * from exposing chat history on another device.
 * ============================================================ */
static esp_err_t history_derive_key(uint8_t slot, uint8_t *out_key)
{
    const uint8_t salt[] = "simplego-history";

    /* Build device-bound info string: "simplego-slot-XX-AABBCCDDEEFF"
     * where XX is the hex slot index and AABBCCDDEEFF is the chip MAC. */
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    char info[48];
    int info_len = snprintf(info, sizeof(info),
                            "simplego-slot-%02x-%02x%02x%02x%02x%02x%02x",
                            slot,
                            mac[0], mac[1], mac[2],
                            mac[3], mac[4], mac[5]);

    int ret = mbedtls_hkdf(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        salt, sizeof(salt) - 1,    // salt (without null terminator)
        s_master_key, HISTORY_MASTER_KEY_LEN,  // input key material
        (const uint8_t *)info, (size_t)info_len,  // info = slot + device MAC
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
    ESP_LOGI(TAG, "History engine initialized (AES-256-GCM, device-bound HKDF)");
    return ESP_OK;
}

/* ============================================================
 * Append: Encrypt and append one message to the history file
 *
 * Session 40a: Crypto separated from SPI mutex.
 *   Existing file: Pass 1 (read header) → CPU crypto → Pass 2 (write)
 *   New file:      CPU crypto → single Pass (write header + record)
 *
 * NOTE: Between Pass 1 (read msg_count) and Pass 2 (write),
 * another append could theoretically change msg_count and cause
 * a GCM nonce collision. Currently safe: all history ops run on
 * single App Task. If ever multi-tasked, atomize nonce assignment
 * inside write mutex or use a sequence counter in PSRAM.
 * ============================================================ */
esp_err_t smp_history_append(uint8_t slot, const history_message_t *msg)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!smp_storage_sd_available()) return ESP_ERR_NOT_FOUND;
    if (slot >= 128 || !msg) return ESP_ERR_INVALID_ARG;

    char path[64];
    history_build_path(slot, path, sizeof(path));

    /* ---- Pass 1: Read header from SD (short mutex) ---- */
    history_header_t header;
    bool new_file = false;

    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (append pass1 slot %u)", slot);
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "rb");
    if (f) {
        if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
            ESP_LOGE(TAG, "Corrupt header in %s", path);
            fclose(f);
            sd_unlock();
            return ESP_FAIL;
        }
        fclose(f);

        /* Validate magic */
        if (header.magic[0] != HISTORY_MAGIC_0 || header.magic[1] != HISTORY_MAGIC_1 ||
            header.magic[2] != HISTORY_MAGIC_2 || header.magic[3] != HISTORY_MAGIC_3) {
            ESP_LOGE(TAG, "Invalid magic in %s", path);
            sd_unlock();
            return ESP_FAIL;
        }
    } else {
        /* New file: no header to read, initialize in memory */
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

    sd_unlock();
    /* ---- End Pass 1 ---- */

    /* ---- CPU Work: Key derivation + encryption (NO mutex) ---- */

    /* Derive per-contact key */
    uint8_t contact_key[HISTORY_MASTER_KEY_LEN];
    esp_err_t err = history_derive_key(slot, contact_key);
    if (err != ESP_OK) {
        return err;
    }

    /* Build plaintext payload: format depends on file version.
     * v1: direction(1) + timestamp(8) + text_len(2) + text(N)
     * v2: direction(1) + delivery_status(1) + timestamp(8) + text_len(2) + text(N)
     * Existing v1 files keep v1 format. Only new files use v2. */
    uint16_t text_len = msg->text_len;
    if (text_len > HISTORY_MAX_TEXT) text_len = HISTORY_MAX_TEXT;
    size_t plaintext_len;
    if (header.version >= 0x02) {
        plaintext_len = 1 + 1 + 8 + 2 + text_len;  // v2: +1 for delivery_status
    } else {
        plaintext_len = 1 + 8 + 2 + text_len;       // v1: original format
    }
    uint8_t *plaintext = malloc(plaintext_len);
    if (!plaintext) {
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;
    plaintext[offset++] = msg->direction;
    if (header.version >= 0x02) {
        plaintext[offset++] = msg->delivery_status;  // v2 only
    }
    /* timestamp big-endian */
    int64_t ts = msg->timestamp;
    for (int i = 7; i >= 0; i--) {
        plaintext[offset++] = (uint8_t)((ts >> (i * 8)) & 0xFF);
    }
    plaintext[offset++] = (uint8_t)((text_len >> 8) & 0xFF);
    plaintext[offset++] = (uint8_t)(text_len & 0xFF);
    memcpy(&plaintext[offset], msg->text, text_len);

    /* Build nonce from msg_count obtained in Pass 1 */
    uint8_t iv[HISTORY_GCM_IV_LEN];
    history_build_nonce(slot, header.msg_count, iv);

    /* Encrypt with AES-256-GCM */
    uint8_t *ciphertext = malloc(plaintext_len);
    uint8_t tag[HISTORY_GCM_TAG_LEN];
    if (!ciphertext) {
        free(plaintext);
        memset(contact_key, 0, sizeof(contact_key));
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
        return ESP_FAIL;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len,
                                     iv, HISTORY_GCM_IV_LEN,
                                     NULL, 0,  /* no AAD */
                                     plaintext, ciphertext,
                                     HISTORY_GCM_TAG_LEN, tag);
    mbedtls_gcm_free(&gcm);
    free(plaintext);

    if (ret != 0) {
        ESP_LOGE(TAG, "GCM encrypt failed: -0x%04X", (unsigned)-ret);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_FAIL;
    }

    /* Build record: record_len(2) + iv(12) + ciphertext(N) + tag(16) */
    uint16_t record_len = 2 + HISTORY_GCM_IV_LEN + plaintext_len + HISTORY_GCM_TAG_LEN;

    /* ---- End CPU Work ---- */

    /* ---- Pass 2: Write record to SD (short mutex) ---- */
    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (append pass2 slot %u)", slot);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_ERR_TIMEOUT;
    }

    f = fopen(path, new_file ? "wb" : "r+b");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s for writing", path);
        free(ciphertext);
        memset(contact_key, 0, sizeof(contact_key));
        sd_unlock();
        return ESP_FAIL;
    }

    if (new_file) {
        /* Write header first for new file */
        fwrite(&header, 1, HISTORY_HEADER_SIZE, f);
    }

    /* Seek to end for append */
    fseek(f, 0, SEEK_END);

    /* Write record */
    uint8_t rec_len_bytes[2] = {
        (uint8_t)((record_len >> 8) & 0xFF),
        (uint8_t)(record_len & 0xFF)
    };
    fwrite(rec_len_bytes, 1, 2, f);
    fwrite(iv, 1, HISTORY_GCM_IV_LEN, f);
    fwrite(ciphertext, 1, plaintext_len, f);
    fwrite(tag, 1, HISTORY_GCM_TAG_LEN, f);

    free(ciphertext);

    /* Update header counters */
    header.msg_count++;
    if (msg->direction == HISTORY_DIR_RECEIVED) {
        header.unread_count++;
    }

    /* Seek back to header and overwrite */
    fseek(f, 0, SEEK_SET);
    fwrite(&header, 1, HISTORY_HEADER_SIZE, f);

    fflush(f);
    fclose(f);

    sd_unlock();
    /* ---- End Pass 2 ---- */

    ESP_LOGI(TAG, "Appended msg #%u to slot %u (%u bytes)",
             header.msg_count - 1, slot, record_len);

    /* Clear derived key from stack */
    memset(contact_key, 0, sizeof(contact_key));

    return ESP_OK;
}

/* ============================================================
 * Load Recent: Read and decrypt the last N messages from file.
 * Delivery status reconstructed from header.last_delivered_idx.
 *
 * Session 40a: Crypto separated from SPI mutex.
 *   CPU:    derive_key (no mutex)
 *   Pass 1: sd_lock → read header + skip + read ALL raw records → sd_unlock
 *   CPU:    decrypt all records, parse, truncate text (no mutex)
 *   Free:   PSRAM buffer
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

    /* ---- CPU Work: Derive key (NO mutex) ---- */
    uint8_t contact_key[HISTORY_MASTER_KEY_LEN];
    esp_err_t err = history_derive_key(slot, contact_key);
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Pass 1: Read header + raw records from SD (single mutex) ---- */
    if (!sd_lock()) {
        ESP_LOGE(TAG, "SD mutex timeout (load slot %u)", slot);
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_ERR_TIMEOUT;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        sd_unlock();
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_OK;  /* No history yet -- not an error */
    }

    /* Read header */
    history_header_t header;
    if (fread(&header, 1, HISTORY_HEADER_SIZE, f) != HISTORY_HEADER_SIZE) {
        fclose(f);
        sd_unlock();
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_FAIL;
    }

    if (header.msg_count == 0) {
        fclose(f);
        sd_unlock();
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_OK;
    }

    /* Determine start index (skip older messages) */
    int start_idx = 0;
    if (header.msg_count > count) {
        start_idx = header.msg_count - count;
    }
    int to_load = header.msg_count - start_idx;
    if (to_load > count) to_load = count;

    /* Allocate PSRAM buffer for raw encrypted records.
     * Worst case per record: 2(len) + 12(IV) + 11(hdr) + HISTORY_MAX_PAYLOAD + 16(tag)
     * Old records (pre-40a) may be up to 4096+11 bytes payload.
     * Using HISTORY_MAX_PAYLOAD for safety margin with any stored records. */
    size_t worst_per_record = 2 + HISTORY_GCM_IV_LEN + 11 + HISTORY_MAX_PAYLOAD + HISTORY_GCM_TAG_LEN;
    size_t buf_alloc = (size_t)to_load * worst_per_record;
    uint8_t *raw_buf = heap_caps_malloc(buf_alloc, MALLOC_CAP_SPIRAM);
    if (!raw_buf) {
        ESP_LOGE(TAG, "PSRAM alloc failed for %u bytes (load %d records)",
                 (unsigned)buf_alloc, to_load);
        fclose(f);
        sd_unlock();
        memset(contact_key, 0, sizeof(contact_key));
        return ESP_ERR_NO_MEM;
    }

    /* Seek forward through records to reach start_idx */
    fseek(f, HISTORY_HEADER_SIZE, SEEK_SET);

    for (int i = 0; i < start_idx; i++) {
        uint8_t len_bytes[2];
        if (fread(len_bytes, 1, 2, f) != 2) {
            ESP_LOGE(TAG, "Unexpected EOF skipping record %d", i);
            fclose(f);
            sd_unlock();
            heap_caps_free(raw_buf);
            memset(contact_key, 0, sizeof(contact_key));
            return ESP_FAIL;
        }
        uint16_t rec_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];
        /* Skip rest of record (rec_len includes the 2 length bytes) */
        fseek(f, rec_len - 2, SEEK_CUR);
    }

    /* Read ALL raw records into PSRAM buffer */
    size_t offsets[HISTORY_MAX_LOAD];  /* Stack array: record offsets within raw_buf */
    int records_read = 0;
    size_t buf_used = 0;

    for (int i = 0; i < to_load && i < HISTORY_MAX_LOAD; i++) {
        /* Read record_len */
        uint8_t len_bytes[2];
        if (fread(len_bytes, 1, 2, f) != 2) {
            ESP_LOGW(TAG, "EOF reading record %d of %d", i, to_load);
            break;
        }
        uint16_t rec_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];
        size_t data_len = rec_len - 2;  /* IV + ciphertext + tag */

        /* Safety check: record fits in remaining buffer */
        if (buf_used + 2 + data_len > buf_alloc) {
            ESP_LOGE(TAG, "Record %d exceeds buffer: need %zu, have %zu",
                     i, buf_used + 2 + data_len, buf_alloc);
            break;
        }

        offsets[i] = buf_used;

        /* Store len_bytes + rest of record in buffer */
        raw_buf[buf_used++] = len_bytes[0];
        raw_buf[buf_used++] = len_bytes[1];

        if (fread(&raw_buf[buf_used], 1, data_len, f) != data_len) {
            ESP_LOGW(TAG, "Partial read on record %d", i);
            break;
        }
        buf_used += data_len;
        records_read++;
    }

    fclose(f);
    sd_unlock();
    /* ---- End Pass 1 ---- */

    /* ---- CPU Work: Decrypt all records (NO mutex) ---- */
    mbedtls_gcm_context gcm;

    for (int i = 0; i < records_read; i++) {
        uint16_t msg_idx = start_idx + i;
        size_t off = offsets[i];

        /* Parse record header from buffer */
        uint16_t rec_len = ((uint16_t)raw_buf[off] << 8) | raw_buf[off + 1];
        off += 2;

        /* IV */
        uint8_t *iv = &raw_buf[off];
        off += HISTORY_GCM_IV_LEN;

        /* Ciphertext length = rec_len - 2(len) - 12(iv) - 16(tag) */
        size_t ct_len = rec_len - 2 - HISTORY_GCM_IV_LEN - HISTORY_GCM_TAG_LEN;
        if (ct_len > 11 + HISTORY_MAX_PAYLOAD) {
            ESP_LOGE(TAG, "Record %d too large: %zu", msg_idx, ct_len);
            continue;
        }

        uint8_t *ciphertext = &raw_buf[off];
        off += ct_len;

        /* Tag */
        uint8_t *tag = &raw_buf[off];

        /* Decrypt in-place: output overwrites ciphertext in PSRAM buffer.
         * mbedtls GCM supports input == output. IV and tag are at
         * different positions in the buffer and remain intact. */
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, contact_key, 256);

        int ret = mbedtls_gcm_auth_decrypt(&gcm, ct_len,
                                            iv, HISTORY_GCM_IV_LEN,
                                            NULL, 0,
                                            tag, HISTORY_GCM_TAG_LEN,
                                            ciphertext, ciphertext);
        mbedtls_gcm_free(&gcm);

        if (ret != 0) {
            ESP_LOGE(TAG, "GCM decrypt failed for msg %d: -0x%04X", msg_idx, (unsigned)-ret);
            continue;
        }

        /* Parse plaintext (now at ciphertext pointer) into history_message_t */
        uint8_t *plaintext = ciphertext;
        history_message_t *m = &out[*loaded];
        memset(m, 0, sizeof(history_message_t));

        size_t p = 0;
        m->direction = plaintext[p++];

        if (header.version >= 0x02) {
            // v2: delivery_status stored in record
            m->delivery_status = plaintext[p++];
        } else {
            // v1: no delivery_status in record, default to sent
            m->delivery_status = 0;
        }

        /* High-water mark: upgrade sent(0) to delivered(1).
         * Never override FAILED(2) or PENDING(3) - those are terminal/retry states
         * that must survive the high-water mark. */
        if (m->direction == HISTORY_DIR_SENT &&
            msg_idx <= header.last_delivered_idx &&
            m->delivery_status < 2) {
            m->delivery_status = 1;  /* delivered (vv) */
        }

        m->timestamp = 0;
        for (int b = 7; b >= 0; b--) {
            m->timestamp |= ((int64_t)plaintext[p++]) << (b * 8);
        }

        /* Parse text: full text preserved in history_message_t (up to HISTORY_MAX_TEXT).
         * UI truncation to HISTORY_DISPLAY_TEXT happens in create_bubble_internal(),
         * NOT here. This preserves full text in PSRAM cache for future features. */
        uint16_t pt_text_len = ((uint16_t)plaintext[p] << 8) | plaintext[p + 1];
        p += 2;

        if (pt_text_len > HISTORY_MAX_TEXT) pt_text_len = HISTORY_MAX_TEXT;
        memcpy(m->text, &plaintext[p], pt_text_len);
        m->text[pt_text_len] = '\0';
        m->text_len = pt_text_len;

        (*loaded)++;
    }

    /* ---- Cleanup ---- */
    heap_caps_free(raw_buf);
    memset(contact_key, 0, sizeof(contact_key));

    ESP_LOGI(TAG, "Loaded %d messages from slot %u (read %d raw records)",
             *loaded, slot, records_read);
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
