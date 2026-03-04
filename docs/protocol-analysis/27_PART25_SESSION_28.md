![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 25: Session 28
# FreeRTOS Task Architecture Phase 2b

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-15
**Status:** COMPLETED -- Three FreeRTOS tasks running with PSRAM
**Previous:** Part 24 - Session 27
**Next:** Part 26 - Session 29
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 28 SUMMARY

```
Session 28 implemented the FreeRTOS multi-task architecture after
reverting the broken Phase 2 commit from Session 27. Key fix: all
non-DMA resources moved to PSRAM, preserving ~40KB internal SRAM for
mbedTLS and WiFi. Three tasks running: Network (Core 0, 12KB),
App (Core 1, 16KB), UI (Core 1, 8KB). Frame pool (4x4KB) and ring
buffers (3KB) allocated in PSRAM. Tasks start AFTER successful
connection (init stays sequential). erase-flash after branch switch
established as mandatory first step for debugging.

 main branch repaired (git revert of Phase 2)
 Phase 2b: Three FreeRTOS tasks running in parallel
 PSRAM solution: frame pool + ring buffers + task stacks
 Internal heap preserved: ~40KB for mbedTLS/WiFi
 Duration: ~4 hours (vs 2 days for failed Session 27)
```

---

## Main Branch Repair

Phase 2 commit (251fa1b) reverted cleanly: `git revert 251fa1b`. Phase 1 folder structure preserved.

After revert, connection stayed stuck on "connecting". Root cause was not code but stale NVS: old cryptographic state from branch switches no longer matched. Solution: `idf.py erase-flash` then create new contact.

**Lesson 138:** After EVERY branch switch, sdkconfig change, or "suddenly doesn't work," `idf.py erase-flash` is the FIRST step.

---

## Task Architecture

```
Network Task  Core 0  12KB stack  Priority 7
App Task      Core 1  16KB stack  Priority 6
UI Task       Core 1   8KB stack  Priority 5

Ring Buffers: Network->App 2KB, App->Network 1KB
Frame Pool: 4 frames x 4KB = 16KB
```

Tasks start AFTER successful connection:

```c
smp_connect();          // full memory available
smp_tasks_init();       // allocate resources
smp_tasks_start(&ssl);  // tasks take over
// receive loop stays unchanged
```

---

## PSRAM Solution

Tasks + frame pool + ring buffers in internal SRAM pushed free heap from 40KB to 19KB, breaking mbedtls_ssl_read(). Moving everything non-DMA to PSRAM restored the heap:

| Component | Size | Allocation |
|-----------|------|------------|
| Frame Pool | 16KB | heap_caps_calloc(MALLOC_CAP_SPIRAM) |
| Ring Buffers | 3KB | xRingbufferCreateWithCaps(MALLOC_CAP_SPIRAM) |
| Task Stacks | 36KB | Already PSRAM |

Result: internal heap ~38-40KB preserved, receive works with tasks running.

---

## ESP32-S3 Memory Architecture

```
Internal SRAM (~200KB total, ~40KB free after WiFi+TLS)
  mbedTLS (DMA-bound), WiFi/TCP buffers (DMA), FreeRTOS kernel

PSRAM (8MB external via SPI)
  Frame pools, task stacks, ring buffers, LVGL buffers

NVS Flash (128KB, persistent)
  Ratchet states, queue credentials, contact data

eFuse (one-time programmable)
  Flash Encryption Key (AES-256), Secure Boot Key (RSA-3072)

SD Card (up to 128GB, external)
  Chat history (AES-256-GCM), XFTP attachments
```

Rule: on ESP32-S3 with TLS+WiFi+Display, everything that does not strictly need internal SRAM must go to PSRAM. Internal SRAM is reserved for DMA-dependent operations.

---

## Implementation Details

Frame pool: 4 static blocks of 4096 bytes in PSRAM, sodium_memzero() at init and free, portMUX_TYPE spinlock for thread safety, pointer validation at free.

Task features: empty task loops (logging + heap monitoring only, receive loop stays in main for now), rollback on failed task creation, heap logging at 5 locations.

sdkconfig values confirmed surviving the revert:

```ini
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=49152
CONFIG_MBEDTLS_HARDWARE_SHA=y
```

---

## New Files

| File | Location | Purpose |
|------|----------|---------|
| smp_events.h | main/include/ | Event types for inter-task communication |
| smp_frame_pool.h | main/include/ | Frame pool interface |
| smp_tasks.h | main/include/ | Task management interface |
| smp_frame_pool.c | main/core/ | Frame pool with PSRAM + sodium_memzero |
| smp_tasks.c | main/core/ | Three tasks with PSRAM stacks and ring buffers |

---

## Working Feature Set (End of Session 28)

WiFi + TLS 1.3 to SMP server, complete SMP handshake, queue creation and subscription, bidirectional message encrypt/decrypt (Double Ratchet), send messages via keyboard, delivery receipts, ratchet state persistence (survives reboot), invitation links, and now three FreeRTOS tasks running in parallel with PSRAM allocation.

---

*Part 25 - Session 28: FreeRTOS Phase 2b*
*SimpleGo Protocol Analysis*
*Original date: February 15, 2026*
*Rewritten: March 4, 2026 (v2)*
*Init stays sequential. Tasks start AFTER connection.*
