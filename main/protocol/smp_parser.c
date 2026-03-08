/**
 * SimpleGo - smp_parser.c
 * Agent protocol message parser implementation
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_parser.h"
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_crypto.h"
#include "smp_peer.h"
#include "smp_contacts.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "smp_queue.h"
#include "reply_queue.h"   // Session 34 Phase 6: per-contact reply queues
#include "smp_ratchet.h"
#include "sodium.h"

static const char *TAG = "SMP_PARS";

// ============== Helper: Find simplex:/invitation in decrypted data ==============

static char* extract_full_invitation_uri(const uint8_t *data, int data_len, int *out_len) {
    for (int i = 0; i < data_len - 20; i++) {
        if (data[i] == 's' && data[i+1] == 'i' && data[i+2] == 'm' && 
            data[i+3] == 'p' && data[i+4] == 'l' && data[i+5] == 'e' &&
            data[i+6] == 'x' && data[i+7] == ':') {
            
            int start = i;
            int end = i;
            
            while (end < data_len && data[end] >= 32 && data[end] < 127) {
                end++;
            }
            
            int uri_len = end - start;
            if (uri_len > 50 && uri_len < 16384) {
                char *uri = malloc(uri_len + 1);
                if (uri) {
                    memcpy(uri, &data[start], uri_len);
                    uri[uri_len] = 0;
                    *out_len = uri_len;
                    return uri;
                }
            }
        }
    }
    return NULL;
}

// ============== Helper: Parse e2e parameter ==============

static void parse_e2e_params(const char *uri) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      🔐 PARSING E2E RATCHET PARAMETERS...");
    
    // Find e2e version (e2e=v=2-3 or after decode)
    const char *e2e_v = strstr(uri, "e2e=v=");
    if (!e2e_v) {
        e2e_v = strstr(uri, "&e2e=v");
    }
    
    if (e2e_v) {
        const char *v_start = strstr(e2e_v, "v=");
        if (v_start) {
            v_start += 2;
            char version[20] = {0};
            int vi = 0;
            while (*v_start && *v_start != '&' && *v_start != ' ' && vi < 19) {
                version[vi++] = *v_start++;
            }
            ESP_LOGI(TAG, "      E2E Version: %s", version);
        }
    }
    
    // Find x3dh= directly in the (decoded) URI
    const char *x3dh = strstr(uri, "x3dh=");
    if (!x3dh) {
        ESP_LOGW(TAG, "      ⚠️ No x3dh= parameter found!");
        return;
    }
    
    x3dh += 5;  // skip "x3dh="
    
    ESP_LOGI(TAG, "      🔑 X3DH Keys found!");
    
    // Find comma separator between two keys
    const char *comma = strchr(x3dh, ',');
    if (!comma) {
        ESP_LOGW(TAG, "      ⚠️ No comma found in x3dh - expected two keys!");
        return;
    }
    
    // First key (before comma)
    int key1_len = comma - x3dh;
    char key1_b64[150] = {0};
    if (key1_len > 0 && key1_len < (int)sizeof(key1_b64)) {
        memcpy(key1_b64, x3dh, key1_len);
        ESP_LOGI(TAG, "      Key1 B64 (%d chars): %.60s...", key1_len, key1_b64);
        
        // Clean up the base64 string - remove any trailing whitespace
        int clean_len = strlen(key1_b64);
        while (clean_len > 0 && (key1_b64[clean_len-1] == ' ' || key1_b64[clean_len-1] == '\n')) {
            key1_b64[--clean_len] = '\0';
        }
        
        uint8_t key1_spki[100];
        int key1_decoded = base64url_decode(key1_b64, key1_spki, sizeof(key1_spki));
        ESP_LOGI(TAG, "      Key1 decode result: %d bytes", key1_decoded);
        if (key1_decoded > 0) {
            ESP_LOGI(TAG, "      Key1 decoded: %d bytes", key1_decoded);
            
            // Verify X448 OID (1.3.101.111 = 2b 65 6f)
            if (key1_decoded >= 68 && 
                key1_spki[0] == 0x30 && key1_spki[2] == 0x30 &&
                key1_spki[4] == 0x06 && key1_spki[5] == 0x03 &&
                key1_spki[6] == 0x2b && key1_spki[7] == 0x65 && key1_spki[8] == 0x6f) {
                
                ESP_LOGI(TAG, "      ✅ Key1 is X448 (OID 1.3.101.111)!");
                memcpy(pending_peer.e2e_key1, key1_spki + 12, 56);
                pending_peer.has_e2e = 1;
                
            } else {
                ESP_LOGW(TAG, "      ⚠️ Key1 format unexpected (not X448 SPKI?)");
            }
        } else {
            ESP_LOGW(TAG, "      ❌ Key1 decode FAILED! Check base64url_decode");
            // Show first and last chars of the base64 for debugging
            ESP_LOGW(TAG, "         First 20 chars: %.20s", key1_b64);
            ESP_LOGW(TAG, "         Last 10 chars: %s", key1_b64 + (key1_len > 10 ? key1_len - 10 : 0));
        }
    }
    
    // Second key (after comma, until & or end)
    const char *key2_start = comma + 1;
    const char *key2_end = key2_start;
    while (*key2_end && *key2_end != '&' && *key2_end != ' ') key2_end++;
    
    int key2_len = key2_end - key2_start;
    char key2_b64[150] = {0};
    if (key2_len > 0 && key2_len < (int)sizeof(key2_b64)) {
        memcpy(key2_b64, key2_start, key2_len);
        ESP_LOGI(TAG, "      Key2 B64 (%d chars): %.60s...", key2_len, key2_b64);
        
        // Clean up
        int clean_len = strlen(key2_b64);
        while (clean_len > 0 && (key2_b64[clean_len-1] == ' ' || key2_b64[clean_len-1] == '\n')) {
            key2_b64[--clean_len] = '\0';
        }
        
        uint8_t key2_spki[100];
        int key2_decoded = base64url_decode(key2_b64, key2_spki, sizeof(key2_spki));
        ESP_LOGI(TAG, "      Key2 decode result: %d bytes", key2_decoded);
        if (key2_decoded > 0) {
            ESP_LOGI(TAG, "      Key2 decoded: %d bytes", key2_decoded);
            
            // Verify X448 OID
            if (key2_decoded >= 68 && 
                key2_spki[0] == 0x30 && key2_spki[2] == 0x30 &&
                key2_spki[4] == 0x06 && key2_spki[5] == 0x03 &&
                key2_spki[6] == 0x2b && key2_spki[7] == 0x65 && key2_spki[8] == 0x6f) {
                
                ESP_LOGI(TAG, "      ✅ Key2 is X448 (OID 1.3.101.111)!");
                memcpy(pending_peer.e2e_key2, key2_spki + 12, 56);
                
            } else {
                ESP_LOGW(TAG, "      ⚠️ Key2 format unexpected");
            }
        } else {
            ESP_LOGW(TAG, "      ❌ Key2 decode FAILED!");
        }
    }
    
    // Check for kem_key (PQ encryption)
    if (strstr(uri, "kem_key=")) {
        ESP_LOGI(TAG, "      🔒 KEM key found (Post-Quantum encryption!)");
    }
    
    if (pending_peer.has_e2e) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      ✅ E2E KEYS EXTRACTED SUCCESSFULLY!");
        ESP_LOGI(TAG, "      ⚠️  NOTE: X448 crypto not yet implemented!");
        ESP_LOGI(TAG, "      ⚠️  Need wolfSSL for X448 DH operations.");
    }
}

// ============== Agent Message Parsing ==============

void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   🔬 AGENT MESSAGE ANALYSIS:");
    
    const uint8_t *content = plain;
    int content_len = plain_len;
    
    if (plain_len >= 2) {
        uint16_t prefix_len = (plain[0] << 8) | plain[1];
        if (prefix_len > 0 && prefix_len < plain_len - 2 && prefix_len < 16100) {
            ESP_LOGI(TAG, "      📏 Length prefix: %d bytes (total: %d)", prefix_len, plain_len);
            content = plain + 2;
            content_len = prefix_len;
        }
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📊 Raw message structure (first 64 bytes):");
    printf("         ");
    for (int i = 0; i < 64 && i < content_len; i++) {
        printf("%02x ", content[i]);
        if ((i + 1) % 16 == 0) printf("\n         ");
    }
    printf("\n");
    
    // Scan for X25519 SPKI header
    int sender_key_offset = -1;
    uint8_t sender_key_raw[32];
    
    for (int i = 0; i < content_len - 44; i++) {
        if (content[i] == 0x30 && content[i+1] == 0x2a && content[i+2] == 0x30 &&
            content[i+3] == 0x05 && content[i+4] == 0x06 && content[i+5] == 0x03 &&
            content[i+6] == 0x2b && content[i+7] == 0x65 && content[i+8] == 0x6e &&
            content[i+9] == 0x03 && content[i+10] == 0x21 && content[i+11] == 0x00) {
            
            sender_key_offset = i;
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "      🔑 Found X25519 SPKI at offset %d!", i);
            
            memcpy(sender_key_raw, &content[i + 12], 32);
            break;
        }
    }
    
    if (sender_key_offset < 0) {
        ESP_LOGW(TAG, "      ⚠️ No X25519 SPKI found in message!");
        return;
    }
    
    int after_key_offset = sender_key_offset + 44;
    int after_key_len = content_len - after_key_offset;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "      📦 Data after SPKI key (%d bytes):", after_key_len);
    if (after_key_len > 0) {
    }
    
    // Try DH decryption
    if (after_key_len > 40) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      🔐 Attempting DH decryption on post-key data...");
        
        uint8_t *decrypted = malloc(after_key_len);
        if (decrypted) {
            int dec_len = decrypt_client_msg(&content[after_key_offset], after_key_len,
                                             sender_key_raw,
                                             contact->rcv_dh_secret,
                                             decrypted);
            
            if (dec_len > 0) {
                ESP_LOGI(TAG, "      ✅ DH Decryption SUCCESS! (%d bytes)", dec_len);
                
                if (dec_len >= 6) {
                    int toff = -1;
                    for (int i = 0; i < 10 && i < dec_len - 3; i++) {
                        if (decrypted[i] == '_') { toff = i; }  // 36b: find LAST '_' (handles __ separator)
                    }
                    if (toff < 0) toff = 2;
                    
                    uint16_t ver = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
                    char type = decrypted[toff + 3];
                    
                    ESP_LOGI(TAG, "");
                    ESP_LOGI(TAG, "      📦 Agent Version: %d", ver);
                    ESP_LOGI(TAG, "      📬 Message Type: '%c' (0x%02x)", 
                             (type >= 32 && type < 127) ? type : '?', type);
                    
                    if (type == 'C') {
                        ESP_LOGI(TAG, "      🎉 CONFIRMATION received!");
                    }
                    else if (type == 'I') {
                        ESP_LOGI(TAG, "      🎉 INVITATION received!");
                        
                        int uri_len = 0;
                        char *full_uri = extract_full_invitation_uri(decrypted, dec_len, &uri_len);
                        
                        if (full_uri) {
                            ESP_LOGI(TAG, "");
                            ESP_LOGI(TAG, "      📋 FULL INVITATION URI (%d chars):", uri_len);
                            
                            for (int pass = 0; pass < 4; pass++) {
                                if (!strchr(full_uri, '%')) break;
                                url_decode_inplace(full_uri);
                            }
                            
                            int show_len = strlen(full_uri);
                            for (int i = 0; i < show_len; i += 120) {
                                ESP_LOGI(TAG, "         %.120s", full_uri + i);
                            }

                            // Find where URI ends in original decrypted buffer
                            // Search for "simplex:" in decrypted to find start
                            int uri_start_in_dec = -1;
                            for (int i = 0; i < dec_len - 10; i++) {
                                if (decrypted[i] == 's' && decrypted[i+1] == 'i' && 
                                    decrypted[i+2] == 'm' && decrypted[i+3] == 'p' &&
                                    decrypted[i+4] == 'l' && decrypted[i+5] == 'e' &&
                                    decrypted[i+6] == 'x' && decrypted[i+7] == ':') {
                                    uri_start_in_dec = i;
                                    break;
                                }
                            }
                            
                            if (uri_start_in_dec >= 0) {
                                // Find where URI ends (first non-printable char)
                                int uri_end_in_dec = uri_start_in_dec;
                                while (uri_end_in_dec < dec_len && 
                                       decrypted[uri_end_in_dec] >= 32 && 
                                       decrypted[uri_end_in_dec] < 127) {
                                    uri_end_in_dec++;
}
                                
                                ESP_LOGI(TAG, "      📋 URI in decrypted: offset %d-%d", 
                                         uri_start_in_dec, uri_end_in_dec);
                                
                                // DEBUG: Show bytes AFTER URI
                                ESP_LOGI(TAG, "      📋 Bytes after URI (offset %d+):", uri_end_in_dec);
                                printf("         ");
                                for (int i = uri_end_in_dec; i < uri_end_in_dec + 80 && i < dec_len; i++) {
                                    printf("%02x ", decrypted[i]);
                                    if ((i - uri_end_in_dec + 1) % 16 == 0) printf("\n         ");
                                }
                                printf("\n");
                                ESP_LOGI(TAG, "      📋 Remaining bytes after URI: %d", dec_len - uri_end_in_dec);

                                // Search for ALL X25519 SPKI in entire message
                                ESP_LOGI(TAG, "      🔍 Searching ALL X25519 SPKI in decrypted:");
                                int found_count = 0;
                                for (int i = 0; i < dec_len - 44; i++) {
                                    if (decrypted[i] == 0x30 && decrypted[i+1] == 0x2a) {
                                        found_count++;
                                        uint8_t *key = &decrypted[i + 12];
                                        ESP_LOGI(TAG, "         [%d] SPKI at offset %d: %02x%02x%02x%02x...", 
                                                 found_count, i, key[0], key[1], key[2], key[3]);
                                    }
                                }
                                ESP_LOGI(TAG, "      📊 Total X25519 keys found: %d", found_count);
                                
                                // Search for X25519 SPKI AFTER the URI - DON'T SET reply_queue_e2e_peer_public here!
                                // The correct E2E key is extracted in main.c from the PubHeader SPKI
                                for (int i = uri_end_in_dec; i < dec_len - 44; i++) {
                                    if (decrypted[i] == 0x30 && decrypted[i+1] == 0x2a) {
                                        ESP_LOGI(TAG, "      🔑 Found X25519 SPKI at offset %d!", i);
                                        uint8_t *key = &decrypted[i + 12];
                                        ESP_LOGI(TAG, "         Key: %02x%02x%02x%02x...", 
                                                 key[0], key[1], key[2], key[3]);
                                        // NOTE: NOT setting reply_queue_e2e_peer_public here!
                                        // The correct key comes from PubHeader in Contact Queue message
                                        break;
                                    }
                                }
                            }
                            
                            // Parse SMP server info
                            char *smp_start = strstr(full_uri, "smp://");
                            if (smp_start) {
                                char *at = strchr(smp_start, '@');
                                char *slash = at ? strchr(at, '/') : NULL;
                                char *hash = slash ? strchr(slash, '#') : NULL;
                                
                                if (at && slash) {
                                    int hostlen = slash - at - 1;
                                    if (hostlen > 0 && hostlen < 63) {
                                        memcpy(pending_peer.host, at + 1, hostlen);
                                        pending_peer.host[hostlen] = 0;
                                        
                                        char *colon = strchr(pending_peer.host, ':');
                                        if (colon) {
                                            pending_peer.port = atoi(colon + 1);
                                            *colon = 0;
                                        } else {
                                            pending_peer.port = 5223;
                                        }
                                        
                                        ESP_LOGI(TAG, "");
                                        ESP_LOGI(TAG, "      🖥️  Peer Server: %s:%d", 
                                                 pending_peer.host, pending_peer.port);
                                    }
                                    
                                    char *qend = hash ? hash : (slash + strlen(slash));
                                    char *q_mark = strchr(slash, '?');
                                    if (q_mark && (!qend || q_mark < qend)) qend = q_mark;
                                    
                                    int qlen = qend - slash - 1;
                                    if (qlen > 0 && qlen < 48) {
                                        char qid_b64[48] = {0};
                                        memcpy(qid_b64, slash + 1, qlen);
                                        pending_peer.queue_id_len = base64url_decode(
                                            qid_b64, pending_peer.queue_id, 32);
                                        ESP_LOGI(TAG, "      📮 Queue ID: %s (%d bytes)", 
                                                 qid_b64, pending_peer.queue_id_len);
                                    }
                                }
                                
                                // Find dh= parameter (SMP level) - this is for SERVER-LEVEL encryption only!
                                char *dh = strstr(smp_start, "dh=");
                                if (dh) {
                                    dh += 3;
                                    char *dh_end = dh;
                                    while (*dh_end && *dh_end != '&' && *dh_end != ' ' && *dh_end != '#') dh_end++;
                                    int dh_len = dh_end - dh;
                                    
                                    if (dh_len > 40 && dh_len < 120) {
                                        char dh_b64[100] = {0};
                                        memcpy(dh_b64, dh, dh_len);
                                        
                                        uint8_t spki[48];
                                        int spki_len = base64url_decode(dh_b64, spki, 48);
                                        
                                        if (spki_len >= 44) {
                                            memcpy(pending_peer.dh_public, spki + 12, 32);
                                            pending_peer.has_dh = 1;
                                            
                                            // ======================================================
                                            // BUG FIX v0.1.18: DO NOT set reply_queue_e2e_peer_public here!
                                            // The dh= parameter is for SERVER-LEVEL queue encryption,
                                            // NOT for E2E Layer 2 encryption!
                                            // The correct E2E key comes from the PubHeader SPKI
                                            // in the Contact Queue message, extracted in main.c
                                            // ======================================================
                                            // OLD BUGGY CODE (REMOVED):
                                            // memcpy(reply_queue_e2e_peer_public, pending_peer.dh_public, 32);
                                            // reply_queue_e2e_peer_valid = true;
                                            
                                            ESP_LOGI(TAG, "      🔑 SMP DH Key: %02x%02x%02x%02x... ✅ (server-level only)",
                                                     pending_peer.dh_public[0], pending_peer.dh_public[1],
                                                     pending_peer.dh_public[2], pending_peer.dh_public[3]);
                                        }
                                    }
                                }
                            }
                            
                            // Parse E2E parameters (the critical part!)
                            parse_e2e_params(full_uri);
                            
                            pending_peer.valid = (strlen(pending_peer.host) > 0 && 
                                                  pending_peer.queue_id_len > 0);
                            
                            if (pending_peer.valid && pending_peer.has_dh) {
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      ╔══════════════════════════════════════╗");
                                if (pending_peer.has_e2e) {
                                    ESP_LOGI(TAG, "      ║  🎯 READY! (with E2E Ratchet keys)  ║");
                                } else {
                                    ESP_LOGI(TAG, "      ║  ⚠️  READY! (NO E2E keys found!)    ║");
                                }
                                ESP_LOGI(TAG, "      ╚══════════════════════════════════════╝");
                                
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      🚀 Auto-connecting to peer...");
                                
                                if (peer_connect(pending_peer.host, pending_peer.port)) {
                                    // Switch ratchet to this contact's slot
                                    int contact_idx = (int)(contact - contacts_db.contacts);
                                    ratchet_set_active(contact_idx);
                                    ESP_LOGI(TAG, "      Ratchet slot -> [%d] for X3DH + encrypt", contact_idx);
                                    send_agent_confirmation(contact, contact_idx);
                                    peer_disconnect();
                                }
                            }
                            
                            free(full_uri);
                        } else {
                            ESP_LOGW(TAG, "      ⚠️ Could not extract invitation URI!");
                        }
                    }
                }
                
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "      📝 Decrypted text (first 500 chars):");
                printf("         \"");
                for (int i = 0; i < dec_len && i < 500; i++) {
                    char c = decrypted[i];
                    if (c >= 32 && c < 127) printf("%c", c);
                    else if (c == '\n') printf("\\n");
                    else printf(".");
                }
                printf("\"\n");
            }
            free(decrypted);
        }
    }
    
    if (sender_key_offset > 0) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "      📋 Data BEFORE SPKI key (%d bytes):", sender_key_offset);
        
        printf("         Text: \"");
        for (int i = 0; i < sender_key_offset; i++) {
            char c = content[i];
            if (c >= 32 && c < 127) printf("%c", c);
            else printf(".");
        }
        printf("\"\n");
        
        for (int i = 0; i < sender_key_offset - 1; i++) {
            if (content[i] >= '1' && content[i] <= '9' && content[i+1] == ',') {
                ESP_LOGI(TAG, "      📌 Version '%c' found at offset %d", content[i], i);
            }
        }
    }
    
    ESP_LOGI(TAG, "");
}

// ============== NEW: Parse AgentConfirmation for Reply Queue E2E Key ==============

// Skip length-prefixed data, returns new offset or -1 on error
static int skip_len_prefixed(const uint8_t *data, int offset, int max_len) {
    if (offset >= max_len) return -1;
    
    int len;
    int new_offset;
    
    if (data[offset] < 0x20) {
        // Large encoding: 2-byte length
        if (offset + 2 > max_len) return -1;
        len = (data[offset] << 8) | data[offset + 1];
        new_offset = offset + 2;
    } else {
        // Small encoding: 1-byte length
        len = data[offset];
        new_offset = offset + 1;
    }
    
    if (new_offset + len > max_len) return -1;
    return new_offset + len;
}

/**
 * Parse AgentConnInfoReply and extract dhPublicKey (X25519)
 * Format: 'D' + list_len + SMPQueueInfo[] + connInfo
 */
