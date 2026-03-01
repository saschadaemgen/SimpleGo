![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 17: Session 20
# Complete Double Ratchet Body Decrypt + Peer Profile JSON

**Document Version:** v34  
**Date:** 2026-02-06 Session 20  
**Status:** Body Decrypt SUCCESS — Peer Profile Read on ESP32  
**Previous:** Part 16 - Session 19 (Header Decrypt SUCCESS)

---

## 355. Session 20 Overview

### 355.1 Starting Point

After Session 19 we had:
- Double Ratchet Header Decrypt SUCCESS
- MsgHeader fully parsed (msgMaxVersion=3, DH Key, PN=0, Ns=0)
- Bug #19 found: header_key_recv overwritten (workaround: saved_nhk)
- Body decrypt not yet implemented
- ConnInfo not yet parsed

### 355.2 Session 20 BREAKTHROUGH

**Complete crypto chain from TLS to JSON on ESP32!**

```
TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
→ ClientMessage → EncRatchetMessage → Header Decrypt (AES-GCM)
→ DH Ratchet Step (2× rootKdf) → Chain KDF → Body Decrypt (AES-GCM)
→ unPad → AgentConnInfo 'I' → Zstd Decompress → Peer-Profil JSON
```

**Peer profile read:** `"displayName": "cannatoshi"` on an ESP32.

### 355.3 Session 20 Achievements

1. ✅ Bug #19 Root Cause found: Debug Self-Decrypt-Test in smp_peer.c:347
2. ✅ Bug #19 FIXED: Debug test removed
3. ✅ DH Ratchet Step implemented (2× rootKdf)
4. ✅ Chain KDF implemented
5. ✅ Body Decrypt SUCCESS (AES-256-GCM, 14832 → 8887 bytes)
6. ✅ unPad on body plaintext
7. ✅ ConnInfo tag 'I' = AgentConnInfo identified
8. ✅ Zstd decompression integrated (8881 → 12268 bytes)
9. ✅ Peer Profile JSON fully parsed
10. ✅ Complete decryption chain working end-to-end

### 355.4 Session 20 Stats

| Metric | Value |
|--------|-------|
| Duration | ~4 hours |
| Claude Code Tasks | 4 |
| Bugs fixed | 1 (Bug #19) |
| New functions | ratchet_decrypt_body(), Zstd Component |
| New code lines | ~400 (Ratchet) + Zstd Library |
| Flash usage Zstd | ~117KB |
| Bytes decrypted | 8887 (Body) → 12268 (JSON) |
| Milestone | First complete Double Ratchet Decrypt on ESP32 |

---

## 356. Bug #19: Root Cause Analysis & Fix

### 356.1 Symptom (from Session 19)

```
header_key_recv after X3DH:    1c08e86e... (correct)
header_key_recv at receipt:    cf0c74d2... (WRONG, overwritten)
```

### 356.2 Claude Code Analysis (Opus 4.6 on SimpleGo Repo)

Task: Find all write accesses to `header_key_recv` in entire codebase.
Result: `smp_peer.c:347` — Debug Self-Decrypt-Test.

### 356.3 Root Cause

After encrypting the AgentConfirmation, a debug test called `ratchet_decrypt()` on
our own encrypted message. This function has **side effects** — it performs a DH
Ratchet Step when the DH key in the decrypted header differs from `dh_peer`.

Since the header contained `dh_self.public_key` (our own key), `dh_changed = true`
triggered a spurious Ratchet Step.

### 356.4 Corrupted Fields (all in smp_ratchet.c:670-677)

| Field | Before | After |
|-------|--------|-------|
| header_key_recv | 1c08e86e... (correct) | cf0c74d2... (wrong) |
| root_key | correct from X3DH | corrupted |
| chain_key_recv | unset | corrupted |
| dh_peer | Peer's Key | our own Key |
| msg_num_recv | 0 | reset to 0 |

### 356.5 What It Was NOT

| Function | Writes to header_key_recv? | Notes |
|----------|---------------------------|-------|
| ratchet_init_sender() | NO | Writes only root_key, chain_key_send, dh_self, dh_peer |
| ratchet_encrypt() | NO | Reads recv keys only, never writes them |
| complete_handshake() | NO | Calls ratchet_encrypt() |
| send_hello_message() | NO | Calls ratchet_encrypt() |

### 356.6 Call Flow (Annotated)

```
send_agent_confirmation():
  [309] ratchet_x3dh_sender()    → header_key_recv = 1c08e86e... ✅
  [317] ratchet_init_sender()    → no change to header_key_recv ✅
  [335] ratchet_encrypt()        → msg #0, no change to recv keys ✅
  [347] ratchet_decrypt() DEBUG  → header_key_recv = cf0c74d2... ❌ BUG!
  [689] complete_handshake()     → ratchet_encrypt() msg #1 (HELLO)
  ... later: ratchet_decrypt() on incoming msg → fails with wrong key
```

### 356.7 Fix Applied

Removed the debug self-decrypt test from `smp_peer.c:343-359`.
Branch: `claude/fix-header-key-recv-bug-DNYeF` → merged to main.

### 356.8 Lesson Learned

**#52: Tests must NEVER modify production state!** Side-effect-free testing
is essential. Debug decrypt tests on own messages trigger spurious DH ratchet
steps because the DH key in the header differs from `dh_peer`.

---

## 357. DH Ratchet Step + Body Decrypt Analysis

### 357.1 Claude Code Analysis (Opus 4.6 on simplexmq Repo)

Task: Analyze complete flow from Header-Decrypt to Body-Decrypt.

### 357.2 Key Findings from Haskell Source

1. **YES, DH Ratchet Step BEFORE chainKdf** — even TWO rootKdf calls:
   - rootKdf #1: peer_new_pub × our_old_priv → recv chain
   - rootKdf #2: peer_new_pub × our_NEW_priv → send chain

2. **16-byte IV** for AES-GCM confirmed (SimpleX-specific, GHASH internal)

3. **rcAD = raw X448 keys** (56+56=112 bytes, no ASN.1), sender first

4. **chainKdf salt = ""** (empty!)

5. **iv1 = Body-IV, iv2 = Header-IV** (during decrypt, iv2 is ignored)

### 357.3 Haskell Source References

| Function | File | Lines |
|----------|------|-------|
| rcDecrypt | Ratchet.hs | 990-1010 |
| ratchetStep | Ratchet.hs | 1043-1071 |
| rootKdf | Ratchet.hs | 1159-1166 |
| chainKdf | Ratchet.hs | 1168-1172 |
| decryptAEAD | Crypto.hs | 1035-1038 |
| initAEAD (16B IV) | Crypto.hs | 1115-1120 |
| unPad | Crypto.hs | 1067-1073 |

### 357.4 Erkenntnis: iv1 vs iv2 Assignment for Decrypt

**CRITICAL CORRECTION from Session 4/8 understanding:**

Chain KDF output layout:
```
[0-31]   next_chain_key
[32-63]  message_key
[64-79]  iv1 = BODY IV (used for body decrypt!)
[80-95]  iv2 = HEADER IV (ignored during decrypt, used only for encrypt)
```

During **decrypt**, the header IV comes from `ehIV` in the EncMessageHeader,
not from the Chain KDF. The Chain KDF's `iv1` is the **body** IV.

---

## 358. Body Decrypt Implementation

### 358.1 Complete 6-Step Process

**Step 1 — DH Ratchet Step Recv:**
```
dh_secret_recv = X448_DH(peer_new_pub, our_old_priv)  // 56 bytes
HKDF_SHA512(salt=root_key, ikm=dh_secret_recv, info="SimpleXRootRatchet", len=96)
→ new_root_key_1 [0-31], recv_chain_key [32-63], new_nhk_recv [64-95]
```

**Step 2 — DH Ratchet Step Send:**
```
(our_new_priv, our_new_pub) = X448_GENERATE_KEYPAIR()
dh_secret_send = X448_DH(peer_new_pub, our_new_priv)  // 56 bytes
HKDF_SHA512(salt=new_root_key_1, ikm=dh_secret_send, info="SimpleXRootRatchet", len=96)
→ new_root_key_2 [0-31], send_chain_key [32-63], new_nhk_send [64-95]
```

**Step 3 — Chain KDF:**
```
HKDF_SHA512(salt="", ikm=recv_chain_key, info="SimpleXChainRatchet", len=96)
→ next_recv_ck [0-31], message_key [32-63], iv_body [64-79], ignored [80-95]
```

**Step 4 — AES-256-GCM Decrypt:**
```
AAD = rcAD[112] || emHeader[raw 123 bytes]
AES256_GCM_DECRYPT(key=message_key, iv=iv_body[16], aad=AAD, ct=emBody, tag=emAuthTag)
```

**Step 5 — unPad:**
```
msg_len = BE_uint16(decrypted[0..1])
plaintext = decrypted[2 .. 2+msg_len-1]
```

**Step 6 — State Update (LOG ONLY, not activated)**

### 358.2 Verified Intermediate Values

```
root_key:        b0d3fd0e76379553d10718617a973bc69a289c8381ff608f7d1057f292df90dd
dh_secret_recv:  9a66056fff2882bb4690a098ca000b8ac69a0283790ffbfbbb630c20ba3061b1...
new_root_key_1:  82190a059a10b8097355b6a612a1ef21a18b0f46c5ed4c8e066f9c97b90d1e97
recv_chain_key:  747dcc01aa665f0d85295950fdbc4b2fa398cd90615a8f9259efd62ba6318ef5
message_key:     ea8461db5d92ce9f70474bae4d241bca2a99d87cac4ccd48d0af177019b8d44d
iv_body:         a187e7d0636a7e54902a607b05dfbdd8
```

### 358.3 Result

Body Decrypt SUCCESS! 14832 bytes ciphertext → 8887 bytes plaintext (after unPad).

### 358.4 AAD Construction for Body Decrypt

```
Payload AAD = rcAD || emHeader (raw)
            = 112 bytes + 123 bytes = 235 bytes total

rcAD:     our_key1[56] || peer_key1[56]  (raw X448, no ASN.1)
emHeader: [ehVersion 2B][ehIV 16B][ehAuthTag 16B][ehBodyLen 1B][ehBody 88B] = 123 bytes
```

**IMPORTANT:** AAD uses the raw emHeader bytes as received, NOT re-serialized.

---

## 359. ConnInfo Tag Analysis

### 359.1 Initial Observation

First byte after body decrypt + unPad: `0x49` = `'I'`

Expected: `'D'` (AgentConnInfoReply with SMP Queues)
Actual: `'I'` (AgentConnInfo)

### 359.2 Claude Code Analysis (Opus 4.6 on simplex-chat Repo)

**Result:** `'I'` is CORRECT!

| Tag | Constructor | Who sends | Content |
|-----|------------|-----------|---------|
| 'I' | AgentConnInfo | Initiator (or Joiner on Reply Queue) | ConnInfo only (Profile) |
| 'D' | AgentConnInfoReply | Joiner (on Contact Queue) | Reply Queues + ConnInfo |

### 359.3 Encoding

```
AgentConnInfo:      'I' <Tail connInfo>
AgentConnInfoReply: 'D' <smpQueues> <Tail connInfo>
```

After the `'I'` tag, the rest is directly the ConnInfo (no length prefix, Tail encoding).

### 359.4 Handshake Flow (Verified)

```
App (Joiner)      →  Contact Queue: 'D' (Queues + Profile)  →  ESP32 (Initiator)
ESP32 (Initiator) →  Reply Queue: 'C'+'I' (our Profile)     →  App (Joiner)
App (Joiner)      →  Reply Queue: 'C'+'I' (their Profile)   →  ESP32 ← WE SEE THIS
App (Joiner)      →  Reply Queue: HELLO                     →  ESP32 (not yet processed)
```

### 359.5 Lesson Learned

**#53: Understand roles!** Initiator sends `'I'` (AgentConnInfo), Joiner sends `'D'`
(AgentConnInfoReply). On the Reply Queue, the Joiner's AgentConfirmation contains
`'I'` (just profile), NOT `'D'` (queues + profile).

---

## 360. Zstd Decompression

### 360.1 ConnInfo Byte Layout After 'I' Tag

```
Offset  Hex    Meaning
0       49     'I' — AgentConnInfo Tag
1       58     'X' — Compressed batch marker
2       01     1 Item in NonEmpty list
3       31     '1' — Compressed (not Passthrough)
4-5     22 b1  BE uint16 = 8881 bytes Zstd data
6-9     28 b5  Zstd Frame Magic (little-endian: 0xFD2FB528)
        2f fd
10+     ...    Zstd compressed data
```

### 360.2 Claude Code Analysis (Opus 4.6 on simplex-chat Repo)

- `'X'` triggers `decodeCompressed` in Protocol.hs:698-724
- NonEmpty Compressed: `'0'` = Passthrough (≤180 bytes), `'1'` = Zstd compressed
- No dictionary, standard Zstd Level 3
- Max decompressed: 65,536 bytes

### 360.3 Compressed Encoding Reference

```
ConnInfo = 'I' <compressed_batch>

compressed_batch:
  'X'                           — Compressed marker
  <Word16 BE count>             — NonEmpty list count
  For each item:
    '0' <Tail data>             — Passthrough (≤180 bytes, no compression)
    '1' <Word16 BE len> <data>  — Zstd compressed
```

### 360.4 Zstd Library Integration

- Built `components/zstd/` as ESP-IDF Component
- Amalgamated `zstd.c` v1.5.5 (Decompress-Only)
- ~117KB Flash after linker gc-sections
- Multithread disabled, error strings enabled

### 360.5 Result

8881 bytes Zstd → 12,268 bytes JSON successfully decompressed!

### 360.6 Lesson Learned

**#54: Check for `'X'`=0x58 marker, look for Zstd magic `28 b5 2f fd`!**
ConnInfo can be compressed with Zstd. Items tagged `'1'` need decompression,
items tagged `'0'` are passthrough.

---

## 361. Peer Profile JSON

### 361.1 Decompressed Content

```json
{
  "v": "1-16",
  "event": "x.info",
  "params": {
    "profile": {
      "displayName": "cannatoshi",
      "fullName": "",
      "shortDescr": "Independent Security Researcher & FOSS Developer 💫",
      "image": "data:image/jpg;base64,/9j/4AAQ...",
      "contactLink": "https://smp10.simplex.im/a#N1ych...",
      "preferences": {
        "calls": {"allow": "no"},
        "files": {"allow": "always"},
        "reactions": {"allow": "yes"},
        "voice": {"allow": "yes"},
        "timedMessages": {"allow": "yes"},
        "sessions": {"allow": "no"},
        "fullDelete": {"allow": "no"}
      }
    }
  }
}
```

### 361.2 Field Analysis

| Field | Value | Meaning |
|-------|-------|---------|
| v | "1-16" | Chat protocol version range |
| event | "x.info" | XInfo Profile (peer profile exchange) |
| displayName | "cannatoshi" | User's display name |
| fullName | "" | Optional full name (empty) |
| shortDescr | "Independent Security..." | Profile description |
| image | "data:image/jpg;base64,..." | Base64-encoded JPEG profile picture |
| contactLink | "https://smp10.simplex.im/..." | Peer's own SimpleX contact link |
| preferences | {...} | Chat feature preferences |

### 361.3 Significance

This is the **first time** a peer's SimpleX profile has been read on an ESP32 device.
The complete decryption chain from TLS 1.3 through 8 layers of crypto to readable
JSON demonstrates that the SimpleGo implementation correctly handles the entire
SimpleX protocol stack.

---

## 362. Session 20 Erkenntnisse (Key Discoveries)

### 362.1 Erkenntnis 1: Debug Tests With Side Effects (Bug #19)

`ratchet_decrypt()` has side effects: it performs a DH Ratchet Step when
`dh_changed = true`. Calling it on our own encrypted message triggers a spurious
ratchet step because `dh_self.public_key != dh_peer`.

**Rule:** Never use production decrypt functions for debug self-tests.

### 362.2 Erkenntnis 2: Two rootKdf Calls Per DH Ratchet Step

The DH Ratchet Step requires TWO rootKdf calls, not one:
1. rootKdf #1: `peer_new_pub × our_old_priv` → recv chain + new_root_key_1
2. rootKdf #2: `peer_new_pub × our_NEW_priv` → send chain + new_root_key_2

A new sending keypair is generated between the two calls.

### 362.3 Erkenntnis 3: iv1 = Body IV, iv2 = Header IV

Chain KDF output `[64-79]` is iv1 = **body** IV (not header IV!).
During decrypt, the header IV comes from `ehIV` in the wire format.
iv2 `[80-95]` is only used during encrypt for header encryption.

### 362.4 Erkenntnis 4: ConnInfo Tags 'I' vs 'D'

| Tag | Constructor | Role | Content |
|-----|------------|------|---------|
| 'I' | AgentConnInfo | Any sender on Reply Queue | Profile only |
| 'D' | AgentConnInfoReply | Joiner on Contact Queue | SMP Queues + Profile |

### 362.5 Erkenntnis 5: Zstd Compression in ConnInfo

ConnInfo is wrapped in a compressed batch format:
- `'X'` marker indicates compressed batch
- NonEmpty list with `'0'`=passthrough, `'1'`=Zstd compressed
- Standard Zstd Level 3, no dictionary
- Max decompressed size: 65,536 bytes

### 362.6 Erkenntnis 6: rcAD Uses Raw X448 Keys

rcAD (Associated Data for Double Ratchet) uses raw 56-byte X448 keys,
NOT ASN.1/SPKI-encoded keys. Sender's key first: `sender_key1[56] || receiver_key1[56]`.

### 362.7 Erkenntnis 7: Body AAD = rcAD || emHeader (Raw)

The body decrypt AAD is the concatenation of rcAD (112 bytes) and the raw
emHeader bytes as received (123 bytes) = 235 bytes total.
The emHeader is NOT re-serialized — use the exact bytes from the wire.

---

## 363. Verified Byte-Map (Body Decrypt Chain)

### 363.1 DH Ratchet Step Inputs

```
our_old_priv:     ratchet_state.dh_self.private_key (56 bytes X448)
peer_new_pub:     from MsgHeader DH Key (56 bytes raw, extracted from 68B SPKI)
root_key:         from X3DH HKDF[64-95] (32 bytes)
```

### 363.2 DH Ratchet Step Recv (rootKdf #1)

```
Salt:   root_key (32 bytes)
IKM:    X448_DH(peer_new_pub, our_old_priv) = 56 bytes
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   new_root_key_1:   82190a05...
  [32-63]  recv_chain_key:   747dcc01...
  [64-95]  new_nhk_recv:     (saved for next header)
```

### 363.3 DH Ratchet Step Send (rootKdf #2)

```
Salt:   new_root_key_1 (32 bytes)
IKM:    X448_DH(peer_new_pub, our_NEW_priv) = 56 bytes
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   new_root_key_2:   (new root key)
  [32-63]  send_chain_key:   (for our next encrypt)
  [64-95]  new_nhk_send:     (for our next header encrypt)
```

### 363.4 Chain KDF

```
Salt:   "" (empty!)
IKM:    recv_chain_key (32 bytes)
Info:   "SimpleXChainRatchet"
Output: 96 bytes
  [0-31]   next_recv_ck:     (next chain key)
  [32-63]  message_key:      ea8461db...
  [64-79]  iv_body:          a187e7d0636a7e54902a607b05dfbdd8
  [80-95]  iv_header:        (ignored during decrypt)
```

### 363.5 Body AES-GCM Decrypt

```
Key:        message_key (32 bytes)
IV:         iv_body (16 bytes)
AAD:        rcAD[112] || emHeader[123] = 235 bytes
Ciphertext: emBody (14832 bytes)
AuthTag:    emAuthTag (16 bytes)
Output:     8889 bytes (with 2-byte unPad prefix)
After unPad: 8887 bytes plaintext
```

### 363.6 ConnInfo Parse

```
Offset  Hex    Field                         Status
[0]     49     'I' — AgentConnInfo Tag       ✅
[1]     58     'X' — Compressed marker       ✅
[2]     01     NonEmpty count: 1             ✅
[3]     31     '1' — Zstd compressed         ✅
[4-5]   22 b1  Zstd data length: 8881       ✅
[6-8886]       Zstd compressed data          ✅

After Zstd decompress: 12268 bytes JSON
```

---

## 364. Complete Decryption Chain (Updated Session 20)

```
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓ Output: [2B len prefix][ClientMsgEnvelope][padding 0x23...]
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working (S18)
  ↓ Output: 15904 bytes (padded)
  ↓
Layer 2.5: unPad                                               ✅ Working (S19)
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
  ↓ Key: header_key_recv (nhk from X3DH HKDF[32-63])
  ↓ IV: ehIV (16 bytes)
  ↓ AAD: rcAD (112 bytes = our_key1 || peer_key1)
  ↓ Output: MsgHeader (79 bytes content + 9 bytes header/padding)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ✅ Working (S20)
  ↓ DH Ratchet Step: 2× rootKdf → recv_chain_key
  ↓ Chain KDF: → message_key + iv_body
  ↓ AES-256-GCM: key=message_key, iv=iv_body, AAD=rcAD||emHeader
  ↓ Output: 8889 bytes → unPad → 8887 bytes plaintext
  ↓
Layer 7: ConnInfo Parse                                        ✅ Working (S20)
  ↓ Tag: 'I' = AgentConnInfo (peer profile)
  ↓ 'X' compressed batch → Zstd decompress
  ↓ Output: 12268 bytes JSON
  ↓
Layer 8: Peer Profile                                          ✅ Working (S20)
  ↓ event: "x.info" — XInfo Profile
  ↓ displayName: "cannatoshi"
  ↓ Full profile with image, preferences, contact link
  ↓
Layer 9: Connection Established                                ⏳ Next Step
  ↓ Need: Process HELLO message
  ↓ Need: Send HELLO back
  ↓ Need: Bidirectional messaging
```

---

## 365. HKDF Chain Reference (Updated Session 20)

### 365.1 Complete HKDF Chain (All Steps Verified)

```
HKDF #1: X3DH → (hk, nhk, sk)                                ✅ S19
  Salt:  64 × 0x00
  IKM:   DH1 || DH2 || DH3 (168 Bytes for X448)
  Info:  "SimpleXX3DH"
  Output: hk[0-31], nhk[32-63], sk[64-95]

HKDF #2: Root KDF Recv → (rk1, ck_recv, nhk_recv)             ✅ S20 NEW
  Salt:  sk (32 Bytes, Root Key from X3DH)
  IKM:   DH(peer_new_pub, our_old_priv) [56 Bytes X448]
  Info:  "SimpleXRootRatchet"
  Output: rk1[0-31], ck_recv[32-63], nhk_recv[64-95]

HKDF #3: Root KDF Send → (rk2, ck_send, nhk_send)             ✅ S20 NEW
  Salt:  rk1 (32 Bytes, from HKDF #2)
  IKM:   DH(peer_new_pub, our_NEW_priv) [56 Bytes X448]
  Info:  "SimpleXRootRatchet"
  Output: rk2[0-31], ck_send[32-63], nhk_send[64-95]

HKDF #4: Chain KDF Recv → (ck', mk, iv1, iv2)                 ✅ S20 NEW
  Salt:  "" (empty!)
  IKM:   ck_recv (32 Bytes, from HKDF #2)
  Info:  "SimpleXChainRatchet"
  Output: ck'[0-31], mk[32-63], iv_body[64-79], iv_header[80-95]
```

### 365.2 Key Assignment Summary (Updated)

| HKDF # | Output | Bytes | Name | Usage | Verified |
|--------|--------|-------|------|-------|----------|
| X3DH #1 | Block 1 | [0-31] | hk | Peer decrypts our headers | S19 |
| X3DH #1 | Block 2 | [32-63] | nhk | WE decrypt peer's headers | S19 |
| X3DH #1 | Block 3 | [64-95] | sk | Input for Root KDF | S19 |
| Root #2 | Block 1 | [0-31] | rk1 | Input for Root KDF Send | S20 |
| Root #2 | Block 2 | [32-63] | ck_recv | Recv chain key | S20 |
| Root #2 | Block 3 | [64-95] | nhk_recv | Next header key recv | S20 |
| Root #3 | Block 1 | [0-31] | rk2 | New root key | S20 |
| Root #3 | Block 2 | [32-63] | ck_send | Send chain key | S20 |
| Root #3 | Block 3 | [64-95] | nhk_send | Next header key send | S20 |
| Chain #4 | Block 1 | [0-31] | ck' | Next recv chain key | S20 |
| Chain #4 | Block 2 | [32-63] | mk | Message key (body decrypt) | S20 |
| Chain #4 | Block 3a | [64-79] | iv1 | **Body IV** (NOT header!) | S20 |
| Chain #4 | Block 3b | [80-95] | iv2 | Header IV (ignored in decrypt) | S20 |

---

## 366. Claude Code Analyses (Session 20)

| # | Repo | Task | Result |
|---|------|------|--------|
| 1 | SimpleGo | Bug #19 Root Cause | Self-Decrypt-Test in smp_peer.c:347 |
| 2 | simplexmq | DH Ratchet + Body Decrypt Flow | Complete 6-step pseudo-code |
| 3 | simplex-chat | ConnInfo Tag 'I' meaning | AgentConnInfo = Initiator Profile |
| 4 | simplex-chat | Zstd ConnInfo format | 'X' + NonEmpty Compressed layout |

---

## 367. Files Changed (Session 20)

| File | Change |
|------|--------|
| main/include/smp_ratchet.h | `ratchet_decrypt_body()` declaration |
| main/smp_ratchet.c | ~300 lines Body-Decrypt implementation |
| main/smp_peer.c | Removed debug self-decrypt test (Bug #19 fix) |
| main/main.c | Phase 2b block + Zstd integration + `#include "zstd.h"` |
| main/CMakeLists.txt | `zstd` added to REQUIRES |
| components/zstd/CMakeLists.txt | **NEW** — ESP-IDF Component |
| components/zstd/zstd.c | **NEW** — Amalgamated library v1.5.5 |
| components/zstd/include/zstd.h | **NEW** — Public API header |
| components/zstd/include/zstd_errors.h | **NEW** — Error codes |
| components/zstd/LICENSE | **NEW** — BSD + GPLv2 |
| docs/SIMPLEX_VS_MATRIX.md | Updated |

---

## 368. Session 20 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-06 | Bug #19 Root Cause found | Debug self-decrypt in smp_peer.c:347 |
| 2026-02-06 | Bug #19 FIXED | Debug test removed, merged to main |
| 2026-02-06 | DH Ratchet analysis | 2× rootKdf + chainKdf documented |
| 2026-02-06 | ratchet_decrypt_body() | ~300 lines implementation |
| 2026-02-06 | Body Decrypt SUCCESS | 14832 → 8887 bytes |
| 2026-02-06 | ConnInfo 'I' tag identified | AgentConnInfo = profile only |
| 2026-02-06 | Zstd component integrated | v1.5.5, ~117KB Flash |
| 2026-02-06 | Zstd decompress SUCCESS | 8881 → 12268 bytes JSON |
| 2026-02-06 | Peer profile read | displayName: "cannatoshi" |

---

## 369. Next Steps (Session 21)

### 369.1 Step 1: Process HELLO Message
- Receive peer's HELLO on Reply Queue
- HELLO = next Double Ratchet message (Ns=1 or new DH ratchet)
- Decrypt using updated ratchet state

### 369.2 Step 2: Ratchet State Persistence
- Activate state update in ratchet_decrypt_body() (currently LOG ONLY)
- Persist ratchet state for subsequent messages

### 369.3 Step 3: Send HELLO Response
- Encrypt HELLO message using send chain
- Send on Reply Queue
- Connection status → "Connected"

### 369.4 Step 4: Bidirectional Messaging
- Implement message send/receive loop
- Handle multiple DH ratchet steps
- Support message ordering (PN, Ns counters)

### 369.5 Step 5: UI Integration
- Display peer profile on T-Deck screen
- Show connection status
- Enable message composition

---

## 370. Session 20 Summary

### What Was Achieved

- **Bug #19 FIXED:** Debug self-decrypt test removed, root cause: side effects in ratchet_decrypt()
- **DH Ratchet Step implemented:** 2× rootKdf (recv + send chains)
- **Body Decrypt SUCCESS:** 14832 → 8887 bytes via AES-256-GCM
- **ConnInfo parsed:** Tag 'I' = AgentConnInfo (profile only)
- **Zstd integrated:** 8881 → 12268 bytes decompressed
- **Peer Profile read:** `"displayName": "cannatoshi"` on ESP32
- **Complete crypto chain:** TLS 1.3 through 8 layers to readable JSON

### What Was NOT Achieved (Deferred to Session 21)

- HELLO message processing
- Ratchet state persistence (state update is LOG ONLY)
- Bidirectional messaging
- UI integration

### Key Takeaway

```
SESSION 20 SUMMARY:
  - Bug #19: Debug self-decrypt corrupted ratchet state → removed
  - DH Ratchet Step: 2× rootKdf (recv chain + send chain)
  - iv1 = Body IV, iv2 = Header IV (CORRECTION from earlier sessions)
  - ConnInfo: 'I' = AgentConnInfo (profile), 'D' = AgentConnInfoReply (queues+profile)
  - Zstd: 'X' marker, '1'=compressed, '0'=passthrough
  - Complete chain: TLS → SMP → E2E → Ratchet → Zstd → JSON

"TLS to JSON in one session."
"displayName: cannatoshi — read on an ESP32."
"Evidence before fix. Always." 🐭
```

---

**DOCUMENT CREATED: 2026-02-06 Session 20 v34**  
**Status: Body Decrypt SUCCESS! Peer Profile Read on ESP32!**  
**Key Achievement: Complete crypto chain TLS → JSON working end-to-end**  
**Next: HELLO processing, Ratchet State Persistence, Bidirectional Messaging**
