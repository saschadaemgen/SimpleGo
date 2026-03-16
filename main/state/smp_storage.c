/**
 * SimpleGo - smp_storage.c
 * Persistent Storage Module Implementation
 * v0.1.17-alpha
 *
 * Two-Step Init Architecture:
 *   Step 1: smp_storage_init()    -> NVS only (before display, no SPI)
 *   Step 2: smp_storage_init_sd() -> SD card on existing SPI bus (after display)
 *
 * The T-Deck display owns SPI2_HOST. We share that bus for SD card
 * with a separate CS pin. Never call spi_bus_initialize() ourselves.
 *
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_storage.h"
#include "smp_ratchet.h"

#include "mbedtls/platform_util.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

static const char *TAG = "SMP_STOR";

// ============== T-Deck SD Card Configuration ==============
// T-Deck shares SPI2_HOST with display (SCLK=40, MOSI=41, MISO=38)
// Display CS = GPIO 12, SD card needs its OWN CS pin
// T-Deck Plus SD_CS is typically GPIO 39
// SD_PIN_CS per device_config.h

#define SD_PIN_CS       GPIO_NUM_39
#define SD_SPI_HOST     SPI2_HOST       // Same bus as display - DO NOT re-initialize!

// ============== Internal State ==============

static struct {
    nvs_handle_t nvs_handle;
    bool nvs_ready;
    bool sd_mounted;
    sdmmc_card_t *sd_card;
} storage = {0};

// ============== Helper: Create directory recursively ==============

static esp_err_t mkdir_p(const char *path) {
    char tmp[128];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    int ret = mkdir(tmp, 0755);
    if (ret != 0 && errno != EEXIST) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ============== NVS Init (call BEFORE display) ==============

esp_err_t smp_storage_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SimpleGo Storage Init (NVS) ===");

    ret = nvs_open(SMP_STORAGE_NVS_NAMESPACE, NVS_READWRITE, &storage.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }
    storage.nvs_ready = true;
    ESP_LOGI(TAG, "NVS namespace '%s' opened - ready for Write-Before-Send", SMP_STORAGE_NVS_NAMESPACE);
    ESP_LOGI(TAG, "");

    return ESP_OK;
}

// ============== SD Card Init (call AFTER display) ==============

esp_err_t smp_storage_init_sd(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SimpleGo Storage Init (SD Card) ===");

    if (storage.sd_mounted) {
        ESP_LOGW(TAG, "SD already mounted");
        return ESP_OK;
    }

    // Deactivate LoRa CS to free SPI bus for SD card
    gpio_set_direction(GPIO_NUM_9, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_9, 1);  // HIGH = deselected

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Use existing SPI bus from display - DO NOT call spi_bus_initialize()!
    // Display already initialized SPI2_HOST with SCLK=40, MOSI=41, MISO=38
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SD_SPI_HOST;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        SMP_STORAGE_SD_MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &storage.sd_card
    );

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "  (This is OK - message history will use NVS fallback)");
        storage.sd_mounted = false;
    } else {
        storage.sd_mounted = true;
        ESP_LOGI(TAG, "SD card mounted at %s", SMP_STORAGE_SD_MOUNT_POINT);
        sdmmc_card_print_info(stdout, storage.sd_card);
        mkdir_p(SMP_STORAGE_SD_MSG_DIR);
        ESP_LOGI(TAG, "SD directories created");
    }

    ESP_LOGI(TAG, "Storage complete: NVS=%s, SD=%s",
             storage.nvs_ready ? "OK" : "FAIL",
             storage.sd_mounted ? "OK" : "N/A");
    ESP_LOGI(TAG, "");

    return ESP_OK;
}

void smp_storage_deinit(void) {
    if (storage.nvs_ready) {
        nvs_close(storage.nvs_handle);
        storage.nvs_ready = false;
        ESP_LOGI(TAG, "NVS closed");
    }

    if (storage.sd_mounted) {
        esp_vfs_fat_sdcard_unmount(SMP_STORAGE_SD_MOUNT_POINT, storage.sd_card);
        storage.sd_mounted = false;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

bool smp_storage_sd_available(void) {
    return storage.sd_mounted;
}

// ============== NVS Backend ==============

esp_err_t smp_storage_save_blob(const char *key, const void *data, size_t len) {
    if (!storage.nvs_ready) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!key || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len > SMP_STORAGE_MAX_BLOB_SIZE) {
        ESP_LOGE(TAG, "Blob too large: %zu > %d", len, SMP_STORAGE_MAX_BLOB_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = nvs_set_blob(storage.nvs_handle, key, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob('%s') failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(storage.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "NVS save: '%s' (%zu bytes)", key, len);
    return ESP_OK;
}

esp_err_t smp_storage_load_blob(const char *key, void *buf, size_t buf_len, size_t *out_len) {
    if (!storage.nvs_ready) {
        ESP_LOGE(TAG, "NVS not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!key || !buf || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(storage.nvs_handle, key, NULL, &required_size);
    if (ret != ESP_OK) {
        return ret;
    }

    if (required_size > buf_len) {
        ESP_LOGE(TAG, "Buffer too small for '%s': need %zu, have %zu", key, required_size, buf_len);
        return ESP_ERR_INVALID_SIZE;
    }

    ret = nvs_get_blob(storage.nvs_handle, key, buf, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob('%s') failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    if (out_len) {
        *out_len = required_size;
    }

    ESP_LOGD(TAG, "NVS load: '%s' (%zu bytes)", key, required_size);
    return ESP_OK;
}

esp_err_t smp_storage_delete(const char *key) {
    if (!storage.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (!key) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = nvs_erase_key(storage.nvs_handle, key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_erase_key('%s') failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    nvs_commit(storage.nvs_handle);
    ESP_LOGD(TAG, "NVS delete: '%s' %s", key,
             ret == ESP_ERR_NVS_NOT_FOUND ? "(not found)" : "OK");
    return ret;
}

bool smp_storage_exists(const char *key) {
    if (!storage.nvs_ready || !key) return false;

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(storage.nvs_handle, key, NULL, &required_size);
    return (ret == ESP_OK && required_size > 0);
}

// ============== Write-Before-Send (Evgeny's Pattern) ==============

esp_err_t smp_storage_save_blob_sync(const char *key, const void *data, size_t len) {
    if (!storage.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (!key || !data || len == 0) return ESP_ERR_INVALID_ARG;
    if (len > SMP_STORAGE_MAX_BLOB_SIZE) return ESP_ERR_INVALID_SIZE;

    int64_t t_start = esp_timer_get_time();

    esp_err_t ret = nvs_set_blob(storage.nvs_handle, key, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SYNC write '%s' failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(storage.nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SYNC commit '%s' failed: %s", key, esp_err_to_name(ret));
        return ret;
    }

    uint8_t *verify_buf = malloc(len);
    if (!verify_buf) {
        ESP_LOGE(TAG, "SYNC verify malloc failed (%zu bytes)", len);
        return ESP_ERR_NO_MEM;
    }

    size_t verify_len = len;
    ret = nvs_get_blob(storage.nvs_handle, key, verify_buf, &verify_len);
    if (ret != ESP_OK || verify_len != len || memcmp(data, verify_buf, len) != 0) {
        ESP_LOGE(TAG, "SYNC verify FAILED for '%s'! Data corruption!", key);
        free(verify_buf);
        return ESP_ERR_INVALID_RESPONSE;
    }
    free(verify_buf);

    int64_t t_end = esp_timer_get_time();
    int64_t elapsed_us = t_end - t_start;

    ESP_LOGI(TAG, "SYNC save: '%s' (%zu bytes) verified in %lld us", key, len, elapsed_us);

    return ESP_OK;
}

// ============== SD Card Backend ==============

esp_err_t smp_storage_sd_write(const char *path, const void *data, size_t len) {
    if (!storage.sd_mounted) {
        ESP_LOGW(TAG, "SD write skipped - no SD card");
        return ESP_ERR_NOT_FOUND;
    }
    if (!path || !data || len == 0) return ESP_ERR_INVALID_ARG;

    char dir[128];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir);
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "SD fopen failed: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        ESP_LOGE(TAG, "SD write incomplete: %zu/%zu bytes for %s", written, len, path);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "SD write: %s (%zu bytes)", path, len);
    return ESP_OK;
}

esp_err_t smp_storage_sd_read(const char *path, void *buf, size_t buf_len, size_t *out_len) {
    if (!storage.sd_mounted) return ESP_ERR_NOT_FOUND;
    if (!path || !buf || buf_len == 0) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_read = fread(buf, 1, buf_len, f);
    fclose(f);

    if (out_len) {
        *out_len = bytes_read;
    }

    ESP_LOGD(TAG, "SD read: %s (%zu bytes)", path, bytes_read);
    return ESP_OK;
}

esp_err_t smp_storage_sd_delete(const char *path) {
    if (!storage.sd_mounted) return ESP_ERR_NOT_FOUND;
    if (!path) return ESP_ERR_INVALID_ARG;

    if (remove(path) != 0) {
        if (errno == ENOENT) return ESP_ERR_NOT_FOUND;
        ESP_LOGE(TAG, "SD delete failed: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "SD delete: %s", path);
    return ESP_OK;
}

bool smp_storage_sd_file_exists(const char *path) {
    if (!storage.sd_mounted || !path) return false;

    struct stat st;
    return (stat(path, &st) == 0);
}

// ============== Display Name (Session 43) ==============

#define DISPLAY_NAME_NVS_KEY    "user_name"
#define DISPLAY_NAME_MAX_LEN    31
#define DISPLAY_NAME_DEFAULT    "SimpleGo"

void storage_get_display_name(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;
    buf[0] = '\0';

    if (!storage.nvs_ready) {
        snprintf(buf, buf_size, "%s", DISPLAY_NAME_DEFAULT);
        return;
    }

    size_t out_len = 0;
    esp_err_t ret = smp_storage_load_blob(DISPLAY_NAME_NVS_KEY, buf, buf_size - 1, &out_len);
    if (ret != ESP_OK || out_len == 0) {
        snprintf(buf, buf_size, "%s", DISPLAY_NAME_DEFAULT);
        return;
    }
    buf[out_len] = '\0';
}

esp_err_t storage_set_display_name(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;

    size_t len = strlen(name);
    if (len == 0 || len > DISPLAY_NAME_MAX_LEN) {
        ESP_LOGW(TAG, "Display name invalid length: %zu (max %d)", len, DISPLAY_NAME_MAX_LEN);
        return ESP_ERR_INVALID_ARG;
    }

    /* JSON safety: reject quotes and backslashes */
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '"' || name[i] == '\\') {
            ESP_LOGW(TAG, "Display name contains forbidden char at pos %zu", i);
            return ESP_ERR_INVALID_ARG;
        }
    }

    return smp_storage_save_blob(DISPLAY_NAME_NVS_KEY, name, len);
}

