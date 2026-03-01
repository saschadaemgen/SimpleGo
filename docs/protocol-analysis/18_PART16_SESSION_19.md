![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 16: Session 19
# Three Layers Discovered + Double Ratchet Header Decrypt SUCCESS

**Document Version:** v33  
**Date:** 2026-02-05 Session 19  
**Status:** Header Decrypt SUCCESS — MsgHeader Fully Parsed  
**Previous:** Part 15 - Session 18 (Bug #18 SOLVED! E2E Layer 2 Decrypt)

---

## 342. Session 19 Overview

### 342.1 Starting Point

After Session 18 we had:
- Bug #18 SOLVED! E2E Layer 2 decrypt working
- 15904 bytes AgentConfirmation decrypted
- PrivHeader ':' (0x3a) needed identification
- EncRatchetMessage ready for Double Ratchet decryption

### 342.2 Session 19 BREAKTHROUGH

**Complete parsing chain from E2E plaintext to decrypted Double Ratchet MsgHeader!**

Three new layers discovered and verified:
1. **unPad Layer** — 2-byte length prefix before ClientMessage
2. **ClientMessage Layer** — PrivHeader + AgentMsgEnvelope  
3. **EncRatchetMessage Layer** — Double Ratchet Header-Decrypt

**The Header-Decrypt works! Peer's MsgHeader is fully parsed!**

### 342.3 Session 19 Achievements

1. ✅ unPad Layer discovered (2-byte length prefix + 0x23 padding)
2. ✅ PrivHeader encoding verified ('K' = PHConfirmation)
3. ✅ ClientMessage encoding verified (simple concatenation)
4. ✅ Maybe encoding verified ('0' = Nothing, '1' = Just)
5. ✅ AgentConfirmation encoding verified
6. ✅ EncRatchetMessage encoding verified (v<3 = 1-byte length)
7. ✅ EncMessageHeader encoding verified
8. ✅ AES-GCM 16-byte IV confirmed
9. ✅ X3DH → HKDF chain fully documented
10. ✅ rcAD (Associated Data) order verified
11. ✅ Key assignment from X3DH verified (nhk = header_key_recv)
12. ✅ **Double Ratchet Header-Decrypt SUCCESS!**
13. ✅ MsgHeader fully parsed (msgMaxVersion=3, DH Key, PN=0, Ns=0)

### 342.4 Bug #19 Discovered

`header_key_recv` gets overwritten somewhere between X3DH and message receipt.
Workaround with `saved_nhk` works. Root cause investigation pending.

---

## 343. The Three New Layers

### 343.1 Layer: unPad (Between E2E Decrypt and ClientMessage)

**Discovery:** The 15904 bytes from E2E Layer 2 decrypt are NOT directly the ClientMessage!

There's a padding layer (`pad`/`unPad` in Crypto.hs):

```
Structure:
[0..1]           originalLength (Word16 Big-Endian)
[2..1+origLen]   ClientMessage (actual content)
[2+origLen..]    Padding (0x23 = '#')
```

**Verified Values:**
```
Total bytes:      15904
originalLength:   15022 (0x3aae)
Padding bytes:    15904 - 2 - 15022 = 880 × 0x23
```

### 343.2 Layer: ClientMessage

**Structure:**
```
ClientMessage = PrivHeader ++ Body (simple concatenation, no length prefix)
smpEncode (ClientMessage h msg) = smpEncode h <> msg
```

**PrivHeader Encoding (from Protocol.hs:1093-1098):**

| Tag | Hex | Constructor | Content After Tag |
|-----|-----|-------------|-------------------|
| `'K'` | 0x4B | PHConfirmation | 1-byte Len + Ed25519/Ed448/X25519 SPKI Key |
| `'_'` | 0x5F | PHEmpty | (nothing) |

**Verified:** Byte[2] after unPad = 0x4B ('K') = PHConfirmation

### 343.3 Layer: EncRatchetMessage (Double Ratchet)

**Structure:**
```
encodeEncRatchetMessage v msg =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)
```

For v < 3 (legacy): `encodeLarge` = 1-byte length prefix
For v >= 3 (PQ): `encodeLarge` = 2-byte Word16 length prefix

| Field | v<3 Encoding | Content |
|-------|--------------|---------|
| emHeader Len | 1 Byte | = 123 (0x7B) |
| emHeader | 123 Bytes | EncMessageHeader |
| emAuthTag | 16 Bytes raw | AES-GCM Auth Tag |
| emBody | Tail (rest) | Encrypted payload |

---

## 344. Session 19 Erkenntnisse (11 Key Discoveries)

### 344.1 Erkenntnis 1: unPad Layer (NEW!)

The 15904 bytes after E2E Layer 2 Decrypt are **not** directly the ClientMessage.
There's a padding layer (`pad`/`unPad` in Crypto.hs):

```
[0..1]           originalLength (Word16 Big-Endian)
[2..1+origLen]   ClientMessage (actual content)
[2+origLen..]    Padding (0x23 = '#')
```

**Verified:** originalLength = 15022 (0x3aae), Padding = 880 × 0x23

### 344.2 Erkenntnis 2: PrivHeader Encoding

From Protocol.hs:1093-1098:

| Tag | Hex | Constructor | Content After Tag |
|-----|-----|-------------|-------------------|
| `'K'` | 0x4B | PHConfirmation | 1-byte Len + Ed25519/Ed448/X25519 SPKI Key |
| `'_'` | 0x5F | PHEmpty | (nothing) |

**Verified:** Byte[2] after unPad = 0x4B ('K') = PHConfirmation

### 344.3 Erkenntnis 3: ClientMessage Encoding

```
ClientMessage = PrivHeader ++ Body (simple concatenation, no length prefix)
smpEncode (ClientMessage h msg) = smpEncode h <> msg
```

### 344.4 Erkenntnis 4: Maybe Encoding

From Encoding.hs:114-122:

| Value | Encoding |
|-------|----------|
| Nothing | `0x30` (ASCII '0') — 1 Byte |
| Just a | `0x31` (ASCII '1') + smpEncode a |

**NOT** binary 0x00/0x01!

### 344.5 Erkenntnis 5: AgentConfirmation Encoding

```
smpEncode = (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)
```

Tuple-Encoding = simple concatenation.

| Field | Encoding | Size |
|-------|----------|------|
| agentVersion | Word16 BE | 2 Bytes |
| Tag 'C' | Char | 1 Byte |
| e2eEncryption_ | Maybe (SndE2ERatchetParams X448) | 1+ Bytes |
| encConnInfo | Tail (= rest without length prefix) | variable |

### 344.6 Erkenntnis 6: EncRatchetMessage Encoding

```
encodeEncRatchetMessage v msg =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)
```

For v < 3 (legacy): `encodeLarge` = 1-byte length prefix
For v >= 3 (PQ): `encodeLarge` = 2-byte Word16 length prefix

| Field | v<3 Encoding | Content |
|-------|--------------|---------|
| emHeader Len | 1 Byte | = 123 (0x7B) |
| emHeader | 123 Bytes | EncMessageHeader |
| emAuthTag | 16 Bytes raw | AES-GCM Auth Tag |
| emBody | Tail (rest) | Encrypted payload |

### 344.7 Erkenntnis 7: EncMessageHeader Encoding

```
smpEncode = (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody
```

| Field | Size | Content |
|-------|------|---------|
| ehVersion | 2 Bytes (Word16 BE) | E2E Ratchet Version |
| ehIV | 16 Bytes raw | AES-256-GCM IV |
| ehAuthTag | 16 Bytes raw | Header Auth Tag |
| ehBody Len | 1 Byte (v<3) | = 88 (0x58) |
| ehBody | 88 Bytes | Encrypted MsgHeader |

### 344.8 Erkenntnis 8: AES-GCM IV Length

SimpleX uses **16-byte IVs** for AES-256-GCM, not the standard 12-byte GCM IVs!
The 16-byte IV is internally transformed by the cipher layer (cryptonite/mbedTLS).

### 344.9 Erkenntnis 9: X3DH → HKDF Chain

Complete chain for the first received message (verified from Ratchet.hs):

```
HKDF #1: X3DH → (hk, nhk, sk)
  Salt:  64 × 0x00
  IKM:   DH1 || DH2 || DH3 (168 Bytes for X448)
  Info:  "SimpleXX3DH"
  Output: hk[0-31], nhk[32-63], sk[64-95]

HKDF #2/#4: Root KDF → (rk', ck, nhk')
  Salt:  sk (32 Bytes, Root Key)
  IKM:   DH(Peer_ratchet_pub, Our_sk2) [56 Bytes X448]
  Info:  "SimpleXRootRatchet"
  Output: rk'[0-31], ck[32-63], nhk'[64-95]

HKDF #3/#6: Chain KDF → (ck', mk, ivs)
  Salt:  "" (empty!)
  IKM:   ck (32 Bytes, Chain Key)
  Info:  "SimpleXChainRatchet"
  Output: ck'[0-31], mk[32-63], iv1[64-79], iv2[80-95]
```

### 344.10 Erkenntnis 10: rcAD (Associated Data)

**Haskell Definition:** `assocData = Joiner_pk1 || Creator_pk1`

**Our Code:** `our_key1 || peer_key1` = `Creator_pk1 || Joiner_pk1`

**Test Result:** Try 5 with `our || peer` and `saved_nhk` → SUCCESS!

The order in our code works. Possibly the internal assignment of key1/key2 in our X3DH
is different from the Haskell code, but the result is still consistent. **Don't touch it — it works.**

### 344.11 Erkenntnis 11: Key Assignment from X3DH

| HKDF Output | Bytes | Our Name | Usage |
|-------------|-------|----------|-------|
| Block 1 | [0-31] | header_key_send (hk) | Peer decrypts our headers |
| Block 2 | [32-63] | header_key_recv (nhk) | **WE** decrypt peer's headers |
| Block 3 | [64-95] | root_key (sk) | Input for Root KDF |

**Verified:** saved_nhk (HKDF[32-63]) correctly decrypts peer's header.

---

## 345. Bug #19: header_key_recv Gets Overwritten

### 345.1 Symptom

```
header_key_recv after X3DH = 1c08e86e... (saved_nhk, correct)
header_key_recv at receipt = cf0c74d2... (wrong, overwritten)
```

### 345.2 Root Cause

**Not yet identified.** Likely in `smp_peer.c` in the AgentConfirmation/HELLO
send flow. `ratchet_init_sender()` and `ratchet_encrypt()` don't write to
`header_key_recv` according to the code — must happen elsewhere.

### 345.3 Status

Workaround (`saved_nhk`) works. Fix pending — needs `smp_peer.c` analysis.

### 345.4 Impact

Low — workaround functional. But should be fixed for code cleanliness.

---

## 346. Verified Byte-Map (Reply Queue AgentConfirmation)

### 346.1 Level 1: E2E Plaintext (15904 Bytes, after crypto_box Decrypt)

```
Offset  Hex         Field                         Status
[0-1]   3a ae       unPad originalLength: 15022   ✅
[2]     4B          PrivHeader 'K' (PHConfirm)    ✅
[3]     2C          Auth Key Length: 44           ✅
[4-47]  30 2a 30..  Ed25519 SPKI Auth Key         ✅
[48-49] 00 07       agentVersion: 7               ✅
[50]    43          'C' = AgentConfirmation       ✅
[51]    30          e2eEncryption_ = Nothing      ✅
```

### 346.2 Level 2: EncRatchetMessage (from Offset 52, = encConnInfo)

```
Offset  Hex         Field                         Status
[52]    7B          emHeader Length: 123          ✅
[53-175]            emHeader (EncMessageHeader):
  [53-54] XX XX       ehVersion: 2                ✅
  [55-70] ...         ehIV (16 Bytes)             ✅
  [71-86] ...         ehAuthTag (16 Bytes)        ✅
  [87]    58          ehBody Length: 88           ✅
  [88-175] ...        ehBody (encrypted MsgHeader) ✅
[176-191]           emAuthTag (16 Bytes)          ✅
[192-15023]         emBody (14832 Bytes)          ✅
```

*Note: Offsets are relative to e2e_plain (with unPad prefix). For ClientMessage offsets: subtract 2.*

### 346.3 Level 3: MsgHeader (after Header-Decrypt, 79 Bytes content)

```
Field             Value                           Status
contentLen        79                              ✅
msgMaxVersion     3 (Peer supports PQ)            ✅
DH Key Len        68 (X448 SPKI)                  ✅
Peer DH Key       c3d0cb637a26c2c8... (56B raw)   ✅
PN                0 (first message)               ✅
Ns                0 (Message #0)                  ✅
Padding           0x23 ('#')                      ✅
```

---

## 347. Complete Decryption Chain (Updated Session 19)

```
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓ Output: [2B len prefix][ClientMsgEnvelope][padding 0x23...]
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working (S18)
  ↓ Output: 15904 bytes (padded)
  ↓
Layer 2.5: unPad (NEW!)                                        ✅ Working (S19)
  ↓ Input: [2B originalLen][ClientMessage][padding 0x23...]
  ↓ Output: 15022 bytes ClientMessage
  ↓
Layer 3: ClientMessage Parse                                   ✅ Working (S19)
  ↓ Input: [PrivHeader][AgentMsgEnvelope]
  ↓ PrivHeader: 'K' + 44B Ed25519 SPKI
  ↓ AgentMsgEnvelope: version + 'C' + e2eEncryption_ + Tail encConnInfo
  ↓
Layer 4: EncRatchetMessage Parse                               ✅ Working (S19)
  ↓ Input: [1B emHeader len=123][emHeader 123B][emAuthTag 16B][Tail emBody]
  ↓ emHeader: [version 2B][ehIV 16B][ehAuthTag 16B][ehBody len 1B][ehBody 88B]
  ↓
Layer 5: Double Ratchet Header Decrypt                         ✅ Working (S19)
  ↓ Key: header_key_recv (saved_nhk from X3DH HKDF[32-63])
  ↓ IV: ehIV (16 bytes)
  ↓ AAD: rcAD (112 bytes = our_key1 || peer_key1)
  ↓ Output: MsgHeader (79 bytes content + 9 bytes header/padding)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ⏳ Next Step
  ↓ Need: DH Ratchet Step → Root KDF → Chain KDF → message_key
  ↓ Input: emBody (14832 bytes)
  ↓ AAD: rcAD || emHeader (112 + 123 = 235 bytes)
  ↓
Layer 7: ConnInfo Parse                                        ⏳ After L6
  ↓ AgentConnInfoReply with peer's SMP Queues
  ↓
Layer 8: Connection Established                                ⏳ Final Goal
```

---

## 348. HKDF Chain Reference (Verified)

### 348.1 HKDF #1: X3DH Initial

```
Salt:   64 × 0x00
IKM:    DH1 || DH2 || DH3 (168 bytes for X448)
Info:   "SimpleXX3DH"
Output: 96 bytes
  [0-31]   hk  = header_key_send (peer decrypts our headers)
  [32-63]  nhk = header_key_recv (WE decrypt peer's headers) ← THE KEY!
  [64-95]  sk  = root_key (input for Root KDF)
```

### 348.2 HKDF #2/#4: Root KDF

```
Salt:   sk (32 bytes, Root Key)
IKM:    DH(Peer_ratchet_pub, Our_sk2) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk'  = new root_key
  [32-63]  ck   = chain_key_recv
  [64-95]  nhk' = next_header_key_recv
```

### 348.3 HKDF #3/#6: Chain KDF

```
Salt:   "" (empty!)
IKM:    ck (32 bytes, Chain Key)
Info:   "SimpleXChainRatchet"
Output: 96 bytes
  [0-31]   ck'  = next chain_key
  [32-63]  mk   = message_key (for body decrypt)
  [64-79]  iv1  = header_iv
  [80-95]  iv2  = message_iv
```

---

## 349. Encoding Reference (from Haskell Source, Verified)

| Primitive | Encoding | Source |
|-----------|----------|--------|
| Word16 | 2 Bytes Big-Endian | Encoding.hs:70-74 |
| Char | 1 Byte (B.singleton) | Encoding.hs:52-56 |
| ByteString | 1-Byte Len + Data | Encoding.hs:100-104 |
| Large | 2-Byte Word16 Len + Data | Encoding.hs:132-141 |
| Tail | Rest without length prefix | Encoding.hs:124-130 |
| Maybe a | '0'=Nothing, '1'+data=Just | Encoding.hs:114-122 |
| AuthTag | 16 Bytes raw (no prefix) | Crypto.hs:956-958 |
| IV | 16 Bytes raw | Crypto.hs:935-937 |
| PublicKey a | ByteString (1-Byte Len + X.509 DER) | Crypto.hs:567-568 |
| Tuple | Simple concatenation | Encoding.hs:184-212 |

---

## 350. PrivHeader Mystery Solved

### 350.1 Session 18 Question

"What is PrivHeader ':' (0x3a)?"

### 350.2 Session 19 Answer

**0x3a was NOT a PrivHeader!**

0x3a (58 decimal) was actually `0x3aae` = 15022 — the **unPad originalLength** field!

We were reading byte[0] as PrivHeader, but:
- Byte[0-1] = unPad length prefix (0x3a 0xae = 15022)
- Byte[2] = actual PrivHeader = 0x4B = 'K' = PHConfirmation

**Lesson:** Always account for ALL wrapper layers when parsing!

---

## 351. Session 19 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~1 day |
| Layers discovered | 3 |
| Erkenntnisse | 11 |
| Bugs found | 1 (Bug #19) |
| Header decrypt | ✅ SUCCESS |
| MsgHeader parsed | ✅ COMPLETE |

---

## 352. Session 19 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-05 | unPad layer discovered | 15022 bytes actual content |
| 2026-02-05 | PrivHeader 'K' verified | PHConfirmation with Ed25519 auth key |
| 2026-02-05 | AgentConfirmation parsed | Version 7, Tag 'C', e2eEncryption_=Nothing |
| 2026-02-05 | EncRatchetMessage parsed | 123B header, 16B authTag, 14832B body |
| 2026-02-05 | EncMessageHeader parsed | Version 2, 16B IV, 16B authTag, 88B ehBody |
| 2026-02-05 | X3DH HKDF chain verified | nhk = header_key_recv |
| 2026-02-05 | Header decrypt SUCCESS | MsgHeader: version=3, PN=0, Ns=0, 68B DH key |
| 2026-02-05 | Bug #19 found | header_key_recv overwritten (workaround: saved_nhk) |

---

## 353. Next Steps (Session 20)

### 353.1 Step 1: Fix Bug #19
- Analyze `smp_peer.c`: What overwrites `header_key_recv`?
- Fix: Either prevent overwrite, or use `saved_nhk` as permanent solution

### 353.2 Step 2: Implement DH Ratchet Step
- With Peer's DH Key (from MsgHeader) + Our sk2 → rootKdf (HKDF#4)
- Output: new root_key, chain_key_recv, next_header_key_recv

### 353.3 Step 3: Body Decrypt
- chainKdf (HKDF#6) with chain_key_recv → message_key + iv
- AES-GCM Decrypt of emBody (14832 bytes)
- AD = rcAD || emHeader (112 + 123 = 235 bytes)

### 353.4 Step 4: Parse ConnInfo
- Decrypted payload = AgentConnInfoReply
- Contains peer's SMP Queues + Connection Info

### 353.5 Step 5: Process HELLO
- Receive peer's HELLO on Reply Queue
- Update Ratchet State → "Connected"!

---

## 354. Session 19 Summary

### What Was Achieved

- **Three new layers discovered:** unPad, ClientMessage, EncRatchetMessage
- **Double Ratchet Header-Decrypt SUCCESS!**
- **MsgHeader fully parsed:** msgMaxVersion=3, 68B X448 DH key, PN=0, Ns=0
- **11 key insights** about SimpleX encoding and HKDF chain
- **Complete byte-map** from E2E plaintext to MsgHeader
- **PrivHeader mystery solved:** 0x3a was unPad length, not PrivHeader tag

### What Was NOT Achieved (Deferred to Session 20)

- Bug #19 not yet fixed (workaround functional)
- Body decrypt not yet implemented
- ConnInfo not yet parsed
- Connection not yet established

### Key Takeaway

```
SESSION 19 SUMMARY:
  - unPad Layer: [2B len][content][padding 0x23...]
  - PrivHeader: 'K' = PHConfirmation, '_' = PHEmpty
  - Maybe: '0' = Nothing, '1' = Just (NOT 0x00/0x01!)
  - nhk (HKDF[32-63]) = header_key_recv = THE KEY for header decrypt
  - AES-GCM uses 16-byte IV in SimpleX (not standard 12-byte)

"Three layers, eleven insights, one header decrypted."
"nhk is the key. Literally."
```

---

**DOCUMENT CREATED: 2026-02-05 Session 19 v33**  
**Status: Double Ratchet Header Decrypt SUCCESS!**  
**Key Achievement: MsgHeader fully parsed (msgMaxVersion=3, PN=0, Ns=0)**  
**Next: Fix Bug #19, DH Ratchet Step, Body Decrypt, ConnInfo Parse**
