---
title: "SimpleGo Architecture & Security"
sidebar_position: 1
---

# SimpleGo Architecture & Security

**Document version:** Session 43 | March 2026  
**Hardware:** LilyGo T-Deck Plus  
**Copyright:** © 2025-2026 Sascha Dämgen, IT and More Systems, Recklinghausen  
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Overview

| Property | Details |
|----------|---------|
| Protocol | SimpleX Messaging Protocol (SMP) v7, first native C implementation worldwide |
| Hardware | LilyGo T-Deck Plus: ESP32-S3 Dual-Core 240 MHz, 8 MB PSRAM, 320×240 ST7789V, BB Q20 QWERTY |
| Firmware stack | ESP-IDF 5.5.2 / FreeRTOS / LVGL v9 / mbedTLS / libsodium |
| Encryption | 4 independent layers per message: Double Ratchet (X448) + 2× NaCl cryptobox + TLS 1.3 |
| Test status | Verified against official SimpleX Chat App, 7 parallel contacts stable |
| Codebase | 47 source files, 21,863 lines of C |
| License | AGPL-3.0 (software), CERN-OHL-W-2.0 (hardware) |

SimpleGo is not an app running on a smartphone OS. It is a complete autonomous firmware running as multiple parallel FreeRTOS tasks on two CPU cores of the ESP32-S3. No Android, no Linux, no baseband processor.

---

## 1. FreeRTOS Task Architecture

Four tasks run concurrently across the two cores. The allocation is deliberate: network I/O is isolated on Core 0 so a hanging TLS handshake never blocks the UI. App logic requiring NVS writes stays on a Main Task with internal SRAM stack (a hard ESP32-S3 hardware constraint).

| Task | Core | Stack | Responsibility |
|------|------|-------|---------------|
| `network_task` | Core 0 | 16 KB SRAM | All SSL/TLS connections. Reads SMP frames from server, writes commands. Isolated so a hanging TLS handshake never blocks the UI core. |
| `smp_app_task` | Core 1 | 16 KB SRAM | SMP protocol state machine, ratchet encryption, NVS persistence, contact management. Must run in internal SRAM: tasks with PSRAM stacks cannot write NVS on ESP32-S3 (hardware constraint: PSRAM cache is disabled during flash writes). |
| `lvgl_task` | Core 1 | 8 KB SRAM | Dedicated LVGL rendering task. Calls `lv_timer_handler()` in its own loop. Shares SPI2 bus with SD card via recursive mutex `tdeck_lvgl_lock`. |
| `wifi_manager` | Core 0 | 4 KB PSRAM | WiFi connection management, multi-network storage, reconnects, WPA3 compatibility. |

### Inter-Task Communication

| Mechanism | Direction | Description |
|-----------|-----------|-------------|
| `rx_ring_buffer` | Network → App | Received SMP frames (up to 16 KB each). Capacity: 4 frames = 64 KB in PSRAM. |
| `tx_ring_buffer` | App → Network | Encrypted commands, sent via active TLS connection. |
| `ui_event_queue` | UI → App | LVGL events as FreeRTOS queue. `smp_app_task` processes sequentially. |
| `frame_pool` | global | Reusable frame pool in PSRAM. `sodium_memzero()` on release. |
| `tdeck_lvgl_lock` | LVGL ↔ SD | Recursive mutex for SPI2 bus. AES-GCM runs outside this mutex. Otherwise 500 ms hold and display freeze. |

---

## 2. Memory Architecture

The ESP32-S3 has four physically separate memory regions. Understanding this separation is central to both performance and security: PSRAM has no DMA support, which forces mbedTLS and LVGL draw buffers into internal SRAM.

| Region | Size | Utilization | Contents |
|--------|------|-------------|---------|
| Internal SRAM | 512 KB | ~35% | TLS stack, LVGL draw buffers (25.6 KB, DMA required), task stacks. mbedTLS cannot use PSRAM (no DMA). |
| PSRAM | 8 MB | ~2% | 128-contact ratchet array (~68 KB), frame pool (64 KB), ring buffers, WiFi manager stack. **Security gap: `s_msg_cache` holds up to 30 decrypted messages in plaintext, never zeroed (SEC-01, open).** |
| NVS Flash | 128 KB | ~50% | Ratchet keys, queue keys, handshake keys. **Security gap: `nvs_flash_init()` without encryption. All private keys readable in plaintext from flash (SEC-02, deferred to Kickstarter phase).** |
| SD card | up to 128 GB | < 1% | Correct: AES-256-GCM encrypted. HKDF-SHA512 per-contact key. Plaintext never leaves this layer without an explicit decrypt call. |
| LVGL pool | 64 KB (internal) | ~31% | Separate memory subsystem, does not appear in heap reports. LVGL labels contain message text. Stable at 31% after Session 41f screen lifecycle fix. Memory wipe on standby missing (SEC-04, open). |