static int parse_conn_info_reply(const uint8_t *data, size_t len, uint8_t *dh_public_out) {
    ESP_LOGI(TAG, "   📋 Parsing AgentConnInfoReply (%zu bytes)...", len);
    
    if (len < 3 || data[0] != 'D') {
        ESP_LOGE(TAG, "      ❌ Expected 'D', got 0x%02x", data[0]);
        return -1;
    }
    ESP_LOGI(TAG, "      ✅ Tag = 'D' (AgentConnInfoReply)");
    
    // Search for X25519 SPKI header in the data
    ESP_LOGI(TAG, "      🔎 Searching for X25519 SPKI (30 2a 30 05 06 03 2b 65 6e)...");
    
    for (size_t i = 2; i < len - 44; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x2a && 
            data[i+2] == 0x30 && data[i+3] == 0x05 &&
            data[i+4] == 0x06 && data[i+5] == 0x03 &&
            data[i+6] == 0x2b && data[i+7] == 0x65 &&
            data[i+8] == 0x6e) {
            
            ESP_LOGI(TAG, "      ✅ Found X25519 SPKI at offset %zu", i);
            memcpy(dh_public_out, &data[i + 12], 32);
            ESP_LOGI(TAG, "      🔑 dhPublicKey: %02x%02x%02x%02x%02x%02x%02x%02x...",
                     dh_public_out[0], dh_public_out[1], dh_public_out[2], dh_public_out[3],
                     dh_public_out[4], dh_public_out[5], dh_public_out[6], dh_public_out[7]);
            return 0;
        }
    }
    
    ESP_LOGE(TAG, "      ❌ X25519 SPKI not found!");
    return -1;
}

