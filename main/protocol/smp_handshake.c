/**
 * SimpleGo - smp_handshake.c
 * SMP connection handshake protocol implementation
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_handshake.h"
#include "smp_types.h"
#include "smp_contacts.h"
#include "smp_network.h"
#include "smp_queue.h"
#include "smp_ratchet.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "mbedtls/sha256.h"
#include "sodium.h"
#include "smp_storage.h"

static const char *TAG = "SMP_HAND";

// Per-contact handshake state
#define MAX_HANDSHAKE_STATES 128

typedef struct {
    bool confirmation_received;
    bool hello_sent;
    bool hello_received;
    bool connected;
    uint64_t msg_id;           // Sequential message ID
    uint8_t prev_msg_hash[32]; // Hash of previous message
    ratchet_state_t *ratchet;  // Pointer to ratchet state
} handshake_entry_t;

static handshake_entry_t *handshake_array = NULL;
static uint8_t active_handshake_idx = 0;

// Convenience macro -- existing code works unchanged
#define handshake_state (handshake_array[active_handshake_idx])

// Retry buffer: holds encrypted transmission after write failure.
// Needed because ratchet has already advanced - re-encryption would desync.
// After reconnect, session_id changes - signature must be re-computed.
typedef struct {
    uint8_t *transmission;      // encrypted payload (PSRAM)
    size_t   transmission_len;  // payload length
    uint8_t  prev_msg_hash[32]; // SHA-256 of plaintext, for handshake_save_state after retry
    uint8_t  snd_auth_private[64]; // Ed25519 signing key for re-signing after reconnect
    uint8_t  contact_slot;      // which contact this belongs to
    bool     pending;           // retry waiting?
} retry_buffer_t;

static retry_buffer_t s_retry = {0};

bool handshake_multi_init(void) {
    if (handshake_array) return true;

    handshake_array = (handshake_entry_t *)heap_caps_calloc(
        MAX_HANDSHAKE_STATES, sizeof(handshake_entry_t), MALLOC_CAP_SPIRAM);

    if (!handshake_array) {
        ESP_LOGE(TAG, "Failed to allocate handshake array in PSRAM!");
        return false;
    }

    active_handshake_idx = 0;
    ESP_LOGI(TAG, "Handshake array allocated: %d slots, %zu bytes in PSRAM",
             MAX_HANDSHAKE_STATES,
             MAX_HANDSHAKE_STATES * sizeof(handshake_entry_t));
    return true;
}

void handshake_set_active(uint8_t idx) {
    if (idx < MAX_HANDSHAKE_STATES && handshake_array) {
        // Save current state before switching
        if (active_handshake_idx != idx) {
            handshake_save_state();
        }
        active_handshake_idx = idx;
        handshake_load_state();
    }
}

// ============== HELLO Message Building ==============

/**
 * Build HELLO message according to agent-protocol.md:
 * 
 * agentMessage = %s"M" agentMsgHeader aMessage msgPadding
 * agentMsgHeader = agentMsgId prevMsgHash
 * agentMsgId = 8*8 OCTET      ; Int64 Big-Endian
 * prevMsgHash = shortString   ; [len][hash...]
 * HELLO = %s"H"
 */
static int build_hello_message(uint8_t *output, int max_len) {
    int p = 0;
    
    // agentMessage tag = 'M'
    output[p++] = 'M';
    
    // agentMsgHeader:
    // agentMsgId = 8 bytes Big-Endian (sequential message ID)
    handshake_state.msg_id++;
    uint64_t msg_id = handshake_state.msg_id;
    output[p++] = (msg_id >> 56) & 0xFF;
    output[p++] = (msg_id >> 48) & 0xFF;
    output[p++] = (msg_id >> 40) & 0xFF;
    output[p++] = (msg_id >> 32) & 0xFF;
    output[p++] = (msg_id >> 24) & 0xFF;
    output[p++] = (msg_id >> 16) & 0xFF;
    output[p++] = (msg_id >> 8) & 0xFF;
    output[p++] = msg_id & 0xFF;
    
    // prevMsgHash = shortString [Word8 len][hash...]
    // For first message, use empty (len=0)
    if (handshake_state.msg_id == 1) {
        output[p++] = 0x00;  // Word8 len = 0 (empty hash)
    } else {
        output[p++] = 32;    // Word8 len = 32
        memcpy(&output[p], handshake_state.prev_msg_hash, 32);
        p += 32;
    }
    // aMessage = HELLO = %s"H"
    output[p++] = 'H';

    return p;
}

// ============== Chat Message Building (A_MSG) ==============

/**
 * Build A_MSG message:
 * 
 * agentMessage = %s"M" agentMsgHeader aMessage
 * agentMsgHeader = agentMsgId prevMsgHash
 * aMessage = %s"M" msgBody
 * msgBody = UTF-8 text
 * 
 * Byte layout:
 *   [0]      'M'     AgentMessage tag
 *   [1-8]    Int64   sndMsgId (Big-Endian)
 *   [9]      len     prevMsgHash length (0 or 32)
 *   [10-41]  hash    prevMsgHash (if len=32)
 *   [N]      'M'     A_MSG tag
 *   [N+1..]  text    msgBody (UTF-8)
 */
static int build_chat_message(const char *message, uint8_t *output, int max_len) {
    int p = 0;
    
    // agentMessage tag = 'M'
    output[p++] = 'M';
    
    // agentMsgId = 8 bytes Big-Endian
    handshake_state.msg_id++;
    uint64_t msg_id = handshake_state.msg_id;
    output[p++] = (msg_id >> 56) & 0xFF;
    output[p++] = (msg_id >> 48) & 0xFF;
    output[p++] = (msg_id >> 40) & 0xFF;
    output[p++] = (msg_id >> 32) & 0xFF;
    output[p++] = (msg_id >> 24) & 0xFF;
    output[p++] = (msg_id >> 16) & 0xFF;
    output[p++] = (msg_id >> 8) & 0xFF;
    output[p++] = msg_id & 0xFF;
    
    // prevMsgHash = shortString
    if (handshake_state.msg_id == 1) {
        output[p++] = 0x00;  // empty
    } else {
        output[p++] = 32;
        memcpy(&output[p], handshake_state.prev_msg_hash, 32);
        p += 32;
    }
    
    // aMessage = A_MSG = 'M' + msgBody (ChatMessage JSON)
    output[p++] = 'M';
    
    // msgBody = ChatMessage JSON format
    // Same format as x.info but with x.msg.new event
    char json_buf[400];
    int json_len = snprintf(json_buf, sizeof(json_buf),
        "{\"v\":\"1\",\"event\":\"x.msg.new\",\"params\":{\"content\":{\"type\":\"text\",\"text\":\"%s\"}}}",
        message);
    memcpy(&output[p], json_buf, json_len);
    p += json_len;
    
    return p;
}

