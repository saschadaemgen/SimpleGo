/**
 * SimpleGo - smp_queue.h
 * SMP Queue Management (NEW, SUB, KEY, ACK commands)
 * v0.1.15-alpha
 */

#ifndef SMP_QUEUE_H
#define SMP_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

// Queue ID sizes
#define QUEUE_ID_SIZE       24
#define DH_PUBLIC_SIZE      32
#define AUTH_KEY_SIZE        32

// Our created queue info
typedef struct {
    // IDs from server
    uint8_t rcv_id[QUEUE_ID_SIZE];      // Our recipient ID (for receiving)
    int rcv_id_len;
    uint8_t snd_id[QUEUE_ID_SIZE];      // Sender ID (give to peer)
    int snd_id_len;
    
    // Server's DH key
    uint8_t srv_dh_public[DH_PUBLIC_SIZE];
    
    // Our keys for this queue
    uint8_t rcv_auth_public[AUTH_KEY_SIZE];   // Ed25519 public (signing)
    uint8_t rcv_auth_private[64];              // Ed25519 private
    uint8_t rcv_dh_public[DH_PUBLIC_SIZE];    // X25519 public
    uint8_t rcv_dh_private[DH_PUBLIC_SIZE];   // X25519 private

    // E2E keys (separate from server DH!)
    uint8_t e2e_public[DH_PUBLIC_SIZE];       // X25519 public for E2E
    uint8_t e2e_private[DH_PUBLIC_SIZE];      // X25519 private for E2E
    
    // Shared secret (our_dh_private * srv_dh_public)
    uint8_t shared_secret[DH_PUBLIC_SIZE];
    
    // Server info (for SMPQueueInfo encoding)
    char server_host[64];
    int server_port;
    uint8_t server_key_hash[32];
    
    bool valid;
} our_queue_t;

// Global instance
extern our_queue_t our_queue;

/**
 * Create a new receive queue on our SMP server
 */
bool queue_create(const char *host, int port);

/**
 * Subscribe to our queue to receive messages
 */
bool queue_subscribe(void);

/**
 * Read one raw SMP block from the Reply Queue connection.
 * Returns content_len (>0 success), <0 error. Data at buf+2.
 */
int queue_read_raw(uint8_t *buf, int buf_size, int timeout_ms);

/**
 * Reconnect to our reply queue server (after connection dropped).
 * Reuses stored host/port, does NOT create a new queue.
 */
bool queue_reconnect(void);

/**
 * Send KEY command to register peer's sender auth key on our reply queue.
 * Must be called after receiving peer's AgentConfirmation with PHConfirmation.
 *
 * @param peer_auth_key_spki  44-byte Ed25519 SPKI (as received in PrivHeader 'K')
 * @param key_len             Length (must be 44)
 * @return true if server accepted KEY
 */
bool queue_send_key(const uint8_t *peer_auth_key_spki, int key_len);

/**
 * Send ACK command to acknowledge a received message.
 * Must be called after processing each MSG to unblock the queue.
 * Auftrag 45a.
 *
 * @param msg_id      Message ID from MSG command (server nonce)
 * @param msg_id_len  Length of message ID
 * @return true if server accepted ACK
 */
bool queue_send_ack(const uint8_t *msg_id, int msg_id_len);

/**
 * Encode our queue as SMPQueueInfo for AgentConnInfoReply
 */
int queue_encode_info(uint8_t *buf, int max_len);

/**
 * Disconnect from our queue server
 */
void queue_disconnect(void);

// ============== Persistence (Auftrag 50b) ==============

/**
 * Save queue credentials to NVS.
 * Saves our_queue struct ("queue_our") + reply queue E2E peer key ("queue_e2e").
 * Uses save_blob_sync for Write-Before-Send safety.
 *
 * @return true on success
 */
bool queue_save_credentials(void);

/**
 * Load queue credentials from NVS.
 * Restores our_queue + reply queue E2E peer key.
 * Validates our_queue.valid before accepting.
 *
 * @return true on success (queue credentials restored)
 */
bool queue_load_credentials(void);

// Reply Queue E2E peer public key (from AgentConnInfoReply)
extern uint8_t reply_queue_e2e_peer_public[32];
extern bool reply_queue_e2e_peer_valid;

#endif // SMP_QUEUE_H