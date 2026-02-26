/**
 * @file hal_input.h
 * @brief Input Hardware Abstraction Layer Interface
 * 
 * Abstracts input hardware across different devices:
 * - T-Deck Plus: Physical keyboard + Trackball
 * - T-Embed CC1101: Rotary encoder + Buttons
 * - Raspberry Pi: USB keyboard/mouse + GPIO buttons
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_INPUT_H
#define HAL_INPUT_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * INPUT TYPES
 *==========================================================================*/

/**
 * @brief Input device types available
 */
typedef enum {
    HAL_INPUT_NONE          = 0,
    HAL_INPUT_KEYBOARD      = (1 << 0),  /**< Physical keyboard */
    HAL_INPUT_TOUCH         = (1 << 1),  /**< Touchscreen */
    HAL_INPUT_TRACKBALL     = (1 << 2),  /**< Trackball/Trackpad */
    HAL_INPUT_ENCODER       = (1 << 3),  /**< Rotary encoder */
    HAL_INPUT_BUTTONS       = (1 << 4),  /**< Physical buttons */
    HAL_INPUT_MOUSE         = (1 << 5),  /**< USB/BT mouse */
} hal_input_type_t;

/**
 * @brief Key event type
 */
typedef enum {
    HAL_KEY_PRESSED,
    HAL_KEY_RELEASED,
    HAL_KEY_REPEATED,
} hal_key_event_type_t;

/**
 * @brief Key codes (based on USB HID)
 */
typedef enum {
    HAL_KEY_NONE            = 0x00,
    
    // Letters
    HAL_KEY_A               = 0x04,
    HAL_KEY_B               = 0x05,
    HAL_KEY_C               = 0x06,
    HAL_KEY_D               = 0x07,
    HAL_KEY_E               = 0x08,
    HAL_KEY_F               = 0x09,
    HAL_KEY_G               = 0x0A,
    HAL_KEY_H               = 0x0B,
    HAL_KEY_I               = 0x0C,
    HAL_KEY_J               = 0x0D,
    HAL_KEY_K               = 0x0E,
    HAL_KEY_L               = 0x0F,
    HAL_KEY_M               = 0x10,
    HAL_KEY_N               = 0x11,
    HAL_KEY_O               = 0x12,
    HAL_KEY_P               = 0x13,
    HAL_KEY_Q               = 0x14,
    HAL_KEY_R               = 0x15,
    HAL_KEY_S               = 0x16,
    HAL_KEY_T               = 0x17,
    HAL_KEY_U               = 0x18,
    HAL_KEY_V               = 0x19,
    HAL_KEY_W               = 0x1A,
    HAL_KEY_X               = 0x1B,
    HAL_KEY_Y               = 0x1C,
    HAL_KEY_Z               = 0x1D,
    
    // Numbers
    HAL_KEY_1               = 0x1E,
    HAL_KEY_2               = 0x1F,
    HAL_KEY_3               = 0x20,
    HAL_KEY_4               = 0x21,
    HAL_KEY_5               = 0x22,
    HAL_KEY_6               = 0x23,
    HAL_KEY_7               = 0x24,
    HAL_KEY_8               = 0x25,
    HAL_KEY_9               = 0x26,
    HAL_KEY_0               = 0x27,
    
    // Control keys
    HAL_KEY_ENTER           = 0x28,
    HAL_KEY_ESC             = 0x29,
    HAL_KEY_BACKSPACE       = 0x2A,
    HAL_KEY_TAB             = 0x2B,
    HAL_KEY_SPACE           = 0x2C,
    
    // Punctuation
    HAL_KEY_MINUS           = 0x2D,
    HAL_KEY_EQUAL           = 0x2E,
    HAL_KEY_LEFTBRACE       = 0x2F,
    HAL_KEY_RIGHTBRACE      = 0x30,
    HAL_KEY_BACKSLASH       = 0x31,
    HAL_KEY_SEMICOLON       = 0x33,
    HAL_KEY_APOSTROPHE      = 0x34,
    HAL_KEY_GRAVE           = 0x35,
    HAL_KEY_COMMA           = 0x36,
    HAL_KEY_DOT             = 0x37,
    HAL_KEY_SLASH           = 0x38,
    
    // Function keys
    HAL_KEY_F1              = 0x3A,
    HAL_KEY_F2              = 0x3B,
    HAL_KEY_F3              = 0x3C,
    HAL_KEY_F4              = 0x3D,
    HAL_KEY_F5              = 0x3E,
    HAL_KEY_F6              = 0x3F,
    HAL_KEY_F7              = 0x40,
    HAL_KEY_F8              = 0x41,
    HAL_KEY_F9              = 0x42,
    HAL_KEY_F10             = 0x43,
    HAL_KEY_F11             = 0x44,
    HAL_KEY_F12             = 0x45,
    
    // Navigation
    HAL_KEY_HOME            = 0x4A,
    HAL_KEY_PAGEUP          = 0x4B,
    HAL_KEY_DELETE          = 0x4C,
    HAL_KEY_END             = 0x4D,
    HAL_KEY_PAGEDOWN        = 0x4E,
    HAL_KEY_RIGHT           = 0x4F,
    HAL_KEY_LEFT            = 0x50,
    HAL_KEY_DOWN            = 0x51,
    HAL_KEY_UP              = 0x52,
    
    // Modifiers (as separate key codes)
    HAL_KEY_LCTRL           = 0xE0,
    HAL_KEY_LSHIFT          = 0xE1,
    HAL_KEY_LALT            = 0xE2,
    HAL_KEY_LGUI            = 0xE3,
    HAL_KEY_RCTRL           = 0xE4,
    HAL_KEY_RSHIFT          = 0xE5,
    HAL_KEY_RALT            = 0xE6,
    HAL_KEY_RGUI            = 0xE7,
    
    // Virtual keys (for encoder/buttons mapping)
    HAL_KEY_ENCODER_CW      = 0xF0,  /**< Encoder clockwise */
    HAL_KEY_ENCODER_CCW     = 0xF1,  /**< Encoder counter-clockwise */
    HAL_KEY_ENCODER_PRESS   = 0xF2,  /**< Encoder button */
    HAL_KEY_BUTTON_A        = 0xF3,  /**< Generic button A */
    HAL_KEY_BUTTON_B        = 0xF4,  /**< Generic button B */
    HAL_KEY_BUTTON_C        = 0xF5,  /**< Generic button C */
    HAL_KEY_TRACKBALL_PRESS = 0xF6,  /**< Trackball click */
} hal_key_code_t;