/*
 * agentMsgEnvelope = agentVersion %s"M" encAgentMessage
 * encAgentMessage = doubleRatchetEncryptedMessage
 */
static int build_agent_msg_envelope(
    ratchet_state_t *ratchet,
    const uint8_t *plaintext,
    int plain_len,
    uint8_t *output,
    int max_len
) {
    int p = 0;
    
    // agentVersion = 2 bytes Big-Endian (version 7)
    output[p++] = 0x00;
    output[p++] = 0x07;
    
    // Message type = 'M' (AgentMsgEnvelope)
    output[p++] = 'M';
    
    // encAgentMessage (Tail = no length prefix!)
    size_t enc_len = 0;
    if (ratchet_encrypt(plaintext, plain_len, &output[p], &enc_len, 15840) != 0) {
        ESP_LOGE(TAG, "   [FAIL] Ratchet encryption failed!");
        return -1;
    }
    p += enc_len;
    
    ESP_LOGI(TAG, "   [PKG] AgentMsgEnvelope: %d bytes", p);
    return p;
}

// ============== Send SKEY Command ==============

/**
 * Send SKEY command to secure the peer's queue.
 * This must be sent BEFORE HELLO according to agent-protocol.md:
 * "Agent B secures Alice's queue with SMP command SKEY"
 * 
 * Format: SKEY <senderAuthPublicKey>
 * senderAuthPublicKey = length x509encoded (Ed25519 SPKI)
 */
bool send_skey_command(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *our_auth_public  // Ed25519 public key (32 bytes raw)
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [LOCK] SENDING SKEY COMMAND (Secure Queue)                      |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    
    // Build SKEY command body
    // Format: [corrId][entityId=peer_queue_id]SKEY [authKey]
    uint8_t skey_body[150];
    int sbp = 0;
    
    // CorrId (length-prefixed string)
    skey_body[sbp++] = 1;    // Length
    skey_body[sbp++] = 'K';  // 'K' for SKEY
    
    // EntityId = peer's queue ID
    skey_body[sbp++] = (uint8_t)peer_queue_id_len;
    memcpy(&skey_body[sbp], peer_queue_id, peer_queue_id_len);
    sbp += peer_queue_id_len;
    
    // SKEY command
    memcpy(&skey_body[sbp], "SKEY ", 5);
    sbp += 5;
    
    // senderAuthPublicKey = length + Ed25519 SPKI (44 bytes)
    skey_body[sbp++] = 44;  // Length of SPKI
    memcpy(&skey_body[sbp], ED25519_SPKI_HEADER, 12);
    sbp += 12;
    memcpy(&skey_body[sbp], our_auth_public, 32);
    sbp += 32;
    
    ESP_LOGD(TAG, "   SKEY body: %d bytes", sbp);
    
    // Build transmission (no signature for SKEY - it's sender securing)
    uint8_t transmission[200];
    int tp = 0;
    
    // Auth = 0 (SKEY doesn't need auth, we're the sender)
    transmission[tp++] = 0;
    
    // SessionId (length-prefixed)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], session_id, 32);
    tp += 32;
    
    // Body
    memcpy(&transmission[tp], skey_body, sbp);
    tp += sbp;
    
    ESP_LOGD(TAG, "   Transmission: %d bytes", tp);
    
    // Send
    int ret = smp_write_command_block(ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send SKEY failed: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "   [SEND] SKEY sent! Waiting for response...");
    
    // Wait for OK response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] No response!");
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Look for OK
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   [OK] SKEY accepted! Queue secured.");
            return true;
        }
    }
    
    // Debug: show response
    ESP_LOGW(TAG, "   SKEY response not OK (%d bytes)", content_len);
    
    // Check for specific errors
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R' && resp[i+3] == ' ') {
            ESP_LOGW(TAG, "   [WARN]  Server returned error (queue may already be secured)");
            return true;  // Continue anyway - might already be secured
        }
    }
    
    return false;
}

// ============== Send HELLO ==============

/**
 * Send HELLO message to complete the connection handshake.
 * 
 * This is sent to the PEER's queue (the one from the invitation)
 * after we've sent our AgentConfirmation.
 */
