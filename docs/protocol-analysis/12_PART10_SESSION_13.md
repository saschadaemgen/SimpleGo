![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 10: Session 13
# Reply Queue E2E Deep Analysis - MAC Position & Crypto Differences

**Document Version:** v27  
**Date:** 2026-01-30 Session 13  
**Status:** IN PROGRESS - Multiple crypto approaches tested, all failing  
**Previous:** Part 9 - Session 12 (E2E Keypair Fix Attempt)

---

## 241. Session 13 Overview (2026-01-30)

### 241.1 Starting Point

After Session 12 we had:
- Outgoing messages: WORKING (Server accepts with "OK")
- Contact Queue decrypt: WORKING (TEST4 SUCCESS)
- Reply Queue decrypt: FAILING
- App Status: "connecting" / shows "A_CRYPTO" error
- Discovered: Haskell uses TWO separate X25519 keypairs
- Implemented: Separate e2e_public/e2e_private in our code

### 241.2 Session 13 Goals

1. Deep-dive into Haskell crypto implementation
2. Understand exact wire format differences
3. Test multiple crypto approaches
4. Find root cause of E2E decrypt failure

---

## 242. Critical Discovery: PubHeader Structure

### 242.1 The Parsing Bug Found

**Our code was WRONG:**
```c
uint8_t maybe_corrId = plain[offset];     // [14] - WRONG NAME!
uint8_t maybe_e2e = plain[offset + 1];    // [15] - THIS IS LENGTH, NOT A TAG!
```

**PubHeader has ONLY ONE Maybe field:**
```haskell
data PubHeader = PubHeader
  { phVersion :: VersionSMPC,                  -- Word16 (2 bytes)
    phE2ePubDhKey :: Maybe C.PublicKeyX25519   -- ONLY THIS ONE!
  }
```

### 242.2 Correct Interpretation

```
[14] = '1' (0x31) = phE2ePubDhKey = Just -> KEY IS PRESENT!
[15] = 0x2c = 44 = SPKI Length
[16-59] = X25519 SPKI (44 bytes) = E2E PUBLIC KEY!
[60-83] = cmNonce (24 bytes)
[84+] = cmEncBody
```

### 242.3 Log Proof

```
0016: 30 2a 30 05 06 03 2b 65 6e 03 21 00 c3 99 54 05
      ^SPKI Header!
```

`30 2a 30 05 06 03 2b 65 6e 03 21 00` = Standard X25519 SPKI Header!

**The e2ePubKey IS in the message! At offset 16-59!**

---

## 243. Complete Message Structure (Verified from Haskell)

### 243.1 ClientRcvMsgBody (after Server-decrypt)

```haskell
data ClientRcvMsgBody = ClientRcvMsgBody
  { msgTs :: SystemTime,      -- Int64 = 8 bytes (Big Endian)
    msgFlags :: MsgFlags,     -- 1 byte (notification bool) + ' ' space
    msgBody :: ByteString     -- ClientMsgEnvelope!
  }
```

### 243.2 Full Byte Layout

```
=== ClientRcvMsgBody ===
[0-1]    Large length prefix (Word16 BE)
[2-9]    msgTs (SystemTime = Int64 BE, 8 bytes)
[10]     msgFlags (1 byte: '0' or '1')
[11]     Space ' ' (0x20)

=== ClientMsgEnvelope (msgBody) starts at Offset 12 ===
[12-13]  phVersion (Word16 BE, e.g. 00 04 = Version 4)
[14]     phE2ePubDhKey Maybe tag: '0' (Nothing) or '1' (Just)
[15]     IF '1': SPKI length = 44 (0x2c)
[16-59]  X25519 SPKI (44 bytes)
  [16-27]  SPKI header (12 bytes)
  [28-59]  Raw X25519 key (32 bytes) <- THE E2E KEY!
[60-83]  cmNonce (24 bytes)
[84+]    cmEncBody (encrypted)
```

### 243.3 Log Verification

```
First 64 bytes after server-decrypt:
     3e 82 00 00 00 00 69 7c e2 58 54 20 00 04 31 2c
     30 2a 30 05 06 03 2b 65 6e 03 21 00 42 60 ec a8

- 3e 82 = 16002 ✅ Large length
- 00 00 00 00 69 7c e2 58 = msgTs ✅
- 54 = msgFlags ✅
- 20 = ' ' ✅
- 00 04 = phVersion v4 ✅
- 31 = '1' = Just ✅
- 2c = 44 = SPKI len ✅
- 30 2a 30 05 06 03 2b 65 6e 03 21 00 = SPKI header ✅
- 42 60 ec a8... = Raw key start ✅
```

**THE OFFSETS ARE CORRECT!**

---

## 244. Critical Discovery: HSalsa20 Difference

### 244.1 Haskell cryptoBox Implementation

```haskell
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s    -- DH secret DIRECTLY to XSalsa20!
    tag = Poly1305.auth rs c
```

### 244.2 Our Code (libsodium)

```c
crypto_box_beforenm(e2e_dh_secret, peer_pub, our_priv);  // HSalsa20 applied!
crypto_box_open_easy_afternm(..., e2e_dh_secret);
```

### 244.3 The Problem

| Step | Haskell | libsodium crypto_box |
|------|---------|---------------------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce, msg) | **HSalsa20(secret)** -> key |
| 3 | - | XSalsa20(key, nonce, msg) |

