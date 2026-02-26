/**
 * SimpleGo - smp_globals.c
 * Global variable definitions
 */

#include "smp_types.h"

// SPKI headers
const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00
};

const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

// Base64URL characters
const char base64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Global state
contacts_db_t contacts_db = {0};
peer_queue_t pending_peer = {0};
peer_connection_t peer_conn = {0};
volatile bool wifi_connected = false;
