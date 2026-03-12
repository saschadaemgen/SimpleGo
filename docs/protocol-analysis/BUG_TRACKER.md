![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# Bug Tracker

## Complete Documentation of All 71 Bugs

This document provides detailed documentation of all bugs discovered during SimpleGo development, including the incorrect code, correct code, and root cause analysis.

---

## Summary

| Bug # | Component | Session | Status |
|-------|-----------|---------|--------|
| 1 | E2E key length | 4 | FIXED |
| 2 | prevMsgHash length | 4 | FIXED |
| 3 | MsgHeader DH key | 4 | FIXED |
| 4 | ehBody length | 4 | FIXED |
| 5 | emHeader size | 4 | FIXED |
| 6 | Payload AAD size | 4 | FIXED |
| 7 | Root KDF output order | 4 | FIXED |
| 8 | Chain KDF IV order | 4 | FIXED |
| 9 | wolfSSL X448 byte order | 5 | FIXED |
| 10 | Port encoding | 6 | FIXED |
| 11 | smpQueues count | 6 | FIXED |
| 12 | queueMode Nothing | 6 | FIXED |
| 13 | Payload AAD length prefix | 8 | FIXED |
| 14 | chainKdf IV assignment | 8 | FIXED |
| 15 | Reply Queue HSalsa20 | 9 | FIXED |
| 16 | A_CRYPTO header AAD | 9 | FIXED |
| 17 | cmNonce instead of msgId | 10C | FIXED |
| 18 | Reply Queue E2E | 12-18 | FIXED |
| 19 | header_key_recv overwritten | 19-20 | FIXED |
| 20 | PrivHeader for HELLO | 21 | FIXED |
| 21 | AgentVersion for AgentMessage | 21 | FIXED |
| 22 | prevMsgHash encoding | 21 | FIXED |
| 23 | cbEncrypt padding | 21 | FIXED |
| 24 | DH Key for HELLO | 21 | FIXED |
| 25 | PubHeader Nothing encoding | 21 | FIXED |
| 26 | v2/v3 EncRatchetMessage format | 21 | FIXED |
| 27 | E2E Version Mismatch | 22 | FIXED |
| 28 | KEM Parser Crash | 22 | FIXED |
| 29 | Body Decrypt Pointer-Arithmetik | 22 | FIXED |
| 30 | HKs/NHKs Init + Promotion | 22 | FIXED |
| 31 | Phase 2a Try-Order | 22 | FIXED |
| **32** | **Heap Overflow PQ Headers** | **25** | **FIXED** |
| **33** | **txCount Hardcoded** | **25** | **FIXED** |
| **34** | **Nonce Offset Wrong** | **25** | **FIXED** |
| **35** | **Ratchet State Copy** | **25** | **FIXED** |
| **36** | **Chain KDF Skip Relative** | **25** | **FIXED** |
| **37** | **Receipt count=Word16** | **25** | **FIXED** |
| **38** | **Receipt rcptInfo=Word32** | **25** | **FIXED** |
| **39** | **NULL contact Reply Queue** | **25** | **FIXED** |

**Total: 72 bugs documented, 69 FIXED, 1 identified (SPI3), 1 temp fix, 1 SHOWSTOPPER**

---

## Bug #1: E2E Key Length Prefix

**Session:** 4
**Component:** E2ERatchetParams encoding
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44; // 68 as Word16
memcpy(&buf[p], spki_key, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44; // 68 as single byte
memcpy(&buf[p], spki_key, 68);
```

### Root Cause

E2ERatchetParams keys are encoded as ByteString (1-byte prefix), not Large (Word16 prefix).

---

## Bug #2: prevMsgHash Length Prefix

**Session:** 4
**Component:** AgentMessage encoding
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// 1-byte length prefix (WRONG!)
buf[p++] = 0x00; // Empty hash
```

### Correct Code
```c
// Word16 BE length prefix (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x00; // Empty hash as Word16
```

### Root Cause

AgentMessage uses Large wrapper for prevMsgHash, requiring Word16 prefix.

---

## Bug #3: MsgHeader DH Key Length

**Session:** 4
**Component:** MsgHeader encoding
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Root Cause

MsgHeader msgDHRs is PublicKey, encoded as ByteString with 1-byte prefix.

---

## Bug #4: ehBody Length Prefix

**Session:** 4
**Component:** EncMessageHeader encoding
**Impact:** Critical - cascades to bugs #5 and #6

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
em_header[hp++] = 0x00;
em_header[hp++] = 0x58; // 88 as Word16
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
em_header[hp++] = 0x58; // 88 as single byte
```

### Root Cause

ehBody is ByteString, not Large.

---

## Bug #5: emHeader Size

**Session:** 4
**Component:** EncMessageHeader structure
**Impact:** Critical - cascades to bug #6

### Incorrect Code
```c
#define EM_HEADER_SIZE 124
uint8_t em_header[124];
```

### Correct Code
```c
#define EM_HEADER_SIZE 123
uint8_t em_header[123];
```

### Root Cause

Cascaded from Bug #4 - with 1-byte prefix, size is 123 not 124.

---

## Bug #6: Payload AAD Size

**Session:** 4
**Component:** AES-GCM AAD
**Impact:** Critical - auth tag mismatch

### Incorrect Code
```c
uint8_t payload_aad[236]; // WRONG!
aes_gcm_encrypt(..., payload_aad, 236, ...);
```

### Correct Code
```c
uint8_t payload_aad[235]; // CORRECT!
aes_gcm_encrypt(..., payload_aad, 235, ...);
```

### Root Cause

Cascaded from Bug #5 - AAD = 112 + 123 = 235, not 236.

---

## Bug #7: Root KDF Output Order

**Session:** 4
**Component:** Root KDF implementation
**Impact:** Critical - all keys wrong

### Incorrect Code
```c
// Wrong order!
memcpy(chain_key, kdf_output, 32);
memcpy(new_root_key, kdf_output + 32, 32);
```

### Correct Code
```c
// Correct order per Haskell
memcpy(new_root_key, kdf_output, 32);
memcpy(chain_key, kdf_output + 32, 32);
memcpy(next_header_key, kdf_output + 64, 32);
```

### Root Cause

Misread Haskell source - output order is root, chain, header.

---

## Bug #8: Chain KDF IV Order

**Session:** 4
**Component:** Chain KDF implementation
**Impact:** Critical - encryption uses wrong IVs

### Incorrect Code
```c
// Swapped! (WRONG!)
memcpy(msg_iv, kdf_output + 64, 16);
memcpy(header_iv, kdf_output + 80, 16);
```

### Correct Code
```c
// Correct order!
memcpy(header_iv, kdf_output + 64, 16); // iv1 = header
memcpy(msg_iv, kdf_output + 80, 16); // iv2 = message
```

### Root Cause

iv1 (bytes 64-79) is header IV, iv2 (bytes 80-95) is message IV.

---

## Bug #9: wolfSSL X448 Byte Order

**Session:** 5
**Component:** X448 cryptography
**Impact:** Critical - all DH computations wrong

### The Problem

wolfSSL X448 uses little-endian, SimpleX expects big-endian.

### The Fix
```c
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
 for (size_t i = 0; i < len; i++) {
 dst[i] = src[len - 1 - i];
 }
}

// After key generation:
reverse_bytes(pub_tmp, keypair->public_key, 56);
reverse_bytes(priv_tmp, keypair->private_key, 56);

// Before DH:
reverse_bytes(their_public, their_public_rev, 56);
reverse_bytes(my_private, my_private_rev, 56);

// After DH:
reverse_bytes(secret_tmp, shared_secret, 56);
```

### Root Cause

wolfSSL defines EC448_LITTLE_ENDIAN internally.

---

## Bug #10: Port Encoding

**Session:** 6
**Component:** SMPQueueInfo encoding
**Impact:** Critical - parser fails

### Incorrect Code
```c
// Length prefix (WRONG!)
buf[p++] = (uint8_t)strlen(port_str);
memcpy(&buf[p], port_str, strlen(port_str));
```

### Correct Code
```c
// Space separator (CORRECT!)
buf[p++] = ' '; // 0x20
memcpy(&buf[p], port_str, strlen(port_str));
```

### Root Cause

SMPServer encoding uses space separator, not length prefix.

---

## Bug #11: smpQueues Count

**Session:** 6
**Component:** NonEmpty list encoding
**Impact:** Critical - parser fails

### Incorrect Code
```c
// 1-byte count (WRONG!)
buf[p++] = 0x01;
```

### Correct Code
```c
// Word16 BE count (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x01;
```

### Root Cause

NonEmpty list uses Word16 for count.

---

## Bug #12: queueMode Nothing

**Session:** 6
**Component:** SMPQueueInfo encoding
**Impact:** Medium - parser might fail

### Incorrect Code
```c
// Send '0' byte (WRONG!)
buf[p++] = '0'; // 0x30
```

### Correct Code
```c
// Send NOTHING (CORRECT!)
// (no code - just don't write anything)
```

### Root Cause

queueMode uses "maybe empty" not standard Maybe encoding.

---

## Bug #13: Payload AAD Length Prefix (SESSION 8 BREAKTHROUGH!)

**Session:** 8
**Component:** Payload AAD construction
**Impact:** Critical - AgentConfirmation rejected

### The Discovery

Haskell `largeP` parser removes length prefix from parsed object:
```haskell
largeP :: Parser a -> Parser a
largeP p = smpP >>= \len -> A.take (fromIntegral (len :: Word16)) >>= parseAll p
```

### Incorrect Code
```c
// AAD with length prefix (WRONG!)
uint8_t payload_aad[237]; // 2 + 112 + 123
payload_aad[0] = (total_len >> 8) & 0xFF; // Length prefix
payload_aad[1] = total_len & 0xFF;
memcpy(&payload_aad[2], header_aad, 112);
memcpy(&payload_aad[114], em_header, 123);
```

### Correct Code
```c
// AAD WITHOUT length prefix (CORRECT!)
uint8_t payload_aad[235]; // 112 + 123
memcpy(&payload_aad[0], header_aad, 112);
memcpy(&payload_aad[112], em_header, 123);
```

### Root Cause

The length prefix is consumed by the parser, not included in AAD.

---

## Bug #14: chainKdf IV Assignment (SESSION 8)

**Session:** 8
**Component:** Chain KDF IV handling
**Impact:** Critical - wrong IVs used for encryption

### The Discovery

Session 4 found the order but assignment was still swapped later.

### Incorrect Code
```c
// Assignments swapped (WRONG!)
uint8_t *header_iv = &chain_kdf_output[80]; // iv2
uint8_t *msg_iv = &chain_kdf_output[64]; // iv1
```

### Correct Code
```c
// Correct assignments!
uint8_t *header_iv = &chain_kdf_output[64]; // iv1 = header
uint8_t *msg_iv = &chain_kdf_output[80]; // iv2 = message
```

### Root Cause

Chain KDF output layout:
```
[0:32] next_chain_key
[32:64] message_key
[64:80] iv1 = HEADER_IV
[80:96] iv2 = MESSAGE_IV
```

---

## Bug #15: Reply Queue HSalsa20 (SESSION 9)

**Session:** 9
**Component:** Reply Queue E2E decryption
**Impact:** Critical - Reply Queue decrypt fails

### The Discovery

NaCl `crypto_box` includes HSalsa20 key derivation internally.

### Incorrect Code
```c
// crypto_scalarmult only does raw X25519 (WRONG!)
crypto_scalarmult(dh_secret, rcv_dh_private, srv_dh_public);
// dh_secret is RAW, not ready for XSalsa20-Poly1305!
```

### Correct Code
```c
// crypto_box_beforenm does X25519 + HSalsa20 (CORRECT!)
crypto_box_beforenm(dh_secret, srv_dh_public, rcv_dh_private);
// dh_secret is NOW ready for crypto_box_open_easy_afternm!
```

### Root Cause

Must use same crypto primitive chain as sender.

---

## Bug #16: A_CRYPTO Header AAD (SESSION 9)

**Session:** 9
**Component:** Header encryption AAD
**Impact:** Critical - A_CRYPTO error in app

### The Problem

Header encryption AAD format was incorrect.

### Root Cause

Incorrect AAD construction for header encryption causing authentication failure.

---

## Bug #17: cmNonce instead of msgId (SESSION 10C)

**Session:** 10C
**Component:** Per-Queue E2E Decryption
**Impact:** Critical - All Contact Queue messages fail decryption

### The Discovery

Used `msgId` as nonce for per-queue E2E decryption, but the correct nonce is `cmNonce` from the ClientMsgEnvelope structure.

### Incorrect Code
```c
// WRONG - used msgId as nonce
memcpy(nonce, msg_id, msgIdLen); // msgId from MSG header
```

### Correct Code
```c
// CORRECT - extract cmNonce from ClientMsgEnvelope
int cm_nonce_offset = spki_offset + 44; // [60-83]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);

// Then decrypt with cmNonce
crypto_box_open_easy_afternm(plain, &data[cm_enc_body_offset],
 enc_len, cm_nonce, dh_shared);
```

---

## Bug #18: Reply Queue E2E Decryption -- SOLVED!

**Sessions:** 12, 13, 14, 15, 16, 17, 18
**Component:** Reply Queue Per-Queue E2E Layer 2 → envelope_len calculation
**Impact:** Cannot decrypt Reply Queue messages
**Status:** **SOLVED in Session 18!**

### Root Cause & Fix

```
ROOT CAUSE:
 envelope_len = plain_len - 2 = 16104 ← WRONG! Includes 102B SMP padding
 envelope_len = raw_len_prefix = 16002 ← CORRECT! Exact content length

FIX -- ONE LINE:
 envelope_len = raw_len_prefix;

RESULT:
 Method 0 (decrypt_client_msg): SUCCESS!
 Decrypted: 15904 bytes AgentConfirmation + EncRatchetMessage
```

See Session 18 documentation for full 7-session debugging history.

---

## Bug #19: header_key_recv Gets Overwritten -- SOLVED!

**Sessions:** 19, 20
**Component:** Double Ratchet key management → debug self-decrypt test
**Impact:** Medium - header decrypt fails without workaround
**Status:** **SOLVED in Session 20!**

### 19.1 Symptom

```
header_key_recv after X3DH = 1c08e86e... (saved_nhk, correct)
header_key_recv at receipt = cf0c74d2... (wrong, overwritten)
```

### 19.2 Root Cause -- FOUND (Session 20)

**`smp_peer.c:347`** -- Debug self-decrypt test calling `ratchet_decrypt()`.

After encrypting the AgentConfirmation, a debug self-test called `ratchet_decrypt()`
on our own encrypted message. `ratchet_decrypt()` has **side effects**: it performs
a DH ratchet step when it detects a "new" DH key in the decrypted header.

Corrupted: `header_key_recv`, `root_key`, `chain_key_recv`, `dh_peer`, `msg_num_recv`.

### 19.3 Fix Applied (Session 20)

Removed the debug self-decrypt test from `smp_peer.c:343-359`.
Branch: `claude/fix-header-key-recv-bug-DNYeF` → merged to main.

---

## Bug #20: PrivHeader for HELLO (SESSION 21)

**Session:** 21
**Component:** ClientMessage encoding for HELLO
**Impact:** Critical - wrong message type indicator

### Incorrect Code
```c
// Used PHEmpty tag (WRONG!)
buf[p++] = '_'; // 0x5F = PHEmpty (Confirmation without key)
```

### Correct Code
```c
// No PrivHeader for regular messages (CORRECT!)
buf[p++] = 0x00; // No PrivHeader
```

### Root Cause

PrivHeader encoding is NOT a standard Maybe:
- `'K'` (0x4B) = PHConfirmation (with sender auth key)
- `'_'` (0x5F) = PHEmpty (confirmation without key)
- `0x00` = No PrivHeader (regular messages like HELLO)

HELLO is a regular AgentMessage, not a Confirmation.

---

## Bug #21: AgentVersion for AgentMessage (SESSION 21)

**Session:** 21
**Component:** AgentMsgEnvelope encoding
**Impact:** Critical - parser version mismatch

### Incorrect Code
```c
// Used Agent protocol version (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x02; // agentVersion = 2
```

### Correct Code
```c
// AgentMessage uses version 1 (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x01; // agentVersion = 1
```

### Root Cause

AgentConfirmation uses agentVersion=7 (protocol version), but AgentMessage (HELLO)
uses agentVersion=1 (message format version). Different fields, different values.

---

## Bug #22: prevMsgHash Encoding (SESSION 21)

**Session:** 21
**Component:** AgentMessage encoding
**Impact:** Critical - parser fails on hash field

### Incorrect Code
```c
// Raw empty bytes or missing (WRONG!)
```

### Correct Code
```c
// smpEncode(ByteString) with Word16 prefix (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x00; // Word16 BE length = 0 (empty hash)
```

### Root Cause

prevMsgHash field uses Large encoding (Word16 prefix). For empty hash: `[0x00][0x00]`.
Related to Bug #2 (same encoding pattern).

---

## Bug #23: cbEncrypt Padding (SESSION 21)

**Session:** 21
**Component:** Server-level encryption (cbEncrypt)
**Impact:** Critical - server rejects or app can't decrypt

### Incorrect Code
```c
// Encrypt raw plaintext (WRONG!)
cbEncrypt(key, nonce, raw_plaintext, raw_len, ...);
```

### Correct Code
```c
// Pad BEFORE encrypt (CORRECT!)
pad(raw_plaintext, raw_len, padded_buf, &padded_len);
cbEncrypt(key, nonce, padded_buf, padded_len, ...);
```

### Root Cause

The `pad` function adds a 2-byte length prefix and 0x23 padding BEFORE encryption.
Receiver does: decrypt → unPad. Sender must: pad → encrypt.

---

## Bug #24: DH Key for HELLO (SESSION 21)

**Session:** 21
**Component:** Per-queue E2E encryption key selection
**Impact:** Critical - E2E layer fails

### Incorrect Code
```c
// Used receiver's DH key (WRONG!)
compute_e2e_secret(rcv_dh_public, our_private, ...);
```

### Correct Code
```c
// Use sender's DH key for HELLO (CORRECT!)
compute_e2e_secret(snd_dh_public, our_private, ...);
```

### Root Cause

For Confirmation: use `rcv_dh` (receiver's DH key from the queue).
For HELLO: use `snd_dh` (sender's DH key for the reply queue).

---

## Bug #25: PubHeader Nothing Encoding (SESSION 21)

**Session:** 21
**Component:** ClientMsgEnvelope PubHeader field
**Impact:** Medium - parser may fail

### Incorrect Code
```c
// Field missing entirely (WRONG!)
```

### Correct Code
```c
// Maybe Nothing = '0' (CORRECT!)
buf[p++] = '0'; // 0x30 = Nothing
```

### Root Cause

PubHeader in ClientMsgEnvelope is a Maybe type. When Nothing, must be encoded
as `'0'` (0x30), not omitted.

---

## Bug #26: v2/v3 EncRatchetMessage Format (SESSION 21)

**Session:** 21
**Component:** EncRatchetMessage encoding
**Impact:** Critical - App can't decrypt HELLO (RSYNC error)

### The Discovery

App initialized ratchet with `currentE2EEncryptVersion = 3` (v3), but our
EncRatchetMessage was encoded in v2 format.

### Incorrect Code (v2)
```c
#define RATCHET_VERSION 2
em_header[hp++] = 0x7B; // emHeader len = 123 (1 byte)
em_header[hp++] = 0x58; // ehBody len = 88 (1 byte)
#define EM_HEADER_SIZE 123
// No KEM field in MsgHeader
```

### Correct Code (v3)
```c
#define RATCHET_VERSION 3
em_header[hp++] = 0x00;
em_header[hp++] = 0x7C; // emHeader len = 124 (2 bytes Word16 BE)
em_header[hp++] = 0x00;
em_header[hp++] = 0x58; // ehBody len = 88 (2 bytes Word16 BE)
#define EM_HEADER_SIZE 124
// KEM Nothing: msg_header[p++] = '0';
```

### Root Cause

`encodeLarge` switches at v≥3: 1-byte (Word8) → 2-byte (Word16 BE) prefix.
Also MsgHeader must include KEM Nothing field in v3.

---

## Bug #27: E2E Version Mismatch (SESSION 22)

**Session:** 22
**Component:** `smp_x448.c` E2ERatchetParams encoding
**Impact:** Critical - App breaks silence after fix!

### The Discovery

`smp_x448.c` sent `version_min = 2` in the AgentConfirmation, but `smp_ratchet.c`
encrypted HELLO in v3 format. The version mismatch caused the App to expect v2
format but receive v3.

### Incorrect Code
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x02; // version_min = 2
// No KEM Nothing-Byte after key2
```

### Correct Code
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x03; // version_min = 3
// After key2:
buf[p++] = 0x30; // KEM Nothing ('0' = 0x30)
```

### Root Cause

`smp_x448.c` was not updated in Session 21 when v3 was implemented in `smp_ratchet.c`.
The `version_min` in E2ERatchetParams must match `RATCHET_VERSION` used for encryption.

---

## Bug #28: KEM Parser Crash (SESSION 22)

**Session:** 22
**Component:** `smp_ratchet.c` MsgHeader parser
**Impact:** Critical - Parser crash on PQ responses

### The Discovery

App responds with v3 + SNTRUP761 KEM (2310 bytes) instead of 88-byte header.
Parser had fixed offsets → read garbage → crash.

### Incorrect Code
```c
// Fixed offset calculation
int dh_key_offset = 4; // contentLen(2) + msgMaxVersion(2)
int pn_offset = dh_key_offset + 1 + dh_key_len; // No KEM handling
```

### Correct Code
```c
// Dynamic KEM handling
int kem_offset = dh_key_offset + 1 + dh_key_len;
uint8_t kem_tag = decrypted_header[kem_offset];
if (kem_tag == '0') { // Nothing
 pn_offset = kem_offset + 1;
} else if (kem_tag == '1') { // Just
 uint8_t state_tag = decrypted_header[kem_offset + 1];
 if (state_tag == 'P' || state_tag == 'A') {
 // Read length prefix, skip KEM data
 uint16_t kem_len = (decrypted_header[kem_offset + 2] << 8) |
 decrypted_header[kem_offset + 3];
 pn_offset = kem_offset + 4 + kem_len;
 }
}
```

### Root Cause

MsgHeader parser expected 88-byte header without KEM field. v3+PQ headers can be
2346 bytes with SNTRUP761 (1158B pubkey + 1039B ciphertext + overhead).

---

## Bug #29: Body Decrypt Pointer-Arithmetik (SESSION 22)

**Session:** 22
**Component:** `main.c` body decrypt offset calculation
**Impact:** Critical - 2GB malloc fail on body decrypt

### The Discovery

emHeader is now 2346 bytes (v3+PQ) instead of 123 bytes (v2), but pointer
calculation for emAuthTag/emBody was hardcoded → garbage offsets → 2GB malloc fail.

### Incorrect Code
```c
#define EM_HEADER_SIZE 124 // Hardcoded
uint8_t *emAuthTag = &encrypted[EM_HEADER_SIZE];
uint8_t *emBody = &encrypted[EM_HEADER_SIZE + 16];
```

### Correct Code
```c
// Read ehVersion to determine size
uint16_t ehVersion = (encrypted[0] << 8) | encrypted[1];
size_t emHeader_size;
if (ehVersion >= 3) {
 // v3: 2-byte length prefix
 emHeader_size = (encrypted[2] << 8) | encrypted[3];
 emHeader_size += 4; // Include prefix itself
} else {
 // v2: 1-byte length prefix
 emHeader_size = encrypted[2] + 3;
}
uint8_t *emAuthTag = &encrypted[emHeader_size];
uint8_t *emBody = &encrypted[emHeader_size + 16];
```

### Root Cause

Header sizes vary dramatically:
- v2: 123 bytes
- v3: 124 bytes
- v3+PQ: 2346 bytes (with SNTRUP761)

All offset calculations must be dynamic based on actual header content.

---

## Bug #30: HKs/NHKs Init + Promotion (SESSION 22)

**Session:** 22
**Component:** `smp_ratchet.c` header key management
**Impact:** Critical - Header key chain broken from init to promotion

### The Discovery

Three connected problems in header key handling:

**Problem 30a:** `next_header_key_send` was never stored in ratchet state (local variable only).

**Problem 30b:** `ratchet_x3dh_sender()` stored `nhk` (= rcvNextHK = NHKr) incorrectly
in `header_key_recv` instead of `next_header_key_recv`.

**Problem 30c:** After DH Ratchet Step, KDF output was set directly as HKs instead
of proper NHKs→HKs promotion.

### Incorrect Code
```c
// In ratchet_init_sender():
uint8_t next_header_key_send[32]; // Local variable, never saved!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.header_key_recv, nhk, 32); // WRONG! nhk is NHKr
// ...
// After DH Ratchet Step:
memcpy(ratchet_state.header_key_send, kdf_output + 64, 32); // Direct, no promotion
```

### Correct Code
```c
// In ratchet_init_sender():
memcpy(ratchet_state.next_header_key_send, hkdf_output + 64, 32); // SAVE to state!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.next_header_key_recv, nhk, 32); // NHKr, will promote to HKr
// ...
// After DH Ratchet Step - PROMOTION:
memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32); // NHKs→HKs
memcpy(ratchet_state.next_header_key_send, kdf_output + 64, 32); // New NHKs from KDF
```

### Root Cause

The 4 Header Key architecture requires:
- HKs/NHKs for sending (current/next)
- HKr/NHKr for receiving (current/next)

Promotion: `HKs ← NHKs` then `NHKs ← KDF output` (not direct assignment).
Initial: `nhk` from X3DH is NHKr, promotes to HKr on first AdvanceRatchet.

---

## Bug #31: Phase 2a Try-Order (SESSION 22)

**Session:** 22
**Component:** `main.c` header decrypt try sequence
**Impact:** Critical - AdvanceRatchet never triggered

### The Discovery

Header decrypt tried `next_header_key_recv` only via debug fallback (`saved_nhk`),
not as a regular try → AdvanceRatchet was never triggered → ratchet state stuck.

### Incorrect Code
```c
// Only tried HKr
if (try_header_decrypt(header_key_recv, ...)) {
 // SameRatchet
} else {
 // Debug fallback using saved_nhk (not proper flow)
 if (try_header_decrypt(saved_nhk, ...)) {
 // This worked but didn't trigger AdvanceRatchet!
 }
}
```

### Correct Code
```c
// Try HKr first (SameRatchet)
if (try_header_decrypt(header_key_recv, ...)) {
 decrypt_mode = SAME_RATCHET;
}
// Try NHKr second (AdvanceRatchet)
else if (try_header_decrypt(next_header_key_recv, ...)) {
 decrypt_mode = ADVANCE_RATCHET;
 // Promote: HKr ← NHKr
 memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
 // Trigger full DH ratchet step...
}
```

### Root Cause

Double Ratchet requires trying keys in order:
1. HKr (SameRatchet) -- same DH key, just chain forward
2. NHKr (AdvanceRatchet) -- new DH key, full ratchet step

If NHKr succeeds, it triggers AdvanceRatchet and promotes NHKr→HKr.

---

## Bug Discovery Timeline

| Date | Session | Bugs Found |
|------|---------|------------|
| Jan 23, 2026 | S4 | #1-#6 |
| Jan 24, 2026 | S4 | #7-#8 |
| Jan 24, 2026 | S5 | #9 |
| Jan 24, 2026 | S6 | #10-#12 |
| Jan 27, 2026 | S8 | #13-#14 |
| Jan 27, 2026 | S9 | #15-#16 |
| Jan 28, 2026 | S10C | #17 |
| Jan 30, 2026 | S12-S13 | #18 (deep analysis) |
| Jan 31-Feb 1 | S14 | #18 DH SECRET VERIFIED! |
| Feb 1 | S15 | #18 Root Cause (later disproven) |
| Feb 1-3 | S16 | #18 Custom XSalsa20! |
| Feb 4 | S17 | #18 Key Consistency Debug |
| Feb 5 | S18 | #18 SOLVED! One-line fix! |
| Feb 5 | S19 | #19 header_key_recv overwritten (workaround) |
| Feb 6 | S20 | #19 SOLVED! Root cause: debug self-decrypt |
| Feb 6-7 | S21 | #20-#26 HELLO format + v3 format (7 bugs!) |
| **Feb 7** | **S22** | **#27-#31 E2E version, KEM parser, NHK promotion (5 bugs!)** |
| **Feb 7-8** | **S23** | ** ZERO new bugs -- CONNECTED!** |
| **Feb 11-13** | **S24** | ** ZERO new bugs -- First Chat Message!** |

---

## Bug Categories

```
31 Bugs Total (31 FIXED):
- 7x Length Prefix issues (#1-6, #13)
- 3x KDF/IV Order issues (#7, #8, #14)
- 1x Byte Order issue (#9 - wolfSSL)
- 1x Separator issue (#10)
- 1x Maybe encoding issue (#12)
- 1x AAD construction issue (#13)
- 1x NaCl crypto layer issue (#15 - HSalsa20)
- 1x Header encryption issue (#16)
- 1x Nonce source issue (#17 - cmNonce)
- 1x Envelope length calculation issue (#18 - SMP padding)
- 1x Key management issue (#19 - debug self-decrypt side effects)
- 1x Message type indicator issue (#20 - PrivHeader for HELLO)
- 1x Version field issue (#21 - AgentVersion)
- 1x Hash encoding issue (#22 - prevMsgHash)
- 1x Encryption order issue (#23 - pad before encrypt)
- 1x Key selection issue (#24 - rcv_dh vs snd_dh)
- 1x Maybe field issue (#25 - PubHeader Nothing)
- 1x Format version issue (#26 - v2/v3 encodeLarge)
- 1x Version mismatch issue (#27 - E2E version_min vs RATCHET_VERSION)
- 1x Dynamic parsing issue (#28 - KEM parser for variable header sizes)
- 1x Pointer arithmetic issue (#29 - dynamic emHeader size calculation)
- 1x Key storage/promotion issue (#30 - HKs/NHKs init and promotion chain)
- 1x Try-order issue (#31 - header decrypt sequence for AdvanceRatchet)

 Session 23: CONNECTED with ZERO new bugs! The crypto was already correct!
```

---

## Lessons Learned

1. **Length encoding varies by context** - always check Haskell source
2. **Crypto libraries differ** - verify against reference implementations
3. **Cascade effects are real** - one bug can cause multiple symptoms
4. **A_MESSAGE != A_CRYPTO** - parsing error vs crypto error
5. **Tail means no prefix** - last fields don't need length
6. **Two pad() functions exist** - Lazy.hs (Int64) vs Crypto.hs (Word16)
7. **Wire format != Crypto format** - length prefixes for serialization, not always for AAD
8. **Haskell parser awareness** - `largeP` removes length prefix from parsed object
9. **Python verification essential** - systematically verify all crypto operations
10. **Community support helps** - SimpleX developers are responsive and helpful
11. **NaCl crypto layers** - crypto_box includes HSalsa20, crypto_scalarmult does not
12. **cmNonce != msgId** - Different nonces for different layers
13. **If it works, don't touch it!** - Session 11 regression
14. **Git is your friend** - Commit at working state, reset when needed
15. **Two keypairs exist** - Server DH vs E2E DH are separate!
16. **HSalsa20 matters** - libsodium adds extra step vs Haskell
17. **MAC position matters** - [MAC][Cipher] vs [Cipher][MAC]
18. **Parse SMPConfirmation** - Contains App's e2ePubKey
19. **Verify theories against source code** - Handoff document was WRONG! (Session 14)
20. **crypto_scalarmult vs crypto_box_beforenm** - Use raw DH, not derived key! (Session 14)
21. **Python verification is proof** - DH Secret match proves crypto basis correct! (Session 14)
22. **maybe_e2e = Nothing means pre-computed** - No key in message, use stored secret! (Session 15)
23. **Two key types in protocol** - dh= for SMP, sndQueue.e2ePubKey for E2E (Session 15)
24. **Missing message = missing key** - App's AgentConfirmation has the e2ePubKey! (Session 15) **DISPROVEN S16**
25. **Protocol flow analysis essential** - Must understand full message sequence! (Session 15)
26. **Ask the developer!** - Evgeny's "in the same message" disproved Session 15 theory! (Session 16)
27. **SimpleX uses NON-STANDARD XSalsa20** - HSalsa20(key, zeros[16]) not nonce[0:16]! (Session 16)
28. **Custom crypto may be needed** - simplex_crypto.c for ESP32 (Session 16)
29. **Key race conditions** - Multiple writes to same variable = bugs! (Session 16)
30. **Self-decrypt failure is BY DESIGN** - Asymmetric header keys (Session 16)
31. **Problem can shift between layers** - L4 fixed, L5 broke (Session 16)
32. **Verify all layers before moving on** - Wire-format , AAD , Keys (Session 16)
33. **ALWAYS search past Evgeny conversations first!** - He already answered Jan 28 (Session 17)
34. **Length prefix differs per queue** - Reply Queue has 2-byte prefix, Contact Queue doesn't (Session 17)
35. **cmNonce is RANDOM** - Directly in message, not calculated (Session 17)
36. **ALWAYS use length prefix for content boundaries** - Never assume buffer_size - header = content_size! (Session 18)
37. **SMP block-padding exists** - 0x23 padding for traffic analysis resistance, must be excluded! (Session 18)
38. **corrId is SMP Transport, NOT in ClientMsgEnvelope** - Parsed before envelope, not inside it! (Session 18)
39. **Contact Queue has NO E2E Layer 2** - Only server-level decryption, no separate E2E! (Session 18)
40. **Compare working code with broken code** - Contact Queue parser used prefix_len correctly, Reply Queue didn't! (Session 18)
41. **No comma separators in smpEncode** - Direct concatenation: `smpEncode a <> smpEncode b`! (Session 18)
42. **Wrapper chain matters** - EncRcvMsgBody → ClientRcvMsgBody → ClientMsgEnvelope → ClientMessage! (Session 18)
43. **One line can block weeks of progress** - Bug #18 was ONE LINE: envelope_len = raw_len_prefix! (Session 18)
44. **unPad layer exists between crypto_box and ClientMessage** - [2B len][content][padding 0x23...] (Session 19)
45. **PrivHeader tags: 'K'=PHConfirmation, '_'=PHEmpty** - Check Protocol.hs for encoding! (Session 19)
46. **Maybe encoding is ASCII '0'/'1', NOT binary 0x00/0x01** - Check Encoding.hs! (Session 19)
47. **nhk (HKDF[32-63]) = header_key_recv** - Second block of X3DH HKDF output! (Session 19)
48. **AES-GCM uses 16-byte IV in SimpleX** - Not standard 12-byte! (Session 19)
49. **Save keys immediately after derivation** - Prevents overwrite bugs like #19! (Session 19)
50. **Always account for ALL wrapper layers when parsing** - 0x3a wasn't PrivHeader, it was unPad length! (Session 19)
51. **Analysis first, implementation second** - Don't code until you understand the wire format! (Session 19)
52. **Tests must NEVER modify production state** - Debug self-decrypt corrupted ratchet state! (Session 20)
53. **Understand roles: Initiator='I', Joiner='D'** - ConnInfo tags differ by role in handshake! (Session 20)
54. **Check for Zstd compression** - 'X'=0x58 marker, magic 28 b5 2f fd, '1'=compressed! (Session 20)
55. **DH Ratchet Step = TWO rootKdf calls** - recv chain + send chain, new keypair in between! (Session 20)
56. **iv1 = Body IV, iv2 = Header IV** - During decrypt, header IV comes from ehIV, not chainKdf! (Session 20)
57. **Body AAD = rcAD || emHeader (raw bytes)** - Use exact wire bytes, don't re-serialize! (Session 20)
58. **ESP32 = Accepting Party, App = Joining Party** - Roles determine key/queue usage! (Session 21)
59. **PrivHeader: HELLO=0x00, CONF='K'** - Regular messages have NO PrivHeader, not PHEmpty! (Session 21)
60. **AgentMessage uses agentVersion=1, not v2/v7** - Different from AgentConfirmation! (Session 21)
61. **prevMsgHash must be smpEncoded** - Word16 prefix even when empty: [0x00][0x00]! (Session 21)
62. **DH Keys: rcv_dh for Confirmation, snd_dh for HELLO** - Different keys for different msg types! (Session 21)
63. **PubHeader Nothing = '0' (0x30), not missing** - Standard Maybe encoding, must be present! (Session 21)
64. **NOT_AVAILABLE = AUTH error on App side** - App can't SEND because queue not secured! (Session 21)
65. **KEY Command timing: after Confirmation, before HELLO** - Authorize sender before they can send! (Session 21)
66. **Reply Queues are unsecured** - SEND works without KEY auth! (Session 21)
67. **chatItemNotFoundByContactId = RSYNC Crypto Error** - Not HELLO parsing, but decrypt failure! (Session 21)
68. **RSYNC = Ratchet Sync Event** - Triggered on decrypt failure, not protocol error! (Session 21)
69. **v2/v3 encodeLarge switch at v≥3** - 1-byte → 2-byte prefix, affects header sizes! (Session 21)
70. **Version from E2ERatchetParams, not hardcoded** - Confirmation determines peer's expected format! (Session 21)
71. **Confirmation can work v2, HELLO expected v3** - Version mismatch between message types! (Session 21)
72. **Modern SimpleX (v2 + senderCanSecure) needs NO HELLO** - Use Reply Queue flow instead! (Session 22)
73. **AgentConnInfo on Reply Queue, not HELLO on Contact Queue** - Different protocol flow! (Session 22)
74. **smpReplyQueues in Tag 'D' AgentConnInfoReply** - Innermost layer of AgentConfirmation! (Session 22)
75. **SNTRUP761 for PQ KEM, not Kyber1024** - 1158B pubkey, 1039B ciphertext, 32B secret! (Session 22)
76. **PQ-Graceful-Degradation: KEM Nothing → pure DH** - No error on fallback! (Session 22)
77. **E2E version_min MUST match RATCHET_VERSION** - Mismatch causes format confusion! (Session 22)
78. **KEM Parser must be dynamic** - v3+PQ headers up to 2346 bytes! (Session 22)
79. **emHeader size dynamic based on ehVersion** - Don't hardcode offset calculations! (Session 22)
80. **NHKs must be stored in state at init** - Local variable loses value! (Session 22)
81. **nhk from X3DH = NHKr, not HKr directly** - Promotes to HKr on first AdvanceRatchet! (Session 22)
82. **NHKs→HKs promotion THEN KDF→NHKs** - Two-step promotion, not direct assignment! (Session 22)
83. **Header decrypt try-order: HKr, then NHKr** - Wrong order prevents AdvanceRatchet! (Session 22)
84. **ESP32 = Bob (Accepting), App = Alice (Initiating)** - Clear role names! (Session 23)
85. **Tag 'D' sent BY US, Tag 'I' received FROM App** - We send Reply Queue info, App doesn't! (Session 23)
86. **Legacy Path (PHConfirmation 'K') requires KEY + HELLO** - Not Modern/senderCanSecure Path! (Session 23)
87. **KEY is a RECIPIENT command** - Signed with rcv_private_auth_key, authorizes the SENDER! (Session 23)
88. **TLS timeout during Confirmation processing** - Reply Queue connection drops, must reconnect! (Session 23)
89. **Sequence: KEY BEFORE HELLO** - Can't send HELLO before authorizing the sender! (Session 23)
90. **Reconnect sequence: TLS → SUB → KEY** - Must re-subscribe to queue after reconnect! (Session 23)
91. **Padding: 14832B for ConnInfo, 15840B for HELLO** - Different message types, different sizes! (Session 23)
92. **Session 22's "No HELLO" theory was WRONG** - Legacy Path still requires HELLO exchange! (Session 23)
93. **Assumptions must be verified with logs** - Tag 'D' branch never triggered = wrong assumption! (Session 23)
94. **Complete handshake is 7 steps** - Not 3, not 5, exactly 7 steps for Legacy Path! (Session 23)
95. **CONNECTED requires BOTH HELLOs** - We send on Q_A, App sends on Q_B, then CON! (Session 23)
96. **Session 23 "HELLO on Q_B" was FALSE POSITIVE** - Random 0x48 in ciphertext, no actual HELLO! (Session 24)
97. **msgBody must be ChatMessage JSON** - Raw UTF-8 fails: "error parsing chat message"! (Session 24)
98. **ChatMessage format** - `{"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"..."}}}` (Session 24)
99. **SMP ACK is flow control** - Missing ACK blocks ALL further MSG delivery! (Session 24)
100. **ACK is Recipient Command** - Signed with rcv_private_auth_key, like KEY and SUB! (Session 24)
101. **ACK response can be MSG** - Server immediately delivers next message if pending! (Session 24)
102. **Response multiplexing on subscribed queues** - OK, MSG, END can interleave at any time! (Session 24)
103. **pending_msg buffer needed** - Catch MSG during ACK/SUB, return on next read! (Session 24)
104. **PQ-Kyber in the wild** - App sends emHeaderLen=2346, our graceful degradation works! (Session 24)
105. **Scan-based > Parser-based** - Simple "find OK/MSG" beats complex offset calculation! (Session 24)
106. **Claude Code: NO git access** - Creates chaos, branches, undeclared structs! (Session 24)
107. **Aschenputtel for log analysis** - Byte-for-byte verification, keeps strategy chat clean! (Session 24)
108. **One checkmark = server accepted, not delivered** - App sends but ESP32 doesn't receive! (Session 24)
109. **App may not fully activate connection** - Shows "Connected" but doesn't send to Q_B! (Session 24)
110. **Socket routing bug (late discovery)** - subscribe_all_contacts() SUBs on main ssl, listen reads queue_conn.ssl! (Session 24)
111. **App's own messages are the best protocol reference** - Byte comparison beats source analysis! (Session 25)
112. **Test NULL pointers in extended code paths** - New features may use previously-unused parameters! (Session 25)
113. **Write-Before-Send is architectural, not optional** - Evgeny's pattern prevents state desync bugs (Session 26)
114. **ESP32 is not a smartphone** - NVS + SD card beats SQLite for embedded (Session 26)
115. **SPI bus sharing requires ownership model** - Two-Phase Init is the clean pattern (Session 26)
116. **Validate before you memcpy** - Load to local, validate, then copy to global state (Session 26)
117. **Unified save strategy beats dual codepaths** - save_blob_sync everywhere, 7.5ms acceptable (Session 26)
118. **Test laboratory != product architecture** - Blocking main loop must become multi-task (Session 26)
119. **SD card portability is a product feature** - Swap encrypted SD between devices (Session 26)
120. **Role discipline in multi-agent workflow** - Mausi coordinates, Hasi implements, no crossing (Session 26)
121. **Tasks AFTER connection, never at boot** - ~90KB RAM at boot starves TLS/WiFi (Session 27)
122. **Always baseline-test main before debugging feature branch** - 2 days wasted on wrong branch (Session 27)
123. **Git bisect is your friend** - Would have found breaking commit in minutes (Session 27)
124. **CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384 is mandatory** - Default 4096 deadlocks 16KB writes (Session 27)
125. **CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768 minimum** - Smaller causes TLS fragmentation (Session 27)
126. **Bigger TCP buffer ≠ better** - Larger buffers enable async that breaks request-response (Session 27)
127. **mbedtls_ssl_write + WANT_READ is normal for TLS 1.3** - Don't hack with non-blocking (Session 27)
128. **Socket timeouts affect mbedTLS unpredictably** - SO_SNDTIMEO/SO_RCVTIMEO interact badly (Session 27)
129. **Session Tickets -- leave default (enabled)** - Haskell uses noSessionManager (Session 27)
130. **Check RAM budget after every architecture change** - esp_get_free_heap_size() at key points (Session 27)
131. **Allocate memory pools on demand, not at boot** - Frame pools when needed, not init (Session 27)
132. **Init stays sequential, tasks take over running operation** - Proven since Session 7 (Session 27)
133. **Diff after EVERY task, no rushing ahead** - Prevents cumulative errors (Session 27)
134. **Claude Code -- "analyze only, NO code changes"** - Explicitly state to prevent unwanted PRs (Session 27)
135. **Multi-agent debugging works** - Different AI instances catch each other's errors (Session 27)
136. **Cleanup commands before git add, not after** - Avoid committing build artifacts (Session 27)
137. **Two days in a circle teaches you to measure the circle** - But measure first next time (Session 27)
138. **erase-flash after branch switch -- ALWAYS** - NVS crypto state doesn't match after code changes (Session 28)
139. **Read handoff protocol COMPLETELY** - Lesson 138 was in S27 handoff and was overlooked (Session 28)
140. **ESP32-S3: Everything non-DMA → PSRAM** - Internal SRAM limited to ~40KB after WiFi+TLS (Session 28)
141. **Mutual control catches hallucinations** - Mausi ↔ Hasi cross-review is a reliability mechanism (Session 28)
142. **Royal tone → better collaboration** - Respectful address leads to productive interactions (Session 28)
143. **sdkconfig survives git revert** - Check separately after revert operations (Session 28)
144. **PSRAM stacks + NVS writes = Crash on ESP32-S3** - SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack. CRITICAL! (Session 29)
145. **NOSPLIT Ring Buffers need ~2.3× payload size** - For 16KB frames we needed 37KB buffer, not 20KB (Session 29)
146. **Main Task as App Logic Carrier** - Main Task has largest Internal SRAM stack (64KB), ideal for NVS-writing logic. Don't let it sleep! (Session 29)
147. **Three separate SSL connections** - Main SSL (Network Task), Peer SSL (smp_peer.c), Reply Queue SSL (smp_queue.c). Only main SSL needs task isolation (Session 29)
148. **Read timeout 1000ms instead of 5000ms** - When Network Task services return channel, shorter timeout prevents ACK waiting 5 seconds (Session 29)
149. **SMP Versions: Official spec documents ONLY v6 and v7** - Server reports internal range (e.g. 6-17/18), but third-party clients should use max v7. v8+ is actively rejected by server (Connection Reset). ALPN "smp/1" enables full range; without ALPN only v6 (Session 30)
150. **corrId must be 24 random bytes** - NOT 1 byte as previously implemented. Server accepts both, but protocol spec requires 24 bytes. corrId is reused as NaCL nonce (therefore random and unique) (Session 30)
151. **Drain Loop for Multi-Command responses** - After 42d handshake, responses come in unpredictable order. ACK response can arrive BEFORE SUB response. Drain loop with entity matching (recipientId comparison) solves the problem (Session 30)
152. **Batch Framing is mandatory from v4** - Every block MUST have `[2B contentLen][1B txCount][2B txLen][transmission][padding '#']`. Even for single transmissions: txCount=1. Handshake blocks (ClientHello, ServerHello) are the ONLY exception (Session 30)

---

## Session 23: CONNECTED with ZERO New Bugs!

Session 23 achieved the historic milestone of **CONNECTED** status without introducing
any new bugs. All 31 existing bugs were already fixed, and the complete 7-step
handshake was successfully implemented.

### The Complete 7-Step Handshake (Verified Working)

```
Step Queue Direction Content

1. -- App NEW → Q_A, creates Invitation
2a. Q_A ESP32→App SKEY (Register Sender Auth)
2b. Q_A ESP32→App CONF Tag 'D' (Q_B + Profile)
3. -- App processConf → CONF Event
4. -- App LET/Accept Confirmation
5a. Q_A App KEY on Q_A (senderKey)
5b. Q_B App→ESP32 SKEY on Q_B
5c. Q_B App→ESP32 Tag 'I' (App Profile)
6a. Q_B ESP32 Reconnect + SUB + KEY
6b. Q_A ESP32→App HELLO
6c. Q_B App→ESP32 HELLO
7. -- Both CON -- "CONNECTED"
```

### Key Corrections from Session 22

- **Session 22 assumed:** "Modern SimpleX needs no HELLO, App sends Reply Queue in Tag 'D'"
- **Session 23 discovered:** App sends Tag 'I' (no Queue info), WE send Tag 'D', Legacy Path needs HELLO

---

## Session 24: First Chat Message -- MILESTONE #2!

Session 24 achieved the second historic milestone: **First chat message from a microcontroller!**

"Hello from ESP32!" displayed in SimpleX App.

### Key Discoveries

1. **msgBody format:** Must be ChatMessage JSON, not raw UTF-8
2. **Session 23 correction:** "HELLO on Q_B" was false positive (was Tag 'I' ConnInfo)
3. **ACK protocol:** Critical flow control, missing ACK blocks all delivery
4. **PQ-Kyber:** App sends PQ headers, our graceful degradation works
5. **Bidirectional blocked:** App doesn't send to Q_B despite "Connected" status

### Open Bug → ROOT CAUSE FOUND (Late in Session)

```
Initially believed: Format error in our messages causes silent discard.

Late discovery at session end:
 subscribe_all_contacts() subscribes Reply Queue on main ssl connection.
 Listen-Loop reads from queue_conn.ssl (separate connection).
 Messages delivered to wrong socket!

Fix for Session 25:
 A) Process Q_B messages in Main Receive Loop
 B) Don't SUB Q_B in subscribe_all_contacts()
```

---

## Session 25: Bidirectional Chat + Receipts -- MILESTONES 3, 4, 5!

Session 25 achieved **THREE milestones** in the Valentine's Day session:
- Milestone 3: First App message decrypted on ESP32
- Milestone 4: Bidirectional encrypted chat
- Milestone 5: Delivery receipts ()

### Bugs Fixed (8 total)

| # | Bug | Severity | Fix |
|---|-----|----------|-----|
| S25-1 | Heap Overflow PQ Headers | Critical | `malloc(eh_body_len + 16)` |
| S25-2 | txCount Hardcoded | Critical | Read as counter, don't validate |
| S25-3 | Nonce Offset Wrong | Critical | Brute-force found offset=13 |
| S25-4 | Ratchet State Copy | Critical | Use pointer `*rs` not copy |
| S25-5 | Chain KDF Skip Relative | Critical | `skip_from = msg_num_recv` |
| S25-6 | Receipt count=Word16 | High | Change to Word8 |
| S25-7 | Receipt rcptInfo=Word32 | High | Change to Word16 |
| S25-8 | NULL contact Reply Queue | High | NULL guard |

### Key Discovery: Nonce Offset 13

```
Session 24 believed: Byte [12] = corrId tag '0' → use cache
Session 25 discovered: Byte [12] = first nonce byte!

Regular Q_B messages: [12B header][nonce@13][ciphertext]
Parser was reading at wrong offset (14 instead of 13).

Brute-force scan found the truth:
 DECRYPT OK at nonce_offset=13!
```

### Key Discovery: Ratchet State Persistence

```
// WRONG -- works on copy, changes lost:
ratchet_state_t rs = *ratchet_get_state();

// CORRECT -- works on pointer, changes persist:
ratchet_state_t *rs = ratchet_get_state();
```

### Key Discovery: Receipt Wire Format

```
Our receipt: 90 bytes (WRONG)
App receipt: 87 bytes (CORRECT)
Difference: 3 bytes

Errors:
 - count: Word16 → Word8 (−1 byte)
 - rcptInfo: Word32 → Word16 (−2 bytes)
```

---

## Session 26: Persistence & Storage -- MILESTONE 6!

Session 26 achieved **Milestone 6** (Ratchet State Persistence) in the Valentine's Day Part 2 session:
- ESP32 survives reboot without losing cryptographic state
- Write-Before-Send pattern (Evgeny's golden rule) implemented
- NVS partition expanded to 128KB (150+ contacts)

### No New Protocol Bugs

Session 26 was a feature session (persistence), not protocol debugging. No new protocol bugs discovered.

### Key Achievement: Ratchet State Persistence

```
Before Session 26: Reboot = complete amnesia
After Session 26: ESP32 remembers ratchet state, queue credentials, peer info

Test result:
 - Rebooted ESP32
 - App sent message "Test Two"
 - ESP32 decrypted message without new handshake
 - Delivery receipt () worked!
```

### Storage Architecture

```
NVS (Internal Flash) -- 128KB partition
 rat_XX Ratchet State (520 bytes per contact)
 queue_our Queue credentials
 cont_XX Contact credentials
 peer_XX Peer connection state

SD Card (External) -- Optional, for message history
```

### Key Metrics

| Metric | Value |
|--------|-------|
| NVS write timing | 7.5ms (verified) |
| Contacts supported | 150+ |
| Ratchet state size | 520 bytes |

---

## Session 27: FreeRTOS Architecture Investigation

Session 27 attempted the FreeRTOS multi-task architecture transformation. While the architecture design is correct, the implementation reserved ~90KB RAM at boot and broke TLS/WiFi.

### No New Protocol Bugs

Session 27 was an architecture session. No new protocol bugs, but 17 lessons learned about embedded systems development.

### Root Cause

```
Phase 2 commit reserved ~90KB RAM at boot:
 Network Task Stack: 16KB
 App Task Stack: 32KB
 UI Task Stack: 10KB
 Frame Pool: 32KB
 Ring Buffers: 12KB

 Total: ~90KB

This starved smp_connect() of memory for TLS/WiFi.
Solution: Start tasks AFTER connection, not at boot.
```

### sdkconfig Fixes Found

```ini
# Mandatory for 16KB SMP blocks:
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384

# Minimum for TLS records > 4096:
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768
```

### Key Insight

**Always baseline-test main before debugging feature branch.** 2 days were spent debugging Phase 3 when Phase 2 had broken main.

---

## Session 28: FreeRTOS Task Architecture -- Phase 2b Success!

Session 28 successfully implemented the FreeRTOS multi-task architecture after learning from Session 27's failure. The key was moving all non-DMA resources to PSRAM.

### No New Protocol Bugs

Session 28 was an architecture session. No new protocol bugs, but 6 critical lessons about embedded development.

### Key Achievement: Three Tasks Running

```
Task Architecture (Phase 2b):
 Network Task (Core 0, 12KB stack, Priority 7)
 App Task (Core 1, 16KB stack, Priority 6)
 UI Task (Core 1, 8KB stack, Priority 5)

PSRAM Allocation:
 Frame Pool: 16KB → PSRAM (heap_caps_calloc)
 Ring Buffers: 3KB → PSRAM (xRingbufferCreateWithCaps)
 Task Stacks: 36KB → PSRAM (already was)

Result: Internal Heap ~40KB preserved for mbedTLS/WiFi
```

### Critical Lesson: erase-flash

```powershell
# After EVERY branch switch or sdkconfig change:
idf.py erase-flash -p COM6

# Then create new contact in app
```

NVS stores crypto state (ratchet, queues, contacts) that doesn't match after code changes.

---

## Session 29: Multi-Task Architecture -- BREAKTHROUGH!

Session 29 successfully implemented the multi-task architecture for SimpleGo. The complete encrypted messaging pipeline now runs over FreeRTOS Tasks with cross-core Ring Buffer IPC.

### No New Protocol Bugs

Session 29 was an architecture session. No new protocol bugs, but 5 critical lessons about ESP32-S3 hardware.

### Critical Discovery: PSRAM + NVS

```
ESP32-S3: Tasks with PSRAM stack must NEVER write to NVS!

Root Cause:
 - SPI Flash write disables cache
 - PSRAM is cache-based (SPI bus, mapped in cache)
 - Task loses access to its own stack during Flash write
 - Immediate crash!

Solution:
 - App logic runs in Main Task (64KB Internal SRAM)
 - Network Task (PSRAM) only does SSL reads (no NVS)
 - UI Task (PSRAM) empty for now (no NVS)
```

### Architecture After Session 29

```
Network Task (Core 0, 12KB PSRAM):
 → SSL read loop → Ring Buffer → Main Task

Main Task (64KB Internal SRAM):
 → Parse → Decrypt → NVS → 42d handshake

Ring Buffer IPC:
 → net_to_app: 37KB (frames)
 → app_to_net: 1KB (commands)
```

---

## Session 30: Intensive Debug Session -- 10 Hypotheses, 14 Fixes

Session 30 was the most intensive debug session of the project. T5 (Keyboard-Send) was successfully completed. T6 (Bidirectional baseline test) revealed a deep receive problem that could not be solved despite 10 systematic fix attempts.

### No New Protocol Bugs (But Major Discovery)

Session 30 was a debug session. The App→ESP32 receive problem remains unresolved, but 10 hypotheses were systematically excluded and an expert question was sent to Evgeny Poberezkin.

### T5: Keyboard-Send Integration PASSED

```c
// smp_tasks.c - Non-blocking keyboard poll
if (kbd_queue != NULL) {
 kbd_msg_t kbd_msg;
 if (xQueueReceive(kbd_queue, &kbd_msg, 0) == pdTRUE) {
 peer_send_chat_message(kbd_msg.text);
 }
}
```

### Excluded Hypotheses (10 total)

| # | Hypothesis | Evidence for Exclusion |
|---|------------|------------------------|
| 1 | corrId wrong (1 byte instead of 24) | 24 bytes, server says OK, no MSG |
| 2 | Batch framing missing or wrong | `[contentLen][txCount][txLen]` correct |
| 3 | Subscribe failed | `ent_match=1`, Command OK confirmed |
| 4 | Delivery blocked (delivered=Just) | Wildcard ACK → `ERR NO_MSG` |
| 5 | Network Task hangs or crashes | Heartbeats every ~30s, task lives |
| 6 | SSL connection broken | RECV logs show active connection |
| 7 | SMP version v6 incompatible | v7 upgrade, server accepts, problem remains |
| 8 | SessionId in wire disturbs server | Removed, 118 bytes, server happy |
| 9 | Response parser offset (after v7) | sessLen removed from 6 parsers |
| 10 | ACK chain interrupted | Everything that arrives gets ACKed |

### SMP v6 → v7 Upgrade

```
v6 SUB Transmission: 151 bytes
v7 SUB Transmission: 118 bytes (33 bytes saved - SessionId removed from wire)
```

### Expert Question to Evgeny (Feb 18, 2026)

> "Is there a condition where the server would accept a SUB (respond OK) but then not deliver incoming MSGs to that subscription?"

---

## Session 31 -- Root Cause Found! (2026-02-18)

### T6 RESOLVED: txCount==1 Filter in Drain-Loop

**Root Cause:** The Drain-Loop in `subscribe_all_contacts()` had `if (rq_resp[rrp] == 1)` which discarded all batched server responses with txCount > 1. The server batched SUB OK + pending MSG with txCount=2, and the MSG in TX2 was silently dropped.

**The Fix:**
```c
// BEFORE (broken):
if (rq_resp[rrp] == 1) { // Only accept txCount == 1

// AFTER (fixed):
if (rq_resp[rrp] >= 1) { // Accept txCount >= 1
```

**One character change: `==` → `>=`**

### Six Fixes Applied

| # | Fix | File | Description |
|---|-----|------|-------------|
| 1 | TCP Keep-Alive | smp_network.c | keepIdle=30, keepIntvl=15, keepCnt=4 |
| 2 | SMP PING/PONG | smp_tasks.c | 30s interval, connection health |
| 3 | Reply Queue SUB | smp_contacts.c | Explicit SUB on sock 54 (main socket) |
| 4 | txCount >= 1 | smp_contacts.c | ROOT CAUSE! Accept batched responses |
| 5 | TX2 Forwarding | smp_contacts.c | Forward MSG from batch to App Task |
| 6 | Re-Delivery | smp_ratchet.c | msg_ns < recv → ACK only, no decrypt |

### Evgeny's Response (Key Insights)

> "Subscription can only exist in one socket though."
> "if you subscribe from another socket, the first would receive END"
> "concurrency is hard."

### SMP Batch Format (Definitive Reference)

```
[2B content_length] ← Big Endian
[1B txCount] ← Number of transmissions (can be > 1!)
[2B tx1_length] ← Large-encoded length of TX1
[tx1_data] ← First transmission
[2B tx2_length] ← Large-encoded length of TX2 (if txCount > 1)
[tx2_data] ← Second transmission
[padding '#' to 16384] ← Pad to SMP_BLOCK_SIZE
```

`batch = True` is hardcoded in Transport.hs since v4. Third-party clients MUST handle txCount > 1.

### New Lessons Learned (Session 31)

153. **PING not required for subscription survival** - Server does NOT drop subscriptions due to missing PING/PONG. Only after 6h without ANY subscription. PING is for connection health and NAT refresh (Session 31)
154. **TCP Keep-Alive is for NAT, not subscription** - keepIdle/keepIntvl/keepCnt prevents NAT table expiry. Does not affect SMP subscription state (Session 31)
155. **Reply Queue needs explicit SUB on main socket** - After 42d handshake creates Reply Queue on sock 55, must re-SUB on sock 54 after sock 55 closes (Session 31)
156. **"Different Server" is a recurring dead end** - Disproven in S24, repeated in S30 and S31. NEVER revisit without concrete new evidence (Session 31)
157. **txCount > 1 is normal SMP batch behavior** - `batch = True` hardcoded in Transport.hs since v4. Parsers MUST handle txCount > 1, especially SUB OK + pending MSG (Session 31)
158. **Drain-Loops can discard MSG frames** - A Drain-Loop filtering for expected responses must not silently drop unexpected frames. Forward or buffer, never discard (Session 31)
159. **Re-delivery is normal after reconnect/re-subscribe** - Server re-delivers last unACKed MSG. Client detects: msg_ns < recv → ACK only, no decrypt (Session 31)
160. **Hasi reports to Mausi, no independent theories** - Implementation agent executes assigned tasks and reports results. Observations go to strategy agent first (Session 31)
161. **RAW hex dump before parsing reveals truth** - NT_RAW hex dump is the definitive diagnostic. MSG in RAW → parsing problem. No MSG in RAW → server/network problem (Session 31)

---

*Bug Tracker v26.0*
*Last updated: February 18, 2026 - Session 31*
*Total bugs documented: 39 (all FIXED)*
*161 lessons learned!*
* Session 31: Bidirectional Chat Restored! Milestone 7!*

---

## Session 32 -- "The Demonstration" (2026-02-19/20)

### From Protocol to Messenger -- Full UI Integration

**No new protocol bugs.** Session 32 was pure UI/architecture work: Keyboard-to-Chat integration (7 steps), delivery status system, LVGL display fix, multi-contact analysis, navigation stack fix, 128-contact planning.

### New Lessons Learned (Session 32)

162. **FreeRTOS Queue as thread-safe UI bridge** - LVGL functions may only be called from LVGL thread. Queue + Timer pattern (50ms poll) is the clean cross-task UI update solution. Never call lv_obj_create() from protocol task (Session 32)
163. **LVGL does not auto-invalidate flex containers in timer callbacks** - Must manually call `lv_obj_update_layout()` + `lv_obj_invalidate()` after adding children from timer callback. Without this, new elements invisible until screen switch (Session 32)
164. **Receipt wire format: 'V' + count(1B) + [msg_id(8B BE) + hash_len(1B) + hash(NB)]** - Reverse-engineered from Ratchet body bytes. count=Word8, msg_id=8B Big Endian, hash typically 32B SHA256. Maps to sent msg_id for delivery status (Session 32)
165. **Encapsulate msg_id access via function, not extern** - `handshake_get_last_msg_id()` cleaner than `extern uint64_t msg_id_counter`. Prevents accidental modification, explicit dependency (Session 32)
166. **Receive path already multi-contact capable** - `find_contact_by_recipient_id()` correctly routes to right contact_t. Only send path (7 locations referencing contacts[0]) needs modification (Session 32)
167. **Navigation Stack instead of prev_screen** - Single prev_screen causes infinite ping-pong in three-level navigation. 8-deep stack with push on navigate, pop on back. Splash never pushed (Session 32)
168. **PSRAM Ratchet Array eliminates swap latency** - `ratchet_state_t ratchets[128]` uses ~68KB (0.8% of 8MB PSRAM). Zero latency on contact switch vs 5-20ms NVS load per swap. NVS only at boot + after each message (Session 32)

---

*Bug Tracker v27.0*
*Last updated: February 20, 2026 - Session 32*
*Total bugs documented: 39 (all FIXED)*
*168 lessons learned!*
* Session 32: "The Demonstration" -- From Protocol to Messenger*

---

## Session 34 -- Multi-Contact Architecture (2026-02-23)

### From Singleton to Per-Contact Reply Queue

**8 commits in one session.** Production cleanup (-200+ lines), runtime add-contact, per-contact 42d tracking (bitmap), UI contact list, per-contact reply queue (128 PSRAM slots), SMP v7 signing fix. KEY command bug open, handed to Claude Code.

### SMP v7 Signing Fix

```
WRONG: [2B corrLen][corrId][2B entLen][entityId][command]
RIGHT: [1B corrLen][corrId][1B entLen][entityId][command]

2-byte Large-encoded prefix in signing buffer causes signature
verification failure. Affected SUB, KEY, NEW simultaneously.
Fixed: all three use 1-byte prefix now.
```

### PSRAM Architecture (128 contacts)

```
Ratchet States: 66,560 B (128 slots)
Handshake States: 7,296 B (128 slots)
Contacts DB: 35,200 B (128 slots)
Reply Queue Array: 49,152 B (128 slots)

Total: ~158 KB / 8 MB (1.9%)
```

### New Lessons Learned (Session 34)

169. **Strip ALL private keys from logs before production** - Private key hex dumps are a security risk. Replace 32-byte dumps with 4-byte fingerprints or move to ESP_LOGD (disabled in release). Includes DH private, chain keys, message keys, cleartext (Session 34)
170. **Runtime add-contact uses Ring Buffer command pattern** - UI triggers intent, Main Task packages NET_CMD_ADD_CONTACT, Ring Buffer delivers to Network Task with SSL connection. Same NET_CMD_* pattern for any network-requiring operation (Session 34)
171. **Per-contact 42d tracking with 128-bit bitmap** - `uint32_t[4]` bitmap uses 16 bytes for 128 contacts. O(1) set/check via `is_42d_done(idx)` and `mark_42d_done(idx)` inline functions. Replaces boolean singleton (Session 34)
172. **SMP v7 signing uses 1-byte session-length prefix** - Signed payload concatenates corrId + entityId + command. Length prefixes MUST be 1-byte (not 2-byte Large). 2-byte causes signature verification failure on server. Affected SUB, KEY, NEW (Session 34)
173. **Per-contact reply queue array in PSRAM (~49KB for 128 slots)** - Each reply queue ~384 bytes (IDs, keys, flags, server host). NVS persistence via rq_00 through rq_127. Combined PSRAM total: ~158KB (1.9% of 8MB) (Session 34)
174. **KEY command requires line-by-line Haskell comparison** - When server rejects command without clear error, byte-level comparison with Haskell reference is the only reliable approach. Evgeny: "100x reading to writing" ratio (Session 34)
175. **Stack size must account for buffer allocations in called functions** - reply_queue_create() allocates large TLS buffers on stack. Network Task increased 20KB → 32KB. Always check deepest call path for stack usage (Session 34)

---

*Bug Tracker v28.0*
*Last updated: February 23, 2026 - Session 34*
*Total bugs documented: 39 (all FIXED)*
*175 lessons learned!*
* Session 34: Multi-Contact Architecture -- KEY Command open*

---

## Session 34 Day 2 -- Multi-Contact Bidirectional BREAKTHROUGH (2026-02-24)

### HISTORIC MILESTONE: Two Contacts Bidirectional Encrypted on ESP32

**11 bugs found and fixed across 6 phases.** All bugs followed ONE pattern: global/hardcoded state instead of per-contact routing. KEY Command fixed, Ghost Write eliminated, global state removed, encoder corrected, index routing fixed, crypto corrected. Result: Contact 0 + Contact 1 both bidirectional encrypted.

### Bug #40: Wrong Queue Credentials for KEY (CRITICAL)

```
WRONG: KEY -> Reply Queue rcvId, signed with Reply Queue rcvPrivateKey
RIGHT: KEY -> Contact Queue recipientId, signed with Contact Queue rcv_auth_secret

Server checks signature against addressed queue's recipient keys.
Reply Queue credentials on Contact Queue = complete mismatch = ERR AUTH.
```

### Bug #41: Missing SPKI Length Prefix in KEY

```
WRONG: "KEY " + [44 bytes SPKI]
RIGHT: "KEY " + [0x2C] + [44 bytes SPKI]

0x2C = 44 decimal = smpEncode length prefix for SPKI blob.
```

### Bug #42: Ghost Write in reply_queue_create() (5 ERRORS)

```
reply_queue_create() bypassed smp_write_command_block():
 1. Missing txCount(1B), txLen(2B), sigLen(1B) - server reads garbage
 2. Zero-padding instead of '#'-padding
 3. Direct mbedtls_ssl_read without loop (partial read = stream desync)
 4. 16KB stack buffer uint8_t padded[SMP_BLOCK_SIZE] (stack overflow)
 5. Missing SPKI length prefixes before auth/DH keys

Result: ERR BLOCK on entire connection (sock 54).
Fix: Complete rewrite using smp_write_command_block() + smp_read_block().
```

### Bug #43: IDS Response Parsing After Ghost Write Fix

```
After switching to smp_read_block(), block contains txCount/txLen header.
Structured parser read txCount as corrLen, landed in padding.
Fix: Linear scan like in add_contact().
```

### Bug #44: NVS PSRAM Crash

```
reply_queue_save() called nvs_set_blob() from Network Task.
NVS writes SPI flash, disables cache.
Network Task stack in PSRAM: assert failed: esp_task_stack_is_sane_cache_disabled
Fix: NVS save deferred to non-PSRAM context.
```

### Bug #45: Global pending_peer Overwritten

```
Global pending_peer in smp_peer.c overwritten after Contact 1 CONFIRMATION.
All send functions (A_MSG, HELLO, A_RCVD) used Contact 1's server/queue.
Contact 0 gets ERR AUTH after Contact 1 creation.
Fix: peer_prepare_for_contact() loads per-contact state from NVS before each send.
```

### Bug #46: Frame Loss During Subscribe (4x DISCARD)

```
subscribe_all_contacts() had 4 drain loops discarding MSG frames
with "wrong" entity ID. Reply Queue messages silently lost:
ConnInfo, receipts, chat messages all dropped.
Fix: 4x DISCARD -> FORWARD via Ring Buffer to App Task.
```

### Bug #47: SMPQueueInfo Encoder 3 Byte Errors

```
reply_queue_encode_info() output: 132 bytes (expected 134)
 1. Version: 1B instead of 2B Big-Endian (04 vs 00 04)
 2. Host Count: missing 0x01 byte before host_len
 3. DH Key Prefix: missing 0x2C (44) before SPKI
Phone could not parse SMPQueueInfo, never responded.
Fix: Encoder byte-identical to queue_encode_info().
```

### Bug #48: peer_contact_idx Always 0

```
Pointer arithmetic: contact - contacts_db.contacts
returned 0 for Contact 1 (struct size division unreliable).
Contact 1 CONFIRMATION contained Contact 0's queue data.
Fix: Explicit contact_idx parameter.
```

### Bug #49: NVS Key Hardcoded peer_00

```
All contacts used "peer_00" NVS key instead of "peer_XX".
Fix: Dynamic peer_%02x format with correct index.
```

### Bug #50: crypto_scalarmult vs crypto_box_beforenm

```
WRONG: crypto_scalarmult(shared_secret, our_private, server_public)
 = raw X25519 DH output (32 bytes)

RIGHT: crypto_box_beforenm(shared_secret, server_public, our_private)
 = scalarmult + HSalsa20 key derivation

crypto_box_open_easy_afternm() expects beforenm output.
Using scalarmult = decrypt failure (ret=-2).
One line. Critical difference.
```

### Architecture Discovery: The ONE Pattern

```
All 11 bugs followed one pattern:
 WRONG: global/hardcoded state (slot 0, our_queue, pending_peer, peer_00)
 RIGHT: per-contact state (contacts[idx], RQ[idx], peer_%02x)

No new algorithms. No protocol changes. Only consistent index routing.
Pattern established for contacts 2-127.
```

### New Lessons Learned (Session 34 Day 2)

176. **KEY uses Contact Queue credentials, NOT Reply Queue** - EntityId = Contact Queue recipientId, signing key = rcv_auth_secret. Reply Queue credentials on Contact Queue = ERR AUTH (Session 34b)
177. **smpEncode of SPKI requires 1-byte length prefix (0x2C = 44)** - KEY body: "KEY " + [0x2C] + [44B SPKI]. Missing prefix = server misparse (Session 34b)
178. **Ghost writes bypassing standard write path cause ERR BLOCK** - Any direct mbedtls_ssl_write without smp_write_command_block() sends malformed transmission. Diagnostic: ERR BLOCK before first instrumented write = uninstrumented write path (Session 34b)
179. **Five errors in one function: always copy from working reference** - reply_queue_create() had wrong wire format, wrong padding, no read loop, stack bomb, missing prefixes. Standalone prototype diverges on every aspect. Copy from add_contact() (Session 34b)
180. **Global pending_peer must be per-contact** - A global peer struct gets overwritten on second contact creation. peer_prepare_for_contact() loads correct NVS state before each send (Session 34b)
181. **DISCARD frames in subscribe loops must be FORWARDED** - Drain loops discarding non-matching entity ID frames silently lose Reply Queue messages. Forward via Ring Buffer instead (Session 34b)
182. **SMPQueueInfo encoder must be byte-identical to reference** - Three byte errors (version 1B vs 2B BE, missing host_count, missing DH prefix) = 132B vs 134B output. Phone cannot parse, never responds (Session 34b)
183. **Pointer arithmetic for contact index is unreliable** - `contact - array_base` with struct pointers may return wrong index. Use explicit parameter instead (Session 34b)
184. **crypto_box_beforenm vs crypto_scalarmult: HSalsa20 derivation** - scalarmult = raw DH. beforenm = scalarmult + HSalsa20. afternm expects beforenm output. One function, complete crypto failure (Session 34b)
185. **If instrumentation finds nothing, there is an uninstrumented code path** - When diagnostic logs show no evidence, the problem is not in instrumented code. Enumerate ALL code paths that could cause the symptom (Session 34b)
186. **All multi-contact bugs follow ONE pattern: global -> per-contact** - Every bug (11 total) was global/hardcoded state instead of per-contact routing. Pattern established for contacts 2-127 (Session 34b)

---

*Bug Tracker v29.0*
*Last updated: February 24, 2026 - Session 34 Day 2*
*Total bugs documented: 50 (all FIXED)*
*186 lessons learned!*
* Session 34 Day 2: Multi-Contact Bidirectional Encrypted Messaging -- HISTORIC MILESTONE*

---

## Session 35 -- Multi-Contact Victory (2026-02-24)

### All Planned Bugs Fixed -- Chat Filter Working

**6 fixes (35a-35h) across 10 files, 1 commit.** Ratchet slot ordering, KEY target queue, per-contact chat filter, PSRAM guard, NVS fallback for Contact >0. Verified with erase-flash and 20+ messages across 2 contacts.

### Root Cause Pattern: "Wrong Slot Active"

```
Every fix in Session 35 followed the Session 34 pattern:
 - Decrypt with wrong ratchet slot = crypto failure
 - KEY on wrong queue = connection failure
 - NVS load overwrites active PSRAM = data corruption

Rule: EVERY operation touching ratchet/handshake state
 MUST call set_active(contact_idx) FIRST.
```

### Fix Details

```
35a: ratchet_set_active() + handshake_set_active() BEFORE smp_agent_process_message()
 (was called AFTER, causing Contact 1 decrypt with Slot 0 keys)

35c: KEY entityId = reply_queue_get(idx)->rcv_id (Reply Queue)
 (was contacts[idx].recipient_id = Contact Queue, phone stuck on "connecting")

35e: Per-contact chat filter via lv_obj_set_user_data(bubble, contact_idx)
 + LV_OBJ_FLAG_HIDDEN for non-matching contacts on switch

35f: PSRAM guard in reply_queue: check if slot already valid before NVS load
 + deferred NVS save to prevent overwrite of active session

35g: ratchet_set_active() before Contact Queue decrypt path
 (was missing, CQ decrypt used whatever slot was active)

35h: NVS fallback in ratchet_set_active() and handshake_set_active():
 if PSRAM slot empty (zeroed), load from NVS first
 (after boot, only Slot 0 loaded; Slot 1+ were empty)
```

### New Lessons Learned (Session 35)

187. **"Wrong slot active" is the most common multi-contact bug** - Every operation touching ratchet/handshake state MUST call set_active(contact_idx) first. Decrypt with wrong slot = crypto failure, KEY on wrong queue = connection failure (Session 35)
188. **PSRAM slots do not survive reboot for Contact >0** - After boot, only Slot 0 loaded from NVS. ratchet_set_active(N) for N>0 must include NVS fallback: if PSRAM empty, load from NVS first (Session 35)
189. **KEY secures the Reply Queue, not the Contact Queue** - Phone sends HELLO/chat/receipts to Reply Queue. KEY entityId must be reply_queue_get(idx)->rcv_id. Wrong target = phone stuck on "connecting" (Session 35)
190. **Per-contact chat filter via LVGL user_data + HIDDEN flag** - Each bubble stores contact_idx via lv_obj_set_user_data(). On contact switch, iterate children and set LV_OBJ_FLAG_HIDDEN for non-matching. Avoids rebuilding entire chat view (Session 35)
191. **PSRAM guard prevents NVS overwrite of active session** - Check if PSRAM slot already valid before loading from NVS at boot. Deferred NVS save ensures PSRAM data persists without overwriting current session state (Session 35)
192. **Systematic file-by-file comparison beats log analysis** - Comparing Contact 0 (working) vs Contact 1 (broken) through file-by-file tracing finds root causes faster. Identify where code paths diverge for different contact indices (Session 35)

---

*Bug Tracker v30.0*
*Last updated: February 24, 2026 - Session 35*
*Total bugs documented: 50 (all FIXED)*
*192 lessons learned!*
* Session 35: Multi-Contact Victory -- All Planned Bugs Fixed*

---

## Session 36 -- Contact Lifecycle (2026-02-25)

### Contact Lifecycle: Delete, Recreate, Zero Compromise

**7 bugs found and fixed across 4 sub-sessions (36a-36d).** Complete contact lifecycle implemented: Create → Chat → Delete → Recreate without erase-flash. NTP timestamps, contact names from ConnInfo JSON, 4-key NVS cleanup, KEY-HELLO race condition fix, and UI cleanup on delete.

### Bug E: Contact Name Shows Placeholder

```
handle_conninfo() in smp_agent.c decompressed JSON but never extracted displayName.

JSON: {"v":"1-16","event":"x.info","params":{"profile":{"displayName":"Name","fullName":""}}}

Fix: strstr() for "displayName":"" in decompressed JSON → contact_t → NVS → UI header.
Fallback path for uncompressed JSON also covered.
```

### Bug #51: Orphaned NVS Keys on Delete (CRITICAL)

```
Contact delete only erased rat_XX (ratchet state).
Three more key families remained as orphans:

 rat_XX = Root Key, Chain Keys, Header Keys (decrypt past + future messages)
 peer_XX = Queue IDs, DH Keys, Server Host (identity theft)
 hand_XX = X3DH Handshake Keys (foundation of encryption)
 rq_XX = Reply Queue Auth Private Key (send as user)

Mausi found all 4 by reading source code (smp_peer.c:1131).
Hasi had only checked current NVS contents -- missing 3 families.
Fix: 4-key cleanup loop in remove_contact() AND on_popup_delete().
```

### Bug #52: NVS Key Format Mismatch

```
cnt_%02x uses hex format, rat_%02u uses decimal format.
Identical for indices 0-9, diverges from 10 onwards:
 Index 10: cnt_0a vs rat_10 ← MISMATCH!
 Index 15: cnt_0f vs rat_15 ← MISMATCH!

Fix: All NVS key formatting unified to %02x (hex).
```

### Bug #53: Parser Double-Underscore Separator

```
SimpleX changed agent message type separator from _ to __.
Parser splitting on first _ broke message type detection for INVITATION and others.

Fix: Find last underscore in agent message type string.
```

### Bug #54: KEY-HELLO Race Condition (CRITICAL)

```
Timeline from log:
 (245267) APP: 42d -- SEND_KEY queued (slot=0)
 (245767) peer_send_hello starts
 (246797) HELLO sent via Peer (sock 56) <- HELLO FIRST!
 (247197) NET Task executes KEY now (sock 54) <- KEY AFTER!

Root Cause:
 App Task queued KEY to Net Task (Ring Buffer), waited 500ms blind,
 then fired HELLO over separate Peer connection. Two sockets, no sync.

Why phone stays "connecting":
 1. Phone receives HELLO → wants to respond on Reply Queue
 2. KEY not arrived yet → Server has no phone auth key
 3. Server rejects with ERR AUTH
 4. Phone stuck on "connecting"

Fix: FreeRTOS TaskNotification
 - s_app_task_handle + NOTIFY_KEY_DONE define
 - Net Task: xTaskNotify() after KEY OK/Fail/Timeout (all 3 paths!)
 - App Task: xTaskNotifyWait(5000ms) instead of vTaskDelay(500ms)
 - No deadlock: all paths notify

Evgeny quote confirming: "concurrency is hard."
```

### Bug #55: Chat Bubbles Survive Contact Delete

```
LVGL bubble objects persist after contact deletion.
Switching back to chat screen shows dead contact's messages.

Fix: ui_chat_clear_contact(int idx) iterates all bubble children,
checks lv_obj_get_user_data() == idx, deletes matching objects.
```

### Bug #56: QR Code Flashes After Delete

```
QR code widget caches last rendered content.
After deleting contact, QR screen briefly shows old invitation.

Fix: ui_connect_reset() hides QR, shows placeholder, sets "Generating...".
```

### Bug #57: Stale QR on "+ New" Contact

```
Pressing "+ New Contact" shows previous invitation QR before new one generates.
User might scan old (invalid) QR code.

Fix: ui_connect_reset() called in on_bar_new() BEFORE smp_request_add_contact().
```

### Security Analysis: NVS Key Exposure

```
NVS is currently NOT encrypted:
 nvs_flash_init() instead of nvs_flash_secure_init()
 All crypto keys stored in PLAINTEXT in flash.

Impact of each key family if extracted:
 rat_XX → Decrypt past + future messages (Root Key, Chain Keys, Header Keys)
 peer_XX → Identity theft (Queue IDs, DH Keys, Server Host)
 hand_XX → Foundation of encryption (X3DH Handshake Keys)
 rq_XX → Send messages as user (Reply Queue Auth Private Key)

Mitigation: Proper deletion on contact delete (Bug #51 fix).
TODO: NVS Encryption (nvs_flash_secure_init + eFuse keys) for production.
Principle: What is not there cannot be stolen. Deletion > trusting encryption.
```

### Additional Non-Bug Fixes (Session 36)

```
36-prep: UART baudrate 115200 → 921600 (8x speedup for 5000+ line logs)
36-flow: Auto-QR and auto-contact on fresh start removed
36-perf: Handshake delays reduced 6.5s → 2s
36-ui: Contact list rewritten (665 lines, long-press menu)
36-log: Heartbeat 5min (was 30s), hex dumps removed
36-fix: 42d bitmap reset on delete: smp_clear_42d(idx)
36-fix: LIST_H macro collision with FreeRTOS → renamed CLIST_H
```

### New Lessons Learned (Session 36)

193. **Always search source code for where NVS keys are written, not current NVS contents** - Mausi read smp_peer.c:1131 and found 4 key families. Hasi only checked NVS dump and found 1. 100x Reading Principle applies to code, not runtime state (Session 36a)
194. **NVS is currently NOT encrypted (nvs_flash_init not nvs_flash_secure_init)** - All crypto keys (Root Key, Chain Keys, DH Keys, Auth Keys) stored in plaintext in flash. Critical for production: must use nvs_flash_secure_init + eFuse keys (Session 36)
195. **What is not there cannot be stolen. Deletion > trusting encryption** - Defense in Depth: proper key deletion on contact delete is more reliable than trusting encryption alone. Orphaned keys are attack surface (Session 36)
196. **FreeRTOS TaskNotification is more lightweight than Semaphore for 1:1 task synchronization** - No kernel object needed, no priority inversion risk. xTaskNotify/xTaskNotifyWait is simpler and faster than binary semaphore for point-to-point sync (Session 36c)
197. **xTaskNotify must send on ALL paths (success, error, timeout) or the waiting task deadlocks** - KEY handler must notify on OK, on ERR, and on timeout. Missing any path = App Task waits forever at xTaskNotifyWait. Three paths, three notifications (Session 36c)
198. **LVGL bubble objects must be explicitly deleted on contact delete** - LVGL objects survive their logical parent (contact struct). ui_chat_clear_contact() iterates children and deletes by user_data tag match (Session 36d)
199. **QR code widget caches last content -- must be explicitly reset on Delete AND before New** - Two separate points of staleness: after delete (old QR flashes) and before new contact (previous QR shows). ui_connect_reset() needed in both paths (Session 36d)
200. **FreeRTOS list.h defines LIST_H as include guard -- own macros must not collide** - Custom header using #define LIST_H conflicts with FreeRTOS internals. Renamed to CLIST_H to avoid silent include-guard collision (Session 36b)
201. **SimpleX changed from single underscore to double underscore separator** - Agent message type detection must find the LAST underscore, not the first. Parser assumed single separator but protocol evolved to double (Session 36b)
202. **UART 115200 at 5000+ log lines = ~39s overhead. 921600 = ~5s. 8x faster, one sdkconfig change** - Four sdkconfig entries control UART baudrate. At high log volume, baudrate becomes a significant development bottleneck (Session 36)

---

*Bug Tracker v31.0*
*Last updated: February 25, 2026 - Session 36*
*Total bugs documented: 57 (all FIXED) + Bug E*
*202 lessons learned!*
* Session 36: Contact Lifecycle -- Delete, Recreate, Zero Compromise*

---

## Session 37 -- Encrypted Chat History (2026-02-25 to 2026-02-27)

### AES-256-GCM Encrypted Chat History on SD Card

**2 bugs found and fixed across 4 sub-sessions (37a-37d).** Implemented complete encrypted chat persistence with per-contact key derivation, append-only storage, SPI bus serialization, and progressive chunked rendering. Contact list redesigned with single-line cards and search.

### Bug #58: SPI2 Bus Collision (Display + SD Card)

```
Symptoms:
 assert failed: spi_ll_get_running_cmd
 Display tearing/smearing when scrolling during SD access
 Triggered by chat history load from SD card

Root Cause:
 Display and SD card share the SAME SPI2 bus on T-Deck Plus.
 LVGL Task (display refresh) and App Task (SD read/write) = collision.

Fix (3 parts):
 1. SPI2 bus serialization: recursive LVGL mutex for ALL SD operations
 lvgl_port_lock(0) → SD operation → lvgl_port_unlock()
 2. DMA draw buffer moved from PSRAM to internal SRAM (~12.8KB)
 PSRAM access during SPI DMA transfers caused tearing
 3. Proper error handling for SPI timeout conditions
```

### Bug #59: 1.5s Display Freeze on Chat Open

```
Symptom:
 Opening chat with history freezes display for 1.5 seconds.
 All 20 LVGL bubble objects created synchronously.

Root Cause:
 smp_history_load() → create 20 bubbles → LVGL layout recalc → 1.5s block

Fix: Chunked rendering
 OLD: load_history() → create 20 bubbles → 1.5s freeze
 NEW: load_history() → queue 20 records → timer creates 3/tick → 350ms fluid
 "Loading..." indicator shown during progressive render

 3 bubbles per LVGL timer tick (50ms each)
 Total render time: ~350ms (vs 1.5s), display stays responsive
```

### Chat History Architecture

```
Key Management:
 Master Key (256-bit random) stored in NVS
 Per-contact key = HKDF-SHA256("simplego-chat", slot_index)

GCM Nonce (deterministic, never reused):
 nonce[0..3] = slot_index (uint32 LE)
 nonce[4..7] = msg_index (uint32 LE)
 nonce[8..11] = 0x00000000

Record Format:
 [4B record_len][12B nonce][16B GCM tag][encrypted payload]

File Header (unencrypted):
 [4B magic "SGH1"][4B version][4B msg_count][4B last_delivered_idx]

Storage Path: /sdcard/simplego/msgs/chat_XX.bin (one file per contact)
```

### Additional Fixes (Session 37)

```
37-guard: Ring buffer NULL-guard for subscribe_all_contacts race condition
 (subscribe loop could fire before ring buffer init at startup)

37d: Contact list redesign
 - CARD_H: 44px → 28px (single line, 5-6 contacts visible)
 - Bottom bar: 3 real lv_btn (100x36px touch targets)
 - Search overlay with filtered contact list
 - All green colors → cyan
```

### New Lessons Learned (Session 37)

203. **SPI2 bus is shared between display AND SD card on T-Deck Plus** - Every SD access needs the LVGL mutex (lvgl_port_lock/unlock). Not two separate buses -- one single bus. Display and SD card cannot operate concurrently (Session 37b)
204. **Chunked rendering is mandatory for history loading** - 20 LVGL objects created at once blocks display for 1.5s. 3 per tick (50ms) = 350ms total, display stays responsive. "Loading..." indicator covers the progressive render gap (Session 37b)

---

*Bug Tracker v32.0*
*Last updated: February 27, 2026 - Session 37*
*Total bugs documented: 59 (all FIXED) + Bug E*
*204 lessons learned!*
* Session 37: Encrypted Chat History -- SD Card, SPI Bus Wars, Progressive Rendering*

---

## Session 38 -- The SPI2 Bus Hunt (2026-02-28 to 2026-03-01)

### Eight Hypotheses, One Root Cause

**2 bugs found across 2 days of intensive debugging.** Added display and keyboard backlight, moved WiFi/LWIP to PSRAM, then spent two days hunting the display freeze. Eight hypotheses tested systematically -- seven wrong, one correct. SD card physically removed proved SPI2 bus sharing is the root cause.

### Bug #60: Display Freeze on SD Card Access (SPI2 Bus Contention)

```
Symptoms:
 Display freezes completely when loading chat history from SD card
 Image frozen, main loop continues (heartbeat logs still printing)
 No crash, no assert, just visual freeze
 Always in chat with the most messages

Root Cause:
 Display (ST7789) and SD card share the SAME SPI2 bus on T-Deck Plus.
 When SD card is read, SPI2 bus blocked → display rendering stalls.
 LVGL hangs in lv_timer_handler() waiting for SPI2.
 S37 mutex fix prevented crashes/tearing but contention remains:
 one bus, two masters, blocking serialization.

Proof:
 SD card physically removed → device runs HOURS, 100% stable
 SD card reinserted → freeze returns immediately on history load

Eight Hypotheses Tested:
 1. DMA Timeout → Freeze not in DMA wait path
 2. Memory Crash → ESP32 heap was never the problem
 3. DMA Callback Revert → Freeze identical without callback
 4. bubble_draw_cb → Freeze identical without custom callbacks
 5. LVGL Pool 64→192KB → WiFi init crashes (no internal SRAM left)
 6. LVGL Pool 64→96KB → Freeze continues unchanged
 7. trans_queue_depth 2→1 → Display artifacts (stripes) + OOM
 8. SD Card Removed → STABLE → SPI2 bus sharing IS the root cause

Fix Plan (Session 39):
 Move SD card to SPI3 bus. T-Deck Plus has SPI3 available.
 Separate buses = zero contention = parallel operation.

Status: ROOT CAUSE IDENTIFIED -- fix scheduled for Session 39
```

### Bug #61: LVGL Heap Exhaustion with Many Chat Bubbles

```
Symptom:
 Display freeze or crash when too many chat bubbles displayed.
 Always in chat with the most messages (same pattern as Bug #60).

Root Cause:
 LVGL has its OWN memory pool (LV_MEM_SIZE=64KB in sdkconfig).
 This is COMPLETELY SEPARATE from ESP32's system heap.
 heap_caps_get_free_size() shows ESP32 heap, NOT LVGL pool!
 64KB supports approximately 8 chat bubbles.
 More bubbles = pool exhaustion = freeze or crash.

Fix:
 MAX_VISIBLE_BUBBLES = 5 (temporary, target 8)
 Sliding window: only N most recent bubbles exist as LVGL objects
 Older messages loaded from SD history on scroll-up

Status: TEMPORARY FIX -- final sliding window in Session 39
```

### Features Implemented (Session 38)

```
1. Keyboard Backlight
 - I2C address 0x55
 - Auto-off timer
 - Independent from SPI bus (I2C)

2. Display Backlight
 - GPIO 42 with pulse-counting (16 brightness levels)
 - Independent from SPI bus (pure GPIO)
 - Starts at 50% on boot

3. Settings Screen
 - Brightness sliders for display and keyboard
 - Preset buttons for quick adjustment
 - Gear button in chat header for quick access

4. WiFi/LWIP → PSRAM
 - WiFi and LWIP buffers moved from internal SRAM to PSRAM
 - 56KB internal SRAM freed
 - No performance impact observed
```

### SPI Architecture Decisions

```
Synchronous SPI (stable):
 DMA callback mechanism (S38f) identified as adding complexity
 without solving fundamental SPI2 contention. Removed.
 Replaced with synchronous draw_bitmap() + flush_ready().

trans_queue_depth:
 MUST remain at 2. Setting to 1 causes OOM + display stripes.
 This is a hard constraint of the ESP-IDF SPI driver.

Uncommitted changes (waiting for S39 SPI3 fix):
 - main/main.c: MAX_VISIBLE_BUBBLES 5
 - tdeck_lvgl.c: Synchronous SPI (DMA callback removed)
```

### Key Insight: Correlation ≠ Causation

```
The display freeze bug existed since Session 37 (SD card introduction).
It was never triggered earlier because there weren't enough chat
messages to cause significant SD read time.

The backlight commits (Session 38) were temporally correlated but
NOT causally related. Backlight uses I2C (keyboard) and GPIO (display),
both completely independent from SPI2.

Classic case: Correlation ≠ Causation.
```

### Commits (Session 38)

```
f0616e4 feat(core): integrate backlight initialization in boot sequence
0cfc8ca feat(ui): add gear button in chat header for backlight control
a5995ec feat(ui): add settings screen with display and keyboard brightness
1179c74 feat(hal): add display backlight control via pulse-counting
fa4d40a feat(hal): add dedicated keyboard backlight module
91b380f docs(config): correct SD card pin definitions for T-Deck Plus
381122c perf(config): move WiFi/LWIP buffers to PSRAM, free 56KB internal SRAM
108b4c8 feat(keyboard): add backlight control with auto-off timer
25ad16f fix(display): sync DMA completion before mutex release, add OOM retry
6f1436d perf(display): reduce SPI transfer size and queue depth
```

### New Lessons Learned (Session 38)

205. **SPI2 bus sharing is the root cause of display freeze** - Not DMA, not memory, not LVGL pool. SD card removed = device 100% stable for hours. Correlation (backlight commits) ≠ Causation (SPI2 bus). Physical elimination test is definitive (Session 38)
206. **LVGL has its own heap (LV_MEM_SIZE), separate from ESP32 heap** - heap_caps_get_free_size() does NOT show LVGL pool status. LV_MEM_SIZE=64KB in sdkconfig supports ~8 chat bubbles. Monitoring ESP32 heap tells you NOTHING about LVGL memory (Session 38)
207. **trans_queue_depth MUST stay at 2** - Setting to 1 causes OOM errors and display artifacts (stripes). Synchronous draw_bitmap() + flush_ready() is more stable than async DMA callback. Hard constraint of ESP-IDF SPI driver (Session 38)
208. **When multiple hypotheses fail, remove the suspected hardware** - Physical elimination test beats software debugging. 8 hypotheses tested in software, none conclusive. SD card physically removed = instant answer. Always try the simplest physical test (Session 38)
209. **WiFi/LWIP buffers can safely run from PSRAM, freeing ~56KB internal SRAM** - No performance impact observed. WiFi and LWIP are not latency-critical enough to require internal SRAM. Good optimization for memory-constrained ESP32-S3 projects (Session 38)

---

*Bug Tracker v33.0*
*Last updated: March 1, 2026 - Session 38*
*Total bugs documented: 61 (59 FIXED, 1 identified for S39, 1 temp fix) + Bug E*
*209 lessons learned!*
* Session 38: The SPI2 Bus Hunt -- Eight Hypotheses, One Root Cause*

---

## Session 39 -- On-Device WiFi Manager (2026-03-03)

### First On-Device WiFi Manager for T-Deck Hardware

**9 bugs found and fixed in a single day.** After exhaustive market research (Meshtastic, MeshCore, Bruce, ESP32Berry, Armachat, ESPP, all ESP-IDF WiFi libraries), confirmed no T-Deck project has on-device WiFi setup. SimpleGo is the first. Complete WiFi backend rewrite, first-boot flow, WPA3 fix, SPI DMA fix, dynamic UI.

### Bug #62: Dual-File WiFi Race Condition

```
Problem:
 smp_wifi.c and wifi_manager.c fought each other.
 smp_wifi.c had unconditional auto-reconnect on DISCONNECT event.
 wifi_manager.c disconnects to switch network → smp_wifi.c reconnects to old.

 wifi_manager: esp_wifi_disconnect() → DISCONNECT event fires
 smp_wifi: event_handler() → esp_wifi_connect(old_network!)
 wifi_manager: esp_wifi_connect(new) → ALREADY CONNECTED to old!

Fix:
 Both files merged into single wifi_manager.c.
 One state machine, one event handler chain, no conflicts.
 Kconfig credentials eliminated as priority source, NVS-only storage.
```

### Bug #63: UI Freeze from vTaskDelay in LVGL Context

```
Problem:
 deferred_wifi_rebuild() called vTaskDelay(pdMS_TO_TICKS(200))
 Blocked LVGL render task for 200ms -- no screen updates, no input.

Fix:
 Replaced with lv_timer_create() one-shot timer.
 Fires after 200ms without blocking LVGL task.
```

### Bug #64: WiFi Scan Race Condition

```
Problem:
 wifi_scan_poll_cb() called ESP-IDF APIs directly:
 esp_wifi_scan_get_ap_num() + esp_wifi_scan_get_ap_records()
 Instead of using cached results from backend.
 Inconsistent results when scan and UI polling out of sync.

Fix:
 Switched to wifi_manager_get_scan_count() / get_scan_results().
 Backend caches results after scan completion, UI reads cache only.
```

### Bug #65: First Scan Shows No Results

```
Problem:
 500ms "stale flag guard" ignored wifi_manager_is_scan_done() for first 500ms.
 ESP32 completes scan in <500ms with few APs → poll misses done signal.

Fix:
 Stale flag guard completely removed.
 Backend sets s_scan_done = false before each new scan. Sufficient.
```

### Bug #66: WPA3 SAE Authentication Failure (0x600)

```
Symptom:
 auth -> init (0x600) on every connection attempt
 10 retries, then state machine dead
 Router: WPA2/WPA3 Transition Mode

Root Cause:
 WIFI_AUTH_WPA_WPA2_PSK threshold made ESP32 attempt WPA3-SAE.
 SAE negotiation on ESP32-S3 (ESP-IDF 5.5.2) fails consistently
 with Transition Mode routers.

Fix:
 Threshold: WIFI_AUTH_WPA2_PSK
 sae_pwe_h2e = WPA3_SAE_PWE_BOTH
 pmf_cfg.capable = true, required = false
 Accepts WPA2 and WPA3, but doesn't aggressively attempt SAE.

 1 line of code, 100+ test attempts to isolate.
```

### Bug #67: Dead State Machine After Exhausted Retries

```
Problem:
 After 10 failed boot retries, WiFi state machine stayed dead.
 Manual connect via WiFi Manager ignored (retry counter exhausted).

Fix:
 Retry counter reset on every new wifi_manager_connect() call.
 esp_wifi_disconnect() before esp_wifi_connect() to clean driver state.
```

### Bug #68: SPI DMA OOM on Screen Switch

```
Symptom:
 lcd_panel.io.spi: spi transmit (queue) color failed
 TDECK_LVGL: draw_bitmap FAILED: ESP_ERR_NO_MEM (0x101)
 LVGL buffer at 0x3c1d79a8 (PSRAM range)

Root Cause:
 Previously invisible because WiFi Manager never worked.
 Working WiFi Manager = first "screen switch during active network session".
 TLS/SMP/crypto consumed internal SRAM → malloc fell back to PSRAM.
 ESP32-S3 SPI DMA cannot read from PSRAM.

Fix:
 LVGL draw buffer: MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
 Guarantees internal SRAM placement regardless of heap pressure.
```

### Bug #69: Splash Timer Overwrites WiFi Manager

```
Problem:
 First boot: SMP task detects "no WiFi" at ~2040ms → opens Settings/WiFi
 Splash timer fires at ~3770ms → overwrites with Main Screen

Fix:
 Navigation guard in ui_splash.c:
 If current screen != Splash (task already navigated), timer does nothing.
```

### Bug #70: SSID Not Visible on Main Screen

```
Problem:
 WiFi SSID only appeared after visiting Settings/WiFi.

Root Cause:
 ui_main_refresh() called only once at screen create.
 WiFi not yet connected at that point.

Fix:
 3-second timer hdr_refresh_cb checks WiFi status + unread count.
 Dynamic header: unread (blue mail icon) / SSID (cyan) / "No WiFi" (grey)
```

### Market Research Results

```
No T-Deck project has an on-device WiFi manager:

 Meshtastic (9,000 commits): CLI, phone app, web UI only
 LilyGo Factory Firmware: Hardware examples, no WiFi
 ESP32Berry: Had LVGL WiFi, archived May 2024
 Bruce Firmware: Best WiFi, but TFT_eSPI not LVGL
 All ESP-IDF libraries: Web portals or BLE, no on-device LVGL

 SimpleGo: FIRST on-device WiFi manager for T-Deck with LVGL + keyboard
```

### New Lessons Learned (Session 39)

210. **WPA3 SAE negotiation on ESP32-S3 is fragile with WPA2/WPA3 Transition Mode routers** - WIFI_AUTH_WPA2_PSK as threshold is the safe default. Allows WPA3 when forced, but doesn't aggressively attempt SAE. Poorly documented in ESP-IDF. 100+ test attempts to isolate (Session 39)
211. **ESP32-S3 SPI DMA cannot read from PSRAM** - When internal SRAM is consumed by TLS/SMP/crypto, malloc falls back to PSRAM silently. LVGL draw buffers must use MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL to prevent runtime OOM. The failure only manifests under memory pressure (Session 39)
212. **FreeRTOS tasks and LVGL timers run asynchronously** - Timer-based navigation must always check "am I still the active screen?" before acting. Without guards, a late-firing timer overwrites earlier task navigation. Essential for every timer callback that changes screens (Session 39)
213. **No T-Deck project has an on-device WiFi manager** - After analyzing Meshtastic, MeshCore, Bruce, ESP32Berry, Armachat, ESPP, and all ESP-IDF WiFi libraries: SimpleGo is the first. Market gap confirmed across 6+ projects and 10+ libraries (Session 39)

---

*Bug Tracker v34.0*
*Last updated: March 3, 2026 - Session 39*
*Total bugs documented: 70 (68 FIXED, 1 identified for SPI3, 1 temp fix) + Bug E*
*213 lessons learned!*
* Session 39: WiFi Manager -- First On-Device WiFi Setup for T-Deck*

---

## Session 40 -- Sliding Window Chat History (2026-03-03 to 2026-03-04)

### Three-Stage Pipeline Architecture

Session 40 solved the chat history display problem with a three-stage sliding window: SD card (unlimited, AES-256-GCM) to PSRAM cache (30 messages) to LVGL window (5 bubbles, ~6KB). One bug fixed, seven lessons learned.

### Bug #71: Scroll Re-Entrancy

```
Symptom:
 7 bubbles rendered instead of 5 during scroll operations.

Root Cause:
 lv_obj_scroll_to_y() inside load_older_messages() synchronously fires
 a new LV_EVENT_SCROLL, triggering load_newer_messages() in the same frame.

Fix:
 s_window_busy flag prevents re-entrant scroll handling.
 Set before scroll operations, cleared after completion.
```

### Package 40a: Crypto-Separation from SPI Mutex

SD operations held the LVGL/SPI2 mutex during AES-GCM crypto, blocking display rendering for ~500ms. Refactored to two-pass architecture: file I/O inside mutex (< 10ms), crypto outside. File handles closed between passes (FATFS safety).

### Package 40b: LVGL Pool Profiling

Measured actual per-bubble cost at ~1.2KB (vs estimated 3-4KB). LVGL pool total ~61KB (TLSF overhead ~3KB from 64KB configured). Fixed UI costs ~28KB. Safety: 8KB margin check before bubble creation, text truncation to 512 chars.

### Package 40c: Bidirectional Scroll

PSRAM cache (MSG_CACHE_SIZE=30), LVGL window (BUBBLE_WINDOW_SIZE=5), scroll triggers at 10px threshold. Scroll-up removes bottom bubbles, inserts older at top with position correction. Scroll-down symmetric reverse. Setup guard blocks scroll events during construction. Progressive render 3 bubbles per 50ms tick.

### New Lessons Learned (Session 40)

214. LVGL v9 with LV_STDLIB_BUILTIN has a fixed 64KB pool (TLSF, effectively ~61KB). This is NOT the ESP32 system heap and NOT PSRAM (Session 40)
215. A single 15KB message would consume nearly the entire LVGL pool. Text truncation in the bubble layer is essential for survival (Session 40)
216. lv_obj_scroll_to_y() inside LV_EVENT_SCROLL fires a synchronous new scroll event. Re-entrancy guard is mandatory (Session 40)
217. Crypto operations (AES-GCM, HKDF) are pure CPU work and do not belong inside SPI mutex blocks. Separation reduces hold time from ~500ms to < 10ms (Session 40)
218. File handles must be closed between mutex passes. Open handles across mutex release are FATFS-unsafe on ESP-IDF (Session 40)
219. HISTORY_MAX_TEXT (storage) and HISTORY_DISPLAY_TEXT (UI) must be separate constants. Conflation causes data loss or pool overflow (Session 40)
220. Per-bubble LVGL pool cost is predictable (~1.2KB) with text truncation active. Without truncation, cost varies by factor 10+ (Session 40)

---

*Bug Tracker v35.0*
*Last updated: March 4, 2026 - Session 40*
*Total bugs documented: 71 (69 FIXED, 1 identified for SPI3, 1 temp fix) + Bug E*
*220 lessons learned*
*Session 40: Sliding Window -- Unlimited Encrypted History at Constant Memory*

---

## Session 41 -- Pre-GitHub Cleanup and Stabilization (2026-03-04)

### Pre-Release Code Review

Session 41 was the most comprehensive cleanup session in the project. Eight sub-packets (41a-41h) produced 11 commits. No new bugs introduced, but several existing issues hardened and debug artifacts removed across 15+ files.

### 41a: Security Cleanup

```
Deleted:
 simplex_secretbox_open_debug() -- ~90 lines from simplex_crypto.c/h
 Brute-force E2E decrypt loop (Methods 0-3) -- replaced with direct decrypt_client_msg()
 KEY_DEBUG ESP_LOG_BUFFER_HEX blocks from smp_tasks.c
 Empty ui_task() and its xTaskCreatePinnedToCore call
 T6-Diag5 hex dump block from smp_network.c

Changed:
 RECV, BLOCK_TX, DH shared secret: LOGI to LOGD
 smp_storage.c: memset() replaced with mbedtls_platform_zeroize() (CWE-14)

GitHub Security:
 CodeQL C/C++ analysis enabled
 Dependabot alerts enabled
 Secret push protection enabled
 SECURITY.md vulnerability reporting policy added
```

### 41b: LVGL Pool Measurements (Definitive)

```
Per-bubble cost: 960-1368 bytes (average ~1150 bytes)
BUBBLE_WINDOW_SIZE = 5 confirmed correct
5 bubbles consume ~5500-6500 bytes from ~59KB available pool
No memory leak detected on contact switch
Fragmentation: 44% to 48%, stabilizes (no degradation over time)
```

### 41c: Hardware AES Fix

```
Symptom:
 AES-GCM decrypt fails on 13KB+ message bodies
 Only 9.6KB contiguous internal SRAM free at runtime

Root Cause:
 ESP-IDF hardware AES accelerator requires contiguous internal SRAM for DMA.
 When internal SRAM is fragmented, hardware accelerator silently fails.

Fix:
 CONFIG_MBEDTLS_HARDWARE_AES=n in sdkconfig.defaults
 Software AES uses CPU, allocates from any heap including PSRAM.

Build: idf.py fullclean && idf.py erase-flash && idf.py build flash monitor
```

### 41d: Live Bubble Eviction Order Fix

```
Root Cause:
 Live message handler created bubble THEN checked eviction.
 Pool check rejected new bubble because eviction had not freed space.

Fix:
 Evict FIRST if bubble_count >= BUBBLE_WINDOW_SIZE, THEN create.
 Safety margin lowered from 8192 to 4096 bytes (eviction frees ~1.2KB).
 Files: ui_chat.c, ui_chat_bubble.c
```

### 41e: LVGL Pool Fragmentation Diagnosis

```
Finding: Screens created on first visit but NEVER deleted.
 After Main + Contacts + Chat + Settings: 4 screens simultaneous
 ~14KB permanently consumed by inactive screens
 Pool after: 8576 bytes free (86% used) -- insufficient for 5 bubbles
```

### 41f: Screen Lifecycle Fix + Dangling Pointer Protection

```
Screen Lifecycle (ui_manager.c):
 Delete previous screen after lv_scr_load().
 Main screen exempt (permanent). All others: ephemeral.

Dangling Pointer Protection:
 ui_chat.c: ui_chat_cleanup() nullifies 6 static LVGL pointers.
 if (!screen) return guards on 4 public functions.
 ui_chat_bubble.c: chat_bubble_cleanup() zeros tracked_msgs[].

Result: Pool ~42,970 bytes free (31% used), stable across 8 visits.
 Before fix: 8,576 bytes free (86% used). Recovery: ~34KB.
```

### 41g: Comment Cleanup (9 files)

```
smp_tasks.c, smp_peer.c, smp_e2e.c, smp_network.c, smp_storage.c
ui_manager.c, ui_chat.c, ui_chat_bubble.c, tdeck_display.c

Removed: All Session/Auftrag/Phase/T-ref comments
Translated: German comments to English
Marked: extern declarations with TODO for header migration
```

### 41h: send_agent_confirmation() Cleanup

```
133 lines removed from smp_peer.c:
 CONF_CMP diagnostic blocks, AUFTRAG-15a/17 blocks
 6 printf hex-dump loops, DEBUG-Check block

Zero printf remaining in production code.
Build fix: dead call to deleted simplex_secretbox_open_debug()
 replaced with simplex_secretbox_open().
```

### New Lessons Learned (Session 41)

221. **ESP-IDF hardware AES accelerator requires contiguous internal SRAM for DMA** - When internal SRAM is fragmented (< 13KB contiguous), hardware AES silently fails. CONFIG_MBEDTLS_HARDWARE_AES=n forces software AES from any heap including PSRAM. Negligible performance impact (Session 41)
222. **LVGL screens created but never deleted consume pool memory permanently** - After navigation sequence: ~14KB consumed by inactive screens, only 8.5KB left. Ephemeral screen pattern (create on enter, destroy on leave) recovers ~34KB, pool at 31% (Session 41)
223. **Background tasks may call UI functions after screen destruction** - All public UI functions need if (!screen) return guards. All static LVGL pointers must be nullified in cleanup. Without this, protocol task crashes on destroyed chat (Session 41)
224. **Bubble eviction order: evict-before-create, not create-then-evict** - Create-then-evict fails because pool check rejects new bubble before eviction frees space (~1.2KB). Evict first provides headroom (Session 41)
225. **mbedtls_platform_zeroize() for sensitive buffer clearing (CWE-14)** - Unlike memset(), guaranteed not optimized away by compiler. Required for cryptographic material (Session 41)

---

*Bug Tracker v36.0*
*Last updated: March 4, 2026 - Session 41*
*Total bugs documented: 71 (69 FIXED, 1 identified for SPI3, 1 temp fix) + Bug E*
*225 lessons learned*
*Session 41: Pre-GitHub Cleanup and Stabilization*

---

## Session 42 -- Consolidation and Quality Pass (2026-03-04 to 2026-03-05)

### Pure Structural Session

No new bugs. No functional changes. Every task produced identical object code. Session focused on code hygiene, architectural correctness, and license compliance across all 47 source files.

### Task 1: smp_handshake.c Debug Cleanup

```
1281 to 1207 lines (74 lines removed):
 9 printf blocks removed (Layer dumps L0-L6, SKEY hex, A_MSG hex+ASCII)
 Auth-key-prefix log removed (security: Ed25519 key bytes to serial)
 Plaintext message log removed (privacy: cleartext to serial)
 8 verbose LOGIs downgraded to LOGD
 Session/task comments removed, German to English

Result: Zero printf in production. Verified across entire codebase.

Side fix: smp_storage.c missing #include "mbedtls/platform_util.h" added.
```

### Task 2: smp_globals.c Dissolved (11 files)

```
Architectural anomaly eliminated. 7 symbols migrated to owning modules:

 ED25519_SPKI_HEADER[12] -> smp_contacts.c / smp_contacts.h
 X25519_SPKI_HEADER[12] -> smp_contacts.c / smp_contacts.h
 contacts_db -> smp_contacts.c / smp_contacts.h
 base64url_chars[] -> smp_utils.c / smp_utils.h
 pending_peer -> smp_peer.c / smp_peer.h
 peer_conn -> smp_peer.c / smp_peer.h
 wifi_connected -> wifi_manager.c / wifi_manager.h

smp_types.h: types only, no object declarations.
smp_globals.c: deleted.
3 follow-up build errors fixed (missing includes in smp_queue.c,
reply_queue.c, smp_parser.c).
```

### Task 3: extern TODO Resolved

```
All TODO markers already cleaned during Task 2.
One manual find: extern bool peer_send_hello() in smp_tasks.c.
Moved to smp_peer.h. Local extern removed.
```

### Task 4: Re-delivery Log (Verified Correct)

```
smp_ratchet.c re-delivery log already ESP_LOGW. No changes needed.
```

### Task 5: smp_app_run() Refactored

```
530 lines to 118 lines. Five static helpers extracted:

 app_init_run() Initialization, subscribe, wildcard ACK
 app_process_deferred_work() NVS saves (contacts, RQ, history)
 app_process_keyboard_queue() Keyboard send, delivery status, history
 app_handle_reply_queue_msg() Reply Queue: E2E, agent, 42d, ACK
 app_handle_contact_queue_msg() Contact Queue: SMP, parse, ACK

No new headers. No logic changes. Identical object code.
```

### Task 6: License Header Audit (47 files)

```
All 47 source files in main/ now carry standardized header:

 /**
 * SimpleGo - filename.c
 * Brief description
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

Side effect: 7 files had UTF-8 BOM characters, cleaned during pass.
Processed in 9 rounds with intermediate build verification.
```

### Ownership Model Established

```
smp_types.h: types only (typedef, enum, #define)
smp_contacts.c: contacts_db, SPKI headers
smp_peer.c: pending_peer, peer_conn
wifi_manager.c: wifi_connected
smp_utils.c: base64url_chars
```

### New Lessons Learned (Session 42)

226. **Global state containers are architectural debt** - Every global symbol should live in the module that logically owns it, with extern in that module's header. A catch-all file (smp_globals.c) obscures ownership and creates unnecessary coupling. smp_types.h is for types only (Session 42)
227. **smp_app_run() at 530 lines was unmaintainable** - Five static helpers with clear responsibility boundaries (init, deferred, keyboard, reply queue, contact queue) reduce the main loop to 118 lines. Identical object code. Function names document intent (Session 42)
228. **UTF-8 BOM in C source files causes subtle ESP-IDF warnings** - Clean during any file-wide pass. Primarily affects files created in Windows editors. BOM is not standard for C source (Session 42)
229. **License header standardization requires batch processing with intermediate builds** - 47 files in 9 rounds of 5. Never apply all at once without build verification between rounds. One typo in a header can break the entire build (Session 42)

---

---

## Session 43 -- 2026-03-05 to 2026-03-08 (Documentation + Security + UX)

### Bug #20: SEND Fails After Extended Idle (SHOWSTOPPER)

```
Symptom:
  After 6+ hours idle (PING/PONG running, no messages sent),
  SEND commands fail immediately. Red X on display.
  2-3 red error lines in serial log.

Key Observations:
  PING/PONG working correctly (every ~30s, server responds)
  Problem is NOT keep-alive -- subscription appears active
  Device reset fixes immediately
  Failed messages do NOT appear in chat history (failure before SD write)
  Did not exist before Session 41/42 changes
  Occurs reliably after overnight idle (6-8 hours)

Possible Causes:
  1. WiFi Manager background reconnect creates new socket without SMP recovery
  2. Slow memory leak -- heap exhaustion for SEND buffer after hours
  3. Session 42 refactoring: state variable lost scope in helper extraction
  4. TLS session timeout despite SMP PING keeping SMP layer alive
  5. Ratchet synchronization issue after long idle

Status: SHOWSTOPPER -- Priority P0 for Session 44
```

### Security Log Cleanup (Workstream 2)

```
smp_parser.c (9 removals):
  Key1/Key2 SPKI header printf loops, raw key printf loops,
  dump_hex for SPKI key, raw key, after-key, before-key data.
  CRITICAL: decrypted plaintext dump removed.

smp_tasks.c (2 blocks):
  KEY_DEBUG transmission hex dump, KEY response hex dump.

smp_contacts.c (5 lines + cleanup):
  Response hex dump, correlation ID, entity ID, recipient ID,
  command bytes. Orphaned cmd_dump variable removed.

Remaining: dump_hex calls in smp_contacts.c (+0000: prefix) for S44.
```

### Display Name Feature (Workstream 3)

```
Problem: Display name hardcoded "ESP32" in AgentConfirmation JSON.

Fix (4 parts):
  1. NVS: storage_get/set_display_name(), "user_name" key
  2. Dynamic AgentConfirmation: NVS lookup replaces hardcoded "ESP32"
  3. Settings: clickable name row in INFO tab, fullscreen overlay editor
  4. First-boot: UI_SCREEN_NAME_SETUP, one-shot check before MAIN

Crash fix: Guru Meditation LoadProhibited in ui_connect.c
  (dangling pointers after screen deletion)

Design decision: No broadcasting to existing contacts.
  Deferred to future privacy settings architecture.
```

### New Lessons Learned (Session 43)

230. **Add-Content in PowerShell appends blindly to any file regardless of structure** - For structured markdown documents always use Set-Content with complete content or precise -replace operations. Never use Add-Content for session documentation files (Session 43)
231. **Session documentation files have strict naming conventions** - Pattern is NN_PARTMM_SESSION_SS.md where NN is file sequence, MM is part number, SS is session number. The SESSION_XX_PROTOCOL.md pattern does not exist in this codebase (Session 43)
232. **Official architecture documents from Evgeny must be preserved in evgeny_reference.md** - Unverified figures (battery runtime) must not be published in public docs until measured on device. Architectural explanations (resource-based addressing, polling elimination) are verified and can be integrated immediately (Session 43)

### Open Security Items (carried forward)

- [ ] Remaining dump_hex calls in smp_contacts.c (+0000: prefix format)
- [ ] eFuse + nvs_flash_secure_init combined with CRYSTALS-Kyber (Kickstarter phase)
- [ ] Private Message Routing (post-MVP)

---

*Bug Tracker v39.0*
*Last updated: March 8, 2026 - Session 43*
*Total bugs documented: 72 (69 FIXED, 1 identified for SPI3, 1 temp fix, 1 SHOWSTOPPER) + Bug E*
*232 lessons learned*
*Session 43: Documentation + Security Cleanup + Display Name*
*OPEN: Bug #20 SEND fails after 6+ hours idle -- SHOWSTOPPER*

---

## Session 44 -- 2026-03-08 (Security Architecture Session)

No firmware changes. Pure architecture and documentation session.

### Bug #20 Status Change: SHOWSTOPPER demoted to KNOWN

```
Decision: Accepted for Alpha release. SEND-after-idle (6+ hours) is
documented behavior for Alpha. Diagnostic logging deferred to future session.
Rationale: Alpha users can reset device. Bug does not affect security.
```

### Bug #21: SD Card Phantom Counter (NEW, LOW)

```
Symptom:
  After idf.py erase-flash and reflash, contact list shows old message
  counters (e.g. "50 messages") and possibly old display names.

Root Cause:
  SD card survives erase-flash. Firmware reads SD card metadata without
  verifying that corresponding NVS key material exists. Messages themselves
  are NOT readable (encryption keys gone), but metadata indicator persists.

Fix (planned):
  Validate SD metadata against NVS state on boot. Missing NVS key for
  a contact means SD data is orphaned -- ignore or clean.

Priority: LOW (cosmetic, no security impact)
```

### Security Architecture Research Findings

```
8 ESP32 vulnerabilities cataloged:

  AR2022-003: ESP32-S3 likely affected by AES side-channel extraction
    (Ledger Donjon, IACR 2023/090, ~300K power measurements, ~2 hours)
  HMAC peripheral (SHA-256): NOT specifically targeted -- potential advantage

  ESP32-C6 DPA countermeasures proven ineffective (Courk 2024):
    Anti-DPA pseudo-rounds and clock randomization bypassed
    ESP32-P4 similar XTS_DPA_PSEUDO_LEVEL: unproven until validated

  Attack economics:
    Without vault: $15, 5 minutes (flash readout)
    With HMAC vault: $2K-30K equipment, days to weeks, single device
    SiliconToaster (EMFI): $50-150 DIY (open-source fault injection)

Post-quantum discovery:
  SimpleX uses sntrup761 (Streamlined NTRU Prime), NOT Kyber/ML-KEM
  Must confirm with Evgeny before implementation
```

### Four Security Modes Defined

```
Mode 1 (Open):    No eFuse. Full dev access. Community Edition.
Mode 2 (Vault):   HMAC key in eFuse. NVS encrypted. Web flash works. Kickstarter.
Mode 3 (Fortress): Mode 2 + Flash Encryption. No web flash. OTA only.
Mode 4 (Bunker):  Mode 3 + Secure Boot. Only signed firmware. JTAG disabled.
```

### New Lessons Learned (Session 44)

233. **SimpleX uses sntrup761, NOT CRYSTALS-Kyber/ML-KEM for post-quantum** - Must verify with Evgeny before implementation. Both fit within ESP32 constraints (16 KB SMP block, < 13 ms key exchange). Protocol compatibility is non-negotiable (Session 44)
234. **HMAC-based NVS encryption (HMAC_UP) is superior to Flash-Encryption-based NVS** - Independent operation, no nvs_keys partition, runtime key derivation, ESP-IDF v6.0 alignment. Uses BLOCK_KEY1. Chosen for SimpleGo Alpha (Session 44)
235. **ESP32-C6 DPA countermeasure bypass (Courk 2024) means P4's similar features are unproven** - Never claim anti-DPA as "effective" without independent validation. XTS_DPA_PSEUDO_LEVEL is a mitigation attempt, not a guarantee (Session 44)
236. **Espressif AR2022-003 lists ESP32-S3 as likely affected by AES side-channel** - HMAC peripheral (SHA-256) not specifically targeted, making it potentially stronger for key protection. Architecture choice: HMAC over AES for key derivation (Session 44)
237. **SD card survives idf.py erase-flash** - Any SD metadata (message counters, display names) must be validated against NVS state. Missing NVS keys = orphaned SD data = ignore or clean (Session 44)

---

*Bug Tracker v40.0*
*Last updated: March 8, 2026 - Session 44*
*Total bugs documented: 73 (69 FIXED, 1 SPI3, 1 temp fix, 1 KNOWN idle, 1 LOW phantom) + Bug E*
*237 lessons learned*
*Session 44: Hardware Class 1 Security Architecture -- 15 docs, 3,243 lines, 191 KB*

---

## Session 45 -- 2026-03-10 (Security Implementation)

### Four Security Findings CLOSED

Session 45 closed 4 of the 6 tracked security findings, including both CRITICALs. This is the most impactful security session in the project's history.

### SEC-01 CLOSED (CRITICAL): PSRAM Plaintext Wipe

```
Problem:
  30 decrypted messages sat as plaintext in s_msg_cache (PSRAM).
  Physical attacker with JTAG could read all messages while powered.
  123,600 bytes of plaintext, never zeroed.

Fix:
  New function ui_chat_secure_wipe() in ui_chat.c:
  1. wipe_labels_recursive(): iterate LVGL tree, set all labels to ""
  2. sodium_memzero(s_msg_cache, 123600): guaranteed non-optimizable wipe

  Called at 4 points:
    ui_chat_cleanup()        -- screen destroy
    ui_chat_switch_contact() -- before loading new contact
    ui_chat_cache_history()  -- before copying new history
    ui_manager_lock()        -- before lock screen

  Log: "SEC-04: Bubble labels wiped" + "SEC-01: PSRAM msg cache wiped (123600 bytes)"
```

### SEC-04 CLOSED (HIGH): Auto-Lock with Memory Wipe

```
New files: ui_lock.c, ui_lock.h

Lock screen: lock icon + "Press any key to unlock"
Hidden LVGL textarea captures keypress -> ui_manager_unlock() -> go_back()

LVGL timer in ui_manager.c checks lv_disp_get_inactive_time() every 2 seconds.
After 60 seconds: ui_manager_lock() -> ui_chat_secure_wipe() -> lock screen.
SMP connection and PING/PONG continue during lock.

Minor fix: UI_FONT_LG does not exist in theme, corrected to UI_FONT_MD.
```

### SEC-02 CLOSED (CRITICAL): HMAC NVS Vault

```
Problem:
  All cryptographic keys plaintext in NVS flash.
  Physical attacker reading flash chip gets all private keys.

Fix:
  ESP-IDF HMAC-based NVS encryption.
  nvs_flash_init() auto-provisions on first boot:
    - Registers HMAC-based scheme
    - Generates NVS encryption keys
    - Burns eFuse BLOCK_KEY1 (HMAC_UP purpose)
    - Encrypts NVS partition

  Verified on Opfer-T-Deck (COM8):
    KEY_PURPOSE_1 = HMAC_UP, status R/- (write-protected)
    BLOCK_KEY1 = ??? (read-protected)
    BLOCK_KEY0, KEY2-5 = empty (available as reserve)

  main.c: logs Security Mode (Open/Vault), halts on NVS init error
  (no fallback to unencrypted)
```

### SEC-05 CLOSED (MEDIUM): Device-Bound HKDF

```
Problem:
  HKDF info parameter was only slot index (1 byte).
  Same master key + same slot on different device = identical derived key.

Fix:
  Info parameter: "simplego-slot-XX-AABBCCDDEEFF"
  Last 12 hex chars = chip MAC from esp_efuse_mac_get_default()
  Each device derives unique per-contact encryption keys.

  File: smp_history.c
```

### Build System Issues (Session 45)

```
1. Minimal sdkconfig.defaults: WRONG approach
   Missing LVGL modules (lv_qrcode, lv_font_montserrat_10) on regeneration.
   Fix: export complete working sdkconfig as defaults (93 KB).

2. Windows PowerShell semicolon:
   -D SDKCONFIG_DEFAULTS="a;b" silently ignores second file.
   Fix: manual Add-Content to append vault lines.

3. Spurious esp_efuse.h include:
   nvs_flash_init() handles everything, no direct eFuse calls needed.
   Caused linker error (efuse not in PRIV_REQUIRES).
```

### Security Status Summary

```
SEC-01 CLOSED (Session 45): sodium_memzero on PSRAM cache
SEC-02 CLOSED (Session 45): HMAC NVS vault, eFuse BLOCK_KEY1
SEC-03 CLOSED (Session 42): mbedtls_platform_zeroize
SEC-04 CLOSED (Session 45): Auto-lock + memory wipe (60s)
SEC-05 CLOSED (Session 45): Device-bound HKDF (chip MAC)
SEC-06 DEFERRED:            Post-quantum (sntrup761, pending Evgeny)

5 of 6 CLOSED. Only SEC-06 remains, intentionally deferred.
```

### New Lessons Learned (Session 45)

238. **sodium_memzero must be called at every cache transition point** - Four call sites: cleanup, contact switch, history load, screen lock. Missing any one leaves plaintext in PSRAM. Not just on screen destroy (Session 45)
239. **LVGL label text wipe is best-effort only** - LVGL internal pool does not guarantee overwrite on object deletion. Setting text to empty before deletion is closest approximation. True pool zeroing requires custom allocator (Session 45)
240. **ESP-IDF HMAC NVS encryption is fully automatic after sdkconfig activation** - nvs_flash_init() handles eFuse detection, key generation, block burning, partition encryption on first boot. No manual espefuse.py needed (Session 45)
241. **Windows PowerShell does not handle semicolons in -D SDKCONFIG_DEFAULTS** - Second file silently ignored. Workaround: merge overlay content into main defaults via script or manual append (Session 45)
242. **Complete sdkconfig as defaults (93 KB) is safer than minimal handwritten** - Incomplete defaults cause missing LVGL modules and hard-to-diagnose build failures. Large file is the correct source of truth (Session 45)

---

*Bug Tracker v41.0*
*Last updated: March 10, 2026 - Session 45*
*Total bugs documented: 73 (69 FIXED, 1 SPI3, 1 temp fix, 1 KNOWN idle, 1 LOW phantom) + Bug E*
*242 lessons learned*
*Security: 5/6 findings CLOSED*
*Session 45: Runtime Security + HMAC NVS Vault*

---

## Session 46 -- 2026-03-11 to 2026-03-12 (Codename MEGABLAST)

### WORLD FIRST: Post-Quantum Double Ratchet on Dedicated Hardware

SEC-06 CLOSED. sntrup761 integrated into SimpleX Double Ratchet. First quantum-resistant message received 2026-03-12 at 09:16 CET. SimpleX App confirmed "Quantum Resistant". 6/6 security findings ALL CLOSED.

### Bug #22: Standby Freeze Returning from Lock (NEW, MEDIUM)

```
Symptom:
  Device freezes when returning from lock screen after standby.
  Not PQ-related (occurs without PQ operations).

Status: NEW, for Session 47.
```

### 6 Bugs Fixed During PQ Integration

```
Bug 1 (CRITICAL): AES-GCM Body Decrypt Failed (ret=-18)
  Root cause: State machine error. Upon receiving Proposed, encap shared
  secret was immediately fed into receive-kdf_root. Sender had NOT used
  any KEM secret at that point. Different root keys -> decrypt failure.
  Fix: Encap result stored in pending_ss, fed into send-kdf_root on
  next outgoing ratchet step. *kem_ss_valid = false for Fall 2.

Bug 2 (CRITICAL): Heap Crash in ratchet_encrypt (StoreProhibited)
  Root cause: PQ header = 2346 bytes vs 124 non-PQ. All callers allocated
  based on old header size. Buffer overflow, heap corruption.
  Fix: ratchet_encrypt reduces padded_msg_len internally by 2222 bytes
  when PQ active. Total output unchanged. No caller changes.

Bug 3 (HIGH): NVS Blob Limit (ESP_ERR_INVALID_SIZE)
  Root cause: ratchet_state_t grew from 520 to 5640 bytes, exceeding
  NVS blob limit (~4000 bytes).
  Fix: RATCHET_CLASSICAL_SIZE = offsetof(ratchet_state_t, pq) saves
  only classical part. PQ fields persisted separately via pq_XX_* keys.

Bug 4 (HIGH): Crypto-Task Result Not Returned
  Root cause: FreeRTOS queue copies pq_request_t by value. Crypto-Task
  writes result to its local copy, never reaches caller.
  Fix: Result passed as pointer. Caller blocks on semaphore, keeping
  pointed memory valid.

Bug 5 (MEDIUM): WiFi Settings Crash (pre-existing)
  Fix: NULL guard on s_wifi_list in rebuild_timer_cb.

Bug 6 (MEDIUM): Ring Buffer Assert on Early Add Contact (pre-existing)
  Fix: NULL guard on app_to_net_buf in smp_request_add_contact.
```

### sntrup761 Performance on ESP32-S3 (240 MHz)

```
keygen:              1839-1940 ms (10x slower than desktop estimates!)
encap:               70 ms
decap:               151-155 ms
Background pre-comp: 1850 ms (hidden from user)
Per direction change: ~225 ms (encap + decap, perceptible)

PQClean "clean" reference: portable C, no optimizations.
Background pre-computation mandatory (was planned as optional).
```

### PQ Wire Format (Byte-Identical to Haskell)

```
Non-PQ header:  88 bytes, KEM = 0x30 (Nothing)
PQ header:      2310 bytes (padded), anti-downgrade

KEM Proposed:   0x31 0x50 [Word16 BE pk_len=1158] [pk 1158 bytes]
KEM Accepted:   0x31 0x41 [Word16 BE ct_len=1039] [ct 1039 bytes]
KEM Nothing:    0x30 (padding stays at 2310 once PQ active)

Anti-downgrade: pq_support transitions Off->On only, never back.
```

### PQ State Machine

```
Fall 1 (Nothing received):
  Generate keypair, set state to proposed.

Fall 2 (Proposed received):
  Encapsulate against peer key.
  Store shared secret in pending_ss (NOT receive-kdf_root!).
  Store ciphertext in pending_ct. Set state to accepted.
  *kem_ss_valid = false.

Fall 3 (Accepted received):
  Decapsulate with own SK (secret into receive-kdf_root).
  Store new peer key. Generate new keypair.
  Encapsulate for next round (new secret into pending_ss).

Send side: pending_ss fed into send-kdf_root on next outgoing step.
```

### Memory Impact

```
Flash:              1.82 MB -> 1.85 MB (+30 KB sntrup761)
PSRAM ratchet:      66 KB -> 722 KB (+656 KB PQ fields)
PSRAM crypto task:  0 -> 80 KB (new, actual usage ~16 KB)
PSRAM free:         8.05 MB -> 7.21 MB
Internal SRAM:      unchanged (only 6 KB free, crypto task in PSRAM)
```

### Security Status: 6/6 ALL CLOSED

```
SEC-01 CLOSED (S45): sodium_memzero on PSRAM cache
SEC-02 CLOSED (S45): HMAC NVS vault, eFuse BLOCK_KEY1
SEC-03 CLOSED (S42): mbedtls_platform_zeroize
SEC-04 CLOSED (S45): Auto-lock 60s + memory wipe
SEC-05 CLOSED (S45): Device-bound HKDF with chip MAC
SEC-06 CLOSED (S46): sntrup761 post-quantum KEM in Double Ratchet
```

### Five Encryption Layers Per Message

```
Layer 1a: X448 Double Ratchet + AES-256-GCM (classical E2E with PFS)
Layer 1b: sntrup761 KEM (hybrid PQ, every ratchet step)
Layer 2:  NaCl cryptobox X25519+XSalsa20+Poly1305 (per-queue)
Layer 3:  NaCl cryptobox server-to-recipient (server traffic)
Layer 4:  TLS 1.3 (transport)
```

### New Lessons Learned (Session 46)

243. **PQClean "clean" sntrup761 keygen on ESP32-S3 takes 1.85 seconds** - 10x slower than desktop benchmarks. Background pre-computation is mandatory, not optional. PQClean avx2/aarch64 variants not portable to Xtensa (Session 46)
244. **ESP32-S3 has only 6 KB free internal SRAM after all tasks** - Any new task with significant stack must use PSRAM. SHA-512 hardware accelerator works from PSRAM stacks (memory-mapped, not DMA). Crypto-Task uses 80 KB PSRAM, actual ~16 KB (Session 46)
245. **When receiving KEM Proposed, encap shared secret must NOT go into receive-kdf_root** - Sender has not used any KEM secret at this point. Secret goes into pending_ss, fed into SEND-kdf_root on next outgoing step. Getting this wrong breaks ALL messaging (Session 46)
246. **PQ header adds 2222 bytes (2346 vs 124)** - ratchet_encrypt must reduce padded_msg_len internally when PQ active to stay within 16 KB SMP block. Internal adjustment cleaner than changing all callers (Session 46)
247. **NVS blob limit is ~4000 bytes** - ratchet_state_t at 5640 bytes exceeds this. Split: classical part via offsetof, PQ fields via separate NVS keys (pq_XX_act, _st, _opk, _osk, _ppk, _ct, _ss). Write-Before-Send at every transition (Session 46)
248. **FreeRTOS queue copies structs by value** - Crypto-Task result via pointer to caller memory, not field in queued struct. Caller blocks on semaphore to keep memory valid (Session 46)
249. **When PQ breaks messaging, do NOT revert the feature** - Diagnose which operation mismatches (which side feeds KEM into kdf_root at which step), fix that specific operation. Reverting loses all progress (Session 46)
250. **Fork critical crypto dependencies** - github.com/saschadaemgen/sntrup761. Upstream tag changes, renames, or deletions cannot break your build. Independent source control for supply chain security (Session 46)

---

*Bug Tracker v42.0*
*Last updated: March 12, 2026 - Session 46 Codename MEGABLAST*
*Total bugs documented: 74 (Bug #22 standby freeze new) + Bug E*
*250 lessons learned*
*Security: 6/6 ALL CLOSED*
*First quantum-resistant message: 2026-03-12, 09:16 CET*
*The first quantum-resistant dedicated hardware messenger in the world.*