**libsodium has an EXTRA HSalsa20 step!**

### 244.4 Double HSalsa20 Problem

**Haskell xSalsa20 (CORRECT):**
```
1. subkey = HSalsa20(dh_secret, nonce[0:16])
2. Salsa20(subkey, nonce[16:24]) -> encrypt/decrypt
```

**Our code with _beforenm/_afternm (WRONG):**
```
1. k = HSalsa20(dh_secret, ZERO_NONCE)    <- beforenm
2. subkey = HSalsa20(k, nonce[0:16])      <- afternm  
3. Salsa20(subkey, nonce[16:24])
```

**That's DOUBLE HSalsa20!** The result is completely different!

---

## 245. Critical Discovery: MAC Position Difference

### 245.1 Haskell cbDecrypt

```haskell
sbDecryptNoPad_ secret (CbNonce nonce) packet
  | B.length packet < 16 = Left CBDecryptError
  | BA.constEq tag' tag = Right msg
  | otherwise = Left CBDecryptError
  where
    (tag', c) = B.splitAt 16 packet  -- TAG = first 16 bytes!
    (rs, msg) = xSalsa20 secret nonce c
    tag = Poly1305.auth rs c
```

### 245.2 Format Comparison

| Format | Layout |
|--------|--------|
| **Haskell** | `[MAC 16 bytes][Ciphertext]` |
| **libsodium** | `[Ciphertext][MAC 16 bytes]` |

**This is a VERIFIED difference in wire format!**

---

## 246. All Crypto Tests Performed

### 246.1 Test Matrix

| Test | Crypto Method | MAC Handling | Private Key | Result |
|------|---------------|--------------|-------------|--------|
| 1 | crypto_box_open_easy | Auto | e2e_private | FAILED |
| 2 | crypto_box_open_easy | Auto | rcv_dh_private | FAILED |
| 3 | crypto_secretbox_open_easy | None (direct) | e2e_private | FAILED |
| 4 | crypto_secretbox_open_easy | Reordered | e2e_private | FAILED |
| 5 | crypto_secretbox_open_detached | Separate MAC | e2e_private | FAILED |

### 246.2 Test v1: crypto_box_open_easy (standard)

```c
crypto_box_open_easy(plain, cipher, len, nonce, peer_pub, our_priv);
```

**Result:** FAILED (ret=-1)

### 246.3 Test v2: With MAC Reordering

```c
// Haskell format: [MAC][Cipher]
// libsodium expects: [Cipher][MAC]
// Reorder: move MAC from front to back
uint8_t reordered[cipher_len];
memcpy(reordered, &cipher[16], cipher_len - 16);  // Cipher first
memcpy(&reordered[cipher_len - 16], cipher, 16);  // MAC last
crypto_secretbox_open_easy(plain, reordered, cipher_len, nonce, dh_secret);
```

**Result:** FAILED (ret=-1)

### 246.4 Test v3: crypto_secretbox_open_detached

```c
const uint8_t *mac = &server_plain[84];        // First 16 bytes
const uint8_t *ciphertext = &server_plain[100]; // Rest
crypto_secretbox_open_detached(plain, ciphertext, mac, ciphertext_len, nonce, dh_secret);
```

**Result:** FAILED (ret=-1)

### 246.5 Test Log Data

```
e2ePubKey:     88159398... (from message [28-59])
our_e2e_priv:  f3944334... (verified - keypair matches)
cmNonce:       59c05b9e...
DH secret:     dea3d892...
MAC:           143b0d95...
Ciphertext:    16006 bytes
```

