![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 24: Session 27
# FreeRTOS Task Architecture Investigation

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-14/15
**Status:** COMPLETED -- Architecture validated, implementation needs restart
**Previous:** Part 23 - Session 26
**Next:** Part 25 - Session 28
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 27 SUMMARY

```
Session 27 attempted the transition from monolithic blocking
architecture to multi-task FreeRTOS. Phase 1 (folder restructure
into 6 subdirectories) succeeded. Phase 2 (task architecture) broke
main by reserving ~90KB RAM at boot, starving TLS/WiFi of memory.
Phase 3 (network task migration) was attempted on a branch that
became polluted by 2 days of debugging experiments. Root cause was
found only after baseline-testing main (should have been step one).
Architecture design is correct; implementation timing was wrong:
tasks must start AFTER connection, not at boot. sdkconfig fixes
discovered: OUT_CONTENT_LEN=16384 and TCP_SND_BUF=32768 minimum.

 Phase 1: Folder restructure (17 .c files into 6 subdirectories)
 Phase 2: BROKEN (90KB RAM at boot starved TLS/WiFi)
 Phase 3: Branch polluted, discarded
 sdkconfig: OUT_CONTENT_LEN=16384, TCP_SND_BUF=32768
 16KB TLS write deadlock solved
 17 lessons learned
```

---

## Folder Restructure (Phase 1)

17 source files reorganized into subdirectories: core/ (task management, events, frame pool), protocol/ (SMP, agent, handshake), crypto/ (ratchet, E2E), net/ (network, TLS, queues), state/ (storage, contacts, peer), util/ (logging, helpers). CMakeLists.txt changed from explicit file list to `SRC_DIRS`. Build + Flash + Receive test passed.

---

## Task Architecture Failure (Phase 2)

Architecture design (valid):

```
Network Task (Core 0, Priority 7) -- TLS read/write
App Task (Core 1, Priority 6)     -- Crypto, protocol
UI Task (Core 1, Priority 5)      -- LVGL, keyboard

IPC: Ring Buffers (No-Split) for frames, FreeRTOS Queues for events
Memory: 8x4KB static frame pool (32KB)
```

This commit reserved ~90KB RAM at boot (16KB + 32KB + 10KB stacks, 32KB frame pool, 12KB ring buffers), executed BEFORE `smp_connect()`, starving TLS/WiFi.

After 2 days debugging on the phase3 branch, a baseline test of main revealed main was also broken. Checkout of Session 26 commit worked. Phase 2 commit identified as the cause. Git bisect would have found this in minutes.

---

## 16KB TLS Write Deadlock

`mbedtls_ssl_write()` wrote only 4096 of 16384 bytes, then deadlocked.

Root cause: ESP-IDF defaults `CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096` (one TLS record max) and `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760` (exactly one record fits).

Fix:

```ini
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768
```

---

## Command-Write Hang

After handshake write succeeded, command writes (NEW, SUB) hung. 15 debugging attempts over 2 days: socket timeouts, non-blocking recv, MSG_DONTWAIT (ignored by ESP-IDF), select(), pre-drain, various TCP buffer sizes. Attempt 13 tested main baseline and found it also broken, leading to the Phase 2 root cause discovery.

Rejected hypotheses: missing read after ClientHello, TLS 1.3 NewSessionTicket blocks writes, core pinning, PSRAM, Phase 3 regression, firewall, server-side change, SO_RCVTIMEO fix, non-blocking recv during ssl_write.

---

## Confirmed sdkconfig Requirements

```
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384   mandatory for 16KB SMP blocks
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768      minimum for TLS records > 4096
```

Additional findings: Session Tickets should stay at default (enabled, Haskell discards via noSessionManager). TLS 1.3 post-handshake records cause mbedTLS WANT_READ, which is normal. Socket timeouts (`SO_SNDTIMEO`, `SO_RCVTIMEO`) interact unpredictably with mbedTLS internals. Bigger TCP buffer does not mean better: larger buffers enable async behavior that can break request-response protocols.

---

## Correct Architecture (for Session 28)

```c
// WRONG (Session 27): reserves 90KB before connection
app_main() {
    smp_tasks_init();     // 90KB gone
    smp_tasks_start();
    smp_connect();        // not enough memory
}

// CORRECT: tasks start after connection
app_main() {
    smp_connect();        // full memory available
    smp_tasks_init();     // now safe to allocate
    smp_tasks_start();    // tasks take over
}
```

Frame pools and ring buffers should be allocated on demand, not at boot. Init stays sequential (proven since Session 7), tasks take over running operation.

---

## Security Features (Parked)

ESP32-S3 security activation order: Flash Encryption (XTS-AES) first, then NVS Encryption, then Secure Boot v2 (RSA-3072), then JTAG Disable. Development Mode allows 3x reflash; Release is OTA-only. Requires OTA partitions (ota_0, ota_1, nvs_keys). Parked for production phase.

---

*Part 24 - Session 27: Architecture Investigation*
*SimpleGo Protocol Analysis*
*Original dates: February 14-15, 2026*
*Rewritten: March 4, 2026 (v2)*
*Tasks AFTER connection, never at boot*
