/**
 * SimpleGo - smp_x448.c
 * X448 Elliptic Curve Diffie-Hellman for E2E ratchet
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_x448.h"
#include "kem.h"  // Kyber KEM
#include "smp_utils.h"
#include <string.h>
#include "esp_log.h"

// wolfSSL headers
#include <wolfssl/wolfcrypt/settings.h>

#ifdef HAVE_CURVE448
#include <wolfssl/wolfcrypt/curve448.h>
#include <wolfssl/wolfcrypt/random.h>
#endif

static const char *TAG = "SMP_X448";

// X448 SPKI header: SEQUENCE(66) { SEQUENCE(5) { OID(1.3.101.111) } BITSTRING(57) }
const uint8_t X448_SPKI_HEADER[12] = {
    0x30, 0x42,  // SEQUENCE, 66 bytes
    0x30, 0x05,  // SEQUENCE, 5 bytes (algorithm)
    0x06, 0x03,  // OID, 3 bytes
    0x2b, 0x65, 0x6f,  // 1.3.101.111 (X448)
    0x03, 0x39,  // BIT STRING, 57 bytes
    0x00         // no unused bits
};

// Helper function to reverse byte order
// wolfSSL uses different byte order than cryptonite/Python cryptography
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

#ifdef HAVE_CURVE448

// Global RNG for wolfSSL
static WC_RNG rng;
static bool rng_initialized = false;

bool x448_init(void) {
    if (rng_initialized) {
        return true;
    }
    
    int ret = wc_InitRng(&rng);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to init RNG: %d", ret);
        return false;
    }
    
    rng_initialized = true;
    ESP_LOGI(TAG, "✅ X448 crypto initialized (wolfSSL)");
    return true;
}

bool x448_generate_keypair(x448_keypair_t *keypair) {
    if (!rng_initialized && !x448_init()) {
        return false;
    }
    
    curve448_key key;
    int ret;
    
    ret = wc_curve448_init(&key);
    if (ret != 0) {
        ESP_LOGE(TAG, "curve448_init failed: %d", ret);
        return false;
    }
    
    ret = wc_curve448_make_key(&rng, CURVE448_KEY_SIZE, &key);
    if (ret != 0) {
        ESP_LOGE(TAG, "curve448_make_key failed: %d", ret);
        wc_curve448_free(&key);
        return false;
    }
    
    // Export and REVERSE public key for cryptonite compatibility
    uint8_t pub_tmp[56];
    word32 pub_len = X448_KEY_SIZE;
    ret = wc_curve448_export_public_ex(&key, pub_tmp, &pub_len, EC448_BIG_ENDIAN);
    if (ret != 0) {
        ESP_LOGE(TAG, "export_public failed: %d", ret);
        wc_curve448_free(&key);
        return false;
    }
    reverse_bytes(pub_tmp, keypair->public_key, 56);
    
    // Export and REVERSE private key for cryptonite compatibility
    uint8_t priv_tmp[56];
    word32 priv_len = X448_KEY_SIZE;
    ret = wc_curve448_export_private_raw_ex(&key, priv_tmp, &priv_len, EC448_BIG_ENDIAN);
    if (ret != 0) {
        ESP_LOGE(TAG, "export_private failed: %d", ret);
        wc_curve448_free(&key);
        return false;
    }
    reverse_bytes(priv_tmp, keypair->private_key, 56);
    
    wc_curve448_free(&key);
    
    ESP_LOGI(TAG, "🔑 X448 keypair generated!");
    ESP_LOGI(TAG, "   Public: %02x%02x%02x%02x...", 
             keypair->public_key[0], keypair->public_key[1],
             keypair->public_key[2], keypair->public_key[3]);
    return true;
}

bool x448_dh(const uint8_t *their_public, 
             const uint8_t *my_private,
             uint8_t *shared_secret) {
    
    curve448_key my_key, their_key;
    int ret;
    
    // REVERSE inputs for wolfSSL (cryptonite uses opposite byte order)
    uint8_t their_public_rev[56];
    uint8_t my_private_rev[56];
    reverse_bytes(their_public, their_public_rev, 56);
    reverse_bytes(my_private, my_private_rev, 56);
    
    // Initialize and import our private key (REVERSED)
    ret = wc_curve448_init(&my_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "my_key init failed: %d", ret);
        return false;
    }
    
    ret = wc_curve448_import_private_ex(my_private_rev, X448_KEY_SIZE, &my_key, EC448_BIG_ENDIAN);
    if (ret != 0) {
        ESP_LOGE(TAG, "import_private failed: %d", ret);
        wc_curve448_free(&my_key);
        return false;
    }
    
    // Initialize and import their public key (REVERSED)
    ret = wc_curve448_init(&their_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "their_key init failed: %d", ret);
        wc_curve448_free(&my_key);
        return false;
    }
    
    ret = wc_curve448_import_public_ex(their_public_rev, X448_KEY_SIZE, &their_key, EC448_BIG_ENDIAN);
    if (ret != 0) {
        ESP_LOGE(TAG, "import_public failed: %d", ret);
        wc_curve448_free(&my_key);
        wc_curve448_free(&their_key);
        return false;
    }
    
    // Perform DH
    uint8_t secret_tmp[56];
    word32 secret_len = X448_KEY_SIZE;
    ret = wc_curve448_shared_secret_ex(&my_key, &their_key, secret_tmp, &secret_len, EC448_BIG_ENDIAN);
    
    wc_curve448_free(&my_key);
    wc_curve448_free(&their_key);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "shared_secret failed: %d", ret);
        return false;
    }
    
    // REVERSE output to standard format (cryptonite compatibility)
    reverse_bytes(secret_tmp, shared_secret, 56);
    
    ESP_LOGD(TAG, "🤝 X448 DH complete! Secret: %02x%02x%02x%02x...",
             shared_secret[0], shared_secret[1], shared_secret[2], shared_secret[3]);
    return true;
}

#else /* !HAVE_CURVE448 */