bool send_hello_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private  // NEW!
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [SEND] SENDING HELLO MESSAGE                                    |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    
    handshake_state.ratchet = ratchet;
    
    // 1. Build HELLO plaintext
    uint8_t hello_plain[300];
    int hello_plain_len = build_hello_message(hello_plain, sizeof(hello_plain));
    if (hello_plain_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] Failed to build HELLO message!");
        return false;
    }
    
    // 2. Build AgentMsgEnvelope (encrypt with ratchet)
    //    crypto_box plaintext = '_' || AgentMsgEnvelope
    //    '_' = PHEmpty PrivHeader (0x5F)
    #define HELLO_BUFFER_SIZE 17000
    
    uint8_t *agent_envelope = malloc(HELLO_BUFFER_SIZE);
    if (!agent_envelope) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate agent_envelope!");
        return false;
    }
    
    // Write AgentMsgEnvelope at offset 1 (leave room for PrivHeader)
    int envelope_len = build_agent_msg_envelope(ratchet, hello_plain, hello_plain_len,
                                                  agent_envelope + 1, HELLO_BUFFER_SIZE - 1);
    if (envelope_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] Failed to build AgentMsgEnvelope!");
        free(agent_envelope);
        return false;
    }
    
    // Prepend PrivHeader = '_' (PHEmpty)
    agent_envelope[0] = '_';
    int total_plain_len = 1 + envelope_len;

    // 3. cbEncrypt: Pad plaintext before crypto_box
    //    Format: [Word16 BE len of ClientMessage][ClientMessage][0x23 padding to 16000]
    #define E2E_ENC_HELLO_LENGTH 16000
    
    uint8_t *padded = malloc(E2E_ENC_HELLO_LENGTH);
    if (!padded) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate cbEncrypt padding buffer!");
        free(agent_envelope);
        return false;
    }
    
    // Word16 BE length prefix
    padded[0] = (total_plain_len >> 8) & 0xFF;
    padded[1] = total_plain_len & 0xFF;
    
    // Copy ClientMessage ('_' + AgentMsgEnvelope)
    memcpy(&padded[2], agent_envelope, total_plain_len);
    
    // Fill rest with '#' (0x23)
    int pad_start = 2 + total_plain_len;
    memset(&padded[pad_start], '#', E2E_ENC_HELLO_LENGTH - pad_start);
    
    free(agent_envelope);

    // 4. Encrypt with SMP-level crypto (NaCL crypto_box)
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);

    // Client message = PubHeader + nonce + encrypted envelope
    uint8_t *client_msg = malloc(HELLO_BUFFER_SIZE);
    if (!client_msg) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate client_msg!");
        free(padded);
        return false;
    }
    int cmp = 0;

    // PubHeader: SMP Client Version (Word16 BE) - version 4
    client_msg[cmp++] = 0x00;
    client_msg[cmp++] = 0x04;

    // PubHeader: Maybe tag '0' = Nothing (no DH key for HELLO!)
    // SMP uses '0'/'1' for Maybe encoding, not ','
    client_msg[cmp++] = '0';

    // Nonce (24 bytes) - directly after version + Nothing tag
    memcpy(&client_msg[cmp], nonce, 24);
    cmp += 24;

    // Encrypted padded envelope + MAC
    uint8_t *enc_envelope = malloc(E2E_ENC_HELLO_LENGTH + crypto_box_MACBYTES);
    if (!enc_envelope) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate enc_envelope!");
        free(client_msg);
        free(padded);
        return false;
    }
    
    if (crypto_box_easy(enc_envelope, padded, E2E_ENC_HELLO_LENGTH,
                        nonce, peer_dh_public, our_dh_private) != 0) {
        ESP_LOGE(TAG, "   [FAIL] crypto_box failed!");
        free(enc_envelope);
        free(client_msg);
        free(padded);
        return false;
    }
    int enc_envelope_len = E2E_ENC_HELLO_LENGTH + crypto_box_MACBYTES;
    
    free(padded);  // No longer needed after encryption

    memcpy(&client_msg[cmp], enc_envelope, enc_envelope_len);
    cmp += enc_envelope_len;

    free(enc_envelope);

    ESP_LOGD(TAG, "   Client message: %d bytes (3 PubHeader + 24 nonce + %d encrypted)",
             cmp, enc_envelope_len);

    // 4. Build SEND command body
    uint8_t *send_body = malloc(HELLO_BUFFER_SIZE);
    if (!send_body) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate send_body!");
        free(client_msg);
        return false;
    }
    int sbp = 0;

    // CorrId (length-prefixed string)
    send_body[sbp++] = 1;
    send_body[sbp++] = 'H';

    // EntityId = peer's queue ID
    send_body[sbp++] = (uint8_t)peer_queue_id_len;
    memcpy(&send_body[sbp], peer_queue_id, peer_queue_id_len);
    sbp += peer_queue_id_len;

    // SEND command
    memcpy(&send_body[sbp], "SEND ", 5);
    sbp += 5;

    // Flags
    send_body[sbp++] = 'T';
    send_body[sbp++] = ' ';

    // MsgBody
    memcpy(&send_body[sbp], client_msg, cmp);
    sbp += cmp;

    free(client_msg);  // No longer needed

    ESP_LOGD(TAG, "   SEND body: %d bytes", sbp);

    // 5. Build the "authorized" part (what we sign)
    // authorized = sessionIdentifier corrId entityId smpCommand
    uint8_t *authorized = malloc(HELLO_BUFFER_SIZE);
    if (!authorized) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate authorized!");
        free(send_body);
        return false;
    }
    int ap = 0;
    
    // sessionIdentifier (length-prefixed)
    authorized[ap++] = 32;
    memcpy(&authorized[ap], session_id, 32);
    ap += 32;
    
    // The rest is send_body (corrId + entityId + command)
    memcpy(&authorized[ap], send_body, sbp);
    ap += sbp;
    
    free(send_body);
    
    // 6. Sign the authorized part with Ed25519
    uint8_t signature[64];
    crypto_sign_detached(signature, NULL, authorized, ap, snd_auth_private);
    
    ESP_LOGD(TAG, "   Signed SEND (%d bytes)", ap);
    
    // 7. Build transmission = authorization + authorized
    uint8_t *transmission = malloc(HELLO_BUFFER_SIZE);
    if (!transmission) {
        ESP_LOGE(TAG, "   [FAIL] Failed to allocate transmission!");
        free(authorized);
        return false;
    }
    int tp = 0;
    
    // authorization = [length][signature]
    transmission[tp++] = 64;  // Signature length
    memcpy(&transmission[tp], signature, 64);
    tp += 64;
    
    // authorized part (without the length prefix for session_id this time - it's already in authorized)
    memcpy(&transmission[tp], authorized, ap);
    tp += ap;
    
    free(authorized);
    
    ESP_LOGD(TAG, "   Transmission: %d bytes (with signature)", tp);

    // 8. Send
    int ret = smp_write_command_block(ssl, block, transmission, tp);
    free(transmission);  // No longer needed
    
    if (ret != 0) {
        ESP_LOGE(TAG, "   [FAIL] Send failed: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "   [SEND] HELLO sent! Waiting for response...");
    
    // 9. Wait for OK response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] No response!");
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Look for OK or OK#
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   [OK] HELLO accepted by server!");
            handshake_state.hello_sent = true;
            
            // Update prev_msg_hash for next message
            mbedtls_sha256(hello_plain, hello_plain_len, handshake_state.prev_msg_hash, 0);
            
            // Evgeny's Rule: persist msg_id + prev_msg_hash before next send
            handshake_save_state();
            
            return true;
        }
    }
    
    // Debug: show full response with better parsing
    ESP_LOGW(TAG, "   Response not OK (%d bytes)", content_len);
    
    // Try to find and show error message
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R' && resp[i+3] == ' ') {
            // Found "ERR ", print the error type
            char err_type[32] = {0};
            int j = 0;
            for (int k = i + 4; k < content_len && j < 31 && resp[k] >= 0x20 && resp[k] < 0x7F; k++) {
                err_type[j++] = resp[k];
            }
            ESP_LOGE(TAG, "   [STOP] Server Error: ERR %s", err_type);
            break;
        }
    }
    
    return false;
}

