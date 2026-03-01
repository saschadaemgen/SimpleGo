![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 22: Session 25
# 🎯 Bidirectional Encrypted Chat + Delivery Receipts!

**Document Version:** v1  
**Date:** 2026-02-13/14 Session 25 (Valentinstag-Session)  
**Status:** ✅ MILESTONES 3, 4, 5 Achieved!  
**Previous:** Part 21 - Session 24 (First Chat Message)

---

## 🎯 THREE MILESTONES IN ONE SESSION!

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

## 425. Session 25 Overview

### 425.1 Starting Point

After Session 24 we had:
- ✅ ESP32 → App: "Hello from ESP32!" works
- ❌ App → ESP32: Blocked — server has 0 messages on Q_B
- ❌ Code quality: main.c = 2440 lines monolith, 40% debug logging
- ❌ ACK code: 3x duplicated
- ❌ E2E code: 2x duplicated

**Session 24 Root Cause Hypothesis:** App doesn't mark connection as Active due to format error.

### 425.2 Session 25 Goals

1. Refactor monolithic main.c into clean modules
2. Fix bidirectional communication (App → ESP32)
3. Implement delivery receipts for ✓✓

### 425.3 Session 25 Results

| Goal | Result |
|------|--------|
| Refactoring | ✅ main.c 2440 → 611 lines (−75%), 4 new modules |
| App → ESP32 | ✅ **MILESTONE 3+4!** Bidirectional encrypted chat! |
| Delivery Receipts | ✅ **MILESTONE 5!** App shows ✓✓ |
| Bugs Fixed | 8 critical bugs |
| Lessons Learned | #111, #112 |

### 425.4 Session Statistics

| Metric | Value |
|--------|-------|
| Start | 2026-02-13 evening |
| End | 2026-02-14 morning |
| Duration | ~16 hours |
| Tasks | 46a-d, 47a-g, 48a, 49a-b |
| Milestones | 3 (Nr. 3, 4, 5) |
| Bugs Fixed | 8 |

---

## 426. Phase 1: Refactoring (Tasks 46a–46d)

### 426.1 Task 46a — Debug Cleanup + ACK Consolidation

**Executed by:** Hasi 🐰

**Removed (~800 lines):**
- Reverse-engineering hex dumps (RAW server_plain, SENDER_PUB FULL, etc.)
- SPKI pattern searches, Maybe marker searches
- Header decrypt fallback tries 3-8 (kept only Try 1 HKr + Try 2 NHKr)
- All Task diagnostic blocks (38b, 41a, 42a, 45b)

**Consolidated:**
- ACK code 3x → 1x in `smp_send_ack()`

### 426.2 Task 46b — WiFi Extraction

**Executed by:** Hasi 🐰

**New files:**
- `smp_wifi.c` (52 lines)
- `smp_wifi.h` (13 lines)

**Extracted:**
- `wifi_init()`, `wifi_event_handler()`, WiFi includes

### 426.3 Task 46c — E2E Deduplication

**Executed by:** Hasi 🐰

**New files:**
- `smp_e2e.c` (249 lines)
- `smp_e2e.h` (45 lines)

**Consolidated:**
- 2x duplicated E2E decrypt → 1x `smp_e2e_decrypt_envelope()`
- Length-prefix detection, SPKI extraction, multi-method decrypt

**Runtime Fix:**
- `malloc(256)` → `malloc(eh_body_len + 16)` (heap overflow with PQ headers)

### 426.4 Task 46d — Agent Protocol Layer

**Executed by:** Hasi 🐰

**New files:**
- `smp_agent.c` (590 lines)
- `smp_agent.h` (48 lines)

**Features:**
- PrivHeader parsing ('K' PHConfirmation, '_' PHEmpty)
- Agent tag dispatch ('H' HELLO, 'M' A_MSG, 'I' ConnInfo, 'D' Reply)
- Ratchet header+body decrypt orchestration
- Zstd decompression for ConnInfo
- Chat text extraction from A_MSG

### 426.5 Refactoring Results

```
main.c before:  2440 lines (monolith)
main.c after:    611 lines (session loop + init)
Reduction:      −75% (1829 lines extracted)

New Modules:
  - smp_ack.c/h      ACK handling
  - smp_wifi.c/h     WiFi initialization
  - smp_e2e.c/h      E2E envelope decryption
  - smp_agent.c/h    Agent protocol layer

Build Fix:      1 (decrypt_client_msg extern declaration)
Runtime Fix:    1 (heap overflow with PQ headers)
Hardware Test:  ✅ "Hello from ESP32!" sent, server accepts
```

---

## 427. Phase 2: Bidirectional Bugfix (Tasks 47a–47g)

### 427.1 Task 47a — Haskell Source Analysis

**Executed by:** Claude Code 🔒

**Key Discoveries:**

**Connection State Machine:**
```
ConnNew → ConnConfirmed → ConnSecured → ConnReady → ConnSndReady
                                                  ↓
                                              ConnReady (both directions)
```

**Checkmarks:**
- One ✓ = `CISSndSent` = Server accepted SEND
- Two ✓✓ = `CISSndRcvd` = Recipient sent receipt

**Important:** HELLO receipt does NOT directly set Active — RcvQueue is set Active on ANY incoming message.

### 427.2 Key Insight: App Sends on Q_B, Not Q_A

**Analysis by:** Mausi 👑

```
Q_A (Contact Queue):  One-time handshake channel
Q_B (Reply Queue):    Permanent message channel App → ESP32

The App sends correctly on Q_B — our error was waiting on Q_A.
```

### 427.3 Task 47d — txCount Parser Fix

**Bug:** Parser hardcoded `if (resp[p] != 1) continue;` — dropped all messages after re-SUB because server sends txCount=2+.

**Fix:** Read txCount as sequence counter, don't validate.

**Result:** 16279-byte block no longer dropped. But: still no MSG on expected queue.

### 427.4 Task 47e — X-Ray Diagnostics

**Executed by:** Hasi 🐰

**Findings:**
- Layer 1 (Server Decrypt): ✅ WORKS — `shared_secret: 32b06070` → OK
- Layer 2 (E2E Decrypt): ❌ sender_pub always same → wrong key
- Envelope byte 12 = `0x30` = suspected `corrId='0'` → cache used
- Cache contains Contact Queue key instead of Reply Queue key

### 427.5 Task 47g — Brute-Force Nonce Offset Scan — MILESTONE 3!

**Executed by:** Hasi 🐰 after Mausi's theory

**Hypothesis (Mausi):** Byte `0x30` is NOT a corrId — it's the first byte of the nonce. Parser reads at wrong position.

**Result:**
```
=== BRUTE-FORCE NONCE OFFSET SCAN ===
✅✅✅ DECRYPT OK at nonce_offset=13! pt_len=16000
```

**The Truth:** Regular Q_B messages have format `[12B header][nonce@13][ciphertext]`. Byte [12] was never a corrId tag — it was a random nonce byte that looked like ASCII `'0'`.

**Follow-up Decryption (immediate):**
```
🎉 E2E LAYER 2 DECRYPT SUCCESS! — 16000 bytes
🎉 HEADER DECRYPT SUCCESS! (AdvanceRatchet)
🎉 PHASE 2b BODY DECRYPT SUCCESS!
   Plaintext: 4d 00 00 00 00 00 00 00 02 ... = 'M' = A_MSG!
```

**MILESTONE 3 ACHIEVED: First App message decrypted on ESP32!**

---

## 428. Phase 2b: Ratchet State Persistence (Task 48a)

### 428.1 Problem After Milestone 3

Only MSG #0 (AdvanceRatchet) decrypts. MSG #1+ (SameRatchet) fail with `ret=-18`.

### 428.2 Root Cause

```c
// WRONG: Works on copy
ratchet_state_t rs = *ratchet_get_state();
// ... decrypt ...
// chain_key_recv updated in rs but never written back!

// CORRECT: Works on pointer
ratchet_state_t *rs = ratchet_get_state();
// ... decrypt ...
// chain_key_recv persists in global state!
```

**Additional Bug:** Chain KDF skip calculated relative instead of absolute.

### 428.3 Fix

**Fix 1:** `ratchet_state_t *rs = ratchet_get_state()` (pointer instead of copy)
**Fix 2:** `skip_from = msg_num_recv` (absolute instead of relative)

**Result:**
```
msg_num_recv=1 → ✅ SameRatchet OK
msg_num_recv=2 → ✅ SameRatchet OK
msg_num_recv=3 → ✅ SameRatchet OK
msg_num_recv=4 → ✅ "Hello?" ← FIRST APP MESSAGE READ!
```

**MILESTONE 4 ACHIEVED: Bidirectional encrypted chat!**
**Timestamp:** 2026-02-14 00:25

---

## 429. Phase 3: Delivery Receipts (Tasks 49a–49b)

### 429.1 Task 49a — Receipt Format Analysis

**Executed by:** Claude Code 🔒

**Wire Format:**
```
A_RCVD ('V'):
  'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]
  
AMessageReceipt:
  agentMsgId(8B Int64 BE) + msgHash(1+32B SHA256) + rcptInfo(Large Word16)
```

- Receipt only sent if `connAgentVersion >= v4`
- Sending triggered by `ackMessage(connId, msgId, Just rcptInfo)`
- App validates hash → `MROk` or `MRBadMsgHash`

### 429.2 Task 49b — Receipt Implementation

**Executed by:** Hasi 🐰

**New Functions:**

| Function | File | Purpose |
|----------|------|---------|
| `encrypt_and_send_agent_msg()` | smp_handshake.c | Shared pipeline: Ratchet → E2E → SEND |
| `build_receipt_message()` | smp_handshake.c | Build A_RCVD payload |
| `send_receipt_message()` | smp_handshake.c | Orchestrate receipt send |
| `peer_send_receipt()` | smp_peer.c | Wrapper with reconnect logic |

### 429.3 Bug 1: Guru Meditation Crash

**Symptom:** `EXCVADDR: 0x00000080` (LoadProhibited)
**Cause:** `contact=NULL` for Reply Queue messages
**Fix:** NULL guard + `contacts_db.contacts[0]`

### 429.4 Bug 2: App Shows Only ✓ Instead of ✓✓

**Analysis Method:** Byte-for-byte comparison with App's own receipt

**Finding:** Our receipt 90 bytes, App's receipt 87 bytes

| Field | Our Receipt (wrong) | App's Receipt (correct) | Delta |
|-------|---------------------|------------------------|-------|
| count | Word16 `00 01` | Word8 `01` | +1 byte |
| rcptInfo | Word32 `00 00 00 00` | Word16 `00 00` | +2 bytes |
| **Total** | **90 bytes** | **87 bytes** | **+3 bytes** |

**Root Cause:** App parses count as Word8 → reads `0x00` → count=0 → "no receipts"
**Fix:** count=Word8, rcptInfo=Word16 Large

**MILESTONE 5 ACHIEVED: App shows ✓✓ for ESP32-received messages!**
**Timestamp:** 2026-02-14 ~10:00

---

## 430. Complete Bug List Session 25

| # | Bug | Severity | Symptom | Root Cause | Fix | Task |
|---|-----|----------|---------|------------|-----|---------|
| 1 | Heap Overflow PQ Headers | Critical | Crash with large headers | `malloc(256)` too small | `malloc(eh_body_len + 16)` | 46c |
| 2 | txCount Hardcoded | Critical | MSG after re-SUB dropped | `if (resp[p] != 1) continue` | Read txCount as counter | 47d |
| 3 | Nonce Offset Wrong | Critical | E2E Decrypt MAC Fail | Offset 14 instead of 13 | Brute-force → permanent 13 | 47g |
| 4 | Ratchet State Copy | Critical | Only MSG #0 decryptable | Pointer instead of copy | `*rs = ratchet_get_state()` | 48a |
| 5 | Chain KDF Skip Relative | Critical | msg_num wrongly calculated | Relative instead of absolute | `skip_from = msg_num_recv` | 48a |
| 6 | Receipt count=Word16 | High | App ignores receipt | Word16 instead of Word8 | 1 byte instead of 2 | 49b |
| 7 | Receipt rcptInfo=Word32 | High | 3 bytes too many | Word32 instead of Word16 | Large=Word16 | 49b |
| 8 | NULL contact Reply Queue | High | Guru Meditation Crash | contact=NULL not checked | NULL guard | 49b |

---

## 431. Lessons Learned

### #111: The App's Own Protocol Messages Are the Best Reference

Instead of analyzing Haskell source, direct byte comparison with the App's receipt revealed the exact wire format in minutes. 90 vs 87 bytes → 3 bytes difference → two encoding errors identified and fixed.

**Reverse-engineering through observed behavior beats source code analysis when the reference implementation is already communicating.**

### #112: Test NULL Pointers in Extended Code Paths

Existing function parameters that were never used before (here: `contact` in `handle_empty`) can be NULL. New features that use these parameters for the first time must check NULL safety.

---

## 432. Task Overview Session 25

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| 46a | Hasi | Refactor | Debug cleanup + ACK consolidation | ✅ −800 lines |
| 46b | Hasi | Refactor | WiFi extraction | ✅ smp_wifi.c/h |
| 46c | Hasi | Refactor | E2E deduplication | ✅ smp_e2e.c/h + heap fix |
| 46d | Hasi | Refactor | Agent protocol layer | ✅ smp_agent.c/h |
| 47a | Claude Code | Analysis | Haskell connection state | ✅ State machine documented |
| 47c | Hasi | Code | ACK fix + re-SUB | ✅ Implemented |
| 47d | Hasi | Fix | txCount parser | ✅ Variable txCount |
| 47e | Hasi | Debug | X-ray diagnostics | ✅ Wrong key identified |
| 47f | Hasi | Fix | Cache fix (failed) | ❌ Wrong hypothesis |
| 47g | Hasi | Fix | Brute-force nonce scan | ✅ **MILESTONE 3!** |
| 48a | Hasi | Fix | Ratchet state persistence | ✅ **MILESTONE 4!** |
| 49a | Claude Code | Analysis | Receipt format | ✅ Wire format documented |
| 49b | Hasi | Feature | Receipt implementation | ✅ **MILESTONE 5!** |

---

## 433. Git Commits Session 25

| # | Commit Message |
|---|----------------|
| 1 | `refactor(ack): extract ACK handling into dedicated module` |
| 2 | `refactor(wifi): extract WiFi initialization into dedicated module` |
| 3 | `refactor(e2e): extract E2E envelope decryption into dedicated module` |
| 4 | `refactor(agent): extract Agent Protocol Layer into dedicated module` |
| 5 | `refactor(main): reduce main.c from 2440 to 611 lines (-75%)` |
| 6 | `fix(transport): accept variable txCount in SMP parser` |
| 7 | `fix(e2e): correct nonce offset for Reply Queue regular messages` |
| 8 | `fix(ratchet): persist chain_key_recv after body decrypt` |
| 9 | `feat(receipt): add delivery receipts for double-check marks` |
| 10 | `fix(build): add missing header declarations for receipt functions` |

---

## 434. Files Changed Session 25

| File | Changes |
|------|---------|
| `main/main.c` | 2440 → 611 lines (−75%), session loop only |
| `main/smp_ack.c` | **NEW** — ACK handling |
| `main/smp_wifi.c` | **NEW** — WiFi initialization |
| `main/smp_e2e.c` | **NEW** — E2E envelope decryption |
| `main/smp_agent.c` | **NEW** — Agent protocol layer |
| `main/smp_queue.c` | txCount fix, nonce offset fix |
| `main/smp_ratchet.c` | State persistence fix, chain KDF fix |
| `main/smp_handshake.c` | Receipt functions |
| `main/smp_peer.c` | `peer_send_receipt()` |
| `main/include/*.h` | Corresponding headers |

---

## 435. Milestone Overview

| # | Milestone | Date | Session | Description |
|---|-----------|------|---------|-------------|
| 1 | 🎉 CONNECTED | 2026-02-08 | 23 | First SimpleX connection on microcontroller |
| 2 | 🏆 First A_MSG | 2026-02-11 | 24 | First chat message from microcontroller |
| 3 | 📥 App→ESP32 Decrypt | 2026-02-14 | 25 | First App message decrypted on ESP32 |
| 4 | 🔄 Bidirectional Chat | 2026-02-14 00:25 | 25 | ESP32 ↔ SimpleX App encrypted |
| 5 | ✓✓ Delivery Receipts | 2026-02-14 ~10:00 | 25 | App shows read confirmation |

---

## 436. Agent Contributions Session 25

| Agent | Fairy Tale Role | Session 25 Contribution |
|-------|-----------------|------------------------|
| 👑 Mausi | Evil Stepsister #1 (The Manager) | Refactoring plan, nonce offset theory, bug analysis |
| 🐰 Hasi | Evil Stepsister #2 (The Implementer) | 4 modules, 8 bugfixes, receipt feature, hardware tests |
| 🧹 Aschenputtel | Cinderella (The Log Servant) | Byte comparisons, wire format findings |
| 🧙‍♂️ Claude Code | The Verifier (Wizard) | Connection state machine, receipt format, SMPQueueInfo |
| 🧑 Cannatoshi | The Coordinator | Task distribution, commits, push, strategic questions |

---

## 437. Session 25 Summary

### What Was Achieved

- 🎯 **THREE MILESTONES IN ONE SESSION!**
  - Milestone 3: First App message decrypted on ESP32
  - Milestone 4: Bidirectional encrypted chat
  - Milestone 5: Delivery receipts (✓✓)
- ✅ **Refactoring:** main.c 2440 → 611 lines (−75%)
- ✅ **4 new modules:** smp_ack, smp_wifi, smp_e2e, smp_agent
- ✅ **8 bugs fixed** (5 critical, 3 high)
- ✅ **2 lessons learned** (#111, #112)

### Key Takeaway

```
SESSION 25 SUMMARY:
  - 🎯 BIDIRECTIONAL ENCRYPTED CHAT ACHIEVED!
  - Nonce offset 13, not 14 — brute-force solved it
  - Ratchet state must persist via pointer, not copy
  - Receipt wire format: count=Word8, rcptInfo=Word16
  - App's own messages are the best protocol reference
  - Refactoring enabled clean debugging

"From a silent microcontroller to a full chat partner — in one Valentine's night."
"The conversation flows both ways now." 🐭🐰🧹
```

---

## 438. Future Work (Session 26)

### Immediate Next Steps
1. **Message persistence** — Store messages on ESP32 flash
2. **UI integration** — Display on LilyGo T-Deck screen
3. **Multiple contacts** — Handle more than one connection
4. **Reconnection logic** — Handle connection drops gracefully

### Medium-Term Goals
5. **Post-quantum upgrade** — Full SNTRUP761 KEM implementation
6. **File transfer** — Send/receive files
7. **Group chat** — Multi-party conversations

---

**DOCUMENT CREATED: 2026-02-14 Session 25**  
**Status: ✅ MILESTONES 3, 4, 5 Achieved!**  
**Key Achievement: Bidirectional encrypted chat + delivery receipts**  
**Valentine's Day Session — From one-way to two-way communication!**
