---
slug: /reference/constants
sidebar_position: 7
title: Constants Reference
---

# Constants Reference

Critical constants that define SimpleGo's behavior. Changing these values incorrectly will break protocol compatibility or cause data loss.

## SMP Transport

| Constant | Value | Description |
|----------|-------|-------------|
| `SMP_BLOCK_SIZE` | 16,384 bytes | Hard SMP transport limit per frame |
| `HISTORY_MAX_PAYLOAD` | 16,000 bytes | Usable payload after framing overhead |
| `HISTORY_MAX_TEXT` | 4,096 bytes | Maximum text stored to SD card |
| `HISTORY_DISPLAY_TEXT` | 512 bytes | Truncation for LVGL bubbles only -- never applied before SD write |

**Critical rule:** `HISTORY_DISPLAY_TEXT` is a UI-only truncation. Text must always be written to SD card at full `HISTORY_MAX_TEXT` length before any display truncation.

## Memory Architecture

| Constant | Value | Description |
|----------|-------|-------------|
| LVGL internal pool | 64 KB | Fixed, separate memory subsystem |
| PSRAM message cache | 30 messages (~135 KB) | Sliding window between SD and LVGL |
| LVGL bubble window | 5 bubbles (~6 KB) | Currently visible messages |
| Ratchet array | 128 contacts (~68 KB) | `ratchet_state_t` in PSRAM |
| Frame pool | 4 frames (64 KB) | Reusable SMP frame buffers in PSRAM |

## Task Stacks

| Task | Size | Memory | Core |
|------|------|--------|------|
| `network_task` | 16 KB | SRAM | 0 |
| `smp_app_task` | 16 KB | SRAM | 1 |
| `lvgl_task` | 8 KB | SRAM | 1 |
| `wifi_manager` | 4 KB | PSRAM | 0 |

## Crypto

| Parameter | Value |
|-----------|-------|
| E2E key exchange | X448 (not X25519) |
| E2E symmetric | AES-256-GCM |
| Per-queue encryption | X25519 + XSalsa20 + Poly1305 (NaCl cryptobox) |
| SD card encryption | AES-256-GCM with HKDF-SHA256 per-contact key |
| TLS version | 1.3, ALPN `smp/1` |
| Content padding | 16 KB fixed blocks at every layer |

## Build

| Parameter | Value |
|-----------|-------|
| Toolchain | ESP-IDF 5.5.2 |
| Normal build | `idf.py build flash monitor -p COM6` |
| Full erase | `idf.py erase-flash` then normal build |
| Hardware AES | Disabled (`CONFIG_MBEDTLS_HARDWARE_AES=n`) -- DMA/memory conflict |
| WPA3 threshold | `WIFI_AUTH_WPA2_PSK` (not WPA3/WPA_WPA2) |

## Further Reference

- [Crypto Primitives](/CRYPTO)
- [Wire Format](/WIRE_FORMAT)
- [Full Technical Reference](/TECHNICAL)
