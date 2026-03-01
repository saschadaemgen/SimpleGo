![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 14: Session 17 (FINAL)
# Evgeny Already Answered! + Key Consistency Debug

**Document Version:** v31  
**Date:** 2026-02-04 Session 17  
**Status:** KEY CONSISTENCY INVESTIGATION  
**Previous:** Part 13 - Session 16 (Custom XSalsa20 + Double Ratchet)

---

## 311. Session 17 Overview

### 311.1 Starting Point

After Session 16 we had:
- SimpleX custom XSalsa20 discovered and implemented
- Wire-format, AAD, Keys all VERIFIED correct
- Problem identified as Double Ratchet
- Suspects: rcAD order, X3DH DH order, HKDF params

### 311.2 Session 17 Critical Realization

**WE ASKED EVGENY A QUESTION HE HAD ALREADY ANSWERED!**

Evgeny answered on January 28, 2026 - we just didn't implement his answer correctly.

### 311.3 Session 17 Achievements

- rcAD order analyzed (Claude Code)
- Reply Queue two-layer architecture clarified
- Length prefix difference discovered (Contact vs Reply Queue)
- Key consistency investigation started
- Evgeny's previous answers re-analyzed

---

## 312. Evgeny Already Answered! (Jan 28, 2026)

### 312.1 The Question

"Which key for Reply Queue E2E?"

### 312.2 Answer 1

> "To your question, most likely A"
> (Option A = peer_ephemeral + our rcv_dh_private)

### 312.3 Answer 2

> "you have to combine your private DH key (paired with public DH key in the invitation) with sender's public DH key sent in confirmation header - this is outside of AgentConnInfoReply but in the same message."

### 312.4 Answer 3

> "The key insight you may be missing is that there are TWO separate crypto_box decryption layers before you reach the AgentMsgEnvelope, and they use different keys and different nonces."

### 312.5 What This Means

- Key is in the **confirmation header** (the SPKI in message header)
- **Outside of AgentConnInfoReply but in the same message**
- TWO crypto_box layers with different keys and nonces

---

## 313. Claude Code Analysis Results

### 313.1 Analysis #1: rcAD Order

```
rcAD = sk1 || rk1 = JOINER_KEY || INITIATOR_KEY
     = APP_KEY    || ESP32_KEY
     = PEER       || OUR
```

**But:** Test with PEER||OUR was WORSE than OUR||PEER!

**Status:** Staying with OUR||PEER (works better empirically)

### 313.2 Analysis #2: Reply Queue Two Layers

| Layer | DH Secret | Nonce | Purpose |
|-------|-----------|-------|---------|
| Layer 1 | `rcvDhSecret` | `cbNonce(msgId)` | Server→Recipient |
| Layer 2 | `e2eDhSecret` | `cmNonce` (RANDOM from Envelope!) | Sender→Recipient E2E |

**Critical:** cmNonce is NOT calculated - it's DIRECTLY in the message!

### 313.3 Analysis #3: Two Keypairs at Queue Creation

| Keypair | Purpose | Field | Layer |
|---------|---------|-------|-------|
| A | SMP Transport | `rcvDhSecret` | Layer 1 |
| B | E2E | `e2ePrivKey` | Layer 2 |

**Fact:** Both are generated at queue creation and NEVER changed!

---

## 314. Reply Queue Length Prefix Discovery

### 314.1 The Difference

```
Contact Queue: No length prefix before ClientMsgEnvelope
Reply Queue:   2-byte length prefix (e.g. 0x3E82 = 16002)
```

### 314.2 Fix Applied

Length prefix detected and offset adjusted (+2 bytes).

---

## 315. Key Consistency Investigation

### 315.1 The Observation

Logs showed different e2e_private keys:
```
Queue Creation:      e2e_private = c4cd6fd7...
Reply Queue Decrypt: e2e_private = 6156a27f...
```

### 315.2 Clarification

| Variable | Status |
|----------|--------|
| `reply_queue_e2e_peer_public` (PEER key) | WAS overwritten (fixed in S16) |
| `our_queue.e2e_private` (OUR key) | NOT overwritten (Claude Code confirmed) |

Possible explanations:
- Different test runs (not same session)
- Wrong logging (wrong variable logged)
- Something overlooked

### 315.3 Debug Test Implemented

```c
// At Queue Creation:
ESP_LOGI(TAG, "QUEUE_CREATE e2e_private: %02x%02x%02x%02x...",
    our_queue.e2e_private[0], our_queue.e2e_private[1],
    our_queue.e2e_private[2], our_queue.e2e_private[3]);

// At Reply Queue Decrypt:
ESP_LOGI(TAG, "REPLY_DECRYPT e2e_private: %02x%02x%02x%02x...",
    our_queue.e2e_private[0], our_queue.e2e_private[1],
    our_queue.e2e_private[2], our_queue.e2e_private[3]);
```

### 315.4 Expected Outcomes

| If Keys EQUAL | If Keys DIFFERENT |
|---------------|-------------------|
| Problem is sender_pub extraction | our_queue.e2e_private gets overwritten |
| Check which key we read from message | Find where and why |

---

## 316. Haskell Reference: Layer 2 Decrypt Flow

### 316.1 Key Source Files

| File | Lines | Content |
|------|-------|---------|
| Store.hs | 68-104 | RcvQueue with rcvDhSecret, e2ePrivKey, e2eDhSecret |
| Client.hs | 1378-1417 | newRcvQueue_: Queue creation, both keypairs |
| Client.hs | 1682-1686 | decryptSMPMessage: Layer 1 |
| Client.hs | 1936-1946 | agentCbEncryptOnce: Sender side |
| Agent.hs | 2685-2740 | processSMP: MSG handling |
| Agent.hs | 2885-2896 | decryptClientMessage: Layer 2 |

### 316.2 Layer 2 Decrypt Flow

```
1. Parse ClientMsgEnvelope from Layer 1 output
2. Extract e2ePubKey_ (sender's ephemeral) from PubHeader
3. Get e2ePrivKey from RcvQueue (the key from queue creation!)
4. e2eDh = DH(sender_ephemeral_pub, our_e2e_private)
5. plaintext = crypto_box_open(cmEncBody, cmNonce, e2eDh)
```

---

## 317. Current MAC Mismatch Data

```
Expected MAC: ed319f1858168cfc18ceeb255d414f77
Computed MAC: f48d300da052597801d9c3a721b9e86a
```

These are completely different - indicates wrong key, not wrong offset.

---

## 318. What Works vs What Doesn't

### 318.1 Working Components

- TLS 1.3, SMP Handshake, Queue Creation
- Contact Queue Decrypt (both layers)
- X3DH Key Agreement
- Double Ratchet Header Encrypt
- Server accepts our messages (OK#)
- Reply Queue Layer 1 (Server) Decrypt
- Length prefix recognized and skipped

### 318.2 Not Working

- Reply Queue Layer 2 (E2E) Decrypt → MAC Mismatch
- App shows "Connecting" / "Request to connect"

---

## 319. Session 17 Mistakes

### 319.1 Mistakes Made

1. **Asked a question Evgeny had already answered**
   - Must read Evgeny conversation COMPLETELY before asking new questions

2. **Too many theories without proof**
   - rcAD order → tested, both directions have problems
   - Header key swap → not tested (superseded by key bug)

3. **Contradictory statements**
   - "Code overwrites key at 4 places" vs "Key is not overwritten"
   - Clarification: `reply_queue_e2e_peer_public` (PEER) was overwritten
   - `our_queue.e2e_private` (OUR) is NOT overwritten

---

## 320. Bug #18 Sub-Issues Update

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a-w | Previous items | DONE |
| **#18x** | **rcAD order analyzed (S17)** | **DONE - staying OUR\|\|PEER** |
| **#18y** | **Length prefix fix (S17)** | **DONE** |
| **#18z** | **Key consistency check (S17)** | **INVESTIGATING** |

---

## 321. Session 17 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~1 day |
| Claude Code analyses | 3 |
| Code fixes | 1 (length prefix) |
| Key discoveries | 3 |
| Evgeny questions re-analyzed | 3 |
| **Bug #18 solved** | **NO** |

---

## 322. Next Steps

### 322.1 Immediate: Debug Test Result

1. Wait for debug test output
2. If keys EQUAL: Problem is sender_pub extraction
3. If keys DIFFERENT: Find where our_queue.e2e_private is overwritten

### 322.2 After Fix

- Reply Queue E2E should decrypt
- App should show "Connected"

---

## 323. Session 17 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-04 | Claude Code: rcAD analysis | PEER\|\|OUR should be correct |
| 2026-02-04 | Test: PEER\|\|OUR | WORSE! Back to OUR\|\|PEER |
| 2026-02-04 | Evgeny chat re-analyzed | Answer was already given! |
| 2026-02-04 | Claude Code: Reply Queue flow | Two layers, cmNonce is RANDOM |
| 2026-02-04 | Claude Code: Keypair analysis | Two keypairs, never changed |
| 2026-02-04 | Length prefix fix | Reply Queue has 2-byte prefix |
| 2026-02-04 | Key mismatch found in logs | Debug test started |

---

## 324. Session 17 Summary

### What Was Achieved

- Realized Evgeny already answered our question
- rcAD order analyzed (staying with OUR||PEER)
- Reply Queue two-layer architecture clarified
- Length prefix difference fixed
- Key consistency debug started

### What Was NOT Achieved

- Bug #18 NOT fixed
- Reply Queue E2E decrypt still fails

### Key Takeaway

```
EVGENY ALREADY ANSWERED:
  - Key is in "confirmation header" (SPKI in message header)
  - "outside of AgentConnInfoReply but in the same message"
  - TWO crypto_box layers with different keys and nonces

RULE: ALWAYS search past Evgeny conversations before asking new questions!
```

---

**DOCUMENT CREATED: 2026-02-04 Session 17 v31 FINAL**  
**Status: Key Consistency Investigation**  
**Key Lesson: Evgeny already answered - read ALL previous responses!**  
**Next: Debug test result → Fix sender_pub or find key overwrite**
