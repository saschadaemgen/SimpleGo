/**
 * SimpleGo - smp_queue.c
 * SMP Queue Management (NEW, SUB, KEY, ACK commands)
 * v0.1.15-alpha - FIXED: NEW command now properly signed!
 * + DEBUG: E2E key tracking to find key mismatch bug
 * + Auftrag 42b: KEY command for peer auth registration
 */

#include "smp_queue.h"
#include "smp_types.h"
#include "smp_contacts.h"
#include "smp_network.h"
#include "smp_crypto.h"
#include "smp_storage.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
#include "sodium.h"

static const char *TAG = "SMP_QUEUE";

// Reply Queue E2E peer public key (extracted from AgentConnInfoReply)
uint8_t reply_queue_e2e_peer_public[32] = {0};
bool reply_queue_e2e_peer_valid = false;

// Global queue instance
our_queue_t our_queue = {0};

// Internal connection state for our queue server
static struct {
    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint8_t session_id[32];
    bool connected;
    bool initialized;
} queue_conn = {0};

// ============== Queue Connection ==============

static bool queue_connect(const char *host, int port) {
    int ret;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [PKG] CREATING OUR RECEIVE QUEUE                               |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[CONN] CONNECTING TO OUR SMP SERVER...");
    ESP_LOGI(TAG, "   Host: %s:%d", host, port);
    
    // Store server info
    strncpy(our_queue.server_host, host, sizeof(our_queue.server_host) - 1);
    our_queue.server_port = port;
    
    // Initialize mbedTLS
    mbedtls_ssl_init(&queue_conn.ssl);
    mbedtls_ssl_config_init(&queue_conn.conf);
    mbedtls_entropy_init(&queue_conn.entropy);
    mbedtls_ctr_drbg_init(&queue_conn.ctr_drbg);
    queue_conn.initialized = true;
    
    ret = mbedtls_ctr_drbg_seed(&queue_conn.ctr_drbg, mbedtls_entropy_func, 
                                 &queue_conn.entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] DRBG seed failed");
        return false;
    }
    
    // TCP connect
    ESP_LOGI(TAG, "   Attempting TCP connect...");
    queue_conn.sock = smp_tcp_connect(host, port);
    ESP_LOGI(TAG, "   TCP result: %d", queue_conn.sock);
    if (queue_conn.sock < 0) {
        ESP_LOGE(TAG, "   [FAIL] TCP connect failed");
        return false;
    }
    
    // TLS setup
    ESP_LOGI(TAG, "   Setting TLS defaults...");
    ret = mbedtls_ssl_config_defaults(&queue_conn.conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] mbedTLS failed: -0x%04X", -ret);
        close(queue_conn.sock);
        return false;
    }
    
    mbedtls_ssl_conf_min_tls_version(&queue_conn.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&queue_conn.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&queue_conn.conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&queue_conn.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&queue_conn.conf, mbedtls_ctr_drbg_random, &queue_conn.ctr_drbg);
    
    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&queue_conn.conf, alpn_list);
    
    ESP_LOGI(TAG, "   SSL setup...");
    ret = mbedtls_ssl_setup(&queue_conn.ssl, &queue_conn.conf);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] mbedTLS failed: -0x%04X", -ret);
        close(queue_conn.sock);
        return false;
    }
    
    mbedtls_ssl_set_hostname(&queue_conn.ssl, host);
    mbedtls_ssl_set_bio(&queue_conn.ssl, &queue_conn.sock, my_send_cb, my_recv_cb, NULL);
    
    // TLS handshake
    ESP_LOGI(TAG, "   Starting TLS handshake...");
    while ((ret = mbedtls_ssl_handshake(&queue_conn.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "   [FAIL] TLS handshake failed: -0x%04X", -ret);
            close(queue_conn.sock);
            return false;
        }
    }
    ESP_LOGI(TAG, "   [OK] TLS OK!");
    
    // Allocate block buffer
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "   [FAIL] Block alloc failed");
        return false;
    }
    
    // Wait for ServerHello
    int content_len = smp_read_block(&queue_conn.ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] No ServerHello");
        free(block);
        return false;
    }
    
    uint8_t *hello = block + 2;
    uint8_t sess_id_len = hello[4];
    if (sess_id_len != 32) {
        ESP_LOGE(TAG, "   [FAIL] Bad sessionId length");
        free(block);
        return false;
    }
    memcpy(queue_conn.session_id, &hello[5], 32);
    ESP_LOGI(TAG, "   SessionId: %02x%02x%02x%02x...", 
             queue_conn.session_id[0], queue_conn.session_id[1],
             queue_conn.session_id[2], queue_conn.session_id[3]);
    
    // Parse CA cert for keyHash
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, our_queue.server_key_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, our_queue.server_key_hash, 0);
    }
    
    ESP_LOGI(TAG, "   KeyHash: %02x%02x%02x%02x...",
             our_queue.server_key_hash[0], our_queue.server_key_hash[1],
             our_queue.server_key_hash[2], our_queue.server_key_hash[3]);
    
    // Send ClientHello
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], our_queue.server_key_hash, 32);
    pos += 32;
    
    int ret2 = smp_write_handshake_block(&queue_conn.ssl, block, client_hello, pos);
    free(block);
    
    if (ret2 != 0) {
        ESP_LOGE(TAG, "   [FAIL] ClientHello failed");
        return false;
    }
    
    ESP_LOGI(TAG, "   [OK] SMP Handshake complete!");
    queue_conn.connected = true;
    
    return true;
}