bool storage_has_display_name(void)
{
    return smp_storage_exists(DISPLAY_NAME_NVS_KEY);
}

// ============== Timezone Offset (Session 48) ==============

#define TZ_OFFSET_NVS_KEY  "tz_offset"

int8_t g_tz_offset_hours = 0;

void storage_load_tz_offset(void)
{
    int8_t val = 0;
    size_t out_len = 0;
    esp_err_t ret = smp_storage_load_blob(TZ_OFFSET_NVS_KEY, &val, sizeof(val), &out_len);
    if (ret == ESP_OK && out_len == sizeof(val) && val >= -12 && val <= 14) {
        g_tz_offset_hours = val;
    } else {
        g_tz_offset_hours = 0;
    }
    ESP_LOGI(TAG, "Timezone offset: UTC%+d", g_tz_offset_hours);
}

void storage_set_tz_offset(int8_t offset)
{
    if (offset < -12) offset = -12;
    if (offset > 14) offset = 14;
    g_tz_offset_hours = offset;
    smp_storage_save_blob(TZ_OFFSET_NVS_KEY, &offset, sizeof(offset));
    ESP_LOGI(TAG, "Timezone offset saved: UTC%+d", offset);
}

// ============== Diagnostics ==============

void smp_storage_print_info(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "|     SimpleGo Storage Diagnostics         |");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    ESP_LOGI(TAG, "--- NVS ---");
    ESP_LOGI(TAG, "  Namespace: '%s'", SMP_STORAGE_NVS_NAMESPACE);
    ESP_LOGI(TAG, "  Ready: %s", storage.nvs_ready ? "YES" : "NO");

    if (storage.nvs_ready) {
        nvs_stats_t nvs_stats;
        if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
            ESP_LOGI(TAG, "  Entries: used=%zu, free=%zu, total=%zu, ns_count=%zu",
                     nvs_stats.used_entries, nvs_stats.free_entries,
                     nvs_stats.total_entries, nvs_stats.namespace_count);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- SD Card ---");
    ESP_LOGI(TAG, "  Mounted: %s", storage.sd_mounted ? "YES" : "NO");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Struct Sizes ---");
    ESP_LOGI(TAG, "  ratchet_state_t:   %4zu bytes", sizeof(ratchet_state_t));
    ESP_LOGI(TAG, "  Skipped Key Entry: %4zu bytes (est: 32+4+32 = 68)",
             (size_t)68);

    size_t ratchet_size = sizeof(ratchet_state_t);
    size_t queue_cred_est = 300;
    size_t per_contact = ratchet_size + queue_cred_est + (50 * 68);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Capacity Estimates ---");
    ESP_LOGI(TAG, "  Per contact: ~%zu bytes", per_contact);
    ESP_LOGI(TAG, "    Ratchet State: %zu", ratchet_size);
    ESP_LOGI(TAG, "    Queue Creds:   ~%zu (est)", queue_cred_est);
    ESP_LOGI(TAG, "    Skipped Keys:  ~%zu (50 avg x 68B)", (size_t)(50 * 68));
    ESP_LOGI(TAG, "  128KB NVS fits:  ~%zu contacts", (128 * 1024) / per_contact);
    ESP_LOGI(TAG, "");
}

