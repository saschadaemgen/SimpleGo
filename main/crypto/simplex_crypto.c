/**
 * SimpleX Custom XSalsa20 Implementation for ESP32
 * ================================================
 * 
 * SimpleX's cbEncrypt/cbDecrypt uses a NON-STANDARD XSalsa20 variant!
 * 
 * Standard XSalsa20 (libsodium):
 *   1. subkey = HSalsa20(key, nonce[0:16])
 *   2. stream = Salsa20(subkey, zeros[8] || nonce[16:24])
 * 
 * SimpleX XSalsa20 (cryptonite):
 *   1. subkey1 = HSalsa20(key, zeros[16])
 *   2. subkey2 = HSalsa20(subkey1, nonce[8:24])
 *   3. stream  = Salsa20(subkey2, zeros[8] || nonce[0:8])
 * 
 * This file provides simplex_secretbox_open() which correctly decrypts
 * SimpleX E2E encrypted messages.
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "sodium.h"

/**
 * Decrypt a SimpleX cbEncrypt message
 * 
 * @param plain      Output plaintext buffer (must be at least cipherlen - 16 bytes)
 * @param cipher     Input: [MAC 16 bytes][ciphertext]
 * @param cipherlen  Total length of cipher (MAC + ciphertext)
 * @param nonce24    24-byte nonce (cmNonce from message)
 * @param dh_secret  32-byte raw X25519 DH secret (NOT HSalsa20-derived!)
 * @return           0 on success, -1 on MAC verification failure
 * 
 * Usage:
 *   uint8_t dh_secret[32];
 *   crypto_scalarmult(dh_secret, our_private, peer_public);
 *   int ret = simplex_secretbox_open(plain, cipher, len, nonce, dh_secret);
 */
int simplex_secretbox_open(uint8_t *plain, 
                           const uint8_t *cipher, 
                           size_t cipherlen,
                           const uint8_t *nonce24, 
                           const uint8_t *dh_secret)
{
    // Minimum size: 16 byte MAC
    if (cipherlen < 16) {
        return -1;
    }
    
    // Split input: [MAC 16][ciphertext]
    const uint8_t *mac = cipher;           // First 16 bytes = Poly1305 tag
    const uint8_t *ct = cipher + 16;       // Remaining = ciphertext
    size_t ct_len = cipherlen - 16;
    
    // === STEP 1: HSalsa20(dh_secret, zeros[16]) ===
    // SimpleX uses zeros instead of nonce[0:16]!
    uint8_t zeros_16[16] = {0};
    uint8_t subkey1[32];
    
    // crypto_core_hsalsa20(out, in, key, c)
    //   out: 32-byte output subkey
    //   in:  16-byte input (nonce for HSalsa20)
    //   key: 32-byte key
    //   c:   NULL for default constants
    crypto_core_hsalsa20(subkey1, zeros_16, dh_secret, NULL);
    
    // === STEP 2: HSalsa20(subkey1, nonce[8:24]) ===
    // This is cryptonite's XSalsa.derive step
    uint8_t subkey2[32];
    crypto_core_hsalsa20(subkey2, &nonce24[8], subkey1, NULL);
    
    // === STEP 3: Generate Salsa20 keystream ===
    // Salsa20 nonce is: zeros[8] || nonce[0:8]
    // But crypto_stream_salsa20 uses 8-byte nonce directly
    // In XSalsa20, the nonce[0:8] becomes the Salsa20 block counter seed
    
    // Allocate keystream: 32 bytes for Poly1305 + ct_len for decryption
    size_t stream_len = 32 + ct_len;
    uint8_t *stream = (uint8_t *)malloc(stream_len);
    if (!stream) {
        sodium_memzero(subkey1, 32);
        sodium_memzero(subkey2, 32);
        return -1;
    }
    
    // Generate keystream with Salsa20
    // Nonce for Salsa20 is nonce[0:8] (the iv0 from SimpleX)
    crypto_stream_salsa20(stream, stream_len, nonce24, subkey2);
    
    // First 32 bytes = Poly1305 one-time key (rs in Haskell)
    uint8_t *poly_key = stream;
    
    // === STEP 4: Verify Poly1305 MAC ===
    // SimpleX computes: tag = Poly1305.auth(rs, ciphertext)
    uint8_t computed_mac[16];
    crypto_onetimeauth(computed_mac, ct, ct_len, poly_key);
    
    // Constant-time comparison
    int mac_valid = crypto_verify_16(mac, computed_mac);
    
    if (mac_valid != 0) {
        // MAC mismatch - don't decrypt
        free(stream);
        sodium_memzero(subkey1, 32);
        sodium_memzero(subkey2, 32);
        return -1;
    }
    
    // === STEP 5: Decrypt by XORing with keystream[32:] ===
    for (size_t i = 0; i < ct_len; i++) {
        plain[i] = ct[i] ^ stream[32 + i];
    }
    
    // Clean up sensitive data
    free(stream);
    sodium_memzero(subkey1, 32);
    sodium_memzero(subkey2, 32);
    
    return 0;  // Success!
}

/**
 * Encrypt a message using SimpleX cbEncrypt
 * (For completeness - sending messages from ESP32)
 * 
 * @param cipher     Output: [MAC 16][ciphertext] (must be plainlen + 16 bytes)
 * @param plain      Input plaintext
 * @param plainlen   Length of plaintext
 * @param nonce24    24-byte nonce (generate with randombytes)
 * @param dh_secret  32-byte raw X25519 DH secret
 * @return           0 on success
 */
int simplex_secretbox(uint8_t *cipher,
                      const uint8_t *plain,
                      size_t plainlen,
                      const uint8_t *nonce24,
                      const uint8_t *dh_secret)
{
    uint8_t zeros_16[16] = {0};
    uint8_t subkey1[32], subkey2[32];
    
    // Step 1: HSalsa20(dh_secret, zeros[16])
    crypto_core_hsalsa20(subkey1, zeros_16, dh_secret, NULL);
    
    // Step 2: HSalsa20(subkey1, nonce[8:24])
    crypto_core_hsalsa20(subkey2, &nonce24[8], subkey1, NULL);
    
    // Step 3: Generate keystream
    size_t stream_len = 32 + plainlen;
    uint8_t *stream = (uint8_t *)malloc(stream_len);
    if (!stream) {
        sodium_memzero(subkey1, 32);
        sodium_memzero(subkey2, 32);
        return -1;
    }
    
    crypto_stream_salsa20(stream, stream_len, nonce24, subkey2);
    
    // Step 4: Encrypt by XORing with keystream[32:]
    uint8_t *ct = cipher + 16;  // Leave room for MAC
    for (size_t i = 0; i < plainlen; i++) {
        ct[i] = plain[i] ^ stream[32 + i];
    }
    
    // Step 5: Compute Poly1305 MAC over ciphertext
    crypto_onetimeauth(cipher, ct, plainlen, stream);
    
    // Clean up
    free(stream);
    sodium_memzero(subkey1, 32);
    sodium_memzero(subkey2, 32);
    
    return 0;
}