---

## 247. Haskell Crypto Deep Dive

### 247.1 XSalsa20 Implementation

```haskell
xSalsa20 secret nonce msg = (rs, msg')
  where
    zero = B.replicate 16 $ toEnum 0
    (iv0, iv1) = B.splitAt 8 nonce      -- nonce[0:8], nonce[8:24]
    state0 = XSalsa.initialize 20 secret (zero `B.append` iv0)
    state1 = XSalsa.derive state0 iv1
    (rs, state2) = XSalsa.generate state1 32
    (msg', _) = XSalsa.combine state2 msg
```

### 247.2 Nonce Splitting

- iv0 = nonce[0:8] (8 bytes)
- iv1 = nonce[8:24] (16 bytes)
- HSalsa20(secret, zeros[16] || iv0[8]) -> state0
- derive(state0, iv1) -> state1

This is standard XSalsa20! libsodium crypto_secretbox SHOULD be compatible.

### 247.3 DH Calculation

```haskell
dh' (PublicKeyX25519 k) (PrivateKeyX25519 pk _) = DhSecretX25519 $ X25519.dh k pk
```

Standard X25519 Diffie-Hellman, should match `crypto_scalarmult`.

---

## 248. Two Separate Problems Identified

### 248.1 Problem 1: Reply Queue decrypt (ESP32 receives from App)

- crypto_secretbox_open_easy returns -1
- Possible causes: Wrong offset, wrong nonce, or wrong key

### 248.2 Problem 2: App decrypt (App receives from ESP32)

- App shows "A_CRYPTO" error
- Possible cause: Double Ratchet encryption is wrong

---

## 249. Android vs Desktop App Difference

### 249.1 Desktop App

- INVITATION URI successfully extracted (2090 chars)
- Peer-Connect occurs
- AgentConfirmation + HELLO sent
- "Connecting..." displayed
- Reply Queue E2E decrypt fails

### 249.2 Android App

```
⚠️ Could not extract invitation URI!
```

- **NO** Peer-Connect!
- Only ACK sent
- No "Connecting..." displayed

### 249.3 Log Comparison

**Desktop:** `2a fc 5f 00 07 49...` (starts with `2a`)
**Android:** `09 e7 5f 00 07 49...` (starts with `09`)

The first byte is the **padding prefix**. The parser may be looking for a fixed value.

---

## 250. Key Discovery: e2ePubKey Flow

### 250.1 Where App's e2eDhSecret Comes From

```haskell
-- ~/simplexmq/src/Simplex/Messaging/Agent.hs:3365-3379
newSndQueue userId connId (Compatible (SMPQueueInfo ... {dhPublicKey = rcvE2ePubDhKey})) sndKeys_ = do
  (e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g
  let sq = SndQueue
        { ...
          e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,  -- DH(our_pub, app_priv)
          e2ePubKey = Just e2ePubKey,                     -- App's public key
        }
```

### 250.2 The Flow

1. We send `SMPQueueInfo` with `dhPublicKey = our_queue.e2e_public`
2. App receives and creates SndQueue with:
   - `e2eDhSecret = DH(our_queue.e2e_public, app_e2e_private)`
   - `e2ePubKey = Just app_e2e_public`
3. App sends **first** message with `e2ePubKey = Just key` (sendConfirmation)
4. App sends **subsequent** messages with `e2ePubKey = Nothing` (sendAgentMessage)

### 250.3 The Problem

- Reply Queue message is a **subsequent** message (not the first!)
- Therefore `e2ePubKey = Nothing`
- App's `e2ePubKey` was sent in an **earlier** message
- We need to have saved this key!

### 250.4 Where Was App's e2ePubKey Sent?

| Possibility | Location | Status |
|-------------|----------|--------|
| In AgentInvitation | Contact Queue first msg | Uses different key (from URI) |
| In SMPConfirmation | After our AgentConfirmation | **LIKELY!** |
| In AgentConfirmation | From App | Possible |

---

## 251. SMPConfirmation Contains e2ePubKey!

### 251.1 Found in Haskell Code

**Source:** `~/simplexmq/src/Simplex/Messaging/Agent/Protocol.hs`

```haskell
data SMPConfirmation = SMPConfirmation
  { senderKey :: Maybe SndPublicAuthKey,
    e2ePubKey :: C.PublicKeyX25519,        -- THE E2E KEY FOR REPLY QUEUE!
    connInfo :: ConnInfo,
    smpReplyQueues :: [SMPQueueInfo],       -- App's Reply Queues
    smpClientVersion :: VersionSMPC
  }
```