// ============== Self-Test ==============

esp_err_t smp_storage_self_test(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "|     SimpleGo Storage Self-Test           |");
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    esp_err_t result = ESP_OK;
    const size_t TEST_SIZE = 256;
    uint8_t test_data[256];
    uint8_t read_buf[256];
    size_t read_len = 0;

    esp_fill_random(test_data, TEST_SIZE);

    // ==========================================
    // Test A: NVS Basic Roundtrip
    // ==========================================
    ESP_LOGI(TAG, "--- Test A: NVS Basic Roundtrip ---");
    {
        const char *test_key = "test_rt";

        esp_err_t ret = smp_storage_save_blob(test_key, test_data, TEST_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: save_blob returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (!smp_storage_exists(test_key)) {
            ESP_LOGE(TAG, "  FAIL: exists() returned false after save");
            result = ESP_FAIL;
            goto test_b;
        }

        mbedtls_platform_zeroize(read_buf, TEST_SIZE);
        ret = smp_storage_load_blob(test_key, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: load_blob returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: data mismatch! wrote %zu, read %zu", TEST_SIZE, read_len);
            result = ESP_FAIL;
            goto test_b;
        }

        ret = smp_storage_delete(test_key);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: delete returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_b;
        }

        if (smp_storage_exists(test_key)) {
            ESP_LOGE(TAG, "  FAIL: exists() returned true after delete");
            result = ESP_FAIL;
            goto test_b;
        }

        ESP_LOGI(TAG, "  PASSED: NVS roundtrip OK (%zu bytes)", TEST_SIZE);
    }