// ============== Create Queue ==============

bool queue_create(const char *host, int port) {
    // Connect to server
    if (!queue_connect(host, port)) {
        return false;
    }
    
    // Generate our keypairs FIRST (needed for signing!)
    ESP_LOGI(TAG, "   [KEY] Generating keypairs...");
    
    // Ed25519 for command signing (rcvAuthKey)
    crypto_sign_keypair(our_queue.rcv_auth_public, our_queue.rcv_auth_private);
    
    // X25519 for DH (rcvDhKey)
    crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);

    // E2E keypair (separate from server DH!)
    crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);
    
    ESP_LOGI(TAG, "E2E keypair generated: pub=%02x%02x%02x%02x..",
             our_queue.e2e_public[0], our_queue.e2e_public[1],
             our_queue.e2e_public[2], our_queue.e2e_public[3]);
    
    ESP_LOGI(TAG, "   Auth public: %02x%02x%02x%02x...",
             our_queue.rcv_auth_public[0], our_queue.rcv_auth_public[1],
             our_queue.rcv_auth_public[2], our_queue.rcv_auth_public[3]);
    ESP_LOGI(TAG, "   DH public: %02x%02x%02x%02x...",
             our_queue.rcv_dh_public[0], our_queue.rcv_dh_public[1],
             our_queue.rcv_dh_public[2], our_queue.rcv_dh_public[3]);
    
    /*
     * Build transmission body (what gets signed):
     * [corrIdLen][corrId][entityIdLen=0][command...]
     * 
     * NEW command format:
     * "NEW " + [len]rcvAuthKey(SPKI) + [len]rcvDhKey(SPKI) + subMode
     */
    
    uint8_t trans_body[256];
    int pos = 0;
    
    // CorrId (use simple "1" like smp_contacts.c does)
    trans_body[pos++] = 1;    // corrId length
    trans_body[pos++] = '1';  // corrId value
    
    // EntityId = empty for NEW (new queue has no ID yet)
    trans_body[pos++] = 0;
    
    // Command: "NEW "
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    // rcvAuthKey = Ed25519 SPKI (44 bytes: 12 header + 32 key)
    trans_body[pos++] = 44;  // length prefix
    memcpy(&trans_body[pos], ED25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], our_queue.rcv_auth_public, 32);
    pos += 32;
    
    // rcvDhKey = X25519 SPKI (44 bytes: 12 header + 32 key)
    trans_body[pos++] = 44;  // length prefix
    memcpy(&trans_body[pos], X25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], our_queue.rcv_dh_public, 32);
    pos += 32;
    
    // subMode = SMSubscribe = 'S' (we want to subscribe immediately)
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    ESP_LOGI(TAG, "   [SEND] NEW command body: %d bytes", trans_body_len);
    
    /*
     * Sign: smpEncode(sessionId) + transmission_body
     * smpEncode adds length prefix: [0x20][sessionId 32 bytes]
     * 
     * THIS IS THE FIX! We must sign with rcv_auth_private!
     */
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;  // Length prefix for sessionId
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    // Sign with Ed25519 using our rcvAuthKey private part
    uint8_t signature[crypto_sign_BYTES];  // 64 bytes
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);
    
    ESP_LOGI(TAG, "   [SIG] Signature: %02x%02x%02x%02x...%02x%02x%02x%02x",
             signature[0], signature[1], signature[2], signature[3],
             signature[60], signature[61], signature[62], signature[63]);
    
    // Verify signature locally (sanity check)
    int verify_result = crypto_sign_verify_detached(signature, to_sign, sign_pos, 
                                                     our_queue.rcv_auth_public);
    if (verify_result == 0) {
        ESP_LOGI(TAG, "   [OK] Signature verified locally!");
    } else {
        ESP_LOGE(TAG, "   [FAIL] Local signature verification FAILED!");
        return false;
    }
    
    /*
     * Build final transmission:
     *   [sigLen][signature 64 bytes]
     *   [sessLen][sessionId 32 bytes]
     *   [transmission_body]
     */
    uint8_t transmission[256];
    int tp = 0;
    
    // Signature WITH LENGTH PREFIX (64 bytes)
    transmission[tp++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    
    // SessionId WITH LENGTH PREFIX (32 bytes)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;
    
    // Transmission body (corrId + entityId + command)
    memcpy(&transmission[tp], trans_body, trans_body_len);
    tp += trans_body_len;
    
    ESP_LOGI(TAG, "   [NET] Full transmission: %d bytes", tp);
    
    // Debug: print first 20 bytes
    printf("      First 20 bytes: ");
    for (int i = 0; i < 20; i++) {
        printf("%02x ", transmission[i]);
    }
    printf("\n");
    
    // Send command
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send NEW failed!");
        free(block);
        return false;
    }
    
    ESP_LOGI(TAG, "   [SEND] NEW sent! Waiting for IDS...");
    
    // Wait for IDS response
    int content_len = smp_read_block(&queue_conn.ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] No response");
        free(block);
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Debug
    ESP_LOGI(TAG, "   [RECV] Response (%d bytes):", content_len);
    printf("      ");
    for (int i = 0; i < content_len && i < 80; i++) {
        printf("%02x ", resp[i]);
    }
    printf("\n");
    
    /*
     * Parse response - same format as what we receive in smp_contacts.c
     * Format: [1][txLen:2][authLen][auth...][sessLen][sess...][corrLen][corr...][entLen][ent...][IDS ...]
     */
    
    int p = 0;
    
    // 47d: txCount is a sequence counter, always consume
    p++;  // txCount
    p += 2;  // txLen
    
    int authLen = resp[p++];
    p += authLen;
    
    int sessLen = resp[p++];
    p += sessLen;
    
    int corrLen = resp[p++];
    p += corrLen;
    
    int entLen = resp[p++];
    p += entLen;
    
    // Now we should be at the command
    ESP_LOGI(TAG, "   Command at %d: %c%c%c", p, resp[p], resp[p+1], resp[p+2]);
    
    ESP_LOGD(TAG, "IDS response at offset %d, remaining %d bytes", p, content_len - p);
    
    if (resp[p] == 'I' && resp[p+1] == 'D' && resp[p+2] == 'S') {
        p += 3;  // Skip "IDS"
        if (resp[p] == ' ') p++;  // Skip space
        
        /*
         * Parse IDS response (same as smp_contacts.c):
         * [rcvIdLen][rcvId...][sndIdLen][sndId...][dhKeyLen][dhKey(SPKI)...]...
         */
        
        // rcvId (length-prefixed)
        our_queue.rcv_id_len = resp[p++];
        if (our_queue.rcv_id_len > QUEUE_ID_SIZE) {
            ESP_LOGE(TAG, "   [FAIL] rcvId too long: %d", our_queue.rcv_id_len);
            free(block);
            return false;
        }
        memcpy(our_queue.rcv_id, &resp[p], our_queue.rcv_id_len);
        p += our_queue.rcv_id_len;
        
        // sndId (length-prefixed)
        our_queue.snd_id_len = resp[p++];
        if (our_queue.snd_id_len > QUEUE_ID_SIZE) {
            ESP_LOGE(TAG, "   [FAIL] sndId too long: %d", our_queue.snd_id_len);
            free(block);
            return false;
        }
        memcpy(our_queue.snd_id, &resp[p], our_queue.snd_id_len);
        p += our_queue.snd_id_len;

    
        // rcvPublicDhKey = Server's X25519 SPKI
        int dhLen = resp[p++];
        if (dhLen != 44) {
            ESP_LOGE(TAG, "   [FAIL] Unexpected DH key length: %d", dhLen);
            free(block);
            return false;
        }
        // Skip SPKI header (12 bytes), copy raw key (32 bytes)
        memcpy(our_queue.srv_dh_public, &resp[p + 12], 32);
        ESP_LOGI(TAG, "   srv_dh_public: %02x%02x%02x%02x %02x%02x%02x%02x",
                 our_queue.srv_dh_public[0], our_queue.srv_dh_public[1],
                 our_queue.srv_dh_public[2], our_queue.srv_dh_public[3],
                 our_queue.srv_dh_public[4], our_queue.srv_dh_public[5],
                 our_queue.srv_dh_public[6], our_queue.srv_dh_public[7]);
        ESP_LOGI(TAG, "   rcv_dh_private: %02x%02x%02x%02x %02x%02x%02x%02x",
                 our_queue.rcv_dh_private[0], our_queue.rcv_dh_private[1],
                 our_queue.rcv_dh_private[2], our_queue.rcv_dh_private[3],
                 our_queue.rcv_dh_private[4], our_queue.rcv_dh_private[5],
                 our_queue.rcv_dh_private[6], our_queue.rcv_dh_private[7]);
        p += dhLen;
        
        // Compute shared secret for message decryption
        if (crypto_box_beforenm(our_queue.shared_secret,
                                our_queue.srv_dh_public,
                                our_queue.rcv_dh_private) != 0) {
            ESP_LOGE(TAG, "   [FAIL] DH failed");
            free(block);
            return false;
        }

        ESP_LOGI(TAG, "   shared_secret: %02x%02x%02x%02x..",
                 our_queue.shared_secret[0], our_queue.shared_secret[1],
                 our_queue.shared_secret[2], our_queue.shared_secret[3]);
        
        our_queue.valid = true;
        
        // Auftrag 50b Q2: Persist queue credentials after creation
        queue_save_credentials();

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "+----------------------------------------+");
        ESP_LOGI(TAG, "|   [OK] QUEUE CREATED SUCCESSFULLY!                             |");
        ESP_LOGI(TAG, "+----------------------------------------+");
        ESP_LOGI(TAG, "   rcvId (%d): %02x%02x%02x%02x...", 
                 our_queue.rcv_id_len,
                 our_queue.rcv_id[0], our_queue.rcv_id[1],
                 our_queue.rcv_id[2], our_queue.rcv_id[3]);
        ESP_LOGI(TAG, "   sndId (%d): %02x%02x%02x%02x...", 
                 our_queue.snd_id_len,
                 our_queue.snd_id[0], our_queue.snd_id[1],
                 our_queue.snd_id[2], our_queue.snd_id[3]);
        ESP_LOGI(TAG, "   Server DH: %02x%02x%02x%02x...",
                 our_queue.srv_dh_public[0], our_queue.srv_dh_public[1],
                 our_queue.srv_dh_public[2], our_queue.srv_dh_public[3]);
        
        free(block);
        return true;
        
    } else if (resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
        // Print full error message
        printf("      Error: ");
        for (int i = p; i < content_len && i < p + 40; i++) {
            char c = resp[i];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
        ESP_LOGE(TAG, "   [FAIL] Server error!");
    } else {
        ESP_LOGE(TAG, "   [FAIL] Unexpected response");
    }
    
    free(block);
    return false;
}

