/**
 * @file hal_audio.h
 * @brief Audio Hardware Abstraction Layer Interface
 * 
 * Abstracts audio hardware:
 * - T-Deck Plus: I2S Speaker + Mic
 * - T-Embed: Buzzer only
 * - Raspberry Pi: ALSA audio
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * AUDIO TYPES
 *==========================================================================*/

/**
 * @brief Audio capabilities
 */
typedef enum {
    HAL_AUDIO_CAP_NONE      = 0,
    HAL_AUDIO_CAP_SPEAKER   = (1 << 0),  /**< Has speaker output */
    HAL_AUDIO_CAP_BUZZER    = (1 << 1),  /**< Has buzzer (tones only) */
    HAL_AUDIO_CAP_MIC       = (1 << 2),  /**< Has microphone input */
    HAL_AUDIO_CAP_HEADPHONE = (1 << 3),  /**< Has headphone jack */
    HAL_AUDIO_CAP_BLUETOOTH = (1 << 4),  /**< Bluetooth audio */
} hal_audio_caps_t;

/**
 * @brief Audio sample format
 */
typedef enum {
    HAL_AUDIO_FMT_PCM_8,    /**< 8-bit PCM */
    HAL_AUDIO_FMT_PCM_16,   /**< 16-bit PCM */
    HAL_AUDIO_FMT_PCM_24,   /**< 24-bit PCM */
    HAL_AUDIO_FMT_PCM_32,   /**< 32-bit PCM */
} hal_audio_fmt_t;

/**
 * @brief Audio configuration
 */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate (e.g., 16000, 44100) */
    hal_audio_fmt_t format;     /**< Sample format */
    uint8_t channels;           /**< 1 = mono, 2 = stereo */
    size_t buffer_size;         /**< Buffer size in samples */
} hal_audio_config_t;

/**
 * @brief Audio information
 */
typedef struct {
    uint32_t capabilities;      /**< Capability flags */
    bool has_speaker;           /**< Convenience flag */
    bool has_mic;               /**< Convenience flag */
    bool has_buzzer;            /**< Convenience flag */
    uint8_t max_volume;         /**< Maximum volume level */
    const char *codec_name;     /**< Audio codec name (if applicable) */
} hal_audio_info_t;

/**
 * @brief Audio state
 */
typedef enum {
    HAL_AUDIO_STATE_IDLE,
    HAL_AUDIO_STATE_PLAYING,
    HAL_AUDIO_STATE_RECORDING,
    HAL_AUDIO_STATE_PAUSED,
} hal_audio_state_t;

/**
 * @brief Tone definition (for buzzer)
 */
typedef struct {
    uint16_t frequency;     /**< Frequency in Hz (0 = silence) */
    uint16_t duration;      /**< Duration in ms */
} hal_tone_t;

/**
 * @brief Predefined notification sounds
 */
typedef enum {
    HAL_SOUND_NONE,
    HAL_SOUND_BEEP,             /**< Simple beep */
    HAL_SOUND_SUCCESS,          /**< Success/confirmation */
    HAL_SOUND_ERROR,            /**< Error */
    HAL_SOUND_WARNING,          /**< Warning */
    HAL_SOUND_NOTIFICATION,     /**< New message notification */
    HAL_SOUND_KEYPRESS,         /**< Key press feedback */
    HAL_SOUND_STARTUP,          /**< Startup jingle */
    HAL_SOUND_SHUTDOWN,         /**< Shutdown sound */
} hal_sound_t;

/**
 * @brief Audio event callback
 */
typedef void (*hal_audio_callback_t)(void *user_data);

/*============================================================================
 * AUDIO API
 *==========================================================================*/

/**
 * @brief Initialize audio HAL
 * @return HAL_OK on success
 */
hal_err_t hal_audio_init(void);

/**
 * @brief Deinitialize audio HAL
 * @return HAL_OK on success
 */
hal_err_t hal_audio_deinit(void);

/**
 * @brief Get audio information
 * @return Pointer to audio info
 */
const hal_audio_info_t *hal_audio_get_info(void);

/**
 * @brief Get current audio state
 * @return Audio state
 */
