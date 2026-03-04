![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 2: Sessions 3-4
# Padding Architecture, Wire Format Verification, Length Prefix Corrections

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-23 to 2026-01-24
**Status:** COMPLETED -- A_MESSAGE reduced from 2x to 1x, then persists
**Previous:** Part 1 - Sessions 1-2
**Next:** Part 3 - Sessions 5-6
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 3-4 SUMMARY

```
Sessions 3-4 resolved the padding architecture and all length prefix
encoding bugs. The ratchet internal padding (14832 bytes) and
ClientMessage padding (15904 bytes) were implemented, reducing the
A_MESSAGE error from 2x to 1x. The SimpleX app successfully parsed the
AgentConfirmation for the first time. Eight additional encoding bugs
were found in Session 4, all related to Word16 BE length prefixes and
KDF output ordering. The wolfSSL X448 byte order bug was identified as
the root cause of the remaining A_MESSAGE through Python comparison.

12 Bugs Fixed (S3-S4)
 2 Padding layers implemented (ratchet internal + ClientMessage)
 1 Critical crypto library bug identified (wolfSSL X448)
 8 Length prefix bugs corrected (1-byte to Word16 BE)
```

---

## Padding Architecture

SimpleX uses two nested padding layers to prevent traffic analysis. Every message is padded to fixed sizes so all messages look identical on the wire.

### Layer 1: Ratchet Internal Padding (encConnInfo)

```haskell
e2eEncConnInfoLength :: Version -> PQSupport -> Int
e2eEncConnInfoLength v = \case
  PQSupportOn | v >= pqdrSMPAgentVersion -> 11106
  _ -> 14832  -- standard (non-PQ) mode
```

```haskell
rcEncryptMsg rc paddedMsgLen msg = do
  (emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
```

Format: `[Word16 BE original_len][original data]['#' padding to 14832 bytes]`

The payload is padded before AES-GCM encryption. After encryption: emBody = 14832 bytes ciphertext + 16 bytes AuthTag.

### Layer 2: ClientMessage Padding

```haskell
e2eEncConfirmationLength :: Int
e2eEncConfirmationLength = 15904

pad :: ByteString -> Int -> ByteString
pad msg paddedLen = encodeWord16 len <> msg <> B.replicate padLen '#'
  where
    len = B.length msg
    padLen = paddedLen - 2 - len
```

Format: `[Word16 BE original_len][original data]['#' padding to 15904 bytes]`

### Padding Hierarchy

```
Layer 1 (Ratchet):
  Input: AgentConnInfoReply (225 bytes)
  Pad to: 14832 bytes with '#'
  AES-GCM encrypt: 14832 bytes plaintext
  Output: emBody (14832 bytes) + AuthTag (16 bytes)
  
  EncRatchetMessage total:
    1 (emHeader len) + 124 (emHeader) + 16 (emAuthTag) + 14832 (emBody)
    = 14973 bytes

Layer 2 (ClientMessage):
  Input: AgentConfirmation with encConnInfo (~15000 bytes)
  Pad to: 15904 bytes with '#'
  crypto_box encrypt: 15904 bytes plaintext
  Output: ClientMsgEnvelope for SEND command
```

---

## Session 3: Padding Bugs

### Bug: ClientMessage Padding Missing

ESP32 sent only 556 bytes instead of 15904. The pad() function with '#' (0x23) fill was completely absent.

```c
#define E2E_ENC_CONFIRMATION_LENGTH 15904

uint8_t *padded = malloc(E2E_ENC_CONFIRMATION_LENGTH);
padded[0] = (msg_len >> 8) & 0xFF;  // Word16 BE length
padded[1] = msg_len & 0xFF;
memcpy(&padded[2], plaintext, msg_len);
memset(&padded[2 + msg_len], '#', E2E_ENC_CONFIRMATION_LENGTH - 2 - msg_len);
```

### Bug: Ratchet Internal Padding Missing

encConnInfo was ~365 bytes instead of ~14973. The 14832-byte padding before AES-GCM was completely absent.

```c
#define E2E_ENC_CONN_INFO_LENGTH 14832

uint8_t *padded_payload = malloc(E2E_ENC_CONN_INFO_LENGTH);
padded_payload[0] = (pt_len >> 8) & 0xFF;
padded_payload[1] = pt_len & 0xFF;
memcpy(&padded_payload[2], plaintext, pt_len);
memset(&padded_payload[2 + pt_len], '#', E2E_ENC_CONN_INFO_LENGTH - 2 - pt_len);
// Then AES-GCM encrypt padded_payload with length E2E_ENC_CONN_INFO_LENGTH
```

Result: A_MESSAGE reduced from 2x to 1x. App successfully parsed AgentConfirmation.

### Bug: Buffer Overflow Cascade

With 15904-byte padding, all static stack buffers overflowed:

| Buffer | Before | After |
|--------|--------|-------|
| encrypted[] | 1500 | malloc(15944) |
| client_msg[] | 2000 | malloc(16100) |
| send_body[] | 2500 | malloc(16100) |
| transmission[] | 3000 | malloc(16200) |
| enc_conn_info[] | 512 | malloc(16000) |
| agent_msg[] | 2500 | malloc(20000) |
| plaintext[] | 1200 | malloc(20000) |
| agent_envelope[] | 512 | malloc(16000) |

All converted from stack allocation to heap with corresponding free() calls.

### Bug: Payload AAD Size

Body encryption used only rcAD (112 bytes) as AAD, but SimpleX expects rcAD + emHeader.

```haskell
decryptMessage (MessageKey mk iv) EncRatchetMessage {emHeader, emBody, emAuthTag} =
  tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
```

AAD = rcAD (112 bytes) + emHeader (encrypted blob). Note: emHeader here is the encrypted header bytes, not decrypted content, so the recipient has it before payload decryption.

---

## Session 4: Length Prefix and KDF Bugs

### Discovery: Word16 BE for ALL ByteString Lengths

```haskell
instance Encoding ByteString where
  smpEncode s = smpEncode @Word16 (fromIntegral $ B.length s) <> s
```

All ByteString lengths in SimpleX use Word16 Big-Endian (2 bytes), not 1 byte. This was the most common bug class in Sessions 1-4.

| Value | Hex (Word16 BE) | Usage |
|-------|-----------------|-------|
| 0 | 00 00 | Empty string (prevMsgHash) |
| 68 | 00 44 | SPKI key (12 header + 56 raw) |
| 88 | 00 58 | MsgHeader padded |
| 124 | 00 7C | emHeader |

### Bug: KDF Root Output Order

Variable assignments were swapped in kdf_root:

```haskell
(rk', ck, nhk) = hkdf3 rk ss "SimpleXRootRatchet"
-- rk'  = bytes 0-31  = new ROOT key
-- ck   = bytes 32-63 = CHAIN key
-- nhk  = bytes 64-95 = next HEADER key
```

ESP32 had header_key at offset 0 and next_root_key at offset 64 (reversed).

### Bug: ChainKDF IV Order

IVs from chainKdf were swapped:

```haskell
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)
```

iv1 (bytes 64-79) = header IV, iv2 (bytes 80-95) = message IV. ESP32 had them reversed, so header was encrypted with message IV and vice versa.

### Bug: Payload AAD Size (236, not 235)

After emHeader changed from 123 to 124 bytes (due to ehBody Word16 fix), payload AAD = 112 + 124 = 236 bytes, not 235.

### HELLO Message Format

```
HELLO Plaintext (12 bytes):
  4d 00 00 00 00 00 00 00 01 00 00 48
  'M' [    msgId = 1 (Int64 BE)   ] [W16=0] 'H'
  Tag                                len=0   HELLO
```

prevMsgHash uses Word16 BE length (00 00), not 1-byte length.

---

## Updated Wire Format (after Session 4)

### EncRatchetMessage

```
[2B emHeader-len (00 7C)][124B emHeader][16B payload AuthTag][Tail payload]
```

### emHeader / EncMessageHeader (124 bytes)

```
[2B ehVersion (00 02)][16B ehIV][16B ehAuthTag][2B ehBody-len (00 58)][88B ehBody]
Total: 2 + 16 + 16 + 2 + 88 = 124 bytes
```

### MsgHeader plaintext (88 bytes, padded)

```
Offset  Bytes  Description
0-1     2      msgMaxVersion: 00 02
2-3     2      DHRs key length: 00 44 (Word16 BE = 68)
4-15    12     SPKI header: 30 42 30 05 06 03 2b 65 6f 03 39 00
16-71   56     X448 raw public key
72-75   4      msgPN: 00 00 00 00 (Word32 BE)
76-79   4      msgNs: 00 00 00 00 or 01 (Word32 BE)
80-87   8      zero padding
Total: 2 + 2 + 68 + 4 + 4 + 8 = 88 bytes
```

---

## wolfSSL X448 Byte Order Discovery

After all encoding bugs were fixed and server accepted messages with "OK", the app still showed A_MESSAGE. Python comparison testing revealed the root cause:

wolfSSL's X448 implementation (with EC448_BIG_ENDIAN) uses reversed byte order for all keys and DH outputs compared to cryptonite (Haskell) and Python cryptography.

```
Python (cryptography) with REVERSED keys + REVERSED output:
  dh1: 3810171223bfad2d...  rev: 43f2cb51da2aae9c... MATCH with wolfSSL!
  dh2: fbabf5cb9cfcdb2b...  rev: f1fbeb3d13246dc0... MATCH with wolfSSL!
  dh3: c905ebb129ca3ab7...  rev: 7d289ec9a8c11645... MATCH with wolfSSL!
```

Solution: Byte-reverse all keys on import/export and all DH outputs. Implementation details in Part 3.

---

## Connection Flow Status (after Session 4)

```
ESP32                          Server                         App
  |------- TLS Handshake ------->|                              |
  |<------ TLS Established ------|                              |
  |------- NEW (Create Queue) -->|                              |
  |<------ IDS (Queue Created) --|                              |
  |                              |<---- SEND (Invitation) ------|
  |<------ MSG (Invitation) -----|                              |
  |------- KEY + SEND ---------->|                              |
  |        (AgentConfirmation)   |                              |
  |        (15116 bytes)         |                              |
  |<------ OK -------------------|                              |
  |                              |------- MSG --------------->  |
  |                              |        (AgentConfirmation)   |
  |                              |                          PARSED
  |------- SEND (HELLO) -------->|                              |
  |        (14975 bytes)         |                              |
  |<------ OK -------------------|                              |
  |                              |                          A_MESSAGE
```

AgentConfirmation parsed successfully. HELLO message fails with A_MESSAGE due to wolfSSL X448 byte order producing wrong derived keys.

---

## Consolidated Bug List (Sessions 3-4)

| # | Bug | Session | Root Cause | Fix |
|---|-----|---------|------------|-----|
| 19 | ClientMessage padding missing | S3 | Not padded to 15904 bytes | '#' padding |
| 20 | Stack buffer overflow | S3 | Static buffers too small for 15904 | malloc() |
| 21 | Payload AAD 112 instead of 235 | S3 | Missing emHeader in AAD | rcAD + emHeader |
| 22 | Ratchet padding missing | S3 | encConnInfo not padded to 14832 | '#' padding before AES-GCM |
| 23 | KDF root output order | S4 | root/chain/header variables swapped | Correct assignment |
| 24 | E2E key length 1B | S4 | Should be Word16 BE | 00 44 |
| 25 | HELLO prevMsgHash length 1B | S4 | Should be Word16 BE | 00 00 |
| 26 | MsgHeader DH key length 1B | S4 | Should be Word16 BE | 00 44 |
| 27 | ehBody length 1B | S4 | Should be Word16 BE | 00 58 |
| 28 | emHeader length 1B | S4 | Should be Word16 BE | 00 7C |
| 29 | Payload AAD 235 instead of 236 | S4 | emHeader grew from 123 to 124 | 112 + 124 = 236 |
| 30 | ChainKDF IV order swapped | S4 | header_iv/msg_iv reversed | iv1=header(64-79), iv2=msg(80-95) |

**Result after Sessions 3-4:** A_MESSAGE 2x reduced to 1x. Crypto identified as remaining issue (wolfSSL byte order).

---

*Part 2 - Sessions 3-4: Padding Architecture, Wire Format, Length Prefix Corrections*
*SimpleGo Protocol Analysis*
*Original dates: January 23-24, 2026*
*Rewritten: March 4, 2026 (v2)*
*12 bugs fixed, 2 padding layers implemented, wolfSSL byte order identified*