// ============== Encode Queue Info ==============

int queue_encode_info(uint8_t *buf, int max_len) {
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "queue_encode_info: queue not valid!");
        return -1;
    }

    int p = 0;

    // clientVersion = 4 (2 bytes BE)
    buf[p++] = 0x00;
    buf[p++] = 0x04;

    // smpServer: [host_count] [host_len] [host] [port_len] [port] [keyhash_len] [keyhash]
    int host_len = strlen(our_queue.server_host);

    buf[p++] = 0x01;  // 1 host
    buf[p++] = (uint8_t)host_len;
    memcpy(&buf[p], our_queue.server_host, host_len);
    p += host_len;

    // Port as string with LENGTH PREFIX (not space!)
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", our_queue.server_port);
    int port_len = strlen(port_str);
    buf[p++] = (uint8_t)port_len;  // Length prefix!
    memcpy(&buf[p], port_str, port_len);
    p += port_len;

    // KeyHash (32 bytes)
    buf[p++] = 32;
    memcpy(&buf[p], our_queue.server_key_hash, 32);
    p += 32;

    // senderId
    buf[p++] = (uint8_t)our_queue.snd_id_len;
    memcpy(&buf[p], our_queue.snd_id, our_queue.snd_id_len);
    p += our_queue.snd_id_len;

    // dhPublicKey (X25519 SPKI, 44 bytes)
    buf[p++] = 44;
    memcpy(&buf[p], X25519_SPKI_HEADER, 12);
    p += 12;
    memcpy(&buf[p], our_queue.e2e_public, 32);
    p += 32;
    
    ESP_LOGI(TAG, "SMPQueueInfo encoded: %d bytes, server=%s:%d",
             p, our_queue.server_host, our_queue.server_port);

    ESP_LOGI(TAG, "   Encoded SMPQueueInfo: %d bytes", p);
    return p;
}