hal_audio_state_t hal_audio_get_state(void);

/*============================================================================
 * VOLUME CONTROL
 *==========================================================================*/

/**
 * @brief Set master volume
 * @param volume Volume 0-100
 * @return HAL_OK on success
 */
hal_err_t hal_audio_set_volume(uint8_t volume);

/**
 * @brief Get current volume
 * @return Volume 0-100
 */
uint8_t hal_audio_get_volume(void);

/**
 * @brief Mute/unmute audio
 * @param mute true to mute
 * @return HAL_OK on success
 */
hal_err_t hal_audio_mute(bool mute);

/**
 * @brief Check if muted
 * @return true if muted
 */
bool hal_audio_is_muted(void);

/*============================================================================
 * PLAYBACK API
 *==========================================================================*/

/**
 * @brief Configure audio playback
 * @param config Audio configuration
 * @return HAL_OK on success
 */
hal_err_t hal_audio_config_playback(const hal_audio_config_t *config);

/**
 * @brief Play audio buffer
 * @param data Audio data
 * @param size Data size in bytes
 * @param blocking true to wait for completion
 * @return HAL_OK on success
 */
hal_err_t hal_audio_play(const void *data, size_t size, bool blocking);

/**
 * @brief Stop playback
 * @return HAL_OK on success
 */
hal_err_t hal_audio_stop(void);

/**
 * @brief Pause playback
 * @return HAL_OK on success
 */
hal_err_t hal_audio_pause(void);

/**
 * @brief Resume playback
 * @return HAL_OK on success
 */
hal_err_t hal_audio_resume(void);

/**
 * @brief Set playback complete callback
 * @param cb Callback function
 * @param user_data User data
 */
void hal_audio_set_complete_cb(hal_audio_callback_t cb, void *user_data);

/*============================================================================
 * TONE/BUZZER API
 *==========================================================================*/

/**
 * @brief Play single tone
 * @param frequency Frequency in Hz
 * @param duration Duration in ms
 * @return HAL_OK on success
 */
hal_err_t hal_audio_tone(uint16_t frequency, uint16_t duration);

/**
 * @brief Play sequence of tones
 * @param tones Array of tones (last entry has frequency=0, duration=0)
 * @param blocking true to wait for completion
 * @return HAL_OK on success
 */
hal_err_t hal_audio_play_tones(const hal_tone_t *tones, bool blocking);

/**
 * @brief Play predefined sound
 * @param sound Sound ID
 * @return HAL_OK on success
 */
hal_err_t hal_audio_play_sound(hal_sound_t sound);

/*============================================================================
 * RECORDING API (if HAL_AUDIO_CAP_MIC)
 *==========================================================================*/

/**
 * @brief Configure audio recording
 * @param config Audio configuration
 * @return HAL_OK on success
 */
hal_err_t hal_audio_config_record(const hal_audio_config_t *config);

/**
 * @brief Start recording
 * @param buffer Recording buffer
 * @param size Buffer size
 * @return HAL_OK on success
 */
hal_err_t hal_audio_record_start(void *buffer, size_t size);

/**
 * @brief Stop recording
 * @param recorded_size Actual recorded size (output)
 * @return HAL_OK on success
 */
hal_err_t hal_audio_record_stop(size_t *recorded_size);

/**
 * @brief Set recording callback (called when buffer is full)
 * @param cb Callback function
 * @param user_data User data
 */
void hal_audio_set_record_cb(hal_audio_callback_t cb, void *user_data);

/*============================================================================
 * PREDEFINED TONE SEQUENCES
 *==========================================================================*/

/** Beep tone */
#define HAL_TONE_BEEP       {{1000, 100}, {0, 0}}

/** Success sound */
#define HAL_TONE_SUCCESS    {{880, 100}, {0, 50}, {1047, 150}, {0, 0}}

/** Error sound */
#define HAL_TONE_ERROR      {{200, 200}, {0, 100}, {200, 200}, {0, 0}}

/** Notification sound */
#define HAL_TONE_NOTIFY     {{659, 100}, {0, 50}, {784, 100}, {0, 0}}

#ifdef __cplusplus
}
#endif

#endif /* HAL_AUDIO_H */