// ============== Shared Encrypt+Send Pipeline ==============

/**
 * Shared encrypt+send pipeline for all outgoing agent messages.
 * Extracted from send_chat_message - zero code duplication.
 *
 * Pipeline: AgentMessage -> Ratchet Encrypt -> AgentMsgEnvelope ->
 *   PrivHeader('_') -> ClientMessage -> E2E Pad -> crypto_box ->
 *   PubHeader -> SEND (signed) -> OK check -> update prev_msg_hash
 *
 * @param corr_id   CorrId character: 'A' for A_MSG, 'R' for Receipt, 'H' for Hello
 * @param notify    true = 'T' (push notification), false = 'F' (silent)
 * @param label     Logging label (e.g. "A_MSG", "RECEIPT")
 */
static bool encrypt_and_send_agent_msg(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    const uint8_t *msg_plain,
    int msg_plain_len,
    char corr_id,
    bool notify,
    const char *label
) {
    #define SEND_BUFFER_SIZE 17000
    #define E2E_PADDED_LENGTH 16000
    
    // 1. Build AgentMsgEnvelope (Ratchet encrypt)
    uint8_t *agent_envelope = malloc(SEND_BUFFER_SIZE);
    if (!agent_envelope) {
        ESP_LOGE(TAG, "   [FAIL] [%s] malloc agent_envelope failed!", label);
        return false;
    }
    
    // PrivHeader '_' at offset 0, AgentMsgEnvelope at offset 1
    agent_envelope[0] = '_';
    int envelope_len = build_agent_msg_envelope(ratchet, msg_plain, msg_plain_len,
                                                  agent_envelope + 1, SEND_BUFFER_SIZE - 1);
    if (envelope_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] [%s] Ratchet encrypt failed!", label);
        free(agent_envelope);
        return false;
    }
    int total_plain_len = 1 + envelope_len;
    
    ESP_LOGD(TAG, "   [%s] ClientMessage: %d bytes", label, total_plain_len);
    
    // 2. E2E Pad: [Word16 BE len][ClientMessage][0x23 padding -> 16000]
    uint8_t *padded = malloc(E2E_PADDED_LENGTH);
    if (!padded) {
        ESP_LOGE(TAG, "   [FAIL] [%s] malloc padded failed!", label);
        free(agent_envelope);
        return false;
    }
    
    padded[0] = (total_plain_len >> 8) & 0xFF;
    padded[1] = total_plain_len & 0xFF;
    memcpy(&padded[2], agent_envelope, total_plain_len);
    memset(&padded[2 + total_plain_len], '#', E2E_PADDED_LENGTH - 2 - total_plain_len);
    free(agent_envelope);
    
    // 3. crypto_box: PubHeader + nonce + encrypted
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);
    
    uint8_t *client_msg = malloc(SEND_BUFFER_SIZE);
    if (!client_msg) {
        ESP_LOGE(TAG, "   [FAIL] [%s] malloc client_msg failed!", label);
        free(padded);
        return false;
    }
    int cmp = 0;
    
    // PubHeader: version 4
    client_msg[cmp++] = 0x00;
    client_msg[cmp++] = 0x04;
    
    // PubHeader: '0' = Nothing (no new DH key)
    client_msg[cmp++] = '0';
    
    // Nonce
    memcpy(&client_msg[cmp], nonce, 24);
    cmp += 24;
    
    // Encrypt
    uint8_t *enc_buf = malloc(E2E_PADDED_LENGTH + crypto_box_MACBYTES);
    if (!enc_buf) {
        ESP_LOGE(TAG, "   [FAIL] [%s] malloc enc_buf failed!", label);
        free(client_msg);
        free(padded);
        return false;
    }
    
    if (crypto_box_easy(enc_buf, padded, E2E_PADDED_LENGTH,
                        nonce, peer_dh_public, our_dh_private) != 0) {
        ESP_LOGE(TAG, "   [FAIL] [%s] crypto_box failed!", label);
        free(enc_buf);
        free(client_msg);
        free(padded);
        return false;
    }
    int enc_len = E2E_PADDED_LENGTH + crypto_box_MACBYTES;
    free(padded);
    
    memcpy(&client_msg[cmp], enc_buf, enc_len);
    cmp += enc_len;
    free(enc_buf);
    
    ESP_LOGD(TAG, "   [%s] ClientMsgEnvelope: %d bytes", label, cmp);
    
    // 4. Build SEND command
    uint8_t *send_body = malloc(SEND_BUFFER_SIZE);
    if (!send_body) {
        free(client_msg);
        return false;
    }
    int sbp = 0;
    
    // CorrId
    send_body[sbp++] = 1;
    send_body[sbp++] = corr_id;
    
    // EntityId = peer's queue
    send_body[sbp++] = (uint8_t)peer_queue_id_len;
    memcpy(&send_body[sbp], peer_queue_id, peer_queue_id_len);
    sbp += peer_queue_id_len;
    
    // SEND command
    memcpy(&send_body[sbp], "SEND ", 5);
    sbp += 5;
    
    // MsgFlags: 'T' = notification True, 'F' = silent
    send_body[sbp++] = notify ? 'T' : 'F';
    send_body[sbp++] = ' ';
    
    // MsgBody
    memcpy(&send_body[sbp], client_msg, cmp);
    sbp += cmp;
    free(client_msg);
    
    ESP_LOGD(TAG, "   [%s] SEND body: %d bytes (notify=%c)", label, sbp, notify ? 'T' : 'F');
    
    // 5. Sign: authorized = sessionId + send_body
    uint8_t *authorized = malloc(SEND_BUFFER_SIZE);
    if (!authorized) {
        free(send_body);
        return false;
    }
    int ap = 0;
    
    authorized[ap++] = 32;
    memcpy(&authorized[ap], session_id, 32);
    ap += 32;
    
    memcpy(&authorized[ap], send_body, sbp);
    ap += sbp;
    free(send_body);
    
    uint8_t signature[64];
    // DEBUG Bug#21: dump session_id and first 8 bytes of signing key
    ESP_LOG_BUFFER_HEX("SIG_SESS", session_id, 32);
    ESP_LOG_BUFFER_HEX("SIG_AUTH", snd_auth_private, 8);
    crypto_sign_detached(signature, NULL, authorized, ap, snd_auth_private);
    
    ESP_LOGD(TAG, "   [%s] Signed SEND (%d bytes)", label, ap);
    
    // 6. Transmission = [sigLen][signature][authorized]
    uint8_t *transmission = malloc(SEND_BUFFER_SIZE);
    if (!transmission) {
        free(authorized);
        return false;
    }
    int tp = 0;
    
    transmission[tp++] = 64;
    memcpy(&transmission[tp], signature, 64);
    tp += 64;
    
    memcpy(&transmission[tp], authorized, ap);
    tp += ap;
    free(authorized);
    
    ESP_LOGD(TAG, "   [%s] Transmission: %d bytes", label, tp);
    
    // Pre-compute prev_msg_hash BEFORE network write.
    // On success: used normally by the OK handler below.
    // On failure: saved in retry buffer for deferred handshake_save_state().
    uint8_t pre_msg_hash[32];
    mbedtls_sha256(msg_plain, msg_plain_len, pre_msg_hash, 0);
    
    // 7. Send!
    int ret = smp_write_command_block(ssl, block, transmission, tp);

    if (ret != 0) {
        ESP_LOGE(TAG, "   [%s] Write failed: %d - saving retry buffer", label, ret);

        // Free old retry buffer if any
        if (s_retry.transmission) {
            sodium_memzero(s_retry.transmission, s_retry.transmission_len);
            free(s_retry.transmission);
        }

        // Move to PSRAM retry buffer (internal SRAM too scarce for long-term hold)
        s_retry.transmission = (uint8_t *)heap_caps_malloc(tp, MALLOC_CAP_SPIRAM);
        if (s_retry.transmission) {
            memcpy(s_retry.transmission, transmission, tp);
            s_retry.transmission_len = tp;
            memcpy(s_retry.prev_msg_hash, pre_msg_hash, 32);
            memcpy(s_retry.snd_auth_private, snd_auth_private, 64);
            s_retry.contact_slot = active_handshake_idx;
            s_retry.pending = true;
            ESP_LOGI(TAG, "   [%s] Retry buffer: %d bytes saved in PSRAM", label, tp);
        } else {
            ESP_LOGE(TAG, "   [%s] PSRAM alloc FAILED! Message lost.", label);
            s_retry.pending = false;
        }

        free(transmission);
        return false;
    }

    free(transmission);
    
    ESP_LOGI(TAG, "   [%s] sent! Waiting for response...", label);
    
    // 8. Wait for response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] [%s] No response!", label);
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   [%s] Server accepted!", label);
            
            // Update prev_msg_hash from pre-computed value
            memcpy(handshake_state.prev_msg_hash, pre_msg_hash, 32);
            
            // Evgeny's Rule: persist before next send
            handshake_save_state();
            
            return true;
        }
    }
    
    // Check for error
    ESP_LOGW(TAG, "   [%s] Response not OK (%d bytes)", label, content_len);
    
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R' && resp[i+3] == ' ') {
            char err_type[32] = {0};
            int j = 0;
            for (int k = i + 4; k < content_len && j < 31 && resp[k] >= 0x20 && resp[k] < 0x7F; k++) {
                err_type[j++] = resp[k];
            }
            ESP_LOGE(TAG, "   [STOP] [%s] Server Error: ERR %s", label, err_type);
            break;
        }
    }
    
    return false;
}