bool x448_init(void) {
    ESP_LOGE(TAG, "❌ X448 not available - wolfSSL not configured with HAVE_CURVE448!");
    return false;
}

bool x448_generate_keypair(x448_keypair_t *keypair) {
    (void)keypair;
    ESP_LOGE(TAG, "❌ X448 not available!");
    return false;
}

bool x448_dh(const uint8_t *their_public, const uint8_t *my_private, uint8_t *shared_secret) {
    (void)their_public;
    (void)my_private;
    (void)shared_secret;
    ESP_LOGE(TAG, "❌ X448 not available!");
    return false;
}

#endif /* HAVE_CURVE448 */

// These functions work without wolfSSL

void x448_encode_spki(const uint8_t *public_key, uint8_t *output) {
    memcpy(output, X448_SPKI_HEADER, 12);
    memcpy(output + 12, public_key, X448_KEY_SIZE);
}

int x448_encode_base64url(const uint8_t *public_key, char *output) {
    uint8_t spki[X448_SPKI_SIZE];
    x448_encode_spki(public_key, spki);
    return base64url_encode(spki, X448_SPKI_SIZE, output, 100);
}

bool e2e_generate_params(e2e_params_t *params) {
    params->version_min = 3;  // v3: App uses this for encodeLarge() prefix size
    params->version_max = 3;
    params->has_kem = false;

    if (!x448_generate_keypair(&params->key1)) {
        ESP_LOGE(TAG, "Failed to generate key1");
        return false;
    }

    if (!x448_generate_keypair(&params->key2)) {
        ESP_LOGE(TAG, "Failed to generate key2");
        return false;
    }

    // Generate Kyber1024 keypair for PQ
    if (crypto_kem_keypair(params->kem_public_key, params->kem_secret_key) != 0) {
        ESP_LOGE(TAG, "Failed to generate Kyber keypair");
        return false;
    }
    params->has_kem = false;
    ESP_LOGI(TAG, "🔐 Kyber1024 keypair generated!");

    ESP_LOGI(TAG, "✅ E2E params generated (v%d-%d, PQ=%d)",
             params->version_min, params->version_max, params->has_kem);
    return true;
}

int e2e_encode_params(const e2e_params_t *params, uint8_t *output) {
    int offset = 0;
    
    // Version (2 bytes BE)
    output[offset++] = 0x00;
    output[offset++] = params->version_min;
    
    // Key1 - 1-BYTE length prefix + SPKI (68 bytes)
    output[offset++] = 68;    // 1 BYTE only!
    x448_encode_spki(params->key1.public_key, output + offset);
    offset += 68;
    
    // Key2 - 1-BYTE length prefix + SPKI (68 bytes)  
    output[offset++] = 68;    // 1 BYTE only!
    x448_encode_spki(params->key2.public_key, output + offset);
    offset += 68;
    
    // KEM Nothing - v3 requires Maybe-encoding for KEM field
    // '0' (0x30) = Nothing in SimpleX Maybe-encoding
    output[offset++] = 0x30;  // '0' = No KEM/PQ
    
    ESP_LOGI(TAG, "📦 E2E params encoded: %d bytes (v3, no KEM)", offset);
    return offset;  // 2 + 69 + 69 + 1 = 141 bytes
}