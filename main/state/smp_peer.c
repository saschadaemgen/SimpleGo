/**
 * SimpleGo - smp_peer.c
 * Peer server connection for AgentConfirmation
 * v0.1.14-alpha - Corrected AgentConfirmation format
 */

#include "smp_peer.h"
#include "smp_types.h"
#include "smp_network.h"
#include "smp_crypto.h"
#include <string.h>
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
#include "smp_x448.h"      // NEU: X448 E2E Ratchet
#include "smp_ratchet.h"   // NEU
#include "smp_queue.h"     // NEU: Für unsere Queue
#include "reply_queue.h"   // per-contact reply queues
#include "smp_handshake.h"
#include "smp_storage.h"   // NVS persistence
#include "smp_contacts.h"  // contact index resolution

static const char *TAG = "SMP_PEER";

// ============== Global Definitions ==============

peer_queue_t pending_peer = {0};
peer_connection_t peer_conn = {0};

// TODO: move to smp_network.h
extern const int ciphersuites[];
extern int my_send_cb(void *ctx, const unsigned char *buf, size_t len);
extern int my_recv_cb(void *ctx, unsigned char *buf, size_t len);
extern int smp_tcp_connect(const char *host, int port);
extern int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms);

// ============== Peer Connection State ==============

static struct {
    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint8_t session_id[32];
    uint8_t server_key_hash[32];
    bool connected;
    bool initialized;
    char last_host[64];   // saved for reconnect
    int last_port;        // saved for reconnect
} peer_state = {0};

// ============== Peer Connection ==============

bool peer_connect(const char *host, int port) {
    int ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔗 CONNECTING TO PEER SERVER...");
    ESP_LOGI(TAG, "   Host: %s:%d", host, port);

    // Save for reconnect
    strncpy(peer_state.last_host, host, sizeof(peer_state.last_host) - 1);
    peer_state.last_port = port;

    // Initialize mbedTLS
    mbedtls_ssl_init(&peer_state.ssl);
    mbedtls_ssl_config_init(&peer_state.conf);
    mbedtls_entropy_init(&peer_state.entropy);
    mbedtls_ctr_drbg_init(&peer_state.ctr_drbg);
    peer_state.initialized = true;

    ret = mbedtls_ctr_drbg_seed(&peer_state.ctr_drbg, mbedtls_entropy_func,
                                &peer_state.entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ DRBG seed failed");
        return false;
    }

    // TCP connect
    peer_state.sock = smp_tcp_connect(host, port);
    if (peer_state.sock < 0) {
        ESP_LOGE(TAG, "   ❌ TCP connect failed");
        return false;
    }

    // TLS setup
    ret = mbedtls_ssl_config_defaults(&peer_state.conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        close(peer_state.sock);
        return false;
    }

    mbedtls_ssl_conf_min_tls_version(&peer_state.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&peer_state.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&peer_state.conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&peer_state.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&peer_state.conf, mbedtls_ctr_drbg_random, &peer_state.ctr_drbg);

    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&peer_state.conf, alpn_list);

    ret = mbedtls_ssl_setup(&peer_state.ssl, &peer_state.conf);
    if (ret != 0) {
        close(peer_state.sock);
        return false;
    }

    mbedtls_ssl_set_hostname(&peer_state.ssl, host);
    mbedtls_ssl_set_bio(&peer_state.ssl, &peer_state.sock, my_send_cb, my_recv_cb, NULL);

    // TLS handshake
    while ((ret = mbedtls_ssl_handshake(&peer_state.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "   ❌ TLS handshake failed: -0x%04X", -ret);
            close(peer_state.sock);
            return false;
        }
    }
    ESP_LOGI(TAG, "   ✅ TLS OK!");

    // Allocate block buffer
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "   ❌ Block alloc failed");
        return false;
    }

    // Wait for ServerHello
    int content_len = smp_read_block(&peer_state.ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   ❌ No ServerHello");
        free(block);
        return false;
    }

    uint8_t *hello = block + 2;
    uint8_t sess_id_len = hello[4];
    if (sess_id_len != 32) {
        ESP_LOGE(TAG, "   ❌ Bad sessionId length");
        free(block);
        return false;
    }

    memcpy(peer_state.session_id, &hello[5], 32);
    ESP_LOGI(TAG, "   SessionId: %02x%02x%02x%02x...",
             peer_state.session_id[0], peer_state.session_id[1],
             peer_state.session_id[2], peer_state.session_id[3]);

    // Parse CA cert for keyHash
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);

    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, peer_state.server_key_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, peer_state.server_key_hash, 0);
    }

    // Send ClientHello
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], peer_state.server_key_hash, 32);
    pos += 32;

    int ret2 = smp_write_handshake_block(&peer_state.ssl, block, client_hello, pos);
    free(block);

    if (ret2 != 0) {
        ESP_LOGE(TAG, "   ❌ ClientHello failed");
        return false;
    }

    ESP_LOGI(TAG, "   ✅ SMP Handshake complete!");
    peer_state.connected = true;

    // Update global peer_conn for compatibility
    peer_conn.connected = true;
    memcpy(peer_conn.session_id, peer_state.session_id, 32);

    return true;
}

