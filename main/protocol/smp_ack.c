/**
 * SimpleGo - SMP ACK Command
 * Consolidated ACK sending for all queue types
 */

#include "smp_ack.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "sodium.h"
#include "smp_network.h"

static const char *TAG = "SMP_ACK";

bool smp_send_ack(mbedtls_ssl_context *ssl, uint8_t *block,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret)
{
    if (!ssl || !block || !session_id || !recipient_id || !msg_id || !rcv_auth_secret) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    // Build ACK body: [version=1]['A'][rcvIdLen][rcvId]["ACK "][msgIdLen][msgId]
    uint8_t ack_body[128];
    int ap = 0;

    // T6-Fix4b: corrId must be 24 random bytes per SMP protocol spec
    ack_body[ap++] = 24;                                         // corrId length
    uint8_t ack_corr_id[24];
    esp_fill_random(ack_corr_id, 24);
    memcpy(&ack_body[ap], ack_corr_id, 24);
    ap += 24;
    ack_body[ap++] = (uint8_t)recipient_id_len;                  // recipient ID length
    memcpy(&ack_body[ap], recipient_id, recipient_id_len);
    ap += recipient_id_len;
    ack_body[ap++] = 'A';                                        // ACK command
    ack_body[ap++] = 'C';
    ack_body[ap++] = 'K';
    ack_body[ap++] = ' ';
    ack_body[ap++] = (uint8_t)msg_id_len;                        // message ID length
    memcpy(&ack_body[ap], msg_id, msg_id_len);
    ap += msg_id_len;

    // Build data to sign: [sessLen=32][sessionId][body]
    uint8_t to_sign[192];
    int sp = 0;
    to_sign[sp++] = 32;
    memcpy(&to_sign[sp], session_id, 32);
    sp += 32;
    memcpy(&to_sign[sp], ack_body, ap);
    sp += ap;

    // Sign with Ed25519
    uint8_t sig[crypto_sign_BYTES];
    crypto_sign_detached(sig, NULL, to_sign, sp, rcv_auth_secret);

    // Build transport frame v7: [sigLen][signature][body] (no sessionId on wire)
    uint8_t transport[192];
    int tp = 0;
    transport[tp++] = crypto_sign_BYTES;
    memcpy(&transport[tp], sig, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    memcpy(&transport[tp], ack_body, ap);
    tp += ap;

    smp_write_command_block(ssl, block, transport, tp);

    ESP_LOGD(TAG, "ACK sent (rcvId=%02x%02x..., msgId=%02x%02x...)",
             recipient_id[0], recipient_id[1], msg_id[0], msg_id[1]);

    return true;
}
