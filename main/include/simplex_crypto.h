/**
 * SimpleX Custom XSalsa20-Poly1305 Decryption
 * ============================================
 * 
 * Header file for SimpleX E2E message decryption.
 * 
 * Usage in main.c for Reply Queue E2E decryption:
 * 
 *   #include "simplex_crypto.h"
 *   
 *   // After server-level decryption...
 *   uint8_t dh_secret[32];
 *   crypto_scalarmult(dh_secret, our_queue.e2e_private, sender_pub);
 *   
 *   const uint8_t *cm_nonce = &server_plain[60];
 *   const uint8_t *e2e_encrypted = &server_plain[84];
 *   size_t e2e_len = plain_len - 84;
 *   
 *   uint8_t *e2e_plain = malloc(e2e_len - 16);
 *   int ret = simplex_secretbox_open(e2e_plain, e2e_encrypted, e2e_len,
 *                                     cm_nonce, dh_secret);
 *   if (ret == 0) {
 *       // Success! Parse ClientMessage...
 *   }
 */

#ifndef SIMPLEX_CRYPTO_H
#define SIMPLEX_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/**
 * Decrypt a SimpleX cbEncrypt message (E2E Layer)
 * 
 * SimpleX uses a NON-STANDARD XSalsa20 implementation:
 *   1. subkey1 = HSalsa20(dh_secret, zeros[16])    <- NOT nonce[0:16]!
 *   2. subkey2 = HSalsa20(subkey1, nonce[8:24])
 *   3. stream  = Salsa20(subkey2, nonce[0:8])
 * 
 * @param plain     Output buffer for plaintext (must be cipherlen - 16 bytes)
 * @param cipher    Input: [MAC 16][ciphertext]
 * @param cipherlen Length of cipher (MAC + ciphertext)
 * @param nonce24   24-byte nonce (cmNonce from message)
 * @param dh_secret 32-byte raw X25519 DH secret (from crypto_scalarmult)
 * @return          0 on success, -1 on MAC verification failure
 */
int simplex_secretbox_open(uint8_t *plain, 
                           const uint8_t *cipher, 
                           size_t cipherlen,
                           const uint8_t *nonce24, 
                           const uint8_t *dh_secret);

/**
 * Encrypt a message using SimpleX cbEncrypt
 * 
 * @param cipher    Output: [MAC 16][ciphertext] (must be plainlen + 16 bytes)
 * @param plain     Input plaintext
 * @param plainlen  Length of plaintext
 * @param nonce24   24-byte nonce (generate with randombytes)
 * @param dh_secret 32-byte raw X25519 DH secret
 * @return          0 on success
 */
int simplex_secretbox(uint8_t *cipher,
                      const uint8_t *plain,
                      size_t plainlen,
                      const uint8_t *nonce24,
                      const uint8_t *dh_secret);

/**
 * Debug version with verbose logging (printf)
 */
int simplex_secretbox_open_debug(uint8_t *plain, 
                                  const uint8_t *cipher, 
                                  size_t cipherlen,
                                  const uint8_t *nonce24, 
                                  const uint8_t *dh_secret,
                                  const char *tag);

#endif /* SIMPLEX_CRYPTO_H */
