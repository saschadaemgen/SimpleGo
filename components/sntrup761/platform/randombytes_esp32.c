/*
 * randombytes_esp32.c - ESP32-S3 hardware RNG wrapper for PQClean
 *
 * Uses esp_fill_random() from ESP-IDF, which provides:
 * - True random numbers when WiFi/BT is active (hardware entropy source)
 * - PRNG seeded from hardware noise when WiFi/BT is inactive
 *
 * Public Domain (PQClean compatibility layer)
 */

#include "randombytes.h"
#include "esp_random.h"

int PQCLEAN_randombytes(uint8_t *output, size_t n) {
    esp_fill_random(output, n);
    return 0;
}
