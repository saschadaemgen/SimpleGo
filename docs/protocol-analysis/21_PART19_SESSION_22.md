![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 19: Session 22
# Reply Queue Flow Discovery + E2E v3 + KEM Parser + NHK Promotion

**Document Version:** v36  
**Date:** 2026-02-07 Session 22  
**Status:** Reply Queue Flow Identified — App Still "Connecting..."  
**Previous:** Part 18 - Session 21 (v3 Format + HELLO Debugging)

---

## 385. Session 22 Overview

### 385.1 Starting Point

After Session 21 we had:
- v3 EncRatchetMessage format implemented and verified
- 7 HELLO format bugs fixed (#20-#26)
- Server accepts HELLO with OK
- App shows RSYNC crypto error (can't decrypt)
- 4 Header Key architecture (HKs/NHKs/HKr/NHKr)
- SameRatchet vs AdvanceRatchet modes
- Top suspects: HKs/NHKs promotion, E2E version, DH key encoding

### 385.2 Session 22 Goal

App-Status von "Connecting..." auf "Connected" bringen.

### 385.3 Session 22 Achievements

1. ✅ Bug #27: E2E Version Mismatch — `version_min = 2` → `version_min = 3` + KEM Nothing-Byte
2. ✅ Bug #28: KEM Parser Crash — Dynamic parser for SNTRUP761 (2310 bytes vs 88)
3. ✅ Bug #29: Body Decrypt Pointer-Arithmetik — Dynamic emHeader size calculation
4. ✅ Bug #30: HKs/NHKs Init + Promotion — Three connected fixes for header key chain
5. ✅ Bug #31: Phase 2a Try-Order — Correct order: HKr (SameRatchet), NHKr (AdvanceRatchet)
6. ✅ **Fundamental Protocol Discovery:** Modern SimpleX (v2 + `senderCanSecure`) needs NO HELLO!

### 385.4 What Was NOT Achieved

| Blocker | Status | Root Cause |
|---------|--------|------------|
| App shows "Connecting..." not "Connected" | ❌ | Missing Reply Queue Flow |
| No "Connected" status | ❌ | Need AgentConnInfo on Reply Queue |

---

## 386. Bugs Fixed (Session 22)

### 386.1 Bug #27: E2E Version Mismatch (Task 31a) — CRITICAL

**Session:** 22  
**Component:** `smp_x448.c` E2ERatchetParams encoding  
**Impact:** Critical - App's first response breaks silence!

**Problem:**
`smp_x448.c` sent `version_min = 2` in the AgentConfirmation, but `smp_ratchet.c` 
encrypted HELLO in v3 format (2-byte prefixes). The version mismatch caused the App 
to expect v2 format but receive v3.

**Cause:**
`smp_x448.c` was not updated in Session 21 when v3 was implemented in `smp_ratchet.c`.

**Incorrect Code:**
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x02;  // version_min = 2
// No KEM Nothing-Byte after key2
```

**Correct Code:**
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x03;  // version_min = 3
// After key2:
buf[p++] = 0x30;  // KEM Nothing ('0' = 0x30)
```

**Result:** App breaks silence and responds for the first time! 🎉

---

### 386.2 Bug #28: KEM Parser Crash (Task 33b)

**Session:** 22  
**Component:** `smp_ratchet.c` MsgHeader parser  
**Impact:** Critical - Parser crash on PQ responses

**Problem:**
App responds with v3 + SNTRUP761 KEM (2310 bytes) instead of 88-byte header.
Parser had fixed offsets → read garbage → crash.

**Cause:**
MsgHeader parser expected 88-byte header without KEM field. When App sent PQ KEM
data, the parser used wrong offsets for all subsequent fields.

**SNTRUP761 Details:**
- Public Key: 1158 bytes
- Ciphertext: 1039 bytes
- Shared Secret: 32 bytes

**Incorrect Code:**
```c
// Fixed offset calculation
int dh_key_offset = 4;  // contentLen(2) + msgMaxVersion(2)
int pn_offset = dh_key_offset + 1 + dh_key_len;  // No KEM handling
```

**Correct Code:**
```c
// Dynamic KEM handling
int kem_offset = dh_key_offset + 1 + dh_key_len;
uint8_t kem_tag = decrypted_header[kem_offset];
if (kem_tag == '0') {  // Nothing
    pn_offset = kem_offset + 1;
} else if (kem_tag == '1') {  // Just
    uint8_t state_tag = decrypted_header[kem_offset + 1];
    if (state_tag == 'P' || state_tag == 'A') {
        // Read length prefix, skip KEM data
        uint16_t kem_len = (decrypted_header[kem_offset + 2] << 8) | 
                            decrypted_header[kem_offset + 3];
        pn_offset = kem_offset + 4 + kem_len;
    }
}
```

**Result:** No crash on PQ responses, parser handles variable header sizes.

---

### 386.3 Bug #29: Body Decrypt Pointer-Arithmetik (Task 34a)

**Session:** 22  
**Component:** `main.c` body decrypt offset calculation  
**Impact:** Critical - 2GB malloc fail on body decrypt

**Problem:**
emHeader is now 2346 bytes (v3+PQ) instead of 123 bytes (v2), but pointer 
calculation for emAuthTag/emBody was hardcoded → garbage offsets → 2GB malloc fail.

**Cause:**
After receiving v3+PQ response, the emHeader size changes dramatically:
- v2: 123 bytes
- v3: 124 bytes  
- v3+PQ: 2346 bytes (with SNTRUP761 ciphertext)

**Incorrect Code:**
```c
#define EM_HEADER_SIZE 124  // Hardcoded
uint8_t *emAuthTag = &encrypted[EM_HEADER_SIZE];
uint8_t *emBody = &encrypted[EM_HEADER_SIZE + 16];
```

**Correct Code:**
```c
// Read ehVersion to determine size
uint16_t ehVersion = (encrypted[0] << 8) | encrypted[1];
size_t emHeader_size;
if (ehVersion >= 3) {
    // v3: 2-byte length prefix
    emHeader_size = (encrypted[2] << 8) | encrypted[3];
    emHeader_size += 4;  // Include prefix itself
} else {
    // v2: 1-byte length prefix
    emHeader_size = encrypted[2] + 3;
}
uint8_t *emAuthTag = &encrypted[emHeader_size];
uint8_t *emBody = &encrypted[emHeader_size + 16];
```

**Result:** Body decrypt works with any header size.

---

### 386.4 Bug #30: HKs/NHKs Init + Promotion (Task 31b extended)

**Session:** 22  
**Component:** `smp_ratchet.c` header key management  
**Impact:** Critical - Header key chain broken from init to promotion

**Problem 30a:** `next_header_key_send` was never stored in ratchet state (local variable only).

**Problem 30b:** `ratchet_x3dh_sender()` stored `nhk` (= rcvNextHK = NHKr) incorrectly 
in `header_key_recv` instead of `next_header_key_recv`.

**Problem 30c:** After DH Ratchet Step, KDF output was set directly as HKs instead 
of proper NHKs→HKs promotion.

**Incorrect Code:**
```c
// In ratchet_init_sender():
uint8_t next_header_key_send[32];  // Local variable, never saved!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.header_key_recv, nhk, 32);  // WRONG! nhk is NHKr
// ...
// After DH Ratchet Step:
memcpy(ratchet_state.header_key_send, kdf_output + 64, 32);  // Direct, no promotion
```

**Correct Code:**
```c
// In ratchet_init_sender():
memcpy(ratchet_state.next_header_key_send, hkdf_output + 64, 32);  // SAVE to state!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.next_header_key_recv, nhk, 32);  // NHKr, will promote to HKr
// ...
// After DH Ratchet Step - PROMOTION:
memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32);  // NHKs→HKs
memcpy(ratchet_state.next_header_key_send, kdf_output + 64, 32);  // New NHKs from KDF
```

**Result:** Header key chain correct from initialization through all promotions.

---

### 386.5 Bug #31: Phase 2a Try-Order (Task 35a)

**Session:** 22  
**Component:** `main.c` header decrypt try sequence  
**Impact:** Critical - AdvanceRatchet never triggered

**Problem:**
Header decrypt tried `next_header_key_recv` only via debug fallback (`saved_nhk`), 
not as a regular try → AdvanceRatchet was never triggered → ratchet state stuck.

**Cause:**
The Double Ratchet requires trying keys in specific order:
1. HKr (SameRatchet) — same DH key, just chain forward
2. NHKr (AdvanceRatchet) — new DH key, full ratchet step

If NHKr succeeds, it triggers AdvanceRatchet and promotes NHKr→HKr.

**Incorrect Code:**
```c
// Only tried HKr
if (try_header_decrypt(header_key_recv, ...)) {
    // SameRatchet
} else {
    // Debug fallback using saved_nhk (not proper flow)
    if (try_header_decrypt(saved_nhk, ...)) {
        // This worked but didn't trigger AdvanceRatchet!
    }
}
```

**Correct Code:**
```c
// Try HKr first (SameRatchet)
if (try_header_decrypt(header_key_recv, ...)) {
    decrypt_mode = SAME_RATCHET;
} 
// Try NHKr second (AdvanceRatchet)
else if (try_header_decrypt(next_header_key_recv, ...)) {
    decrypt_mode = ADVANCE_RATCHET;
    // Promote: HKr ← NHKr
    memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
    // Trigger full DH ratchet step...
}
```

**Result:** AdvanceRatchet triggers correctly, ratchet state advances properly.

---

## 387. Fundamental Protocol Discovery

### 387.1 Modern SimpleX Protocol Needs NO HELLO!

Claude Code Task 35a analyzed the complete Agent-Level Connection Flow and 
discovered: **The modern SimpleX protocol (v2 with `senderCanSecure = True`, 
`QMMessaging`) does not require HELLO messages!**

### 387.2 Modern Protocol Flow (`senderCanSecure = True`, `QMMessaging`)

```
1. ESP32 (Alice) creates Invitation           ✅ Working
2. App (Bob) sends AgentConfirmation          ✅ Working
   → Contains Reply Queue Info + ConnInfo
   → Ratchet + crypto_box encrypted
3. ESP32 extracts Reply Queue Info            ❌ MISSING
4. ESP32 connects to Reply Queue Server       ❌ MISSING
5. ESP32 sends SKEY on Reply Queue            ❌ MISSING
6. ESP32 sends AgentConnInfo on Reply Queue   ❌ MISSING
   → Only our profile, no queue info
   → Ratchet + crypto_box encrypted
7. App receives → CON → "Connected"           ❌ Never happens
```

### 387.3 HELLO Only in Older Protocol

HELLO is only relevant in the older protocol flow (without `senderCanSecure`).
The modern protocol skips HELLO entirely and expects AgentConnInfo directly 
on the Reply Queue.

### 387.4 Reply Queue Info Location

The `smpReplyQueues` (= `NonEmpty SMPQueueInfo`) are inside the **ratchet-encrypted 
`AgentConnInfoReply`** with tag `'D'` (0x44). They are at the innermost encryption 
layer. We already decrypt them successfully but only parse the ConnInfo (profile JSON) 
and skip the Queue Info.

### 387.5 AgentConfirmation Layers (Updated)

```
Layer 1: SMP Server (rcvDhSecret)
Layer 2: ClientMsgEnvelope (PubHeader + cmNonce + cmEncBody)
Layer 3: crypto_box → ClientMessage (PrivHeader + AgentConfirmation)
Layer 4: Double Ratchet → AgentMessage (Tag 'D' + SMPQueueInfo + ConnInfo)
                                        ↑
                                        └── Reply Queue Info HERE!
```

---

## 388. SMPQueueInfo Wire Format (for Parser in Session 23)

### 388.1 Structure

```
[1B count] [SMPQueueInfo:]
  [2B clientVersion] [SMPServer:] [1B+N senderId] [1B+44 DH X25519 SPKI] [1B QueueMode 'M']

SMPServer:
  [1B host count] [1B+N hostname] [1B+N port] [1B+N keyHash]
```

### 388.2 Example

```
01                              — count: 1 queue
00 08                           — clientVersion: 8
02                              — host count: 2
  0D 73 6D 70 31 2E ...         — hostname: "smp1.simplex.im"
  05 35 32 32 33                — port: "5223"
  20 XX XX XX ...               — keyHash: 32 bytes
08 AA BB CC DD EE FF GG HH      — senderId: 8 bytes
2C 30 2A 30 05 ...              — DH key: 44 bytes X25519 SPKI
4D                              — queueMode: 'M' = Messaging
```

---

## 389. SNTRUP761 Post-Quantum KEM

### 389.1 Algorithm Choice

SimpleX uses **SNTRUP761**, not Kyber1024, for Post-Quantum KEM.

### 389.2 Sizes

| Component | Size |
|-----------|------|
| Public Key | 1158 bytes |
| Ciphertext | 1039 bytes |
| Shared Secret | 32 bytes |

### 389.3 PQ-Graceful-Degradation

When we send v3 + KEM Nothing, the App understands "PQ-capable but no KEM active yet".
It responds with KEM Proposed (sends its SNTRUP761 Public Key). If we reply with 
KEM Nothing, the ratchet falls back to pure DH — no error, no abort.

---

## 390. Files Changed (Session 22)

| File | Tasks | Description |
|------|----------|-------------|
| `smp_x448.c` | 31a | E2E v3 + KEM Nothing in e2e_encode_params() |
| `smp_ratchet.c` | 31b, 33b | NHK Init/Promotion + KEM Parser + v3 Offsets |
| `smp_ratchet.h` | 31b | Struct changes for NHKs field |
| `main.c` | 34a, 35a | Body Decrypt Pointer + Header Decrypt Try-Order |

---

## 391. Status After Session 22

### 391.1 Working ✅

| Layer/Feature | Status |
|---------------|--------|
| TLS 1.3 Handshake | ✅ |
| SMP Protocol | ✅ |
| X3DH Key Agreement | ✅ |
| Double Ratchet Header Decrypt (with PQ Skip) | ✅ |
| Double Ratchet Body Decrypt | ✅ |
| Zstd Decompression | ✅ |
| ConnInfo JSON Parsing | ✅ |
| AdvanceRatchet (NHKr → ratchetStep) | ✅ |
| HKs/NHKs Promotion | ✅ |
| HELLO senden + Server OK | ✅ |
| App → ESP Nachricht empfangen + entschlüsseln | ✅ |
| E2E v3 in AgentConfirmation | ✅ |
| KEM Parser (dynamic, handles PQ) | ✅ |
| Body Decrypt (dynamic emHeader size) | ✅ |
| Header Decrypt Try-Order (HKr, NHKr) | ✅ |

### 391.2 Missing ❌

| Feature | Status | Priority |
|---------|--------|----------|
| Reply Queue Info aus Confirmation parsen | ❌ | ★★★★★ |
| Zweite TLS-Verbindung zum Reply Queue Server | ❌ | ★★★★★ |
| SMP Handshake auf Reply Queue | ❌ | ★★★★★ |
| SKEY auf Reply Queue senden | ❌ | ★★★★★ |
| AgentConnInfo auf Reply Queue senden | ❌ | ★★★★★ |
| App zeigt "Connected" | ❌ | Result of above |

---

## 392. Session 22 Discoveries (Key Discoveries)

### 392.1 Discovery 1: Modern Protocol Flow

Modern SimpleX (v2 + `senderCanSecure = True`) does NOT need HELLO.
Instead, AgentConnInfo must be sent on the Reply Queue.

### 392.2 Discovery 2: Reply Queue Info Location

`smpReplyQueues` are in the innermost layer: inside the ratchet-decrypted 
`AgentConnInfoReply` with tag `'D'` (0x44).

### 392.3 Discovery 3: SNTRUP761 for PQ

SimpleX uses SNTRUP761 (not Kyber) with 1158B pubkey, 1039B ciphertext.

### 392.4 Discovery 4: PQ-Graceful-Degradation

v3 + KEM Nothing = "PQ-capable, not yet active". App responds with KEM Proposed.
Reply with KEM Nothing → graceful fallback to pure DH.

### 392.5 Discovery 5: E2E Version Consistency

`version_min` in AgentConfirmation MUST match `RATCHET_VERSION` used for encryption.
Mismatch causes decoder to use wrong format.

### 392.6 Discovery 6: Dynamic Header Sizes

v3+PQ headers can be 2346 bytes (vs 123/124 for v2/v3 without PQ).
All offset calculations must be dynamic based on ehVersion and KEM presence.

### 392.7 Discovery 7: NHK Storage at Init

`next_header_key_send` must be stored in ratchet state during initialization,
not kept as local variable.

### 392.8 Discovery 8: nhk = NHKr, not HKr

The `nhk` from X3DH HKDF output is `next_header_key_recv` (NHKr), which promotes 
to `header_key_recv` (HKr) on first AdvanceRatchet.

### 392.9 Discovery 9: NHKs→HKs Promotion

After DH ratchet step: first promote `HKs ← NHKs`, then set `NHKs ← KDF output`.
Do NOT set KDF output directly as HKs.

### 392.10 Discovery 10: Header Decrypt Try-Order

Must try keys in order:
1. HKr (SameRatchet) — same DH, chain forward
2. NHKr (AdvanceRatchet) — new DH, full ratchet step

If NHKr succeeds → promote NHKr→HKr and trigger AdvanceRatchet.

---

## 393. Complete Decryption Chain (Updated Session 22)

```
RECEIVE CHAIN (all working):
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working (S18)
  ↓
Layer 2.5: unPad                                               ✅ Working (S19)
  ↓
Layer 3: ClientMessage Parse                                   ✅ Working (S19)
  ↓
Layer 4: EncRatchetMessage Parse (dynamic KEM)                 ✅ Working (S22)
  ↓
Layer 5: Double Ratchet Header Decrypt (Try-Order fixed)       ✅ Working (S22)
  ↓
Layer 6: Double Ratchet Body Decrypt (dynamic offsets)         ✅ Working (S22)
  ↓
Layer 7: ConnInfo Parse + Zstd                                 ✅ Working (S20)
  ↓
Layer 8: Peer Profile JSON                                     ✅ Working (S20)

SEND CHAIN (HELLO → now known: need Reply Queue):
Layer 9a: HELLO Send                                           ✅ Server OK (but not needed!)
  ↓
Layer 9b: Reply Queue Info Parse                               ❌ MISSING
  ↓
Layer 9c: Reply Queue TLS Connect                              ❌ MISSING
  ↓
Layer 9d: Reply Queue SMP Handshake                            ❌ MISSING
  ↓
Layer 9e: SKEY on Reply Queue                                  ❌ MISSING
  ↓
Layer 9f: AgentConnInfo on Reply Queue                         ❌ MISSING
  ↓
Layer 10: App receives → CON                                   ⏳ Blocked
  ↓
Layer 11: Connection Established ("Connected")                 ⏳ Final Goal
```

---

## 394. Session 22 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-07 | Bug #27: E2E version_min 2→3 + KEM Nothing | App breaks silence! |
| 2026-02-07 | Bug #28: KEM Parser dynamic | No crash on PQ responses |
| 2026-02-07 | Bug #29: Body Decrypt dynamic offsets | Works with any header size |
| 2026-02-07 | Bug #30: HKs/NHKs Init + Promotion | Header key chain correct |
| 2026-02-07 | Bug #31: Header Decrypt Try-Order | AdvanceRatchet triggers |
| 2026-02-07 | Protocol Discovery: No HELLO needed | Modern flow identified |
| 2026-02-07 | Reply Queue Info location found | Tag 'D' in AgentConnInfoReply |
| 2026-02-07 | SNTRUP761 identified | Not Kyber, different sizes |
| 2026-02-07 | SMPQueueInfo wire format documented | Ready for parser |

---

## 395. Session 22 Statistics

| Metric | Value |
|--------|-------|
| Duration | 1 chat session |
| Tasks completed | 5 (31a, 31b, 33b, 34a, 35a) |
| Bugs found & fixed | 5 (Bug #27-#31) |
| Claude Code Analyses | Multiple (Connection Flow, KEM format, NHK promotion) |
| Protocol Discovery | Modern flow needs no HELLO |
| Next Steps | Reply Queue implementation |

---

## 396. Session 22 Summary

### What Was Achieved

- **5 bugs fixed** (#27-#31): E2E version, KEM parser, body pointer, NHK init/promotion, try-order
- **App breaks silence** — first response after E2E version fix!
- **Dynamic parsing** — handles v2, v3, and v3+PQ headers
- **Header key chain** — correct from init through all promotions
- **AdvanceRatchet** — triggers correctly on NHKr success
- **Protocol discovery** — modern SimpleX needs no HELLO, needs Reply Queue flow

### What Was NOT Achieved

- App still shows "Connecting..." (not "Connected")
- Reply Queue flow not implemented

### Root Cause for "Connecting..."

**The modern SimpleX protocol (v2 + `senderCanSecure`) expects AgentConnInfo on 
the Reply Queue, not a HELLO on the Contact Queue.**

### Next Steps (Session 23)

1. Parse Reply Queue Info from AgentConfirmation (tag 'D')
2. Establish second TLS connection to Reply Queue server
3. Perform SMP handshake on Reply Queue
4. Send SKEY command on Reply Queue
5. Send AgentConnInfo (our profile) on Reply Queue
6. Receive CON → "Connected"!

### Key Takeaway

```
SESSION 22 SUMMARY:
  - 5 bugs fixed (#27-#31)
  - App responds for first time after E2E v3 fix!
  - KEM parser handles SNTRUP761 (2310 bytes)
  - NHK promotion chain corrected
  - AdvanceRatchet triggers correctly
  - BREAKTHROUGH: Modern protocol needs NO HELLO!
  - Need Reply Queue flow, not HELLO

"Five bugs, one protocol discovery, zero HELLOs needed."
"The App was waiting for Reply Queue, not HELLO."
"Evidence before fix. Always." 🐭
```

---

**DOCUMENT CREATED: 2026-02-07 Session 22 v36**  
**Status: Reply Queue Flow Identified, App Still "Connecting..."**  
**Key Achievement: 5 bugs fixed, Modern Protocol Flow discovered**  
**Next: Reply Queue Implementation (Parse, Connect, SKEY, AgentConnInfo)**
