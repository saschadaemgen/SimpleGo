---
slug: /architecture
sidebar_position: 3
title: Architecture Overview
---

# Architecture Overview

SimpleGo runs as multiple parallel FreeRTOS tasks on two CPU cores of the ESP32-S3. No Android, no Linux, no baseband processor -- complete autonomous firmware.

## Task Architecture

Four tasks run concurrently across two cores:

| Task | Core | Stack | Responsibility |
|------|------|-------|---------------|
| `network_task` | Core 0 | 16 KB SRAM | All SSL/TLS connections, SMP frame I/O |
| `smp_app_task` | Core 1 | 16 KB SRAM | Protocol state machine, ratchet encryption, NVS persistence |
| `lvgl_task` | Core 1 | 8 KB SRAM | LVGL rendering, SPI2 bus sharing with SD card |
| `wifi_manager` | Core 0 | 4 KB PSRAM | WiFi connection management, multi-network, WPA3 |

Network I/O is isolated on Core 0 so a hanging TLS handshake never blocks the UI. App logic requiring NVS writes runs on a Main Task with internal SRAM stack -- a hard ESP32-S3 hardware constraint (PSRAM-stack tasks cannot write NVS due to cache conflicts).

## Memory Architecture

| Region | Size | Contents |
|--------|------|---------|
| Internal SRAM | 512 KB | TLS stack, LVGL draw buffers (DMA required), task stacks |
| PSRAM | 8 MB | Ratchet array (128 contacts), frame pool, ring buffers |
| NVS Flash | 128 KB | Ratchet keys, queue keys, handshake keys |
| SD Card | up to 128 GB | AES-256-GCM encrypted chat history |
| LVGL Pool | 64 KB | Separate subsystem, message text in labels |

## Four Encryption Layers

Every message passes through four cryptographically independent layers:

1. **Double Ratchet (E2E):** X3DH (X448) + AES-256-GCM -- perfect forward secrecy, every message has its own key
2. **Per-Queue NaCl:** X25519 + XSalsa20 + Poly1305 -- prevents traffic correlation between queues
3. **Server-to-Recipient NaCl:** additional cryptobox preventing correlation of server I/O
4. **TLS 1.3:** transport layer, ALPN `smp/1`, mbedTLS

All messages padded to fixed 16 KB blocks at every layer.

## Deep Dive

For the complete architecture document including inter-task communication, sliding window design, and file-by-file analysis, see the [full Architecture reference](/ARCHITECTURE).