### 251.2 AgentMsgEnvelope Structure

```haskell
data AgentMsgEnvelope
  = AgentConfirmation
      { agentVersion :: VersionSMPA,
        e2eEncryption_ :: Maybe (SndE2ERatchetParams 'C.X448),
        encConnInfo :: ByteString        -- ENCRYPTED! SMPConfirmation is HERE!
      }
  | AgentMsgEnvelope
      { agentVersion :: VersionSMPA,
        encAgentMessage :: ByteString
      }
```

### 251.3 encConnInfo Decryption

```haskell
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> do          -- e2ePubKey from PubHeader!
    let e2eDh = C.dh' e2ePubKey e2ePrivKey
    decryptClientMessage e2eDh clientMsg >>= \case
      ...
      (agentMsgBody_, rc', skipped) <- CR.rcDecrypt g rc M.empty encConnInfo
      parseMessage agentMsgBody >>= \case
        AgentConnInfoReply smpQueues connInfo -> ...
```

---

## 252. Remaining Hypotheses

### 252.1 Hypothesis H1: Wrong DH Key

- The key at [28-59] is **corrId SPKI**, not e2ePubKey!
- For Reply Queue with `maybe_e2e = ','` (Nothing) there's no key in header
- App uses **pre-computed secret** from earlier message

### 252.2 Hypothesis H2: Wrong Private Key

- We use `our_queue.e2e_private`
- But App might have encrypted with a different key
- Where did App's e2ePubKey come from? From which earlier message?

### 252.3 Hypothesis H3: Structure Completely Different

- Maybe the message is already Double-Ratchet encrypted
- No per-queue E2E layer present
- maybe_e2e = ',' means: direct to Ratchet decrypt

### 252.4 Hypothesis H4: XSalsa20 vs crypto_secretbox Different

- libsodium crypto_secretbox = XSalsa20-Poly1305
- But Haskell does XSalsa20 manually with its own state management
- Could have subtle differences

---

## 253. Important Variables in Code

### 253.1 ESP32 Side

```c
our_queue.e2e_public[32]      // Our E2E public key
our_queue.e2e_private[32]     // Our E2E private key  
our_queue.rcv_dh_public[32]   // Server DH public
our_queue.rcv_dh_private[32]  // Server DH private
our_queue.shared_secret[32]   // Server-level shared secret

pending_peer.dh_public[32]    // App's DH key from Invitation URI
reply_queue_e2e_peer_public[32] // App's E2E key for Reply Queue (EMPTY!)
```

### 253.2 Key Combinations Tested

| Peer Public Key | Our Private Key | Result |
|-----------------|-----------------|--------|
| server_plain[28-59] (3b8ebc09...) | e2e_private | FAILED |
| server_plain[28-59] | rcv_dh_private | FAILED |
| pending_peer.dh_public (84273719...) | e2e_private | FAILED |

---

## 254. Code Changes Summary

### 254.1 Parsing Fix (Correct Offsets)

```c
// OLD (wrong) interpretation:
int offset = 14;
uint8_t maybe_corrId = plain[offset];     // '1'
uint8_t maybe_e2e = plain[offset + 1];    // ',' interpreted as Nothing

// NEW (correct) interpretation:
int offset = 14;
uint8_t maybe_e2e = plain[offset];        // '1' = Just
if (maybe_e2e == '1') {
    // e2ePubKey is at offset + 2 to offset + 2 + 44
    memcpy(e2e_peer_public, &plain[offset + 2 + 12], 32);  // +12 for SPKI header
    // cmNonce is at offset + 2 + 44 = offset + 46
    memcpy(cm_nonce, &plain[offset + 46], 24);
    // cmEncBody is at offset + 46 + 24 = offset + 70
}
```

### 254.2 Raw DH Calculation

```c
// Use crypto_scalarmult for raw DH (without HSalsa20)
uint8_t dh_secret[32];
crypto_scalarmult(dh_secret, our_queue.e2e_private, peer_e2e_public);
```

### 254.3 MAC Reordering

```c
// Haskell format: [MAC 16][Ciphertext]
// libsodium format: [Ciphertext][MAC 16]
uint8_t *reordered = malloc(enc_len);
memcpy(reordered, &cmEncBody[16], enc_len - 16);  // Ciphertext first
memcpy(&reordered[enc_len - 16], cmEncBody, 16);   // MAC last
```