test_b:
    // ==========================================
    // Test B: NVS Write-Before-Send + Timing
    // ==========================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Test B: Write-Before-Send (Sync) + Timing ---");
    {
        const char *test_key = "test_wbs";

        int64_t t_start = esp_timer_get_time();

        esp_err_t ret = smp_storage_save_blob_sync(test_key, test_data, TEST_SIZE);

        int64_t t_end = esp_timer_get_time();
        int64_t elapsed = t_end - t_start;

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: save_blob_sync returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_c;
        }

        mbedtls_platform_zeroize(read_buf, TEST_SIZE);
        ret = smp_storage_load_blob(test_key, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK || read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: immediate read-back mismatch");
            result = ESP_FAIL;
            goto test_c;
        }

        smp_storage_delete(test_key);

        ESP_LOGI(TAG, "  PASSED: Sync write+verify in %lld us (%.1f ms)",
                 elapsed, elapsed / 1000.0);

        if (elapsed < 5000) {
            ESP_LOGI(TAG, "     Excellent: <5ms");
        } else if (elapsed < 20000) {
            ESP_LOGI(TAG, "     Good: <20ms - acceptable for Write-Before-Send");
        } else {
            ESP_LOGW(TAG, "     Slow: %lldms - may impact real-time feel", elapsed / 1000);
        }
    }

test_c:
    // ==========================================
    // Test C: SD Card (if available)
    // ==========================================
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "--- Test C: SD Card Roundtrip ---");
    {
        if (!storage.sd_mounted) {
            ESP_LOGW(TAG, "  SKIPPED: SD card not available");
            goto test_done;
        }

        const char *test_path = SMP_STORAGE_SD_MSG_DIR "/test_file.bin";

        esp_err_t ret = smp_storage_sd_write(test_path, test_data, TEST_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_write returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (!smp_storage_sd_file_exists(test_path)) {
            ESP_LOGE(TAG, "  FAIL: sd_file_exists returned false after write");
            result = ESP_FAIL;
            goto test_done;
        }

        mbedtls_platform_zeroize(read_buf, TEST_SIZE);
        ret = smp_storage_sd_read(test_path, read_buf, TEST_SIZE, &read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_read returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (read_len != TEST_SIZE || memcmp(test_data, read_buf, TEST_SIZE) != 0) {
            ESP_LOGE(TAG, "  FAIL: SD data mismatch! wrote %zu, read %zu", TEST_SIZE, read_len);
            result = ESP_FAIL;
            goto test_done;
        }

        ret = smp_storage_sd_delete(test_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "  FAIL: sd_delete returned %s", esp_err_to_name(ret));
            result = ESP_FAIL;
            goto test_done;
        }

        if (smp_storage_sd_file_exists(test_path)) {
            ESP_LOGE(TAG, "  FAIL: sd_file_exists returned true after delete");
            result = ESP_FAIL;
            goto test_done;
        }

        ESP_LOGI(TAG, "  PASSED: SD roundtrip OK (%zu bytes)", TEST_SIZE);
    }

test_done:
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+==========================================+");
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "|  ALL STORAGE TESTS PASSED                |");
    } else {
        ESP_LOGE(TAG, "|  SOME TESTS FAILED                       |");
    }
    ESP_LOGI(TAG, "+==========================================+");
    ESP_LOGI(TAG, "");

    return result;
}
