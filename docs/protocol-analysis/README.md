![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## LATEST: Documentation + Security Cleanup + Display Name (2026-03-08 Session 43)

```
Wiki live at wiki.simplego.dev (Docusaurus 3, search, mobile-ready)
Security: all cryptographic material removed from serial output
Display name: NVS-backed user identity, first-boot prompt
Bug #20 discovered: SEND fails after 6+ hours idle (SHOWSTOPPER)
Performance: QR 60% faster, handshake 40% faster, boot 30% faster
13 commits, 3 new lessons (#230-#232)
```

## Session 42: Consolidation and Quality Pass (2026-03-05)

```
Pure consolidation. No new features. Production-grade code quality.
smp_handshake.c: 74 lines debug removed, zero printf in production
smp_globals.c dissolved: 7 symbols to owning modules, file deleted
smp_app_run(): 530 to 118 lines via 5 static helpers
License headers: 47 files AGPL-3.0 + SPDX standardized
Ownership model: smp_types.h = types only, no object declarations
```

## Session 41: Pre-GitHub Cleanup (2026-03-04)

```
Pre-release code review and stabilization. 8 sub-packets, 11 commits.
Security cleanup: debug crypto removed, CWE-14 buffer clearing
Hardware AES fix: software fallback for DMA SRAM fragmentation
Screen lifecycle: ephemeral screens, ~14KB pool recovered
Dangling pointer protection: null guards on background UI calls
Comment cleanup: 9 files, session refs removed, German to English
GitHub security: CodeQL, Dependabot, secret push protection
5 lessons (#221-#225), MILESTONE 17
```

## Session 40: Sliding Window Chat History (2026-03-04)

```
Three-stage pipeline: SD (AES-256-GCM) > PSRAM Cache (30) > LVGL (5 bubbles)
Crypto outside SPI mutex (< 10ms hold), ~1.2KB per bubble
Bidirectional scroll with position correction and re-entrancy guard
1 bug (#71), 7 lessons (#214-#220), 7 files, 3 commits
```

## Session 39: On-Device WiFi Manager (2026-03-03)

```


 FIRST ON-DEVICE WIFI MANAGER FOR T-DECK HARDWARE

 Unified WiFi backend (NVS-only, single state machine)
 First-boot auto-launch, WPA3 SAE fix
 SPI DMA buffer pinned, dynamic main header
 Info tab redesign with live stats
 No other T-Deck project has on-device WiFi setup

 9 bugs (#62-#70), 4 lessons (#210-#213), 15 files
 Date: March 3, 2026


```

## Session 38: The SPI2 Bus Hunt (2026-03-01)

```


 THE SPI2 BUS HUNT: EIGHT HYPOTHESES, ONE ROOT CAUSE

 Display + keyboard backlight
 Settings screen with brightness sliders
 WiFi/LWIP → PSRAM (56KB freed)
 Root cause: SPI2 bus sharing (display + SD)
 SD removed = 100% stable for hours
 LVGL heap = separate 64KB pool (~8 bubbles)

 10 commits, 2 bugs (#60-#61), 5 lessons (#205-#209)
 8 hypotheses, 7 wrong, 1 correct
 Date: February 28 - March 1, 2026


```

## Session 37: Encrypted Chat History (2026-02-27)

```


 ENCRYPTED CHAT HISTORY ON SD CARD

 AES-256-GCM per-contact encryption
 HKDF-SHA256 key derivation from master key
 SPI2 bus serialization (display + SD share bus)
 Chunked rendering: 3 bubbles/tick progressive
 Contact list redesign (28px cards, search)

 4 sub-sessions, 2 commits, 2 bugs (#58-#59)
 Date: February 25-27, 2026


```

## Session 36: Contact Lifecycle (2026-02-25)

```


 CONTACT LIFECYCLE: DELETE, RECREATE, ZERO COMPROMISE

 Create → Chat → Delete → Recreate (no erase-flash!)
 NTP timestamps, ConnInfo displayName
 4-key NVS cleanup, KEY-HELLO TaskNotification
 UI cleanup on delete (bubbles + QR reset)

 4 sub-sessions, 12 commits, 7 bugs (#51-#57)
 Date: February 25, 2026


```

## Session 35: Multi-Contact Victory (2026-02-24)

```


 MULTI-CONTACT VICTORY -- ALL PLANNED BUGS FIXED

 2 contacts bidirectional, receipts, chat filter
 Ratchet slot ordering, KEY target, PSRAM guard
 Tested after erase-flash with 20+ messages

 6 fixes (35a-35h), 6 lessons (#187-#192)
 Date: February 24, 2026


```

## Session 34 Day 2: Multi-Contact Bidirectional (2026-02-24)

```


 MULTI-CONTACT BIDIRECTIONAL ENCRYPTED MESSAGING

 HISTORIC MILESTONE: First multi-contact bidirectional
 encrypted messenger on a microcontroller

 Contact 0: ESP32 <-> Phone Bidirectional Encrypted
 Contact 1: ESP32 <-> Phone Bidirectional Encrypted

 11 bugs (#40-#50) found and fixed in 6 phases
 ALL bugs: ONE pattern (global -> per-contact routing)
 Pattern established for contacts 2-127

 Date: February 24, 2026


```

## SESSION 34 Day 1: Multi-Contact Architecture (2026-02-23)

```


 MULTI-CONTACT -- FROM SINGLETON TO PER-CONTACT

 8 Commits -- most productive session yet
 Production Cleanup: zero private keys in logs
 Runtime Add-Contact: NET_CMD via Ring Buffer
 Per-Contact 42d: 128-bit bitmap
 Reply Queue Array: 128 slots, ~49KB PSRAM
 SMP v7 Signing Fix: 1-byte prefix
 PSRAM Total: ~158KB / 8MB (1.9%)
 KEY Command: rejected → Claude Code handover

 Date: February 23, 2026


```

## SESSION 32: "The Demonstration" (2026-02-20)

```


 "THE DEMONSTRATION" -- FROM PROTOCOL TO MESSENGER

 Before: Serial Monitor only
 After: Full messenger with Bubbles, Keyboard, Delivery Status

 7 Keyboard-to-Chat Steps
 Delivery Status: ... → → →
 LVGL Display Refresh Fix
 128-Contact Architecture
 2+ hours stable

 Date: February 19-20, 2026


```

## SESSION 31: Bidirectional Chat Restored! (2026-02-18)

```


 BIDIRECTIONAL CHAT RESTORED -- ROOT CAUSE FOUND!

 T6: Bidirectional RESOLVED (was in Session 30)

 Root Cause: txCount==1 filter in Drain-Loop
 One character: == instead of >= killed App→ESP32 for 3 weeks
 6 fixes applied, Evgeny guidance integrated
 MILESTONE 7: Multi-Task Bidirectional Chat

 Date: February 18, 2026


```

## SESSION 30: Intensive Debug Session (2026-02-18)

```


 INTENSIVE DEBUG SESSION -- 10 HYPOTHESES, 14 FIXES

 T5: Keyboard-Send PASSED
 T6: Bidirectional UNRESOLVED

 Problem: App→ESP32 messages never arrive after successful SUB
 10 hypotheses systematically excluded
 14 fixes and diagnostics applied
 SMP v6 → v7 upgrade (33 bytes saved)
 Expert question sent to Evgeny Poberezkin

 Date: February 16-18, 2026


```

## SESSION 29: Multi-Task BREAKTHROUGH! (2026-02-16)

```


 MULTI-TASK ARCHITECTURE -- BREAKTHROUGH!

 Complete encrypted messaging pipeline over FreeRTOS Tasks!
 - Network Task (Core 0, PSRAM): SSL read loop
 - Main Task (Internal SRAM): Parse, decrypt, NVS
 - Ring Buffer IPC: Cross-core communication

 First message "Hello from ESP32!" sent via new architecture!
 CRITICAL: PSRAM stacks + NVS writes = CRASH!

 Date: February 16, 2026


```

## SESSION 28: Phase 2b Success! (2026-02-15)

```


 FREERTOS TASK ARCHITECTURE -- PHASE 2b COMPLETE!

 Three FreeRTOS Tasks running in parallel!
 - Network Task (Core 0, 12KB stack)
 - App Task (Core 1, 16KB stack)
 - UI Task (Core 1, 8KB stack)

 Key Fix: ALL non-DMA resources moved to PSRAM
 Internal Heap: ~40KB preserved for mbedTLS/WiFi
 Duration: ~4 hours

 Date: February 15, 2026


```

## SESSION 27: Architecture Investigation (2026-02-15)

```


 FREERTOS ARCHITECTURE INVESTIGATION

 Phase 1: Folder restructure WORKS
 Phase 2: FreeRTOS Tasks BROKE main (90KB RAM at boot)
 Phase 3: Network Task Migration -- branch polluted

 ROOT CAUSE: Tasks started at boot, starved TLS/WiFi
 SOLUTION: Start tasks AFTER connection, not at boot
 LESSONS: 17 new (137 total)

 Date: February 14-15, 2026


```

## SESSION 26: Persistence! (2026-02-14)

```


 RATCHET STATE PERSISTENCE!

 MILESTONE 6: ESP32 survives reboot without losing crypto state!
 • Ratchet state restored from NVS flash
 • Queue credentials persisted
 • Delivery receipts work after reboot
 • Write-Before-Send: 7.5ms verified

 Date: February 14, 2026 (Valentine's Day Part 2)


```

## SESSION 25: Bidirectional Chat + Receipts! (2026-02-14)

```


 BIDIRECTIONAL ENCRYPTED CHAT + DELIVERY RECEIPTS!

 THREE MILESTONES in ONE Valentine's Day Session:
 • Milestone 3: First App message decrypted on ESP32
 • Milestone 4: Bidirectional encrypted chat
 • Milestone 5: Delivery receipts ()

 Refactoring: main.c 2440 → 611 lines (−75%)
 Date: February 14, 2026


```

## HISTORIC MILESTONE: CONNECTED! (2026-02-08 Session 23)

```


 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER!

 SimpleX App shows: "ESP32 -- Connected"
 Date: February 8, 2026 ~17:36 UTC


```

## MILESTONE #2: First Chat Message! (2026-02-11 Session 24)

```


 FIRST CHAT MESSAGE FROM A MICROCONTROLLER!

 SimpleX App shows: "Hello from ESP32!"
 Date: February 11, 2026
 Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App


```

## Current Status (2026-02-14 Session 25)

```
SESSION 25 - BIDIRECTIONAL CHAT + RECEIPTS!
===================================================

THREE MILESTONES ACHIEVED:
 • Milestone 3: First App message decrypted on ESP32
 • Milestone 4: Bidirectional encrypted chat ESP32 ↔ App
 • Milestone 5: Delivery receipts () working!

Session 25 Achievements:
 - 8 bugs fixed (5 critical, 3 high)
 - Nonce offset corrected (13, not 14)
 - Ratchet state persistence fixed
 - Receipt wire format corrected
 - main.c refactored: 2440 → 611 lines (−75%)
 - 4 new modules: smp_ack, smp_wifi, smp_e2e, smp_agent

NEXT: Message persistence, UI integration, multiple contacts
```

---

## Acknowledgments and Respect

**This project would not be possible without the incredible work of the SimpleX team.**

SimpleX Chat represents a groundbreaking achievement in privacy-preserving communication technology. The protocol design is elegant, well-thought-out, and prioritizes user privacy above all else. We have the deepest respect for:

- **Evgeny Poberezkin** and the entire SimpleX Chat team
- The brilliant cryptographic design combining X3DH, Double Ratchet, and post-quantum algorithms
- The commitment to open source (AGPL-3.0) that made this project possible
- The comprehensive Haskell implementation that served as our reference

**Links:**
- SimpleX Chat: https://simplex.chat
- SimpleX GitHub: https://github.com/simplex-chat

---

## Document Structure

| Document | Lines | Description |
|----------|-------|-------------|
| [01_SIMPLEX_PROTOCOL_INDEX.md](01_SIMPLEX_PROTOCOL_INDEX.md) | ~310 | Navigation index |
| [STATUS.md](STATUS.md) | ~330 | Current status summary |
| [03_PART1_INTRO_SESSIONS_1-2.md](03_PART1_INTRO_SESSIONS_1-2.md) | ~2300 | Foundation, TLS 1.3, basic SMP |
| [04_PART2_SESSIONS_3-4.md](04_PART2_SESSIONS_3-4.md) | ~1000 | Wire format, bugs #1-8 |
| [05_PART3_SESSIONS_5-6.md](05_PART3_SESSIONS_5-6.md) | ~800 | X448 breakthrough, SMPQueueInfo |
| [06_PART4_SESSION_7.md](06_PART4_SESSION_7.md) | ~3200 | AES-GCM verification, Tail encoding |
| [07_PART5_SESSION_8.md](07_PART5_SESSION_8.md) | ~400 | AgentConfirmation works! |
| [08_PART6_SESSION_9.md](08_PART6_SESSION_9.md) | ~450 | Reply Queue HSalsa20 fix |
| [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | cmNonce fix, app "connecting" |
| [10_PART8_SESSION_11.md](10_PART8_SESSION_11.md) | ~400 | Regression & Recovery |
| [11_PART9_SESSION_12.md](11_PART9_SESSION_12.md) | ~400 | E2E Keypair Fix Attempt |
| [12_PART10_SESSION_13.md](12_PART10_SESSION_13.md) | ~700 | E2E Crypto Deep Analysis |
| [13_PART11_SESSION_14.md](13_PART11_SESSION_14.md) | ~900 | DH SECRET VERIFIED! |
| [14_PART12_SESSION_15.md](14_PART12_SESSION_15.md) | ~650 | Root Cause Found |
| [15_PART13_SESSION_16.md](15_PART13_SESSION_16.md) | ~900 | Custom XSalsa20 + Double Ratchet |
| [16_PART14_SESSION_17.md](16_PART14_SESSION_17.md) | ~500 | Key Consistency Debug |
| [17_PART15_SESSION_18.md](17_PART15_SESSION_18.md) | ~600 | BUG #18 SOLVED! E2E Decrypt SUCCESS |
| [18_PART16_SESSION_19.md](18_PART16_SESSION_19.md) | ~550 | Header Decrypt SUCCESS! |
| [19_PART17_SESSION_20.md](19_PART17_SESSION_20.md) | ~600 | Body Decrypt SUCCESS! Peer Profile! |
| [20_PART18_SESSION_21.md](20_PART18_SESSION_21.md) | ~700 | v3 Format + HELLO Debugging |
| [21_PART19_SESSION_22.md](21_PART19_SESSION_22.md) | ~600 | Reply Queue Flow Discovery |
| [22_PART20_SESSION_23.md](22_PART20_SESSION_23.md) | ~570 | CONNECTED! Historic Milestone! |
| [23_PART21_SESSION_24.md](23_PART21_SESSION_24.md) | ~600 | First Chat Message! Milestone #2! |
| [24_PART22_SESSION_25.md](24_PART22_SESSION_25.md) | ~480 | Bidirectional + Receipts! M3,4,5! |
| [25_PART23_SESSION_26.md](25_PART23_SESSION_26.md) | ~600 | Persistence! Milestone 6! |
| [26_PART24_SESSION_27.md](26_PART24_SESSION_27.md) | ~650 | FreeRTOS Architecture Investigation |
| [27_PART25_SESSION_28.md](27_PART25_SESSION_28.md) | ~550 | Phase 2b Success -- Tasks Running! |
| [28_PART26_SESSION_29.md](28_PART26_SESSION_29.md) | ~750 | Multi-Task Architecture BREAKTHROUGH! |
| [29_PART27_SESSION_30.md](29_PART27_SESSION_30.md) | ~660 | ** Intensive Debug -- 10 Hypotheses, 14 Fixes** |
| [30_PART28_SESSION_31.md](30_PART28_SESSION_31.md) | ~850 | ** Bidirectional Restored! Milestone 7!** |
| [31_PART29_SESSION_32.md](31_PART29_SESSION_32.md) | ~500 | ** "The Demonstration" -- From Protocol to Messenger** |
| [32_PART30_SESSION_34.md](32_PART30_SESSION_34.md) | ~473 | ** Multi-Contact Architecture -- Per-Contact Reply Queue** |
| [33_PART31_SESSION_34_BREAKTHROUGH.md](33_PART31_SESSION_34_BREAKTHROUGH.md) | ~545 | ** Multi-Contact Bidirectional -- HISTORIC MILESTONE** |
| [34_PART32_SESSION_35.md](34_PART32_SESSION_35.md) | ~304 | ** Multi-Contact Victory -- All Planned Bugs Fixed** |
| [35_PART33_SESSION_36.md](35_PART33_SESSION_36.md) | ~389 | ** Contact Lifecycle: Delete, Recreate, Zero Compromise** |
| [36_PART34_SESSION_37.md](36_PART34_SESSION_37.md) | ~332 | ** Encrypted Chat History: SD Card, SPI Bus Wars** |
| [37_PART35_SESSION_38.md](37_PART35_SESSION_38.md) | ~324 | ** The SPI2 Bus Hunt: Eight Hypotheses, One Root Cause** |
| [38_PART36_SESSION_39.md](38_PART36_SESSION_39.md) | ~310 | **WiFi Manager: First On-Device WiFi Setup for T-Deck** |
| [39_PART37_SESSION_40.md](39_PART37_SESSION_40.md) | ~273 | **Sliding Window: Unlimited Encrypted History at Constant Memory** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~2600 | Complete bug documentation (71 bugs, 220 lessons) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~2950 | Constants, wire formats, verified values |

**Total: ~33,000+ lines of detailed protocol analysis (Session docs + reference docs)**

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | AES-GCM verification, SimpleX contact | - |
| 8 | Jan 27, 2026 | AgentConfirmation WORKS! | #13-14 |
| 9 | Jan 27, 2026 | Reply Queue HSalsa20 fix | #15-16 |
| 10C | Jan 28, 2026 | cmNonce fix, app "connecting" | #17 |
| 11 | Jan 30, 2026 | Regression & Recovery | - |
| 12 | Jan 30, 2026 | E2E Keypair Analysis | - |
| 13 | Jan 30, 2026 | E2E Crypto Deep Analysis | - |
| 14 | Jan 31 - Feb 1 | DH SECRET VERIFIED! | #18 (partial) |
| 15 | Feb 1 | Root Cause Found (later disproven) | #18 (root cause) |
| 16 | Feb 1-3 | Custom XSalsa20 + Double Ratchet | #18 (narrowed) |
| 17 | Feb 4 | Key Consistency Debug | #18 (investigating) |
| 18 | Feb 5 | BUG #18 SOLVED! E2E Decrypt SUCCESS! | #18 SOLVED |
| 19 | Feb 5 | Header Decrypt SUCCESS! MsgHeader Parsed | #19 found |
| 20 | Feb 6 | Body Decrypt! Peer Profile on ESP32! | #19 SOLVED |
| 21 | Feb 6-7 | v3 Format + HELLO Debugging (7 bugs!) | #20-#26 |
| **22** | **Feb 7** | **Reply Queue Flow Discovery (5 bugs!)** | **#27-#31** |
| **23** | **Feb 7-8** | ** CONNECTED! Historic Milestone!** | **ZERO new!** |
| **24** | **Feb 11-13** | ** First Chat Message! Milestone #2!** | **ZERO new!** |
| **25** | **Feb 13-14** | ** Bidirectional + Receipts! M3,4,5!** | **8 bugs!** |
| **26** | **Feb 14** | ** Persistence! Milestone 6!** | **0 bugs** |
| **27** | **Feb 14-15** | ** FreeRTOS Architecture Investigation** | **17 lessons** |
| **28** | **Feb 15** | ** Phase 2b Success -- 3 Tasks Running!** | **6 lessons** |
| **29** | **Feb 16** | ** Multi-Task Architecture BREAKTHROUGH!** | **5 lessons** |
| **30** | **Feb 16-18** | ** Intensive Debug -- 10 Hypotheses, 14 Fixes** | **4 lessons** |
| **31** | **Feb 18** | ** Bidirectional Restored! Milestone 7!** | **9 lessons** |
| **32** | **Feb 19-20** | ** "The Demonstration" -- Full Messenger UI!** | **7 lessons** |
| **34** | **Feb 23** | ** Multi-Contact Architecture -- 8 Commits!** | **7 lessons** |
| **34b** | **Feb 24** | ** Multi-Contact Bidirectional -- HISTORIC!** | **11 bugs!** |
| **35** | **Feb 24** | ** Multi-Contact Victory -- All Bugs Fixed!** | **6 lessons** |
| **36** | **Feb 25** | ** Contact Lifecycle: Delete, Recreate, Zero Compromise** | **7 bugs, 10 lessons** |
| **37** | **Feb 25-27** | ** Encrypted Chat History: SD Card, SPI Bus Wars** | **2 bugs, 2 lessons** |
| **38** | **Feb 28 - Mar 1** | ** The SPI2 Bus Hunt: Eight Hypotheses, One Root Cause** | **2 bugs, 5 lessons** |
| **39** | **Mar 3** | ** WiFi Manager: First On-Device WiFi for T-Deck** | **9 bugs, 4 lessons** |
| **40** | **Mar 3-4** | **Sliding Window: Unlimited Encrypted History** | **1 bug, 7 lessons** |
| **41** | **Mar 4** | ** Pre-GitHub Cleanup and Stabilization** | **5 lessons** |
| **42** | **Mar 4-5** | ** Consolidation and Quality Pass** | **4 lessons** |
| **43** | **Mar 5-8** | **Wiki + Security Cleanup + Display Name + Bug #20** | **3 lessons, 1 SHOWSTOPPER** |

---

## Session 43 Key Achievements -- Documentation + Security + Display Name

### Wiki Live at wiki.simplego.dev
```
Docusaurus 3, GitHub Actions deployment, offline search (1924 docs)
17 migrated documents + 10 new smp-in-c/ pages (world-first)
SimpleGo design system, mobile navigation fix, zero broken links
SimpleGo cited in official SimpleX Network Architecture document
```

### Security Log Cleanup
```
All cryptographic material removed from serial output:
smp_parser.c (9 removals), smp_tasks.c (2 blocks), smp_contacts.c (5 lines)
Including CRITICAL: decrypted plaintext dump removed
```

### Display Name Feature
```
NVS-backed user name replaces hardcoded "ESP32"
First-boot prompt (UI_SCREEN_NAME_SETUP)
Settings editor with fullscreen overlay keyboard
Crash fix: ui_connect.c dangling pointers
```

### Bug #20: SEND After Extended Idle (SHOWSTOPPER)
```
SEND fails after 6+ hours idle. PING/PONG still working.
Red X on display. Device reset fixes immediately.
Must be resolved before alpha release.
```

---

## Session 42 Key Achievements -- Consolidation and Quality Pass

```
smp_handshake.c: 74 lines debug removed (9 printf blocks, auth-key leak, plaintext leak)
smp_globals.c dissolved: 7 symbols migrated to owning modules, file deleted
smp_app_run(): 530 lines to 118 via 5 static helpers (identical object code)
License headers: 47 files standardized (AGPL-3.0 + SPDX)
Ownership model: smp_types.h = types only, no object declarations
UTF-8 BOM cleanup (7 files), extern TODO resolved, re-delivery log verified
Zero printf in production. Build green. Device stable.
```

---

## Session 41 Key Achievements -- Pre-GitHub Cleanup

```
Security: simplex_secretbox_open_debug() deleted, brute-force loop replaced
Hardware AES: CONFIG_MBEDTLS_HARDWARE_AES=n (DMA needs contiguous internal SRAM)
Screen lifecycle: ephemeral pattern recovers ~34KB LVGL pool
Dangling pointers: ui_chat_cleanup() + if(!screen) return guards
Bubble eviction: evict-before-create order fix
LVGL pool: 960-1368 bytes/bubble (avg ~1150), no leak on contact switch
Comment cleanup: 9 files, session refs removed, German to English
CWE-14: mbedtls_platform_zeroize() for sensitive buffers
GitHub: CodeQL, Dependabot, secret push protection, SECURITY.md
```

---

## Session 40 Key Achievements -- Sliding Window Chat History

```
Three-stage pipeline: SD (AES-256-GCM) > PSRAM Cache (30) > LVGL (5 bubbles)
Crypto-separation from SPI mutex: hold time reduced from ~500ms to < 10ms
LVGL pool profiled: ~1.2KB/bubble, 64KB pool effectively ~61KB
Bidirectional scroll with position correction, re-entrancy guard (Bug #71)
Unlimited encrypted history at constant 6KB memory consumption
```

---

## Session 39 Key Achievements -- On-Device WiFi Manager

### First On-Device WiFi for T-Deck Hardware

```
Market research: Meshtastic, Bruce, ESP32Berry, MeshCore, ESPP, all
ESP-IDF WiFi libraries -- none has on-device WiFi with LVGL + keyboard.

Backend: Unified wifi_manager.c (was 2 fighting files)
WPA3: SAE fix (WIFI_AUTH_WPA2_PSK threshold, 100+ test attempts)
DMA: LVGL buffer pinned to internal SRAM (PSRAM = SPI DMA fail)
Boot: First-boot auto-launch WiFi Manager, navigation guard
UI: Dynamic header (SSID/unread/NoWiFi), info tab live stats
```

---

## Session 38 Key Achievements -- The SPI2 Bus Hunt

### Backlight Control + Root Cause Discovery

```
Features: Display backlight (GPIO 42, 16 levels), keyboard (I2C 0x55)
 Settings screen with sliders, WiFi/LWIP → PSRAM (56KB freed)

Root Cause: SPI2 bus sharing (display + SD card)
 8 hypotheses tested: 7 wrong, 1 correct
 SD removed = device runs hours, 100% stable
 Fix: Move SD to SPI3 (Session 39)

LVGL Discovery: Separate 64KB heap, ~8 bubbles max
 heap_caps_get_free_size() ≠ LVGL pool
 MAX_VISIBLE_BUBBLES sliding window introduced
```

---

## Session 37 Key Achievements -- Encrypted Chat History

### AES-256-GCM Encrypted SD Storage

```
Architecture: Master Key (NVS) → HKDF-SHA256 → Per-Contact Key
Format: Append-only, [4B len][12B nonce][16B tag][encrypted payload]
Nonce: Deterministic (slot_index + msg_index), never reused
Path: /sdcard/simplego/msgs/chat_XX.bin (one per contact)
```

### SPI2 Bus Fix + Chunked Rendering

```
Bug #58: Display + SD share SPI2 bus → LVGL mutex serialization
Bug #59: 20 bubbles at once = 1.5s freeze → 3/tick chunked = 350ms
DMA draw buffer: PSRAM → internal SRAM (anti-tearing)
Contact list: 44px → 28px cards, search overlay, cyan bottom bar
```

---

## Session 36 Key Achievements -- Contact Lifecycle

### Complete Contact Lifecycle

```
Create → Chat → Delete → Recreate -- No erase-flash!
NTP: SNTP time sync, real timestamps (Mon | 14:35)
Name: displayName from ConnInfo JSON
Delete: 4-key NVS cleanup (rat/peer/hand/rq_%02x)
Sync: KEY-HELLO race fix (FreeRTOS TaskNotification)
UI: Bubbles cleared, QR reset, contact list redesigned
Security: NVS unencrypted -- TODO nvs_flash_secure_init
```

---

## Session 35 Key Achievements -- Multi-Contact Victory

### All Planned Bugs Fixed

```
Fix 35a: Ratchet slot ordering (set_active BEFORE process_message)
Fix 35c: KEY targets Reply Queue (not Contact Queue)
Fix 35e: Per-contact chat filter (bubble tagging + HIDDEN)
Fix 35f: PSRAM guard (prevents NVS overwrite)
Fix 35g: Contact Queue decrypt ratchet switch
Fix 35h: NVS fallback for Contact >0 after boot

Verified: 2 contacts, bidirectional, receipts, 20+ messages
Root cause: "wrong slot active" -- set_active() FIRST, always
```

---

## Session 34 Day 2 -- Multi-Contact Bidirectional Encrypted Messaging

### HISTORIC MILESTONE

```
Contact 0: ESP32 <-> Phone Bidirectional Encrypted
Contact 1: ESP32 <-> Phone Bidirectional Encrypted

First multi-contact bidirectional encrypted messenger on a microcontroller.
```

### 11 Bugs Fixed in 6 Phases

```
Phase 1: KEY Command Fix
 #40: Wrong queue credentials (Reply Queue -> Contact Queue)
 #41: Missing SPKI length prefix (0x2C)

Phase 2: Ghost Write -> ERR BLOCK
 #42: reply_queue_create() bypassed standard write (5 sub-errors)
 #43: IDS response parsing (txCount header)
 #44: NVS PSRAM crash (cache disabled assert)

Phase 3: Global State Elimination
 #45: Global pending_peer overwritten (peer_prepare_for_contact)
 #46: 4x DISCARD -> FORWARD (frame loss in subscribe)

Phase 4: Reply Queue Encoder
 #47: SMPQueueInfo 3 byte errors (132B -> 134B)

Phase 5a: Per-Contact Index Routing
 #48: Pointer arithmetic contact_idx=0 (explicit parameter)
 #49: NVS key hardcoded peer_00 (dynamic peer_%02x)

Phase 5b: Crypto Fix
 #50: crypto_scalarmult -> crypto_box_beforenm (HSalsa20)
```

### The ONE Pattern

```
ALL 11 bugs: global/hardcoded state -> per-contact routing
No new algorithms. Only consistent index routing.
Pattern established for contacts 2-127.
```

---

## Session 34 Key Achievements -- Multi-Contact Architecture

### 1. Production Cleanup (-200+ lines)

```
Stripped from logs:
 - All 32-byte private key hex dumps → 4-byte fingerprints
 - DH secrets, chain keys, message keys, cleartext
 - "Hello from ESP32!" auto-message test artifact
 - "FULL KEYS FOR PYTHON TEST" blocks
```

### 2. Per-Contact Reply Queue (128 PSRAM slots)

```
reply_queue_t: ~384 bytes per slot
 rcv_id[24] + snd_id[24] + 4 keys[32 each] + flags + server_host[64]

128 slots = ~49KB PSRAM
NVS persistence: rq_00 through rq_127
Total PSRAM: ~158KB / 8MB (1.9%)
```

### 3. SMP v7 Signing Fix

```
Signing buffer length prefixes must be 1-byte, not 2-byte Large.
WRONG: [2B corrLen][corrId][2B entLen][entityId][command]
RIGHT: [1B corrLen][corrId][1B entLen][entityId][command]
Fixed: SUB, KEY, NEW all corrected.
```

### 4. 8 Commits (Most Productive Session)

```
1. refactor(ratchet): strip debug dumps, fix index range for 128 contacts
2. refactor(cleanup): remove debug dumps and test artifacts for production
3. feat(contacts): runtime add-contact via network command
4. feat(handshake): per-contact 42d tracking with bitmap
5. feat(ui): add-contact button with auto-naming and QR flow
6. feat(queue): per-contact reply queue array in PSRAM
7. feat(queue): wire reply queues into protocol flow and 42d
8. fix(smp): correct SMP command signing format (1-byte session prefix)
```

---

## Session 32 Key Achievements -- "The Demonstration"

### 1. Keyboard-to-Chat Integration (7 Steps)

```
T-Deck HW Keyboard → LVGL kbd_indev → Textarea → Enter
 → on_input_ready() → send_cb() → kbd_msg_queue → smp_app_run()
 → peer_send_chat_message() → Server → App

Reverse (Receive):
 decrypt → smp_notify_ui_message() → app_to_ui_queue
 → LVGL Timer (50ms) → ui_chat_add_message() → Cyan Bubble
```

### 2. Delivery Status System

```
"..." (Sending) → "" (Sent) → "" (Delivered) → "" (Failed)
 Dim color Dim color Green Red

16-slot tracking: seq → LVGL label pointer
Receipt parsing: inner_tag 'V' → msg_id → seq lookup → ""
```

### 3. Multi-Contact Architecture Analysis

```
Finding 1: Receive path ALREADY multi-contact capable
 find_contact_by_recipient_id() routes correctly

Finding 2: Send path hardcoded to contacts[0]
 7 locations, 3 need change, 4 stay (42d handshake init)

Solution: smp_set_active_contact(idx) / smp_get_active_contact()
```

### 4. 128-Contact PSRAM Planning

```
ratchet_state_t ratchets[128] in PSRAM
 128 × 530B = 68KB (0.8% of 8MB)
 Zero latency contact switch
 NVS only at boot + after each message
```

---

## Session 31 Key Achievements -- Bidirectional Chat Restored!

### 1. Root Cause Found: txCount==1 Filter

```
subscribe_all_contacts() Drain-Loop:
 BEFORE: if (rq_resp[rrp] == 1) { ... } ← DISCARDS txCount > 1
 AFTER: if (rq_resp[rrp] >= 1) { ... } ← Accepts batched responses

Server batches SUB OK + pending MSG with txCount=2:
 TX1: OK (53 bytes)
 TX2: MSG (16178 bytes) ← was silently discarded!

One character change: == → >=
Three weeks of debugging resolved.
```

### 2. Six Fixes Applied

```
1. TCP Keep-Alive (keepIdle=30, keepIntvl=15, keepCnt=4)
2. SMP PING/PONG (30s interval, connection health)
3. Reply Queue SUB on main socket (sock 54)
4. txCount >= 1 acceptance (ROOT CAUSE!)
5. TX2 MSG Forwarding to App Task via Ring Buffer
6. Re-Delivery Handling (msg_ns < recv → ACK only)
```

### 3. Evgeny Guidance Integrated

```
Key insights from SimpleX protocol creator:
 - "Subscription can only exist in one socket though"
 - "if you subscribe from another socket, the first would receive END"
 - "concurrency is hard."
 - NEW creates subscribed by default, SUB is noop
 - TCP Keep-Alive for NAT, not for subscription survival
```

### 4. Milestone 7: Multi-Task Bidirectional Chat

```
Complete encrypted messaging in FreeRTOS Multi-Task Architecture:
 ESP32 → App: (was working since S29)
 App → ESP32: (RESTORED in S31!)
 Delivery Receipts:
 Ratchet Persistence:
 TCP Keep-Alive + PING/PONG:
 Batch Handling (txCount > 1):
 Re-Delivery Detection:
```

---

## Session 30 Key Achievements -- Intensive Debug Session

### 1. T5: Keyboard-Send Integration

```c
// Non-blocking keyboard poll in smp_app_run()
if (kbd_queue != NULL) {
 kbd_msg_t kbd_msg;
 if (xQueueReceive(kbd_queue, &kbd_msg, 0) == pdTRUE) {
 peer_send_chat_message(kbd_msg.text);
 }
}
```

### 2. 10 Hypotheses Systematically Excluded

```
Problem: App→ESP32 messages never arrive after successful SUB

Excluded:
 1. corrId format (1 byte → 24 bytes)
 2. Batch framing
 3. Subscribe failure
 4. Delivery blocked
 5. Network Task crash
 6. SSL connection broken
 7. SMP v6 incompatibility
 8. SessionId on wire
 9. Response parser offset
 10. ACK chain interruption
```

### 3. SMP v6 → v7 Upgrade

```
v6 SUB: 151 bytes
v7 SUB: 118 bytes (33 bytes saved -- SessionId removed from wire)
```

### 4. Expert Question to Evgeny

```
"Is there a condition where the server would accept a SUB
 but then not deliver incoming MSGs to that subscription?"

Status: Awaiting response
```

---

## Session 29 Key Achievements -- Multi-Task BREAKTHROUGH!

### 1. Complete Multi-Task Architecture

```
Network Task (Core 0, 12KB PSRAM):
 → SSL read loop → Ring Buffer → Main Task

Main Task (64KB Internal SRAM):
 → smp_app_run() → Parse → Decrypt → NVS

Ring Buffer IPC:
 → net_to_app: 37KB (frames)
 → app_to_net: 1KB (commands)
```

### 2. Critical Discovery: PSRAM + NVS = CRASH!

```
ESP32-S3: Tasks with PSRAM stack must NEVER write to NVS!

Root Cause:
 - SPI Flash write disables cache
 - PSRAM is cache-based
 - Task loses stack access during Flash write

Solution:
 - App logic runs in Main Task (Internal SRAM)
 - Network Task (PSRAM) only does SSL reads
```

### 3. First Message via New Architecture

```
"Hello from ESP32!" successfully sent via:
 Main Task → Peer SSL → SimpleX App
```

---

## Session 28 Key Achievements -- Phase 2b Success!

### 1. Three FreeRTOS Tasks Running

```
Task Architecture:
 Network Task (Core 0, 12KB stack, Priority 7)
 App Task (Core 1, 16KB stack, Priority 6)
 UI Task (Core 1, 8KB stack, Priority 5)

Ring Buffers:
 Network→App: 2KB (PSRAM)
 App→Network: 1KB (PSRAM)
```

### 2. PSRAM Solution

```
Problem: Tasks + Frame Pool pushed internal heap to 19KB → receive dead
Solution: Move ALL non-DMA resources to PSRAM

Moved to PSRAM:
 - Frame Pool: 16KB → heap_caps_calloc(MALLOC_CAP_SPIRAM)
 - Ring Buffers: 3KB → xRingbufferCreateWithCaps(MALLOC_CAP_SPIRAM)
 - Task Stacks: 36KB → already PSRAM

Result: Internal Heap ~40KB preserved
```

### 3. Critical Lesson: erase-flash

```powershell
# After EVERY branch switch or sdkconfig change:
idf.py erase-flash -p COM6
# Then create new contact in app
```

---

## Session 27 Key Findings -- Architecture Investigation

### 1. Phase Results

```
Phase 1 (Folder restructure): Works perfectly
Phase 2 (FreeRTOS tasks): Broke main branch
Phase 3 (Network task): Branch polluted by debugging

Root Cause: ~90KB RAM reserved at boot starved TLS/WiFi
Solution: Start tasks AFTER connection, not at boot
```

### 2. sdkconfig Fixes Found

```ini
# Mandatory for 16KB SMP blocks:
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384

# Minimum for TLS records > 4096:
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768
```

### 3. Key Lesson

```
Always baseline-test main before debugging feature branch.
Git bisect would have saved 2 days.
```

---

## Session 26 Key Achievements -- Persistence!

### 1. MILESTONE 6: Ratchet State Persistence!

```
ESP32 survives reboot without losing crypto state!

Write-Before-Send pattern (Evgeny's golden rule):
 Generate key → Persist to flash → THEN send

NVS Storage: 128KB partition, 150+ contacts supported
Write timing: 7.5ms verified (negligible vs network latency)
```

### 2. Storage Architecture

```
NVS (Internal Flash) SD Card (External)
 Ratchet States Message History
 Queue Credentials Contact Profiles
 Peer Connection File Attachments
 Device Config

Capacity: 256 million texts, 19 years mixed usage on 128GB
```

### 3. Delivery Receipts After Reboot

```
Test: Reboot ESP32 → App sends message → ESP32 decrypts → works!
Verified with multiple consecutive reboots.
```

### 4. Keyboard Integration (Partial)

```
Working: Keyboard → Serial → Message arrives in app
Not working: Chat screen UI (architecture refactor needed)
```

---

## Session 25 Key Achievements -- Bidirectional + Receipts!

### 1. THREE MILESTONES IN ONE SESSION!

```
Milestone 3: First App message decrypted on ESP32
Milestone 4: Bidirectional encrypted chat ESP32 ↔ App
Milestone 5: Delivery receipts () working!
```

### 2. Massive Refactoring

```
main.c: 2440 → 611 lines (−75%)

New modules:
 - smp_ack.c/h ACK handling
 - smp_wifi.c/h WiFi initialization
 - smp_e2e.c/h E2E envelope decryption
 - smp_agent.c/h Agent protocol layer
```

### 3. Critical Bug Fixes (8 total)

```
Nonce Offset: 14 → 13 (brute-force discovered)
Ratchet State: Copy → Pointer (persistence bug)
Chain KDF: Relative → Absolute skip
Receipt count: Word16 → Word8
Receipt rcptInfo: Word32 → Word16
txCount Parser: Hardcoded → Variable
Heap Overflow: malloc(256) → dynamic
NULL Guard: contact check for Reply Queue
```

### 4. Key Discovery: Nonce Offset 13

```
Session 24 believed: Byte [12] = corrId '0' → use cache
Session 25 discovered: Byte [12] = first nonce byte!

Brute-force scan: DECRYPT OK at nonce_offset=13!
```

### 5. Receipt Wire Format

```
A_RCVD ('V') payload:
 'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]

AMessageReceipt:
 agentMsgId(8B) + msgHash(1+32B) + rcptInfo(Word16)
```

---

## Session 24 Key Achievements -- First Chat Message!

### 1. ZERO New Bugs -- Again!

All 31 bugs from Sessions 4-22 remain sufficient. Session 23 and 24 achieved milestones with ZERO new bugs!

### 2. First Chat Message from Microcontroller

```
"Hello from ESP32!" displayed in SimpleX App!

Required: ChatMessage JSON format (not raw UTF-8)
 {"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"Hello from ESP32!"}}}
```

### 3. Session 23 Correction

```
Session 23: "HELLO received on Q_B" → FALSE POSITIVE!
Reality: Random 0x48 byte in Ratchet ciphertext matched 'H'
Actual: Tag 'I' ConnInfo (after implementing Q_B Ratchet decrypt)
```

### 4. ACK Protocol Documented

```
SMP Flow Control:
 - Server delivers MSG → blocks until ACK
 - Missing ACK = queue backs up, no further delivery
 - ACK is Recipient Command (signed with rcv_private_auth_key)
 - ACK response can be OK (empty) or MSG (next message)
```

### 5. PQ-Kyber Graceful Degradation Verified

```
App sends: emHeaderLen=2346 (Post-Quantum Kyber)
Our sends: emHeaderLen=124 (pure DH, KEM Nothing)
Result: Both directions work! Graceful degradation successful.
```

### 6. Bug Fixed in Session 25

```
Session 24 Hypothesis: Format error in AgentConfirmation or HELLO
Session 25 Discovery: Nonce offset was 14 instead of 13!

Brute-force scan found the truth, bidirectional now works!
```

---

## Session 23 Key Achievements -- CONNECTED!

### 1. ZERO New Bugs!

All 31 bugs from Sessions 4-22 were sufficient. The crypto was already correct!

### 2. Role Clarification

| Role | Party | Creates | Sends Tag |
|------|-------|---------|-----------|
| Bob | ESP32 (Accepting) | Reply Queue (Q_B) | Tag 'D' (with Q_B info) |
| Alice | App (Initiating) | Contact Queue (Q_A) | Tag 'I' (profile only) |

**Session 22 assumed** App sends Reply Queue info in Tag 'D' -- **WRONG!**
**Session 23 discovered** WE send Tag 'D', App sends Tag 'I'.

### 3. Legacy vs Modern Path

```
PHConfirmation 'K' → Legacy Path:
 - Requires KEY command + HELLO exchange
 - Both parties must send HELLO
 - We use this path!

PHEmpty '_' → Modern Path (senderCanSecure):
 - Only ACK, CON immediate
 - No HELLO needed
 - NOT what the App uses with us!
```

### 4. KEY Command Discovery

```
KEY = Recipient Command:
 - Signed with: rcv_private_auth_key (OUR key)
 - Sent on: OUR queue (where we're recipient)
 - Authorizes: The SENDER (App) to send messages
 - Body: "KEY " + 0x2C + 44B peer_sender_auth_key SPKI
```

### 5. TLS Timeout + Reconnect

```
Problem: Reply Queue TLS connection times out during Confirmation processing

Solution:
 1. Reconnect TLS to Reply Queue server
 2. Send SUB (re-subscribe to queue)
 3. Send KEY command
 4. Send HELLO
```

### 6. Complete 7-Step Handshake Verified

```
Step Queue Direction Content

1. -- App NEW → Q_A, Invitation QR
2a. Q_A ESP32→App SKEY (Register Sender Auth)
2b. Q_A ESP32→App CONF Tag 'D' (Q_B + Profile)
3. -- App processConf → CONF Event
4. -- App LET/Accept Confirmation
5a. Q_A App KEY on Q_A (senderKey)
5b. Q_B App→ESP32 SKEY on Q_B
5c. Q_B App→ESP32 Tag 'I' (App Profile)
6a. Q_B ESP32 Reconnect + SUB + KEY
6b. Q_A ESP32→App HELLO
6c. Q_B App→ESP32 HELLO
7. -- Both CON -- "CONNECTED"
```

### 7. Evgeny Contact Restored

Evgeny reached out on Feb 8 -- he wasn't upset about the deleted conversation,
he had simply missed the file! Relationship restored, SimpleX team continues support.

---

## Session 22 Key Achievements

### 1. Five Bugs Fixed (#27-#31)

| Bug | Component | Fix |
|-----|-----------|-----|
| #27 | E2E version_min | 2 → 3 + KEM Nothing (App breaks silence!) |
| #28 | KEM Parser | Dynamic for SNTRUP761 (up to 2346 bytes) |
| #29 | Body Decrypt Pointer | Dynamic emHeader size calculation |
| #30 | HKs/NHKs Init + Promotion | Three-part header key chain fix |
| #31 | Header Decrypt Try-Order | HKr first, NHKr second for AdvanceRatchet |

### 2. Protocol Discovery (Later Corrected in S23!)

**Session 22 Theory:** Modern SimpleX (v2 + `senderCanSecure = True`) does NOT need HELLO!

**Session 23 Correction:** This is only true for Modern Path (PHEmpty '_').
We receive PHConfirmation 'K' = Legacy Path = KEY + HELLO required!

```
Session 22 Assumed Flow (WRONG for Legacy Path):
 1. ESP32 creates Invitation Working
 2. App sends AgentConfirmation Working
 3. ESP32 extracts Reply Queue Info ← WRONG! App sends 'I', not 'D'!
 4-7. Modern Path flow ← WRONG! We use Legacy Path!

Session 23 Correct Flow (Legacy Path):
 See 7-step handshake above!
```

### 3. Post-Quantum KEM

SimpleX uses **SNTRUP761** (not Kyber1024):
- Public Key: 1158 bytes
- Ciphertext: 1039 bytes
- Shared Secret: 32 bytes

PQ-Graceful-Degradation: v3 + KEM Nothing → pure DH fallback (no error).

### 4. Reply Queue Info Location

The `smpReplyQueues` are inside Tag `'D'` (AgentConnInfoReply) at the innermost
ratchet-decrypted layer. **We send this, not receive it!** (Corrected in S23)

---

## Session 21 Key Achievements

### 1. Seven HELLO Format Bugs Fixed (#20-#26)

| Bug | Component | Fix |
|-----|-----------|-----|
| #20 | PrivHeader for HELLO | '_' → 0x00 (no PrivHeader) |
| #21 | AgentVersion | v2 → v1 for AgentMessage |
| #22 | prevMsgHash | Raw → Word16 prefix encoding |
| #23 | cbEncrypt padding | Pad BEFORE encrypt |
| #24 | DH Key selection | rcv_dh → snd_dh for HELLO |
| #25 | PubHeader Nothing | Missing → '0' (0x30) |
| #26 | v2/v3 format | 1-byte → 2-byte prefixes + KEM Nothing |

### 2. v3 EncRatchetMessage Format

```
v3 changes from v2:
 - emHeader prefix: 1 byte → 2 bytes Word16 BE
 - emHeader size: 123 → 124 bytes
 - ehBody prefix: 1 byte → 2 bytes Word16 BE
 - MsgHeader: +KEM Nothing ('0'), contentLen 79→80
 - Verified byte-correct, Server accepts with OK
```

### 3. New Architecture

- **4 Header Keys:** HKs/NHKs/HKr/NHKr with promotion
- **SameRatchet vs AdvanceRatchet** modes
- **KEY Command** implemented (optional for unsecured queues)

---

## Session 20 Key Achievements

### 1. Bug #19 FIXED

Root cause: Debug self-decrypt test corrupted ratchet state.

### 2. Complete Crypto Chain

```
TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
→ ClientMessage → EncRatchetMessage → Header Decrypt
→ DH Ratchet Step → Chain KDF → Body Decrypt
→ unPad → ConnInfo 'I' → Zstd → Peer Profile JSON
```

### 3. Peer Profile Read

`"displayName": "cannatoshi"` -- first SimpleX profile read on ESP32!

---

## Session 19 Key Achievements

### 1. Three New Layers + Header Decrypt SUCCESS

- unPad Layer, ClientMessage Layer, EncRatchetMessage Layer
- MsgHeader fully parsed (msgMaxVersion=3, PN=0, Ns=0)

---

## Session 18 Key Achievements

### 1. BUG #18 SOLVED After Weeks of Debugging!

**Root Cause:** `envelope_len = plain_len - 2` included SMP padding
**Fix:** `envelope_len = raw_len_prefix` -- ONE LINE!

---

## Session 17 Key Achievements

### Key Consistency Debug
- Systematic verification of all Double Ratchet key derivation paths
- Chain key, message key, and header key consistency confirmed

---

## Session 16 Key Achievements

### Custom XSalsa20 + Double Ratchet Init
- Discovered SimpleX uses NON-STANDARD XSalsa20: HSalsa20(key, zeros[16]) not HSalsa20(key, nonce[0:16])
- Self-decrypt failure is BY DESIGN (asymmetric header keys)
- Double Ratchet initialization implemented

---

## Session 15 Key Achievements

### Root Cause Theory (Later Disproven in S18)
- Extensive analysis of Reply Queue E2E decryption failure
- Theory formulated but ultimately wrong (correct fix found in Session 18)

---

## Session 14 Key Achievements

### DH SECRET VERIFIED!
- X25519 Diffie-Hellman shared secret byte-identical to Haskell reference
- Verification via Python cross-check confirmed cryptographic correctness

---

## Sessions 12-13 Key Achievements

### E2E Crypto Deep Analysis
- Complete analysis of the E2E encryption pipeline
- Reply Queue decrypt path traced through all layers
- Foundation for the Bug #18 fix in Session 18

---

## Session 11 Key Achievements

### Regression and Recovery
- Code regression identified and fixed after experimental changes
- Stable baseline restored for continued E2E debugging

---

## Session 10C Key Achievements

### cmNonce Fix (Bug #17)
- Discovery: cmNonce is constructed from msgId, not a separate field
- App "connecting" status reached for the first time

---

## Session 9 Key Achievements

### Reply Queue HSalsa20 Fix (Bugs #15-16)
- HSalsa20 key derivation for Reply Queue corrected
- A_CRYPTO header AAD construction fixed

---

## Session 8 Key Achievements

### BREAKTHROUGH: AgentConfirmation Works! (Bugs #13-14)
- Payload AAD length prefix removed (Bug #13)
- chainKdf IV assignment order fixed (Bug #14)
- First successful AgentConfirmation decrypted on ESP32
- Confirmed: first native SMP implementation outside Haskell

---

## Session 7 Key Achievements

### Research and Verification
- AES-GCM verification against Haskell reference
- Tail encoding pattern documented
- Confirmed SimpleGo as the first third-party SMP implementation

---

## Session 6 Key Achievements

### SMPQueueInfo Encoding (Bugs #10-12)
- Port encoding fixed for SMP server addresses
- smpQueues count field corrected
- queueMode Nothing encoding discovered

---

## Session 5 Key Achievements

### X448 Byte-Order Breakthrough (Bug #9)
- wolfSSL X448 key exchange byte order reversed
- First successful X448 DH computation on ESP32

---

## Sessions 3-4 Key Achievements

### Wire Format Foundation (Bugs #1-8)
- 8 wire format bugs found and fixed in a single session
- E2E key length prefix, prevMsgHash, MsgHeader DH, ehBody, emHeader size
- Root KDF output order, Chain KDF IV order corrected
- Foundation established for all subsequent crypto work

---

## Sessions 1-2 Key Achievements

### Project Foundation
- TLS 1.3 connection to SMP relay server established
- ESP-IDF 5.x + mbedTLS integration on ESP32-S3
- Basic SMP protocol frame parsing implemented
- T-Deck Plus hardware (320x240, QWERTY keyboard) configured

---

## Quick Navigation

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1-8 | Wire format bugs | [Part 2](04_PART2_SESSIONS_3-4.md) |
| #9 | wolfSSL X448 byte-order | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #10-12 | SMPQueueInfo encoding | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #13-14 | AAD prefix, IV order | [Part 5](07_PART5_SESSION_8.md) |
| #15-16 | HSalsa20, A_CRYPTO | [Part 6](08_PART6_SESSION_9.md) |
| #17 | cmNonce instead of msgId | [Part 7](09_PART7_SESSION_10.md) |
| #18 | Reply Queue E2E -- SOLVED! | [Part 15](17_PART15_SESSION_18.md) |
| #19 | header_key_recv -- SOLVED! | [Part 16](18_PART16_SESSION_19.md) + [Part 17](19_PART17_SESSION_20.md) |
| #20-#26 | HELLO format + v3 | [Part 18](20_PART18_SESSION_21.md) |
| #27-#31 | E2E v3, KEM, NHK, Try-Order | [Part 19](21_PART19_SESSION_22.md) |
| #32-#39 | Bidirectional + Receipts | [Part 22](24_PART22_SESSION_25.md) |
| #40-#50 | Multi-Contact Routing | [Part 31](33_PART31_SESSION_34_BREAKTHROUGH.md) |
| #51-#57 | Contact Lifecycle | [Part 33](35_PART33_SESSION_36.md) |
| #58-#59 | SPI Bus + Chat History | [Part 34](36_PART34_SESSION_37.md) |
| #60-#61 | SPI2 Root Cause + LVGL Pool | [Part 35](37_PART35_SESSION_38.md) |
| #62-#70 | WiFi Manager | [Part 36](38_PART36_SESSION_39.md) |
| #71 | Scroll Re-Entrancy | [Part 37](39_PART37_SESSION_40.md) |

### By Topic

| Topic | Document |
|-------|----------|
| TLS 1.3, Basic SMP | [Part 1](03_PART1_INTRO_SESSIONS_1-2.md) |
| Wire format, smpEncode | [Part 2](04_PART2_SESSIONS_3-4.md), [Part 4](06_PART4_SESSION_7.md) |
| X448 Cryptography | [Part 3](05_PART3_SESSIONS_5-6.md) |
| AgentConfirmation | [Part 5](07_PART5_SESSION_8.md) |
| Reply Queue E2E | [Part 6](08_PART6_SESSION_9.md) - [Part 15](17_PART15_SESSION_18.md) |
| Double Ratchet Header | [Part 16](18_PART16_SESSION_19.md) |
| Double Ratchet Body | [Part 17](19_PART17_SESSION_20.md) |
| ConnInfo + Zstd | [Part 17](19_PART17_SESSION_20.md) |
| HELLO + v3 Format | [Part 18](20_PART18_SESSION_21.md) |
| Reply Queue Flow | [Part 19](21_PART19_SESSION_22.md) |
| FreeRTOS Multi-Task | [Part 24](26_PART24_SESSION_27.md) - [Part 26](28_PART26_SESSION_29.md) |
| Multi-Contact Architecture | [Part 30](32_PART30_SESSION_33.md) - [Part 32](34_PART32_SESSION_35.md) |
| Encrypted Chat History | [Part 34](36_PART34_SESSION_37.md) |
| SPI2 Bus Investigation | [Part 35](37_PART35_SESSION_38.md) |
| WiFi Manager | [Part 36](38_PART36_SESSION_39.md) |
| Sliding Window | [Part 37](39_PART37_SESSION_40.md) |
| Pre-GitHub Cleanup | [Part 38](40_PART38_SESSION_41.md) |
| Quality Pass | [Part 39](41_PART39_SESSION_42.md) |
| Documentation Site + SMP-in-C | [Part 40](42_PART40_SESSION_43.md) |
| All Bugs | [BUG_TRACKER](BUG_TRACKER.md) |
| Quick Reference | [QUICK_REFERENCE](QUICK_REFERENCE.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: March 8, 2026 - Session 43 (Documentation + Security Cleanup + Display Name)*
