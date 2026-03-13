#ifndef _USER_SETTINGS_H_
#define _USER_SETTINGS_H_

/* ESP32 Platform */
#define WOLFSSL_ESPIDF
#define WOLFSSL_ESP32
#define SINGLE_THREADED
#define NO_FILESYSTEM

/* WOLFCRYPT ONLY - we use mbedTLS for TLS, wolfSSL only for X448 */
#define WOLFCRYPT_ONLY
#define NO_WOLFSSL_SERVER
#define NO_WOLFSSL_CLIENT

/* X448/Curve448 - SimpleX E2E Double Ratchet */
#define HAVE_CURVE448
#define CURVE448_SMALL

/* X25519 */
#define HAVE_CURVE25519
#define CURVE25519_SMALL

/* Ed25519 */
#define HAVE_ED25519
#define ED25519_SMALL

/* Hashing */
#define WOLFSSL_SHA512
#define WOLFSSL_SHA384
#define WOLFSSL_SHA256

/* Random */
#define HAVE_HASHDRBG
#define WC_NO_HARDEN

/* Memory optimization for ESP32 */
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
#define NO_OLD_TLS

#endif
