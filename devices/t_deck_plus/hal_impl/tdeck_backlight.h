/**
 * @file tdeck_backlight.h
 * @brief T-Deck Plus Display Backlight (Pulse-Counting GPIO 42)
 *
 * The backlight chip uses a one-wire pulse-counting protocol with
 * exactly 16 brightness levels. NOT standard PWM/LEDC!
 * Reverse-engineered from LilyGO factory firmware UnitTest.ino:277-300.
 *
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef TDECK_BACKLIGHT_H
#define TDECK_BACKLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize display backlight (GPIO 42, starts at level 16 = max)
 */
void tdeck_backlight_init(void);

/**
 * @brief Set display brightness level
 * @param value 0=off, 1-16=brightness (16 = maximum)
 */
void tdeck_backlight_set(uint8_t value);

/**
 * @brief Get current display brightness level
 * @return Current level (0-16)
 */
uint8_t tdeck_backlight_get(void);

#ifdef __cplusplus
}
#endif

#endif /* TDECK_BACKLIGHT_H */