void peer_disconnect(void) {
    if (peer_state.connected || peer_state.initialized) {
        mbedtls_ssl_close_notify(&peer_state.ssl);
        mbedtls_ssl_free(&peer_state.ssl);
        mbedtls_ssl_config_free(&peer_state.conf);
        mbedtls_ctr_drbg_free(&peer_state.ctr_drbg);
        mbedtls_entropy_free(&peer_state.entropy);
        if (peer_state.sock >= 0) close(peer_state.sock);
        peer_state.connected = false;
        peer_state.initialized = false;
        peer_conn.connected = false;
        ESP_LOGI(TAG, "   🔌 Peer connection closed");
    }
}

// ============== Per-Contact Peer State ==============

/**
 * Prepare peer_state and pending_peer for a specific contact.
 * Loads the correct peer_XX from NVS, sets ratchet/handshake active index,
 * and disconnects if currently connected to a different server.
 * 
 * This is the KEY FIX for multi-contact: without this, all send functions
 * use whatever pending_peer was last set (always the last contact's data).
 */
static bool peer_prepare_for_contact(contact_t *contact) {
    // Resolve contact index from pointer
    int contact_idx = -1;
    if (contact >= &contacts_db.contacts[0] &&
        contact < &contacts_db.contacts[MAX_CONTACTS]) {
        contact_idx = (int)(contact - &contacts_db.contacts[0]);
    }
    if (contact_idx < 0 || !contacts_db.contacts[contact_idx].active) {
        ESP_LOGE(TAG, "peer_prepare_for_contact: invalid contact!");
        return false;
    }

    // Save old host for comparison
    char old_host[64];
    int old_port = pending_peer.port;
    strncpy(old_host, pending_peer.host, sizeof(old_host) - 1);
    old_host[63] = '\0';

    // Load this contact's peer state from NVS
    if (!peer_load_state(contact_idx)) {
        ESP_LOGE(TAG, "peer_prepare_for_contact: peer_%02x not found!", contact_idx);
        return false;
    }

    // Set active ratchet + handshake for this contact
    ratchet_set_active(contact_idx);
    handshake_set_active(contact_idx);

    // If connected to a DIFFERENT server, disconnect first
    if (peer_state.connected &&
        (strcmp(old_host, pending_peer.host) != 0 || old_port != pending_peer.port)) {
        ESP_LOGW(TAG, "peer_prepare: server changed %s:%d -> %s:%d, disconnecting",
                 old_host, old_port, pending_peer.host, pending_peer.port);
        peer_disconnect();
    }

    ESP_LOGI(TAG, "peer_prepare: contact [%d] -> %s:%d, queue %02x%02x%02x%02x...",
             contact_idx, pending_peer.host, pending_peer.port,
             pending_peer.queue_id[0], pending_peer.queue_id[1],
             pending_peer.queue_id[2], pending_peer.queue_id[3]);
    return true;
}

// ============== AgentConfirmation ==============

