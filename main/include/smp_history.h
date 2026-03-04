/**
 * SimpleGo - smp_history.h
 * Encrypted Chat History on SD Card
 *
 * Per-contact AES-256-GCM encrypted, append-only message files.
 * Master key in NVS, per-contact keys derived via HKDF-SHA256.
 *
 * Session 37 — Auftrag von Prinzessin Mausi
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SMP_HISTORY_H
#define SMP_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define HISTORY_MAGIC_0       'S'
#define HISTORY_MAGIC_1       'G'
#define HISTORY_MAGIC_2       'H'
#define HISTORY_MAGIC_3       0x01
#define HISTORY_VERSION       0x01
#define HISTORY_HEADER_SIZE   32
#define HISTORY_MAX_LOAD      20
#define HISTORY_MAX_TEXT       4096
#define HISTORY_MAX_PAYLOAD  16000   /* Max SMP message stored on SD */
#define HISTORY_DISPLAY_TEXT   512   /* Max displayed chars in LVGL bubble (UI truncation) */
#define HISTORY_GCM_IV_LEN    12
#define HISTORY_GCM_TAG_LEN   16
#define HISTORY_MASTER_KEY_LEN 32
#define HISTORY_NVS_KEY       "hist_mk"

// Directions
#define HISTORY_DIR_SENT      0x00
#define HISTORY_DIR_RECEIVED  0x01

/**
 * File header -- written once when chat file is created.
 * Sits at byte 0 of /sdcard/simplego/msgs/chat_XX.bin
 *
 * Delivery status tracking:
 *   last_delivered_idx = highest msg_index with confirmed delivery receipt.
 *   On load, all SENT messages with index <= last_delivered_idx get
 *   delivery_status = 1 (vv). This avoids GCM nonce-reuse from in-place
 *   record updates -- only the unencrypted header is modified.
 */
typedef struct __attribute__((packed)) {
    uint8_t   magic[4];              // "SGH\x01"
    uint8_t   version;               // 0x01
    uint8_t   slot_index;            // 0-127
    uint16_t  msg_count;             // total messages in file
    uint16_t  unread_count;          // unread messages (updated on append + mark_read)
    uint16_t  last_delivered_idx;    // highest msg_index with delivery receipt (vv)
    uint8_t   reserved[20];         // future use, zero-filled
} history_header_t;                  // exactly 32 bytes

/**
 * Plaintext message structure -- this gets encrypted before writing.
 * Also used as output when loading messages.
 *
 * Session 40a: text[] holds full message (up to HISTORY_MAX_TEXT = 4096).
 * On SD, records may contain longer text (up to HISTORY_MAX_PAYLOAD).
 * UI truncation to HISTORY_DISPLAY_TEXT (512) happens in the bubble layer,
 * NOT here. Full text is preserved in the PSRAM cache for future use.
 */
typedef struct {
    uint8_t   direction;       // HISTORY_DIR_SENT or HISTORY_DIR_RECEIVED
    uint8_t   delivery_status; // 0 = sent (v), 1 = delivered (vv). Reconstructed on load.
    int64_t   timestamp;       // Unix timestamp (seconds)
    uint16_t  text_len;        // length of text in bytes
    char      text[HISTORY_MAX_TEXT];  // UTF-8 message text
} history_message_t;

/**
 * On-disk record layout (variable length):
 *   uint16_t  record_len     -- total bytes of this record (for seeking)
 *   uint8_t   iv[12]         -- GCM nonce
 *   uint8_t   ciphertext[N]  -- encrypted payload
 *   uint8_t   tag[16]        -- GCM auth tag
 *
 * record_len = 2 + 12 + N + 16
 * N = sizeof(direction) + sizeof(timestamp) + sizeof(text_len) + text_len
 *   = 1 + 8 + 2 + text_len
 */

// --- Public API (all called from App Task ONLY) ---

/** Initialize history system. Loads or generates master key from NVS. */
esp_err_t smp_history_init(void);

/** Append one message to the history file for the given slot. */
esp_err_t smp_history_append(uint8_t slot, const history_message_t *msg);

/**
 * Load the most recent messages from a slot's history.
 * Delivery status is reconstructed from header.last_delivered_idx:
 * SENT messages with index <= last_delivered_idx get delivery_status = 1.
 *
 * @param slot      Contact slot index (0-127)
 * @param out       Output array, must have space for 'count' elements
 * @param count     Max messages to load (typically HISTORY_MAX_LOAD)
 * @param loaded    Output: actual number of messages loaded
 */
esp_err_t smp_history_load_recent(uint8_t slot, history_message_t *out, int count, int *loaded);

/** Get message counts without loading messages. */
esp_err_t smp_history_get_counts(uint8_t slot, uint16_t *total, uint16_t *unread);

/** Mark all messages in a slot as read (sets unread_count = 0 in header). */
esp_err_t smp_history_mark_read(uint8_t slot);

/**
 * Update delivery high-water mark: all sent messages up to msg_index
 * are considered "delivered" (vv). Only updates if new index is higher.
 * Safe to call repeatedly -- header-only update, no GCM nonce reuse.
 */
esp_err_t smp_history_update_delivered(uint8_t slot, uint16_t msg_index);

/** Delete the entire history file for a slot. Called during contact delete. */
esp_err_t smp_history_delete(uint8_t slot);

#endif // SMP_HISTORY_H
