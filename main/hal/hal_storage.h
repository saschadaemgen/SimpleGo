/**
 * @file hal_storage.h
 * @brief Storage Hardware Abstraction Layer Interface
 * 
 * Abstracts storage across different devices:
 * - ESP32: NVS + Optional SD Card
 * - Raspberry Pi: File system + SQLite
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_STORAGE_H
#define HAL_STORAGE_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * STORAGE TYPES
 *==========================================================================*/

/**
 * @brief Storage type
 */
typedef enum {
    HAL_STORAGE_NVS,        /**< Non-volatile key-value (small items) */
    HAL_STORAGE_SD,         /**< SD card file system */
    HAL_STORAGE_FLASH,      /**< Flash partition */
    HAL_STORAGE_FILE,       /**< Generic file system */
} hal_storage_type_t;

/**
 * @brief Storage capabilities
 */
typedef enum {
    HAL_STORAGE_CAP_NONE        = 0,
    HAL_STORAGE_CAP_NVS         = (1 << 0),  /**< Key-value storage */
    HAL_STORAGE_CAP_SD          = (1 << 1),  /**< SD card */
    HAL_STORAGE_CAP_LARGE_FILES = (1 << 2),  /**< Files > 4KB */
    HAL_STORAGE_CAP_SECURE      = (1 << 3),  /**< Encrypted storage */
    HAL_STORAGE_CAP_WEAR_LEVEL  = (1 << 4),  /**< Wear leveling */
} hal_storage_caps_t;

/**
 * @brief Storage information
 */
typedef struct {
    uint32_t capabilities;      /**< Capability flags */
    size_t nvs_free;            /**< Free NVS space in bytes */
    size_t nvs_total;           /**< Total NVS space */
    size_t sd_free;             /**< Free SD space in bytes */
    size_t sd_total;            /**< Total SD space */
    bool sd_mounted;            /**< SD card mounted */
} hal_storage_info_t;

/**
 * @brief Data type for NVS
 */
typedef enum {
    HAL_NVS_TYPE_U8,
    HAL_NVS_TYPE_I8,
    HAL_NVS_TYPE_U16,
    HAL_NVS_TYPE_I16,
    HAL_NVS_TYPE_U32,
    HAL_NVS_TYPE_I32,
    HAL_NVS_TYPE_U64,
    HAL_NVS_TYPE_I64,
    HAL_NVS_TYPE_STR,
    HAL_NVS_TYPE_BLOB,
} hal_nvs_type_t;

/**
 * @brief File handle
 */
typedef void* hal_file_t;

/**
 * @brief File mode
 */
typedef enum {
    HAL_FILE_READ       = (1 << 0),
    HAL_FILE_WRITE      = (1 << 1),
    HAL_FILE_APPEND     = (1 << 2),
    HAL_FILE_CREATE     = (1 << 3),
    HAL_FILE_TRUNCATE   = (1 << 4),
    HAL_FILE_BINARY     = (1 << 5),
} hal_file_mode_t;

/**
 * @brief File seek origin
 */
typedef enum {
    HAL_SEEK_SET,       /**< From beginning */
    HAL_SEEK_CUR,       /**< From current position */
    HAL_SEEK_END,       /**< From end */
} hal_seek_t;

/**
 * @brief File info
 */
typedef struct {
    char name[64];          /**< File name */
    size_t size;            /**< File size */
    bool is_directory;      /**< Is directory */
    uint32_t modified;      /**< Modified time (Unix timestamp) */
} hal_file_info_t;

/*============================================================================
 * STORAGE API - INITIALIZATION
 *==========================================================================*/

/**
 * @brief Initialize storage HAL
 * @return HAL_OK on success
 */
hal_err_t hal_storage_init(void);

/**
 * @brief Deinitialize storage HAL
 * @return HAL_OK on success
 */
hal_err_t hal_storage_deinit(void);

/**
 * @brief Get storage information
 * @return Pointer to storage info
 */
const hal_storage_info_t *hal_storage_get_info(void);

/*============================================================================
 * NVS API (Key-Value Storage)
 *==========================================================================*/

/**
 * @brief Open NVS namespace
 * @param namespace Namespace name (max 15 chars)
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_open(const char *namespace);

/**
 * @brief Close NVS namespace
 */
void hal_nvs_close(void);

/**
 * @brief Set integer value
 * @param key Key name (max 15 chars)
 * @param value Value
 * @param type Value type
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_set_int(const char *key, int64_t value, hal_nvs_type_t type);

/**
 * @brief Get integer value
 * @param key Key name
 * @param value Output value
 * @param type Value type
 * @return HAL_OK on success, HAL_ERR_NOT_FOUND if key doesn't exist
 */
hal_err_t hal_nvs_get_int(const char *key, int64_t *value, hal_nvs_type_t type);