bool send_agent_confirmation(contact_t *contact, int contact_idx) {
    if (!peer_state.connected || !pending_peer.valid || !pending_peer.has_dh) {
        ESP_LOGE(TAG, "❌ Cannot send CONF: not ready");
        return false;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  📤 SENDING AGENT CONFIRMATION (v0.1.14 Format)              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "   To queue: %02x%02x%02x%02x... (%d bytes)",
             pending_peer.queue_id[0], pending_peer.queue_id[1],
             pending_peer.queue_id[2], pending_peer.queue_id[3],
             pending_peer.queue_id_len);

    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;

    // NOTE: SKEY removed - it's for Fast Duplex (v9+ SMP protocol).
    // Standard flow: SEND without auth works on unsecured queues.

    // ========== Build connInfo (ChatMessage JSON) ==========
    const char *conn_info_json =
        "{\"v\":\"1-16\",\"event\":\"x.info\",\"params\":{\"profile\":{\"displayName\":\"ESP32\",\"fullName\":\"\"}}}";
    int json_len = (int)strlen(conn_info_json);

    ESP_LOGI(TAG, "   📋 ConnInfo JSON (%d bytes):", json_len);
    ESP_LOGI(TAG, "      %s", conn_info_json);

    // ========== Build AgentConnInfoReply ==========
    // Format: 'D' + smpQueues (NonEmpty SMPQueueInfo) + connInfo (Tail)
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "❌ Our queue not created! Call queue_create() first!");
        free(block);
        return false;
    }

    uint8_t agent_conn_info[512];
    int aci_len = 0;

    // Tag = 'D' for AgentConnInfoReply (joining party sends this)
    agent_conn_info[aci_len++] = 'D';

    // smpQueues = NonEmpty list with 1 element
    agent_conn_info[aci_len++] = 0x01;  // 1 queue in list

    int queue_info_len = -1;

    // Use explicit contact_idx from caller, NOT pointer arithmetic!
    int peer_contact_idx = contact_idx;
    if (peer_contact_idx < 0 || peer_contact_idx >= MAX_CONTACTS ||
        !contacts_db.contacts[peer_contact_idx].active) {
        ESP_LOGE(TAG, "❌ Invalid contact_idx %d, falling back to 0", contact_idx);
        peer_contact_idx = 0;
    }
    ESP_LOGI(TAG, "peer_contact_idx = %d (EXPLICIT from caller)", peer_contact_idx);

    if (peer_contact_idx >= 0) {
        reply_queue_t *rq = reply_queue_get(peer_contact_idx);
        if (rq && rq->valid) {
            queue_info_len = reply_queue_encode_info(peer_contact_idx,
                &agent_conn_info[aci_len], sizeof(agent_conn_info) - (size_t)aci_len);
            ESP_LOGI(TAG, "Using per-contact reply queue RQ[%d]", peer_contact_idx);
        } else if (peer_contact_idx > 0) {
            // NEVER use our_queue for non-zero contacts - it sends WRONG queue info!
            ESP_LOGE(TAG, "RQ[%d] NOT VALID and slot > 0 - cannot send confirmation!", peer_contact_idx);
            ESP_LOGE(TAG, "   This means reply_queue_create() data was lost.");
            ESP_LOGE(TAG, "   Check if subscribe_all_contacts() overwrites PSRAM via reply_queue_load()");
            free(block);
            return false;
        }
    }

    // Fallback to legacy global our_queue (ONLY for Contact 0 / legacy mode)
    if (queue_info_len < 0) {
        queue_info_len = queue_encode_info(&agent_conn_info[aci_len],
            sizeof(agent_conn_info) - (size_t)aci_len);
        ESP_LOGW(TAG, "Fallback to legacy our_queue for CONFIRMATION (contact 0)");
    }
    if (queue_info_len < 0) {
        ESP_LOGE(TAG, "❌ Failed to encode queue info!");
        free(block);
        return false;
    }
    aci_len += queue_info_len;
    // connInfo = JSON tail (no extra length prefix here)
        memcpy(&agent_conn_info[aci_len], conn_info_json, (size_t)json_len);
        aci_len += json_len;

        ESP_LOGI(TAG, "   📦 AgentConnInfoReply: %d bytes (tag='D' + queue + JSON)", aci_len);

    // ========== Generate E2E Ratchet Parameters ==========
    e2e_params_t *e2e_params = (e2e_params_t *)heap_caps_malloc(sizeof(e2e_params_t), MALLOC_CAP_8BIT);
    if (!e2e_params) {
        ESP_LOGE(TAG, "❌ Failed to allocate e2e_params!");
        free(block);
        return false;
    }

    if (!e2e_generate_params(e2e_params)) {
        ESP_LOGE(TAG, "❌ Failed to generate E2E params!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Encode E2E params (SndE2ERatchetParams format)
    uint8_t e2e_encoded[1800];
    int e2e_len = e2e_encode_params(e2e_params, e2e_encoded);
    ESP_LOGI(TAG, "   🔐 E2E params: %d bytes", e2e_len);

    // ========== X3DH + Ratchet Encryption ==========
    // Set active ratchet/handshake slot BEFORE X3DH
    // Without this, Contact 2's ratchet overwrites Contact 1's rat_00!
    if (peer_contact_idx >= 0) {
        ratchet_set_active((uint8_t)peer_contact_idx);
        handshake_set_active((uint8_t)peer_contact_idx);
        ESP_LOGI(TAG, "🔄 Set active ratchet+handshake to slot [%d]", peer_contact_idx);
    }

    if (!pending_peer.has_e2e) {
        ESP_LOGE(TAG, "❌ No E2E keys from peer!");
        free(e2e_params);
        free(block);
        return false;
    }

    // X3DH sender
    if (!ratchet_x3dh_sender(pending_peer.e2e_key1, pending_peer.e2e_key2, &e2e_params->key1, &e2e_params->key2)) {
        ESP_LOGE(TAG, "❌ X3DH failed!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Ratchet init (global state)
    if (!ratchet_init_sender(pending_peer.e2e_key2, &e2e_params->key2)) {
        ESP_LOGE(TAG, "❌ Ratchet init failed!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Encrypt AgentConnInfo with ratchet
    #define ENC_CONN_INFO_SIZE (1 + 123 + 16 + 14832 + 100)  // ~15072 bytes
    uint8_t *enc_conn_info = malloc(ENC_CONN_INFO_SIZE);
    if (!enc_conn_info) {
        ESP_LOGE(TAG, "❌ Failed to allocate enc_conn_info!");
        free(e2e_params);
        free(block);
        return false;
    }
    size_t enc_conn_info_len = 0;

    if (ratchet_encrypt(agent_conn_info, (size_t)aci_len, enc_conn_info, &enc_conn_info_len, 14832) != 0) {
        ESP_LOGE(TAG, "❌ Ratchet encrypt failed!");
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
    }

    // BUG #19 FIX: Removed debug self-decrypt test that was here.
    // ratchet_decrypt() has side effects (DH ratchet step) that corrupted
    // header_key_recv, root_key, chain_key_recv, and dh_peer when called
    // on our own message. The DH key in our header (dh_self) differs from
    // dh_peer, triggering a spurious ratchet step that overwrote the keys
    // needed to decrypt incoming messages from the peer.

    ESP_LOGI(TAG, "   🔒 encConnInfo encrypted: %d bytes", (int)enc_conn_info_len);

    // ========== Build AgentConfirmation ==========
    uint8_t *agent_msg = malloc(20000);
    if (!agent_msg) {
        ESP_LOGE(TAG, "❌ Failed to allocate agent_msg!");
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
}
    int amp = 0;

    // agentVersion = 7 (2 bytes Big Endian)
    agent_msg[amp++] = 0x00;
    agent_msg[amp++] = 0x07;

    // Type = 'C' (Confirmation)
    agent_msg[amp++] = 'C';

    // Maybe tag for E2E params: ASCII '1' or '0'
    agent_msg[amp++] = '1';

    // E2E params first
    memcpy(&agent_msg[amp], e2e_encoded, (size_t)e2e_len);
    amp += e2e_len;

    // encConnInfo after params (Tail = no length prefix!)
    memcpy(&agent_msg[amp], enc_conn_info, enc_conn_info_len);
    amp += (int)enc_conn_info_len;

    ESP_LOGI(TAG, "    📨 AgentConfirmation: %d bytes", amp);

    // ========== Build ClientMessage Plaintext ==========
    // PrivHeader for Confirmation = 'K' + Length + Ed25519 SPKI
    uint8_t *plaintext = malloc(20000);
    if (!plaintext) {
        ESP_LOGE(TAG, "❌ Failed to allocate plaintext!");
        free(agent_msg);
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
    }
    int pp = 0;

    // Resolve per-contact auth key for PrivHeader + SEND signing
    uint8_t *sender_auth_public = our_queue.rcv_auth_public;
    uint8_t *sender_auth_private = our_queue.rcv_auth_private;
    if (peer_contact_idx >= 0) {
        reply_queue_t *rq_auth = reply_queue_get(peer_contact_idx);
        if (rq_auth && rq_auth->valid) {
            sender_auth_public = rq_auth->rcv_auth_public;
            sender_auth_private = rq_auth->rcv_auth_private;
            ESP_LOGI(TAG, "   Using RQ[%d] auth key for PrivHeader", peer_contact_idx);
        }
    }

    // 'K' = PHConfirmation tag
    plaintext[pp++] = 'K';

    // Ed25519 SPKI with length prefix (44 bytes)
    plaintext[pp++] = 44;

    // Ed25519 SPKI (12 header + 32 key = 44 bytes)
    memcpy(&plaintext[pp], ED25519_SPKI_HEADER, 12);
    pp += 12;
    memcpy(&plaintext[pp], sender_auth_public, 32);  // per-contact key
    pp += 32;

    // AgentMsgEnvelope (AgentConfirmation)
    memcpy(&plaintext[pp], agent_msg, (size_t)amp);
    pp += amp;

    ESP_LOGI(TAG, "   📝 ClientMessage plaintext: %d bytes (PrivHeader + AgentMsg)", pp);

    // ========== Encrypt with Peer's DH Key (crypto_box) ==========
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);

    // Build PADDED plaintext with SimpleX padding scheme
    // e2eEncConfirmationLength = 15904 bytes
    #define E2E_ENC_CONFIRMATION_LENGTH 15904
    
    uint8_t *padded = malloc(E2E_ENC_CONFIRMATION_LENGTH);
    if (!padded) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate padding buffer!");
        free(e2e_params);
        free(block);
        return false;
    }
    
    // 2-Byte Length Prefix (Big Endian) - message length
    uint16_t msg_len = pp;  // pp = plaintext length
    padded[0] = (msg_len >> 8) & 0xFF;  // High byte
    padded[1] = msg_len & 0xFF;         // Low byte
    
    // Copy actual plaintext
    memcpy(&padded[2], plaintext, pp);
    
    // Fill rest with '#' (0x23)
    int pad_start = 2 + pp;
    memset(&padded[pad_start], '#', E2E_ENC_CONFIRMATION_LENGTH - pad_start);
    
    int padded_len = E2E_ENC_CONFIRMATION_LENGTH;

// crypto_box with PADDED plaintext
    uint8_t *encrypted = malloc(24 + E2E_ENC_CONFIRMATION_LENGTH + crypto_box_MACBYTES);
    if (!encrypted) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate encrypted buffer!");
        free(padded);
        free(e2e_params);
        free(block);
        return false;
    }
    memcpy(encrypted, nonce, 24);
    if (crypto_box_easy(&encrypted[24], padded, (unsigned long long)padded_len, nonce,
                        pending_peer.dh_public, contact->rcv_dh_secret) != 0) {
        ESP_LOGE(TAG, "   ❌ Encryption failed!");
        free(padded);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    
    free(padded);  // free after successful encryption

    // IMPORTANT: encrypted_len must use padded_len!
    int encrypted_len = 24 + padded_len + crypto_box_MACBYTES;
    ESP_LOGI(TAG, "   🔐 Encrypted: %d bytes (nonce + ciphertext + MAC)", encrypted_len);

    // ========== Wrap in SMP ClientMsgEnvelope ==========
    // PubHeader = [Version (2B BE)][Maybe '1'][len=44][X25519 SPKI]
    // Body = [nonce+ciphertext+mac]
    uint8_t *client_msg = malloc(24 + E2E_ENC_CONFIRMATION_LENGTH + crypto_box_MACBYTES + 100);
    if (!client_msg) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate client_msg buffer!");
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int cmp = 0;

    // SMP client version 4
    client_msg[cmp++] = 0x00;
    client_msg[cmp++] = 0x04;

    // Maybe tag: ASCII '1'
    client_msg[cmp++] = '1';

    // LENGTH PREFIX for 44-byte X25519 SPKI - FIX
    client_msg[cmp++] = 44;  // 0x2C

    // X25519 DH public key SPKI
    memcpy(&client_msg[cmp], X25519_SPKI_HEADER, 12);
    cmp += 12;
    memcpy(&client_msg[cmp], contact->rcv_dh_public, 32);
    cmp += 32;

    // Encrypted body
    // Nonce (24 bytes) sent separately, not part of encrypted body
    memcpy(&client_msg[cmp], nonce, 24);
    cmp += 24;

    // Ciphertext + MAC (without the first 24 nonce bytes)
    memcpy(&client_msg[cmp], &encrypted[24], (size_t)(encrypted_len - 24));
    cmp += (encrypted_len - 24);

    ESP_LOGI(TAG, "   📦 Client message: %d bytes (PubHeader + encrypted)", cmp);

    #define SEND_BUFFER_SIZE (E2E_ENC_CONFIRMATION_LENGTH + 200)
    uint8_t *send_body = malloc(SEND_BUFFER_SIZE);
    if (!send_body) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate send_body!");
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int sbp = 0;

    // CorrId (length-prefixed)
    send_body[sbp++] = 1;
    send_body[sbp++] = 'C';

    // EntityId = peer's queue ID (length-prefixed)
    send_body[sbp++] = pending_peer.queue_id_len;
    memcpy(&send_body[sbp], pending_peer.queue_id, (size_t)pending_peer.queue_id_len);
    sbp += pending_peer.queue_id_len;

    // Command: "SEND "
    send_body[sbp++] = 'S';
    send_body[sbp++] = 'E';
    send_body[sbp++] = 'N';
    send_body[sbp++] = 'D';
    send_body[sbp++] = ' ';

    // Flags: "T "
    send_body[sbp++] = 'T';
    send_body[sbp++] = ' ';

    // MsgBody (no length prefix for SEND)
    memcpy(&send_body[sbp], client_msg, (size_t)cmp);
    sbp += cmp;

    ESP_LOGI(TAG, "   📮 SEND body: %d bytes", sbp);

    // ========== Build Transmission ==========
    uint8_t *transmission = malloc(SEND_BUFFER_SIZE + 100);
    if (!transmission) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate transmission!");
        free(send_body);
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int tp = 0;

    // No auth signature for SEND
    transmission[tp++] = 0;

    // SessionId (length-prefixed)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], peer_state.session_id, 32);
    tp += 32;

    // Body
    memcpy(&transmission[tp], send_body, (size_t)sbp);
    tp += sbp;

    ESP_LOGI(TAG, "   📡 Transmission: %d bytes", tp);

    // ========== Send ==========
    int ret = smp_write_command_block(&peer_state.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Send failed!");
        free(transmission);
        free(send_body);
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }

    ESP_LOGI(TAG, "   📤 SEND command sent! Waiting for response...");

    // Wait for response
    int content_len = smp_read_block(&peer_state.ssl, block, 10000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;

        ESP_LOGI(TAG, "   📥 Response (%d bytes)", content_len);

        // Parse response (lightweight, best-effort)
        int p = 0;
        if (resp[p] == 1) {
            p++;
            p += 2;  // skip next byte + something (your existing framing)

            int authLen = resp[p++]; p += authLen;
            int sessLen = resp[p++]; p += sessLen;
            int corrLen = resp[p++]; p += corrLen;
            int entLen  = resp[p++]; p += entLen;

            ESP_LOGI(TAG, "   Response command at offset %d: %c%c%c",
                     p, resp[p], resp[p + 1], resp[p + 2]);

            if (p + 1 < content_len && resp[p] == 'O' && resp[p + 1] == 'K') {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
                ESP_LOGI(TAG, "║   🎉🎉🎉 CONFIRMATION ACCEPTED BY SERVER! 🎉🎉🎉            ║");
                ESP_LOGI(TAG, "║                                                              ║");
                ESP_LOGI(TAG, "║   Server accepted our message.                               ║");
                ESP_LOGI(TAG, "║   Now waiting for SimpleX App to process it...               ║");
                ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");

                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   📤 Starting HELLO handshake...");

                if (pending_peer.has_dh && pending_peer.valid) {
                    uint8_t our_dh_private[32];
                    uint8_t our_dh_public[32];
                    memcpy(our_dh_private, contact->rcv_dh_secret, 32);   // Invitation DH (same as Confirmation!)
                    memcpy(our_dh_public,  contact->rcv_dh_public,  32);  // Matching public key

                    bool hello_ok = complete_handshake(
                        &peer_state.ssl,
                        block,
                        peer_state.session_id,
                        pending_peer.queue_id,
                        pending_peer.queue_id_len,
                        pending_peer.dh_public,
                        our_dh_private,
                        our_dh_public,
                        ratchet_get_state(),
                        sender_auth_private  // per-contact key
                    );

                    if (hello_ok) {
                        ESP_LOGI(TAG, "   ✅ HELLO handshake complete!");
                    } else {
                        ESP_LOGW(TAG, "   ⚠️  HELLO handshake failed (connection may still work)");
                    }
                } else {
                    ESP_LOGW(TAG, "   ⚠️  No peer DH key available, skipping HELLO");
                }

                // Persist peer state after successful handshake
                peer_save_state(peer_contact_idx >= 0 ? (uint8_t)peer_contact_idx : 0);

                free(transmission);
                free(send_body);
                free(client_msg);
                free(encrypted);
                free(e2e_params);
                free(block);
                return true;

            } else if (p + 2 < content_len && resp[p] == 'E' && resp[p + 1] == 'R' && resp[p + 2] == 'R') {
                ESP_LOGE(TAG, "   ❌ Server error: %.*s", 30, &resp[p]);
            }
        }
    } else {
        ESP_LOGW(TAG, "   ⚠️ No response received (timeout or error)");
    }

    free(transmission);
    free(send_body);
    free(client_msg);
    free(encrypted);
    free(e2e_params);
    free(block);
    return false;
}

// ============== Send HELLO ==============

bool peer_send_hello(contact_t *contact) {
    // Load correct peer state for THIS contact
    if (!peer_prepare_for_contact(contact)) {
        ESP_LOGE(TAG, "❌ peer_send_hello: peer_prepare failed!");
        return false;
    }

    // Reconnect to peer if connection was closed
    if (!peer_state.connected) {
        if (peer_state.last_host[0] == '\0' || peer_state.last_port == 0) {
            ESP_LOGE(TAG, "❌ peer_send_hello: no saved peer host/port!");
            return false;
        }
        ESP_LOGI(TAG, "   🔄 Reconnecting to peer server %s:%d...", 
                 peer_state.last_host, peer_state.last_port);
        if (!peer_connect(peer_state.last_host, peer_state.last_port)) {
            ESP_LOGE(TAG, "❌ peer_send_hello: reconnect failed!");
            return false;
        }
        ESP_LOGI(TAG, "   ✅ Peer reconnected!");
    }

    if (!pending_peer.valid || !pending_peer.has_dh) {
        ESP_LOGE(TAG, "❌ peer_send_hello: no pending peer data!");
        return false;
    }

    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "❌ peer_send_hello: malloc failed!");
        return false;
    }

    uint8_t our_dh_private[32];
    uint8_t our_dh_public[32];
    memcpy(our_dh_private, contact->rcv_dh_secret, 32);
    memcpy(our_dh_public,  contact->rcv_dh_public,  32);

    // Resolve per-contact auth key
    int hello_cidx = (contact >= &contacts_db.contacts[0] &&
                      contact < &contacts_db.contacts[MAX_CONTACTS])
                     ? (int)(contact - &contacts_db.contacts[0]) : -1;
    uint8_t *hello_auth_private = our_queue.rcv_auth_private;
    if (hello_cidx >= 0) {
        reply_queue_t *rq = reply_queue_get(hello_cidx);
        if (rq && rq->valid) {
            hello_auth_private = rq->rcv_auth_private;
        }
    }

    bool ok = send_hello_message(
        &peer_state.ssl,
        block,
        peer_state.session_id,
        pending_peer.queue_id,
        pending_peer.queue_id_len,
        pending_peer.dh_public,
        our_dh_private,
        our_dh_public,
        ratchet_get_state(),
        hello_auth_private  // per-contact key
    );

    free(block);
    return ok;
}

// ============== Send Chat Message ==============

bool peer_send_chat_message(contact_t *contact, const char *message) {
    // Load correct peer state for THIS contact
    if (!peer_prepare_for_contact(contact)) {
        ESP_LOGE(TAG, "❌ peer_send_chat_message: peer_prepare failed!");
        return false;
    }

    // Reconnect to peer if needed
    if (!peer_state.connected) {
        if (peer_state.last_host[0] == '\0' || peer_state.last_port == 0) {
            ESP_LOGE(TAG, "❌ peer_send_chat_message: no saved peer host/port!");
            return false;
        }
        ESP_LOGI(TAG, "   🔄 Reconnecting to peer server %s:%d...",
                 peer_state.last_host, peer_state.last_port);
        if (!peer_connect(peer_state.last_host, peer_state.last_port)) {
            ESP_LOGE(TAG, "❌ peer_send_chat_message: reconnect failed!");
            return false;
        }
        ESP_LOGI(TAG, "   ✅ Peer reconnected!");
    }

    if (!pending_peer.valid || !pending_peer.has_dh) {
        ESP_LOGE(TAG, "❌ peer_send_chat_message: no pending peer data!");
        return false;
    }

    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "❌ peer_send_chat_message: malloc failed!");
        return false;
    }

    uint8_t our_dh_private[32];
    uint8_t our_dh_public[32];
    memcpy(our_dh_private, contact->rcv_dh_secret, 32);
    memcpy(our_dh_public,  contact->rcv_dh_public,  32);

    // Resolve per-contact auth key
    int msg_cidx = (contact >= &contacts_db.contacts[0] &&
                    contact < &contacts_db.contacts[MAX_CONTACTS])
                   ? (int)(contact - &contacts_db.contacts[0]) : -1;
    uint8_t *msg_auth_private = our_queue.rcv_auth_private;
    if (msg_cidx >= 0) {
        reply_queue_t *rq = reply_queue_get(msg_cidx);
        if (rq && rq->valid) {
            msg_auth_private = rq->rcv_auth_private;
        }
    }

    bool ok = send_chat_message(
        &peer_state.ssl,
        block,
        peer_state.session_id,
        pending_peer.queue_id,
        pending_peer.queue_id_len,
        pending_peer.dh_public,
        our_dh_private,
        our_dh_public,
        ratchet_get_state(),
        msg_auth_private,  // per-contact key
        message
    );

    free(block);
    return ok;
}

