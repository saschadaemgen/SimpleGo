![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 12: Session 15
# Root Cause Found: Missing E2E Key Exchange on Contact Queue

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-01
**Status:** COMPLETED -- Root cause identified, fix requires Session 16
**Previous:** Part 11 - Session 14
**Next:** Part 13 - Session 16
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 15 SUMMARY

```
Session 15 identified the definitive root cause of Bug #18. The Reply
Queue HELLO message arrives with maybe_e2e = Nothing, meaning no
ephemeral key is included. The message is encrypted with a pre-computed
e2eDhSecret that requires the app's sndQueue.e2ePubKey, which is sent
in the app's AgentConfirmation on our Contact Queue. We never receive
this second Contact Queue message. Four Python tests with different
keys and brute-force nonce offset scanning confirmed the required key
is not present in any data we currently have. The fix requires
receiving and processing the app's AgentConfirmation on the Contact
Queue (Session 16).

 Root cause fully identified
 4 Python tests confirmed missing key
 maybe_e2e = Nothing semantics documented
 Two-phase key exchange architecture understood
```

---

## Root Cause

The Reply Queue E2E decryption fails because the message uses a pre-computed shared secret, and we lack the key needed to compute it.

### maybe_e2e = Nothing in Message Header

The received message has this structure at offset [14-15]:

```
[14] = 31 ('1')   -- maybe_corrId = Just (corrId follows)
[15] = 2c (',')   -- maybe_e2e = Nothing (0x2C = comma)
```

When `maybe_e2e = Nothing`, the Haskell code (Agent.hs:2708-2721) takes the pre-computed path:

```haskell
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> do
    let e2eDh = C.dh' e2ePubKey e2ePrivKey    -- fresh DH from header key
    decryptClientMessage e2eDh clientMsg
  (Just e2eDh, Nothing) -> do
    decryptClientMessage e2eDh clientMsg       -- pre-computed secret
```

Our case is the second branch: no ephemeral key in the message, decrypt using `e2eDhSecret` that was stored during connection setup.

### The Missing Key

The pre-computed `e2eDhSecret` requires:

```
App side:  e2eDhSecret = DH(our_queue.e2e_public, app.sndQueue.e2ePrivKey)
ESP32 side: dh_secret  = DH(app.sndQueue.e2ePubKey, our_queue.e2e_private)
```

We have `our_queue.e2e_private` but lack `app.sndQueue.e2ePubKey`. This key is sent in the app's AgentConfirmation, which arrives on our Contact Queue as the app's response to our connection request. We never receive this message.

---

## Protocol Flow: What Should Happen vs What Happens

```
ESP32                                              SimpleX App

  1. INVITATION on Contact Queue
  <--------------------------------------------------
  contains: peer.dh_public, X448 keys

  2. AgentConfirmation on Peer's Queue
  -------------------------------------------------->
  contains: our Reply Queue info, E2E params

  3. HELLO on Peer's Queue
  -------------------------------------------------->

  4. App's AgentConfirmation on Contact Queue          NOT RECEIVED
  <--------------------------------------------------
  contains: app.sndQueue.e2ePubKey

  5. App's HELLO on Reply Queue
  <--------------------------------------------------
  encrypted with pre-computed e2eDhSecret
```

Steps 1-3 work. Step 4 is never received, so step 5 cannot be decrypted.

### Log Evidence

```
I (28982) SMP_PEER: CONFIRMATION ACCEPTED BY SERVER!
I (29742) SMP_HAND: HELLO accepted by server!
I (30582) SMP:      Message on REPLY QUEUE from peer!
E (31702) SMP:      E2E decrypt FAILED (ret=-1)
```

No second "MESSAGE RECEIVED" appears on the Contact Queue after HELLO is sent.

---

## Tests Confirming Missing Key

### Test 1: Key from Invitation URI

```python
dh_secret = crypto_scalarmult(our_e2e_private, pending_peer_dh)
# Result: 685e7514...  Decrypt: FAILED
```

`pending_peer.dh_public` is the SMP-level DH key, not the per-queue E2E key.

### Test 2: Key from Message Header (corrId SPKI)

```python
dh_secret = crypto_scalarmult(our_e2e_private, msg_key_offset_28)
# Result: 3863509c...  Decrypt: FAILED
```

The key at offset 28 is the corrId, not an E2E key.

### Test 3: Brute-Force Nonce Offset Scan

Tested all nonce offsets from 48 to 80, with both keys above. All combinations failed.

### Test 4: Search for X25519 SPKI in INVITATION

Searched the entire decrypted INVITATION payload for X25519 SPKI headers. Found zero X25519 keys after the URI section. The required key is not present in any data we currently have.

---

## Two Key Types in SimpleX

| Key | Source | Usage |
|-----|--------|-------|
| `dh=` from URI | INVITATION link | SMP-level handshake (server auth) |
| `sndQueue.e2ePubKey` | App's AgentConfirmation | Per-queue E2E encryption |

These serve completely different purposes. The URI DH key authenticates the SMP connection to the peer server. The sndQueue E2E key encrypts the per-queue crypto_box layer.

---

## maybe_e2e Encoding

```
'0' (0x30) = Nothing (no value present)
'1' (0x31) = Just (value follows)
',' (0x2C) = Nothing (alternative marker in this context)
```

When `maybe_e2e = Nothing`, the message contains no ephemeral key. The sender used a pre-computed `e2eDhSecret` from earlier key exchange. The receiver must have stored `app.sndQueue.e2ePubKey` from a previous message to compute the matching secret.

---

## Message Structure when maybe_e2e = Nothing

```
[0-1]    Length prefix (Word16 BE)
[2-9]    Timestamp (Int64 BE)
[10]     Flags
[11]     Space
[12-13]  phVersion (Word16 BE)
[14]     maybe_corrId = '1' (Just)
[15]     SPKI length = 44 (0x2C)
[16-59]  corrId SPKI (44 bytes, X25519)
[60]     maybe_e2e = ',' (Nothing -- no e2ePubKey!)
[61-84]  cmNonce (24 bytes)
[85+]    cmEncBody [MAC 16][Ciphertext]
```

Note: When maybe_e2e = Nothing, cmNonce starts at offset 61 (immediately after the Nothing tag), not at offset 60.

---

## Verified Components Summary

| Component | Status | Evidence |
|-----------|--------|----------|
| X3DH key agreement | Correct | Python byte-for-byte match |
| Double Ratchet (sending) | Working | Server accepts "OK" |
| Server-level decrypt | Working | 16106 bytes decrypted |
| E2E keypair generation | Correct | scalarmult_base verified |
| DH secret calculation | Correct | Python match (when key available) |
| Per-queue E2E decrypt | Failing | Missing app.sndQueue.e2ePubKey |

---

## Solution Path (Session 16)

1. Continue listening on Contact Queue after sending AgentConfirmation + HELLO
2. Receive app's AgentConfirmation (second message on Contact Queue)
3. Parse SMPConfirmation from the decrypted content
4. Extract `sndQueue.e2ePubKey`
5. Compute `e2eDhSecret = DH(app.sndQueue.e2ePubKey, our_queue.e2e_private)`
6. Store this secret for all future Reply Queue messages
7. Decrypt Reply Queue HELLO with the correct secret

---

## Bug #18 Status

| Phase | Description | Status |
|-------|-------------|--------|
| E2E keypair generation | Separate keypair for Reply Queue | Done |
| E2E public in SMPQueueInfo | Sent in invitation link | Done |
| PubHeader parsing | Correct offsets verified | Done |
| HSalsa20 difference | Use crypto_scalarmult for raw DH | Done |
| MAC position | Detached handling implemented | Done |
| DH secret verified | Python byte-for-byte match | Done |
| Handoff theory disproven | No 2nd MSG on Contact Queue | Done |
| Message flow corrected | HELLO on Reply Queue confirmed | Done |
| Root cause: missing key | app.sndQueue.e2ePubKey needed | Identified |
| Receive app's Confirmation | Listen on Contact Queue | TODO (S16) |
| Extract e2ePubKey | Parse SMPConfirmation | TODO (S16) |

---

## Haskell Source Reference

| File | Lines | Relevance |
|------|-------|-----------|
| Agent.hs | 2686 | RcvQueue with e2eDhSecret |
| Agent.hs | 2708-2721 | e2eDhSecret vs e2ePubKey dispatch |
| Agent.hs | 3365-3379 | newSndQueue: e2eDhSecret calculation |
| Protocol.hs | 1310-1330 | SMPQueueAddress with dhPublicKey |
| Crypto.hs | 1302 | cbDecrypt implementation |

---

*Part 12 - Session 15: Root Cause Found - Missing E2E Key Exchange*
*SimpleGo Protocol Analysis*
*Original date: February 1, 2026*
*Rewritten: March 4, 2026 (v2)*
*Root cause identified, solution requires receiving app's AgentConfirmation*