/**
 * Parse AgentConfirmation from ClientMessage
 * Extracts encConnInfo, decrypts with Double Ratchet, parses AgentConnInfoReply
 * and stores the Reply Queue E2E peer public key
 */
int parse_agent_confirmation(const uint8_t *cm_plain, int cm_plain_len) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  📬 PARSING AGENT CONFIRMATION                               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    
    int offset = 0;
    
    // ========== 1. Parse PrivHeader ==========
    if (cm_plain_len < 1) {
        ESP_LOGE(TAG, "   ❌ Empty ClientMessage");
        return -1;
    }
    
    uint8_t priv_header_tag = cm_plain[0];
    ESP_LOGI(TAG, "   [1] PrivHeader: '%c' (0x%02x)", 
             (priv_header_tag >= 0x20 && priv_header_tag < 0x7f) ? priv_header_tag : '.', 
             priv_header_tag);
    offset = 1;
    
    if (priv_header_tag == 'K') {
        // PHConfirmation - skip senderKey (length-prefixed SPKI)
        offset = skip_len_prefixed(cm_plain, offset, cm_plain_len);
        if (offset < 0) {
            ESP_LOGE(TAG, "   ❌ Failed to skip senderKey");
            return -1;
        }
        ESP_LOGI(TAG, "      Skipped senderKey, now at offset %d", offset);
    } else if (priv_header_tag != '_') {
        ESP_LOGE(TAG, "   ❌ Unknown PrivHeader: 0x%02x", priv_header_tag);
        return -1;
    }
    
    // ========== 2. Parse AgentMsgEnvelope header ==========
    if (offset + 3 > cm_plain_len) {
        ESP_LOGE(TAG, "   ❌ Not enough data for AgentConfirmation");
        return -1;
    }
    
    uint16_t agent_version = (cm_plain[offset] << 8) | cm_plain[offset + 1];
    uint8_t msg_tag = cm_plain[offset + 2];
    ESP_LOGI(TAG, "   [2] AgentVersion: %d, Tag: '%c'", agent_version, msg_tag);
    offset += 3;
    
    if (msg_tag != 'C') {
        ESP_LOGI(TAG, "   ℹ️  Not AgentConfirmation (tag='%c'), skipping", msg_tag);
        return -1;
    }
    
    // ========== 3. Parse Maybe e2eEncryption_ ==========
    if (offset >= cm_plain_len) {
        ESP_LOGE(TAG, "   ❌ Not enough data for e2eEncryption_");
        return -1;
    }
    
    uint8_t maybe_e2e = cm_plain[offset];
    ESP_LOGI(TAG, "   [3] Maybe e2eEncryption_: '%c'", maybe_e2e);
    offset++;
    
    if (maybe_e2e == '1') {
        // Has E2ERatchetParams X448: version(2) + key1(68) + key2(68) + maybe_kem
        if (offset + 2 > cm_plain_len) return -1;
        
        uint16_t e2e_version = (cm_plain[offset] << 8) | cm_plain[offset + 1];
        ESP_LOGI(TAG, "      E2E Version: %d", e2e_version);
        offset += 2;
        
        // Skip two X448 keys (68 bytes each)
        offset += 68;  // key1
        if (offset > cm_plain_len) return -1;
        offset += 68;  // key2
        if (offset > cm_plain_len) return -1;
        
        // Maybe KEM
        if (offset < cm_plain_len) {
            uint8_t maybe_kem = cm_plain[offset];
            ESP_LOGI(TAG, "      Maybe KEM: '%c'", maybe_kem);
            offset++;
            
            if (maybe_kem == '1') {
                // Has KEM - search for EncRatchetMessage start
                ESP_LOGI(TAG, "      ⚠️  KEM present, searching for encConnInfo...");
                for (int i = offset; i < cm_plain_len - 4; i++) {
                    if (cm_plain[i] == 0x7b && cm_plain[i+1] == 0x00 && cm_plain[i+2] == 0x02) {
                        ESP_LOGI(TAG, "      ✅ Found EncRatchetMessage at offset %d", i);
                        offset = i;
                        break;
                    }
                }
            }
        }
    } else if (maybe_e2e != ',') {
        ESP_LOGE(TAG, "   ❌ Unknown Maybe: 0x%02x", maybe_e2e);
        return -1;
    }
    
    // ========== 4. encConnInfo is the rest (Tail) ==========
    int enc_conn_info_len = cm_plain_len - offset;
    const uint8_t *enc_conn_info = &cm_plain[offset];
    
    ESP_LOGI(TAG, "   [4] encConnInfo: %d bytes at offset %d", enc_conn_info_len, offset);
    ESP_LOGI(TAG, "      First 16: %02x %02x %02x %02x %02x %02x %02x %02x...",
             enc_conn_info[0], enc_conn_info[1], enc_conn_info[2], enc_conn_info[3],
             enc_conn_info[4], enc_conn_info[5], enc_conn_info[6], enc_conn_info[7]);
    
    // ========== 5. Double Ratchet decrypt encConnInfo ==========
    if (!ratchet_is_initialized()) {
        ESP_LOGE(TAG, "   ❌ Ratchet not initialized!");
        return -1;
    }
    
    uint8_t *conn_info_plain = malloc(enc_conn_info_len);
    if (!conn_info_plain) {
        ESP_LOGE(TAG, "   ❌ malloc failed");
        return -1;
    }
    
    size_t conn_info_plain_len = 0;
    
    // Use the RECEIVER ratchet to decrypt (we're receiving from peer)
    int rc_result = ratchet_decrypt(enc_conn_info, enc_conn_info_len,
                                    conn_info_plain, &conn_info_plain_len);
    
    if (rc_result != 0) {
        ESP_LOGE(TAG, "   ❌ Ratchet decrypt FAILED (code %d)", rc_result);
        free(conn_info_plain);
        return -1;
    }
    
    ESP_LOGI(TAG, "   ✅ Ratchet decrypt SUCCESS! (%zu bytes)", conn_info_plain_len);
    ESP_LOGI(TAG, "      First 32:");
    printf("         ");
    for (size_t i = 0; i < 32 && i < conn_info_plain_len; i++) {
        printf("%02x ", conn_info_plain[i]);
    }
    printf("\n");
    
    // ========== 6. Parse AgentConnInfoReply ==========
    uint8_t dh_public[32];
    int parse_result = parse_conn_info_reply(conn_info_plain, conn_info_plain_len, dh_public);
    
    if (parse_result == 0) {
        // ========== 7. Store Reply Queue E2E peer public key ==========
        memcpy(reply_queue_e2e_peer_public, dh_public, 32);
        reply_queue_e2e_peer_valid = true;

        // Session 34 Phase 6: Also store in per-contact reply queue
        for (int rqi = 0; rqi < MAX_CONTACTS; rqi++) {
            if (!contacts_db.contacts[rqi].active) continue;
            if (contacts_db.contacts[rqi].sender_id_len == pending_peer.queue_id_len &&
                memcmp(contacts_db.contacts[rqi].sender_id, pending_peer.queue_id,
                       pending_peer.queue_id_len) == 0) {
                reply_queue_t *rq = reply_queue_get(rqi);
                if (rq && rq->valid) {
                    memcpy(rq->e2e_peer_public, dh_public, 32);
                    rq->e2e_peer_valid = true;
                    reply_queue_save(rqi);
                    ESP_LOGI(TAG, "   RQ[%d] peer E2E key stored", rqi);
                }
                break;
            }
        }
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  🎉 REPLY QUEUE E2E KEY EXTRACTED!                           ║");
        ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TAG, "   🔑 reply_queue_e2e_peer_public: %02x%02x%02x%02x%02x%02x%02x%02x...",
                 reply_queue_e2e_peer_public[0], reply_queue_e2e_peer_public[1],
                 reply_queue_e2e_peer_public[2], reply_queue_e2e_peer_public[3],
                 reply_queue_e2e_peer_public[4], reply_queue_e2e_peer_public[5],
                 reply_queue_e2e_peer_public[6], reply_queue_e2e_peer_public[7]);
    }
    
    free(conn_info_plain);
    return parse_result;
}