// ============== Send Delivery Receipt ==============

bool peer_send_receipt(contact_t *contact, uint64_t peer_snd_msg_id, const uint8_t *msg_hash) {
    ESP_LOGI(TAG, "📬 SENDING DELIVERY RECEIPT (A_RCVD)");
    
    // Load correct peer state for THIS contact
    if (!peer_prepare_for_contact(contact)) {
        ESP_LOGE(TAG, "❌ peer_send_receipt: peer_prepare failed!");
        return false;
    }

    // Reconnect to peer if needed
    if (!peer_state.connected) {
        if (peer_state.last_host[0] == '\0' || peer_state.last_port == 0) {
            ESP_LOGE(TAG, "❌ peer_send_receipt: no saved peer host/port!");
            return false;
        }
        ESP_LOGI(TAG, "   🔄 Reconnecting to peer server %s:%d...",
                 peer_state.last_host, peer_state.last_port);
        if (!peer_connect(peer_state.last_host, peer_state.last_port)) {
            ESP_LOGE(TAG, "❌ peer_send_receipt: reconnect failed!");
            return false;
        }
        ESP_LOGI(TAG, "   ✅ Peer reconnected!");
    }

    if (!pending_peer.valid || !pending_peer.has_dh) {
        ESP_LOGE(TAG, "❌ peer_send_receipt: no pending peer data!");
        return false;
    }

    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "❌ peer_send_receipt: malloc failed!");
        return false;
    }

    uint8_t our_dh_private[32];
    uint8_t our_dh_public[32];
    memcpy(our_dh_private, contact->rcv_dh_secret, 32);
    memcpy(our_dh_public,  contact->rcv_dh_public,  32);

    // Use per-contact reply queue auth key, not global our_queue!
    int rcpt_cidx = (contact >= &contacts_db.contacts[0] &&
                     contact < &contacts_db.contacts[MAX_CONTACTS])
                    ? (int)(contact - &contacts_db.contacts[0]) : -1;
    const uint8_t *rcpt_auth_private = our_queue.rcv_auth_private;  // fallback
    if (rcpt_cidx >= 0) {
        reply_queue_t *rq = reply_queue_get(rcpt_cidx);
        if (rq && rq->valid) {
            rcpt_auth_private = rq->rcv_auth_private;
        }
    }

    bool ok = send_receipt_message(
        &peer_state.ssl,
        block,
        peer_state.session_id,
        pending_peer.queue_id,
        pending_peer.queue_id_len,
        pending_peer.dh_public,
        our_dh_private,
        our_dh_public,
        ratchet_get_state(),
        rcpt_auth_private,
        peer_snd_msg_id,
        msg_hash
    );

    free(block);
    return ok;
}