### PSRAM vs Internal SRAM: Decision Rules

The following operations **must** stay in internal SRAM:
- mbedTLS TLS stack (no DMA in PSRAM)
- LVGL draw buffers (SPI-DMA requirement)
- Task stacks for any task that writes NVS

The following can use PSRAM:
- Ratchet state array (128 contacts × ~530 bytes = ~68 KB)
- Frame pool and ring buffers
- WiFi manager stack (no NVS writes)

---

## 3. Four Encryption Layers

Every message passes through four cryptographically independent layers. Each layer protects against a distinct attacker model. They are not sequential. They are nested envelopes.

| # | Layer | Algorithm | Protects Against |
|---|-------|-----------|-----------------|
| 1 | Double Ratchet (E2E) | X3DH (X448) + Double Ratchet + AES-256-GCM | End-to-end interception. Perfect Forward Secrecy + Post-Compromise Security. Every message has its own key. |
| 2 | Per-Queue NaCl cryptobox | X25519 + XSalsa20 + Poly1305 | Traffic correlation between queues. Knowledge of Queue A gives zero information about Queue B. |
| 3 | Server-to-Recipient NaCl | NaCl cryptobox (X25519 + XSalsa20 + Poly1305) | Correlation of incoming and outgoing server frames. Protects against full server access. |
| 4 | TLS 1.3 transport | TLS 1.3, ALPN `smp/1`, mbedTLS | Network attacker. No downgrade. Transport layer only, not a message envelope. |

**Content padding:** All messages are padded to fixed 16 KB blocks at every layer. A network attacker sees only equal-sized packets. Actual message length is not observable.

### Reverse-Engineered Crypto Details

Two non-obvious protocol details that took significant effort to discover and are documented here for implementers:

**Non-standard XSalsa20:** SimpleX uses `HSalsa20(key, zeros[16])` instead of the standard `HSalsa20(key, nonce[0:16])`. Without this exact knowledge, decryption fails silently with no error message.

**X448 byte order:** Haskell's `cryptonite` library outputs X448 keys in reversed byte order. SimpleGo explicitly reverses in `smp_x448.c` for protocol compatibility. This was verified byte-by-byte against the Haskell reference implementation.

### Crypto Library Assignment

| Purpose | Algorithm | Library |
|---------|-----------|---------|
| X3DH Key Agreement | X448 | wolfSSL (wolfcrypt/curve448) |
| Per-Queue NaCl (Layer 2) | X25519 + XSalsa20 + Poly1305 | libsodium |
| Server-to-Recipient NaCl (Layer 3) | X25519 + XSalsa20 + Poly1305 | libsodium |
| Double Ratchet Symmetric | AES-256-GCM | mbedTLS |
| Key Derivation (Ratchet) | HKDF-SHA512 | mbedTLS |
| Message Hashing | SHA-256 | mbedTLS |
| SD Card Encryption | AES-256-GCM | mbedTLS |
| TLS 1.3 Transport | TLS 1.3 | mbedTLS |
| Secure Memory Wipe | sodium_memzero / mbedtls_platform_zeroize | libsodium / mbedTLS |

---

### SMP Connection Lifecycle

| Phase | What happens |
|-------|-------------|
| 1. TLS Connect | TCP connect, TLS 1.3 handshake, ALPN `smp/1`. mbedTLS verifies server certificate via key hash pinning. |
| 2. SMP Handshake | Server sends version range and public key. Client responds with own key and auth. Version negotiation settles on SMP v7. |
| 3. Subscribe | For each contact: SUB command. `NEW` already creates the queue subscribed. SUB is a noop but re-delivers the last unACKed message. `subscribe_all_contacts()` iterates all 128 slots. |
| 4. Message Loop | Decrypt MSG frames (all 3 message layers), parse JSON, write SD history, update UI, send ACK. |
| 5. Keep-Alive | **Missing: PING command not yet implemented.** Without keep-alive, server drops subscription after timeout. Scheduled for next session. |

### Lost Response Handling

This was identified by Evgeny Poberezkin (SimpleX inventor) as the most critical networking correctness requirement:

