![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 16: Session 19
# Three Parsing Layers Discovered, Double Ratchet Header Decrypt Success

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-05
**Status:** COMPLETED -- Header decrypt working, MsgHeader fully parsed
**Previous:** Part 15 - Session 18
**Next:** Part 17 - Session 20
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 19 SUMMARY

```
Session 19 discovered three additional parsing layers between E2E
decrypt output and the Double Ratchet: unPad (2-byte length prefix +
0x23 padding), ClientMessage (PrivHeader + AgentMsgEnvelope), and
EncRatchetMessage (v2 format with 1-byte length prefixes). The
PrivHeader mystery from Session 18 was solved: 0x3a was the unPad
length prefix (0x3aae = 15022), not a PrivHeader tag. The actual
PrivHeader is 'K' (PHConfirmation) at byte[2]. Double Ratchet header
decrypt succeeded using nhk (HKDF[32-63]) as header_key_recv.
MsgHeader parsed: msgMaxVersion=3, 68-byte X448 DH key, PN=0, Ns=0.
Bug #19 found: header_key_recv overwritten (workaround functional).

 3 parsing layers discovered (unPad, ClientMessage, EncRatchetMessage)
 PrivHeader mystery solved (0x3a was unPad length, not tag)
 Double Ratchet header decrypt SUCCESS
 MsgHeader fully parsed (version=3, PN=0, Ns=0)
 Bug #19: header_key_recv overwritten (workaround: saved_nhk)
```

---

## Three New Parsing Layers

### Layer: unPad

The 15904 bytes from E2E Layer 2 are not directly a ClientMessage. A padding layer wraps the content:

```
[0-1]           originalLength (Word16 BE) = 15022 (0x3aae)
[2..15023]      ClientMessage (actual content)
[15024..15903]  Padding: 880 bytes of 0x23 ('#')
```

### Layer: ClientMessage

```haskell
smpEncode (ClientMessage h msg) = smpEncode h <> msg
```

Simple concatenation of PrivHeader + body, no length prefix.

PrivHeader encoding (Protocol.hs:1093-1098):

| Tag | Hex | Constructor | Content |
|-----|-----|-------------|---------|
| 'K' | 0x4B | PHConfirmation | 1-byte length + Ed25519/X25519 SPKI key |
| '_' | 0x5F | PHEmpty | nothing |

Byte[2] after unPad = 0x4B ('K') = PHConfirmation.

### Layer: EncRatchetMessage

For v < 3 (legacy): `encodeLarge` uses 1-byte length prefix.

| Field | Encoding | Content |
|-------|----------|---------|
| emHeader Len | 1 byte | 123 (0x7B) |
| emHeader | 123 bytes | EncMessageHeader |
| emAuthTag | 16 bytes raw | AES-GCM auth tag |
| emBody | Tail (rest) | Encrypted payload |

EncMessageHeader (inside emHeader):

| Field | Size | Content |
|-------|------|---------|
| ehVersion | 2 bytes (Word16 BE) | E2E ratchet version |
| ehIV | 16 bytes raw | AES-256-GCM IV |
| ehAuthTag | 16 bytes raw | Header auth tag |
| ehBody Len | 1 byte (v<3) | 88 (0x58) |
| ehBody | 88 bytes | Encrypted MsgHeader |

SimpleX uses 16-byte IVs for AES-256-GCM (not the standard 12 bytes). The 16-byte IV is internally transformed by the cipher layer.

---

## PrivHeader Mystery Solved

Session 18 asked: "What is PrivHeader ':' (0x3a)?"

0x3a was NOT a PrivHeader. It was the first byte of the unPad length prefix: 0x3a 0xae = 15022. The actual PrivHeader is at byte[2] = 0x4B ('K') = PHConfirmation.

---

## X3DH to Header Key Assignment

```
HKDF #1 (X3DH):
  Salt:  64 x 0x00
  IKM:   DH1 || DH2 || DH3 (168 bytes for X448)
  Info:  "SimpleXX3DH"
  Output:
    [0-31]   hk  = header_key_send (peer decrypts our headers)
    [32-63]  nhk = header_key_recv (WE decrypt peer headers)
    [64-95]  sk  = root_key (input for Root KDF)
```

nhk (HKDF[32-63]) is the key that decrypts incoming headers. Verified: `saved_nhk` correctly decrypts the peer's header.

---

## Decrypted MsgHeader

```
contentLen:     79
msgMaxVersion:  3 (peer supports PQ)
DH Key Len:     68 (X448 SPKI)
Peer DH Key:    c3d0cb637a26c2c8... (56 bytes raw)
PN:             0 (first message)
Ns:             0 (message #0)
Padding:        0x23 ('#')
```

---

## Verified Byte-Map

```
Level 1: E2E Plaintext (15904 bytes)
  [0-1]     3a ae       unPad originalLength: 15022
  [2]       4B          PrivHeader 'K' (PHConfirmation)
  [3]       2C          Auth Key Length: 44
  [4-47]    30 2a 30..  Ed25519 SPKI Auth Key
  [48-49]   00 07       agentVersion: 7
  [50]      43          'C' = AgentConfirmation
  [51]      30          e2eEncryption_ = Nothing

Level 2: EncRatchetMessage (from offset 52)
  [52]      7B          emHeader Length: 123
  [53-175]              emHeader (EncMessageHeader)
  [176-191]             emAuthTag (16 bytes)
  [192-15023]           emBody (14832 bytes)
```

---

## Bug #19: header_key_recv Overwritten

```
header_key_recv after X3DH:   1c08e86e... (correct)
header_key_recv at receipt:   cf0c74d2... (wrong)
```

Root cause not yet identified. Workaround: `saved_nhk` copied immediately after X3DH. Fix pending for Session 20.

---

## Encoding Reference (from Haskell Source)

| Primitive | Encoding |
|-----------|----------|
| Word16 | 2 bytes Big-Endian |
| Char | 1 byte |
| ByteString | 1-byte length + data |
| Large | 2-byte Word16 length + data |
| Tail | Rest without length prefix |
| Maybe a | '0'=Nothing, '1'+data=Just (NOT 0x00/0x01!) |
| AuthTag | 16 bytes raw (no prefix) |
| IV | 16 bytes raw |
| PublicKey | 1-byte length + X.509 DER SPKI |
| Tuple | Simple concatenation |

---

*Part 16 - Session 19: Three Layers + Header Decrypt*
*SimpleGo Protocol Analysis*
*Original date: February 5, 2026*
*Rewritten: March 4, 2026 (v2)*
*unPad, ClientMessage, EncRatchetMessage layers, MsgHeader parsed*
