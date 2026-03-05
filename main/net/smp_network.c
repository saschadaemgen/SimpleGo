/**
 * SimpleGo - smp_network.c
 * TLS/TCP networking for SMP protocol
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "smp_network.h"
#include "smp_types.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"

static const char *TAG = "SMP_NET";

const int ciphersuites[] = {
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
    0
};

// ============== TCP Helpers ==============

int smp_tcp_connect(const char *host, int port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS failed: %d", err);
        return -1;
    }
    
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket failed");
        freeaddrinfo(res);
        return -1;
    }
    
    // TCP Keep-Alive (matches Haskell client: keepIdle=30, keepIntvl=15, keepCnt=4)
    int keepalive = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    int keepidle = 30;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    int keepintvl = 15;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    int keepcnt = 4;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    ESP_LOGI(TAG, "TCP Keep-Alive set: idle=%d, intvl=%d, cnt=%d", keepidle, keepintvl, keepcnt);

    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Connect failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    ESP_LOGI(TAG, "TCP connected: sock %d to %s:%d", sock, host, port);
    return sock;
}

// ============== mbedTLS I/O ==============

int my_send_cb(void *ctx, const unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = send(sock, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return -1;
    }
    return ret;
}

int my_recv_cb(void *ctx, unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = recv(sock, buf, len, 0);
    if (ret > 0) {
        ESP_LOGI(TAG, "RECV: %d bytes on sock %d", ret, sock);
    }
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
        ESP_LOGE(TAG, "RECV: error errno=%d on sock %d", errno, sock);
        return -1;
    }
    return ret;
}

// ============== SMP Block I/O ==============

static TickType_t get_tick_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

int read_exact(mbedtls_ssl_context *ssl, uint8_t *buf, size_t len, int timeout_ms) {
    size_t received = 0;
    TickType_t start = get_tick_ms();
    
    while (received < len) {
        int ret = mbedtls_ssl_read(ssl, buf + received, len - received);
        
        if (ret > 0) {
            received += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
            ESP_LOGW(TAG, "Connection closed");
            return -1;
        } else {
            ESP_LOGE(TAG, "Read error: -0x%04X", -ret);
            return -1;
        }
        
        if ((get_tick_ms() - start) > (TickType_t)timeout_ms) {
            if (received > 0) {
                ESP_LOGW(TAG, "read_exact: timeout with partial data: %d/%d bytes", (int)received, (int)len);
                return received;
            }
            return -2;  // Timeout (don't log, too frequent)
        }
    }
    return received;
}

int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms) {
    int ret = read_exact(ssl, block, SMP_BLOCK_SIZE, timeout_ms);
    if (ret < 0) return ret;
    
    uint16_t content_len = (block[0] << 8) | block[1];
    if (content_len > SMP_BLOCK_SIZE - 2) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_len);
        return -3;
    }
    return content_len;
}

int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block,
                               const uint8_t *content, size_t content_len) {
    if (content_len > SMP_BLOCK_SIZE - 2) return -1;
    
    memset(block, '#', SMP_BLOCK_SIZE);
    block[0] = (content_len >> 8) & 0xFF;
    block[1] = content_len & 0xFF;
    memcpy(block + 2, content, content_len);
    
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int ret = mbedtls_ssl_write(ssl, block + written, SMP_BLOCK_SIZE - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "Write error: -0x%04X", -ret);
            return ret;
        }
    }
    return 0;
}

int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                             const uint8_t *transmission, size_t trans_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    
    int pos = 0;
    
    // originalLength = 1 (txCount) + 2 (txLen) + trans_len
    uint16_t orig_len = 1 + 2 + trans_len;
    block[pos++] = (orig_len >> 8) & 0xFF;
    block[pos++] = orig_len & 0xFF;
    
    // transmissionCount = 1
    block[pos++] = 1;
    
    // transmissionLength
    block[pos++] = (trans_len >> 8) & 0xFF;
    block[pos++] = trans_len & 0xFF;
    
    // transmission data
    memcpy(&block[pos], transmission, trans_len);
    
    // Hex dump first 16 bytes of outgoing block
    {
        char hex[64] = {0}; int hx = 0;
        for (int j = 0; j < 16; j++)
            hx += sprintf(&hex[hx], "%02x ", block[j]);
        ESP_LOGW("SMP_NET", "BLOCK OUT first 16B: %s (content_len=%d, tx_len=%d)", 
                 hex, (block[0]<<8)|block[1], (int)trans_len);
    }
    
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int want = SMP_BLOCK_SIZE - written;
        int ret = mbedtls_ssl_write(ssl, block + written, want);
        if (ret > 0) {
            ESP_LOGI("BLOCK_TX", "write %d/%d -> returned %d (total %d/%d)",
                     want, SMP_BLOCK_SIZE, ret, written + ret, SMP_BLOCK_SIZE);
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGW("BLOCK_TX", "WANT_WRITE at %d/%d", written, SMP_BLOCK_SIZE);
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "Write error: -0x%04X at %d/%d", -ret, written, SMP_BLOCK_SIZE);
            return ret;
        }
    }
    return 0;
}

// ============== Certificate Parsing ==============

int parse_cert_chain(const uint8_t *data, int len,
                     int *cert1_off, int *cert1_len,
                     int *cert2_off, int *cert2_len) {
    *cert1_off = -1;
    *cert2_off = -1;
    
    // Find first certificate (0x30 0x82 = ASN.1 SEQUENCE)
    for (int i = 0; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert1_off = i;
            *cert1_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    if (*cert1_off < 0) return -1;
    
    // Find second certificate (CA cert for keyHash)
    int search_start = *cert1_off + *cert1_len;
    for (int i = search_start; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert2_off = i;
            *cert2_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    return 0;
}