---

## 255. Next Steps

### 255.1 Immediate

1. **Analyze App's first message on Contact Queue**
   - Does it contain an e2ePubKey?
   - If yes: save it for Reply Queue decrypt

2. **Parse SMPConfirmation**
   - After our AgentConfirmation, App sends SMPConfirmation
   - This contains `e2ePubKey` for our Reply Queue

### 255.2 Short-term

3. **Implement Double Ratchet for receiving**
   - maybe_e2e = ',' might mean: direct Double Ratchet
   - No per-queue E2E layer

4. **Python Verification**
   - Test exact log values in Python
   - Verify if crypto is correct

---

## 256. Bug Status Update

| Bug # | Description | Status |
|-------|-------------|--------|
| #1-#17 | Earlier bugs | FIXED |
| **#18** | **Reply Queue E2E Decryption** | **OPEN** |

### Bug #18 Sub-Issues

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | Parsing fix (correct offsets) | DONE |
| #18d | MAC reordering tested | DONE - Still fails |
| #18e | Raw DH (crypto_scalarmult) tested | DONE - Still fails |
| #18f | crypto_secretbox_open_detached tested | DONE - Still fails |
| #18g | Find where App's e2ePubKey comes from | UNKNOWN |
| #18h | SMPConfirmation parsing needed | TODO |

---

## 257. Session 13 Changelog

| Time | Change | Result |
|------|--------|--------|
| 09:00 | Correct offset parsing implemented | Key extraction works |
| 09:30 | Test crypto_box_open_easy | FAILED |
| 10:00 | Test with rcv_dh_private | FAILED |
| 10:30 | Test crypto_secretbox_open_easy | FAILED |
| 11:00 | Test with MAC reordering | FAILED |
| 11:30 | Test crypto_secretbox_open_detached | FAILED |
| 12:00 | Haskell xSalsa20 analysis | Documented differences |
| 12:30 | SMPConfirmation discovery | Contains e2ePubKey! |
| 13:00 | Android vs Desktop difference found | Different padding prefix |

---

## 258. Critical Findings Summary

### 258.1 Verified Correct

- Key extraction: [28-59] = Raw X25519 key
- Nonce extraction: [60-83] = 24 bytes
- Body offset: [84+] = cmEncBody
- Keypair verification: e2e_public matches derived from e2e_private

### 258.2 Discovered Differences

| Aspect | Haskell | libsodium |
|--------|---------|-----------|
| DH usage | Direct to XSalsa20 | HSalsa20 then XSalsa20 |
| MAC position | [MAC][Cipher] | [Cipher][MAC] |
| Wire format | Custom cbEncrypt | crypto_secretbox |

### 258.3 Open Questions

1. **Is the key at [28-59] really e2ePubKey or corrId?**
2. **Where is App's e2ePubKey for Reply Queue stored?**
3. **Do we need to parse SMPConfirmation first?**
4. **Is this even per-queue E2E or direct Double Ratchet?**

---

## 259. SimpleGo Version Update

```
SimpleGo v0.1.17-alpha - E2E Crypto Analysis
===============================================================

Session 13 Summary:
- Fixed message parsing (correct offsets verified)
- Discovered HSalsa20 difference (Haskell vs libsodium)
- Discovered MAC position difference ([MAC][Cipher] vs [Cipher][MAC])
- Tested 5 different crypto approaches - ALL FAILED
- Found SMPConfirmation contains e2ePubKey
- Android vs Desktop apps behave differently

All Crypto Tests Failed:
1. crypto_box_open_easy + e2e_private
2. crypto_box_open_easy + rcv_dh_private
3. crypto_secretbox_open_easy (direct)
4. crypto_secretbox_open_easy (MAC reordered)
5. crypto_secretbox_open_detached (MAC separate)

Key Question:
- Where does App's e2ePubKey for Reply Queue come from?
- SMPConfirmation? Earlier message? X3DH derived?

Next: Parse SMPConfirmation to extract App's e2ePubKey

===============================================================
```

---

**DOCUMENT CREATED: 2026-01-30 Session 13 v27**  
**Status: IN PROGRESS - All crypto tests failed**  
**Key Discovery: MAC position [MAC][Cipher] vs [Cipher][MAC]**  
**Next: Parse SMPConfirmation for App's e2ePubKey**