> "Whatever you do for networking, make sure to handle lost responses, that was the biggest learning. For us it was a large source of bugs."

The problem: a command is sent, no response arrives. Did the server not receive it, or was the response lost in transit? The device cannot distinguish these cases at the network level.

The solution: all state-changing commands are idempotent. Keys are persisted to NVS flash **before** the command is sent. If the response is lost, the same stored key is reused on retry. No new key is generated, no ratchet desynchronization.

```
Generate key → Persist to NVS → Send command → [response lost] → Retry with SAME key
```

This pattern is implemented correctly in `smp_queue.c` for SKEY and LKEY operations.

### 16 KB Block Framing

Every SMP frame is exactly 16,384 bytes. The first 2 bytes are the content length (big-endian). The remaining bytes are content followed by zero padding. The receive buffer is allocated at exactly `SMP_BLOCK_SIZE`. No dynamic sizing, no chunking, no XFTP fallback for text messages.

| Constant | Value | Meaning |
|----------|-------|---------|
| `SMP_BLOCK_SIZE` | 16,384 bytes | Hard transport limit |
| `HISTORY_MAX_PAYLOAD` | 16,000 bytes | Usable payload after framing overhead |
| `HISTORY_MAX_TEXT` | 4,096 bytes | Maximum text stored to SD card |
| `HISTORY_DISPLAY_TEXT` | 512 bytes | Truncation limit for LVGL bubbles only, never applied before SD write |

---

## 5. UI Architecture

### Screen Lifecycle

All screens except Main are ephemeral, created on entry, destroyed on exit. LVGL pool consumption dropped from 86% to 31% after the Session 41f lifecycle fix.

| Screen | Type | Details |
|--------|------|---------|
| Main Screen | Permanent | Always in memory. Never destroyed. |
| Contacts Screen | Ephemeral | Created on entry, destroyed on exit. |
| Chat Screen | Ephemeral | `ui_chat_cleanup()` must be called before destroy. Nulls all 6 static LVGL pointers, prevents dangling pointer crashes. Does **not** zero the PSRAM cache (SEC-01). |
| Settings Screen | Ephemeral | Settings written directly to NVS on change. |

### Chat Sliding Window

Three-tier architecture to avoid SD card reads on every scroll operation:

| Tier | Capacity | Security Status |
|------|----------|----------------|
| SD card | Unlimited | Correct: AES-256-GCM encrypted. Plaintext only leaves via explicit decrypt call. |
| PSRAM cache | 30 messages (~120 KB) | **Security gap: plaintext in PSRAM, never zeroed (SEC-01). `sodium_memzero()` missing on `s_msg_cache`.** |
| LVGL window | 5 bubbles (~6 KB) | LVGL labels contain message text. No explicit wipe on bubble eviction. Screen destroy is not sufficient. |

---

## 6. File-by-File Analysis

### core/

| File | Function | Status |
|------|----------|--------|
| `core/smp_tasks.c` | Task definitions, `smp_app_run()` | Clean after Session 42 refactor: 530 → 118 lines, 5 static helpers extracted. |
| `core/smp_frame_pool.c` | Reusable 16 KB frame pool in PSRAM | `sodium_memzero()` on release. Correct. |

### crypto/

| File | Function | Status |
|------|----------|--------|
| `crypto/simplex_crypto.c` | Core NaCl crypto: `simplex_secretbox_open/seal` (layers 2+3), non-standard XSalsa20 | libsodium. Correct. |
| `crypto/smp_crypto.c` | NaCl crypto_box wrappers: Layer 3 decrypt, peer encrypt/decrypt | libsodium only. Correct. |
| `crypto/smp_x448.c` | X448 DH for X3DH | wolfSSL. Byte-order reversal correct. No DH secret in logs. |

### net/

| File | Function | Status |
|------|----------|--------|
| `net/smp_network.c` | Raw TLS send/receive, mbedTLS context | mbedTLS. Core transport layer. |
| `net/wifi_manager.c` | Multi-network WiFi, WPA3 | Credentials persisted before `esp_wifi_connect()` correctly. |

### protocol/

