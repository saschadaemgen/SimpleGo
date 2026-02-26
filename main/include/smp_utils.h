/**
 * SimpleGo - smp_utils.h
 * URL encoding, Base64 encoding/decoding utilities
 */

#ifndef SMP_UTILS_H
#define SMP_UTILS_H

#include <stdint.h>

// Base64URL encoding (no padding)
int base64url_encode(const uint8_t *input, int input_len, char *output, int output_max);

// Base64URL decoding (handles padding)
int base64url_decode(const char *input, uint8_t *output, int output_max);

// Base64 decoding with padding support (for DH keys from invitations)
int base64_decode_with_padding(const char *input, uint8_t *output, int output_max);

// URL encoding
int url_encode(const char *input, char *output, int output_max);

// URL decoding (in-place, single pass)
void url_decode_inplace(char *str);

// Hex dump for debugging
void dump_hex(const char *label, const uint8_t *data, int len, int max_bytes);

#endif // SMP_UTILS_H
