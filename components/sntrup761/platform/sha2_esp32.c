/*
 * sha2_esp32.c - SHA-512 via mbedTLS for PQClean compatibility
 *
 * Uses mbedtls_sha512() which automatically leverages the
 * ESP32-S3 SHA hardware accelerator when available.
 *
 * Public Domain (PQClean compatibility layer)
 */

#include "sha2.h"
#include "mbedtls/sha512.h"

void sha512(uint8_t *out, const uint8_t *in, size_t inlen) {
    /* mbedtls_sha512(input, ilen, output, is384)
     * is384 = 0 means SHA-512 (not SHA-384) */
    mbedtls_sha512(in, inlen, out, 0);
}