/**
 * @brief Key modifier flags
 */
typedef enum {
    HAL_KEY_MOD_NONE    = 0,
    HAL_KEY_MOD_LCTRL   = (1 << 0),
    HAL_KEY_MOD_LSHIFT  = (1 << 1),
    HAL_KEY_MOD_LALT    = (1 << 2),
    HAL_KEY_MOD_LGUI    = (1 << 3),
    HAL_KEY_MOD_RCTRL   = (1 << 4),
    HAL_KEY_MOD_RSHIFT  = (1 << 5),
    HAL_KEY_MOD_RALT    = (1 << 6),
    HAL_KEY_MOD_RGUI    = (1 << 7),
    HAL_KEY_MOD_CTRL    = HAL_KEY_MOD_LCTRL | HAL_KEY_MOD_RCTRL,
    HAL_KEY_MOD_SHIFT   = HAL_KEY_MOD_LSHIFT | HAL_KEY_MOD_RSHIFT,
    HAL_KEY_MOD_ALT     = HAL_KEY_MOD_LALT | HAL_KEY_MOD_RALT,
} hal_key_mod_t;

/**
 * @brief Key event structure
 */
typedef struct {
    hal_key_code_t key;         /**< Key code */
    hal_key_event_type_t type;  /**< Event type */
    uint8_t modifiers;          /**< Active modifiers */
    char character;             /**< ASCII character (0 if none) */
    uint32_t timestamp;         /**< Timestamp in ms */
} hal_key_event_t;

/**
 * @brief Touch/pointer state
 */