// Pending message buffer: stores MSG received during ACK/SUB reads
static struct {
    uint8_t *data;
    int len;
    bool valid;
} pending_msg = {0};

// ============== Subscribe ==============

bool queue_subscribe(void) {
    if (!queue_conn.connected || !our_queue.valid) {
        ESP_LOGE(TAG, "queue_subscribe: not ready");
        return false;
    }
    
    ESP_LOGI(TAG, "   [RECV] Subscribing to queue...");
    
    /*
     * Build SUB command with proper signing (same pattern as queue_create)
     */
    
    // Build the unsigned body: corrId + entityId + command
    uint8_t body[128];
    int bp = 0;
    
    // CorrId
    body[bp++] = 1;
    body[bp++] = '2';  // Use different corrId
    
    // EntityId = our rcvId (for SUB we identify the queue)
    body[bp++] = (uint8_t)our_queue.rcv_id_len;
    memcpy(&body[bp], our_queue.rcv_id, our_queue.rcv_id_len);
    bp += our_queue.rcv_id_len;
    
    // Command: "SUB"
    body[bp++] = 'S';
    body[bp++] = 'U';
    body[bp++] = 'B';
    
    // Sign: [sessIdLen][sessId] + body
    uint8_t to_sign[1 + 32 + 128];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], body, bp);
    sign_pos += bp;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);
    
    // Build transmission with signature
    uint8_t transmission[256];
    int tp = 0;
    
    // Auth = Ed25519 signature (length-prefixed)
    transmission[tp++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    
    // SessionId
    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;
    
    // Body
    memcpy(&transmission[tp], body, bp);
    tp += bp;
    
    // Send
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send SUB failed!");
        free(block);
        return false;
    }
    
    // Wait for response
    int content_len = smp_read_block(&queue_conn.ssl, block, 5000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;

        // Scan for OK
        for (int i = 0; i < content_len - 1; i++) {
            if (resp[i] == 'O' && resp[i+1] == 'K') {
                ESP_LOGI(TAG, "   [OK] Subscribed!");
                free(block);
                return true;
            }
        }

        // Scan for END
        for (int i = 0; i < content_len - 2; i++) {
            if (resp[i] == 'E' && resp[i+1] == 'N' && resp[i+2] == 'D') {
                ESP_LOGI(TAG, "   [OK] Subscribed (END - queue empty)!");
                free(block);
                return true;
            }
        }

        // Scan for MSG (pending message - buffer it!)
        for (int i = 0; i < content_len - 2; i++) {
            if (resp[i] == 'M' && resp[i+1] == 'S' && resp[i+2] == 'G') {
                ESP_LOGI(TAG, "   [OK] Subscribed (server delivered pending MSG)");
                ESP_LOGI(TAG, "   [MSG] Storing MSG in pending buffer (%d bytes)", content_len + 2);
                pending_msg.data = malloc(content_len + 2);
                if (pending_msg.data) {
                    memcpy(pending_msg.data, block, content_len + 2);
                    pending_msg.len = content_len + 2;
                    pending_msg.valid = true;
                }
                free(block);
                return true;
            }
        }

        ESP_LOGW(TAG, "   [WARN] SUB response (%d bytes): no OK/END/MSG found", content_len);
    }

    free(block);
    return false;
}