| File | Function | Status |
|------|----------|--------|
| `protocol/smp_handshake.c` | TLS connect, SMP handshake, version negotiation | Clean after Session 42: 74 debug statements removed, 1281 → 1207 lines. |
| `protocol/smp_agent.c` | Protocol state machine, frame dispatch | Clean. |
| `protocol/smp_queue.c` | Queue operations: NEW, SUB, SEND | Very clean. Write-before-send correctly implemented. |
| `protocol/smp_ack.c` | ACK command for all queue types | libsodium + mbedTLS via smp_network. Clean. |
| `protocol/smp_parser.c` | Agent protocol message parser | Parses all incoming server frames. Uses smp_crypto (libsodium). Clean. |
| `protocol/smp_e2e.c` | E2E decrypt flow | Clean: direct call, not brute-force methods 0-3. |
| `protocol/smp_ratchet.c` | Double Ratchet (X448/wolfSSL + AES-256-GCM/mbedTLS + HKDF-SHA512/mbedTLS) | Cleanest file in codebase. Zero printf. Re-delivery error -10 is normal behavior, not an error. |
| `protocol/reply_queue.c` | Reply queues for 128 contacts | PSRAM guard correct. NVS-deferred pattern correct. |

### state/

| File | Function | Status |
|------|----------|--------|
| `state/smp_contacts.c` | 128-contact management in PSRAM | Correct. `subscribe_all_contacts()` stable. |
| `state/smp_peer.c` | Peer connection management | Clean. BUG #19 comment documentation excellent. |
| `state/smp_history.c` | SD card history (AES-256-GCM + HKDF-SHA512) | mbedTLS. AES-GCM runs outside SPI2 mutex (correct). HKDF info parameter weak (SEC-05, deferred). |
| `state/smp_storage.c` | NVS abstraction | `mbedtls_platform_zeroize()` correctly used after Session 42 fix (SEC-03 closed). |

### ui/

| File | Function | Status |
|------|----------|--------|
| `ui/ui_manager.c` | Screen navigation and lifecycle | Correct after Session 41f. Ephemeral screens correctly destroyed. |
| `ui/ui_theme.c` | Colors, fonts, spacing | Clean. |
| `ui/screens/ui_main.c` | Main screen (permanent, never destroyed) | Stable. |
| `ui/screens/ui_chat.c` | Chat screen, PSRAM cache management | `s_msg_cache` never zeroed (SEC-01 open). `sodium_memzero()` completely missing. |
| `ui/screens/ui_chat_bubble.c` | Message bubble widget | Clean. |
| `ui/screens/ui_contacts.c` | Contact list screen | Ephemeral. Clean. |
| `ui/screens/ui_contacts_row.c` | Contact list row widget | Clean. |
| `ui/screens/ui_contacts_popup.c` | Contact action popup | Clean. |
| `ui/screens/ui_connect.c` | Connection setup screen | Ephemeral. Clean. |
| `ui/screens/ui_settings.c` | Settings screen (root) | Ephemeral. NVS write on change. |
| `ui/screens/ui_settings_wifi.c` | WiFi settings sub-screen | Clean. |
| `ui/screens/ui_settings_bright.c` | Backlight settings sub-screen | Clean. |
| `ui/screens/ui_settings_info.c` | Device info sub-screen | Clean. |
| `ui/screens/ui_splash.c` | Splash screen on boot | Clean. |
| `ui/screens/ui_developer.c` | Developer debug screen | Should be stripped or gated for production builds. |
| `ui/fonts/simplego_umlauts_10.c` | Custom font glyphs 10px | Static data. |
| `ui/fonts/simplego_umlauts_12.c` | Custom font glyphs 12px | Static data. |
| `ui/fonts/simplego_umlauts_14.c` | Custom font glyphs 14px | Static data. |

### util/ and root

| File | Function | Status |
|------|----------|--------|
| `util/smp_utils.c` | Shared utilities (hex dump, string helpers) | Clean. |
| `main.c` | App entry point, task launch | Clean. |

### devices/t_deck_plus/hal_impl/

| File | Function | Status |
|------|----------|--------|
| `tdeck_lvgl.c` | LVGL init, rendering task Core 1 | Draw buffers correctly in internal SRAM (SPI-DMA requirement). Recursive mutex correct. |
| `tdeck_display.c` | ST7789V SPI display driver | Clean. |
| `tdeck_keyboard.c` | BB Q20 I2C keyboard at 0x55 | Clean. |
| `tdeck_touch.c` | Capacitive touch driver | Clean. |
| `tdeck_backlight.c` | GPIO 42 backlight, pulse-counting protocol, 16 levels | Clean. |

---

## 7. Security Status

### Known Vulnerabilities: Complete List

No finding is downplayed or marked acceptable. This is an honest inventory of what is missing.

