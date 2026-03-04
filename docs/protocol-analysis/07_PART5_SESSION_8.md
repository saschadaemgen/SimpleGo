![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 5: Session 8
# AgentConfirmation Accepted: First Working Native SMP Implementation

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-27
**Status:** COMPLETED -- AgentConfirmation working, Double Ratchet E2E verified
**Previous:** Part 4 - Session 7
**Next:** Part 6 - Session 9
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 8 SUMMARY

```
Session 8 achieved the first successful AgentConfirmation delivery to
the SimpleX App, marking the first working native ESP32 implementation
of the SimpleX Messaging Protocol. Two bugs were fixed: the payload
AAD incorrectly included a length prefix before emHeader (236 bytes
instead of 235), and the chainKdf IV assignment was swapped (iv1 is
message IV, iv2 is header IV). Contact "ESP32" appeared in the SimpleX
App with correct capabilities. Direct contact with Evgeny Poberezkin
(SimpleX founder) was established.

 2 Bugs fixed (payload AAD length prefix, chainKdf IV order)
 AgentConfirmation accepted by SimpleX App
 Contact "ESP32" visible, connStatus: joined
 First external SMP implementation confirmed by Evgeny
```

---

## Bug Fix: Payload AAD Length Prefix

The Associated Authenticated Data (AAD) for payload encryption incorrectly contained a length prefix before emHeader:

```c
// WRONG (236 bytes):
uint8_t payload_aad[236];  // 112 + 1 + 123
memcpy(payload_aad, ratchet_state.assoc_data, 112);
payload_aad[112] = 0x7B;  // Length prefix (123)
memcpy(payload_aad + 113, em_header, 123);

// CORRECT (235 bytes):
uint8_t payload_aad[235];  // 112 + 123 (no length prefix)
memcpy(payload_aad, ratchet_state.assoc_data, 112);
memcpy(payload_aad + 112, em_header, 123);  // direct, no prefix
```

**Haskell reference (Ratchet.hs:1155-1157):**

```haskell
decryptMessage (MessageKey mk iv) EncRatchetMessage {emHeader, emBody, emAuthTag} =
  tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
```

The `emHeader` in this context is the parsed header (after `largeP`), which no longer contains the length prefix. The length prefix is only used for wire transmission (`encodeLarge`), not for AAD calculation. This is a concrete instance of the wire-format-vs-crypto-format distinction identified in earlier sessions.

---

## Bug Fix: chainKdf IV Order

The chainKdf function returns `(ck', mk, iv1, iv2)`. The IV assignment was swapped:

```c
// WRONG:
memcpy(header_iv, kdf_output + 64, 16);  // iv1 assigned to header
memcpy(msg_iv, kdf_output + 80, 16);     // iv2 assigned to message

// CORRECT (per Haskell):
memcpy(msg_iv, kdf_output + 64, 16);     // iv1 = message IV
memcpy(header_iv, kdf_output + 80, 16);  // iv2 = header IV
```

**Haskell reference (Ratchet.hs:1168-1172 and line 906):**

```haskell
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)

-- Line 906: let (ck', mk, iv, ehIV) = chainKdf rcCKs
-- iv (message) = iv1, ehIV (header) = iv2
```

---

## Debug Progression

| Phase | Error | Change |
|-------|-------|--------|
| 1 | 2x A_MESSAGE | Ratchet decryption fails completely |
| 2 | A_PROHIBITED + A_MESSAGE | First message decrypted, wrong content |
| 3 | A_PROHIBITED + A_MESSAGE | IV fix applied, same result |
| 4 | 2x A_MESSAGE | MsgHeader length prefix removed (wrong) |
| 5 | A_PROHIBITED + A_MESSAGE | MsgHeader length prefix restored |
| 6 | Success | Payload AAD length prefix removed |

---

## AgentConfirmation Wire Format (Successfully Sent)

```
AgentConfirmation (15116 bytes total):
  Header: 00 07 43 (version=7, tag='C')
  E2ERatchetParams: 140 bytes
    Version: 2 bytes (0x00 0x02 = v2)
    Key1: 1 + 68 bytes (length + X448 SPKI)
    Key2: 1 + 68 bytes (length + X448 SPKI)
  encConnInfo: 14972 bytes (Double Ratchet encrypted)
    Plaintext: AgentConnInfoReply (224 bytes)
      Tag: 'D' (0x44)
      Queue Count: 0x01 (1 queue)
      SMPQueueInfo: 134 bytes
        clientVersion: 2 bytes (0x00 0x04 = v4)
        smpServer: host + port + keyHash
        senderId: 24 bytes
        dhPublicKey: 44 bytes (X25519 SPKI)
      ConnInfo JSON: 88 bytes
        {"v":"1-16","event":"x.info","params":{"profile":{"displayName":"ESP32","fullName":""}}}
  Outer Layer: crypto_box (15944 bytes encrypted)
```

---

## Cryptographic Verification

All cryptographic operations verified against Haskell reference using Python:

| Component | Status | Method |
|-----------|--------|--------|
| X3DH DH1, DH2, DH3 | Match | Python cryptography X448 |
| X3DH HKDF (hk, nhk, rk) | Match | Python HKDF-SHA512 |
| Root KDF (new_rk, ck, next_hk) | Match | Python HKDF-SHA512 |
| Chain KDF (mk, msg_iv, header_iv) | Match | Python HKDF-SHA512 |
| AES-GCM Header Encryption | Match | Python AESGCM |
| AES-GCM Payload Encryption | Implicit | App accepts message |

---

## SimpleX Developer Contact

During Session 8, direct contact was established with Evgeny Poberezkin, SimpleX founder and lead developer. Evgeny called the project "amazing" and "super cool", confirmed SimpleGo as the first known external SMP implementation outside the official Haskell codebase, and offered to add more context to A_PROHIBITED errors in future releases.

---

## Status After Session 8

Working: AgentConfirmation accepted, Double Ratchet E2E encryption verified, contact "ESP32" visible in app, connStatus transitions (requested, accepted, joined), app sends messages back on Reply Queue.

App capabilities recognized: pqSupport true, pqEncryption true, peerChatVRange v1-16.

Remaining: HELLO handshake returns ERR AUTH (wrong Queue-ID or signature). Incoming message decryption not yet implemented (receiver-side Double Ratchet needed).

---

*Part 5 - Session 8: AgentConfirmation Breakthrough*
*SimpleGo Protocol Analysis*
*Original date: January 27, 2026*
*Rewritten: March 4, 2026 (v2)*
*First working native ESP32 SMP implementation*