// ======== Auftrag 42b: KEY Command ========

bool queue_send_key(const uint8_t *peer_auth_key_spki, int key_len) {
    if (!queue_conn.connected || !our_queue.valid) {
        ESP_LOGE(TAG, "queue_send_key: not ready (connected=%d, valid=%d)",
                 queue_conn.connected, our_queue.valid);
        return false;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [LOCK] SENDING KEY COMMAND (Register Peer Auth)                 |");
    ESP_LOGI(TAG, "+----------------------------------------+");

    // Body: corrId + entityId(rcvId) + "KEY " + [len]authKey
    uint8_t body[128];
    int bp = 0;

    body[bp++] = 1;      // corrId length
    body[bp++] = 'K';    // corrId value

    body[bp++] = (uint8_t)our_queue.rcv_id_len;
    memcpy(&body[bp], our_queue.rcv_id, our_queue.rcv_id_len);
    bp += our_queue.rcv_id_len;

    memcpy(&body[bp], "KEY ", 4);
    bp += 4;

    body[bp++] = (uint8_t)key_len;  // 44 = 0x2C
    memcpy(&body[bp], peer_auth_key_spki, key_len);
    bp += key_len;

    // Sign: [32-len-prefix][sessionId] + body
    uint8_t to_sign[1 + 32 + 128];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], body, bp);
    sign_pos += bp;

    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);

    // Transmission: [sigLen][sig][sessLen][sess][body]
    uint8_t transmission[256];
    int tp = 0;

    transmission[tp++] = crypto_sign_BYTES;
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;

    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;

    memcpy(&transmission[tp], body, bp);
    tp += bp;

    ESP_LOGI(TAG, "   [NET] KEY transmission: %d bytes", tp);
    ESP_LOGI(TAG, "   [KEY] Peer auth key: %02x%02x%02x%02x...%02x%02x%02x%02x",
             peer_auth_key_spki[0], peer_auth_key_spki[1],
             peer_auth_key_spki[2], peer_auth_key_spki[3],
             peer_auth_key_spki[40], peer_auth_key_spki[41],
             peer_auth_key_spki[42], peer_auth_key_spki[43]);

    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;

    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send KEY failed: %d", ret);
        free(block);
        return false;
    }

    ESP_LOGI(TAG, "   [SEND] KEY sent! Waiting for response...");

    int content_len = smp_read_block(&queue_conn.ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] No response!");
        free(block);
        return false;
    }

    uint8_t *resp = block + 2;

    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   [OK] KEY accepted! Peer auth registered on our queue.");
            free(block);
            return true;
        }
    }

    // Debug response
    ESP_LOGW(TAG, "   [WARN] KEY response (%d bytes):", content_len);
    printf("      ");
    for (int i = 0; i < content_len && i < 50; i++) printf("%02x ", resp[i]);
    printf("\n");

    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
            char err[32] = {0};
            int j = 0;
            for (int k = i + 4; k < content_len && j < 31 && resp[k] >= 0x20; k++)
                err[j++] = resp[k];
            ESP_LOGE(TAG, "   [STOP] Server Error: ERR %s", err);
            break;
        }
    }

    free(block);
    return false;
}