| ID | Severity | Description | Status |
|----|----------|-------------|--------|
| SEC-01 | **CRITICAL** | Decrypted messages in PSRAM (`s_msg_cache`, 30 messages, ~120 KB, never zeroed). Physical attacker with JTAG can read all messages in plaintext while device is powered. | **OPEN** |
| SEC-02 | **CRITICAL** | Cryptographic keys plaintext in NVS flash (`nvs_flash_init()` without encryption). Physical attacker who reads the flash chip via SPI tap or desolder gets all private keys. | **OPEN, deferred to Kickstarter phase** |
| SEC-03 | **HIGH** | `memset` instead of `mbedtls_platform_zeroize` in `smp_storage.c` (CWE-14, compiler may eliminate as dead store). | **CLOSED, Session 42** |
| SEC-04 | **HIGH** | No memory wipe on display timeout / screen lock. Device appears locked but holds plaintext in PSRAM and LVGL pool. | **OPEN** |
| SEC-05 | **MEDIUM** | HKDF info parameter too weak (only slot index, no device binding). Same master key on another device with same slot index produces identical derived keys. | **DEFERRED, resolved automatically when eFuse binding implemented** |
| SEC-06 | **MEDIUM** | No post-quantum protection active (`has_kem=false`, Kyber disabled). Key exchange not quantum-resistant. AES-256-GCM for SD card is already quantum-resistant (256-bit survives Grover). | **DEFERRED, requires SEC-01 and SEC-02 first** |

### What Is Correctly Implemented

- Write-before-send in `smp_queue.c`: NVS persist before network send for SKEY/LKEY. Idempotency correct.
- Credentials persistence in `wifi_manager.c`: WiFi credentials saved before `esp_wifi_connect()`.
- PSRAM guard in `reply_queue.c`: prevents NVS overwrite of valid PSRAM state.
- AES-GCM outside SPI2 mutex: mutex hold reduced from ~500 ms to under 10 ms.
- DMA draw buffers in internal SRAM: correct, not in PSRAM (SPI-DMA requirement).
- `sodium_memzero()` in `frame_pool`: frame release securely zeros sensitive data.
- X448 byte-order in `smp_x448.c`: correctly documented and verified against Haskell.
- Non-standard XSalsa20 in `simplex_crypto.c`: correctly reverse-engineered and verified.
- Screen lifecycle after Session 41f: LVGL pool stable at 31%.
- SD card history: AES-256-GCM correct. Plaintext never leaves SD without explicit decrypt.
- `mbedtls_platform_zeroize` in `smp_storage.c`: CWE-14 closed in Session 42.

---

## 8. Roadmap

### Immediate: Next Session

| Task | Priority | Rationale |
|------|----------|-----------|
| SEC-01: `sodium_memzero()` on `s_msg_cache` in `ui_chat_cleanup()`, on contact switch, and on display timeout | Critical | Physical attacker can read all messages in RAM |
| SEC-04: Screen lock with full memory wipe (PSRAM cache + LVGL labels) | High | Depends on SEC-01 |
| PING/PONG keep-alive on main SSL connection | High | Without it, server drops subscription after timeout |
| 5 security log categories cleanup (Aschenputtel Findings 1-5) | High | Production-sensitive data in serial output |

### Kickstarter Phase

| Task | Dependency |
|------|-----------|
| SEC-02: `nvs_flash_secure_init()` + eFuse key binding, all private keys encrypted at rest | eFuse provisioning |
| SEC-05: Strengthen HKDF info to `simplego-slot-XX-DEVICESERIAL` | Resolved by SEC-02 |
| SEC-06: Activate CRYSTALS-Kyber + NTRU Prime (`has_kem=false` → `true`) | SEC-01 + SEC-02 must be closed first |
| Secure boot: firmware signature bound to ESP32-S3 eFuse | eFuse provisioning |
| Private Message Routing | Independent |

### Hardware Roadmap

- **ESP32-P4 board (ordered):** Guition JC4880P443C_I_W: 400 MHz RISC-V dual-core, 32 MB PSRAM, WiFi 6, hardware security features.
- **PCB design:** Model 3 Vault (full specification). Model 2 as downgrade variant via DNP (Do Not Populate).
- **Triple-vendor Secure Elements:** ATECC608B (Microchip) + OPTIGA Trust M (Infineon) + SE050 (NXP). Three vendors: if one chip has a backdoor, the full key cannot be reconstructed.
- **Three physical kill switches:** WiFi/BLE, LoRa, LTE.
- **M.2 slot** for optional LTE modules (avoids LTE certification costs per device).

---

*SimpleGo, IT and More Systems, Recklinghausen*  
*First native C implementation of the SimpleX Messaging Protocol*
