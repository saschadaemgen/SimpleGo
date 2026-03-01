![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 18: Session 21
# HELLO Format Debugging + v3 EncRatchetMessage + KEY Command

**Document Version:** v35  
**Date:** 2026-02-06/07 Session 21  
**Status:** v3 Format Implemented — App Still "Connecting..."  
**Previous:** Part 17 - Session 20 (Body Decrypt SUCCESS, Peer Profile Read)

---

## 371. Session 21 Overview

### 371.1 Starting Point

After Session 20 we had:
- Complete crypto chain TLS → JSON working end-to-end
- Peer profile "cannatoshi" read on ESP32
- Bug #19 FIXED
- All 19 bugs solved
- Next: HELLO processing, Ratchet State Persistence, Bidirectional Messaging

### 371.2 Session 21 Goal

**Phase 1–4:** HELLO send → HELLO receive → Connected → Chat

### 371.3 Session 21 Scope

This was a large session spanning 3 chats (main + 2 crash continuations):
- [Session 21 Main Chat](https://claude.ai/chat/dbd05f28-851e-445d-a68d-993077b3191f)
- [Session 21 Continuation — HELLO Format Debugging](https://claude.ai/chat/6959255e-f475-469f-a3a0-19b5b695b7db)
- Session 21 Continuation — KEY/Timing/v3

### 371.4 Session 21 Achievements

1. ✅ Ratchet State Update activated (LOG ONLY → ACTIVE)
2. ✅ SameRatchet / AdvanceRatchet split implemented
3. ✅ NHK→HK Promotion (4 Header Keys architecture)
4. ✅ HELLO sends, Server responds OK
5. ✅ Bug #20: PrivHeader Fix ('_' → 0x00 for HELLO)
6. ✅ Bug #21: AgentVersion Fix (v2 → v1 for AgentMessage)
7. ✅ Bug #22: prevMsgHash Encoding Fix (raw → smpEncode with Word16 prefix)
8. ✅ Bug #23: cbEncrypt Padding Fix (unPad output → padded input)
9. ✅ Bug #24: DH Key Fix (rcv_dh → snd_dh for HELLO)
10. ✅ Bug #25: PubHeader Nothing Encoding (missing → '0')
11. ✅ KEY Command implemented, Server OK
12. ✅ HELLO timing fix (after Confirmation receipt)
13. ✅ KEY not required (proven by test — queue unsecured)
14. ✅ chatItemNotFoundByContactId → RSYNC Crypto Error identified
15. ✅ v2/v3 format mismatch identified as root cause
16. ✅ Bug #26: v3 EncRatchetMessage format implemented
17. ✅ v3 format byte-correct verified, Server OK

### 371.5 What Was NOT Achieved

| Blocker | Status | Suspect |
|---------|--------|---------|
| App shows "Connecting..." not "Connected" | ❌ | HKs/NHKs Promotion or E2E Version in Confirmation |
| No Reply HELLO from Peer | ❌ | Ratchet Decrypt fails despite v3 format |

---

## 372. Phase 1: Ratchet State Update + NHK Architecture

### 372.1 Ratchet State Update Activation (Aufträge 1-2)

In Session 20, `ratchet_decrypt_body()` had state updates as LOG ONLY.
Session 21 activated all state updates:

```
After successful body decrypt, update:
  - root_key → new_root_key_2 (from rootKdf #2)
  - chain_key_recv → next from chainKdf
  - chain_key_send → from rootKdf #2
  - dh_self → new keypair (generated during rootKdf #2)
  - dh_peer → peer_new_pub (from MsgHeader)
  - msg_num_recv → incremented
  - msg_num_send → reset to 0
  - All header keys → updated per NHK architecture
```

### 372.2 SameRatchet vs AdvanceRatchet (Auftrag 5b)

Two distinct ratchet operations identified:

| Mode | Trigger | DH Step? | Operations |
|------|---------|----------|------------|
| **SameRatchet** | Same DH key as before (dh_changed=false) | NO | chainKdf only → mk, ivs |
| **AdvanceRatchet** | New DH key in header (dh_changed=true) | YES | 2× rootKdf + chainKdf → new chains + mk |

For the first received message (AgentConfirmation), `dh_changed = true` → AdvanceRatchet.
For subsequent messages from same sender without DH change → SameRatchet.

### 372.3 NHK→HK Promotion — 4 Header Key Architecture (Auftrag 5a)

**Discovery:** The Double Ratchet requires 4 header key slots, not 2!

| Key | Full Name | Usage |
|-----|-----------|-------|
| HKs | header_key_send | Current: encrypt our outgoing headers |
| NHKs | next_header_key_send | Next: will become HKs after our DH ratchet |
| HKr | header_key_recv | Current: decrypt incoming headers |
| NHKr | next_header_key_recv | Next: will become HKr after peer's DH ratchet |

**Promotion Flow on AdvanceRatchet (receiving):**
```
1. HKr ← NHKr (old "next" becomes "current")
2. Try decrypt header with HKr
3. rootKdf → new NHKr (for future messages)
```

**Promotion Flow on AdvanceRatchet (sending):**
```
1. HKs ← NHKs (old "next" becomes "current")
2. Encrypt header with HKs
3. rootKdf → new NHKs (for future sends)
```

**Initial Assignment from X3DH:**
```
HKs  = hk     (HKDF[0-31])   — used for our first send
NHKs = (none, set after first recv AdvanceRatchet)
HKr  = (none, NHKr promotes on first recv)
NHKr = nhk    (HKDF[32-63])  — promotes to HKr on first recv
```

---

## 373. Phase 2: HELLO Format Debugging (Bugs #20-#25)

### 373.1 HELLO Wire Format Analysis

HELLO is an AgentMessage, NOT an AgentConfirmation. It has a different format:

```
AgentMessage (HELLO):
  [Word16 BE]     agentVersion = 1 (NOT 2!)
  [Word16 BE]     SMP version
  [Large]         prevMsgHash (smpEncode ByteString)
  [Tail]          AgentMsgBody

AgentMsgBody for HELLO:
  Tag 'H'         HELLO identifier
  AckMode         '0' = AckMode_Off
```

Wrapped in:
```
ClientMessage:
  PrivHeader      0x00 (empty, NOT 'K' and NOT '_')
  Body            AgentMsgEnvelope containing EncRatchetMessage
```

### 373.2 Bug #20: PrivHeader for HELLO

**Session:** 21  
**Component:** ClientMessage encoding for HELLO  
**Impact:** Critical - wrong message type indicator

```
WRONG:  '_' (0x5F) = PHEmpty — this is for Confirmation without key
RIGHT:  0x00 = No PrivHeader — HELLO has no PrivHeader at all
```

The PrivHeader encoding is:
- `'K'` (0x4B) = PHConfirmation (with sender auth key)
- `'_'` (0x5F) = PHEmpty (confirmation without key)
- `0x00` = No PrivHeader (regular messages like HELLO)

**Note:** This is NOT a standard Maybe encoding. PrivHeader uses its own scheme.

### 373.3 Bug #21: AgentVersion for AgentMessage

**Session:** 21  
**Component:** AgentMsgEnvelope encoding  
**Impact:** Critical - parser version mismatch

```
WRONG:  agentVersion = 2 (0x00 0x02)
RIGHT:  agentVersion = 1 (0x00 0x01)
```

AgentConfirmation uses agentVersion=7, but AgentMessage (HELLO) uses agentVersion=1.
The version field in AgentMsgEnvelope is not the same as the Agent protocol version.

### 373.4 Bug #22: prevMsgHash Encoding

**Session:** 21  
**Component:** AgentMessage encoding  
**Impact:** Critical - parser fails on hash field

```
WRONG:  Raw empty bytes or missing
RIGHT:  smpEncode(ByteString) with Word16 prefix → [0x00 0x00] for empty hash
```

The prevMsgHash field uses Large encoding (Word16 prefix), consistent with Bug #2
from Session 4. For an empty hash: `[0x00][0x00]` (Word16 BE length = 0).

### 373.5 Bug #23: cbEncrypt Padding

**Session:** 21  
**Component:** Server-level encryption (cbEncrypt)  
**Impact:** Critical - server rejects or app can't decrypt

```
WRONG:  Encrypt the unPad output (raw plaintext)
RIGHT:  Encrypt the padded plaintext (with pad layer applied)
```

The `pad` function adds a 2-byte length prefix and 0x23 padding BEFORE encryption.
The receiver does: decrypt → unPad. So the sender must: pad → encrypt.

### 373.6 Bug #24: DH Key for HELLO

**Session:** 21  
**Component:** Per-queue E2E encryption key selection  
**Impact:** Critical - E2E layer fails

```
WRONG:  rcv_dh_public (receiver's DH key) for HELLO encryption
RIGHT:  snd_dh_public (sender's DH key) for HELLO encryption
```

For Confirmation: we use `rcv_dh` (the receiver's key from the queue).
For HELLO: we use `snd_dh` (the sender's key for the reply queue).

### 373.7 Bug #25: PubHeader Nothing Encoding

**Session:** 21  
**Component:** ClientMsgEnvelope PubHeader field  
**Impact:** Medium - parser may fail

```
WRONG:  Field missing entirely
RIGHT:  '0' (0x30) = Nothing (standard Maybe encoding)
```

The PubHeader in ClientMsgEnvelope is a Maybe type. When Nothing, it must be
encoded as `'0'` (0x30), not omitted.

### 373.8 Result After Bugs #20-#25

App status changed from "Connecting..." to **CONN NOT_AVAILABLE**.

NOT_AVAILABLE = AUTH error on the App side when it tries to SEND on the queue.
This is progress — the HELLO is being processed, but something is wrong.

---

## 374. Phase 3: KEY Command + Timing

### 374.1 KEY Command Implementation (Auftrag 22-23)

**Discovery:** After receiving AgentConfirmation, the ESP32 (as recipient) should
send a KEY command to authorize the sender.

```
KEY Wire Format:
  [corrId][recipientId] KEY [peer_sender_auth_key 44 bytes]

Signed:    Ed25519 with rcv_auth_private
Server:    Main SSL connection (not peer server)
Response:  OK | ERR AUTH
```

The `peer_sender_auth_key` (44 bytes Ed25519 SPKI) comes from the PHConfirmation
in the received AgentConfirmation message.

### 374.2 KEY Command Result

Server responds OK. The sender (App) is now authorized to SEND on the queue.

### 374.3 HELLO Timing Fix (Auftrag 24)

**Discovery:** HELLO was being sent too early — during `complete_handshake()`,
which runs BEFORE the AgentConfirmation is received and processed.

```
WRONG ORDER:
  1. Send our AgentConfirmation
  2. Send HELLO (immediately after)
  3. Receive peer's AgentConfirmation
  4. Process + Decrypt

RIGHT ORDER:
  1. Send our AgentConfirmation
  2. Receive peer's AgentConfirmation
  3. Process + Decrypt (DH Ratchet Step, get peer's DH key)
  4. Send KEY command
  5. Send HELLO (with correct ratchet state)
```

### 374.4 KEY Not Required — Proven (Auftrag 25b)

**Test:** Send HELLO without KEY command.
**Result:** BEST result yet — `chatItemNotFoundByContactId` error in App.

This proves Reply Queues are **unsecured** — SEND works without KEY authorization.
The KEY command is for security hardening, not functional necessity.

---

## 375. Phase 4: Root Cause Analysis + v3 Format

### 375.1 chatItemNotFoundByContactId (Auftrag 27a)

**Claude Code Analysis (Opus 4.6 on simplex-chat):**

`chatItemNotFoundByContactId` is triggered by **RSYNC** (Ratchet Sync) event.
RSYNC occurs when the App fails to decrypt a received message.

Flow:
```
App receives our HELLO → tries ratchet_decrypt → FAILS → RSYNC event
→ chatItemNotFoundByContactId (no chat item for this contact yet)
```

This means the App CAN'T DECRYPT our HELLO. The crypto is wrong.

### 375.2 v2/v3 Format Mismatch Identified (Auftrag 28)

**Root Cause Discovery:**

The App's ratchet was initialized with `currentE2EEncryptVersion = VersionE2E 3`
(v3). But our EncRatchetMessage was encoded in v2 format.

**The difference:**

| Field | v2 (our code) | v3 (expected) |
|-------|---------------|---------------|
| emHeader length | 1 byte (0x7B = 123) | 2 bytes Word16 BE (0x00 0x7C = 124) |
| emHeader size | 123 bytes | 124 bytes |
| ehBody length | 1 byte (0x58 = 88) | 2 bytes Word16 BE (0x00 0x58 = 88) |
| ehBody size | 88 bytes | 88 bytes (same) |
| MsgHeader | No KEM field | KEM Nothing ('0' = 0x30) |
| contentLen | 79 | 80 (KEM adds 1 byte) |

**Key insight:** `encodeLarge` switches behavior at v≥3:
- v < 3: 1-byte length prefix (max 255)
- v ≥ 3: 2-byte Word16 BE length prefix (max 65535)

### 375.3 Version Negotiation (Auftrag 28)

```
currentE2EEncryptVersion = VersionE2E 3     (module constant in Haskell)
Version in E2ERatchetParams → current → rcVersion
rcVersion controls encodeLarge behavior
```

The version in our E2ERatchetParams (sent in AgentConfirmation) determines what
format the peer expects. If we advertise v2 but the App uses v3 internally,
there's a mismatch.

### 375.4 Bug #26: v3 EncRatchetMessage Format (Auftrag 29b)

**Session:** 21  
**Component:** EncRatchetMessage encoding  
**Impact:** Critical - App can't decrypt HELLO

Implemented v3 format with all changes:

```
RATCHET_VERSION changed: 2 → 3
emHeader length prefix: 1 byte → 2 bytes Word16 BE
emHeader size: 123 → 124 bytes
ehBody length prefix: 1 byte → 2 bytes Word16 BE
MsgHeader: added KEM Nothing ('0' = 0x30)
MsgHeader contentLen: 79 → 80
MsgHeader padding: 7 bytes → 6 bytes (KEM takes 1 byte)
```

### 375.5 v3 EncRatchetMessage Format (Verified)

```
[00 7C]          - emHeader length (2 bytes Word16 BE = 124)
[124 bytes]      - emHeader
[16 bytes]       - payload AuthTag
[N bytes]        - encrypted payload (Tail)

emHeader (124 bytes):
  [00 03]        - ehVersion (v3)
  [16 bytes]     - ehIV (raw)
  [16 bytes]     - ehAuthTag (raw)
  [00 58]        - ehBody length (2 bytes Word16 BE = 88)
  [88 bytes]     - encrypted MsgHeader

MsgHeader (padded to 88 bytes):
  [00 50]        - content length (Word16 BE = 80)
  [00 02]        - msgMaxVersion (Word16 BE = 2)
  [44]           - DH key length (1 byte = 68)
  [68 bytes]     - msgDHRs SPKI (12 header + 56 raw X448)
  [30]           - KEM Nothing ('0' = 0x30)  ← NEW in v3!
  [4 bytes]      - msgPN (Word32 BE)
  [4 bytes]      - msgNs (Word32 BE)
  [6 bytes]      - '#' padding (6× instead of 7× in v2)
```

### 375.6 Result

v3 format byte-correct verified. Server accepts with OK.
But App still shows "Connecting..." — decrypt still fails.

---

## 376. Session 21 Erkenntnisse (Key Discoveries)

### 376.1 Erkenntnis 1: Party Roles

ESP32 = **Accepting Party** (creates Invite link).
App = **Initiating/Joining Party** (scans link).

This affects which keys and queues are used for each direction.

### 376.2 Erkenntnis 2: PrivHeader Encoding (Corrected)

| Value | Hex | When Used |
|-------|-----|-----------|
| PHConfirmation 'K' | 0x4B | AgentConfirmation with sender auth key |
| PHEmpty '_' | 0x5F | AgentConfirmation without key |
| No PrivHeader | 0x00 | Regular messages (HELLO, chat messages) |

**NOT a standard Maybe encoding!** Custom scheme.

### 376.3 Erkenntnis 3: AgentMessage vs AgentConfirmation

| Field | AgentConfirmation | AgentMessage (HELLO) |
|-------|-------------------|----------------------|
| agentVersion | 7 | 1 |
| Format | (version, 'C', e2eEncryption_, Tail encConnInfo) | (version, smpVersion, prevMsgHash, Tail body) |
| PrivHeader | 'K' (with key) or '_' (empty) | 0x00 (none) |

### 376.4 Erkenntnis 4: HELLO Body Format

```
AgentMsgBody for HELLO:
  'H'     — HELLO tag
  '0'     — AckMode_Off (0x30, ASCII '0')

Total: 2 bytes
```

### 376.5 Erkenntnis 5: KEY Command

```
KEY = Recipient command to authorize a sender
SKEY = Sender command (server-side, not client-initiated)

KEY Body: [corrId][recipientId] KEY [sender_auth_key 44B SPKI]
Signed with: rcv_auth_private (Ed25519)
```

Reply Queues are unsecured — KEY is optional for functionality.

### 376.6 Erkenntnis 6: RSYNC = Ratchet Sync

When the App fails to decrypt a received message, it triggers a RSYNC event.
`chatItemNotFoundByContactId` = RSYNC with no existing chat item for this contact.

This is a **crypto error indicator**, not a HELLO parsing error.

### 376.7 Erkenntnis 7: v2/v3 encodeLarge Switch

```
encodeLarge v bs
  | v < VersionE2E 3 = smpEncode (Str.length bs :: Word8) <> bs    -- 1 byte
  | otherwise        = smpEncode (Str.length bs :: Word16) <> bs   -- 2 bytes
```

This affects:
- emHeader length prefix (1 vs 2 bytes)
- ehBody length prefix (1 vs 2 bytes)
- Total emHeader size (123 vs 124 bytes)

### 376.8 Erkenntnis 8: Version Source

The E2E ratchet version comes from E2ERatchetParams in the AgentConfirmation,
NOT from a hardcoded constant. The Confirmation can work with v2 while the
App expects v3 for subsequent messages (HELLO).

---

## 377. Open Suspects for Session 22

### 377.1 Suspect 1: HKs/NHKs Promotion After AdvanceRatchet

After receiving the App's AgentConfirmation (AdvanceRatchet):
```c
// Current code:
memcpy(ratchet_state.header_key_send, new_nhk_send, 32);  // Direct from KDF
```

Standard Double Ratchet promotion:
```
HKs ← old NHKs (the OLD next-header-key becomes current)
NHKs ← KDF output (new value becomes NEXT)
```

If Haskell does promotion → we encrypt HELLO header with WRONG key!

### 377.2 Suspect 2: E2E Version in Our Confirmation

If we send `v=2` in SndE2ERatchetParams, the App initializes its ratchet with
`current=2` → expects v2 format → our v3 HELLO decoded with v2 parser → FAIL.

### 377.3 Suspect 3: DH Key Encoding in v3

Unclear if v3 MsgHeader expects raw 56-byte X448 or still 68-byte SPKI.

---

## 378. Files Changed (Session 21)

| File | Changes |
|------|---------|
| main/main.c | senderKey storage, KEY cmd, HELLO timing (after Conf), format fixes |
| main/smp_handshake.c | KEY Command function |
| main/smp_peer.c | HELLO removed from complete_handshake() |
| main/smp_ratchet.c | v3 format: RATCHET_VERSION=3, 2-byte prefixes, KEM Nothing, NHK split |
| main/include/smp_ratchet.h | v3 constants, NHK fields, ratchet_decrypt_mode_t |

---

## 379. Complete Decryption Chain (Updated Session 21)

```
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
Layer 4: EncRatchetMessage Parse                               ✅ Working (S19, v3 S21)
  ↓
Layer 5: Double Ratchet Header Decrypt                         ✅ Working (S19)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ✅ Working (S20)
  ↓
Layer 7: ConnInfo Parse + Zstd                                 ✅ Working (S20)
  ↓
Layer 8: Peer Profile JSON                                     ✅ Working (S20)
  ↓
Layer 9: HELLO Send                                            ⚠️ Server OK, App can't decrypt
  ↓ v3 format implemented, KEY cmd working
  ↓ App response: RSYNC (crypto error)
  ↓
Layer 10: HELLO Receive                                        ⏳ Blocked by Layer 9
  ↓
Layer 11: Connection Established                               ⏳ Final Goal
```

---

## 380. KEY Command Reference

### 380.1 Wire Format

```
KEY Body:  [corrId][recipientId] KEY [peer_sender_auth_key 44 bytes]
Signed:    Ed25519 with rcv_auth_private
Server:    Main SSL connection (not peer server)
Response:  OK | ERR AUTH
```

### 380.2 Source of sender_auth_key

The `peer_sender_auth_key` (44 bytes Ed25519 SPKI) is extracted from
PHConfirmation in the received AgentConfirmation message.

### 380.3 Timing

```
1. Receive AgentConfirmation from peer
2. Extract sender_auth_key from PHConfirmation
3. Send KEY command to server
4. Send HELLO message
```

### 380.4 Necessity

Reply Queues are **unsecured** — SEND works without KEY authorization.
KEY is optional for functionality but important for security hardening.

---

## 381. Version Format Reference (v2 vs v3)

### 381.1 EncRatchetMessage Differences

| Component | v2 | v3 |
|-----------|----|----|
| emHeader length prefix | 1 byte (Word8) | 2 bytes (Word16 BE) |
| emHeader size | 123 bytes | 124 bytes |
| ehBody length prefix | 1 byte (Word8) | 2 bytes (Word16 BE) |
| ehBody size | 88 bytes | 88 bytes |
| MsgHeader has KEM | No | Yes ('0' = Nothing) |
| MsgHeader contentLen | 79 | 80 |
| MsgHeader padding | 7 bytes | 6 bytes |

### 381.2 encodeLarge Version Switch

```haskell
encodeLarge v bs
  | v < VersionE2E 3 = smpEncode (Str.length bs :: Word8) <> bs    -- 1 byte max 255
  | otherwise        = smpEncode (Str.length bs :: Word16) <> bs   -- 2 bytes max 65535
```

### 381.3 MsgHeader v3 Layout

```
[Word16 BE]     contentLen = 80
[Word16 BE]     msgMaxVersion = 2
[1 byte]        DH key length = 68
[68 bytes]      msgDHRs SPKI
[1 byte]        KEM Nothing = '0' (0x30)     ← NEW in v3
[Word32 BE]     msgPN
[Word32 BE]     msgNs
[6 bytes]       '#' padding
Total: 2 + 80 + 6 = 88 bytes (same as v2)
```

---

## 382. Session 21 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-06 | Ratchet State Update activated | LOG ONLY → ACTIVE |
| 2026-02-06 | SameRatchet/AdvanceRatchet split | Two decrypt modes |
| 2026-02-06 | 4 Header Key architecture | HKs/NHKs/HKr/NHKr |
| 2026-02-06 | HELLO sends, Server OK | First HELLO attempt |
| 2026-02-06 | Bug #20: PrivHeader fix | '_' → 0x00 for HELLO |
| 2026-02-06 | Bug #21: AgentVersion fix | v2 → v1 for AgentMessage |
| 2026-02-06 | Bug #22: prevMsgHash fix | Raw → smpEncode Word16 prefix |
| 2026-02-06 | Bug #23: cbEncrypt padding fix | unPad output → padded input |
| 2026-02-06 | Bug #24: DH Key fix | rcv_dh → snd_dh for HELLO |
| 2026-02-06 | Bug #25: PubHeader Nothing | Missing → '0' (0x30) |
| 2026-02-06 | App status: CONN NOT_AVAILABLE | Progress — HELLO processed |
| 2026-02-06 | KEY Command implemented | Server OK |
| 2026-02-06 | HELLO timing fix | After Confirmation receipt |
| 2026-02-07 | KEY proven not required | Queue unsecured |
| 2026-02-07 | RSYNC Crypto Error identified | chatItemNotFoundByContactId |
| 2026-02-07 | v2/v3 mismatch found | encodeLarge switch at v≥3 |
| 2026-02-07 | Bug #26: v3 format implemented | All bytes correct, Server OK |

---

## 383. Session 21 Statistics

| Metric | Value |
|--------|-------|
| Duration | 3 chats (main + 2 continuations) |
| Aufträge completed | 29 |
| Bugs found & fixed | 7 (Bug #20-#26) |
| Claude Code Analyses | Multiple (wire format, KEY, RSYNC, v3) |
| New architecture | 4 Header Keys, SameRatchet/AdvanceRatchet |
| v3 format | Implemented and verified |
| Connection status | Server OK, App crypto error |

---

## 384. Session 21 Summary

### What Was Achieved

- **Ratchet State Update** activated (no longer LOG ONLY)
- **4 Header Key Architecture** (HKs/NHKs/HKr/NHKr) with promotion
- **SameRatchet/AdvanceRatchet** split for correct ratchet handling
- **7 bugs fixed** (#20-#26): PrivHeader, AgentVersion, prevMsgHash, padding, DH key, PubHeader, v3 format
- **KEY Command** implemented (optional for unsecured queues)
- **HELLO timing** corrected (after Confirmation receipt)
- **v3 EncRatchetMessage** format implemented and byte-verified
- **RSYNC identified** as crypto error indicator

### What Was NOT Achieved

- App still shows "Connecting..." (not "Connected")
- App can't decrypt our HELLO (RSYNC Crypto Error)
- No Reply HELLO received from App

### Root Cause Suspects for Session 22

1. **HKs/NHKs Promotion** — may be using wrong header key for HELLO encryption
2. **E2E Version in Confirmation** — we may advertise v2, App expects v3
3. **DH Key Encoding** — v3 may use different key format

### Key Takeaway

```
SESSION 21 SUMMARY:
  - 7 bugs fixed in HELLO format (#20-#26)
  - v3 EncRatchetMessage: 2-byte prefixes + KEM Nothing
  - KEY command: optional (queues unsecured)
  - RSYNC = Ratchet Sync = crypto decrypt failure
  - 4 Header Keys: HKs/NHKs/HKr/NHKr with promotion
  - Server accepts everything — App crypto still fails

"Seven bugs, one format version, zero connections."
"The server says OK. The App disagrees."
"Evidence before fix. Always." 🐭
```

---

**DOCUMENT CREATED: 2026-02-07 Session 21 v35**  
**Status: v3 Format Implemented, App Still "Connecting..."**  
**Key Achievement: 7 HELLO format bugs fixed, v3 EncRatchetMessage verified**  
**Next: HKs/NHKs Promotion, E2E Version Clarification, Connection Establishment**