/**
 * @brief Set string value
 * @param key Key name
 * @param value Null-terminated string
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_set_str(const char *key, const char *value);

/**
 * @brief Get string value
 * @param key Key name
 * @param value Output buffer
 * @param max_len Buffer size
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_get_str(const char *key, char *value, size_t max_len);

/**
 * @brief Set binary blob
 * @param key Key name
 * @param data Data pointer
 * @param len Data length
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_set_blob(const char *key, const void *data, size_t len);

/**
 * @brief Get binary blob
 * @param key Key name
 * @param data Output buffer
 * @param len Buffer size (in), actual size (out)
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_get_blob(const char *key, void *data, size_t *len);

/**
 * @brief Get blob size without reading data
 * @param key Key name
 * @param len Output size
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_get_blob_size(const char *key, size_t *len);

/**
 * @brief Erase key
 * @param key Key name
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_erase(const char *key);

/**
 * @brief Erase all keys in namespace
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_erase_all(void);

/**
 * @brief Commit pending changes
 * @return HAL_OK on success
 */
hal_err_t hal_nvs_commit(void);

/*============================================================================
 * FILE API (SD Card / File System)
 *==========================================================================*/

/**
 * @brief Mount SD card
 * @return HAL_OK on success
 */
hal_err_t hal_sd_mount(void);

/**
 * @brief Unmount SD card
 * @return HAL_OK on success
 */
hal_err_t hal_sd_unmount(void);

/**
 * @brief Check if SD card is mounted
 * @return true if mounted
 */
bool hal_sd_is_mounted(void);

/**
 * @brief Open file
 * @param path File path
 * @param mode File mode flags
 * @return File handle or NULL on error
 */
hal_file_t hal_file_open(const char *path, uint32_t mode);

/**
 * @brief Close file
 * @param file File handle
 */
void hal_file_close(hal_file_t file);

/**
 * @brief Read from file
 * @param file File handle
 * @param buf Output buffer
 * @param size Bytes to read
 * @return Bytes read, or -1 on error
 */
int hal_file_read(hal_file_t file, void *buf, size_t size);

/**
 * @brief Write to file
 * @param file File handle
 * @param buf Data buffer
 * @param size Bytes to write
 * @return Bytes written, or -1 on error
 */
int hal_file_write(hal_file_t file, const void *buf, size_t size);

/**
 * @brief Seek in file
 * @param file File handle
 * @param offset Offset
 * @param origin Seek origin
 * @return New position, or -1 on error
 */
int hal_file_seek(hal_file_t file, int32_t offset, hal_seek_t origin);

/**
 * @brief Get current position
 * @param file File handle
 * @return Position, or -1 on error
 */
int hal_file_tell(hal_file_t file);

/**
 * @brief Get file size
 * @param file File handle
 * @return Size, or -1 on error
 */
int hal_file_size(hal_file_t file);

/**
 * @brief Flush file buffers
 * @param file File handle
 * @return HAL_OK on success
 */
hal_err_t hal_file_flush(hal_file_t file);

/**
 * @brief Delete file
 * @param path File path
 * @return HAL_OK on success
 */
hal_err_t hal_file_delete(const char *path);

/**
 * @brief Rename file
 * @param old_path Current path
 * @param new_path New path
 * @return HAL_OK on success
 */
hal_err_t hal_file_rename(const char *old_path, const char *new_path);

/**
 * @brief Check if file exists
 * @param path File path
 * @return true if exists
 */
bool hal_file_exists(const char *path);

/**
 * @brief Get file info
 * @param path File path
 * @param info Output info
 * @return HAL_OK on success
 */
hal_err_t hal_file_stat(const char *path, hal_file_info_t *info);

/**
 * @brief Create directory
 * @param path Directory path
 * @return HAL_OK on success
 */
hal_err_t hal_mkdir(const char *path);

/**
 * @brief Remove directory (must be empty)
 * @param path Directory path
 * @return HAL_OK on success
 */
hal_err_t hal_rmdir(const char *path);

/*============================================================================
 * CONVENIENCE MACROS
 *==========================================================================*/

#define hal_nvs_set_u8(key, val)   hal_nvs_set_int(key, (int64_t)(val), HAL_NVS_TYPE_U8)
#define hal_nvs_set_u16(key, val)  hal_nvs_set_int(key, (int64_t)(val), HAL_NVS_TYPE_U16)
#define hal_nvs_set_u32(key, val)  hal_nvs_set_int(key, (int64_t)(val), HAL_NVS_TYPE_U32)
#define hal_nvs_set_i32(key, val)  hal_nvs_set_int(key, (int64_t)(val), HAL_NVS_TYPE_I32)

#define hal_nvs_get_u8(key, val)   hal_nvs_get_int(key, (int64_t*)(val), HAL_NVS_TYPE_U8)
#define hal_nvs_get_u16(key, val)  hal_nvs_get_int(key, (int64_t*)(val), HAL_NVS_TYPE_U16)
#define hal_nvs_get_u32(key, val)  hal_nvs_get_int(key, (int64_t*)(val), HAL_NVS_TYPE_U32)
#define hal_nvs_get_i32(key, val)  hal_nvs_get_int(key, (int64_t*)(val), HAL_NVS_TYPE_I32)

#ifdef __cplusplus
}
#endif

#endif /* HAL_STORAGE_H */
