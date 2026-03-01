![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 8: Session 11
# Format Experiments Regression & Recovery

**Document Version:** v25  
**Date:** 2026-01-30 Session 11  
**Status:** RECOVERED - App shows "connecting" again  
**Previous:** Part 7 - Session 10C (Reply Queue Per-Queue DH Analysis)

---

## 218. Session 11 Overview - Regression & Recovery (2026-01-30)

### 218.1 Starting Point

After Session 10C we had achieved a working state:
- Bug #17 SOLVED (cmNonce instead of msgId)
- App status: **"connecting"**
- Developer (Evgeny) informed
- Server accepts all messages

### 218.2 What Happened?

In the following debug sessions, various format experiments were conducted
that led to a **regression** - the app status fell back to "request to connect".

---

## 219. Conducted Format Experiments (ALL WRONG!)

### 219.1 RATCHET_VERSION Change

**Experiment:**
```c
// From:
#define RATCHET_VERSION 2

// To:
#define RATCHET_VERSION 3
```

**Accompanying changes for Version 3:**
- em_header: 123 -> 124 bytes
- ehBody length prefix: 1 byte -> 2 bytes (HIGH byte 0x00 added)
- emHeader length: 0x7B -> 0x7C

**Result:** Regression to "request to connect"

### 219.2 Maybe Tag Encoding

**Experiment:**
```c
// From (CORRECT):
agent_msg[amp++] = '1';   // ASCII 0x31

// To (WRONG):
agent_msg[amp++] = 0x01;  // Binary 1
```

**Haskell Reference (correct):**
```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
  -- Nothing = '0' (0x30)
  -- Just x  = '1' (0x31) + encoded x
```

**Result:** App couldn't parse Maybe

### 219.3 Version Encoding (E2E Params)

**Experiment:**
```c
// From (CORRECT):
output[offset++] = 0x00;              // HIGH byte
output[offset++] = params->version_min;  // LOW byte = 0x02
// Result: 00 02 = Version 2

// To (WRONG):
output[offset++] = params->version_min;  // 0x02
output[offset++] = params->version_max;  // 0x02
// Result: 02 02 = Version 514!
```

**Result:** Version 514 instead of Version 2

### 219.4 DH Order Swap

**Experiment:** dh1/dh2 order swapped in X3DH

**Result:** Wrong shared secrets

### 219.5 Payload AAD Variations

**Experiment:** 236 bytes <-> 238 bytes (with/without length prefix)

**Result:** No improvement

---

## 220. Insight: Circular Debugging

### 220.1 The Problem

We went in circles:
1. Change A -> failed -> Change B
2. Change B -> failed -> back to A
3. Change A -> failed -> Change C
4. Change C -> failed -> back to A
5. ... and so on

### 220.2 Affected Changes

| Change | Back | Forth | Back | Forth |
|--------|------|-------|------|-------|
| DH Swap | Y | Y | - | - |
| Payload AAD | 238 | 236 | 238 | 236 |
| Maybe tag | '1' | 0x01 | '1' | 0x01 |
| Version | 00 02 | 02 02 | 00 02 | 02 02 |
| RATCHET_VERSION | 2 | 3 | 2 | 3 |

### 220.3 Root Cause

The Python tests had already proven that the cryptography is correct.
The format experiments were unnecessary and only broke working code.

---

## 221. Recovery: Git Reset + cmNonce Fix

### 221.1 Decision

Instead of more experiments: **Complete reset to last commit**

### 221.2 Execution

**Step 1: Reset everything**
```bash
cd /mnt/c/Espressif/projects/simplex_client
git checkout -- main/
```

**Step 2: Identify cmNonce Fix**

The cmNonce fix was the only necessary code:
```c
// WRONG (before Bug #17 fix):
memcpy(nonce, msg_id, msgIdLen);  // Wrong nonce!

// CORRECT (Bug #17 fix):
int cm_nonce_offset = spki_offset + 44;  // [60-83]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);  // Correct!
```

**Step 3: Re-insert TEST4**

The cmNonce fix was inserted as "TEST4" in main.c:
- Offset calculation for ClientMsgEnvelope structure
- cmNonce extraction at offset 60
- Per-queue E2E decryption with correct nonce

### 221.3 Result
```
TEST4 SUCCESS! Per-queue E2E decrypt worked!
   Decrypted 15904 bytes (ClientMessage)
   PrivHeader tag: 'K' (PHConfirmation)
```

**App status: "connecting"**

---

## 222. Current Working Code State

### 222.1 smp_ratchet.c
```c
#define RATCHET_VERSION         2       // Version 2!
uint8_t em_header[123];                 // 123 bytes!
em_header[hp++] = 0x00; 
em_header[hp++] = RATCHET_VERSION;      // ehVersion (Word16 BE)
em_header[hp++] = 0x58;                 // ehBody-len = 88 (1 BYTE for v2!)
output[p++] = 0x7B;                     // emHeader len = 123
```

### 222.2 smp_peer.c
```c
agent_msg[amp++] = '1';                 // ASCII '1' (0x31) for Maybe Just
```

### 222.3 smp_x448.c
```c
output[offset++] = 0x00;                // HIGH byte
output[offset++] = params->version_min; // LOW byte = 0x02
// Result: 00 02 = Version 2
```

### 222.4 main.c (TEST4 - cmNonce Fix)
```c
// ClientMsgEnvelope Structure:
// [0-1]   = length prefix
// [12-13] = version
// [14]    = maybe tag
// [15]    = maybe tag for e2ePubKey
// [16-59] = X25519 SPKI (44 bytes)
// [60-83] = cmNonce (24 bytes) <- CORRECT NONCE!
// [84+]   = cmEncBody

int cm_nonce_offset = spki_offset + 44;        // [60-83]
int cm_enc_body_offset = cm_nonce_offset + 24; // [84+]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
```

---

## 223. Verified Protocol Flow

### 223.1 Outgoing Messages (ESP32 -> App)

| Step | Status | Evidence |
|------|--------|----------|
| Create queue | OK | IDS response |
| Invitation link | OK | App can scan |
| AgentConfirmation | OK | Server: "OK" |
| HELLO | OK | Server: "OK" |
| App status | OK | "connecting" |

### 223.2 Incoming Messages (App -> ESP32)

| Step | Status | Evidence |
|------|--------|----------|
| Server-level decrypt | OK | 16106 bytes |
| Contact Queue (Invitation) | OK | TEST4 SUCCESS |
| Reply Queue (App response) | PENDING | Maybe=',' (Nothing) |
| Double Ratchet decrypt | PENDING | Receiver side missing |

---

## 224. Analysis: Reply Queue Message

### 224.1 Observation
```
Maybe tag = ',' (Nothing)
```

The Reply Queue message has **no e2ePubKey** (Maybe = Nothing).
This means: The message goes directly into Double Ratchet.

### 224.2 Difference to Contact Queue

| Queue | Maybe Tag | Meaning |
|-------|-----------|---------|
| Contact Queue | '1' (Just) | Has e2ePubKey, per-queue E2E layer |
| Reply Queue | ',' (Nothing) | No separate e2ePubKey, direct Double Ratchet |

### 224.3 Implication

For Reply Queue messages we need:
- **Double Ratchet Receiver side** implementation
- Decrypt header with `header_key_recv`
- Then decrypt payload with derived message key

---

## 225. Lessons Learned

### 225.1 What We Learned

1. **If it works, don't touch it!**
   - "connecting" status was achieved
   - Format experiments were unnecessary
   
2. **Git is your friend**
   - Commit at working state
   - Reset is faster than manual reverting

3. **Recognize circular debugging**
   - If you make the same changes multiple times -> STOP
   - Reset systematically

4. **Trust Python tests**
   - If crypto tests pass, crypto is not the problem
   - Format experiments waste time

### 225.2 Anti-Patterns Avoided

| Anti-Pattern | Description |
|--------------|-------------|
| "Shotgun Debugging" | Many changes at once |
| "Circular Changes" | Back and forth between options |
| "Ignoring Evidence" | Python tests showed: Crypto OK |
| "No Checkpoints" | No commit at working state |

---

## 226. Next Steps

### 226.1 Immediate

1. **Git Commit** - Save working state
```bash
git add -A
git commit -m "fix: restore working state - app shows connecting"
```

### 226.2 Short-term

2. **Double Ratchet Receiver** implementation
   - Header decrypt with `header_key_recv`
   - Message decrypt with derived key
   - Update ratchet state

3. **Reply Queue messages** decryption
   - Receive app's AgentConfirmation
   - Receive app's HELLO

### 226.3 Medium-term

4. **Bidirectional communication**
   - Send and receive messages
   - Chat functionality

---

## 227. Updated Bug Status

| Bug # | Description | Status |
|-------|-------------|--------|
| #1-#12 | Earlier encoding bugs | FIXED |
| #13 | Payload AAD prefix | FIXED |
| #14 | chainKdf IV order | FIXED |
| #15 | HSalsa20 key derivation | FIXED |
| #16 | A_CRYPTO header AAD | FIXED |
| #17 | cmNonce instead of msgId | FIXED |
| **#18** | **Reply Queue Double Ratchet** | **OPEN** |

---

## 228. Changelog Session 11

| Time | Change | Result |
|------|--------|--------|
| 2026-01-30 09:00 | RATCHET_VERSION 2->3 | Regression |
| 2026-01-30 09:15 | em_header 123->124 | Regression |
| 2026-01-30 09:30 | Maybe '1'->0x01 | Regression |
| 2026-01-30 09:45 | Version 00 02->02 02 | Regression |
| 2026-01-30 10:00 | Partial rollback | Still broken |
| 2026-01-30 10:30 | git checkout -- main/ | Clean slate |
| 2026-01-30 10:45 | TEST4 cmNonce re-added | "connecting" |

---

## 229. SimpleGo Version Update

```
SimpleGo v0.1.17-alpha - RECOVERED
===============================================================

Session 11 Summary:
- Format experiments caused regression
- Git reset restored clean state
- cmNonce fix (TEST4) re-applied
- App shows "connecting" again!

Anti-Patterns Learned:
- Don't experiment when it works
- Commit at working state
- Trust verified crypto tests
- Recognize circular debugging

Current Status:
- Outgoing messages: ALL WORKING
- Contact Queue decrypt: WORKING
- Reply Queue decrypt: PENDING (Double Ratchet receiver needed)
- App status: "connecting"

Next: Implement Double Ratchet receiver side

===============================================================
```

---

**DOCUMENT CREATED: 2026-01-30 Session 11 v25**  
**Status: RECOVERED - App shows "connecting" again!**  
**Next: Double Ratchet Receiver Implementation**
