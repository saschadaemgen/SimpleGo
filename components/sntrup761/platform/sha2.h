/*
 * sha2.h - PQClean-compatible SHA-2 interface for ESP32-S3
 *
 * Only sha512() is needed by sntrup761. Implemented via mbedTLS
 * which can use the ESP32-S3 SHA hardware accelerator.
 *
 * Public Domain (PQClean compatibility layer)
 */

#ifndef SHA2_H
#define SHA2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All-in-one SHA-512 function.
 * This is the only function used by sntrup761 kem.c.
 */
void sha512(uint8_t *out, const uint8_t *in, size_t inlen);

#ifdef __cplusplus
}
#endif

#endif /* SHA2_H */
