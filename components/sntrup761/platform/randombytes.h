/*
 * randombytes.h - PQClean-compatible random bytes interface for ESP32-S3
 *
 * Wraps esp_fill_random() which uses the ESP32-S3 hardware RNG.
 * When WiFi or BT is enabled, the RNG is fed by true hardware entropy.
 * When neither is active, it uses a PRNG seeded by hardware noise.
 *
 * Public Domain (PQClean compatibility layer)
 */

#ifndef PQCLEAN_RANDOMBYTES_H
#define PQCLEAN_RANDOMBYTES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define randombytes PQCLEAN_randombytes

int randombytes(uint8_t *output, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* PQCLEAN_RANDOMBYTES_H */