// ============== Persistence ==============

bool peer_save_state(uint8_t contact_idx) {
    if (!pending_peer.valid) {
        ESP_LOGW(TAG, "peer_save_state: pending_peer not valid, skipping");
        return false;
    }

    // Dynamic NVS key per contact (was hardcoded "peer_00")
    char key[12];
    snprintf(key, sizeof(key), "peer_%02x", contact_idx);

    esp_err_t ret = smp_storage_save_blob_sync(key, &pending_peer, sizeof(peer_queue_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ peer_save_state('%s') FAILED: %s", key, esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "💾 Peer state saved: '%s' (%zu bytes)", key, sizeof(peer_queue_t));
    ESP_LOGI(TAG, "   host: %s:%d, queueId: %02x%02x%02x%02x... (%d)",
             pending_peer.host, pending_peer.port,
             pending_peer.queue_id[0], pending_peer.queue_id[1],
             pending_peer.queue_id[2], pending_peer.queue_id[3],
             pending_peer.queue_id_len);
    return true;
}

bool peer_load_state(uint8_t contact_idx) {
    // Dynamic NVS key per contact (was hardcoded "peer_00")
    char key[12];
    snprintf(key, sizeof(key), "peer_%02x", contact_idx);

    if (!smp_storage_exists(key)) {
        // Backward compat: try legacy "peer_00" for slot 0
        if (contact_idx == 0 && smp_storage_exists("peer_00")) {
            snprintf(key, sizeof(key), "peer_00");
            ESP_LOGI(TAG, "peer_load_state: using legacy key 'peer_00'");
        } else {
            ESP_LOGI(TAG, "peer_load_state: '%s' not found", key);
            return false;
        }
    }

    size_t loaded_len = 0;
    peer_queue_t loaded;
    esp_err_t ret = smp_storage_load_blob(key, &loaded, sizeof(peer_queue_t), &loaded_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ peer_load_state('%s') FAILED: %s", key, esp_err_to_name(ret));
        return false;
    }

    if (loaded_len != sizeof(peer_queue_t)) {
        ESP_LOGE(TAG, "❌ peer_load_state: size mismatch! got %zu, expected %zu",
                 loaded_len, sizeof(peer_queue_t));
        return false;
    }
    if (!loaded.valid) {
        ESP_LOGW(TAG, "❌ peer_load_state: loaded state has valid=false!");
        return false;
    }
    if (loaded.host[0] == '\0' || loaded.port == 0) {
        ESP_LOGW(TAG, "❌ peer_load_state: host or port empty!");
        return false;
    }
    if (loaded.queue_id_len <= 0 || loaded.queue_id_len > 32) {
        ESP_LOGW(TAG, "❌ peer_load_state: invalid queue_id_len=%d", loaded.queue_id_len);
        return false;
    }

    // Accept loaded state
    memcpy(&pending_peer, &loaded, sizeof(peer_queue_t));

    // Set peer_state reconnect info so send functions can reconnect
    strncpy(peer_state.last_host, pending_peer.host, sizeof(peer_state.last_host) - 1);
    peer_state.last_host[sizeof(peer_state.last_host) - 1] = '\0';
    peer_state.last_port = pending_peer.port;

    ESP_LOGI(TAG, "📂 Peer state restored: '%s' (%zu bytes)", key, loaded_len);
    ESP_LOGI(TAG, "   host: %s:%d, queueId: %02x%02x%02x%02x... (%d)",
             pending_peer.host, pending_peer.port,
             pending_peer.queue_id[0], pending_peer.queue_id[1],
             pending_peer.queue_id[2], pending_peer.queue_id[3],
             pending_peer.queue_id_len);
    ESP_LOGI(TAG, "   dh: %s, e2e: %s",
             pending_peer.has_dh ? "✅" : "❌",
             pending_peer.has_e2e ? "✅" : "❌");
    return true;
}