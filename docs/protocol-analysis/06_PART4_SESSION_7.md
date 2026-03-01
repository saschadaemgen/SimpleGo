![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# ═══════════════════════════════════════════════════════════════════
# SESSION 4 FINALE - 2026-01-24 - COMPLETE WIRE FORMAT!
# ═══════════════════════════════════════════════════════════════════

---

## 85. Session 4 Finale Overview (2026-01-24)

### 85.1 Summary of all fixes

In this session **8 critical bugs** were found and fixed:

| # | Bug | Fix |
|---|-----|-----|
| 1 | KDF Output Order | new_root, chain, next_header |
| 2 | E2E Key Length Prefix | 2 bytes Word16 BE |
| 3 | HELLO prevMsgHash Length | 2 bytes Word16 BE |
| 4 | MsgHeader DH Key Length | 2 bytes Word16 BE |
| 5 | ehBody Length Prefix | 2 bytes Word16 BE |
| 6 | emHeader Length Prefix | 2 bytes Word16 BE |
| 7 | Payload AAD Length | 236 bytes (not 235) |
| 8 | ChainKDF IV Order | header_iv, then msg_iv |

---

## 86. Fix 3: HELLO prevMsgHash Length - Word16 (2026-01-24 Finale)

### 86.1 🔥 BUG: prevMsgHash Length was 1 byte instead of 2 bytes

**Problem:** The length of prevMsgHash was encoded as 1 byte.

### 86.2 Before (WRONG - 11 bytes HELLO)

```c
// prevMsgHash length as 1 byte
output[p++] = 0;  // ❌ Only 1 byte!
```

### 86.3 After (CORRECT - 12 bytes HELLO)

```c
// prevMsgHash length as Word16 Big-Endian (2 bytes)
output[p++] = 0x00;  // High byte
output[p++] = 0x00;  // Low byte = 0 (no hash)
```

### 86.4 HELLO Plaintext (12 bytes, corrected)

```
Offset:  0  1  2  3  4  5  6  7  8  9  10 11
Hex:    4d 00 00 00 00 00 00 00 01 00 00 48
        ─┬ ─────────────┬───────── ──┬── ─┬
         │              │            │    │
        'M'        msgId=1       len=0   'H'
        Tag     (Int64 BE)     (Word16) HELLO
```

---

## 89. 🔥 Fix 8: ChainKDF IV Order - CRITICAL! (2026-01-24 Finale)

### 89.1 🔥🔥 BUG: Header IV and Message IV were SWAPPED!

**Problem:** The IVs from ChainKDF were used in wrong order!

### 89.2 Haskell Reference

```haskell
-- chainKdf Output:
(ck', mk, iv1, iv2) = hkdf4 ck "SimpleXChainRatchet"
-- bytes 0-31:  ck'  = next chain key
-- bytes 32-63: mk   = message key
-- bytes 64-79: iv1  = HEADER IV (for emHeader encryption!)
-- bytes 80-95: iv2  = MESSAGE IV (for payload encryption!)
```

### 89.3 Before (WRONG - swapped!)

```c
// IVs from ChainKDF
memcpy(msg_iv, kdf_output + 64, 16);     // ❌ WRONG! That's iv1 = header_iv!
memcpy(header_iv, kdf_output + 80, 16);  // ❌ WRONG! That's iv2 = msg_iv!
```

**Effect:** Header was encrypted with message IV and vice versa!

### 89.4 After (CORRECT)

```c
// IVs from ChainKDF - CORRECT ORDER
memcpy(header_iv, kdf_output + 64, 16);  // ✅ iv1 = Header IV (bytes 64-79)
memcpy(msg_iv, kdf_output + 80, 16);     // ✅ iv2 = Message IV (bytes 80-95)
```

---

## 91. HKDF Key Derivation - Fully documented (2026-01-24 Finale)

### 91.1 X3DH Initial KDF

```
X3DH Key Derivation:
═══════════════════════════════════════════════════════════════════

Input:
├── Salt: 64 zero bytes
├── IKM: DH1 || DH2 || DH3 (56 + 56 + 56 = 168 bytes)
├── Info: "SimpleXX3DH" (11 bytes)
└── Hash: SHA512

Output (96 bytes):
├── bytes 0-31:  header_key_send (hk)   → for Header Encryption
├── bytes 32-63: header_key_recv (nhk)  → next header key
└── bytes 64-95: root_key (sk)          → for Root KDF

═══════════════════════════════════════════════════════════════════
```

### 91.2 Root KDF (Ratchet Step)

```
Root KDF:
═══════════════════════════════════════════════════════════════════

Input:
├── Salt: current root_key (32 bytes)
├── IKM: DH output (56 bytes)
├── Info: "SimpleXRootRatchet" (18 bytes)
└── Hash: SHA512

Output (96 bytes):
├── bytes 0-31:  NEW root_key        → becomes new root_key
├── bytes 32-63: chain_key           → for Chain KDF
└── bytes 64-95: next_header_key     → for next ratchet step

═══════════════════════════════════════════════════════════════════
```

### 91.3 Chain KDF (Message Key Derivation)

```
Chain KDF:
═══════════════════════════════════════════════════════════════════

Input:
├── Salt: EMPTY (0 bytes) ← Important!
├── IKM: chain_key (32 bytes)
├── Info: "SimpleXChainRatchet" (19 bytes)
└── Hash: SHA512

Output (96 bytes):
├── bytes 0-31:  next_chain_key      → becomes new chain_key
├── bytes 32-63: message_key         → AES-256 key for payload
├── bytes 64-79: header_iv (iv1)     → 16 bytes for Header AES-GCM
└── bytes 80-95: message_iv (iv2)    → 16 bytes for Payload AES-GCM

═══════════════════════════════════════════════════════════════════
```

---

# ═══════════════════════════════════════════════════════════════════
# SESSION 5 - 2026-01-24 - CRYPTOGRAPHY VERIFIED! 🎉
# ═══════════════════════════════════════════════════════════════════

---

## 107. Session 5 Overview - Cryptography solved! (2026-01-24)

### 107.1 BREAKTHROUGH: wolfSSL X448 Byte Order Bug fixed and verified!

**The fix was implemented and ALL crypto values now match Python!**

### 107.2 The implemented fix (smp_x448.c)

```c
// Helper function to reverse byte order
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

// In x448_generate_keypair(): After export reverse keys
reverse_bytes(pub_tmp, keypair->public_key, 56);
reverse_bytes(priv_tmp, keypair->private_key, 56);

// In x448_dh(): Before import reverse keys, after DH reverse output
reverse_bytes(their_public, their_public_rev, 56);
reverse_bytes(my_private, my_private_rev, 56);
// ... execute DH ...
reverse_bytes(secret_tmp, shared_secret, 56);
```

---

## 108. Crypto Verification - Python vs ESP32 (2026-01-24)

### 108.1 🎉 ALL values match!

```
Python:                              ESP32:
═══════════════════════════════════════════════════════════════════

X3DH DH Outputs:
  dh1: 62413115799d7f0a...          62413115799d7f0a... ✅ MATCH!
  dh2: 27d885f054cc7775...          27d885f054cc7775... ✅ MATCH!
  dh3: 8dd161101f1c730f...          8dd161101f1c730f... ✅ MATCH!

X3DH HKDF Output (96 bytes):
  hk:  c65dc5381323839f...          c65dc5381323839f... ✅ MATCH!
  rk:  8b30f093a3b5d75b...          8b30f093a3b5d75b... ✅ MATCH!

Root KDF Output (96 bytes):
  new_rk:  de394bc567ae2e70...      de394bc567ae2e70... ✅ MATCH!
  ck:      5d473bb5b24acc9d...      5d473bb5b24acc9d... ✅ MATCH!
  next_hk: d3d8fbb361ea2e65...      d3d8fbb361ea2e65... ✅ MATCH!

Chain KDF Output (96 bytes):
  mk:        7041ce31dc681820...    7041ce31dc681820... ✅ MATCH!
  header_iv: 708dee3b187dd7ec...    708dee3b187dd7ec... ✅ MATCH!
  msg_iv:    e3b28a0d3df93e3c...    e3b28a0d3df93e3c... ✅ MATCH!

═══════════════════════════════════════════════════════════════════
```

### 108.2 What this means

**The entire cryptography is now CORRECT:**
- ✅ X448 DH (with byte reversal fix)
- ✅ X3DH Key Agreement (all 3 DHs)
- ✅ HKDF-SHA512 for X3DH
- ✅ HKDF-SHA512 for Root KDF
- ✅ HKDF-SHA512 for Chain KDF
- ✅ All keys correctly derived
- ✅ All IVs correctly derived

---

## 152. Lessons Learned - Complete Overview (Sessions 1-6)

### 152.1 Technical Insights

| # | Insight |
|---|---------|
| 1 | **Haskell uses Word16 BE for ALL ByteString lengths** |
| 2 | **"Tail" types have NO length prefix** |
| 3 | **Maybe Nothing ≠ '0'** for all fields (context-dependent!) |
| 4 | **wolfSSL X448 has different byte order than cryptonite** |
| 5 | **Python comparison tests are essential for crypto** |
| 6 | **Space (0x20) is NOT a length prefix!** |

### 152.2 Debug Methodology

```
Successful Debug Strategy:
═══════════════════════════════════════════════════════════════════

1. Haskell Source as Reference
   └── Encoding rules directly from the code

2. Python as Crypto Reference
   └── Byte-by-byte comparison

3. Systematic Hex Dumps
   └── Document every offset

4. Incremental Fixes
   └── One bug at a time

5. Documentation
   └── 180 sections, 8600+ lines!

═══════════════════════════════════════════════════════════════════
```

---

---

## 131. 🔥 Bug 10: SMPQueueInfo Port Encoding (2026-01-24 S6)

### 131.1 The Problem

**Space (0x20) was used instead of length prefix for port string!**

### 131.2 Before (WRONG)

```c
// In queue_encode_info() - smp_queue.c
buf[p++] = ' ';  // Space character (0x20) - WRONG!
memcpy(&buf[p], port_str, port_len);
```

**Result:** Port was encoded as ` 5223` (Space + String) instead of `\x045223` (Length + String)

### 131.3 After (CORRECT)

```c
// In queue_encode_info() - smp_queue.c
buf[p++] = (uint8_t)port_len;  // Length prefix
memcpy(&buf[p], port_str, port_len);
```

**Result:** Port is encoded as `\x045223` (Length=4, then "5223")

### 131.4 Hex Comparison

```
BEFORE (WRONG):
... 16 [22B host] 20 35 32 32 33 ...
                  ^^ 
                  Space (0x20) instead of Length!

AFTER (CORRECT):
... 16 [22B host] 04 35 32 32 33 ...
                  ^^
                  Length=4 ✅
```

---

## 132. Haskell Source Analysis - Wire Format VERIFIED (2026-01-24 S6)

### 132.1 EncRatchetMessage Encoding (confirmed!)

```haskell
encodeEncRatchetMessage v EncRatchetMessage {emHeader, emBody, emAuthTag} =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)
```

**Breakdown:**
| Field | Encoding | Status |
|-------|----------|--------|
| `emHeader` | Word16 BE length + data | ✅ |
| `emAuthTag` | 16 bytes RAW (no length prefix!) | ✅ |
| `emBody` | Tail = no length prefix | ✅ |

**Our format matches 100%!**

### 132.2 EncMessageHeader Encoding (confirmed!)

```haskell
smpEncode EncMessageHeader {ehVersion, ehIV, ehAuthTag, ehBody} =
  smpEncode (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody
```

**Breakdown:**
| Field | Encoding | Status |
|-------|----------|--------|
| `ehVersion` | Word16 BE | ✅ |
| `ehIV` | 16 bytes RAW | ✅ |
| `ehAuthTag` | 16 bytes RAW | ✅ |
| `ehBody` | Word16 BE length + data | ✅ |

**Our format matches 100%!**

---

## 133. Version-dependent Encoding (2026-01-24 S6)

### 133.1 E2E Version Threshold

```haskell
pqRatchetE2EEncryptVersion = VersionE2E 3
```

### 133.2 Version 2 (what we send)

```
E2E Params: (v, k1, k2)              ← NO KEM ✅
MsgHeader:  (msgMaxVersion, msgDHRs, msgPN, msgNs)  ← NO msgKEM ✅
encodeLarge: Standard ByteString with Word16 length ✅
```

### 133.3 Version 3+ (PQ-enabled)

```
E2E Params: (v, k1, k2, kem_)        ← WITH KEM
MsgHeader:  (msgMaxVersion, msgDHRs, msgKEM, msgPN, msgNs)  ← WITH msgKEM
encodeLarge: Large wrapper
```

### 133.4 Important Insight

**Version 2 is CORRECT for non-PQ communication!**

We don't need to send a KEM key when using Version 2.

---

## 141. Session 6 FINALE Overview (2026-01-24)

### 141.1 Two more bugs found and fixed!

| # | Bug | File | Fix |
|---|-----|------|-----|
| 11 | smpQueues List Length | smp_peer.c | 1 byte → Word16 BE |
| 12 | queueMode for Nothing | smp_queue.c | '0' → nothing |

### 141.2 Status

```
ALL KNOWN ENCODING ERRORS FIXED!
═══════════════════════════════════════════════════════════════════

✅ Server accepts AgentConfirmation ("OK")
✅ Server accepts HELLO ("OK")
✅ Wire Formats verified against Haskell Source
✅ Crypto values verified against Python (Session 5)
✅ 12 Bugs found and fixed!

❌ App still shows "error agent AGENT A_MESSAGE"
❌ Connection is NOT activated

THE PROBLEM PERSISTS DESPITE EVERYTHING!

═══════════════════════════════════════════════════════════════════
```

---

## 142. Bug 11: smpQueues List Length (2026-01-24 S6 Finale)

### 142.1 The Problem

**Queue list count was 1 byte instead of Word16 BE!**

### 142.2 Before (WRONG)

```c
// In smp_peer.c line ~243
agent_conn_info[aci_len++] = 0x01;  // Only 1 byte!
```

**Result:** `01` instead of `00 01`

### 142.3 After (CORRECT)

```c
// In smp_peer.c
agent_conn_info[aci_len++] = 0x00;  // High byte
agent_conn_info[aci_len++] = 0x01;  // Low byte = 1
```

**Result:** `00 01` (Word16 BE)

---

## 143. Bug 12: queueMode for Nothing (2026-01-24 S6 Finale)

### 143.1 The Problem

**For `Maybe Nothing`, '0' was sent, but NOTHING should be sent!**

### 143.2 Haskell Source

```haskell
-- Maybe Encoding:
smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
-- Nothing = '0' (0x30) WITHOUT further value
-- Just x  = '1' (0x31) + x

-- BUT for queueMode:
maybe "" smpEncode queueMode
-- Nothing = "" (EMPTY!)
-- Just QMSubscription = "0"
-- Just QMMessaging = "M"
```

### 143.3 The Misunderstanding

```
WRONGLY UNDERSTOOD:
- Nothing → '0'          ← WRONG for queueMode!

CORRECT:
- queueMode = Nothing → "" (nothing!)
- queueMode = Just QMSubscription → "0"
- queueMode = Just QMMessaging → "M"
```

### 143.4 Fix

```c
// BEFORE: (wrong)
buf[p++] = '0';  // queueMode

// AFTER: (correct)
// Don't append anything for Nothing!
```

---


---


## 32. Current Status (2026-01-23 Session 2)

### 32.1 Verified Bytes - Complete Table

| Element | Expected | ESP32 | Status |
|---------|----------|-------|--------|
| agentVersion | `00 07` | `00 07` | ✅ |
| Confirmation tag | `43` ('C') | `43` | ✅ |
| Maybe e2e | `31` ('1') | `31` | ✅ |
| e2eVersion | `00 02` | `00 02` | ✅ |
| key1 len | `44` (68) | `44` | ✅ |
| X448 SPKI OID | `2b 65 6f` | `2b 65 6f` | ✅ |
| key2 len | `44` (68) | `44` | ✅ |
| encConnInfo | 365 bytes | 365 bytes | ✅ |
| queueMode | `30` ('0') | `30` | ✅ |

### 32.2 Ratchet Output Calculation

**Input:** 225 bytes (AgentConnInfoReply with queueMode)

**Output:** 365 bytes (encConnInfo)

**Calculation:**
```
  1 byte    (length prefix for emHeader)
+ 123 bytes (emHeader = IV + encrypted_header + authTag)
+ 16 bytes  (body authTag)
+ 225 bytes (emBody = encrypted AgentConnInfoReply)
-----------
= 365 bytes ✅
```

### 32.3 Open Question

Server accepts everything with `OK`. SimpleX App still shows `A_MESSAGE`.

**Possible problem causes:**
- Double Ratchet Message Format?
- MsgHeader Structure (80 bytes)?
- Padding Schema?
- Message Number Encoding?

### 32.4 Status

🔥 **A_MESSAGE (2x) - STILL UNSOLVED**

---

## 33. Updated Bug Status (2026-01-23 Session 2 Final)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version Ranges corrected |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain Output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF instead of random |
| ratchet_init_sender Key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without Length-Prefix |
| **Port Length-Prefix instead of Space** | ✅ FIXED | 2026-01-23 | `buf[p++] = ' '` |
| **queueMode missing for v4+** | ✅ FIXED | 2026-01-23 | `buf[p++] = '0'` |
| **AssocData (AAD) for Header** | ✅ IMPLEMENTED | 2026-01-23 | 112 bytes AAD |
| **AAD for Payload** | ✅ TESTED | 2026-01-23 | 112 bytes (not the cause!) |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-23 | Parsing Error - UNSOLVED |

---

## 34. Extended Changelog (2026-01-23 Session 2 Final)

| Date | Change |
|------|--------|
| 2026-01-23 S2 | **X3DH Salt Bug found**: NULL → 64 null bytes |
| 2026-01-23 S2 | **X3DH Output Bug found**: 32 → 96 bytes (hk+nhk+sk) |
| 2026-01-23 S2 | **HKDF Hash Bug found**: SHA256 → SHA512 |
| 2026-01-23 S2 | **kdf_root Info-String fixed**: "SimpleXRootRatchet" |
| 2026-01-23 S2 | **kdf_chain Info-String fixed**: "SimpleXChainRatchet" |
| 2026-01-23 S2 | **kdf_chain Output fixed**: 96 bytes with IVs |
| 2026-01-23 S2 | **IVs from KDF instead of random** |
| 2026-01-23 S2 | **ratchet_init_sender fixed**: No new key |
| 2026-01-23 S2 | **emHeader Format fixed**: 123 bytes (no len-prefix for IV/Tag) |
| 2026-01-23 S2 | **AssocData (AAD) Bug found**: 112 bytes missing |
| 2026-01-23 S2 | **ratchet_state_t extended**: assoc_data[112] |
| 2026-01-23 S2 | **AES-GCM calls with AAD** |
| 2026-01-23 S2 | **Port-Encoding Bug found**: Length-Prefix → Space |
| 2026-01-23 S2 | **Port-Encoding Bug fixed**: Line 489 in smp_queue.c |
| 2026-01-23 S2 | **Payload AAD tested**: 112 bytes, A_MESSAGE remains |
| 2026-01-23 S2 | **queueMode Bug found**: Missing for v4+ |
| 2026-01-23 S2 | **queueMode Bug fixed**: `buf[p++] = '0'` |
| 2026-01-23 S2 | **AgentConnInfoReply now 225 bytes** (before 224) |
| 2026-01-23 S2 | **encConnInfo now 365 bytes** |
| 2026-01-23 S2 | Documentation v7 created |

---

## 35. Open Questions (2026-01-23 Session 2 Final)

1. ✅ ~~X3DH Salt correct?~~ **FIXED: 64 null bytes**
2. ✅ ~~HKDF Hash Algorithm?~~ **FIXED: SHA512**
3. ✅ ~~KDF Info-Strings correct?~~ **FIXED: SimpleXRootRatchet, SimpleXChainRatchet**
4. ✅ ~~kdf_chain Output?~~ **FIXED: 96 bytes with IVs**
5. ✅ ~~ratchet_init_sender Key?~~ **FIXED: use our_key2**
6. ✅ ~~emHeader Format?~~ **FIXED: 123 bytes**
7. ✅ ~~Port Encoding?~~ **FIXED: Space instead of Length-Prefix**
8. ✅ ~~queueMode for v4+?~~ **FIXED: '0' added**
9. ✅ ~~AssocData (AAD) implemented?~~ **IMPLEMENTED: 112 bytes**
10. ✅ ~~Payload AAD 235 bytes?~~ **TESTED: 112 bytes sufficient, not the cause**
11. ❓ Are all X448 Keys in SPKI format correct?
12. 🔥 **What exactly causes A_MESSAGE (2x)?** - MAIN PROBLEM

---


---


## 37. Bug 1: ClientMessage Padding Missing Completely (2026-01-23 Session 3)

### 37.1 🔥 BUG: No Padding to 15904 bytes

**Problem identified:** ESP32 was sending only the actual message length without padding. But SimpleX expects a constant size of 15904 bytes for `ClientMessage`.

**ESP32 sent (WRONG):**
```
556 bytes = 2 (Word16 len) + 554 (actual data)
```

**SimpleX expects (CORRECT):**
```
15904 bytes = 2 (Word16 len) + data + '#' padding up to 15904
```

### 37.2 Haskell Reference

**Source:** SimpleX Haskell Source

```haskell
e2eEncConfirmationLength :: Int
e2eEncConfirmationLength = 15904

-- Padding function:
pad :: ByteString -> Int -> ByteString
pad msg paddedLen = encodeWord16 len <> msg <> B.replicate padLen '#'
  where
    len = B.length msg
    padLen = paddedLen - 2 - len
```

**Important:** 
- `encodeWord16 len` = 2 bytes (Big Endian length of original message)
- `msg` = the actual data
- `B.replicate padLen '#'` = padding with `#` (0x23) up to target size

### 37.3 Fix

**File:** `main/smp_peer.c`

```c
#define E2E_ENC_CONFIRMATION_LENGTH 15904

// Padding for ClientMessage
uint8_t *padded = malloc(E2E_ENC_CONFIRMATION_LENGTH);
if (!padded) {
    ESP_LOGE(TAG, "   ❌ Failed to allocate padding buffer");
    return -1;
}

// Word16 length prefix (Big Endian)
padded[0] = (msg_len >> 8) & 0xFF;
padded[1] = msg_len & 0xFF;

// Original message
memcpy(&padded[2], plaintext, msg_len);

// Padding with '#' (0x23)
size_t pad_start = 2 + msg_len;
memset(&padded[pad_start], '#', E2E_ENC_CONFIRMATION_LENGTH - pad_start);

// Now encrypt with padded, E2E_ENC_CONFIRMATION_LENGTH
```

### 37.4 Visualization

```
ClientMessage after Padding (15904 bytes):
┌─────────┬─────────────────────┬──────────────────────────────┐
│ 2 bytes │ msg_len bytes       │ (15904 - 2 - msg_len) bytes  │
│ Length  │ Original Message    │ '#' Padding                  │
│ (BE)    │                     │ (0x23 repeated)              │
└─────────┴─────────────────────┴──────────────────────────────┘
     ↓            ↓                         ↓
   00 22A      [data...]           23 23 23 23 23 23 ...
   (554)
```

### 37.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ 2026-01-23 |
| Root Cause | ✅ Padding to 15904 bytes was missing |
| Fix implemented | ✅ smp_peer.c |
| Constant defined | ✅ E2E_ENC_CONFIRMATION_LENGTH = 15904 |
| Status | ✅ **FIXED** |

---

## 38. Bug 2: Buffer Sizes Too Small - Stack Overflow (2026-01-23 Session 3)

### 38.1 🔥 BUG: Static buffers crashed with large data

**Problem identified:** With the new 15904-byte padding, the static stack buffers were far too small and led to stack overflow / memory corruption.

### 38.2 Affected Buffers

| Buffer | Before | After | Reason |
|--------|--------|-------|--------|
| `encrypted[]` | 1500 | `malloc(15944)` | 15904 + authTag + overhead |
| `client_msg[]` | 2000 | `malloc(16100)` | encrypted + header |
| `send_body[]` | 2500 | `malloc(16100)` | client_msg + envelope |
| `transmission[]` | 3000 | `malloc(16200)` | send_body + framing |

### 38.3 Problem Analysis

```
Stack Layout before (CRASH!):
┌─────────────────────────────────────┐
│ transmission[3000]                  │ ← Too small!
│ send_body[2500]                     │ ← Too small!
│ client_msg[2000]                    │ ← Too small!
│ encrypted[1500]                     │ ← Too small!
│ ... other variables ...             │
└─────────────────────────────────────┘
           ↓
    Stack Overflow at 15904+ bytes!
```

### 38.4 Fix: Switch to Dynamic Allocation

**File:** `main/smp_peer.c`

```c
// BEFORE (CRASH):
uint8_t encrypted[1500];
uint8_t client_msg[2000];
uint8_t send_body[2500];
uint8_t transmission[3000];

// AFTER (CORRECT):
uint8_t *encrypted = malloc(15944);
uint8_t *client_msg = malloc(16100);
uint8_t *send_body = malloc(16100);
uint8_t *transmission = malloc(16200);

// Important: Free at the end!
free(encrypted);
free(client_msg);
free(send_body);
free(transmission);
```

### 38.5 ESP32 Heap Analysis

```
ESP32 WROOM-32 Memory:
- Total DRAM: ~320 KB
- Free Heap (typical): ~200 KB
- Required for Padding: ~65 KB (4 × ~16 KB)
- Result: ✅ Fits!
```

### 38.6 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ 2026-01-23 |
| Root Cause | ✅ Static buffers too small |
| Fix implemented | ✅ malloc() instead of Stack |
| Memory-Leaks checked | ✅ free() at the end |
| Status | ✅ **FIXED** |

---

## 39. Bug 3: Payload AAD Wrong - 112 instead of 235 bytes (2026-01-23 Session 3)

### 39.1 🔥 BUG: Body encryption used wrong AAD

**Problem identified:** During payload/body encryption, only `rcAD` (112 bytes) was used as AAD, but SimpleX expects `rcAD + emHeader` (235 bytes).

**ESP32 used (WRONG):**
```
AAD = rcAD (112 bytes) = our_key1_pub || peer_key1
```

**SimpleX expects (CORRECT):**
```
AAD = rcAD + emHeader (112 + 123 = 235 bytes)
```

### 39.2 Haskell Reference

**Source:** SimpleX Ratchet.hs - `decryptMessage`

```haskell
decryptMessage :: MessageKey -> EncRatchetMessage -> ExceptT CryptoError IO ByteString
decryptMessage (MessageKey mk iv) EncRatchetMessage {emHeader, emBody, emAuthTag} =
  tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
--                         ^^^^^^^^^^^^^^^^^
--                         AAD = rcAD (112) + emHeader (123) = 235 bytes!
```

**Important:** `emHeader` is the **encrypted** header (123 bytes blob), not the decrypted one!

### 39.3 Why This Makes Cryptographic Sense

```
Decrypt flow at receiver:
1. Receiver receives: emHeader (123 bytes) + emBody + emAuthTag
2. Receiver knows: rcAD (112 bytes) from X3DH
3. AAD for Body-Decrypt: rcAD || emHeader (235 bytes)
   └── emHeader is still encrypted at this point!
4. Only AFTER successful body verification: Decrypt header
```

### 39.4 Fix

**File:** `main/smp_ratchet.c`

```c
// BEFORE (WRONG):
if (aes_gcm_encrypt(message_key,
                    msg_iv, GCM_IV_LEN,
                    ratchet_state.assoc_data, 112,  // ← Only rcAD!
                    plaintext, pt_len,
                    encrypted_body, body_tag) != 0) {

// AFTER (CORRECT):
// First build AAD
uint8_t payload_aad[235];
memcpy(payload_aad, ratchet_state.assoc_data, 112);  // rcAD
memcpy(payload_aad + 112, em_header, 123);           // + emHeader

// Then encrypt with correct AAD
if (aes_gcm_encrypt(message_key,
                    msg_iv, GCM_IV_LEN,
                    payload_aad, 235,  // ← rcAD + emHeader!
                    plaintext, pt_len,
                    encrypted_body, body_tag) != 0) {
```

### 39.5 Visualization

```
Payload AAD Structure (235 bytes):
┌─────────────────────────────────┬──────────────────────────────────┐
│ rcAD (112 bytes)                │ emHeader (123 bytes)             │
├─────────────────────────────────┼──────────────────────────────────┤
│ our_key1_pub (56) │ peer_key1 (56) │ IV (16) │ enc_hdr (91) │ tag (16) │
└─────────────────────────────────┴──────────────────────────────────┘
```

### 39.6 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ 2026-01-23 |
| Root Cause | ✅ AAD too short (112 instead of 235) |
| Haskell reference found | ✅ decryptMessage in Ratchet.hs |
| Fix implemented | ✅ payload_aad[235] |
| Status | ✅ **FIXED** |

---

## 40. Bug 4: 🔥 Ratchet Internal Padding Missing - MAIN PROBLEM 🔥 (2026-01-23 Session 3)

### 40.1 🔥🔥🔥 CRITICAL: Second Padding Layer Completely Missing!

**Problem identified:** Within the ratchet encryption, the payload (`encConnInfo`) is padded to **14832 bytes** **BEFORE** AES-GCM encryption. This padding layer was completely missing!

**ESP32 currently sends (WRONG):**
```
encConnInfo = ~365 bytes (unpadded)
```

**SimpleX expects (CORRECT):**
```
encConnInfo = ~14972 bytes (padded to 14832 before encryption)
```

### 40.2 Haskell Reference

**Source 1:** SimpleX Agent/Protocol.hs

```haskell
e2eEncConnInfoLength :: Version -> PQSupport -> Int
e2eEncConnInfoLength v = \case
  PQSupportOn | v >= pqdrSMPAgentVersion -> 11106
  _ -> 14832  -- ← THIS NUMBER for standard mode!
```

**Source 2:** SimpleX Ratchet.hs - `rcEncryptMsg`

```haskell
rcEncryptMsg :: Ratchet -> Int -> ByteString -> ExceptT CryptoError IO (Ratchet, EncRatchetMessage)
rcEncryptMsg rc@Ratchet {..} paddedMsgLen msg = do
  -- ...
  (emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
  --                                 ^^^^^^^^^^^^
  --                                 paddedMsgLen = 14832!
  -- ...
```

**Important:** `encryptAEAD` internally calls `pad()` before encrypting!

### 40.3 The Two Padding Layers in Detail

```
Padding Hierarchy (from inside out):
═══════════════════════════════════════════════════════════════════

LAYER 1: Ratchet Internal Padding (CURRENTLY MISSING!)
├── Input: AgentConnInfoReply (225 bytes raw)
├── Target size: 14832 bytes
├── Format: Word16(225) + data + '#' × (14832 - 2 - 225)
├── Output: 14832 bytes padded
└── Then: AES-GCM Encrypt → ~14848 bytes (+ IV + Tag)

LAYER 2: ClientMessage Padding (already implemented)
├── Input: AgentConfirmation with encConnInfo (~14972 bytes)
├── Target size: 15904 bytes
├── Format: Word16(len) + data + '#' padding
└── Output: 15904 bytes

═══════════════════════════════════════════════════════════════════
```

### 40.4 Byte Calculation

**Currently (WRONG):**
```
AgentConnInfoReply: 225 bytes
After ratchet_encrypt (without padding):
  = 1 (len prefix) + 123 (emHeader) + 16 (authTag) + 225 (emBody)
  = 365 bytes
```

**Expected (CORRECT):**
```
AgentConnInfoReply: 225 bytes
After padding to 14832:
  = Word16(225) + 225 + '#' × 14605
  = 14832 bytes
After AES-GCM Encrypt:
  = 14832 + 16 (authTag)
  = 14848 bytes
encConnInfo total:
  = 1 (len prefix) + 123 (emHeader) + 16 (emAuthTag) + 14848 (emBody)
  = 14988 bytes (approx.)
```

### 40.5 Fix Required

**File:** `main/smp_ratchet.c` - Function `ratchet_encrypt()`

```c
#define E2E_ENC_CONN_INFO_LENGTH 14832

int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len) {
    
    // === NEW: Padding BEFORE encryption ===
    uint8_t *padded_payload = malloc(E2E_ENC_CONN_INFO_LENGTH);
    if (!padded_payload) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate padding buffer");
        return -1;
    }
    
    // Word16 length prefix (Big Endian)
    padded_payload[0] = (pt_len >> 8) & 0xFF;
    padded_payload[1] = pt_len & 0xFF;
    
    // Original payload
    memcpy(&padded_payload[2], plaintext, pt_len);
    
    // Padding with '#' (0x23)
    size_t pad_start = 2 + pt_len;
    memset(&padded_payload[pad_start], '#', E2E_ENC_CONN_INFO_LENGTH - pad_start);
    
    // Now header encryption (as before)...
    // ...
    
    // Body encryption WITH PADDED PAYLOAD!
    if (aes_gcm_encrypt(message_key,
                        msg_iv, GCM_IV_LEN,
                        payload_aad, 235,
                        padded_payload, E2E_ENC_CONN_INFO_LENGTH,  // ← Padded!
                        encrypted_body, body_tag) != 0) {
        free(padded_payload);
        return -1;
    }
    
    free(padded_payload);
    
    // Assemble output...
}
```

### 40.6 Visualization of Missing Padding

```
CURRENTLY (WRONG):
┌───────────────────────────────────────────────────────────────┐
│ AgentConnInfoReply (225 bytes)                                │
└───────────────────────────────────────────────────────────────┘
                    ↓ AES-GCM Encrypt
┌───────────────────────────────────────────────────────────────┐
│ emBody (241 bytes) + authTag (16)                             │
└───────────────────────────────────────────────────────────────┘

EXPECTED (CORRECT):
┌───────────────────────────────────────────────────────────────────────────┐
│ 2 bytes │ 225 bytes              │ 14605 bytes                            │
│ Length  │ AgentConnInfoReply     │ '#' Padding                            │
│ (BE)    │                        │ (0x23 repeated)                        │
└───────────────────────────────────────────────────────────────────────────┘
                    ↓ AES-GCM Encrypt (14832 bytes input!)
┌───────────────────────────────────────────────────────────────────────────┐
│ emBody (14832 bytes encrypted) + authTag (16)                             │
└───────────────────────────────────────────────────────────────────────────┘
```

### 40.7 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ 2026-01-23 |
| Root Cause | ✅ Ratchet internal padding missing |
| Target size determined | ✅ 14832 bytes |
| Haskell reference found | ✅ e2eEncConnInfoLength, rcEncryptMsg |
| Fix code designed | ✅ See above |
| Status | 🔥 **OPEN - MAIN PROBLEM** |

---


---


## 42. Extended Changelog (2026-01-23 Session 3)

| Date | Change |
|------|--------|
| 2026-01-23 S3 | **ClientMessage Padding Bug found**: 556 → 15904 bytes |
| 2026-01-23 S3 | **ClientMessage Padding fixed**: '#' padding implemented |
| 2026-01-23 S3 | **Buffer-Overflow Bug found**: Static buffers too small |
| 2026-01-23 S3 | **Buffers switched to malloc()**: 15944, 16100, 16100, 16200 |
| 2026-01-23 S3 | **Payload AAD Bug confirmed**: 112 → 235 bytes |
| 2026-01-23 S3 | **Payload AAD fixed**: payload_aad[235] = rcAD + emHeader |
| 2026-01-23 S3 | **🔥 Ratchet-Padding Bug discovered**: encConnInfo not padded! |
| 2026-01-23 S3 | **Target size determined**: e2eEncConnInfoLength = 14832 |
| 2026-01-23 S3 | **A_MESSAGE cause identified**: Missing Ratchet-Padding! |
| 2026-01-23 S3 | Documentation v8 created |

---

## 43. Open Questions (2026-01-23 Session 3)

1. ✅ ~~X3DH Salt correct?~~ **FIXED: 64 null bytes**
2. ✅ ~~HKDF Hash Algorithm?~~ **FIXED: SHA512**
3. ✅ ~~KDF Info-Strings correct?~~ **FIXED: SimpleXRootRatchet, SimpleXChainRatchet**
4. ✅ ~~kdf_chain Output?~~ **FIXED: 96 bytes with IVs**
5. ✅ ~~ratchet_init_sender Key?~~ **FIXED: use our_key2**
6. ✅ ~~emHeader Format?~~ **FIXED: 123 bytes**
7. ✅ ~~Port Encoding?~~ **FIXED: Space instead of Length-Prefix**
8. ✅ ~~queueMode for v4+?~~ **FIXED: '0' added**
9. ✅ ~~ClientMessage Padding?~~ **FIXED: 15904 bytes with '#'**
10. ✅ ~~Buffer sizes?~~ **FIXED: malloc() instead of Stack**
11. ✅ ~~Payload AAD?~~ **FIXED: 235 bytes (rcAD + emHeader)**
12. ❓ Are all X448 Keys in SPKI format correct?
13. 🔥 **Ratchet internal padding (14832 bytes)?** - NEXT STEP!

---

## 44. Next Steps (2026-01-23 Session 3)

### 44.1 Priority 1: Implement Ratchet Padding

**Task:** Modify `ratchet_encrypt()` in `smp_ratchet.c`:

1. Before AES-GCM Encryption: Pad payload to 14832 bytes
2. Format: `Word16(original_len) + data + '#' × padding_count`
3. Adjust buffer sizes accordingly
4. Test!

### 44.2 Expected Result After Fix

```
encConnInfo after fix:
= 1 (len prefix)
+ 123 (emHeader)
+ 16 (emAuthTag)
+ 14832 (emBody, padded)
+ 16 (bodyAuthTag)
≈ 14988 bytes

ClientMessage after fix:
= Word16(14988 + additional headers)
+ AgentConfirmation
+ '#' padding
= 15904 bytes
```

### 44.3 Verification

After the fix:
- ESP32 should send ~15904 bytes ClientMessage
- encConnInfo should be ~14988 bytes
- A_MESSAGE Error should disappear! 🎉

---


## 51. Updated Bug Status (2026-01-24)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version Ranges corrected |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain Output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF not random |
| ratchet_init_sender Key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without Length-Prefix |
| Port Length-Prefix instead of Space | ✅ FIXED | 2026-01-23 | `buf[p++] = ' '` |
| queueMode missing for v4+ | ✅ FIXED | 2026-01-23 | `buf[p++] = '0'` |
| ClientMessage Padding missing | ✅ FIXED | 2026-01-23 S3 | 15904 bytes with '#' |
| Buffer too small (Stack Overflow) | ✅ FIXED | 2026-01-23 S3 | malloc() instead of Stack |
| Payload AAD 112 instead of 235 bytes | ✅ FIXED | 2026-01-23 S3 | `payload_aad[235]` |
| **Ratchet-Padding 14832 bytes** | ✅ FIXED | 2026-01-24 | Padding before AES-GCM |
| **enc_conn_info Buffer (Bug 5)** | ✅ FIXED | 2026-01-24 | malloc(16000) |
| **agent_msg Buffer (Bug 6)** | ✅ FIXED | 2026-01-24 | malloc(20000) |
| **plaintext Buffer (Bug 7)** | ✅ FIXED | 2026-01-24 | malloc(20000) |
| **agent_envelope Buffer (Bug 8)** | ✅ FIXED | 2026-01-24 | malloc(16000) |
| **A_MESSAGE (HELLO)** | 🔥 CURRENT | 2026-01-24 | Buffer in smp_handshake.c |

---

## 52. Extended Changelog (2026-01-24)

| Date | Change |
|------|--------|
| 2026-01-24 | **🎉 Ratchet padding implemented**: 14832 bytes before AES-GCM |
| 2026-01-24 | **Output verified**: 14972 bytes encConnInfo ✅ |
| 2026-01-24 | **A_MESSAGE reduced**: 2x → 1x! |
| 2026-01-24 | **Bug 5 fixed**: enc_conn_info → malloc(16000) |
| 2026-01-24 | **Bug 6 fixed**: agent_msg → malloc(20000) |
| 2026-01-24 | **Bug 7 fixed**: plaintext → malloc(20000) |
| 2026-01-24 | **Bug 8 fixed**: agent_envelope → malloc(16000) |
| 2026-01-24 | **HELLO Message**: Encryption works (14972 bytes) |
| 2026-01-24 | **App parses 1st part**: AgentConfirmation successful! |
| 2026-01-24 | **New crash**: Buffer after HELLO encryption |
| 2026-01-24 | Documentation v9 created |

---

## 53. Open Questions (2026-01-24)

1. ✅ ~~X3DH Salt correct?~~ **FIXED: 64 null bytes**
2. ✅ ~~HKDF Hash Algorithm?~~ **FIXED: SHA512**
3. ✅ ~~KDF Info-Strings correct?~~ **FIXED: SimpleXRootRatchet, SimpleXChainRatchet**
4. ✅ ~~kdf_chain Output?~~ **FIXED: 96 bytes with IVs**
5. ✅ ~~ratchet_init_sender Key?~~ **FIXED: use our_key2**
6. ✅ ~~emHeader Format?~~ **FIXED: 123 bytes**
7. ✅ ~~Port Encoding?~~ **FIXED: Space instead of Length-Prefix**
8. ✅ ~~queueMode for v4+?~~ **FIXED: '0' added**
9. ✅ ~~ClientMessage Padding?~~ **FIXED: 15904 bytes with '#'**
10. ✅ ~~Ratchet Padding?~~ **FIXED: 14832 bytes before AES-GCM**
11. ✅ ~~Buffer sizes?~~ **FIXED: malloc() for all large buffers**
12. ✅ ~~Payload AAD?~~ **FIXED: 235 bytes (rcAD + emHeader)**
13. ❓ Are all X448 Keys in SPKI format correct?
14. 🔥 **Which buffers in smp_handshake.c crash after HELLO?**
15. 🔥 **Why A_MESSAGE for HELLO despite correct padding?**

---

## 54. Next Steps (2026-01-24)

### 54.1 Priority 1: Fix HELLO Message Crash

**Task:** Convert more buffers in `smp_handshake.c` to malloc.

**Suspected affected buffers:**
- Buffer after `agent_envelope` for HELLO
- Transmission buffer for 15904 bytes
- Possibly more static arrays

### 54.2 Priority 2: Debug A_MESSAGE for HELLO

**Possible causes:**
- HELLO message format different from AgentConfirmation?
- AgentMsgEnvelope ('M' tag) encoded differently?
- Wrong padding for HELLO?

---


---


## 56. Ratchet Padding Sizes - Two Different! (2026-01-24 Session 3 Continuation)

### 56.1 Critical Discovery: Different Padding for Different Message Types!

**Problem:** We used 14832 bytes for BOTH message types. But SimpleX uses different padding sizes!

### 56.2 Padding Sizes Table

| Message Type | Padding Length | Usage | Constant |
|--------------|----------------|-------|----------|
| AgentConfirmation (encConnInfo) | **14832 bytes** | X3DH + ConnInfo | `e2eEncConnInfoLength` |
| AgentMsgEnvelope (encAgentMessage) | **15840 bytes** | HELLO, A_MSG, etc. | `e2eEncAgentMsgLength` |

### 56.3 Haskell Reference

**Source:** SimpleX Agent/Protocol.hs

```haskell
-- Padding for encConnInfo (AgentConfirmation)
e2eEncConnInfoLength :: VersionSMPA -> PQSupport -> Int
e2eEncConnInfoLength v = \case
  PQSupportOn | v >= pqdrSMPAgentVersion -> 11106   -- with Post-Quantum
  _ -> 14832  -- without PQ ← WE USE THIS!

-- Padding for encAgentMessage (HELLO, A_MSG, etc.)
e2eEncAgentMsgLength :: VersionSMPA -> PQSupport -> Int  
e2eEncAgentMsgLength v = \case
  PQSupportOn | v >= pqdrSMPAgentVersion -> 11914   -- with Post-Quantum
  _ -> 15840  -- without PQ ← WE USE THIS!
```

### 56.4 Important Distinction

```
Message Type Distinction:
═══════════════════════════════════════════════════════════════════

AgentConfirmation ('C' Tag):
├── Contains: e2eEncryption_ + encConnInfo
├── encConnInfo Padding: 14832 bytes
├── Total size after Ratchet: ~14972 bytes
└── Usage: Once during Connection Setup

AgentMsgEnvelope ('M' Tag):
├── Contains: encAgentMessage
├── encAgentMessage Padding: 15840 bytes
├── Total size after Ratchet: ~15980 bytes
└── Usage: All regular messages (HELLO, A_MSG, etc.)

═══════════════════════════════════════════════════════════════════
```

### 56.5 Implementation

**Change in `smp_ratchet.c`:**

```c
// NEW: Parameter for padding size
int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    size_t padded_msg_len,  // ← NEW!
                    uint8_t *output, size_t *out_len) {
    
    // Padding with passed size
    uint8_t *padded_payload = malloc(padded_msg_len);
    // ...
}

// Call for AgentConfirmation:
#define E2E_ENC_CONN_INFO_LENGTH 14832
ratchet_encrypt(conn_info, conn_info_len, E2E_ENC_CONN_INFO_LENGTH, ...);

// Call for HELLO:
#define E2E_ENC_AGENT_MSG_LENGTH 15840
ratchet_encrypt(hello_msg, hello_len, E2E_ENC_AGENT_MSG_LENGTH, ...);
```

### 56.6 Status

| Item | Status |
|------|--------|
| Two padding sizes discovered | ✅ 2026-01-24 |
| Haskell reference found | ✅ e2eEncConnInfoLength, e2eEncAgentMsgLength |
| Implementation adapted | ✅ Parameter added |
| Status | ✅ **FIXED** |

---

## 57. HELLO Message Format - Complete Analysis (2026-01-24)

### 57.1 HELLO Structure after Double Ratchet Decryption

**The HELLO message after decryption has the following structure:**

```
AgentMessage = 'M' + APrivHeader + AMessage
APrivHeader  = sndMsgId (Int64 BE) + prevMsgHash (length-prefixed ByteString)
AMessage     = HELLO = 'H'
```

### 57.2 Haskell Encoding Reference

**Source:** SimpleX Agent/Protocol.hs

```haskell
-- AMessage Encoding
instance Encoding AMessage where
  smpEncode = \case
    HELLO -> smpEncode HELLO_           -- = "H" (single byte 0x48)
    A_MSG body -> smpEncode (A_MSG_, Tail body)
    A_RCVD rcptInfo -> smpEncode (A_RCVD_, rcptInfo)
    QCONT addr -> smpEncode (QCONT_, addr)
    QADD qs -> smpEncode (QADD_, qs)
    QKEY qs -> smpEncode (QKEY_, qs)
    QUSE qs -> smpEncode (QUSE_, qs)
    QTEST qs -> smpEncode (QTEST_, qs)
    EREADY v -> smpEncode (EREADY_, v)

-- APrivHeader Encoding
instance Encoding APrivHeader where
  smpEncode APrivHeader {sndMsgId, prevMsgHash} =
    smpEncode (sndMsgId, prevMsgHash)
    -- sndMsgId: Int64 Big-Endian (8 bytes)
    -- prevMsgHash: Length-prefixed ByteString
```

### 57.3 Byte Layout for HELLO (Detailed)

**First HELLO message (without prevMsgHash):**

```
Offset  Length  Field                   Value/Notes
──────────────────────────────────────────────────────────────────
0       1       AgentMessage tag        'M' (0x4D)
1       8       sndMsgId                Int64 BE (e.g. 0x0000000000000001)
9       1       prevMsgHash length      0x00 (for first message)
10      1       AMessage tag            'H' (0x48 = HELLO)
──────────────────────────────────────────────────────────────────
Total:  11 bytes
```

**Following messages (with prevMsgHash):**

```
Offset  Length  Field                   Value/Notes
──────────────────────────────────────────────────────────────────
0       1       AgentMessage tag        'M' (0x4D)
1       8       sndMsgId                Int64 BE (e.g. 0x0000000000000002)
9       1       prevMsgHash length      0x20 (32 bytes)
10      32      prevMsgHash bytes       SHA256 of previous message
42      1       AMessage tag            'H' (0x48 = HELLO)
──────────────────────────────────────────────────────────────────
Total:  43 bytes
```

### 57.4 Visualization

```
HELLO Message (first, 11 bytes):
┌─────┬─────────────────────────┬─────┬─────┐
│ 'M' │ sndMsgId (8 bytes BE)   │ 00  │ 'H' │
│ 4D  │ 00 00 00 00 00 00 00 01 │     │ 48  │
└─────┴─────────────────────────┴─────┴─────┘
  ↓           ↓                   ↓     ↓
 Tag    Message ID #1         No Hash  HELLO
```

### 57.5 Status

| Item | Status |
|------|--------|
| HELLO format analyzed | ✅ |
| Byte layout documented | ✅ |
| Haskell reference found | ✅ |
| ESP32 implementation | ✅ |
| Status | ✅ **ANALYZED** |

---

## 58. Current Status: 2x A_MESSAGE Error (2026-01-24)

### 58.1 ⚠️ Status Update: Back to 2x A_MESSAGE!

**Important correction:** After further testing, the app shows **2x A_MESSAGE Error** again, not 1x as previously reported.

### 58.2 What Works ✅

| Component | Status | Details |
|-----------|--------|---------|
| TLS 1.3 Connection | ✅ | ALPN: "smp/1" |
| SMP Handshake | ✅ | Version negotiated |
| Queue Creation | ✅ | NEW → IDS |
| Invitation Parsing | ✅ | Link correctly decoded |
| X3DH Key Agreement | ✅ | 3× DH with X448 |
| Double Ratchet Init | ✅ | Keys derived |
| Double Ratchet Encryption | ✅ | Works! |
| **Correct Padding Sizes** | ✅ | 14832 / 15840 bytes |
| **Server accepts both** | ✅ | "OK" for both messages |

### 58.3 What Doesn't Work ❌

| Problem | Status | Details |
|---------|--------|---------|
| AgentConfirmation Parsing | ❌ | A_MESSAGE Error |
| HELLO Parsing | ❌ | A_MESSAGE Error |
| Connection Establishment | ❌ | Both sides fail |

---

## 59. Open Hypotheses (2026-01-24)

### 59.1 Hypothesis 1: prevMsgHash Format

**Question:** Must a 32-byte zero hash be sent even for the first message?

**Currently:**
```c
hello_plaintext[9] = 0x00;  // Length = 0 (no hash)
```

**Alternative:**
```c
hello_plaintext[9] = 0x20;  // Length = 32
memset(&hello_plaintext[10], 0x00, 32);  // 32 zero bytes
```

**Status:** ❓ TO TEST

### 59.2 Hypothesis 2: Message Counter Starts at 1?

**Question:** Does `msgNum` in Double Ratchet start at 1 instead of 0?

**Currently:**
```c
ratchet_state.msg_num_send = 0;
// First message: msgNum = 0
```

**Alternative:**
```c
ratchet_state.msg_num_send = 1;
// First message: msgNum = 1
```

**Status:** ❓ TO CHECK

### 59.3 Hypothesis Priority

| # | Hypothesis | Probability | Effort |
|---|------------|-------------|--------|
| 1 | prevMsgHash Format | Medium | Low |
| 2 | msgNum Start at 1 | Medium | Low |
| 3 | Envelope Format | High | Medium |
| 4 | Version Mismatch | Low | Low |

---

## 60. Debugging Strategy (2026-01-24)

### 60.1 Next Steps

**Priority 1: Analyze Haskell code for AgentMsgEnvelope**
- Exact structure of outer envelope
- Difference to AgentConfirmation

**Priority 2: Test prevMsgHash**
- Test with 32 zero bytes instead of length 0
- Log comparison

**Priority 3: msgNum Initialization**
- Haskell source: Where is msgNum initialized?
- Test with msgNum = 1

---

## 61. Updated Bug Status (2026-01-24 Update 2)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version Ranges corrected |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain Output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF instead of random |
| ratchet_init_sender Key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without Length-Prefix |
| Port Length-Prefix instead of Space | ✅ FIXED | 2026-01-23 | `buf[p++] = ' '` |
| queueMode missing for v4+ | ✅ FIXED | 2026-01-23 | `buf[p++] = '0'` |
| ClientMessage Padding missing | ✅ FIXED | 2026-01-23 S3 | 15904 bytes with '#' |
| Buffer too small (Stack Overflow) | ✅ FIXED | 2026-01-23 S3 | malloc() instead of Stack |
| Payload AAD 112 instead of 235 bytes | ✅ FIXED | 2026-01-23 S3 | `payload_aad[235]` |
| Ratchet Padding 14832 bytes | ✅ FIXED | 2026-01-24 | Padding before AES-GCM |
| enc_conn_info Buffer (Bug 5) | ✅ FIXED | 2026-01-24 | malloc(16000) |
| agent_msg Buffer (Bug 6) | ✅ FIXED | 2026-01-24 | malloc(20000) |
| plaintext Buffer (Bug 7) | ✅ FIXED | 2026-01-24 | malloc(20000) |
| agent_envelope Buffer (Bug 8) | ✅ FIXED | 2026-01-24 | malloc(16000) |
| **Two Padding Sizes** | ✅ DISCOVERED | 2026-01-24 | 14832 / 15840 bytes |
| **HELLO Format** | ✅ ANALYZED | 2026-01-24 | 'M' + APrivHeader + 'H' |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-24 | Cause unknown |

---

## 62. Extended Changelog (2026-01-24 Update 2)

| Date | Change |
|------|--------|
| 2026-01-24 | **Two padding sizes discovered**: 14832 (ConnInfo) vs 15840 (AgentMsg) |
| 2026-01-24 | **ratchet_encrypt with padding parameter** |
| 2026-01-24 | **Log output verified**: 14972 / 15980 bytes |
| 2026-01-24 | **HELLO format completely analyzed** |
| 2026-01-24 | **Byte layout documented**: 11 bytes (first) / 43 bytes (following) |
| 2026-01-24 | **Status correction**: 2x A_MESSAGE (not 1x) |
| 2026-01-24 | **Four hypotheses documented** |
| 2026-01-24 | **Debugging strategy created** |
| 2026-01-24 | Documentation v10 created |

---

## 63. Open Questions (2026-01-24 Update 2)

1. ✅ ~~Ratchet Padding?~~ **FIXED: 14832 bytes**
2. ✅ ~~HELLO Padding different?~~ **YES: 15840 bytes!**
3. ✅ ~~HELLO Format?~~ **ANALYZED: 'M' + APrivHeader + 'H'**
4. ❓ **prevMsgHash**: Empty (0x00) or zero-filled (32× 0x00)?
5. ❓ **msgNum**: Starts at 0 or 1?
6. ❓ **AgentMsgEnvelope**: Exact outer structure?
7. ❓ **Version**: Is agentVersion=7 correct?
8. 🔥 **Why 2x A_MESSAGE despite correct padding?**

---


---


## 69. Updated Bug Status (2026-01-24 Session 4)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version Ranges corrected |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain Info-String wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain Output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF not random |
| ratchet_init_sender Key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without Length-Prefix |
| Port Length-Prefix instead of Space | ✅ FIXED | 2026-01-23 | `buf[p++] = ' '` |
| queueMode missing for v4+ | ✅ FIXED | 2026-01-23 | `buf[p++] = '0'` |
| ClientMessage Padding missing | ✅ FIXED | 2026-01-23 S3 | 15904 bytes with '#' |
| Buffer too small (Stack Overflow) | ✅ FIXED | 2026-01-23 S3 | malloc() instead of Stack |
| Payload AAD 112 instead of 235 bytes | ✅ FIXED | 2026-01-23 S3 | `payload_aad[235]` |
| Ratchet-Padding 14832 bytes | ✅ FIXED | 2026-01-24 | Padding before AES-GCM |
| Two Padding Sizes | ✅ RECOGNIZED | 2026-01-24 | 14832 / 15840 bytes |
| **KDF Output Order** | ✅ FIXED | 2026-01-24 S4 | Variable names fixed |
| **HELLO Format** | ✅ VERIFIED | 2026-01-24 S4 | 11 bytes correct |
| **EncRatchetMessage Format** | ✅ VERIFIED | 2026-01-24 S4 | 123 bytes emHeader |
| **rcAD Format** | ✅ VERIFIED | 2026-01-24 S4 | 112 bytes raw keys |
| **SPKI vs Raw in rcAD?** | ❓ TO CHECK | 2026-01-24 S4 | Peer keys format? |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-24 | Cause: Peer Key Format? |

---

## 70. Extended Changelog (2026-01-24 Session 4)

| Date | Change |
|------|--------|
| 2026-01-24 S4 | **HELLO format verified**: 11 bytes, hex dump documented |
| 2026-01-24 S4 | **EncRatchetMessage format verified**: emHeader = 123 bytes |
| 2026-01-24 S4 | **🔥 KDF Output Order Bug found** |
| 2026-01-24 S4 | **kdf_root signature fixed**: new_root_key, chain_key, next_header_key |
| 2026-01-24 S4 | **ratchet_init_sender fixed**: Correct variable assignments |
| 2026-01-24 S4 | **rcAD format documented**: 112 bytes = 56 + 56 raw keys |
| 2026-01-24 S4 | **New hypothesis**: SPKI vs Raw Keys in peer data |
| 2026-01-24 S4 | **Debug steps defined**: Check peer key extraction |
| 2026-01-24 S4 | Documentation v11 created |

---

## 71. Open Questions (2026-01-24 Session 4)

1. ✅ ~~HELLO Format?~~ **VERIFIED: 11 bytes correct**
2. ✅ ~~EncRatchetMessage Format?~~ **VERIFIED: emHeader = 123 bytes**
3. ✅ ~~KDF Output Order?~~ **FIXED: new_root, chain, next_header**
4. ✅ ~~rcAD Format?~~ **DOCUMENTED: 112 bytes raw keys**
5. 🔥 **Peer Keys SPKI or Raw?** - MAIN SUSPECT!
6. ❓ **prevMsgHash**: Empty (0x00) or zero-filled (32× 0x00)?
7. ❓ **msgNum**: Starts at 0 or 1?
8. 🔥 **Why 2x A_MESSAGE despite all fixes?**

---

## 72. SimpleGo Version Update (2026-01-24 Session 4)

```
SimpleGo v0.1.22-alpha
═══════════════════════════════════════════════════════════════════

Changes:
├── ✅ KDF Output Order Bug fixed
├── ✅ HELLO format verified (11 bytes)
├── ✅ EncRatchetMessage format verified (123 bytes emHeader)
├── ✅ rcAD format documented (112 bytes)
└── 🔍 New hypothesis: Peer Key Format (SPKI vs Raw)

Status:
├── 2x A_MESSAGE Error remains
└── Next debug: Check peer key extraction

═══════════════════════════════════════════════════════════════════
```

---

## 75. Fix 2: E2E Params Key Length Prefix - Word16 (2026-01-24 S4T2)

### 75.1 🔥 BUG: Key Length was 1 byte instead of 2 bytes!

**Problem:** The length prefix for E2E keys (68 bytes) was encoded as 1 byte instead of Word16 BE.

### 75.2 Before (WRONG - 140 bytes E2E)

```c
// Key length as 1 byte
e2e_params[pos++] = 68;  // ❌ Only 1 byte!
```

### 75.3 After (CORRECT - 142 bytes E2E)

```c
// Key length as Word16 Big-Endian (2 bytes)
e2e_params[pos++] = 0x00;  // High byte
e2e_params[pos++] = 0x44;  // Low byte = 68
```

### 75.4 Byte Difference Explanation

```
E2E Params Structure:
= 2 (e2eVersion Word16)
+ 2 (key1 length Word16)  ← WAS 1 BYTE!
+ 68 (key1 SPKI)
+ 2 (key2 length Word16)  ← WAS 1 BYTE!
+ 68 (key2 SPKI)
= 142 bytes ✅
```

### 75.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Fix implemented | ✅ Word16 BE for Key Lengths |
| Log verified | ✅ 142 bytes |
| Status | ✅ **FIXED** |

---


---

## 82. Extended Changelog (2026-01-24 S4T2)

| Date | Change |
|------|--------|
| 2026-01-24 S4T2 | **E2E Key Length fixed**: 1 → 2 bytes (Word16 BE) |
| 2026-01-24 S4T2 | **E2E params now 142 bytes** (was 140) |
| 2026-01-24 S4T2 | **ehBody Length fixed**: 1 → 2 bytes (Word16 BE) |
| 2026-01-24 S4T2 | **emHeader now 124 bytes** (was 123) |
| 2026-01-24 S4T2 | **emHeader Length fixed**: 1 → 2 bytes (Word16 BE) |
| 2026-01-24 S4T2 | **Wire format documented**: Fully updated |
| 2026-01-24 S4T2 | **Log verification**: `00 7c 00 02` confirmed |
| 2026-01-24 S4T2 | **Next suspects**: X448 DH, HKDF, rcAD |
| 2026-01-24 S4T2 | Documentation v12 created |

---

## 83. Open Questions (2026-01-24 S4T2)

1. ✅ ~~KDF Output Order?~~ **FIXED**
2. ✅ ~~E2E Key Length Prefix?~~ **FIXED: Word16 BE**
3. ✅ ~~ehBody Length Prefix?~~ **FIXED: Word16 BE**
4. ✅ ~~emHeader Length Prefix?~~ **FIXED: Word16 BE**
5. ✅ ~~emHeader Size?~~ **FIXED: 124 bytes**
6. ❓ **X448 DH Output correct?** - Test with test vectors
7. ❓ **HKDF Output correct?** - Compare with reference
8. ❓ **rcAD Order?** - our_key1 || peer_key1?
9. 🔥 **Why 2x A_MESSAGE despite all encoding fixes?**

---

## 84. SimpleGo Version Update (2026-01-24 S4T2)

```
SimpleGo v0.1.23-alpha
═══════════════════════════════════════════════════════════════════

Changes:
├── ✅ KDF Output Order Bug fixed
├── ✅ E2E Key Length: Word16 BE (142 bytes total)
├── ✅ ehBody Length: Word16 BE
├── ✅ emHeader Length: Word16 BE
├── ✅ emHeader: 124 bytes (was 123)
├── ✅ Wire format fully corrected
└── 🔍 All known encoding bugs fixed

Status:
├── 2x A_MESSAGE Error remains
├── Server accepts everything (OK)
└── App cannot decrypt

Next debug:
├── Test X448 DH with test vectors
├── Compare HKDF output
└── Verify rcAD order

Suspicion: Crypto layer (no longer encoding layer)

═══════════════════════════════════════════════════════════════════
```

---

## 87. Fix 4: MsgHeader DH Key Length - Word16 (2026-01-24 Finale)

### 87.1 🔥 BUG: DH Key Length in MsgHeader was 1 byte

**Problem:** The length of the DH Public Key (SPKI) in MsgHeader was encoded as 1 byte.

### 87.2 Before (WRONG)

```c
// DH key length as 1 byte
header[p++] = 68;  // ❌ Only 1 byte!
```

### 87.3 After (CORRECT)

```c
// DH key length as Word16 Big-Endian (2 bytes)
header[p++] = 0x00;  // High byte
header[p++] = 68;    // Low byte (0x44)
```

### 87.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Fix implemented | ✅ Word16 BE |
| Status | ✅ **FIXED** |

---

## 88. Fix 7: Payload AAD Length - 236 bytes (2026-01-24 Finale)

### 88.1 🔥 BUG: Payload AAD was 235 instead of 236 bytes

**Problem:** Since emHeader is now 124 bytes (not 123), the AAD must also be adjusted accordingly.

### 88.2 Before (WRONG - 235 bytes)

```c
uint8_t payload_aad[235];
memcpy(payload_aad, ratchet_state.assoc_data, 112);
memcpy(payload_aad + 112, em_header, 123);  // ❌ emHeader is 124!
```

### 88.3 After (CORRECT - 236 bytes)

```c
uint8_t payload_aad[236];
memcpy(payload_aad, ratchet_state.assoc_data, 112);  // rcAD
memcpy(payload_aad + 112, em_header, 124);           // emHeader (124 bytes!)
```

### 88.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Fix implemented | ✅ 236 bytes |
| Status | ✅ **FIXED** |

---

## 90. Complete Verified Wire Format (2026-01-24 Finale)

### 90.1 EncRatchetMessage (Final)

```
EncRatchetMessage Wire Format:
═══════════════════════════════════════════════════════════════════

[2B emHeader-len][124B emHeader][16B emAuthTag][payload... (Tail)]
      Word16 BE

emHeader (124 bytes):
├── 2B  ehVersion (Word16 BE)
├── 16B ehIV (raw)
├── 16B ehAuthTag (raw)
├── 2B  ehBody-len (Word16 BE)
└── 88B ehBody (encrypted MsgHeader)

═══════════════════════════════════════════════════════════════════
```

---

## 92. AAD (Associated Data) - Fully Documented (2026-01-24 Finale)

### 92.1 rcAD (Base Associated Data) - 112 bytes

```
rcAD Structure:
┌────────────────────────────────┬────────────────────────────────┐
│ our_key1_public_raw (56 bytes) │ peer_key1_raw (56 bytes)       │
│ [RAW X448 key, NOT SPKI!]      │ [RAW X448 key, NOT SPKI!]      │
└────────────────────────────────┴────────────────────────────────┘
Total: 112 bytes
```

### 92.2 Header Encryption AAD - 112 bytes

```
Header AAD = rcAD (112 bytes)

Used for: AES-GCM Encryption of MsgHeader
```

### 92.3 Payload Encryption AAD - 236 bytes

```
Payload AAD = rcAD (112 bytes) || emHeader (124 bytes)

┌─────────────────────────────────┬──────────────────────────────────┐
│ rcAD (112 bytes)                │ emHeader (124 bytes)             │
├─────────────────────────────────┼──────────────────────────────────┤
│ our_key1 (56) │ peer_key1 (56)  │ [complete EncMessageHeader]      │
└─────────────────────────────────┴──────────────────────────────────┘
Total: 236 bytes
```

---

## 93. Current Status - A_MESSAGE Remains (2026-01-24 Finale)

### 93.1 Status after ALL Fixes

```
Debugging Status:
═══════════════════════════════════════════════════════════════════

✅ Encoding Layer: FULLY FIXED
├── ✅ All Length-Prefixes Word16 BE
├── ✅ emHeader: 124 bytes
├── ✅ HELLO: 12 bytes
├── ✅ E2E params: 142 bytes
├── ✅ Payload AAD: 236 bytes
└── ✅ Wire format completely verified

✅ KDF Layer: FULLY FIXED
├── ✅ X3DH: hk, nhk, sk
├── ✅ Root KDF: new_root, chain, next_header
└── ✅ Chain KDF: chain', msg_key, header_iv, msg_iv

Server:     ✅ OK (accepts both messages)
App:        ❌ A_MESSAGE (cannot decrypt)

❓ Crypto Layer: SUSPECT
├── ❓ X448 DH Output correct?
├── ❓ HKDF mbedTLS vs cryptonite?
└── ❓ AES-GCM Implementation?

═══════════════════════════════════════════════════════════════════
```

---

## 94. Next Debug Steps (2026-01-24 Finale)

### 94.1 Priority 1: X448 Test Vector Comparison

**Test RFC 7748 test vectors against wolfSSL:**

```c
// RFC 7748 Section 6.2 - X448
uint8_t alice_scalar[56] = { /* from RFC */ };
uint8_t bob_public[56] = { /* from RFC */ };
uint8_t expected_shared[56] = { /* from RFC */ };

uint8_t actual_shared[56];
wc_curve448_shared_secret(bob_public, alice_scalar, actual_shared);

// Compare!
```

### 94.2 Priority 2: Python Comparison

**Same inputs in Python with `cryptography` library**

---

## 95. Updated Bug Status (2026-01-24 Finale)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version Ranges |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 | ✅ FIXED | 2026-01-23 | All 3 DHs different |
| X3DH Salt NULL | ✅ FIXED | 2026-01-23 | 64 zero bytes |
| X3DH Output 32→96 | ✅ FIXED | 2026-01-23 | 96 bytes |
| HKDF SHA256→SHA512 | ✅ FIXED | 2026-01-23 | SHA512 |
| kdf_root Info-String | ✅ FIXED | 2026-01-23 | "SimpleXRootRatchet" |
| kdf_chain Info-String | ✅ FIXED | 2026-01-23 | "SimpleXChainRatchet" |
| ClientMessage Padding | ✅ FIXED | 2026-01-23 S3 | 15904 bytes |
| Ratchet Padding | ✅ FIXED | 2026-01-24 | 14832/15840 bytes |
| KDF Output Order | ✅ FIXED | 2026-01-24 S4 | new_root, chain, next_header |
| E2E Key Length 1→2 | ✅ FIXED | 2026-01-24 S4T2 | Word16 BE |
| ehBody Length 1→2 | ✅ FIXED | 2026-01-24 S4T2 | Word16 BE |
| emHeader Length 1→2 | ✅ FIXED | 2026-01-24 S4T2 | Word16 BE |
| **HELLO prevMsgHash 1→2** | ✅ FIXED | 2026-01-24 Finale | Word16 BE |
| **MsgHeader DH Key 1→2** | ✅ FIXED | 2026-01-24 Finale | Word16 BE |
| **Payload AAD 235→236** | ✅ FIXED | 2026-01-24 Finale | +1 for emHeader |
| **ChainKDF IV Order** | ✅ FIXED | 2026-01-24 Finale | header_iv, msg_iv |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-24 | Crypto layer? |

---

## 96. Extended Changelog (2026-01-24 Finale)

| Date | Change |
|------|--------|
| 2026-01-24 Finale | **HELLO prevMsgHash Length**: 1 → 2 bytes (HELLO: 12 bytes) |
| 2026-01-24 Finale | **MsgHeader DH Key Length**: 1 → 2 bytes Word16 BE |
| 2026-01-24 Finale | **Payload AAD**: 235 → 236 bytes |
| 2026-01-24 Finale | **🔥 ChainKDF IV Order fixed**: header_iv (64-79), msg_iv (80-95) |
| 2026-01-24 Finale | **Wire format fully documented** |
| 2026-01-24 Finale | **HKDF fully documented** |
| 2026-01-24 Finale | **AAD fully documented** |
| 2026-01-24 Finale | Documentation v13 created |

---

## 97. Open Questions (2026-01-24 Finale)

1. ✅ ~~All Length-Prefixes?~~ **FIXED: All Word16 BE**
2. ✅ ~~Wire Format?~~ **FULLY DOCUMENTED**
3. ✅ ~~HKDF Outputs?~~ **FULLY DOCUMENTED**
4. ✅ ~~AAD Sizes?~~ **FIXED: 112 / 236 bytes**
5. ✅ ~~IV Order?~~ **FIXED: header_iv, msg_iv**
6. ❓ **X448 Endianness?** - wolfSSL vs cryptonite
7. ❓ **HKDF Implementation?** - mbedTLS vs cryptonite
8. 🔥 **Why A_MESSAGE despite EVERYTHING?** - Crypto layer

---

## 98. SimpleGo Version Update (2026-01-24 Finale)

```
SimpleGo v0.1.24-alpha
═══════════════════════════════════════════════════════════════════

All known encoding bugs: ✅ FIXED
Wire format: ✅ FULLY VERIFIED
HKDF: ✅ FULLY DOCUMENTED
AAD: ✅ CORRECT (112 / 236 bytes)
IV Order: ✅ FIXED

Changes in this session:
├── ✅ HELLO: 12 bytes (was 11)
├── ✅ MsgHeader DH Key: Word16 BE
├── ✅ Payload AAD: 236 bytes
├── ✅ ChainKDF IVs: header_iv, msg_iv
└── ✅ Wire format completely documented

Status:
├── 2x A_MESSAGE Error remains
├── Server: ✅ OK
├── App: ❌ A_MESSAGE
└── Suspicion: Crypto layer (X448, HKDF)

Next debug:
├── RFC 7748 X448 test vectors
├── Python HKDF comparison
└── Wire dump analysis

═══════════════════════════════════════════════════════════════════
```

---


---

## 104. Current Status after BREAKTHROUGH (2026-01-24)

### 104.1 Status Overview

```
BREAKTHROUGH! 🎉
═══════════════════════════════════════════════════════════════════

✅ Encoding Layer: FULLY FIXED (Bugs 2-7)
✅ KDF Layer: FULLY FIXED (Bugs 1, 8)
✅ Crypto Layer: BUG FOUND! (Bug 9)

All 9 bugs identified and fixes documented!

After applying Fix #9 (wolfSSL X448 Byte-Order):
→ ESP32 and SimpleX App should be able to communicate! 🚀

═══════════════════════════════════════════════════════════════════
```

---

## 105. Extended Changelog (2026-01-24 BREAKTHROUGH)

| Date | Change |
|------|--------|
| 2026-01-24 | **🎉 BREAKTHROUGH: wolfSSL X448 Byte-Order Bug found!** |
| 2026-01-24 | **Python comparison**: reversed keys match! |
| 2026-01-24 | **reverse_bytes() Helper** designed |
| 2026-01-24 | **x448_generate_keypair() Fix** documented |
| 2026-01-24 | **x448_dh() Fix** documented |
| 2026-01-24 | **Encoding Convention** documented: All lengths are Word16 BE |
| 2026-01-24 | **Lessons Learned** documented |
| 2026-01-24 | **All 9 Bugs** fully documented |
| 2026-01-24 | Documentation v14 created |

---

## 106. SimpleGo Version Update (2026-01-24 BREAKTHROUGH)

```
SimpleGo v0.1.25-alpha - BREAKTHROUGH! 🎉
═══════════════════════════════════════════════════════════════════

🎉 CRITICAL BUG FOUND: wolfSSL X448 Byte-Order!

All 9 bugs identified:
├── ✅ Bug 1: KDF Output Order
├── ✅ Bug 2: E2E Key Length (Word16 BE)
├── ✅ Bug 3: HELLO prevMsgHash Length (Word16 BE)
├── ✅ Bug 4: MsgHeader DH Key Length (Word16 BE)
├── ✅ Bug 5: ehBody Length (Word16 BE)
├── ✅ Bug 6: emHeader Length (Word16 BE)
├── ✅ Bug 7: Payload AAD (236 bytes)
├── ✅ Bug 8: ChainKDF IV Order
└── 🎉 Bug 9: wolfSSL X448 Byte-Order ← THE MAIN BUG!

Fix:
└── reverse_bytes() for all X448 keys and DH outputs

After this fix, SimpleGo should communicate with SimpleX App! 🚀

═══════════════════════════════════════════════════════════════════
```

---

## 109. ⚠️ Error Remains Despite Correct Cryptography! (2026-01-24)

### 109.1 Current Status

```
Paradoxical situation:
═══════════════════════════════════════════════════════════════════

✅ All crypto values verified correct (Python match)
✅ Server accepts both messages ("OK")
❌ App shows "error agent AGENT A_MESSAGE"
❌ App CANNOT decrypt messages

HOW CAN THIS BE?!

═══════════════════════════════════════════════════════════════════
```

---

## 110. 🔥 New Hypothesis: E2E Version / KEM Mismatch (2026-01-24)

### 110.1 Observation from Invitation

**Invitation from SimpleX App:**
```
E2E Version Range: 2-3
🔒 KEM key found! (Post-Quantum encryption)
```

**Our response:**
```c
params->version_min = 2;
params->version_max = 2;  // ← Only Version 2!
params->has_kem = false;  // ← No KEM!
```

### 110.2 Possible Problem

```
E2E Version Mismatch?
═══════════════════════════════════════════════════════════════════

App Invitation:
├── version_min = 2
├── version_max = 3  ← HIGHER THAN OURS!
└── has_kem = true   ← HAS KEM KEY!

Our Response:
├── version_min = 2
├── version_max = 2  ← ONLY VERSION 2
└── has_kem = false  ← NO KEM

Questions:
1. Must Version 3 be supported?
2. Must KEM key be processed even without PQ?
3. Is there version negotiation we don't understand?

═══════════════════════════════════════════════════════════════════
```

---

## 111. Verified Wire Format (2026-01-24 Session 5)

### 111.1 EncRatchetMessage (confirmed)

```
[00 7C]         emHeader length (Word16 BE = 124)
[124 bytes]     emHeader (EncMessageHeader)
[16 bytes]      payload AuthTag
[N bytes]       encrypted payload (Tail)
```

### 111.2 emHeader / EncMessageHeader (124 bytes, confirmed)

```
[00 02]         ehVersion (Word16 BE = 2)
[16 bytes]      ehIV (raw, no length prefix)
[16 bytes]      ehAuthTag (raw)
[00 58]         ehBody length (Word16 BE = 88)
[88 bytes]      encrypted MsgHeader
```

---

## 112. Python Test Script for Future Comparisons (2026-01-24)

### 112.1 Reference Script

```python
#!/usr/bin/env python3
"""
SimpleX Protocol Crypto Verification Tool
Compares ESP32 outputs with Python reference
"""

from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

def x448_dh(their_public_bytes: bytes, my_private_bytes: bytes) -> bytes:
    """Perform X448 DH key exchange"""
    private_key = X448PrivateKey.from_private_bytes(my_private_bytes)
    public_key = X448PublicKey.from_public_bytes(their_public_bytes)
    return private_key.exchange(public_key)

def hkdf_sha512(salt: bytes, ikm: bytes, info: bytes, length: int) -> bytes:
    """HKDF-SHA512 key derivation"""
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=length,
        salt=salt if len(salt) > 0 else None,
        info=info
    )
    return hkdf.derive(ikm)
```

---

## 113. Next Debug Steps (2026-01-24 Session 5)

### 113.1 Priority 1: Check E2E Version

**Questions to clarify:**
- Must `version_max = 3` be supported?
- What changes with Version 3?
- Is there version negotiation?

### 113.2 Priority 2: Check KEM Handling

**Questions to clarify:**
- Must KEM key be processed even without PQ?
- Is there a "KEM present but not used" encoding?
- How to signal "no PQ support"?

---

## 114. Updated Bug Status (2026-01-24 Session 5)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| KDF Output Order | ✅ FIXED | 2026-01-24 | new_root[0-31], next_header[64-95] |
| E2E Key Length | ✅ FIXED | 2026-01-24 | Word16 BE |
| HELLO prevMsgHash | ✅ FIXED | 2026-01-24 | Word16 BE |
| MsgHeader DH Key | ✅ FIXED | 2026-01-24 | Word16 BE |
| ehBody Length | ✅ FIXED | 2026-01-24 | Word16 BE |
| emHeader Length | ✅ FIXED | 2026-01-24 | Word16 BE |
| Payload AAD | ✅ FIXED | 2026-01-24 | 236 bytes |
| ChainKDF IV Order | ✅ FIXED | 2026-01-24 | header_iv[64-79], msg_iv[80-95] |
| wolfSSL X448 Byte-Order | ✅ FIXED | 2026-01-24 | reverse_bytes() |
| **Cryptography** | ✅ VERIFIED | 2026-01-24 | Python match! |
| **E2E Version/KEM?** | ❓ TO CHECK | 2026-01-24 | Version 3? KEM? |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-24 | Cause unknown |

---

## 115. Extended Changelog (2026-01-24 Session 5)

| Date | Change |
|------|--------|
| 2026-01-24 S5 | **wolfSSL X448 Fix implemented** |
| 2026-01-24 S5 | **🎉 All crypto values verified correct!** |
| 2026-01-24 S5 | **Python match for all DH, HKDF outputs** |
| 2026-01-24 S5 | **New hypothesis: E2E Version/KEM Mismatch** |
| 2026-01-24 S5 | **Invitation shows: version 2-3, KEM present** |
| 2026-01-24 S5 | **Python test script documented** |
| 2026-01-24 S5 | Documentation v15 created |

---

## 116. Open Questions (2026-01-24 Session 5)

1. ✅ ~~X448 DH correct?~~ **VERIFIED: Python match!**
2. ✅ ~~HKDF correct?~~ **VERIFIED: Python match!**
3. ✅ ~~All keys correct?~~ **VERIFIED: Python match!**
4. ✅ ~~All IVs correct?~~ **VERIFIED: Python match!**
5. ❓ **E2E Version**: Must Version 3 be supported?
6. ❓ **KEM Handling**: Must KEM be processed?
7. ❓ **AES-GCM**: Is the encryption itself correct?
8. 🔥 **Why A_MESSAGE despite correct crypto?**

---

## 117. SimpleGo Version Update (2026-01-24 Session 5)

```
SimpleGo v0.1.26-alpha - Cryptography VERIFIED! 🎉
═══════════════════════════════════════════════════════════════════

✅ wolfSSL X448 Byte-Order Fix: IMPLEMENTED
✅ All crypto values: PYTHON MATCH VERIFIED!
   ├── X448 DH: ✅
   ├── X3DH HKDF: ✅
   ├── Root KDF: ✅
   └── Chain KDF: ✅

❌ Error remains: A_MESSAGE (2x)

New hypothesis:
├── E2E Version Mismatch? (App: 2-3, We: 2)
├── KEM Handling? (App sends KEM key)
└── AES-GCM Details?

═══════════════════════════════════════════════════════════════════
```

---

## 118. Lessons Learned Update (2026-01-24 Session 5)

### 118.1 New Insights

| # | Insight |
|---|---------|
| 1 | **Python comparison tests are ESSENTIAL** - Only way to find wolfSSL bug |
| 2 | **Crypto libraries are NOT interchangeable** - Byte order can vary |
| 3 | **Systematic debugging** - Verify each component in isolation |
| 4 | **Correct crypto ≠ Working protocol** - There's more than just keys! |

---


---

## 122. AAD Construction Hypothesis (2026-01-24 Session 6)

### 122.1 AAD Order Question

```
AAD (Associated Data) Order:
═══════════════════════════════════════════════════════════════════

Header AAD (112 bytes):
rcAD = our_key1_raw (56) || peer_key1_raw (56)
       ─────────────────    ─────────────────
       ESP32 key first?     Then App key?

Or should it be:
rcAD = peer_key1_raw (56) || our_key1_raw (56)
       ─────────────────    ─────────────────
       App key first?       Then ESP32 key?

The order must EXACTLY match what the app expects!

═══════════════════════════════════════════════════════════════════
```

---

## 123. Open Hypotheses Detail (2026-01-24 Session 6)

### 123.1 Hypothesis 1: KEM Handling

```
KEM (Key Encapsulation Mechanism) Question:
═══════════════════════════════════════════════════════════════════

App sends in Invitation:
├── E2E Version: 2-3
└── KEM key: PRESENT (Post-Quantum)

We respond:
├── E2E Version: 2
└── KEM: NONE (ignored)

Questions:
1. Must the KEM key at least be parsed?
2. Is there a "KEM acknowledged but not used" flag?
3. Is Version 2 without KEM compatible with Version 2-3 + KEM?

═══════════════════════════════════════════════════════════════════
```

### 123.2 Hypothesis 2: AAD Construction Order

### 123.3 Hypothesis 3: AuthTag Position

```
AuthTag Encoding:
═══════════════════════════════════════════════════════════════════

Currently we send:
[emHeader-len][emHeader][payload AuthTag][encrypted payload]
                        ^^^^^^^^^^^^^^^^
                        WITHOUT length prefix (as "Tail")

Is that correct? Or should it be:
[emHeader-len][emHeader][emAuthTag-len][emAuthTag][payload]
                        ^^^^^^^^^^^^^^
                        WITH length prefix?

Haskell "Tail" types have NO length prefix!

═══════════════════════════════════════════════════════════════════
```

---

## 124. Crypto Verification Summary (2026-01-24 Session 6)

### 124.1 All Values Match!

```
Python vs ESP32 Comparison (100% Match):
═══════════════════════════════════════════════════════════════════

Component           Python                 ESP32               Match
─────────────────────────────────────────────────────────────────────
X3DH dh1            62413115799d7f0a...   62413115799d7f0a... ✅
X3DH dh2            27d885f054cc7775...   27d885f054cc7775... ✅
X3DH dh3            8dd161101f1c730f...   8dd161101f1c730f... ✅
header_key (hk)     c65dc5381323839f...   c65dc5381323839f... ✅
root_key (rk)       8b30f093a3b5d75b...   8b30f093a3b5d75b... ✅
Root KDF new_rk     de394bc567ae2e70...   de394bc567ae2e70... ✅
Root KDF ck         5d473bb5b24acc9d...   5d473bb5b24acc9d... ✅
Root KDF next_hk    d3d8fbb361ea2e65...   d3d8fbb361ea2e65... ✅
Chain KDF mk        7041ce31dc681820...   7041ce31dc681820... ✅
Chain KDF header_iv 708dee3b187dd7ec...   708dee3b187dd7ec... ✅
Chain KDF msg_iv    e3b28a0d3df93e3c...   e3b28a0d3df93e3c... ✅

═══════════════════════════════════════════════════════════════════
```

### 124.2 Conclusion

**The cryptography is NOT the problem!**

---

## 125. Next Debug Steps (2026-01-24 Session 6)

### 125.1 Priority 1: Find App Debug Logs

```
Possible log sources:
├── Android: adb logcat | grep -i simplex
├── iOS: Console.app
├── Desktop: Electron DevTools → Network/Console
└── App-internal debug options?
```

### 125.2 Priority 2: Analyze Haskell Decoder

### 125.3 Priority 3: Compare Wire Capture

---

## 126. Updated Bug Status (2026-01-24 Session 6)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| KDF Output Order | ✅ FIXED | S4 | new_root[0-31], next_header[64-95] |
| Length-Prefixes (6x) | ✅ FIXED | S4 | All Word16 BE |
| Payload AAD | ✅ FIXED | S4 | 236 bytes |
| ChainKDF IV Order | ✅ FIXED | S4 | header_iv[64-79], msg_iv[80-95] |
| wolfSSL X448 Byte-Order | ✅ FIXED | S4 | reverse_bytes() |
| **Cryptography** | ✅ VERIFIED | S5 | 100% Python match! |
| **Wire Format** | ✅ VERIFIED | S6 | All offsets correct |
| **KEM Handling?** | ❓ TO CHECK | S6 | Must KEM be processed? |
| **AAD Order?** | ❓ TO CHECK | S6 | our \|\| peer or peer \|\| our? |
| **AuthTag Encoding?** | ❓ TO CHECK | S6 | With or without length prefix? |
| **A_MESSAGE (2x)** | 🔥 CURRENT | S6 | Decryption fails |

---

## 127. Extended Changelog (2026-01-24 Session 6)

| Date | Change |
|------|--------|
| 2026-01-24 S6 | **Handshake flow documented** (Contact Address q=c) |
| 2026-01-24 S6 | **Wire format detail view** with all offsets |
| 2026-01-24 S6 | **SPKI Header** documented (12 bytes) |
| 2026-01-24 S6 | **App console logs** analyzed (2x error agent) |
| 2026-01-24 S6 | **New hypotheses**: KEM, AAD order, AuthTag |
| 2026-01-24 S6 | **Haskell encoding rules** documented (Tail vs ByteString) |
| 2026-01-24 S6 | Documentation v16 created |

---

## 128. Open Questions (2026-01-24 Session 6)

1. ✅ ~~Cryptography correct?~~ **VERIFIED: 100% Python match!**
2. ✅ ~~Wire format correct?~~ **VERIFIED: All offsets match**
3. ❓ **KEM Handling**: Must KEM key be processed even without PQ?
4. ❓ **AAD Order**: our_key || peer_key or reversed?
5. ❓ **AuthTag Encoding**: With or without length prefix?
6. ❓ **Haskell "Tail"**: Which fields are Tail types?
7. 🔥 **Why A_MESSAGE despite correct crypto and wire format?**

---

## 129. SimpleGo Version Update (2026-01-24 Session 6)

```
SimpleGo v0.1.26-alpha - Handshake Flow Analysis
═══════════════════════════════════════════════════════════════════

✅ Cryptography: 100% VERIFIED (Python match)
✅ Wire Format: All offsets VERIFIED
✅ Server: Accepts all messages

❌ App: "error agent" (2x)
❌ App: Cannot decrypt
❌ Connection: "waiting for acceptance"

═══════════════════════════════════════════════════════════════════
```

---

## 130. Lessons Learned (2026-01-24 Session 6)

### 130.1 New Insights

| # | Insight |
|---|---------|
| 1 | **Contact Address flow requires "acceptance"** - For q=c links, AgentConfirmation must be successfully decrypted |
| 2 | **App console shows only "error agent"** - No details about the error cause |
| 3 | **Cryptography is NOT the problem** - 100% Python match |
| 4 | **KEM might be required** - App sends PQ keys, we ignore them |
| 5 | **Haskell "Tail" types** - Have no length prefix, important for encoding! |

---

## 134. SMPQueueInfo Encoding for clientVersion=4 (2026-01-24 S6)

### 134.1 Haskell Source

```haskell
smpEncode (SMPQueueInfo clientVersion SMPQueueAddress {smpServer, senderId, dhPublicKey, queueMode})
  | clientVersion >= shortLinksSMPClientVersion = addrEnc <> maybe "" smpEncode queueMode
```

**Where:** `shortLinksSMPClientVersion = 4`

### 134.2 Correct Wire Format

```
SMPQueueInfo for clientVersion >= 4:
═══════════════════════════════════════════════════════════════════

Offset  Bytes  Description
─────────────────────────────────────────────────────────────────
0-1     2      clientVersion (Word16 BE)
2       1      host count
3       1      host1 length
4-N     N      host1 string
N+1     1      port length              ← WAS WRONG (space)!
N+2     M      port string
...     1      keyHash length (32)
...     32     keyHash
...     1      senderId length
...     N      senderId
...     1      dhPublicKey length (44)
...     44     dhPublicKey (X25519 SPKI)
...     ?      queueMode (optional)

═══════════════════════════════════════════════════════════════════
```

---

## 135. Raw AgentConnInfoReply Analysis (2026-01-24 S6)

### 135.1 Current Output (224 bytes, after Port Fix)

```
AgentConnInfoReply Byte Layout:
═══════════════════════════════════════════════════════════════════

Offset  Hex   Description
─────────────────────────────────────────────────────────────────
0       44    'D' (AgentConnInfoReply tag)
1       01    1 queue in list
2-3     00 04 clientVersion = 4 (Word16 BE)
4       01    1 host
5       16    22 (host length)
6-27    ...   "smp3.simplexonflux.com" (22 bytes)
28      04    4 (port length) ✅ FIXED!
29-32   ...   "5223" (4 bytes)
33      20    32 (keyHash length)
34-65   ...   keyHash (32 bytes)
66      18    24 (senderId length)
67-90   ...   senderId (24 bytes)
91      2c    44 (dhPublicKey length)
92-135  ...   X25519 SPKI key (44 bytes)
136+    7b... JSON connInfo: {"v":"1-16","event":"x.info"...

═══════════════════════════════════════════════════════════════════
```

---

## 136. Open Questions (2026-01-24 S6 Continuation)

### 136.1 queueMode Handling

```
Question: What is expected for queueMode?

Option A: Send nothing (leave empty)
Option B: '0' (0x30) for QMSubscription
Option C: 'M' (0x4D) for QMMessaging
Option D: No byte at all

Currently we send '0' (0x30) - is that correct?
```

---

## 137. Updated Bug Status (2026-01-24 S6 Continuation)

| # | Bug | Status | Date | Solution |
|---|-----|--------|------|----------|
| 1 | KDF Output Order | ✅ FIXED | S4 | new_root[0-31], next_header[64-95] |
| 2-6 | Length-Prefixes (5x) | ✅ FIXED | S4 | All Word16 BE |
| 7 | Payload AAD | ✅ FIXED | S4 | 236 bytes |
| 8 | ChainKDF IV Order | ✅ FIXED | S4 | header_iv[64-79], msg_iv[80-95] |
| 9 | wolfSSL X448 Byte-Order | ✅ FIXED | S4 | reverse_bytes() |
| **10** | **SMPQueueInfo Port** | ✅ FIXED | S6 | **Length instead of Space!** |
| | Cryptography | ✅ VERIFIED | S5 | 100% Python match |
| | Wire Format (Ratchet) | ✅ VERIFIED | S6 | Haskell source confirmed |
| | queueMode? | ❓ TO CHECK | S6 | '0' or empty? |
| | **A_MESSAGE (2x)** | 🔥 CURRENT | S6 | Error remains! |

---

## 138. Extended Changelog (2026-01-24 S6 Continuation)

| Date | Change |
|------|--------|
| 2026-01-24 S6 | **🔥 Bug 10: Port encoding fixed** (Space → Length) |
| 2026-01-24 S6 | **Haskell source analysis** for EncRatchetMessage |
| 2026-01-24 S6 | **Haskell source analysis** for EncMessageHeader |
| 2026-01-24 S6 | **Version-dependent encoding** documented (v2 vs v3+) |
| 2026-01-24 S6 | **SMPQueueInfo format** for clientVersion=4 documented |
| 2026-01-24 S6 | **Raw AgentConnInfoReply** analyzed (224 bytes) |
| 2026-01-24 S6 | Documentation v17 created |

---

## 139. Next Steps (2026-01-24 S6 Continuation)

### 139.1 Priority 1: Test queueMode

```c
// Option A: Send nothing
// buf[p++] = '0';  // Comment out

// Option B: Send '0' (current)
buf[p++] = '0';

// Option C: Send 'M'
buf[p++] = 'M';
```

---

## 140. SimpleGo Version Update (2026-01-24 S6 Continuation)

```
SimpleGo v0.1.27-alpha - Port Encoding Fix!
═══════════════════════════════════════════════════════════════════

✅ Bug 10 FIXED: SMPQueueInfo Port (Space → Length)
✅ Wire Format: Haskell source VERIFIED
✅ Cryptography: 100% Python match

❌ App: Still "error agent A_MESSAGE"

Open questions:
├── queueMode: '0' or empty or 'M'?
├── AgentConfirmation: Structure correct?
└── More hidden bugs?

10 bugs found and fixed! Error remains...

═══════════════════════════════════════════════════════════════════
```

---

## 144. Verified Raw Output (2026-01-24 S6 Finale)

### 144.1 AgentConnInfoReply (225 bytes, after all fixes)

```
AgentConnInfoReply Byte Layout (FINAL):
═══════════════════════════════════════════════════════════════════

Offset  Hex     Description                              Status
─────────────────────────────────────────────────────────────────
0       44      'D' (AgentConnInfoReply tag)             ✅
1-2     00 01   queue count = 1 (Word16 BE)              ✅ FIXED!
3-4     00 04   clientVersion = 4 (Word16 BE)            ✅
5       01      1 host                                   ✅
6       16      22 (host length)                         ✅
7-28    ...     "smp3.simplexonflux.com" (22 bytes)      ✅
29      04      4 (port length)                          ✅ FIXED!
30-33   ...     "5223" (4 bytes)                         ✅
34      20      32 (keyHash length = 0x20)               ✅
35-66   ...     keyHash (32 bytes)                       ✅
67      18      24 (senderId length)                     ✅
68-91   ...     senderId (24 bytes)                      ✅
92      2c      44 (dhPublicKey length)                  ✅
93-136  ...     X25519 SPKI (44 bytes)                   ✅
137+    ...     connInfo JSON (Tail, no length)          ✅

═══════════════════════════════════════════════════════════════════
```

---

## 145. Current Crypto Values (2026-01-24 S6 Finale)

### 145.1 X3DH

```
X3DH Key Agreement:
═══════════════════════════════════════════════════════════════════

dh1: 3b270d17260a1fbb...
dh2: 407ee5f7d1bce395...
dh3: 133af8004f69a370...

HKDF Output (96 bytes):
hk (header_key):     cba93f5b46e74136...
nhk (next_hdr_key):  [32 bytes]
rk (root_key):       4eb0de5ecbe83f1e...

═══════════════════════════════════════════════════════════════════
```

---


---

## 147. Remaining Hypotheses (2026-01-24 S6 Finale)

### 147.1 Hypothesis 1: rcAD Order

```
Current implementation:
rcAD = our_key1 (56) || peer_key1 (56)

Question: Is the order correct?
- Sender perspective vs Receiver perspective?
- App might expect peer_key1 || our_key1?
```

### 147.2 Hypothesis 2: pubKeyBytes Format

```
We use: raw 56-byte X448 keys for rcAD

Haskell: pubKeyBytes = BA.convert k

Question: Is BA.convert really raw bytes or somehow transformed?
```

### 147.3 Hypothesis 3: App-specific Validation

### 147.4 Hypothesis 4: Timing/State Issue

---

## 148. Next Debug Steps (2026-01-24 S6 Finale)

### 148.1 Priority 1: App-side Logs

**The "error agent" message is too generic!**

### 148.2 Priority 2: Wireshark Capture

### 148.3 Priority 3: Test SimpleX CLI

### 148.4 Priority 4: GitHub Issue

---

## 149. Updated Bug Status (2026-01-24 S6 Finale)

| Bug | Status | Session | Solution |
|-----|--------|---------|----------|
| E2E key length | ✅ | S4 | Word16 BE |
| HELLO prevMsgHash | ✅ | S4 | Word16 BE |
| MsgHeader DH key | ✅ | S4 | Word16 BE |
| ehBody length | ✅ | S4 | Word16 BE |
| emHeader length | ✅ | S4 | Word16 BE |
| Payload AAD | ✅ | S4 | 236 bytes |
| KDF root output | ✅ | S4 | Correct order |
| chainKDF IV order | ✅ | S4 | header=64-79, msg=80-95 |
| wolfSSL X448 | ✅ | S5 | byte reversal |
| SMPQueueInfo port | ✅ | S6 | length prefix |
| smpQueues count | ✅ | S6 | Word16 BE |
| queueMode Nothing | ✅ | S6 | Send nothing |
| **Cryptography** | ✅ | S5 | 100% Python match |
| **Wire Format** | ✅ | S6 | Haskell source verified |
| **rcAD Order?** | ❓ | S6 | To check |
| **A_MESSAGE (2x)** | 🔥 | S6 | PROBLEM PERSISTS! |

---

## 150. Extended Changelog (2026-01-24 S6 Finale)

| Date | Change |
|------|--------|
| 2026-01-24 S6F | **🔥 Bug 11: smpQueues count** (1 byte → Word16 BE) |
| 2026-01-24 S6F | **🔥 Bug 12: queueMode Nothing** ('0' → nothing) |
| 2026-01-24 S6F | **All 12 bugs documented** |
| 2026-01-24 S6F | **Current crypto values** documented |
| 2026-01-24 S6F | **Raw output verified** (225 bytes) |
| 2026-01-24 S6F | **Remaining hypotheses** documented |
| 2026-01-24 S6F | Documentation v18 created |

---

## 151. SimpleGo Version Update (2026-01-24 S6 Finale)

```
SimpleGo v0.1.28-alpha - ALL ENCODING ERRORS FIXED!
═══════════════════════════════════════════════════════════════════

12 bugs found and fixed:
├── 7× Length-Prefix (Word16 BE)
├── 2× KDF Order
├── 1× wolfSSL X448 byte-order
├── 1× Port Space→Length
└── 1× queueMode Nothing

Verified:
├── ✅ Cryptography: 100% Python match
├── ✅ Wire Format: Haskell source confirmed
├── ✅ Server: Accepts all messages

PROBLEM:
└── ❌ App: "error agent A_MESSAGE" remains!

═══════════════════════════════════════════════════════════════════
```

---

## 156. 🔥 New Focus Hypothesis: MsgHeader Parsing (2026-01-24)

### 156.1 The Error Comes from PARSING!

```haskell
-- From Crypto.hs:
decryptHeader k EncMessageHeader {ehVersion, ehBody, ehAuthTag, ehIV} = do
  -- Decryption works (Auth-Tag matches!)
  header <- decryptAEAD k ehIV rcAD ehBody ehAuthTag `catchE` \_ -> throwE CERatchetHeader
  
  -- HERE is where the error comes from: Parsing fails!
  parseE' CryptoHeaderError (msgHeaderP ehVersion) header
  --      ^^^^^^^^^^^^^^^^^
  --      THIS is the error!
```

### 156.2 What This Means

```
Error Analysis:
═══════════════════════════════════════════════════════════════════

1. AES-GCM Decryption: ✅ SUCCESSFUL
   └── Auth-Tag matches, data is decrypted

2. MsgHeader Parsing: ❌ FAILURE
   └── CryptoHeaderError is thrown
   └── parseE' fails
   └── msgHeaderP cannot parse the data

═══════════════════════════════════════════════════════════════════
```

---

## 157. Python Test Script for AES-GCM (2026-01-24 S7)

### 157.1 Reference Script

```python
#!/usr/bin/env python3
"""
AES-GCM 16-byte IV Verification Tool
Compares ESP32 mbedTLS with Python/OpenSSL
"""

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def verify_aesgcm(header_key_hex, header_iv_hex, rcAD_hex, 
                  msg_header_hex, esp32_tag_hex, esp32_ct_hex):
    # ... implementation ...
    pass
```

---

## 158. Updated Bug Status (2026-01-24 Session 7)

| Bug | Status | Session | Solution |
|-----|--------|---------|----------|
| E2E key length | ✅ | S4 | Word16 BE |
| HELLO prevMsgHash | ✅ | S4 | Word16 BE |
| MsgHeader DH key | ✅ | S4 | Word16 BE |
| ehBody length | ✅ | S4 | Word16 BE |
| emHeader length | ✅ | S4 | Word16 BE |
| Payload AAD | ✅ | S4 | 236 bytes |
| KDF root output | ✅ | S4 | Correct order |
| chainKDF IV order | ✅ | S4 | header=64-79, msg=80-95 |
| wolfSSL X448 | ✅ | S5 | byte reversal |
| SMPQueueInfo port | ✅ | S6 | length prefix |
| smpQueues count | ✅ | S6 | Word16 BE |
| queueMode Nothing | ✅ | S6 | Send nothing |
| **Cryptography** | ✅ | S5+S7 | 100% Python match |
| **AES-GCM 16-byte IV** | ✅ | S7 | mbedTLS == Python |
| **Wire Format** | ✅ | S6 | Haskell source verified |
| **MsgHeader Parsing?** | ❓ | S7 | New hypothesis! |
| **A_MESSAGE (2x)** | 🔥 | S7 | Parsing problem! |

---

## 159. Extended Changelog (2026-01-24 Session 7)

| Date | Change |
|------|--------|
| 2026-01-24 S7 | **cryptonite GHASH code analyzed** (16-byte IV handling) |
| 2026-01-24 S7 | **AES-GCM debug output added** to ESP32 code |
| 2026-01-24 S7 | **🎉 AES-GCM verified: mbedTLS == Python/OpenSSL** |
| 2026-01-24 S7 | **New hypothesis: MsgHeader Parsing** after decryption |
| 2026-01-24 S7 | **Python test script** for AES-GCM verification created |
| 2026-01-24 S7 | Documentation v19 created |

---

## 160. SimpleGo Version Update (2026-01-24 Session 7)

```
SimpleGo v0.1.29-alpha - AES-GCM VERIFIED! 🎉
═══════════════════════════════════════════════════════════════════

Session 7 Result:
├── ✅ AES-GCM with 16-byte IV: VERIFIED
├── ✅ GHASH Transformation: IDENTICAL
├── ✅ mbedTLS == Python/OpenSSL
└── ✅ AuthTag: CORRECT

═══════════════════════════════════════════════════════════════════
```

---

## 161. Open Questions (2026-01-24 Session 7)

1. ✅ ~~AES-GCM with 16-byte IV correct?~~ **VERIFIED: Python match!**
2. ✅ ~~GHASH transformation identical?~~ **VERIFIED: mbedTLS == cryptonite**
3. ❓ **MsgHeader format after decrypt** - Does the layout match exactly?
4. ❓ **Padding format** - How does `unPad` work?
5. ❓ **Parser msgHeaderP** - What exactly does it expect?
6. 🔥 **Why A_MESSAGE despite correct crypto?**

---

## 162. Lessons Learned (2026-01-24 Session 7)

### 162.1 New Insights

| # | Insight |
|---|---------|
| 1 | **GCM with 16-byte IV requires GHASH** - Not like 12-byte IV! |
| 2 | **mbedTLS and OpenSSL/cryptonite are compatible** - Same GHASH! |
| 3 | **Auth-Tag success ≠ Parsing success** - Decryption can work but parsing fail! |
| 4 | **Error localization important** - CryptoHeaderError comes from PARSER! |

---

## 163. Hypothesis: 1-byte vs 2-byte Length Prefix (2026-01-24 S7F)

### 163.1 The Theory

**Haskell encodeLarge function:**

```haskell
encodeLarge v s
  | v >= pqRatchetE2EEncryptVersion = smpEncode $ Large s  -- Version 3+: 2-byte
  | otherwise = smpEncode s                                -- Version 2: 1-byte!
```

### 163.2 ❌ Result: NO IMPROVEMENT!

```
Test Result:
═══════════════════════════════════════════════════════════════════

❌ 2x error agent AGENT A_MESSAGE

The 1-byte length prefix hypothesis was WRONG!

═══════════════════════════════════════════════════════════════════
```

---

## 164. Analysis: Why Was the Hypothesis Wrong? (2026-01-24 S7F)

### 164.1 The largeP Parser is TOLERANT!

```haskell
largeP :: Parser ByteString
largeP = do
  len1 <- peekWord8'
  if len1 < 32 
    then unLarge <$> smpP   -- First byte < 32: 2-byte length (Large)
    else smpP               -- First byte >= 32: 1-byte length
```

### 164.2 Insight

**CONCLUSION: We were RIGHT with 2-byte length prefixes! The problem is elsewhere!**

---

## 165. Code Rollback Performed (2026-01-24 S7F)

### 165.1 Back to Correct Version

```c
// CORRECT (active again):
uint8_t em_header[124];
em_header[hp++] = 0x00; 
em_header[hp++] = 0x58;  // ehBody-len (2 bytes, Word16 BE)

uint8_t payload_aad[236];  // 112 + 124

output[p++] = 0x00; 
output[p++] = 0x7C;  // emHeader-len (2 bytes, Word16 BE)
```

---

## 166. Updated Exclusion List (2026-01-24 S7F)

### 166.1 What We EXCLUDED

| Hypothesis | Status | Evidence |
|------------|--------|----------|
| AES-GCM wrong | ❌ Excluded | Python match |
| 16-byte IV problem | ❌ Excluded | Python match |
| X448 DH wrong | ❌ Excluded | Python match |
| X3DH HKDF wrong | ❌ Excluded | Python match |
| Root KDF wrong | ❌ Excluded | Python match |
| Chain KDF wrong | ❌ Excluded | Python match |
| 1-byte vs 2-byte length | ❌ Excluded | Parser is tolerant |
| 12 Encoding bugs | ✅ All fixed | Server accepts |

---

## 167. 🔥 New Focus Hypothesis: X3DH Parameter Asymmetry (2026-01-24 S7F)

### 167.1 The Problem

**The app initializes as RECEIVER with `initRcvRatchet`:**

```haskell
initRcvRatchet :: RatchetKEMState -> RcvE2ERatchetParams -> Ratchet
initRcvRatchet rks RcvE2ERatchetParams {..} = Ratchet {..}
  where
    -- ...
    rcNHKr = sndHK  -- Header Key for RECEIVING
```

### 167.2 Critical Question

```
Is sndHK from pqX3dhRcv == hk from pqX3dhSnd?
═══════════════════════════════════════════════════════════════════

US (Sender):
  pqX3dhSnd → hk (for encrypting the header)

APP (Receiver):  
  pqX3dhRcv → sndHK (for decrypting the header)
             └── rcNHKr = sndHK

MUST BE IDENTICAL!

But: Are the DH calculations symmetric?

═══════════════════════════════════════════════════════════════════
```

---

## 168. Updated Bug Status (2026-01-24 S7F)

| Bug | Status | Session | Solution |
|-----|--------|---------|----------|
| E2E key length | ✅ | S4 | Word16 BE |
| HELLO prevMsgHash | ✅ | S4 | Word16 BE |
| MsgHeader DH key | ✅ | S4 | Word16 BE |
| ehBody length | ✅ | S4 | Word16 BE |
| emHeader length | ✅ | S4 | Word16 BE |
| Payload AAD | ✅ | S4 | 236 bytes |
| KDF root output | ✅ | S4 | Correct order |
| chainKDF IV order | ✅ | S4 | header=64-79, msg=80-95 |
| wolfSSL X448 | ✅ | S5 | byte reversal |
| SMPQueueInfo port | ✅ | S6 | length prefix |
| smpQueues count | ✅ | S6 | Word16 BE |
| queueMode Nothing | ✅ | S6 | Send nothing |
| **Cryptography** | ✅ | S5+S7 | 100% Python match |
| **AES-GCM 16-byte IV** | ✅ | S7 | mbedTLS == Python |
| **1-byte length prefix** | ❌ | S7F | DISPROVEN |
| **X3DH Parameter Order?** | ❓ | S7F | New hypothesis! |
| **A_MESSAGE (2x)** | 🔥 | S7F | Problem persists! |

---

## 169. Extended Changelog (2026-01-24 S7F)

| Date | Change |
|------|--------|
| 2026-01-24 S7F | **1-byte length prefix hypothesis tested** |
| 2026-01-24 S7F | **❌ Hypothesis DISPROVEN** - Parser is tolerant |
| 2026-01-24 S7F | **Code rollback** to 2-byte lengths |
| 2026-01-24 S7F | **largeP parser analyzed** - <32 = Large |
| 2026-01-24 S7F | **New hypothesis: X3DH Parameter Asymmetry** |
| 2026-01-24 S7F | Documentation v20 created |

---

## 170. SimpleGo Version Update (2026-01-24 S7F)

```
SimpleGo v0.1.29-alpha - 1-byte Length Hypothesis DISPROVEN!
═══════════════════════════════════════════════════════════════════

Session 7 Continuation:
├── ❌ 1-byte length prefix: DISPROVEN
├── ✅ 2-byte lengths: Active again (were correct!)
├── ✅ Code rollback: Completed
└── 🔍 New hypothesis: X3DH Parameter Asymmetry

Excluded:
├── ❌ AES-GCM (Python match)
├── ❌ 16-byte IV (Python match)
├── ❌ X448 DH (Python match)
├── ❌ HKDF (Python match)
├── ❌ 12 Encoding bugs (all fixed)
├── ❌ 1-byte vs 2-byte lengths (Parser tolerant)
└── ❌ Wire Format (Haskell source confirmed)

PROBLEM:
└── ❌ App: "error agent A_MESSAGE" remains!

═══════════════════════════════════════════════════════════════════
```

---


**DOCUMENT UPDATED: 2026-01-24 Session 7 Deep Research v21 - 🏆 FIRST native SMP implementation! Tail encoding discovered! 🔥**