typedef enum {
    HAL_POINTER_UP,
    HAL_POINTER_DOWN,
    HAL_POINTER_MOVE,
} hal_pointer_state_t;

/**
 * @brief Touch/pointer event
 */
typedef struct {
    hal_point_t pos;            /**< Position */
    hal_pointer_state_t state;  /**< State */
    uint8_t pressure;           /**< Pressure 0-255 (0 if not available) */
    uint32_t timestamp;         /**< Timestamp in ms */
} hal_pointer_event_t;

/**
 * @brief Encoder event
 */
typedef struct {
    int32_t delta;              /**< Steps since last event (+/- direction) */
    int32_t position;           /**< Absolute position */
    bool pressed;               /**< Button state */
    uint32_t timestamp;         /**< Timestamp in ms */
} hal_encoder_event_t;

/**
 * @brief Input device information
 */
typedef struct {
    uint32_t available;         /**< Bitmask of hal_input_type_t */
    bool has_keyboard;          /**< Convenience: keyboard available */
    bool has_touch;             /**< Convenience: touch available */
    bool has_trackball;         /**< Convenience: trackball available */
    bool has_encoder;           /**< Convenience: encoder available */
    uint8_t button_count;       /**< Number of physical buttons */
    const char *keyboard_layout;/**< Keyboard layout (e.g., "QWERTY") */
} hal_input_info_t;

/**
 * @brief Input event callback types
 */
typedef void (*hal_key_callback_t)(const hal_key_event_t *event, void *user_data);
typedef void (*hal_pointer_callback_t)(const hal_pointer_event_t *event, void *user_data);
typedef void (*hal_encoder_callback_t)(const hal_encoder_event_t *event, void *user_data);

/*============================================================================
 * INPUT API
 *==========================================================================*/

/**
 * @brief Initialize input HAL
 * @return HAL_OK on success
 */
hal_err_t hal_input_init(void);

/**
 * @brief Deinitialize input HAL
 * @return HAL_OK on success
 */
hal_err_t hal_input_deinit(void);

/**
 * @brief Get input device information
 * @return Pointer to input info structure
 */
const hal_input_info_t *hal_input_get_info(void);

/**
 * @brief Poll for input events (call regularly)
 * @return Number of events processed
 */
int hal_input_poll(void);

/**
 * @brief Register key event callback
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void hal_input_set_key_callback(hal_key_callback_t cb, void *user_data);

/**
 * @brief Register pointer event callback
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void hal_input_set_pointer_callback(hal_pointer_callback_t cb, void *user_data);

/**
 * @brief Register encoder event callback
 * @param cb Callback function
 * @param user_data User data passed to callback
 */
void hal_input_set_encoder_callback(hal_encoder_callback_t cb, void *user_data);

/**
 * @brief Check if key is currently pressed
 * @param key Key code
 * @return true if pressed
 */
bool hal_input_key_pressed(hal_key_code_t key);

/**
 * @brief Get current modifier state
 * @return Bitmask of active modifiers
 */
uint8_t hal_input_get_modifiers(void);

/**
 * @brief Get current pointer position
 * @param pos Output position
 * @return true if pointer is available and position valid
 */
bool hal_input_get_pointer_pos(hal_point_t *pos);

/**
 * @brief Get current encoder position
 * @return Encoder position (0 if not available)
 */
int32_t hal_input_get_encoder_pos(void);

/*============================================================================
 * LVGL INTEGRATION
 *==========================================================================*/

/**
 * @brief Read keyboard for LVGL
 * @param indev LVGL input device
 * @param data Output data
 */
void hal_input_lvgl_keyboard_read(void *indev, void *data);

/**
 * @brief Read pointer for LVGL
 * @param indev LVGL input device
 * @param data Output data
 */
void hal_input_lvgl_pointer_read(void *indev, void *data);

/**
 * @brief Read encoder for LVGL
 * @param indev LVGL input device
 * @param data Output data
 */
void hal_input_lvgl_encoder_read(void *indev, void *data);

#ifdef __cplusplus
}
#endif

#endif /* HAL_INPUT_H */
