![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleGo - Current Status (2026-03-04)

**Project:** Native SimpleX SMP Client for ESP32  
**Version:** v0.1.17-alpha  
**Archive:** See `01_SIMPLEX_PROTOCOL_INDEX.md` for complete documentation (670+ sections, 39 parts)

---

## LATEST: Consolidation and Quality Pass (2026-03-05 Session 42)

```
Pure consolidation. No new features. Production-grade code quality.

smp_handshake.c: 74 lines debug removed, zero printf              ✅
smp_globals.c dissolved: 7 symbols to owning modules               ✅
smp_app_run(): 530 to 118 lines via 5 static helpers              ✅
License headers: 47 files AGPL-3.0 + SPDX standardized            ✅
extern TODO markers resolved                                       ✅
Re-delivery log level verified correct                             ✅
UTF-8 BOM cleanup (7 files)                                        ✅

Ownership model: smp_types.h = types only, no object declarations
Build green. Device stable. Zero printf in production.

4 new lessons (#226-#229), 0 new bugs
MILESTONE 18: Production Code Quality
```

## PREVIOUS: Pre-GitHub Cleanup (2026-03-04 Session 41)

```
Three-stage pipeline: SD (encrypted) > PSRAM Cache (30) > LVGL (5 bubbles)
Crypto-separation from SPI mutex (500ms > < 10ms hold time)
LVGL pool profiling: ~1.2KB/bubble, 64KB pool effectively ~61KB
Bidirectional scroll with position correction, re-entrancy guard
1 bug (#71), 7 lessons (#214-#220), 7 files changed, 3 commits
```

---

## PREVIOUS: On-Device WiFi Manager (2026-03-03 Session 39)

```
═══════════════════════════════════════════════════════════════════════════════

  🔍🔍🔍 THE SPI2 BUS HUNT: EIGHT HYPOTHESES, ONE ROOT CAUSE 🔍🔍🔍

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Display backlight (GPIO 42, 16 levels)                    ✅        │
  │   Keyboard backlight (I2C 0x55, auto-off)                   ✅        │
  │   Settings screen with brightness sliders                   ✅        │
  │   WiFi/LWIP buffers → PSRAM (56KB freed)                    ✅        │
  │   ROOT CAUSE: SPI2 bus sharing (display + SD)               🔍        │
  │   LVGL heap = separate 64KB pool (~8 bubbles)               🔍        │
  │   MAX_VISIBLE_BUBBLES sliding window                        ✅        │
  │   SD card removed → device 100% stable for hours            ✅        │
  │                                                                         │
  │   10 commits, 2 bugs (#60-#61), 5 lessons (#205-#209)                 │
  │   8 hypotheses tested, 7 wrong, 1 correct                             │
  │                                                                         │
  │   Date: February 28 - March 1, 2026                                    │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 💾 PREVIOUS: Encrypted Chat History (2026-02-27 Session 37)

```
═══════════════════════════════════════════════════════════════════════════════

  💾💾💾 ENCRYPTED CHAT HISTORY ON SD CARD 💾💾💾

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   AES-256-GCM per-contact encryption                        ✅        │
  │   HKDF-SHA256 key derivation from master key                ✅        │
  │   Append-only file format with delivery status header       ✅        │
  │   SPI2 bus serialization (display + SD share bus)           ✅        │
  │   DMA draw buffer to internal SRAM (anti-tearing)           ✅        │
  │   Chunked rendering: 3 bubbles/tick progressive loading     ✅        │
  │   Contact list redesign (28px cards, search, bottom bar)    ✅        │
  │                                                                         │
  │   4 sub-sessions, 2 commits, 2 bugs fixed (#58-#59)                   │
  │   2 new lessons (#203-#204)                                            │
  │                                                                         │
  │   Date: February 25-27, 2026                                           │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🔄 PREVIOUS: Contact Lifecycle (2026-02-25 Session 36)

```
═══════════════════════════════════════════════════════════════════════════════

  🔄🔄🔄 CONTACT LIFECYCLE: DELETE, RECREATE, ZERO COMPROMISE 🔄🔄🔄

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Complete Contact Lifecycle: Create -> Chat -> Delete -> Recreate     │
  │   No erase-flash required!                                             │
  │                                                                         │
  │   NTP timestamps in chat bubbles                             ✅        │
  │   Contact name from ConnInfo JSON                            ✅        │
  │   4-key NVS cleanup on delete (rat/peer/hand/rq)             ✅        │
  │   KEY-HELLO race condition fixed (TaskNotification)          ✅        │
  │   UI cleanup on delete (bubbles + QR reset)                  ✅        │
  │   Contact list redesign with long-press menu                 ✅        │
  │                                                                         │
  │   4 sub-sessions, 12 commits, 7 bugs fixed (#51-#57)                  │
  │   10 new lessons (#193-#202)                                           │
  │                                                                         │
  │   Date: February 25, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏁 PREVIOUS: Multi-Contact Victory (2026-02-24 Session 35)

```
═══════════════════════════════════════════════════════════════════════════════

  🏁🏁🏁 MULTI-CONTACT VICTORY — ALL PLANNED BUGS FIXED 🏁🏁🏁

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   2 contacts simultaneously connected                                  │
  │   Bidirectional messages for both contacts                             │
  │   Delivery receipts for both contacts                                  │
  │   Per-contact chat filter (messages only in correct chat)              │
  │   Tested after erase-flash with fresh handshakes                       │
  │   20+ messages exchanged                                               │
  │                                                                         │
  │   Fixes: 35a-35h across 10 files                                      │
  │   Root Cause: "wrong slot active" (same as Session 34)                 │
  │                                                                         │
  │   Date: February 24, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏆 SESSION 34 Day 2: Multi-Contact Bidirectional (2026-02-24)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 MULTI-CONTACT BIDIRECTIONAL ENCRYPTED MESSAGING 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   HISTORIC MILESTONE: First multi-contact bidirectional encrypted      │
  │   messenger on a microcontroller                                       │
  │                                                                         │
  │   Contact 0: ESP32 <-> Phone  Bidirectional  Encrypted  ✅            │
  │   Contact 1: ESP32 <-> Phone  Bidirectional  Encrypted  ✅            │
  │                                                                         │
  │   11 bugs found and fixed (#40-#50) across 6 phases                   │
  │   All bugs: ONE pattern (global state -> per-contact routing)          │
  │                                                                         │
  │   KEY Fix + Ghost Write + Global State + Encoder + Index + Crypto      │
  │   Pattern established for contacts 2-127                               │
  │                                                                         │
  │   Date: February 24, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏗️ SESSION 34 Day 1: Multi-Contact Architecture (2026-02-23)

```
═══════════════════════════════════════════════════════════════════════════════

  🏗️🏗️🏗️ MULTI-CONTACT — FROM SINGLETON TO PER-CONTACT 🏗️🏗️🏗️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   8 Commits — most productive session yet                              │
  │                                                                         │
  │   Production Cleanup: zero private keys in logs                        │
  │   Runtime Add-Contact: NET_CMD via Ring Buffer                         │
  │   Per-Contact 42d: 128-bit bitmap                                      │
  │   UI: [+ New Contact] with auto-naming                                 │
  │   Reply Queue Array: 128 slots, ~49KB PSRAM                           │
  │   SMP v7 Signing Fix: 1-byte session prefix                           │
  │   PSRAM Total: ~158KB / 8MB (1.9%)                                    │
  │                                                                         │
  │   Open Bug: KEY Command rejected by server                             │
  │   Handover: Claude Code for Haskell analysis                           │
  │                                                                         │
  │   Date: February 23, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🖥️ SESSION 32: "The Demonstration" (2026-02-20)

```
═══════════════════════════════════════════════════════════════════════════════

  🖥️🖥️🖥️ "THE DEMONSTRATION" — FROM PROTOCOL TO MESSENGER 🖥️🖥️🖥️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Before: Bidirectional chat only visible in Serial Monitor            │
  │   After:  Full messenger with Bubbles, Keyboard,                       │
  │           Delivery Status, Contact List                                │
  │                                                                         │
  │   7 Keyboard-to-Chat Steps completed                                   │
  │   Delivery Status: ... → ✓ → ✓✓ → ✗                                   │
  │   LVGL Display Refresh Fix                                              │
  │   Multi-Contact Analysis (128 Contacts, 68KB PSRAM)                    │
  │   Navigation Stack Fix                                                  │
  │   System 2+ hours stable                                               │
  │                                                                         │
  │   Date: February 19-20, 2026                                           │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎉 SESSION 31: Bidirectional Chat Restored! (2026-02-18)

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 BIDIRECTIONAL CHAT RESTORED — ROOT CAUSE FOUND! 🎉🎉🎉

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   T6: Baseline-Test Bidirectional ✅ RESOLVED (was ❌ in Session 30)   │
  │                                                                         │
  │   Root Cause: txCount==1 filter in Drain-Loop discarded batched         │
  │   server responses. MSG in TX2 was silently dropped.                    │
  │                                                                         │
  │   6 Fixes applied, 5 hypotheses tested, 1 Wizard analysis              │
  │   Evgeny guidance integrated (subscriptions, keep-alive)               │
  │                                                                         │
  │   MILESTONE 7: Multi-Task Bidirectional Chat ✅                        │
  │                                                                         │
  │   Date: February 18, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🔍 SESSION 30: Intensive Debug Session (2026-02-18)

```
═══════════════════════════════════════════════════════════════════════════════

  🔍🔍🔍 INTENSIVE DEBUG SESSION — 10 HYPOTHESES, 14 FIXES 🔍🔍🔍

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   T5: Keyboard-Send Integration ✅ PASSED                              │
  │   T6: Baseline-Test Bidirectional ❌ UNRESOLVED                        │
  │                                                                         │
  │   Problem: App→ESP32 messages never arrive after successful SUB        │
  │   10 hypotheses systematically excluded                                │
  │   14 fixes and diagnostics applied                                     │
  │   5 Wizard (Claude Code) analyses completed                            │
  │                                                                         │
  │   SMP v6 → v7 Upgrade: 33 bytes saved per transmission                 │
  │   Expert question sent to Evgeny Poberezkin                            │
  │                                                                         │
  │   Date: February 16-18, 2026                                           │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏆 SESSION 29: Multi-Task BREAKTHROUGH! (2026-02-16)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 MULTI-TASK ARCHITECTURE — BREAKTHROUGH! 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Complete encrypted messaging pipeline over FreeRTOS Tasks!           │
  │                                                                         │
  │   - Network Task (Core 0, PSRAM): SSL read loop, command handler       │
  │   - Main Task (Internal SRAM): Parse, decrypt, NVS, 42d handshake      │
  │   - Ring Buffer IPC: net_to_app (37KB), app_to_net (1KB)               │
  │                                                                         │
  │   First message "Hello from ESP32!" sent via new architecture!         │
  │                                                                         │
  │   CRITICAL: PSRAM stacks + NVS writes = CRASH!                         │
  │                                                                         │
  │   Date: February 16, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## ✅ SESSION 28: Phase 2b Success! (2026-02-15)

```
═══════════════════════════════════════════════════════════════════════════════

  ✅✅✅ FREERTOS TASK ARCHITECTURE — PHASE 2b COMPLETE! ✅✅✅

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Three FreeRTOS Tasks running in parallel!                            │
  │                                                                         │
  │   - Network Task (Core 0, 12KB stack, Priority 7)                      │
  │   - App Task (Core 1, 16KB stack, Priority 6)                          │
  │   - UI Task (Core 1, 8KB stack, Priority 5)                            │
  │                                                                         │
  │   Key Fix: ALL non-DMA resources moved to PSRAM                        │
  │   Internal Heap: ~40KB preserved for mbedTLS/WiFi                      │
  │                                                                         │
  │   Date: February 15, 2026                                              │
  │   Duration: ~4 hours                                                   │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## ⚠️ SESSION 27: Architecture Investigation (2026-02-15)

```
═══════════════════════════════════════════════════════════════════════════════

  ⚠️⚠️⚠️ FREERTOS ARCHITECTURE INVESTIGATION ⚠️⚠️⚠️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Phase 1: Folder restructure ✅ WORKS                                 │
  │   Phase 2: FreeRTOS Tasks ❌ BROKE main (90KB RAM at boot)             │
  │   Phase 3: Network Task Migration — branch polluted                    │
  │                                                                         │
  │   ROOT CAUSE: Tasks started at boot, starved TLS/WiFi                  │
  │   SOLUTION: Start tasks AFTER connection, not at boot                  │
  │                                                                         │
  │   Date: February 14-15, 2026                                           │
  │   Lessons Learned: 17 new (137 total)                                  │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🗄️ SESSION 26: Persistence! (2026-02-14)

```
═══════════════════════════════════════════════════════════════════════════════

  🗄️🗄️🗄️ RATCHET STATE PERSISTENCE! 🗄️🗄️🗄️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   MILESTONE 6: ESP32 survives reboot without losing crypto state!      │
  │                                                                         │
  │   - Ratchet state restored from NVS flash                              │
  │   - Queue credentials persisted                                        │
  │   - Delivery receipts work after reboot                                │
  │   - Write-Before-Send: 7.5ms verified                                  │
  │                                                                         │
  │   Date: February 14, 2026 (Valentine's Day Part 2)                     │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   NVS Capacity: 150+ contacts                                          │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 SESSION 25: Bidirectional Chat + Receipts! (2026-02-14)

```
═══════════════════════════════════════════════════════════════════════════════

  🎯🎯🎯 BIDIRECTIONAL ENCRYPTED CHAT + DELIVERY RECEIPTS! 🎯🎯🎯

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   MILESTONE 3: First App message decrypted on ESP32                    │
  │   MILESTONE 4: Bidirectional encrypted chat ESP32 ↔ SimpleX App        │
  │   MILESTONE 5: Delivery receipts (✓✓) working!                         │
  │                                                                         │
  │   Date: February 14, 2026 (Valentine's Day!)                           │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Refactoring: main.c 2440 → 611 lines (−75%)                          │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏆 MILESTONE #2: First Chat Message! (2026-02-11 Session 24)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER! 🏆🏆🏆

  SimpleX App shows: "Hello from ESP32!"
  Date: February 11, 2026
  Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎉 MILESTONE #1: CONNECTED! (2026-02-08 Session 23)

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉🎉🎉

  SimpleX App shows: "ESP32 — Connected"
  Date: February 8, 2026 ~17:36 UTC

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 Current Status

```
BIDIRECTIONAL COMMUNICATION:
ESP32 ──► "Hello from ESP32!" ──► App ✅ WORKS!
ESP32 ◄── "Hello?" ◄── App ✅ WORKS!
App shows ✓✓ for received messages ✅ WORKS!
```

---

## ✅ What Works — EVERYTHING!

| Component | Status | Session |
|-----------|--------|---------|
| TLS 1.3 | ✅ | S1-3 |
| SMP Handshake | ✅ | S4-8 |
| Queue Creation | ✅ | S4-8 |
| Invitation Parsing | ✅ | S4-8 |
| X3DH Key Agreement | ✅ | S4-8 |
| Double Ratchet Init | ✅ | S4-8 |
| X448 DH | ✅ | S5 |
| HKDF-SHA512 | ✅ | S4-8 |
| AES-GCM Encryption | ✅ | S4-8 |
| Wire Format | ✅ | S4 |
| Padding | ✅ | S4-8 |
| AAD | ✅ | S8 |
| IV Order | ✅ | S8 |
| AgentConfirmation | ✅ | S8 |
| E2E Encryption | ✅ | S18 |
| Double Ratchet Header Decrypt | ✅ | S19 |
| Double Ratchet Body Decrypt | ✅ | S20 |
| Peer Profile Parsing | ✅ | S20 |
| v3 EncRatchetMessage | ✅ | S21 |
| HELLO Exchange | ✅ | S21-23 |
| KEY Command | ✅ | S23 |
| Reply Queue Setup | ✅ | S22-23 |
| **CONNECTED** | ✅ | **S23** |
| A_MSG Send | ✅ | S24 |
| ChatMessage JSON | ✅ | S24 |
| ACK Protocol | ✅ | S24 |
| **First Chat Message** | ✅ | **S24** |
| A_MSG Receive | ✅ | S25 |
| Ratchet State Persistence | ✅ | S25 |
| **Bidirectional Chat** | ✅ | **S25** |
| **Delivery Receipts (✓✓)** | ✅ | **S25** |

**Result:** Full bidirectional encrypted chat with receipts! 🎯

---

## 📊 Session 25 — The Valentine's Day Session

### Phase 1: Massive Refactoring
```
main.c: 2440 → 611 lines (−75%)

New modules:
  - smp_ack.c/h      ACK handling
  - smp_wifi.c/h     WiFi initialization
  - smp_e2e.c/h      E2E envelope decryption
  - smp_agent.c/h    Agent protocol layer
```

### Phase 2: Bidirectional Bug Fixes (8 bugs)
- Nonce offset: 14 → 13 (brute-force discovered!)
- Ratchet state: Copy → Pointer (persistence)
- Chain KDF: Relative → Absolute skip
- txCount parser: Hardcoded → Variable
- Heap overflow: malloc(256) → dynamic
- Receipt count: Word16 → Word8
- Receipt rcptInfo: Word32 → Word16
- NULL guard: contact check for Reply Queue

### Phase 3: Delivery Receipts
- Receipt wire format documented
- count=Word8, rcptInfo=Word16 (corrected)
- App shows ✓✓ for ESP32-received messages!

---

## 📊 Session 24 — First Chat Message

### Key Achievements
- First A_MSG sent: "Hello from ESP32!"
- ChatMessage JSON format discovered
- Q_B Ratchet decrypt working
- ACK protocol documented
- PQ-Kyber graceful degradation verified

---

## 📊 Session 23 — CONNECTED

### Key Achievements
- ZERO new bugs (31 total sufficient)
- Complete 7-step handshake verified
- Role clarification: ESP32=Bob, App=Alice
- KEY command on Reply Queue
- TLS reconnect + SUB + KEY sequence

---

## 📋 Complete Bug List (50 Bugs - ALL FIXED!)

| Sessions | Bugs | Category |
|----------|------|----------|
| S4 | #1-8 | Wire format, length prefixes, KDF order |
| S5 | #9 | wolfSSL X448 byte order |
| S6 | #10-12 | SMPQueueInfo encoding |
| S8 | #13-14 | AAD prefix, IV assignment |
| S9 | #15-16 | HSalsa20, A_CRYPTO |
| S10C | #17 | cmNonce vs msgId |
| S12-18 | #18 | Reply Queue E2E (ONE LINE FIX!) |
| S19-20 | #19 | header_key_recv overwritten |
| S21 | #20-26 | HELLO format + v3 EncRatchetMessage |
| S22 | #27-31 | E2E v3, KEM parser, NHK promotion |
| S23 | ZERO | CONNECTED! |
| S24 | ZERO | First Chat Message! |
| S25 | #32-39 | Bidirectional + Receipts |
| S34b | #40-50 | Multi-Contact Routing (11 bugs!) |

**All 50 bugs FIXED!**

---

## 📐 Quick Reference - Constants

```c
// Padding sizes
#define E2E_ENC_CONN_INFO_LENGTH    14832  // AgentConfirmation
#define E2E_ENC_AGENT_MSG_LENGTH    15840  // HELLO, A_MSG, etc.
#define E2E_ENC_CONFIRMATION_LENGTH 15904  // Outer ClientMessage

// Structure sizes
#define EM_HEADER_SIZE_V2           123    // EncMessageHeader (v2)
#define EM_HEADER_SIZE_V3           124    // EncMessageHeader (v3)
#define MSG_HEADER_SIZE             88     // MsgHeader (padded)
#define HELLO_SIZE                  12     // HELLO Plaintext
#define E2E_PARAMS_SIZE             140    // SndE2ERatchetParams
#define RCAD_SIZE                   112    // Associated Data (rcAD)
#define PAYLOAD_AAD_SIZE_V2         235    // rcAD + emHeader (v2)
#define PAYLOAD_AAD_SIZE_V3         236    // rcAD + emHeader (v3)

// Versions
#define AGENT_VERSION               7      // 0x0007
#define E2E_VERSION                 2      // 0x0002
#define RATCHET_VERSION             3      // v3 format
```

---

## 📐 Quick Reference - Session 25 Discoveries

### Nonce Offset for Reply Queue Regular Messages
```
WRONG: Offset 14 (Session 24 assumption)
RIGHT: Offset 13 (Brute-force discovered)

Message format: [12B header][nonce@13][ciphertext]
```

### Ratchet State Persistence
```c
// WRONG — changes lost:
ratchet_state_t rs = *ratchet_get_state();

// CORRECT — changes persist:
ratchet_state_t *rs = ratchet_get_state();
```

### Receipt Wire Format
```
A_RCVD ('V') payload:
  'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]

AMessageReceipt:
  agentMsgId(8B Int64 BE) + msgHash(1+32B SHA256) + rcptInfo(Word16)
```

---

## 📐 Quick Reference - Wire Formats (Historical)

### AgentConfirmation (S8 Breakthrough!)
```
[2B version=7][1B 'C'][1B '1'][140B E2EParams][Tail encConnInfo]
```

### EncRatchetMessage
```
v2: [1B len=123][123B emHeader][16B authTag][Tail payload]
v3: [2B len=124][124B emHeader][16B authTag][Tail payload]
```

### Payload AAD - CORRECTED in S8!
```
[112B rcAD][emHeader]  ← NO length prefix before emHeader!
```

---

## 📐 Quick Reference - KDF

### Chain KDF Output (96 bytes)
```
Bytes 0-31:  next_chain_key
Bytes 32-63: message_key
Bytes 64-79: MESSAGE_IV (iv1)  ← FOR PAYLOAD!
Bytes 80-95: HEADER_IV (iv2)   ← FOR HEADER!
```

---

## 📝 Key Learnings (Selection)

1. **Wire Format ≠ Crypto Format** - Length prefixes for serialization, not always for AAD (S8)
2. **Haskell Parser Awareness** - `largeP` removes length prefix from parsed object (S8)
3. **Python Verification** - Essential for debugging crypto operations (S4-8)
4. **Community Support** - SimpleX developers are helpful and responsive (S7)
5. **SimpleX uses NON-STANDARD XSalsa20** - HSalsa20(key, zeros[16]) not nonce[0:16] (S16)
6. **Self-decrypt failure is BY DESIGN** - Asymmetric header keys (S16)
7. **Nonce offset varies by message type** - Contact Queue vs Reply Queue (S25)
8. **Ratchet state must persist** - Use pointer, not copy (S25)
9. **App's own messages are best protocol reference** - Byte comparison beats source analysis (S25)

---

## 📁 Documentation Files

| File | Description |
|------|-------------|
| `01_SIMPLEX_PROTOCOL_INDEX.md` | Navigation index |
| `02_SIMPLEX_STATUS.md` | This file - quick status |
| `README.md` | Project overview |
| `BUG_TRACKER.md` | All 71 bugs, 220 lessons |
| `QUICK_REFERENCE.md` | Constants, wire formats |
| `03-39_PART*.md` | Sessions 1-40 documentation |

---

## 🎯 Milestone Overview

| # | Milestone | Date | Session |
|---|-----------|------|---------|
| 0 | 🎉 AgentConfirmation | 2026-01-27 | 8 |
| 1 | 🎉 CONNECTED | 2026-02-08 | 23 |
| 2 | 🏆 First A_MSG | 2026-02-11 | 24 |
| 3 | 📥 App→ESP32 Decrypt | 2026-02-14 | 25 |
| 4 | 🔄 Bidirectional Chat | 2026-02-14 | 25 |
| 5 | ✓✓ Delivery Receipts | 2026-02-14 | 25 |
| **6** | **🗄️ Ratchet Persistence** | **2026-02-14** | **26** |
| **7** | **🎉 Multi-Task Bidirectional** | **2026-02-18** | **31** |
| **8** | **🖥️ Full Messenger UI** | **2026-02-19** | **32** |
| **9** | **🏗️ Multi-Contact Architecture** | **2026-02-23** | **34** |
| **10** | **🏆 Multi-Contact Bidirectional** | **2026-02-24** | **34b** |
| **11** | **🏁 Multi-Contact Chat Filter** | **2026-02-24** | **35** |
| **12** | **🔄 Contact Lifecycle** | **2026-02-25** | **36** |
| **13** | **💾 Encrypted Chat History** | **2026-02-27** | **37** |
| **14** | **🔍 Backlight + SPI Root Cause** | **2026-03-01** | **38** |
| **15** | **📡 On-Device WiFi Manager** | **2026-03-03** | **39** |
| **16** | **Sliding Window Chat History** | **2026-03-04** | **40** |
| **17** | **🧹 Pre-GitHub Stabilization** | **2026-03-04** | **41** |
| **18** | **🏗️ Production Code Quality** | **2026-03-05** | **42** |

---

## Next Steps (Session 43)

1. Security cleanup: 5 logging categories with production-sensitive data
2. Docusaurus 3 restructure at wiki.simplego.dev
3. README rewrite (positive, professional)
4. SD card on SPI3 bus (Bug #60)
5. German umlaut fallback fonts

**Status:** Production code quality achieved. Documentation restructure next.

---

*Status updated: 2026-03-05 Session 42 -- Consolidation and Quality Pass*  
*History: S8 Breakthrough -> S23 CONNECTED -> S24 First MSG -> S25 Bidirectional -> S26 Persistence -> S27 Architecture -> S28 Tasks -> S29 Multi-Task -> S30 Debug -> S31 RESOLVED -> S32 Messenger UI -> S34 Multi-Contact -> S34b BIDIRECTIONAL -> S35 VICTORY -> S36 LIFECYCLE -> S37 HISTORY -> S38 SPI HUNT -> S39 WIFI -> S40 WINDOW -> S41 CLEANUP -> S42 QUALITY*