// ============== Send Chat Message ==============

/**
 * Send a chat message to peer via A_MSG.
 */
bool send_chat_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    const char *message
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [SEND] SENDING CHAT MESSAGE (A_MSG)                              |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    
    handshake_state.ratchet = ratchet;
    
    // Build A_MSG plaintext
    uint8_t msg_plain[512];
    int msg_plain_len = build_chat_message(message, msg_plain, sizeof(msg_plain));
    if (msg_plain_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] Failed to build A_MSG!");
        return false;
    }
    
    bool ok = encrypt_and_send_agent_msg(
        ssl, block, session_id, peer_queue_id, peer_queue_id_len,
        peer_dh_public, our_dh_private, our_dh_public,
        ratchet, snd_auth_private,
        msg_plain, msg_plain_len,
        'A', true, "A_MSG"
    );
    
    if (ok) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "+----------------------------------------+");
        ESP_LOGI(TAG, "|  [SUCCESS] A_MSG ACCEPTED BY SERVER! [SUCCESS]                     |");
        ESP_LOGI(TAG, "|  Message \"%s\" sent to peer!                                  |", message);
        ESP_LOGI(TAG, "+----------------------------------------+");
    }
    
    return ok;
}

// ============== Delivery Receipt (A_RCVD) ==============

/**
 * Build A_RCVD (delivery receipt) message.
 *
 * Format:
 *   agentMessage = 'M' + sndMsgId(8B) + prevMsgHash + 'V' + Word16(count=1) + receipt
 *   receipt = agentMsgId(8B) + msgHash(shortString 32B) + rcptInfo(Large 0B)
 *
 * @param peer_snd_msg_id  The sndMsgId from the received message (peer's msg_id)
 * @param msg_hash         SHA256 hash of the decrypted body (32 bytes)
 * @param output           Output buffer
 * @param max_len          Output buffer size
 * @return length of built message, or -1 on error
 */
