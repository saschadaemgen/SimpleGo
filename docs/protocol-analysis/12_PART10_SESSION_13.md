![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 10: Session 13
# Reply Queue E2E Deep Analysis: PubHeader, HSalsa20, MAC Position

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-30
**Status:** COMPLETED -- Multiple crypto approaches tested, root cause narrowing
**Previous:** Part 9 - Session 12
**Next:** Part 11 - Session 14
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 13 SUMMARY

```
Session 13 deep-dived into the Reply Queue E2E decryption failure
(Bug #18). A PubHeader parsing bug was corrected, revealing the
correct message byte layout. Critical crypto differences between
Haskell cryptoBox and libsodium were identified: Haskell uses raw DH
directly in XSalsa20 (no HSalsa20 key derivation), and MAC position
is [MAC][Ciphertext] vs libsodium's [Ciphertext][MAC]. Five different
crypto approaches were tested, all failing. SMPConfirmation was
discovered to contain the required e2ePubKey for Reply Queue E2E.

Starting point: outgoing messages working, Contact Queue decrypt
working, Reply Queue E2E decrypt failing with A_CRYPTO.

 1 Parsing bug fixed (PubHeader structure)
 2 Crypto differences identified (HSalsa20, MAC position)
 5 Crypto approaches tested (all failed)
 1 Key discovery: SMPConfirmation contains e2ePubKey
```

---

## PubHeader Parsing Fix

The PubHeader was being parsed with wrong field assumptions. PubHeader has only one Maybe field:

```haskell
data PubHeader = PubHeader
  { phVersion :: VersionSMPC,               -- Word16 (2 bytes)
    phE2ePubDhKey :: Maybe C.PublicKeyX25519 -- the ONLY Maybe field
  }
```

### Corrected Byte Layout (after Server-Level Decrypt)

```
=== ClientRcvMsgBody ===
[0-1]    Large length prefix (Word16 BE, e.g. 3e 82 = 16002)
[2-9]    msgTs (SystemTime = Int64 BE, 8 bytes)
[10]     msgFlags (1 byte: '0' or '1')
[11]     Space ' ' (0x20)

=== ClientMsgEnvelope starts at Offset 12 ===
[12-13]  phVersion (Word16 BE, e.g. 00 04 = Version 4)
[14]     phE2ePubDhKey Maybe tag: '0' (Nothing) or '1' (Just)
[15]     IF '1': SPKI length = 44 (0x2c)
[16-27]  SPKI header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
[28-59]  Raw X25519 key (32 bytes)
[60-83]  cmNonce (24 bytes)
[84+]    cmEncBody: [MAC 16 bytes][Ciphertext]
```

Log verification confirmed all offsets: `3e 82` (length 16002), `00 04` (version 4), `31` (Just), `2c` (SPKI len 44), followed by standard X25519 SPKI header `30 2a 30 05 06 03 2b 65 6e 03 21 00`.

---

## Crypto Differences: Haskell vs libsodium

### HSalsa20 Double-Application Problem

Haskell cryptoBox passes the raw DH secret directly to XSalsa20, which internally runs HSalsa20 once as part of the XSalsa20 construction. libsodium's `crypto_box_beforenm` also runs HSalsa20 on the DH secret, and then `crypto_box_open_easy_afternm` runs XSalsa20 (which includes another HSalsa20). This results in double HSalsa20:

| Step | Haskell | libsodium crypto_box |
|------|---------|---------------------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce, msg) | HSalsa20(secret, zeros) -> key |
| 3 | (HSalsa20 happens inside XSalsa20) | XSalsa20(key, nonce, msg) |

Solution: Use `crypto_scalarmult` for raw DH, then `crypto_secretbox` functions with the raw DH output.

### MAC Position Difference

```haskell
sbDecryptNoPad_ secret nonce packet
  | BA.constEq tag' tag = Right msg
  where
    (tag', c) = B.splitAt 16 packet  -- TAG = first 16 bytes!
```

| Format | Layout |
|--------|--------|
| Haskell | [MAC 16 bytes][Ciphertext] |
| libsodium | [Ciphertext][MAC 16 bytes] |

This is a verified wire format difference. Must use `crypto_secretbox_open_detached` to handle MAC separately, or reorder bytes before decryption.

### Haskell xSalsa20 Implementation

```haskell
xSalsa20 secret nonce msg = (rs, msg')
  where
    zero = B.replicate 16 $ toEnum 0
    (iv0, iv1) = B.splitAt 8 nonce
    state0 = XSalsa.initialize 20 secret (zero `B.append` iv0)
    state1 = XSalsa.derive state0 iv1
    (rs, state2) = XSalsa.generate state1 32   -- Poly1305 subkey
    (msg', _) = XSalsa.combine state2 msg      -- XOR encrypt
```

This is standard XSalsa20. libsodium's crypto_secretbox should be compatible when given the raw DH secret directly.

---

## Crypto Test Matrix

| Test | Method | Key Source | MAC Handling | Result |
|------|--------|-----------|--------------|--------|
| 1 | crypto_box_open_easy | e2e_private | Auto | FAILED |
| 2 | crypto_box_open_easy | rcv_dh_private | Auto | FAILED |
| 3 | crypto_secretbox_open_easy | e2e_private (raw DH) | Auto | FAILED |
| 4 | crypto_secretbox_open_easy | e2e_private (raw DH) | MAC reordered | FAILED |
| 5 | crypto_secretbox_open_detached | e2e_private (raw DH) | Separate MAC | FAILED |

All five approaches failed. The DH secret, nonce, MAC, and ciphertext offsets appeared correct, pointing to a more fundamental issue with which key pair should be used.

---

## e2ePubKey Flow Discovery

Analysis of `newSndQueue` in Agent.hs (line 3365-3379) revealed the key exchange flow:

```haskell
newSndQueue userId connId (Compatible (SMPQueueInfo ... {dhPublicKey = rcvE2ePubDhKey})) = do
  (e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g
  let sq = SndQueue
        { e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey
        , e2ePubKey = Just e2ePubKey
        }
```

The app generates a fresh E2E keypair for its SndQueue. The `e2ePubKey` (app's public) is sent in the first message (sendConfirmation, with `Just key`), and subsequent messages use `Nothing` (pre-computed `e2eDhSecret`). The Reply Queue message is a subsequent message, so `e2ePubKey = Nothing`, meaning the app's public key was sent in an earlier message that we need to have received and processed.

### SMPConfirmation Contains e2ePubKey

```haskell
data SMPConfirmation = SMPConfirmation
  { senderKey :: Maybe SndPublicAuthKey,
    e2ePubKey :: C.PublicKeyX25519,      -- THE E2E KEY FOR REPLY QUEUE
    connInfo :: ConnInfo,
    smpReplyQueues :: [SMPQueueInfo],
    smpClientVersion :: VersionSMPC
  }
```

The app's e2ePubKey for our Reply Queue is contained in the SMPConfirmation, which is sent inside the app's AgentConfirmation on our Contact Queue.

---

## Android vs Desktop Difference

Desktop app: INVITATION URI extraction works, full handshake proceeds.
Android app: INVITATION URI extraction fails with different padding prefix byte (0x09 vs 0x2a). The first byte is a padding prefix that may vary by platform, and the parser was looking for a fixed value.

---

*Part 10 - Session 13: Reply Queue E2E Deep Analysis*
*SimpleGo Protocol Analysis*
*Original date: January 30, 2026*
*Rewritten: March 4, 2026 (v2)*
*PubHeader fix, HSalsa20/MAC differences, 5 crypto tests, SMPConfirmation discovery*
