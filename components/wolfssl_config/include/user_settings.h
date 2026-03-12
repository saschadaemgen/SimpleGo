#ifndef _USER_SETTINGS_H_
#define _USER_SETTINGS_H_

/* ESP32 Platform - NO FreeRTOS threading */
#define WOLFSSL_ESPIDF
#define WOLFSSL_ESP32
#define SINGLE_THREADED
#define NO_FILESYSTEM

/* TLS 1.3 */
#define WOLFSSL_TLS13
#define NO_OLD_TLS
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_ALPN
#define HAVE_SNI

/* X448/Curve448 - SimpleX E2E Ratchet! */
#define HAVE_CURVE448
#define CURVE448_SMALL

/* X25519 */
#define HAVE_CURVE25519
#define CURVE25519_SMALL

/* Ed25519 */
#define HAVE_ED25519
#define ED25519_SMALL

/* ChaCha20-Poly1305 */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_AESGCM

/* Hashing */
#define WOLFSSL_SHA512
#define WOLFSSL_SHA384
#define WOLFSSL_SHA256

/* Memory */
#define WOLFSSL_SMALL_STACK
#define WOLFSSL_SP
#define WOLFSSL_SP_SMALL
#define SP_WORD_SIZE 32

/* Disable unused */
#define NO_RSA
#define NO_DSA
#define NO_RC4
#define NO_MD4
#define NO_DES3
#define NO_PSK

#endif
