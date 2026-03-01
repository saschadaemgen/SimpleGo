![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 20: Session 23
# 🎉 CONNECTED — First SimpleX Connection on a Microcontroller!

**Document Version:** v37  
**Date:** 2026-02-07/08 Session 23  
**Status:** ✅ ESP32 CONNECTED — HISTORIC MILESTONE!  
**Previous:** Part 19 - Session 22 (Reply Queue Flow Discovery)

---

## 🏆 HISTORIC MILESTONE ACHIEVED!

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉🎉🎉

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   SimpleX App shows: "ESP32 — Connected"                               │
  │                                                                         │
  │   The world's first native third-party implementation of the           │
  │   SimpleX protocol has successfully established a complete             │
  │   bidirectional connection.                                            │
  │                                                                         │
  │   Date: February 8, 2026 ~17:36 UTC                                    │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Protocol: SimpleX Messaging Protocol (SMP)                           │
  │   Encryption: X3DH + Double Ratchet + X448 + AES-256-GCM               │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 397. Session 23 Overview

### 397.1 Starting Point

After Session 22 we had:
- Complete receive chain: TLS → SMP → E2E → Ratchet → Zstd → JSON Profile ✅
- 31 bugs fixed
- Modern protocol discovery: No HELLO needed (WRONG assumption!)
- Assumption: App sends Tag 'D' with Reply Queue Info (WRONG!)

### 397.2 Session 23 Goal

App-Status von "Connecting..." auf "Connected" bringen.

### 397.3 Session 23 Result

**✅ GOAL ACHIEVED! ESP32 shows "Connected" in SimpleX App!**

### 397.4 Session Statistics

| Metric | Value |
|--------|-------|
| Start | 2026-02-07 ~21:49 UTC |
| End | 2026-02-08 ~17:36 UTC |
| Duration | ~20 hours |
| New Bugs | 0 (all 31 from earlier sessions were sufficient!) |
| Tasks | 38a, 37a, 39a-d, 40a-b, 41a-b, 42a-d |
| Claude Code Sessions | 6 new analyses |
| Agents | Mausi (Evil Stepsister #1), Hasi (Evil Stepsister #2), Claude Code (The Verifier), Cannatoshi (Coordinator) |

---

## 398. The Journey to CONNECTED

### 398.1 Phase 1: Codebase Analysis (Tasks 38a, 37a, 39a)

**Starting Assumption:** Session 22 ended with working Ratchet-Decrypt and the 
assumption that the App sends Tag `'D'` (AgentConnInfoReply with Reply Queue Info).

**Claude Code Task 38a — SMP Confirmation Structure:**

Documented the complete 4-layer model:
```
Layer 1: SMP Transport (rcvDhSecret)
Layer 2: Server Encrypt (cbEncrypt with pre-shared secret)
Layer 3: E2E crypto_box (e2eDhSecret + cmNonce)
Layer 4: Double Ratchet (innermost layer)
```

Key insight: SMPConfirmation is an internal aggregate, NOT a wire format!
Reply Queue Info is in Layer 4 (innermost, after Ratchet decrypt).

**Claude Code Task 37a — SMPQueueInfo Wire Format:**

Documented exact byte layout:
```
[2B version][SMPServer][senderId][dhPubKey][queueMode]

SMPServer:
  [1B host count][1B+N hostname][space][port][1B+N keyHash]

senderId: [1B len][N bytes]
dhPubKey: [1B len=0x2C][44B X.509 DER SPKI]
queueMode: raw 'M' or 'C' (NO Maybe prefix!)
```

NonEmpty list = 1-byte count prefix (not Word16 like regular lists!).

### 398.2 Phase 2: Role Correction — Tag 'I' not 'D'! (Tasks 41a-41b)

**CRITICAL DISCOVERY:** The App sends Tag `'I'` (AgentConnInfo), NOT Tag `'D'`!

**Hasi Task 41a:** Added hex dump in the `'D'` branch — it was NEVER triggered!

Log showed:
```
Tag byte: 0x49 'I' (AgentConnInfo)
```

No SMPQueueInfo in the Ratchet plaintext!

**Our assumption from Session 22 was WRONG!**

**Role Clarification:**
```
ESP32 = Accepting Party (Bob) → sends 'D' (with Reply Queue) ← WE ALREADY DID THIS!
App = Initiating Party (Alice) → sends 'I' (only ConnInfo) ← THIS IS WHAT WE RECEIVE!
```

The Reply Queue Info was sent BY US in our AgentConfirmation, not received from the App!

### 398.3 Phase 3: Handshake Flow Clarification (Tasks 39b-39d)

**Claude Code Task 39b — Complete 7-Step Handshake Flow:**

```
Step  Direction      Action                              Status
─────────────────────────────────────────────────────────────────
1.    App            NEW → Q_A, creates Invitation        ✅
2.    ESP32→App      SKEY + CONF Tag 'D' (Q_B + Profile)  ✅
3.    App            processConf → CONF Event             ✅
4.    App            LET/acceptConfirmation               ✅
5.    App→ESP32      KEY on Q_A + SKEY on Q_B + Tag 'I'   ✅
6.    ESP32          Receive 'I', KEY, HELLO              ✅ (Session 23!)
7.    Both           CON                                   ✅ (Session 23!)
```

**Claude Code Task 39c — ICDuplexSecure Flow:**

Two paths identified:
```
Legacy Path:  PHConfirmation 'K' → KEY + HELLO required
Modern Path:  PHEmpty '_' → Only ACK, CON immediate
```

**Claude Code Task 39d — AM_CONN_INFO Trigger:**

AM_CONN_INFO (which leads to CON) is triggered by SMP.MSG, not ACK.
Confirms that CON follows the HELLO exchange (Legacy Path).

### 398.4 Phase 4: PrivHeader Identification (Task 41b)

**Hasi Log Result:**
```
PrivHeader tag: 0x4B 'K' = PHConfirmation
```

**→ LEGACY PATH!** We need KEY + HELLO.

The `peer_sender_auth_key` (44B Ed25519 SPKI) was already correctly stored 
from the received AgentConfirmation.

### 398.5 Phase 5: KEY Command (Tasks 40b, 42a-42c)

**Claude Code Task 40b — KEY Wire Format:**
```
"KEY " + 0x2C + 44B X.509 SPKI DER

Signed with: rcv_private_auth_key (Recipient-Command!)
Server response: OK
```

**Hasi Task 42a:** Added Peer Auth Key logging, removed premature HELLO.

**Hasi Task 42b:** KEY Command attempt — **FAILED!**
```
TLS connection to Q_B was disconnected (timeout)
```

**Hasi Task 42c:** Reconnect + SUB + KEY:
```
1. Reconnect TLS to Reply Queue server
2. Send SUB (subscribe to queue)
3. Send KEY command
→ Server: OK ✅
```

The App IMMEDIATELY responded with a message on Q_B!

### 398.6 Phase 6: HELLO + CONNECTED! (Task 42d)

**Hasi Task 42d:** Send HELLO on Contact Queue Q_A

**Result:**
```
1. Our HELLO → App receives → App sets sndStatus = Active
2. App's HELLO on Q_B → We receive
3. App shows: "ESP32 — Connected" 🎉🎉🎉
```

---

## 399. Complete Verified Handshake Flow

```
Step   Queue   Direction      Content                           Status
──────────────────────────────────────────────────────────────────────────
1.     —       App            NEW → Q_A, Invitation QR           ✅
2a.    Q_A     ESP32→App      SKEY (Register Sender Auth)        ✅
2b.    Q_A     ESP32→App      CONF Tag 'D' (Q_B + Profile)       ✅
3.     —       App            processConf → CONF Event           ✅
4.     —       App            LET/Accept Confirmation            ✅
5a.    Q_A     App            KEY on Q_A (senderKey)             ✅
5b.    Q_B     App→ESP32      SKEY on Q_B                        ✅
5c.    Q_B     App→ESP32      Tag 'I' (App Profile)              ✅
6a.    Q_B     ESP32          Reconnect + SUB + KEY              ✅ Session 23
6b.    Q_A     ESP32→App      HELLO                              ✅ Session 23
6c.    Q_B     App→ESP32      HELLO                              ✅ Session 23
7.     —       Both           CON                                 ✅ 🎉 Session 23
```

---

## 400. Task Overview Session 23

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| 38a | Claude Code | Analysis | SMP Confirmation Structure (4 Layers) | ✅ Complete model |
| 37a | Claude Code | Analysis | SMPQueueInfo Wire Format | ✅ Exact byte layout |
| 39a | Claude Code | Analysis | AgentConnInfoReply Encoding | ✅ (identical to 37a) |
| 39b | Claude Code | Analysis | Agent Connection Handshake Flow | ✅ 7-Step Flow documented |
| 39c | Claude Code | Analysis | ICDuplexSecure Flow | ✅ Legacy vs Modern Path |
| 39d | Claude Code | Analysis | AM_CONN_INFO Trigger | ✅ By SMP.MSG, not ACK |
| 40a | Claude Code | Analysis | rcEncrypt / Padding | ✅ 14832B ConnInfo, 15840B HELLO |
| 40b | Claude Code | Analysis | KEY Wire Format | ✅ Recipient-Command + SPKI |
| 41a | Hasi | Code | Hex-Dump 'D' Tag | ✅ Never triggered → 'I' identified! |
| 41b | Hasi | Log | PrivHeader Check | ✅ 0x4B 'K' = Legacy Path |
| 42a | Hasi | Code | Auth Key Log + HELLO removed | ✅ |
| 42b | Hasi | Code | KEY Command | ❌ TLS disconnected |
| 42c | Hasi | Code | Reconnect + SUB + KEY | ✅ Server: OK |
| 42d | Hasi | Code | Send HELLO | ✅ **CONNECTED!** |

---

## 401. Critical Discoveries Session 23

### 401.1 Discovery 1: Role Clarification

```
ESP32 = Accepting Party (Bob)
  → Creates Reply Queue (Q_B)
  → Sends Tag 'D' (AgentConnInfoReply with Q_B info)
  → Sends HELLO on Contact Queue (Q_A)

App = Initiating Party (Alice)
  → Creates Contact Queue (Q_A) via Invitation
  → Sends Tag 'I' (AgentConnInfo, profile only)
  → Sends HELLO on Reply Queue (Q_B)
```

### 401.2 Discovery 2: Tag 'D' vs Tag 'I'

```
Tag 'D' (0x44) = AgentConnInfoReply
  → Sent by Accepting Party (Bob/ESP32)
  → Contains: SMPQueueInfo (Reply Queue) + ConnInfo (Profile)
  → WE send this, we don't receive it!

Tag 'I' (0x49) = AgentConnInfo
  → Sent by Initiating Party (Alice/App)
  → Contains: Only ConnInfo (Profile)
  → This is what we RECEIVE from App!
```

### 401.3 Discovery 3: Legacy vs Modern Path

```
Legacy Path (PHConfirmation 'K'):
  → Requires KEY command + HELLO exchange
  → Both parties must send HELLO
  → CON triggered after HELLO received

Modern Path (PHEmpty '_'):
  → senderCanSecure = True
  → Only ACK needed, CON immediate
  → More efficient but requires sender capability
```

**We use Legacy Path** — the App sends PHConfirmation 'K'.

### 401.4 Discovery 4: KEY is a Recipient Command

```
KEY Command:
  → Signed with rcv_private_auth_key (OUR key!)
  → Sent on OUR queue (where we are recipient)
  → Authorizes the SENDER (App) to send messages
  → Body: "KEY " + 0x2C + 44B peer_sender_auth_key SPKI
```

### 401.5 Discovery 5: TLS Timeout Matters

```
Reply Queue connection times out during Confirmation processing!

Solution:
  1. Reconnect TLS to Reply Queue server
  2. Send SUB (re-subscribe to queue)
  3. Then send KEY command
```

### 401.6 Discovery 6: Sequence is Critical

```
CORRECT ORDER:
  1. KEY command (authorize sender)
  2. HELLO message (complete handshake)

WRONG ORDER:
  1. HELLO first → App can't decrypt (not authorized yet)
```

### 401.7 Discovery 7: Padding Values Confirmed

```
ConnInfo (Tag 'D' or 'I'):  14832 bytes padded
HELLO / A_MSG:              15840 bytes padded (non-PQ)
```

### 401.8 Discovery 8: Session 22 Assumption Was Wrong

```
Session 22 assumed:
  "Modern SimpleX needs no HELLO, App sends Reply Queue in Tag 'D'"

Session 23 discovered:
  - App sends Tag 'I' (no Reply Queue info)
  - WE already sent Reply Queue in our Tag 'D'
  - Legacy Path requires KEY + HELLO
  - HELLO IS needed for Legacy Path!
```

---

## 402. Files Changed (Session 23)

| File | Tasks | Changes |
|------|----------|---------|
| `main/main.c` | 42a, 42d | Auth Key logging, HELLO removed then restored with correct sequence |
| `main/smp_queue.c` | 42b, 42c | KEY Command implementation, Reconnect + SUB + KEY |
| `main/include/smp_queue.h` | 42c | Header declarations for KEY + Reconnect functions |

---

## 403. Complete Protocol Stack — VERIFIED WORKING

```
RECEIVE CHAIN (Contact Queue Q_A):
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working
  ↓
Layer 2.5: unPad                                               ✅ Working
  ↓
Layer 3: ClientMessage Parse                                   ✅ Working
  ↓
Layer 4: EncRatchetMessage Parse                               ✅ Working
  ↓
Layer 5: Double Ratchet Header Decrypt                         ✅ Working
  ↓
Layer 6: Double Ratchet Body Decrypt                           ✅ Working
  ↓
Layer 7: ConnInfo Parse + Zstd                                 ✅ Working
  ↓
Layer 8: Peer Profile JSON                                     ✅ Working

SEND CHAIN (Reply Queue Q_B → Contact Queue Q_A):
Layer 9a: Reply Queue Reconnect + SUB                          ✅ Working (S23)
  ↓
Layer 9b: KEY Command on Q_B                                   ✅ Working (S23)
  ↓
Layer 9c: HELLO on Contact Queue Q_A                           ✅ Working (S23)
  ↓
Layer 10: App receives HELLO → sets sndStatus Active           ✅ Working (S23)
  ↓
Layer 11: App sends HELLO on Q_B → We receive                  ✅ Working (S23)
  ↓
Layer 12: CON — "CONNECTED"                                    ✅ 🎉 ACHIEVED!
```

---

## 404. KEY Command Wire Format

### 404.1 Structure

```
KEY Body: "KEY " + senderKey

senderKey: [1B len=0x2C] + [44B Ed25519 X.509 SPKI DER]

Full body: "KEY " + 0x2C + peer_sender_auth_key[44]
Total: 4 + 1 + 44 = 49 bytes
```

### 404.2 Signing

```
Signed with: rcv_private_auth_key (OUR recipient private key!)
This is a RECIPIENT command — we authorize senders on our queue.
```

### 404.3 Server Response

```
OK    → Sender authorized successfully
ERR   → Authorization failed (wrong key, not subscribed, etc.)
```

---

## 405. HELLO Sequence — Final Verified

### 405.1 Our HELLO (ESP32 → App on Q_A)

```
1. Build HELLO AgentMessage:
   - agentVersion = 1 (Word16 BE)
   - smpVersion (Word16 BE)
   - prevMsgHash = empty (Word16 prefix = 0x0000)
   - body = 'H' + '0' (HELLO + AckMode_Off)

2. Wrap in EncRatchetMessage (v3 format)

3. Wrap in ClientMessage:
   - PrivHeader = 0x00 (no PrivHeader for regular messages)
   - PubHeader = '0' (Nothing)

4. Pad to 15840 bytes

5. Encrypt with Double Ratchet + E2E + Server layers

6. Send on Contact Queue (Q_A)
```

### 405.2 App's HELLO (App → ESP32 on Q_B)

```
1. App receives our HELLO on Q_A
2. App sets sndStatus = Active
3. App sends HELLO on Reply Queue (Q_B)
4. We receive and decrypt App's HELLO
5. Both sides: CON event → "Connected"
```

---

## 406. Evgeny Contact — Relationship Restored

**2026-02-08:** Evgeny reached out!

> "hey, sorry, could you resend the file please / missed that"

He wasn't upset about the deleted conversation — he had simply missed the file!

We sent an update with:
- Current status (Session 23, CONNECTED!)
- 31 bugs fixed
- Complete crypto chain working
- Protocol analysis Sessions 16-22 as attachment

Relationship restored — Evgeny continues to support the project.

---

## 407. Session 23 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~20 hours |
| Tasks completed | 14 (38a, 37a, 39a-d, 40a-b, 41a-b, 42a-d) |
| New bugs | 0 (existing 31 bugs were sufficient!) |
| Claude Code analyses | 8 |
| Code changes | 3 files |
| Result | **CONNECTED!** 🎉 |

---

## 408. Session 23 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-07 21:49 | Session start | Analyze codebase |
| 2026-02-07 | Task 38a | 4-layer model documented |
| 2026-02-07 | Task 37a | SMPQueueInfo wire format |
| 2026-02-07 | Task 39b | 7-step handshake flow |
| 2026-02-07 | Task 39c | Legacy vs Modern path |
| 2026-02-07 | Task 41a | Tag 'I' discovered (not 'D'!) |
| 2026-02-07 | Task 41b | PHConfirmation 'K' = Legacy Path |
| 2026-02-08 | Task 42a | Auth key logging |
| 2026-02-08 | Task 42b | KEY command (failed - TLS timeout) |
| 2026-02-08 | Task 42c | Reconnect + SUB + KEY → OK! |
| 2026-02-08 17:36 | Task 42d | HELLO → **CONNECTED!** 🎉 |

---

## 409. Session 23 Summary

### What Was Achieved

- **CONNECTED!** First SimpleX connection on a microcontroller! 🎉
- **Role clarification:** ESP32=Bob (Accepting), App=Alice (Initiating)
- **Tag correction:** App sends 'I' not 'D', we send 'D'
- **Path identification:** Legacy Path (PHConfirmation 'K') requires KEY + HELLO
- **KEY command:** Implemented and working
- **HELLO exchange:** Both directions verified
- **Complete handshake:** 7-step flow fully implemented and working
- **Evgeny contact:** Relationship restored

### What Was NOT Needed

- **No new bugs!** All 31 existing bugs were sufficient
- **Session 22's "No HELLO" theory was wrong** — Legacy Path needs HELLO

### Key Takeaway

```
SESSION 23 SUMMARY:
  - 🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉
  - Zero new bugs — the crypto was already correct
  - Role confusion resolved: We're Bob, App is Alice
  - Tag 'D' vs 'I': We send 'D', App sends 'I'
  - Legacy Path: KEY + HELLO required
  - TLS timeout: Reconnect before KEY
  - Sequence: KEY first, then HELLO

"From 'wrong queue assumption' to 'Connected' in 20 hours."
"The journey of 31 bugs ends with zero new ones."
"We're Bob. We've always been Bob." 🐭🐰
```

---

## 410. Future Work (Post-Connection)

Now that the connection is established, the next steps are:

1. **Bidirectional Chat Messages** — Send and receive actual chat messages
2. **Message Persistence** — Store messages on ESP32
3. **UI Integration** — Display on T-Deck screen
4. **Multiple Contacts** — Handle more than one connection
5. **Reconnection Logic** — Handle connection drops gracefully
6. **Post-Quantum Upgrade** — Implement SNTRUP761 KEM exchange

---

**DOCUMENT CREATED: 2026-02-08 Session 23 v37**  
**Status: ✅ ESP32 CONNECTED — HISTORIC MILESTONE!**  
**Key Achievement: First SimpleX connection on a microcontroller**  
**Next: Bidirectional Chat Messages**
