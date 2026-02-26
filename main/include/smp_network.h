/**
 * SimpleGo - smp_network.h
 * TLS/TCP networking for SMP protocol
 */

#ifndef SMP_NETWORK_H
#define SMP_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "mbedtls/ssl.h"

// TCP connection
int smp_tcp_connect(const char *host, int port);

// mbedTLS I/O callbacks
int my_send_cb(void *ctx, const unsigned char *buf, size_t len);
int my_recv_cb(void *ctx, unsigned char *buf, size_t len);

// Block I/O
int read_exact(mbedtls_ssl_context *ssl, uint8_t *buf, size_t len, int timeout_ms);
int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms);
int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block,
                               const uint8_t *content, size_t content_len);
int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                             const uint8_t *transmission, size_t trans_len);

// Certificate parsing
int parse_cert_chain(const uint8_t *data, int len,
                     int *cert1_off, int *cert1_len,
                     int *cert2_off, int *cert2_len);

// Ciphersuites
extern const int ciphersuites[];

#endif // SMP_NETWORK_H