static int build_receipt_message(uint64_t peer_snd_msg_id, const uint8_t *msg_hash,
                                  uint8_t *output, int max_len) {
    int p = 0;
    
    // agentMessage tag = 'M'
    output[p++] = 'M';
    
    // agentMsgId = 8 bytes Big-Endian (our sequential message ID)
    handshake_state.msg_id++;
    uint64_t msg_id = handshake_state.msg_id;
    output[p++] = (msg_id >> 56) & 0xFF;
    output[p++] = (msg_id >> 48) & 0xFF;
    output[p++] = (msg_id >> 40) & 0xFF;
    output[p++] = (msg_id >> 32) & 0xFF;
    output[p++] = (msg_id >> 24) & 0xFF;
    output[p++] = (msg_id >> 16) & 0xFF;
    output[p++] = (msg_id >> 8) & 0xFF;
    output[p++] = msg_id & 0xFF;
    
    // prevMsgHash = shortString
    if (handshake_state.msg_id == 1) {
        output[p++] = 0x00;  // empty (shouldn't happen for receipt, but safe)
    } else {
        output[p++] = 32;
        memcpy(&output[p], handshake_state.prev_msg_hash, 32);
        p += 32;
    }
    
    // aMessage = A_RCVD = 'V' (delivery receipt tag)
    output[p++] = 'V';
    
    // Word8 count = 1 (one receipt) - verified from App's own receipt
    output[p++] = 0x01;
    
    // receipt = agentMsgId(8B) + msgHash(shortString) + rcptInfo(Large)
    
    // agentMsgId = peer's sndMsgId (8 bytes BE)
    output[p++] = (peer_snd_msg_id >> 56) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 48) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 40) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 32) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 24) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 16) & 0xFF;
    output[p++] = (peer_snd_msg_id >> 8) & 0xFF;
    output[p++] = peer_snd_msg_id & 0xFF;
    
    // msgHash = shortString (Word8 len + hash)
    output[p++] = 32;  // len = 32
    memcpy(&output[p], msg_hash, 32);
    p += 32;
    
    // rcptInfo = Large (Word16 BE len + data) - empty (len=0)
    // Verified: App sends Word16, not Word32
    output[p++] = 0x00;
    output[p++] = 0x00;
    
    ESP_LOGI(TAG, "   [MSG] Receipt: msg_id=%llu, peer_sndMsgId=%llu, %d bytes",
             (unsigned long long)msg_id, (unsigned long long)peer_snd_msg_id, p);
    
    return p;
}

/**
 * Send a delivery receipt (A_RCVD) to peer.
 * Uses shared encrypt_and_send_agent_msg() - silent (no push notification).
 *
 * @param peer_snd_msg_id  The sndMsgId from the received message
 * @param msg_hash         SHA256 hash of the decrypted body (32 bytes)
 */
bool send_receipt_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    uint64_t peer_snd_msg_id,
    const uint8_t *msg_hash
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [MSG] SENDING DELIVERY RECEIPT (A_RCVD)                        |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    
    handshake_state.ratchet = ratchet;
    
    // Build receipt plaintext
    uint8_t rcpt_plain[256];
    int rcpt_plain_len = build_receipt_message(peer_snd_msg_id, msg_hash,
                                                rcpt_plain, sizeof(rcpt_plain));
    if (rcpt_plain_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] Failed to build receipt!");
        return false;
    }
    
    bool ok = encrypt_and_send_agent_msg(
        ssl, block, session_id, peer_queue_id, peer_queue_id_len,
        peer_dh_public, our_dh_private, our_dh_public,
        ratchet, snd_auth_private,
        rcpt_plain, rcpt_plain_len,
        'R', false, "RECEIPT"  // silent, no push notification
    );
    
    if (ok) {
        ESP_LOGI(TAG, "   [OK] Receipt delivered! (✓✓ in app)");
    }
    
    return ok;
}

// ============== Raw Agent Message (Queue Rotation) ==============

/**
 * Build agentMessage with a raw aMessage payload.
 * Used for QADD ("QA"), QKEY ("QK"), QUSE ("QU"), QTEST ("QT").
 *
 * Format: agentMessage = 'M' + agentMsgHeader + aMessage
 *   agentMsgHeader = agentMsgId(8B BE) + prevMsgHash(shortString)
 *   aMessage = raw payload (caller provides tag + data)
 *
 * @param a_message     Raw aMessage payload (e.g. "QA" + sndQueues binary)
 * @param a_message_len Length of aMessage
 * @param output        Output buffer for full agentMessage
 * @param max_len       Output buffer size
 * @return length of built message, or -1 on error
 */
static int build_raw_agent_message(const uint8_t *a_message, int a_message_len,
                                    uint8_t *output, int max_len) {
    int p = 0;

    // agentMessage tag = 'M'
    if (p + 1 > max_len) return -1;
    output[p++] = 'M';

    // agentMsgId = 8 bytes Big-Endian
    if (p + 8 > max_len) return -1;
    handshake_state.msg_id++;
    uint64_t msg_id = handshake_state.msg_id;
    output[p++] = (msg_id >> 56) & 0xFF;
    output[p++] = (msg_id >> 48) & 0xFF;
    output[p++] = (msg_id >> 40) & 0xFF;
    output[p++] = (msg_id >> 32) & 0xFF;
    output[p++] = (msg_id >> 24) & 0xFF;
    output[p++] = (msg_id >> 16) & 0xFF;
    output[p++] = (msg_id >> 8) & 0xFF;
    output[p++] = msg_id & 0xFF;

    // prevMsgHash = shortString
    if (handshake_state.msg_id == 1) {
        if (p + 1 > max_len) return -1;
        output[p++] = 0x00;  // empty
    } else {
        if (p + 1 + 32 > max_len) return -1;
        output[p++] = 32;
        memcpy(&output[p], handshake_state.prev_msg_hash, 32);
        p += 32;
    }

    // aMessage = raw payload from caller (tag + data)
    if (p + a_message_len > max_len) return -1;
    memcpy(&output[p], a_message, a_message_len);
    p += a_message_len;

    return p;
}