// ======== Ende Auftrag 42b ========

// ======== Auftrag 42d: Read from Reply Queue ========

int queue_read_raw(uint8_t *buf, int buf_size, int timeout_ms) {
    // Check pending buffer first (MSG caught during ACK/SUB)
    if (pending_msg.valid && pending_msg.data && pending_msg.len > 2) {
        int total = pending_msg.len;
        int copy_len = total < buf_size ? total : buf_size;
        memcpy(buf, pending_msg.data, copy_len);
        free(pending_msg.data);
        pending_msg.data = NULL;
        pending_msg.len = 0;
        pending_msg.valid = false;
        int content_len = total - 2;
        ESP_LOGI(TAG, "   [MSG] Returning pending MSG from buffer (content_len=%d)", content_len);
        return content_len;
    }
    
    if (!queue_conn.connected) {
        ESP_LOGE(TAG, "queue_read_raw: not connected!");
        return -1;
    }
    return smp_read_block(&queue_conn.ssl, buf, timeout_ms);
}

// === Auftrag 45m: Raw TLS read bypass (no parser) ===
int queue_raw_tls_read(uint8_t *buf, int buf_size, int timeout_ms) {
    if (!queue_conn.connected) {
        ESP_LOGE(TAG, "queue_raw_tls_read: not connected!");
        return -1;
    }
    mbedtls_ssl_conf_read_timeout(&queue_conn.conf, timeout_ms);
    int ret = mbedtls_ssl_read(&queue_conn.ssl, buf, buf_size);
    return ret;
}

bool queue_has_pending_msg(int *out_len) {
    if (pending_msg.valid && pending_msg.data && pending_msg.len > 0) {
        if (out_len) *out_len = pending_msg.len;
        return true;
    }
    return false;
}
// === Ende Auftrag 45m ===

// ======== Ende Auftrag 42d ========

// ======== Auftrag 45a: ACK Command ========

