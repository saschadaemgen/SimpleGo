![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 9: Session 12
# Reply Queue E2E Keypair Fix Attempt

**Document Version:** v26  
**Date:** 2026-01-30 Session 12  
**Status:** FIX IMPLEMENTED - STILL FAILING  
**Previous:** Part 8 - Session 11 (Regression & Recovery)

---

## 230. Session 12 Overview (2026-01-30)

### 230.1 Starting Point

After Session 11 we had:
- Outgoing messages: WORKING (Server accepts with "OK")
- Contact Queue decrypt: WORKING (TEST4 SUCCESS)
- Reply Queue decrypt: FAILING
- App Status: "connecting"

The problem: Reply Queue messages could not be decrypted.

---

## 231. Root Cause Analysis (Haskell Code)

### 231.1 Discovery: Two Separate X25519 Keypairs

Through analysis of `~/simplexmq/src/Simplex/Messaging/Agent/Client.hs` discovered:

**Line 1357-1361:**
```haskell
newRcvQueue c nm userId connId srv vRange cMode enableNtfs subMode = do
  let qrd = case cMode of SCMInvitation -> CQRMessaging Nothing; SCMContact -> CQRContact Nothing
  e2eKeys <- atomically . C.generateKeyPair =<< asks random  -- SEPARATE E2E KEYPAIR!
  newRcvQueue_ c nm userId connId srv vRange qrd enableNtfs subMode Nothing e2eKeys
```

**Line 1416:**
```haskell
qUri = SMPQueueUri vRange $ SMPQueueAddress srv sndId e2eDhKey queueMode
```

**Conclusion:** Haskell generates TWO separate X25519 keypairs:

| Keypair | Purpose | Used in |
|---------|---------|---------|
| dhKey / privDhKey | Server-level DH (NEW command) | rcvDhSecret |
| e2eDhKey / e2ePrivKey | E2E-level DH (Peer encryption) | SMPQueueAddress |

### 231.2 RcvQueue Structure (Store.hs Line 123-125)

```haskell
rcvSMPQueueAddress :: RcvQueue -> SMPQueueAddress
rcvSMPQueueAddress RcvQueue {server, sndId, e2ePrivKey, queueMode} =
  SMPQueueAddress server sndId (C.publicKey e2ePrivKey) queueMode
```

The e2e_public key is derived from e2ePrivKey and sent in SMPQueueAddress.

### 231.3 E2E Decryption Flow (Agent.hs Line 2708-2710)

```haskell
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> do
    let e2eDh = C.dh' e2ePubKey e2ePrivKey  -- E2E DH calculation!
    decryptClientMessage e2eDh cmNonce cmEncBody
```

---

## 232. Implemented Changes

### 232.1 Structure Extended (main/include/smp_queue.h)

**Before:**
```c
typedef struct {
    uint8_t rcv_dh_public[DH_PUBLIC_SIZE];    // X25519 public
    uint8_t rcv_dh_private[DH_PUBLIC_SIZE];   // X25519 private
    uint8_t shared_secret[DH_PUBLIC_SIZE];
    // ...
} our_queue_t;
```

**After (line 36):**
```c
typedef struct {
    uint8_t rcv_dh_public[DH_PUBLIC_SIZE];    // X25519 public
    uint8_t rcv_dh_private[DH_PUBLIC_SIZE];   // X25519 private
    
    // E2E keys (separate from server DH!)
    uint8_t e2e_public[DH_PUBLIC_SIZE];       // X25519 public for E2E
    uint8_t e2e_private[DH_PUBLIC_SIZE];      // X25519 private for E2E
    
    uint8_t shared_secret[DH_PUBLIC_SIZE];
    // ...
} our_queue_t;
```

### 232.2 E2E Keypair Generated (main/smp_queue.c)

**After line 202:**
```c
crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);
// E2E keypair (separate from server DH!)
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);  // NEW!
```

### 232.3 SMPQueueInfo Corrected (main/smp_queue.c line 548)

**Before:**
```c
memcpy(&buf[p], our_queue.rcv_dh_public, 32);
```

**After:**
```c
memcpy(&buf[p], our_queue.e2e_public, 32);
```

### 232.4 E2E Decrypt Code Changed (main/main.c line 470-480)

**Before:**
```c
uint8_t peer_e2e_pub[32];
memcpy(peer_e2e_pub, &server_plain[28], 32);  // From message
crypto_box_beforenm(e2e_dh_secret, peer_e2e_pub, our_queue.e2e_private);
```

**After:**
```c
// Use peer's DH key from INVITATION (not from message!)
ESP_LOGI(TAG, "         Saved peer DH: %02x%02x%02x%02x...",
         pending_peer.dh_public[0], pending_peer.dh_public[1], 
         pending_peer.dh_public[2], pending_peer.dh_public[3]);
crypto_box_beforenm(e2e_dh_secret, pending_peer.dh_public, our_queue.e2e_private);
```

---

## 233. Test Results

### 233.1 Log Output (After Fix)

```
I (29031) SMP:       TEST: Trying DH decrypt with E2E keys...
I (29031) SMP:          Saved peer DH: 00f61e58...
I (29071) SMP:          E2E DH secret: a992027a...
I (29071) SMP:          Nonce: 76 e6 1a 15...
I (29071) SMP:          Encrypted len: 16006
E (29071) SMP:       E2E decrypt failed (ret=-1)
```

### 233.2 Message Structure Analysis

```
FULL HEX DUMP bytes 0-200:
     0000: 3e 82 00 00 00 00 69 7c af da 54 20 00 04 31 2c  | >.....i|..T ..1,
     0016: 30 2a 30 05 06 03 2b 65 6e 03 21 00 80 60 fd 2d  | 0*0...+en.!..`.-
     0032: 70 db c4 34 94 52 9c e3 d1 d8 f7 c0 27 96 cd 05  | p..4.R......'...
     0048: af e1 4e 04 7a df ee df 83 7a 50 4d 76 e6 1a 15  | ..N.z....zPMv...
```

**Structure Interpretation:**
```
[0-1]   Length prefix: 16002 (0x3e82)
[2-5]   Unknown: 00 00 00 00
[6-9]   Timestamp: 69 7c af da
[10-13] Unknown: 54 20 00 04
[14]    maybe_corrId = '1' (0x31) - Just (has corrId SPKI)
[15]    maybe_e2e = ',' (0x2c) - Nothing! <- PROBLEM!
[16-59] corrId X25519 SPKI (44 bytes)
[60-83] cmNonce (24 bytes)
[84+]   cmEncBody (encrypted)
```

---

## 234. The Real Problem

### 234.1 maybe_e2e = ',' means phE2ePubDhKey = Nothing

The app sends NO E2E public key in the message header!

### 234.2 Why the App Doesn't Send E2E Key

From `newSndQueue` (Agent.hs Line 3365-3379):
```haskell
newSndQueue userId connId (Compatible (SMPQueueInfo ... dhPublicKey = rcvE2ePubDhKey)) sndKeys_ = do
  (e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g
  -- ...
  e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,  -- Pre-computed!
```

The app:
1. Receives our `e2e_public` from SMPQueueInfo
2. Generates its own E2E keypair (e2ePubKey, e2ePrivKey)
3. Immediately computes `e2eDhSecret = DH(our_e2e_pub, app_e2e_priv)`
4. Does NOT send its e2ePubKey to us!

### 234.3 The Dilemma

| Side | Has | Needs |
|------|-----|-------|
| App | our_e2e_public, app_e2e_private | Can compute: DH(our_pub, app_priv) |
| Us | our_e2e_private, ??? | Need app_e2e_public! |

The app computes `e2eDhSecret = DH(our_e2e_public, app_e2e_private)`.
We would need to compute: `e2eDhSecret = DH(app_e2e_public, our_e2e_private)`.

**BUT:** The app NEVER sends its e2e_public to us when phE2ePubDhKey = Nothing!

---

## 235. Possible Causes

### 235.1 Hypothesis A: Protocol Version

Newer protocol versions might have different E2E behavior. The app shows:
- clientVersion = 4
- e2e Version = 2-3

Possibly E2E key exchange is different in certain versions.

### 235.2 Hypothesis B: Queue Mode

The SMPQueueInfo has a queueMode field. Different modes might have different E2E behavior:
- QMMessaging
- QMContact

### 235.3 Hypothesis C: Pre-shared Key from X3DH

The E2E key might be derived from X3DH key agreement, not separately generated.

From the Invitation:
```
dh=MCowBQYDK2VuAyEAAPYeWAEz7HeiS0hOeUO46pT0buXcDpmbUjH9NDPTHy0=
```

That's `00f61e58...` - the key we use as `pending_peer.dh_public`.
But this key is for Contact Queue, not Reply Queue!

### 235.4 Hypothesis D: Wrong Key Used

`pending_peer.dh_public` is the SMP DH key from Invitation for Contact Queue.

For Reply Queue we need a different key - possibly:
- A key derived from X3DH
- Or a key sent in AgentConfirmation

---

## 236. Open Questions

1. **Where does the app send its E2E public key for Reply Queue?**
   - In the Invitation? (Unlikely - that's for Contact Queue)
   - In the AgentConfirmation? (Possible)
   - Not at all? (Then different mechanism)

2. **Is E2E DH Secret derivable from X3DH?**
   - X3DH produces root_key, header_key, etc.
   - Is one of them also used for per-queue E2E?

3. **What does maybe_e2e = ',' really mean?**
   - Is that phE2ePubDhKey = Nothing?
   - Or a different Maybe field?

---

## 237. Next Steps

1. **Further Haskell Code Analysis:**
   - When exactly is phE2ePubDhKey = Just/Nothing set?
   - How is e2eDhSecret initialized at the receiver?

2. **Verify ClientMsgEnvelope Structure:**
   - Parsing of bytes [14-60] more carefully
   - Is [15] = ',' really phE2ePubDhKey?

3. **Test Alternative Keys:**
   - X3DH-derived keys for E2E
   - Extract keys from AgentConfirmation

---

## 238. Bug Status Update

| Bug # | Description | Status |
|-------|-------------|--------|
| #1-#17 | Earlier bugs | FIXED |
| **#18** | **Reply Queue E2E Decryption** | **OPEN - Root cause unclear** |

**Sub-Issues for Bug #18:**
- #18a: Separate E2E Keypair implemented - DONE
- #18b: E2E public sent in SMPQueueInfo - DONE
- #18c: App sends phE2ePubDhKey = Nothing - DISCOVERED
- #18d: Where does app_e2e_public come from? - UNKNOWN

---

## 239. Code Changes Summary

| File | Change | Status |
|------|--------|--------|
| smp_queue.h | Added e2e_public[32], e2e_private[32] | Done |
| smp_queue.c:203 | Added crypto_box_keypair(e2e_public, e2e_private) | Done |
| smp_queue.c:548 | Changed to memcpy(e2e_public) | Done |
| main.c:474-480 | Use pending_peer.dh_public for E2E DH | Done (wrong key?) |

---

## 240. SimpleGo Version Update

```
SimpleGo v0.1.17-alpha - E2E Keypair Analysis
===============================================================

Session 12 Summary:
- Discovered Haskell uses TWO separate X25519 keypairs
- Implemented separate e2e_public/e2e_private in our code
- Fixed SMPQueueInfo to send e2e_public
- STILL FAILING: App sends phE2ePubDhKey = Nothing

The Problem:
- App pre-computes e2eDhSecret at queue creation
- App NEVER sends its e2e_public key to us
- We cannot compute the same shared secret

Key Question:
- Where does app's E2E public key come from?
- Is it derived from X3DH?
- Is it in AgentConfirmation?

Status: Further analysis required

===============================================================
```

---

**DOCUMENT CREATED: 2026-01-30 Session 12 v26**  
**Status: FIX IMPLEMENTED - STILL FAILING**  
**Root Cause: App sends phE2ePubDhKey = Nothing**  
**Next: Find where app's E2E public key comes from**
