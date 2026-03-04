![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 11: Session 14
# DH Secret Verified, Handoff Theory Disproven, Message Flow Corrected

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-31 to 2026-02-01
**Status:** COMPLETED -- DH secret verified, decrypt still fails
**Previous:** Part 10 - Session 13
**Next:** Part 12 - Session 15
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 14 SUMMARY

```
Session 14 disproved the handoff document theory (no second message on
Contact Queue) and established the correct message flow from Haskell
source code and agent-protocol.md. Two bugs were fixed: wrong key
(pending_peer.dh_public instead of e2ePubKey from message header) and
wrong DH function (crypto_box_beforenm with extra HSalsa20 instead of
crypto_scalarmult for raw DH). Python verification confirmed the DH
secret matches byte-for-byte. Decrypt still fails, pointing to an
offset or length prefix issue.

 2 Bugs fixed (wrong key source, wrong DH function)
 DH secret verified with Python (byte-for-byte match)
 Handoff document theory disproven
 Correct message flow established from source code
```

---

## Handoff Document Theory Disproven

The Session 13 handoff document theorized that the app sends two messages on the Contact Queue (INVITATION + AgentConfirmation with PHConfirmation containing the Reply Queue E2E key). Haskell source code analysis proved this wrong.

**Agent.hs line 1549-1551:**
```haskell
ICDuplexSecure _rId senderKey -> ... do
  secure rq senderKey
  void $ enqueueMessage c cData sq SMP.MsgFlags {notification = True} HELLO
```

The app sends HELLO on our Reply Queue, not on the Contact Queue.

**Agent/Client.hs line 1648:**
```haskell
let (privHdr, spKey) = if senderCanSecure queueMode
    then (SMP.PHEmpty, Just sndPrivateKey)
    else (SMP.PHConfirmation sndPublicKey, Nothing)
```

PHConfirmation is only sent when the queue is not yet secured, which is the Contact Queue direction, not the Reply Queue.

**agent-protocol.md line 87-94** confirms: HELLO comes on Reply Queue, not Contact Queue.

| Claim | Handoff Doc | Source Code | Protocol Doc |
|-------|-------------|-------------|--------------|
| 2 MSGs on Contact Queue | Claimed | Wrong | Wrong |
| HELLO on Reply Queue | Not mentioned | Confirmed | Confirmed |
| E2E Key in PHConfirmation | Claimed | Wrong | Not mentioned |

---

## Corrected Message Flow

```
PHASE 1: ESP32 Setup
  ESP32 creates Contact Queue + Reply Queue
  ESP32 generates e2e keypair for Reply Queue
  ESP32 shows Invite Link (contains Contact Queue + e2e_public)

PHASE 2: App Scans Link
  App creates its own Queue + ephemeral E2E keypair
  App calculates e2eDhSecret = DH(our_e2e_public, app_ephemeral_priv)
  App sends INVITATION on Contact Queue (X3DH keys, peer queue info)

PHASE 3: ESP32 Processes (WORKING)
  ESP32 receives INVITATION on Contact Queue
  ESP32 does X3DH, initializes Double Ratchet
  ESP32 sends AgentConfirmation (with Reply Queue Info)
  ESP32 sends HELLO

PHASE 4: App Processes
  App receives AgentConfirmation (extracts Reply Queue Info + e2e_public)
  App receives HELLO, triggers ICDuplexSecure
  App secures queue with KEY command
  App sends HELLO on our Reply Queue (encrypted with pre-computed e2eDhSecret)
  Header contains app_ephemeral_public

PHASE 5: ESP32 Receives (FAILING)
  ESP32 receives message on Reply Queue
  Server-level decrypt: SUCCESS (16106 bytes)
  Extracts app_ephemeral_public from PubHeader
  Calculates DH = app_ephemeral_public * our_e2e_private
  Per-queue E2E decrypt: FAILED (ret=-1)
```

Only one message arrives on the Contact Queue (INVITATION). No second message.

---

## Bugs Fixed

### Bug 1: Wrong Key Source

```c
// WRONG: Used SMP DH key from invitation URI
crypto_box_beforenm(e2e_dh_secret, pending_peer.dh_public, our_queue.e2e_private);

// CORRECT: Extract e2ePubKey from message header at offset 28
uint8_t peer_e2e_pub[32];
memcpy(peer_e2e_pub, &server_plain[28], 32);
```

`pending_peer.dh_public` is the SMP-level DH key from the invitation URI, used for server-level handshake. The per-queue E2E key is a separate app-generated ephemeral key found at offset 28-59 in the decrypted message.

### Bug 2: Wrong DH Function

```c
// WRONG: crypto_box_beforenm does HSalsa20 key derivation
crypto_box_beforenm(e2e_dh_secret, peer_pub, our_priv);

// CORRECT: crypto_scalarmult gives raw DH output (what Haskell does)
crypto_scalarmult(dh_secret, our_queue.e2e_private, peer_e2e_pub);
```

Haskell's `C.dh'` returns the raw X25519 shared secret, which is passed directly to XSalsa20 (which internally applies HSalsa20 once). Using `crypto_box_beforenm` would apply HSalsa20 twice.

---

## Python DH Secret Verification

```python
from nacl.bindings import crypto_scalarmult

our_private = bytes.fromhex('83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa')
peer_public = bytes.fromhex('9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300')

dh_secret = crypto_scalarmult(our_private, peer_public)
# d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
```

```
Python DH:  d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
ESP32 DH:   d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
Match: True
```

Nonce (24 bytes) and MAC (16 bytes) also verified correct. Full decrypt test could not be completed because the Python test only had truncated ciphertext (~600 bytes instead of 16006).

---

## Haskell Crypto Implementation (Verified)

### agentCbEncrypt (Agent/Client.hs:1925-1933)

```haskell
agentCbEncrypt SndQueue {e2eDhSecret, smpClientVersion} e2ePubKey msg = do
  cmNonce <- atomically . C.randomCbNonce =<< asks random
  let paddedLen = maybe SMP.e2eEncMessageLength (const SMP.e2eEncConfirmationLength) e2ePubKey
  cmEncBody <- liftEither . first cryptoError $ C.cbEncrypt e2eDhSecret cmNonce msg paddedLen
  let cmHeader = SMP.PubHeader smpClientVersion e2ePubKey
  pure $ smpEncode SMP.ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody}