bool queue_send_ack(const uint8_t *msg_id, int msg_id_len) {
    if (!queue_conn.connected || !our_queue.valid) {
        ESP_LOGE(TAG, "queue_send_ack: not ready");
        return false;
    }
    
    ESP_LOGI(TAG, "   [MSG] Sending ACK for msgId %02x%02x%02x%02x...",
             msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
    
    // Build body: corrId + entityId(rcvId) + "ACK " + msgId
    uint8_t body[128];
    int bp = 0;
    
    body[bp++] = 1;
    body[bp++] = 'A';  // corrId
    
    body[bp++] = (uint8_t)our_queue.rcv_id_len;
    memcpy(&body[bp], our_queue.rcv_id, our_queue.rcv_id_len);
    bp += our_queue.rcv_id_len;
    
    memcpy(&body[bp], "ACK ", 4);
    bp += 4;
    
    // msgId (length-prefixed)
    body[bp++] = (uint8_t)msg_id_len;
    memcpy(&body[bp], msg_id, msg_id_len);
    bp += msg_id_len;
    
    // Sign
    uint8_t to_sign[1 + 32 + 128];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], body, bp);
    sign_pos += bp;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);
    
    // Transmission
    uint8_t transmission[256];
    int tp = 0;
    
    transmission[tp++] = crypto_sign_BYTES;
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    
    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;
    
    memcpy(&transmission[tp], body, bp);
    tp += bp;
    
    // Send
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send ACK failed!");
        free(block);
        return false;
    }
    
    int content_len = smp_read_block(&queue_conn.ssl, block, 5000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        
        // Scan for OK
        for (int i = 0; i < content_len - 1; i++) {
            if (resp[i] == 'O' && resp[i+1] == 'K') {
                ESP_LOGI(TAG, "   [OK] ACK accepted!");
                free(block);
                return true;
            }
        }
        
        // Scan for MSG (ACK accepted implicitly, server sent next message)
        for (int i = 0; i < content_len - 2; i++) {
            if (resp[i] == 'M' && resp[i+1] == 'S' && resp[i+2] == 'G') {
                ESP_LOGI(TAG, "   [OK] ACK accepted (implicit - server sent next MSG)");
                ESP_LOGI(TAG, "   [MSG] Storing MSG in pending buffer (%d bytes)", content_len + 2);
                pending_msg.data = malloc(content_len + 2);
                if (pending_msg.data) {
                    memcpy(pending_msg.data, block, content_len + 2);
                    pending_msg.len = content_len + 2;
                    pending_msg.valid = true;
                }
                free(block);
                return true;
            }
        }
        
        // Scan for END
        for (int i = 0; i < content_len - 2; i++) {
            if (resp[i] == 'E' && resp[i+1] == 'N' && resp[i+2] == 'D') {
                ESP_LOGI(TAG, "   [OK] ACK accepted (got END - queue empty)");
                free(block);
                return true;
            }
        }
        
        // Scan for ERR
        for (int i = 0; i < content_len - 2; i++) {
            if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
                ESP_LOGE(TAG, "   [FAIL] ACK error: ERR");
                free(block);
                return false;
            }
        }
        
        ESP_LOGW(TAG, "   [WARN] ACK response (%d bytes): no OK/MSG/END/ERR found", content_len);
    }
    
    ESP_LOGW(TAG, "   [WARN] ACK response timeout or parse error");
    free(block);
    return false;
}

// ======== Ende Auftrag 45a ========

// ======== Auftrag 42c: Reconnect ========

bool queue_reconnect(void) {
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "queue_reconnect: queue not valid!");
        return false;
    }
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [SYNC] RECONNECTING TO REPLY QUEUE SERVER                      |");
    ESP_LOGI(TAG, "+----------------------------------------+");

    // Cleanup old connection
    queue_disconnect();

    // Reconnect (TLS + SMP Handshake only, no NEW)
    return queue_connect(our_queue.server_host, our_queue.server_port);
}

// ======== Ende Auftrag 42c ========

// ============== Persistence (Auftrag 50b) ==============

