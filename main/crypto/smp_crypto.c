/**
 * SimpleGo - smp_crypto.c
 * Cryptographic functions for SMP protocol
 */

#include "smp_crypto.h"
#include "smp_types.h"
#include <string.h>
#include "sodium.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "SMP_CRYP";

// ============== SMP Message Decryption (Layer 3) ==============

bool decrypt_smp_message(contact_t *c, const uint8_t *encrypted, int enc_len,
                         const uint8_t *nonce, uint8_t nonce_len,
                         uint8_t *plain, int *plain_len) {
    if (!c || !c->have_srv_dh || enc_len <= crypto_box_MACBYTES) {
        return false;
    }
    
    // Compute shared secret using crypto_box_beforenm
    // This does X25519 + HSalsa20 key derivation
    uint8_t shared[crypto_box_BEFORENMBYTES];
    if (crypto_box_beforenm(shared, c->srv_dh_public, c->rcv_dh_secret) != 0) {
        ESP_LOGE(TAG, "DH key computation failed");
        return false;
    }
    
    // Prepare nonce (24 bytes, zero-padded if needed)
    uint8_t full_nonce[crypto_box_NONCEBYTES];
    memset(full_nonce, 0, crypto_box_NONCEBYTES);
    int copy_len = (nonce_len < crypto_box_NONCEBYTES) ? nonce_len : crypto_box_NONCEBYTES;
    memcpy(full_nonce, nonce, copy_len);
    
    // Decrypt using crypto_box_open_easy_afternm
    if (crypto_box_open_easy_afternm(plain, encrypted, enc_len, full_nonce, shared) != 0) {
        ESP_LOGE(TAG, "Decryption failed");
        return false;
    }
    
    *plain_len = enc_len - crypto_box_MACBYTES;
    return true;
}

// ============== Client Message Decryption (Layer 5) ==============

int decrypt_client_msg(const uint8_t *enc, int enc_len,
                       const uint8_t *sender_dh_pub,
                       const uint8_t *our_dh_priv,
                       uint8_t *plain) {
    // crypto_box format: [24-byte nonce][ciphertext + 16-byte tag]
    if (enc_len < 24 + 16) {
        ESP_LOGE(TAG, "Client msg too short for crypto_box (%d)", enc_len);
        return -1;
    }
    
    const uint8_t *nonce = enc;
    const uint8_t *ciphertext = enc + 24;
    int ciphertext_len = enc_len - 24;
    
    ESP_LOGI(TAG, "Nonce (first 12): %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             nonce[0], nonce[1], nonce[2], nonce[3], nonce[4], nonce[5],
             nonce[6], nonce[7], nonce[8], nonce[9], nonce[10], nonce[11]);
    
    // crypto_box_open_easy(plain, ciphertext, ciphertext_len, nonce, sender_pub, our_priv)
    if (crypto_box_open_easy(plain, ciphertext, ciphertext_len, nonce, 
                              sender_dh_pub, our_dh_priv) != 0) {
        ESP_LOGE(TAG, "Client msg decryption failed!");
        return -1;
    }
    
    return ciphertext_len - 16;  // plaintext is ciphertext minus 16-byte tag
}

// ============== Encrypt for Peer ==============

int encrypt_for_peer(const uint8_t *plain, int plain_len,
                     const uint8_t *peer_dh_pub,
                     const uint8_t *our_dh_priv,
                     uint8_t *encrypted) {
    // Generate random nonce
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);
    
    // Output format: [24-byte nonce][ciphertext + 16-byte tag]
    memcpy(encrypted, nonce, 24);
    
    if (crypto_box_easy(&encrypted[24], plain, plain_len, nonce,
                        peer_dh_pub, our_dh_priv) != 0) {
        ESP_LOGE(TAG, "Encryption failed!");
        return -1;
    }
    
    return 24 + plain_len + crypto_box_MACBYTES;
}
