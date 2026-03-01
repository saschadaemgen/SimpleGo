---
title: "Implementation Guide"
sidebar_position: 1
---

# SimpleGo Implementation Guide

This guide covers the practical aspects of implementing the SimpleX protocol in C on ESP32 hardware. It follows a bottom-up approach matching the protocol stack.

## Implementation Order

The recommended implementation order follows the protocol layers:

1. **Transport** — TLS 1.3 connection to SMP server
2. **SMP Client** — Command encoding, queue management
3. **Server Encryption** — NaCl crypto_box decryption
4. **E2E Encryption** — Sender authentication layer
5. **Double Ratchet** — Per-message key derivation
6. **Agent** — Connection management, integrity
7. **Chat** — Application message handling

## Technology Stack

| Component | Library | Notes |
|-----------|---------|-------|
| TLS 1.3 | mbedTLS (ESP-IDF built-in) | Hardware-accelerated on ESP32 |
| NaCl crypto | libsodium | XSalsa20-Poly1305, Ed25519 |
| X3DH / DH | libsodium + custom | Curve448 via custom implementation |
| Double Ratchet | Custom C | Based on Signal spec |
| JSON parsing | cJSON | Lightweight, ESP-IDF compatible |
| Compression | Zstandard (zstd) | Message decompression |

## Prerequisites

- ESP-IDF v5.5+ (toolchain and build system)
- LilyGo T-Deck or compatible ESP32-S3 board
- Basic understanding of C and embedded development
- Familiarity with cryptographic concepts