bool queue_save_credentials(void) {
    if (!our_queue.valid) {
        ESP_LOGW(TAG, "queue_save_credentials: queue not valid, skipping");
        return false;
    }

    // Save our_queue struct
    esp_err_t ret = smp_storage_save_blob_sync("queue_our", &our_queue, sizeof(our_queue_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[FAIL] queue_save_credentials('queue_our') FAILED: %s", esp_err_to_name(ret));
        return false;
    }

    // Save reply queue E2E peer key (separate blob - set later during handshake)
    uint8_t e2e_blob[33];  // 32 bytes key + 1 byte valid flag
    memcpy(e2e_blob, reply_queue_e2e_peer_public, 32);
    e2e_blob[32] = reply_queue_e2e_peer_valid ? 1 : 0;
    ret = smp_storage_save_blob_sync("queue_e2e", e2e_blob, sizeof(e2e_blob));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[FAIL] queue_save_credentials('queue_e2e') FAILED: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "[SAVE] Queue credentials saved: our_queue (%zu bytes) + E2E peer (valid=%d)",
             sizeof(our_queue_t), reply_queue_e2e_peer_valid);
    ESP_LOGI(TAG, "   rcvId: %02x%02x%02x%02x... sndId: %02x%02x%02x%02x...",
             our_queue.rcv_id[0], our_queue.rcv_id[1], our_queue.rcv_id[2], our_queue.rcv_id[3],
             our_queue.snd_id[0], our_queue.snd_id[1], our_queue.snd_id[2], our_queue.snd_id[3]);
    return true;
}

bool queue_load_credentials(void) {
    if (!smp_storage_exists("queue_our")) {
        ESP_LOGI(TAG, "queue_load_credentials: 'queue_our' not found - fresh start");
        return false;
    }

    // Load our_queue into temporary, validate, then accept
    our_queue_t loaded;
    size_t loaded_len = 0;
    esp_err_t ret = smp_storage_load_blob("queue_our", &loaded, sizeof(our_queue_t), &loaded_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[FAIL] queue_load_credentials: load failed: %s", esp_err_to_name(ret));
        return false;
    }
    if (loaded_len != sizeof(our_queue_t)) {
        ESP_LOGE(TAG, "[FAIL] queue_load_credentials: size mismatch! got %zu, expected %zu",
                 loaded_len, sizeof(our_queue_t));
        return false;
    }
    if (!loaded.valid) {
        ESP_LOGW(TAG, "[FAIL] queue_load_credentials: loaded queue has valid=false!");
        return false;
    }

    // Accept
    memcpy(&our_queue, &loaded, sizeof(our_queue_t));

    // Load reply queue E2E peer key (optional - may not be set yet)
    uint8_t e2e_blob[33];
    size_t e2e_len = 0;
    ret = smp_storage_load_blob("queue_e2e", e2e_blob, sizeof(e2e_blob), &e2e_len);
    if (ret == ESP_OK && e2e_len == 33) {
        memcpy(reply_queue_e2e_peer_public, e2e_blob, 32);
        reply_queue_e2e_peer_valid = (e2e_blob[32] == 1);
        ESP_LOGI(TAG, "   E2E peer key restored (valid=%d)", reply_queue_e2e_peer_valid);
    } else {
        ESP_LOGW(TAG, "   E2E peer key not found - will be set during handshake");
        reply_queue_e2e_peer_valid = false;
    }

    ESP_LOGI(TAG, "[LOAD] Queue credentials restored: our_queue (%zu bytes)",
             loaded_len);
    ESP_LOGI(TAG, "   rcvId (%d): %02x%02x%02x%02x... sndId (%d): %02x%02x%02x%02x...",
             our_queue.rcv_id_len,
             our_queue.rcv_id[0], our_queue.rcv_id[1], our_queue.rcv_id[2], our_queue.rcv_id[3],
             our_queue.snd_id_len,
             our_queue.snd_id[0], our_queue.snd_id[1], our_queue.snd_id[2], our_queue.snd_id[3]);
    ESP_LOGI(TAG, "   server: %s:%d", our_queue.server_host, our_queue.server_port);
    return true;
}

// ============== Disconnect ==============

void queue_disconnect(void) {
    if (queue_conn.connected || queue_conn.initialized) {
        mbedtls_ssl_close_notify(&queue_conn.ssl);
        mbedtls_ssl_free(&queue_conn.ssl);
        mbedtls_ssl_config_free(&queue_conn.conf);
        mbedtls_ctr_drbg_free(&queue_conn.ctr_drbg);
        mbedtls_entropy_free(&queue_conn.entropy);
        if (queue_conn.sock >= 0) close(queue_conn.sock);
        queue_conn.connected = false;
        queue_conn.initialized = false;
        ESP_LOGI(TAG, "   [DISC] Queue connection closed");
    }
}