```

Key insight: `e2eDhSecret` is pre-stored in SndQueue. `e2ePubKey` is sent in header only for the first message (`Just key`); subsequent messages use `Nothing`.

### cryptoBox (Crypto.hs:1295-1298)

```haskell
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s
    tag = Poly1305.auth rs c
```

Output format: `[TAG 16 bytes][Ciphertext]`.

### Crypto Compatibility Summary

| Aspect | Haskell | ESP32 Code | Match |
|--------|---------|------------|-------|
| Algorithm | XSalsa20-Poly1305 | crypto_secretbox | Yes |
| Key | Raw DH (32 bytes) | crypto_scalarmult | Yes |
| Nonce | 24 bytes | 24 bytes | Yes |
| MAC | [MAC][Cipher] format | detached handling | Yes |
| DH | X25519.dh (raw) | crypto_scalarmult | Yes |

---

## Remaining Problem

DH secret, nonce, and MAC are all verified correct. Decrypt still fails (ret=-1). Possible remaining causes:

1. Offset problem: `server_plain[0-1]` is a length prefix (3e 82 = 16002). All offsets might need +2 adjustment if the length prefix was already consumed during server-level parsing.

2. SPKI length interpretation: offset [15] = 0x2C could be interpreted as ASCII ',' (Nothing) instead of integer 44 (SPKI length), depending on whether `maybe_e2e` has already been consumed.

3. Parameter order in `crypto_secretbox_open_detached`: ciphertext length must be the ciphertext-only length, excluding MAC.

### Test Data for Next Session

```python
our_e2e_private = "83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa"
peer_e2e_pub    = "9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300"
dh_secret       = "d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810"
cm_nonce        = "b21fa2bc0dbb5cb02d674dedfd65b0e6ff0fcf793791fd3b"
mac             = "cc3eec548b0440cf0222466a79a00c0c"
cm_enc_len      = 16022  # full ciphertext needed for verification
```

---

## Haskell Source Reference

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| agentCbEncrypt | Agent/Client.hs | 1925-1933 | Per-queue E2E encryption |
| agentCbDecrypt | Agent/Client.hs | 1949-1951 | Per-queue E2E decryption |
| cryptoBox | Crypto.hs | 1295-1298 | XSalsa20-Poly1305 encrypt |
| sbDecryptNoPad_ | Crypto.hs | 1325-1333 | XSalsa20-Poly1305 decrypt |
| xSalsa20 | Crypto.hs | 1449-1456 | XSalsa20 stream cipher |
| e2eDhSecret | Agent.hs | 3379 | DH secret calculation |
| ICDuplexSecure | Agent.hs | 1549-1551 | Triggers HELLO sending |
| sendConfirmation | Agent/Client.hs | 1648 | PHConfirmation logic |

---

*Part 11 - Session 14: DH Secret Verified, Message Flow Corrected*
*SimpleGo Protocol Analysis*
*Original dates: January 31 to February 1, 2026*
*Rewritten: March 4, 2026 (v2)*
*2 bugs fixed, DH verified, handoff theory disproven*