/**
 * Send a raw agent message (QADD/QKEY/QUSE/QTEST) to peer.
 * Uses the same encrypt pipeline as A_MSG and A_RCVD.
 *
 * The caller builds the aMessage payload (e.g. "QA" + binary data)
 * and this function wraps it in agentMessage header, then encrypts
 * and sends via the shared pipeline.
 *
 * @param a_message     Raw aMessage payload (tag + data)
 * @param a_message_len Length of aMessage
 * @param corr_id       CorrId character (e.g. 'Q' for queue rotation)
 * @param notify        true = push notification, false = silent
 * @param label         Logging label (e.g. "QADD")
 */
bool send_raw_agent_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private,
    const uint8_t *a_message,
    int a_message_len,
    char corr_id,
    bool notify,
    const char *label
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [SEND] SENDING %s                     |", label);
    ESP_LOGI(TAG, "+----------------------------------------+");

    handshake_state.ratchet = ratchet;

    // Build full agentMessage: 'M' + header + raw aMessage
    uint8_t msg_plain[1024];
    int msg_plain_len = build_raw_agent_message(a_message, a_message_len,
                                                 msg_plain, sizeof(msg_plain));
    if (msg_plain_len < 0) {
        ESP_LOGE(TAG, "   [FAIL] Failed to build %s agentMessage!", label);
        return false;
    }

    ESP_LOGI(TAG, "   [%s] agentMessage: %d bytes (aMessage: %d bytes)",
             label, msg_plain_len, a_message_len);

    bool ok = encrypt_and_send_agent_msg(
        ssl, block, session_id, peer_queue_id, peer_queue_id_len,
        peer_dh_public, our_dh_private, our_dh_public,
        ratchet, snd_auth_private,
        msg_plain, msg_plain_len,
        corr_id, notify, label
    );

    if (ok) {
        ESP_LOGI(TAG, "   [OK] %s accepted by server!", label);
    }

    return ok;
}

// ============== Parse Incoming HELLO ==============

/**
 * Parse a decrypted message to check if it's HELLO
 * 
 * Format:
 * agentMessage = %s"M" agentMsgHeader aMessage msgPadding
 * agentMsgHeader = agentMsgId prevMsgHash  
 * HELLO = %s"H"
 */
bool parse_hello_message(const uint8_t *data, int len, uint64_t *msg_id_out) {
    if (len < 10) {
        return false;
    }
    
    int p = 0;
    
    // Check for 'M' tag (agentMessage)
    if (data[p++] != 'M') {
        ESP_LOGD(TAG, "   Not agentMessage (tag='%c')", data[p-1]);
        return false;
    }
    
    // agentMsgId = 8 bytes Big-Endian
    uint64_t msg_id = 0;
    for (int i = 0; i < 8; i++) {
        msg_id = (msg_id << 8) | data[p++];
    }
    
    // prevMsgHash = shortString [len][data...]
    uint8_t hash_len = data[p++];
    p += hash_len;
    
    if (p >= len) {
        return false;
    }
    
    // aMessage - check for HELLO = 'H'
    if (data[p] == 'H') {
        ESP_LOGI(TAG, "   [OK] HELLO parsed! MsgId: %llu", (unsigned long long)msg_id);
        handshake_state.hello_received = true;
        if (msg_id_out) *msg_id_out = msg_id;
        return true;
    }
    
    ESP_LOGD(TAG, "   Not HELLO (cmd='%c' 0x%02x)", data[p], data[p]);
    return false;
}

// ============== Complete Connection Handshake ==============

/**
 * Complete the duplex connection handshake after sending AgentConfirmation.
 * 
 * From agent-protocol.md:
 * - After sending confirmation, we (joining party) must:
 *   1. Subscribe to our reply queue (to receive confirmation back)
 *   2. Receive confirmation from initiating party
 *   3. Send HELLO message
 *   4. Receive HELLO back
 *   5. Connection established!
 * 
 * For Fast Duplex (modern apps), steps 1-2 may be skipped.
 */
bool complete_handshake(
    mbedtls_ssl_context *peer_ssl,
    uint8_t *block,
    const uint8_t *peer_session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet,
    const uint8_t *snd_auth_private  // NEW!
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  [CONN] COMPLETING DUPLEX CONNECTION (Confirmation only)          |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "");
    
    // Reset handshake state
    memset(&handshake_state, 0, sizeof(handshake_state));
    
    // HELLO is sent from main.c after KEY command
    ESP_LOGI(TAG, "   [1/2] AgentConfirmation [OK] (already sent)");
    ESP_LOGI(TAG, "   [2/2] HELLO will be sent from main.c after KEY ⏳");
    
    handshake_state.confirmation_received = true;
    
    return true;
}

// ============== Status Getters ==============

bool is_handshake_complete(void) {
    return handshake_state.hello_sent && handshake_state.hello_received;
}

bool is_hello_sent(void) {
    return handshake_state.hello_sent;
}

bool is_hello_received(void) {
    return handshake_state.hello_received;
}

bool is_connected(void) {
    return handshake_state.connected;
}

void reset_handshake_state(void) {
    memset(&handshake_state, 0, sizeof(handshake_state));
}

uint64_t handshake_get_last_msg_id(void) {
    return handshake_state.msg_id;
}

// ============== Retry After Write Failure ==============

