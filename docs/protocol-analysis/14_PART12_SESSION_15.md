![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 12: Session 15 (FINAL)
# Bug #18: Root Cause Found - Missing E2E Key Exchange!

**Document Version:** v29  
**Date:** 2026-02-01 Session 15  
**Status:** ROOT CAUSE IDENTIFIED - Solution requires Session 16  
**Previous:** Part 11 - Session 14 (DH Secret Verified)

---

## 278. Executive Summary

Session 15 has **fully identified the root cause** for Bug #18: The E2E decryption of the incoming HELLO message on the Reply Queue fails because we need a **missing cryptographic key** that was **never sent to us** by the app (or that we don't receive/process).

**Core Problem in One Sentence:**  
The app sends its reply AgentConfirmation (containing the required `sndQueue.e2ePubKey`) on our Contact Queue, but we don't receive this second message.

---

## 279. Session 15 Goals

### Original Goal
Solve Bug #18 - `crypto_secretbox_open_detached` returns -1 when trying to decrypt the incoming HELLO message on the Reply Queue.

### Achieved
- ✅ Root cause fully identified
- ✅ Message structure fully understood
- ✅ Cryptographic calculations verified
- ❌ Bug not fixed (requires protocol change in Session 16)

---

## 280. Verified Components

### 280.1 X3DH Key Agreement - CORRECT ✅

Python verification confirms all calculations are identical:

| Component | Status | Verification |
|-----------|--------|--------------|
| DH1 (peer_key1, our_key1) | ✅ Match | `d3c59b74ee58f533...` |
| DH2 (peer_key1, our_key2) | ✅ Match | `06a9993d25f7ee4a...` |
| DH3 (peer_key2, our_key2) | ✅ Match | `97d10721aa154174...` |
| Header Key (HK) | ✅ Match | `08dc0b6ba9f8bbb9...` |
| Next Header Key (NHK) | ✅ Match | `67818dc678a6bed3...` |
| Root Key (RK) | ✅ Match | `b8866166ac6b1c1e...` |

### 280.2 Keypair Derivation - CORRECT ✅

```c
// Verified: e2e_public = scalarmult_base(e2e_private)
crypto_scalarmult_base(our_queue.e2e_public, our_queue.e2e_private);
```

### 280.3 Double Ratchet Encryption (Sending) - WORKS ✅

- AgentConfirmation accepted by server ("OK")
- HELLO accepted by server ("OK")
- Wire format v2 correctly implemented

### 280.4 Server-Level Decryption - WORKS ✅

```c
// shared_secret correctly calculated
// First encryption layer successfully decrypted
ESP_LOGI(TAG, "SMP-Level Decryption OK! (%d bytes)", plain_len);
```

### 280.5 Per-Queue E2E Decryption - FAILS ❌

```c
int ret = crypto_secretbox_open_detached(...);
// ret = -1 (MAC verification failed)
```

---

## 281. Root Cause Analysis

### 281.1 The Message Structure (Reply Queue HELLO)

After Server-Level Decryption, the message has this structure:

```
Offset  Hex                              Meaning
------  -------------------------------  ---------------------------
[0-1]   3e 82                            Length prefix (16002 bytes)
[2-9]   00 00 00 00 69 7f 46 bd          Padding/Timestamp
[10-11] 54 20                            Unknown flags
[12-13] 00 04                            phVersion = 4
[14]    31 ('1')                         maybe_corrId = Just
[15]    2c (',')                         maybe_e2e = Nothing (!)
[16-59] 30 2a 30 05 06 03 2b 65 6n...    corrId SPKI (44 bytes)
[60-83] 96 ef 9b 41 57 27 bd 09...       cmNonce (24 bytes)
[84+]   2e 39 0d 70...                   cmEncBody [MAC 16][Ciphertext]
```

### 281.2 Critical Discovery: `maybe_e2e = ','` (Nothing)

**What does `maybe_e2e = Nothing` mean?**

In Haskell code (Agent.hs, Line 2708-2721):
```haskell
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> do
    let e2eDh = C.dh' e2ePubKey e2ePrivKey
    decryptClientMessage e2eDh clientMsg
  (Just e2eDh, Nothing) -> do
    decryptClientMessage e2eDh clientMsg  -- ← OUR CASE!
```

When `maybe_e2e = Nothing`:
- **NO ephemeral e2ePubKey** in the message
- Message uses **pre-computed `e2eDhSecret`**
- This secret was created during connection establishment

### 281.3 How is `e2eDhSecret` Calculated?

Haskell code (Agent.hs, Line 3379):
```haskell
e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey
```

**App side:**
```
e2eDhSecret = DH(our_queue.e2e_public, app.sndQueue.e2ePrivKey)
```

**ESP32 side (what we need):**
```
dh_secret = DH(app.sndQueue.e2ePubKey, our_queue.e2e_private)
```

### 281.4 The Missing Puzzle Piece: `app.sndQueue.e2ePubKey`

**Where does this key come from?**

The app generates a keypair for its `SndQueue`:
- `e2ePrivKey` (private, stays with the app)
- `e2ePubKey` (public, must be sent to us)

**Where is the key sent?**

According to SimpleX protocol, `e2ePubKey` is sent in the **AgentConfirmation** from the app, which arrives on our **Contact Queue** as a response to our connection request.

---

## 282. Protocol Flow Analysis

### 282.1 Expected Flow (Duplex Handshake)

```
┌─────────────────┐                          ┌─────────────────┐
│     ESP32       │                          │   SimpleX App   │
└────────┬────────┘                          └────────┬────────┘
         │                                            │
         │  1. INVITATION (Contact Queue)             │
         │ <──────────────────────────────────────────│
         │     contains: peer.dh_public, X448 keys    │
         │                                            │
         │  2. AgentConfirmation (Peer's Queue)       │
         │ ──────────────────────────────────────────>│
         │     contains: our_queue info, E2E params   │
         │                                            │
         │  3. HELLO (Peer's Queue)                   │
         │ ──────────────────────────────────────────>│
         │                                            │
         │  4. App's AgentConfirmation (Contact Queue)│
         │ <──────────────────────────────────────────│ ← MISSING!
         │     contains: app.sndQueue.e2ePubKey (!)   │
         │                                            │
         │  5. App's HELLO (Reply Queue)              │
         │ <──────────────────────────────────────────│
         │     encrypted with pre-computed secret     │
         │                                            │
```

### 282.2 What Actually Happens

```
✅ Step 1: INVITATION received on Contact Queue
✅ Step 2: AgentConfirmation sent → Server: "OK"
✅ Step 3: HELLO sent → Server: "OK"
❌ Step 4: App's AgentConfirmation NOT RECEIVED!
❌ Step 5: App's HELLO received, but cannot decrypt (missing key)
```

### 282.3 Log Evidence

```
I (28982) SMP_PEER: ║ CONFIRMATION ACCEPTED BY SERVER! ║
I (29642) SMP_HAND: HELLO sent! Waiting for response...
I (29742) SMP_HAND: HELLO accepted by server!
I (30582) SMP:      Message on REPLY QUEUE from peer!
E (31702) SMP:      ❌ E2E decrypt FAILED (ret=-1)
```

**No second "MESSAGE RECEIVED for [ESP32]!" after HELLO sent!**

---

## 283. Tests Performed

### 283.1 Test 1: Key from URL (`pending_peer.dh_public`)

```python
our_e2e_private = "7e0da4cc5b7819e38f384cf2e2a173c3..."
pending_peer_dh = "edda0d5592eb36a9b673c98e660c3db7..."
dh_secret = crypto_scalarmult(our_e2e_private, pending_peer_dh)
# Result: 685e7514ce6af69f...
# Decrypt: FAILED
```

### 283.2 Test 2: Key from Message (Offset 28, corrId SPKI)

```python
msg_key = "746d027dc8580c8677d23a9775e69097..."
dh_secret = crypto_scalarmult(our_e2e_private, msg_key)
# Result: 3863509c8f2e2307...
# Decrypt: FAILED
```

### 283.3 Test 3: Brute-Force All Nonce Offsets (48-80)

```python
for nonce_off in range(48, 82):
    # Tested with both keys
    # All combinations: FAILED
```

### 283.4 Test 4: Search for X25519 SPKI in Entire INVITATION

```c
ESP_LOGI(TAG, "Searching ALL X25519 SPKI in decrypted:");
// Result: Total X25519 keys found: 0 (after the URI)
```

**Conclusion of all tests:** The required key is NOT in the data we have.

---

## 284. Key Insights

### 284.1 Two Different Key Types

| Key | Source | Usage |
|-----|--------|-------|
| `dh=` from URL | INVITATION URI | SMP-Level Handshake for App's Contact Queue |
| `sndQueue.e2ePubKey` | App's AgentConfirmation | Per-Queue E2E Encryption for Reply Queue |

### 284.2 Maybe-Encoding in SimpleX

```
'0' (0x30) = Nothing (no value)
'1' (0x31) = Just (value follows)
',' (0x2C) = Nothing (alternative marker) OR length byte (44)
```

**Important:** In our case, `[15] = 0x2C` is the **maybe_e2e tag** (Nothing), NOT a length!

### 284.3 Message Structure when `maybe_e2e = Nothing`

```
The message contains NO ephemeral e2ePubKey!
Instead, a pre-computed shared secret is used,
which was calculated during connection establishment from exchanged keys.
```

### 284.4 X25519 SPKI Format

```
30 2a 30 05 06 03 2b 65 6e 03 21 00 [32 bytes raw key]
└─────────────── Header (12 bytes) ──────────────┘
```

- OID `2b 65 6e` = 1.3.101.110 = X25519
- Total length: 44 bytes (12 Header + 32 Key)

---

## 285. Session 15 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~4 hours |
| Debugging iterations | 15+ |
| Python tests | 8 |
| Hex dump analyses | 12 |
| Haskell source searches | 20+ |
| Wrong hypotheses discarded | 6 |
| Root cause found | YES |
| Bug fixed | NO |

---

## 286. Recommendations for Session 16

### Priority 1: Receive Second Contact Queue Message

**Problem:** After sending AgentConfirmation + HELLO, we don't receive a second message on the Contact Queue.

**Possible Causes:**
1. We don't wait long enough
2. The app sends the confirmation via another channel
3. There's a bug in the Subscribe/Receive loop
4. The app expects a different message sequence

**Actions:**
- [ ] After HELLO sent, continue waiting on Contact Queue (not just Reply Queue)
- [ ] Increase timeout
- [ ] Extend logging to see ALL incoming messages

### Priority 2: Verify Haskell Protocol

**Questions to clarify:**
- When exactly does the app send its AgentConfirmation back?
- Is `sndQueue.e2ePubKey` really contained in it?
- Are there alternative ways the key is transmitted?

**Actions:**
- [ ] Analyze `smpConfirmation` function in Agent.hs
- [ ] Analyze `enqueueConfirmation` function
- [ ] Consult protocol documentation (agent-protocol.md)

### Priority 3: Check Alternative Key Sources

**Hypotheses:**
- Maybe the key is already hidden in the INVITATION (unlikely after test)
- Maybe the app generates a deterministic key based on other data
- Maybe there's a fallback mechanism

---

## 287. Open Questions

1. **Why doesn't a second message arrive on the Contact Queue?**
   - Timing problem?
   - Wrong subscribe status?
   - App waiting for something else?

2. **Where exactly is `sndQueue.e2ePubKey` exchanged?**
   - In which message?
   - In which format?

3. **Is the protocol correctly implemented?**
   - Is a step missing in the handshake?
   - Is the sequence wrong?

---

## 288. Bug #18 Status Update

### 288.1 Sub-Issues (Updated Session 15)

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | Parsing fix (correct offsets) | DONE |
| #18d | HSalsa20 difference identified | DONE |
| #18e | MAC position difference identified | DONE |
| #18f | 5 crypto approaches tested (S13) | DONE - All fail |
| #18g | SMPConfirmation contains e2ePubKey | FOUND |
| #18h | Handoff theory DISPROVEN | DONE (S14) |
| #18i | Wrong key bug fixed | DONE (S14) |
| #18j | Wrong DH function fixed | DONE (S14) |
| #18k | DH SECRET VERIFIED with Python! | DONE (S14) |
| **#18l** | **maybe_e2e = Nothing discovered** | **DONE (S15)** |
| **#18m** | **Missing key exchange identified** | **DONE (S15)** |
| **#18n** | **Protocol flow analyzed** | **DONE (S15)** |
| #18o | Receive App's AgentConfirmation | TODO (S16) |
| #18p | Extract sndQueue.e2ePubKey | TODO (S16) |

### 288.2 Root Cause Summary

```
ROOT CAUSE IDENTIFIED:
======================

1. App sends HELLO on our Reply Queue with maybe_e2e = Nothing
2. This means: NO ephemeral key in message
3. Message encrypted with PRE-COMPUTED e2eDhSecret
4. To decrypt, we need: app.sndQueue.e2ePubKey
5. This key should come in: App's AgentConfirmation on Contact Queue
6. PROBLEM: We don't receive this message!

SOLUTION: Receive and process the second Contact Queue message
```

---

## 289. Relevant Code Locations

### 289.1 ESP32 Code

| File | Lines | Function |
|------|-------|----------|
| `main.c` | 545-560 | maybe_corrId/maybe_e2e parsing |
| `main.c` | 820-900 | maybe_e2e == ',' handler |
| `smp_parser.c` | 24-50 | `extract_full_invitation_uri()` |
| `smp_parser.c` | 307-400 | INVITATION parsing |
| `smp_parser.c` | 383 | `pending_peer.dh_public` extraction |
| `smp_queue.c` | 27-28 | `reply_queue_e2e_peer_public` definition |
| `smp_ratchet.c` | complete | Double Ratchet implementation |

### 289.2 Haskell Reference (simplexmq)

| File | Lines | Relevance |
|------|-------|-----------|
| `Agent.hs` | 2686 | RcvQueue with e2eDhSecret |
| `Agent.hs` | 2708-2721 | e2eDhSecret vs e2ePubKey logic |
| `Agent.hs` | 3379 | e2eDhSecret calculation |
| `Agent.hs` | 3369-3389 | SndQueue keypair generation |
| `Protocol.hs` | 1310-1330 | SMPQueueAddress with dhPublicKey |
| `Crypto.hs` | 1302 | cbDecrypt implementation |

---

## 290. Session 15 Key Insights

1. **`maybe_e2e = Nothing` means no ephemeral key**
   - Message uses pre-computed shared secret
   - We need the app's sndQueue.e2ePubKey to compute it

2. **Two-phase key exchange**
   - Phase 1: X3DH for Double Ratchet (working!)
   - Phase 2: E2E key for per-queue encryption (MISSING!)

3. **The key we need is NOT in the INVITATION**
   - Searched entire payload - no X25519 keys after URI
   - Key must come from separate message

4. **App's AgentConfirmation is required**
   - Contains the missing sndQueue.e2ePubKey
   - Should arrive on Contact Queue after our HELLO
   - WE DON'T RECEIVE IT!

5. **All crypto is correct**
   - X3DH verified with Python
   - Double Ratchet sending works (server: OK)
   - Server-level decrypt works
   - Only missing: the key for E2E layer

---

## 291. Session 15 Summary

### What Was Achieved

- ✅ Root cause **fully identified**
- ✅ Message structure completely understood
- ✅ maybe_e2e = Nothing behavior documented
- ✅ Missing key exchange identified
- ✅ Protocol flow analyzed
- ✅ All existing crypto verified as correct

### What Is Still Open

- ❌ Bug not fixed (cannot fix without the key!)
- ❌ App's AgentConfirmation not received
- ❌ sndQueue.e2ePubKey not available

### The Core Problem

```
We need: app.sndQueue.e2ePubKey
It's in: App's AgentConfirmation on Contact Queue
Problem: WE DON'T RECEIVE THIS MESSAGE!
```

### Next Steps (Session 16)

1. Continue listening on Contact Queue after HELLO
2. Increase timeouts
3. Debug why second message doesn't arrive
4. Once received: extract sndQueue.e2ePubKey
5. Compute correct DH secret
6. Finally decrypt Reply Queue messages!

---

**DOCUMENT CREATED: 2026-02-01 Session 15 v29 FINAL**  
**Status: ROOT CAUSE IDENTIFIED**  
**Key Finding: Missing App's AgentConfirmation with sndQueue.e2ePubKey**  
**Next: Session 16 - Receive Second Contact Queue Message**
