![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 13: Session 16 (FINAL)
# ROOT CAUSE #2: SimpleX Custom XSalsa20 + Double Ratchet Problem

**Document Version:** v30  
**Date:** 2026-02-01 to 2026-02-03 Session 16  
**Status:** DOUBLE RATCHET PROBLEM IDENTIFIED  
**Previous:** Part 12 - Session 15 (Root Cause: Missing Key - DISPROVEN)

---

## 292. Session 16 Overview

### 292.1 Starting Point

Session 14 and 15 had **CONTRADICTORY THEORIES** about Bug #18.
Session 16 resolves which theory was correct.

### 292.2 Session 16 Goals

1. Understand PubHeader structure from Haskell source
2. Identify "sender's public DH key" in header
3. Calculate correct DH secret
4. Make Layer 2 decrypt work

### 292.3 Session 16 Achievements

**Multiple Key Discoveries:**
- Session 15 "missing message" theory was WRONG!
- SimpleX uses NON-STANDARD XSalsa20
- Real problem is Double Ratchet, not Layer 4
- Wire-Format, AAD, Keys all VERIFIED correct
- Problem narrowed to rcAD order or X3DH calculation

---

## 293. The Session 14/15 Contradictions

### 293.1 Byte [14] and [15] Interpretation

| Byte | Session 14 Said | Session 15 Said |
|------|-----------------|-----------------|
| `[14] = 0x31` | maybe_e2e = Just (KEY FOLLOWS!) | maybe_corrId = Just |
| `[15] = 0x2c` | SPKI Length = 44 | maybe_e2e = Nothing (NO KEY!) |

- **Session 14:** "The E2E key IS in the message at offset [28-59]!"
- **Session 15:** "There is NO E2E key in the message!"

### 293.2 DH Secret Verification

| Session 14 | Session 15 |
|------------|------------|
| DH Secret `d0b7b55c...` VERIFIED ✅ | All DH tests FAILED ❌ |
| "Python Match!" | "Key not present!" |

### 293.3 "Second Message" Theory

| Session 14 | Session 15 |
|------------|------------|
| "Handoff theory DISPROVEN!" | "Root Cause: 2nd MSG missing!" |
| "There is NO 2nd MSG!" | "The 2nd MSG doesn't arrive!" |

### 293.4 Resolution: Different Test Runs!

- **Session 14 Nonce:** `b2 1f a2 bc 0d bb 5c b0...`
- **Session 15 Nonce:** `96 ef 9b 41 57 27 bd 09...`

**These are DIFFERENT test runs!** Sessions analyzed different messages.

---

## 294. Evgeny's Hints (Authoritative Source)

Evgeny Poberezkin is the founder and lead developer of SimpleX Chat. His hints have priority over all Session 14/15 theories.

### 294.1 Hint 1: Updated Protocol Docs

> **Evgeny (2026-01-28 14:48):**
> "the updated protocol docs are in **rcv-services branch** of simplexmq, not merged to master yet"

### 294.2 Hint 2: PHConfirmation Contains the Key

> **Evgeny (2026-01-28 21:02):**
> "I think the key would be in **PHConfirmation**, no?"

### 294.3 Hint 3: THE DECISIVE HINT!

> **Evgeny (2026-01-28 21:11):**
> "you have to combine your **private DH key** (paired with public DH key in the invitation) with **sender's public DH key sent in confirmation header** - this is **outside of AgentConnInfoReply but in the same message**."

| Evgeny Says | Meaning |
|-------------|---------|
| "your private DH key" | `our_queue.e2e_private` |
| "sender's public DH key" | Key that App sends |
| "sent in confirmation header" | In **HEADER**, not body! |
| "outside of AgentConnInfoReply" | NOT in Double-Ratchet encrypted part |
| **"in the same message"** | **NO second message needed!** |

### 294.4 Session 15 Theory Was WRONG!

| Session 15 Claimed | Evgeny Says |
|--------------------|-------------|
| "2nd MSG missing" | "in the same message" |
| "Key never arrives" | "sender's public DH key sent in confirmation header" |
| "We don't receive the MSG" | Key is ALREADY in the message! |

### 294.5 Two Separate Crypto Layers

> **Evgeny (2026-01-28 21:15):**
> "The key insight you may be missing is that there are **TWO separate crypto_box decryption layers** before you reach the AgentMsgEnvelope"

| Layer | Key | Nonce | Purpose |
|-------|-----|-------|---------|
| **Layer 1** | `rcvDhSecret` (Server DH) | `cbNonce` | Server-to-Client |
| **Layer 2** | `e2eDhSecret` (E2E DH) | `cmNonce` | Per-Queue E2E |

---

## 295. Haskell Source Analysis

### 295.1 ClientMsgEnvelope (Protocol.hs:1067-1072)

```haskell
data ClientMsgEnvelope = ClientMsgEnvelope
  { cmHeader :: PubHeader,
    cmNonce :: C.CbNonce,      -- 24 bytes nonce for Layer 2
    cmEncBody :: ByteString    -- Encrypted body
  }
```

### 295.2 PubHeader (Protocol.hs:1074-1078)

```haskell
data PubHeader = PubHeader
  { phVersion :: VersionSMPC,
    phE2ePubDhKey :: Maybe C.PublicKeyX25519  -- <- THE E2E KEY!
  }
```

**`phE2ePubDhKey` is the "sender's public DH key" Evgeny mentioned!**

### 295.3 e2eDhSecret Calculation (Agent.hs:3379)

```haskell
e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey
```

### 295.4 Byte Layout After Layer 1 Decrypt

```
[0-1]    unPad Length (Word16 BE)
[2-3]    phVersion (Word16 BE)
[4]      Maybe tag: '1' (0x31) = Just, '0' (0x30) = Nothing
[5]      Key length: 0x20 (32) if Just
[6-37]   phE2ePubDhKey RAW (32 bytes) <- THE KEY!
[38-61]  cmNonce (24 bytes)
[62-77]  MAC (16 bytes Poly1305)
[78+]    Ciphertext
```

---

## 296. ROOT CAUSE #2: SimpleX Custom XSalsa20!

### 296.1 The Critical Difference

```
┌─────────────────────────────────────────────────────────────────────┐
│ Standard libsodium crypto_secretbox:                                │
│   HSalsa20(dh_secret, nonce[0:16])                                  │
├─────────────────────────────────────────────────────────────────────┤
│ SimpleX xSalsa20:                                                   │
│   HSalsa20(dh_secret, zeros[16])    <- ZEROS not nonce!             │
│   HSalsa20(subkey1, nonce[8:24])                                    │
│   Salsa20(subkey2, nonce[0:8])                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 296.2 Python Test Proof

```python
# Standard subkey:
2d4b4528855228d0abf137ea975a1a67997479d61404f9d2e52b05d0f9ee1979

# SimpleX subkey:
ce1b436c8b333a5ff881d4c09c55545594f1b73a214ea61c534936cd0d397912

# SUBKEYS ARE COMPLETELY DIFFERENT!
```

### 296.3 SimpleX Haskell Code (Crypto.hs)

```haskell
xSalsa20 (DhSecretX25519 shared) nonce msg = (rs, msg')
  where
    zero = B.replicate 16 (toEnum 0)           -- 16 ZERO bytes!
    (iv0, iv1) = B.splitAt 8 nonce             -- split at byte 8
    state0 = XSalsa.initialize 20 shared (zero `B.append` iv0)
    state1 = XSalsa.derive state0 iv1
```

### 296.4 Consequence

- `crypto_secretbox_open_easy()` → FAILS ❌
- `crypto_box_open_easy()` → FAILS ❌
- **All previous attempts were DOOMED to fail!**

---

## 297. Custom XSalsa20 Implementation

### 297.1 New Files Created

| File | Description |
|------|-------------|
| `simplex_crypto.c` | Custom XSalsa20 for ESP32 |
| `simplex_crypto.h` | Header with prototypes |
| `test_simplex_crypto.c` | C test program |
| `verify_simplex_crypto.py` | Python verification |

### 297.2 Implementation

```c
int simplex_secretbox_open(uint8_t *plain, const uint8_t *cipher, size_t len,
                           const uint8_t *nonce, const uint8_t *dh_secret) {
    uint8_t subkey1[32], subkey2[32];
    uint8_t zeros[16] = {0};
    
    // Step 1: HSalsa20(dh_secret, zeros[16])
    crypto_core_hsalsa20(subkey1, zeros, dh_secret, NULL);
    
    // Step 2: HSalsa20(subkey1, nonce[8:24])
    crypto_core_hsalsa20(subkey2, &nonce[8], subkey1, NULL);
    
    // Step 3: Salsa20 decrypt + Poly1305 verify
    // ...
}
```

### 297.3 Verification Results

```
DH Secret: 2fa438b42c75071d...
Subkey1:   ce1b436c8b333a5f...  ✅ MATCHES Python!
Subkey2:   987eee21058e15a9...  ✅ MATCHES Python!
Poly1305 MAC: VERIFIED ✅
Decrypted: "Hello from SimpleX!"
```

---

## 298. Claude Code Analysis Results (6 Analyses)

### 298.1 Analysis #1: Double Ratchet Specification

**rcAD (Associated Data):**
```haskell
rcAD = pubKeyBytes(sender_key1) || pubKeyBytes(receiver_key1)
```

| Key Type | Key Size | rcAD Size |
|----------|----------|-----------|
| X448 | 56 Bytes | **112 Bytes** |
| X25519 | 32 Bytes | 64 Bytes |

**X3DH DH Order:**

| # | Operation | Semantics |
|---|-----------|-----------|
| DH1 | `dh'(bob_identity, alice_ephemeral)` | Identity × Ephemeral |
| DH2 | `dh'(bob_ephemeral, alice_identity)` | Ephemeral × Identity |
| DH3 | `dh'(bob_ephemeral, alice_ephemeral)` | Ephemeral × Ephemeral |

**HKDF Derivation:**
```
Salt  = 64 × 0x00 (64 null bytes!)
IKM   = DH1 || DH2 || DH3
Info  = "SimpleXX3DH"
Output = 96 Bytes:
  [0-31]   = hk (Header Key Send)
  [32-63]  = nhk (Next Header Key Receive)  
  [64-95]  = root_key
```

### 298.2 Analysis #2: E2E Key Exchange

**e2eDhSecret is SIMPLE X25519!**
```c
// Crypto.hs:1262-1264
e2eDhSecret = X25519.dh(peer_pub, own_priv)
// NO KDF! NO custom XSalsa20! Direct as NaCl crypto_box key!
```

**maybe_e2e Flag Meaning:**
```
'1' + key-bytes = Confirmation (DH key transmitted)
'0' = Regular message (stored e2eDhSecret used)
```

### 298.3 Analysis #3-6: SimpleGo Code Review

**Bug Found: Key Race Condition!**
```c
// reply_queue_e2e_peer_public written from TWO places:

// 1. main.c:642 -> Contact Queue PubHeader SPKI (CORRECT!)
// 2. smp_parser.c:746 -> AgentConnInfoReply Parser (OVERWRITES!)

// Second write WINS - wrong key!
```

**Fix Applied:** Removed overwrite in smp_parser.c

---

## 299. The Real Problem: Double Ratchet

### 299.1 Self-Decrypt Test

```
E (52970) SMP_RATCH:    Payload decryption failed!
E (52970) SMP_PEER: Self-decrypt FAILED!
```

**But this is BY DESIGN - not a bug!**

Double Ratchet uses asymmetric header keys:

| Perspective | HKs (Send) | HKr (Receive) |
|-------------|------------|---------------|
| **Us (Alice)** | `hk` = kdf[0:32] | `nhk` = kdf[32:64] |
| **Peer (Bob)** | `nhk` = kdf[32:64] | `hk` = kdf[0:32] |

→ Sender CANNOT decrypt own message!

### 299.2 The Causal Chain

```
1. We send AgentConfirmation (Double Ratchet encrypted)
   └── Contains: encConnInfo with our e2e_public
   
2. Peer (SimpleX App) receives AgentConfirmation
   └── CANNOT DECRYPT! ← HERE IS THE PROBLEM!
   
3. Peer doesn't know our e2e_public
   └── He couldn't read encConnInfo
   
4. Peer sends message on Reply Queue anyway
   └── With which key? UNKNOWN!
   
5. We cannot decrypt
   └── We don't have the key the Peer used
```

### 299.3 App Status Proves This

| Platform | Status | Meaning |
|----------|--------|---------|
| **Desktop** | "Connecting" | Received Confirmation, trying |
| **Android** | "Request to connect" | ONLY saw Invitation, Confirmation NOT understood! |

**The Peer CANNOT decrypt our AgentConfirmation!**

---

## 300. Verified Components (All Correct!)

| Component | Status | Proof |
|-----------|--------|-------|
| Wire-Format Parsing | ✅ CORRECT | Hex-dump verified |
| Payload AAD (235 bytes) | ✅ CORRECT | Claude Code verified |
| Header AAD | ✅ CORRECT | Header decrypt works |
| emHeader Encoding | ✅ CORRECT | Version, IV, Tag, Body |
| Key Consistency | ✅ CORRECT | Creation = Sending = Decrypt |
| Custom XSalsa20 | ✅ VERIFIED | Round-trip success |

---

## 301. Where The Problem Is

Since Wire-Format and AAD are correct, problem must be in **cryptography**:

| Suspect | Probability | To Check |
|---------|-------------|----------|
| **rcAD Order** | HIGH | `our||peer` vs `peer||our` vs `initiator||responder` |
| **X3DH DH Order** | MEDIUM | DH1, DH2, DH3 calculation |
| **HKDF Salt/Info** | MEDIUM | "SimpleXX3DH" vs "SimpleXRootRatchet" |
| **Root Key Derivation** | MEDIUM | Output offsets (hk, nhk, rk) |
| **Chain Key Derivation** | LOW | Header encrypt works |

---

## 302. Tests Performed (12 Total)

| Test | Change | Result |
|------|--------|--------|
| 1 | Fresh test with key debug | FAILED |
| 2 | Python DH verification | DH MATCH ✅ |
| 3 | crypto_box_open_easy | FAILED |
| 4 | crypto_secretbox_open_easy with raw DH | FAILED |
| 5 | Selftest | PASS ✅ |
| 6 | Custom SimpleX XSalsa20 | FAILED |
| 7 | Offset fix for maybe_corrId/maybe_e2e | FAILED |
| 8 | Stored peer key from Contact Queue | FAILED |
| 9 | E2E key extraction before parse | Key extracted ✅ |
| 10 | smp_parser.c fix | Key not overwritten ✅ |
| 11 | Reply Queue key extraction fix | Key extracted ✅ |
| 12 | Custom XSalsa20 verification | Round-trip SUCCESS ✅ |

### 4 Different DH Keys Tested - ALL Failed

| Test | Key | Result |
|------|-----|--------|
| REPLY_E2E | `DH(e2e_private, sender_pub)` | MAC mismatch |
| REPLY_QUEUE_DH | `our_queue.shared_secret` | MAC mismatch |
| RCV_DH_x_SENDER | `DH(rcv_dh_private, sender_pub)` | MAC mismatch |
| CONTACT_DH | `DH(contact.rcv_dh_secret, sender_pub)` | MAC mismatch |

---

## 303. Bug #18 Sub-Issues Update

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a-p | Previous items | DONE |
| **#18q** | **SimpleX custom XSalsa20 discovered** | **DONE** |
| **#18r** | **simplex_crypto.c implemented** | **DONE** |
| **#18s** | **Custom XSalsa20 verified** | **DONE** |
| **#18t** | **Key race condition fixed** | **DONE** |
| **#18u** | **Wire-format parsing fixed** | **DONE** |
| **#18v** | **Self-decrypt failure explained** | **DONE** |
| **#18w** | **Problem is Double Ratchet** | **IDENTIFIED** |
| #18x | Fix rcAD order | TODO (S17) |
| #18y | Fix X3DH DH order | TODO (S17) |
| #18z | Decrypt Reply Queue | TODO (S17) |

---

## 304. Session 16 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~3 days |
| Code fixes implemented | 4 |
| Claude Code analyses | 6 |
| Components VERIFIED | 6 |
| New files created | 4 |
| Tests performed | 12 |
| **Bugs solved** | **0** (but narrowed!) |
| **Main suspect** | **rcAD order or X3DH** |

---

## 305. Session 16 Changelog

| Time | Change | Result |
|------|--------|--------|
| 02-01 | Protocol document created | Context documented |
| 02-01 | crypto_scalarmult -> crypto_box_beforenm | FAILED |
| 02-01 | Python DH verification | DH MATCH ✅ |
| 02-01 | crypto_box_open_easy | FAILED |
| 02-01 | Custom XSalsa20 discovered | ROOT CAUSE #2! |
| 02-02 | simplex_crypto.c implemented | Verified ✅ |
| 02-02 | Claude Code Analysis #1-2 | Spec obtained |
| 02-02 | rcAD order test | Regression - reverted |
| 02-03 | Claude Code Analysis #3-6 | Race condition found |
| 02-03 | smp_parser.c fix | Key overwrite fixed ✅ |
| 02-03 | Wire-format verified | All offsets correct ✅ |
| 02-03 | Problem identified | Double Ratchet rcAD/X3DH |

---

## 306. Session 16 Key Insights

### 306.1 What Was Learned

1. **Session 15 "missing message" theory was WRONG**
   - Evgeny: "in the same message"
   - The key IS in the message header

2. **SimpleX uses NON-STANDARD XSalsa20**
   - Standard: `HSalsa20(key, nonce[0:16])`
   - SimpleX: `HSalsa20(key, zeros[16])`
   - All libsodium crypto_box attempts were doomed!

3. **Layer 4 was FIXED, but Layer 5 is the problem**
   - Key extraction now works
   - Wire-format now correct
   - Double Ratchet is broken

4. **Self-decrypt failure is BY DESIGN**
   - Asymmetric header keys
   - Sender cannot decrypt own message

5. **Peer cannot decrypt our AgentConfirmation**
   - Android shows "Request to connect"
   - Desktop shows "Connecting"
   - Our Double Ratchet encryption is wrong

6. **Problem narrowed to rcAD or X3DH**
   - Wire-format: CORRECT
   - AAD: CORRECT
   - Keys: CORRECT
   - Crypto logic: SUSPECT

---

## 307. Next Steps (Session 17)

### 307.1 Priority 1: rcAD Order Verification

```
Questions for Claude Code:
1. What exactly is `sk1` and `rk1` in `rcAD = pubKeyBytes sk1 <> pubKeyBytes rk1`?
2. Is `sk1` the initiator or the sender of current message?
3. How are DH1, DH2, DH3 exactly calculated?
```

### 307.2 Priority 2: X3DH Parameter Comparison

Compare against Haskell:
- DH operation order
- HKDF salt (64×0x00?)
- HKDF info ("SimpleXX3DH"?)
- Output offsets (hk, nhk, rk)

### 307.3 Priority 3: Wait for Evgeny's Response

Question sent - waiting for answer.

---

## 308. Session 16 Summary

### What Was Achieved

- SimpleX custom XSalsa20 discovered and implemented
- Key race condition fixed
- Wire-format parsing fixed
- Components verified correct
- Problem narrowed to Double Ratchet rcAD/X3DH

### What Was NOT Achieved

- Bug #18 NOT fixed
- Reply Queue E2E decrypt still fails
- Connection still not established

### The Situation

```
Wire-Format: ✅ CORRECT
AAD:         ✅ CORRECT  
Keys:        ✅ CORRECT
Custom XSalsa20: ✅ VERIFIED

Problem is in crypto LOGIC:
- rcAD order? our||peer vs peer||our
- X3DH order? DH1, DH2, DH3
- HKDF params?

We are VERY CLOSE! All "obvious" errors excluded.
The bug is subtle - probably wrong role interpretation.
```

---

**DOCUMENT CREATED: 2026-02-03 Session 16 v30 FINAL**  
**Status: Double Ratchet Problem Identified**  
**Key Discovery: SimpleX uses NON-STANDARD XSalsa20!**  
**Next: Session 17 - rcAD/X3DH Verification**