/**
 * Retry sending the last failed message after reconnect.
 * Uses the saved encrypted payload - no re-encryption, no ratchet mutation.
 *
 * @param ssl    New SSL context (after reconnect)
 * @param block  SMP block buffer (SMP_BLOCK_SIZE)
 * @return true if retry succeeded
 */
bool handshake_retry_send(mbedtls_ssl_context *ssl, uint8_t *block,
                          const uint8_t *new_session_id) {
    if (!s_retry.pending || !s_retry.transmission || s_retry.transmission_len == 0) {
        ESP_LOGD(TAG, "No pending retry");
        return false;
    }

    ESP_LOGI(TAG, "Retrying failed send: %zu bytes", s_retry.transmission_len);

    // Transmission layout:
    //   [0]      sigLen = 64 (0x40)
    //   [1-64]   signature (64 bytes)
    //   [65]     sessIdLen = 32 (0x20)
    //   [66-97]  sessionId (32 bytes)
    //   [98+]    send_body (corrId + entityId + SEND + encrypted msg)
    //
    // After reconnect, the server has a NEW session_id.
    // The old signature covers the OLD session_id - ERR AUTH guaranteed.
    // Fix: replace session_id, re-sign, then send.

    uint8_t *tx = s_retry.transmission;
    size_t tx_len = s_retry.transmission_len;

    // Sanity check: sigLen must be 64, sessIdLen must be 32
    if (tx_len < 99 || tx[0] != 64 || tx[65] != 32) {
        ESP_LOGE(TAG, "Retry buffer format invalid: sigLen=%d sessIdLen=%d len=%zu",
                 tx[0], tx[65], tx_len);
        return false;
    }

    // 1. Replace session_id at offset 66-97
    memcpy(&tx[66], new_session_id, 32);
    ESP_LOG_BUFFER_HEX("RETRY_NEW_SESS", new_session_id, 32);

    // 2. Re-sign: "authorized" = everything from offset 65 onward (sessIdLen + sessionId + send_body)
    size_t authorized_len = tx_len - 65;
    uint8_t new_signature[64];
    crypto_sign_detached(new_signature, NULL, &tx[65], authorized_len, s_retry.snd_auth_private);

    // 3. Replace signature at offset 1-64
    memcpy(&tx[1], new_signature, 64);

    ESP_LOGI(TAG, "Retry: session_id replaced + re-signed (%zu bytes authorized)", authorized_len);

    // 4. Send
    int ret = smp_write_command_block(ssl, block, tx, tx_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Retry write ALSO failed: %d - keeping buffer", ret);
        return false;
    }

    // Wait for OK response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "Retry: no server response");
        return false;
    }

    uint8_t *resp = block + 2;
    bool ok = false;
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i + 1] == 'K') {
            ok = true;
            break;
        }
    }

    if (ok) {
        ESP_LOGI(TAG, "Retry SUCCEEDED!");

        // Apply the saved prev_msg_hash and persist
        memcpy(handshake_state.prev_msg_hash, s_retry.prev_msg_hash, 32);
        handshake_save_state();

        // Clean up
        handshake_clear_retry();
        return true;
    }

    ESP_LOGW(TAG, "Retry: server did not respond OK (%d bytes)", content_len);
    return false;
}

bool handshake_has_retry_pending(void) {
    return s_retry.pending;
}

void handshake_clear_retry(void) {
    if (s_retry.transmission) {
        // SEC: encrypted payload + ratchet state = plaintext reconstructable.
        // sodium_memzero is mandatory crypto hygiene, not paranoia.
        sodium_memzero(s_retry.transmission, s_retry.transmission_len);
        free(s_retry.transmission);
        s_retry.transmission = NULL;
    }
    s_retry.transmission_len = 0;
    s_retry.pending = false;
    sodium_memzero(s_retry.prev_msg_hash, 32);
    sodium_memzero(s_retry.snd_auth_private, 64);
    s_retry.contact_slot = 0;
}

// ============== Persistence ==============

// Compact struct for NVS - only the fields needed after reboot
typedef struct {
    uint64_t msg_id;
    uint8_t prev_msg_hash[32];
    uint8_t valid;  // 1 = valid
} handshake_persist_t;

bool handshake_save_state(void) {
    if (!handshake_array) return false;

    handshake_persist_t data;
    data.msg_id = handshake_state.msg_id;
    memcpy(data.prev_msg_hash, handshake_state.prev_msg_hash, 32);
    data.valid = 1;

    char key[16];
    snprintf(key, sizeof(key), "hand_%02x", active_handshake_idx);

    esp_err_t ret = smp_storage_save_blob_sync(key, &data, sizeof(data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "handshake_save_state('%s') FAILED: %s", key, esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "Handshake [%d] saved: msg_id=%llu",
             active_handshake_idx, (unsigned long long)data.msg_id);
    return true;
}

bool handshake_load_state(void) {
    if (!handshake_array) return false;

    char key[16];
    snprintf(key, sizeof(key), "hand_%02x", active_handshake_idx);

    if (!smp_storage_exists(key)) {
        // Fallback to legacy key "hand_00" for backward compatibility
        if (active_handshake_idx == 0 && smp_storage_exists("hand_00")) {
            snprintf(key, sizeof(key), "hand_00");
            ESP_LOGI(TAG, "handshake_load_state: using legacy key 'hand_00'");
        } else {
            ESP_LOGI(TAG, "handshake_load_state: '%s' not found", key);
            return false;
        }
    }

    handshake_persist_t data;
    size_t loaded_len = 0;
    esp_err_t ret = smp_storage_load_blob(key, &data, sizeof(data), &loaded_len);
    if (ret != ESP_OK || loaded_len != sizeof(data) || !data.valid) {
        ESP_LOGW(TAG, "handshake_load_state: invalid data from '%s'", key);
        return false;
    }

    handshake_state.msg_id = data.msg_id;
    memcpy(handshake_state.prev_msg_hash, data.prev_msg_hash, 32);

    ESP_LOGI(TAG, "Handshake [%d] restored: msg_id=%llu",
             active_handshake_idx, (unsigned long long)data.msg_id);
    return true;
}