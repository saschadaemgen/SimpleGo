/**
 * SimpleGo - smp_utils.c
 * URL encoding, Base64 encoding/decoding utilities
 */

#include "smp_utils.h"
#include "smp_types.h"
#include <string.h>
#include <stdio.h>
#include "sodium.h"
#include "esp_log.h"

__attribute__((unused)) static const char *TAG = "SMP_UTIL";

// ============== Base64URL Encoding ==============

int base64url_encode(const uint8_t *input, int input_len, char *output, int output_max) {
    int i, j;
    for (i = 0, j = 0; i < input_len && j < output_max - 4; ) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        output[j++] = base64url_chars[(triple >> 18) & 0x3F];
        output[j++] = base64url_chars[(triple >> 12) & 0x3F];
        output[j++] = base64url_chars[(triple >> 6) & 0x3F];
        output[j++] = base64url_chars[triple & 0x3F];
    }
    
    // Remove padding
    int mod = input_len % 3;
    if (mod == 1) j -= 2;
    else if (mod == 2) j -= 1;
    
    output[j] = '\0';
    return j;
}

// ============== Base64URL Decoding ==============

// FIXED base64url_decode - handles padding and URL-safe chars
int base64url_decode(const char *input, uint8_t *output, int max_output) {
    if (!input || !output || max_output <= 0) return -1;
    
    // Standard Base64 decode table
    static const int8_t decode_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0-15
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 16-31
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63,  // 32-47:  + is 62, - is 62
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  // 48-63:  0-9
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  // 64-79:  A-O
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,  // 80-95:  P-Z, _ is 63
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  // 96-111: a-o
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  // 112-127: p-z
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 128+
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    
    int input_len = strlen(input);
    
    // Skip trailing padding and whitespace
    while (input_len > 0 && (input[input_len-1] == '=' || 
                             input[input_len-1] == ' ' || 
                             input[input_len-1] == '\n')) {
        input_len--;
    }
    
    if (input_len == 0) return 0;
    
    // Calculate output length (3 bytes per 4 input chars)
    int output_len = (input_len * 3) / 4;
    if (output_len > max_output) return -1;
    
    int i = 0, j = 0;
    uint32_t accum = 0;
    int bits = 0;
    
    while (i < input_len) {
        char c = input[i++];
        int8_t val = decode_table[(uint8_t)c];
        
        if (val < 0) {
            // Skip invalid characters (whitespace, etc)
            continue;
        }
        
        accum = (accum << 6) | val;
        bits += 6;
        
        if (bits >= 8) {
            bits -= 8;
            if (j < max_output) {
                output[j++] = (uint8_t)(accum >> bits);
            }
        }
    }
    
    return j;
}

// ============== Base64 Decoding with Padding ==============

int base64_decode_with_padding(const char *input, uint8_t *output, int output_max) {
    // First try URL-safe with padding
    size_t bin_len;
    
    // Make a clean copy without padding for URL-safe decode
    char clean[128];
    strncpy(clean, input, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    
    // Remove trailing '=' padding
    int len = strlen(clean);
    while (len > 0 && clean[len - 1] == '=') {
        clean[--len] = '\0';
    }
    
    // Convert standard Base64 chars to URL-safe
    for (int i = 0; i < len; i++) {
        if (clean[i] == '+') clean[i] = '-';
        if (clean[i] == '/') clean[i] = '_';
    }
    
    if (sodium_base642bin(output, output_max, clean, len,
                          NULL, &bin_len, NULL, sodium_base64_VARIANT_URLSAFE_NO_PADDING) == 0) {
        return (int)bin_len;
    }
    
    return -1;
}

// ============== URL Encoding ==============

int url_encode(const char *input, char *output, int output_max) {
    static const char *hex = "0123456789ABCDEF";
    int j = 0;
    for (int i = 0; input[i] && j < output_max - 4; i++) {
        unsigned char c = (unsigned char)input[i];
        // Only keep alphanumeric and - _ . ~ unreserved
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            output[j++] = c;
        } else {
            // Percent-encode everything else
            output[j++] = '%';
            output[j++] = hex[(c >> 4) & 0x0F];
            output[j++] = hex[c & 0x0F];
        }
    }
    output[j] = '\0';
    return j;
}

// ============== URL Decoding ==============

void url_decode_inplace(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int hi = src[1], lo = src[2];
            hi = (hi >= 'A') ? (hi & 0xDF) - 'A' + 10 : hi - '0';
            lo = (lo >= 'A') ? (lo & 0xDF) - 'A' + 10 : lo - '0';
            *dst++ = (hi << 4) | lo;
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// ============== Debug Helpers ==============

void dump_hex(const char *label, const uint8_t *data, int len, int max_bytes) {
    int show = (len > max_bytes) ? max_bytes : len;
    printf("   %s (%d bytes): ", label, len);
    for (int i = 0; i < show; i++) {
        printf("%02x ", data[i]);
    }
    if (len > max_bytes) printf("...");
    printf("\n");
}