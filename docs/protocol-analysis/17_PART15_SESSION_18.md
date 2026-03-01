![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 15: Session 18
# BUG #18 GELÖST! Wire-Format Root Cause + E2E Layer 2 Decrypt SUCCESS

**Document Version:** v32  
**Date:** 2026-02-05 Session 18  
**Status:** BUG #18 SOLVED — E2E Layer 2 Decrypt SUCCESS  
**Previous:** Part 14 - Session 17 (Key Consistency Debug)

---

## 325. Session 18 Overview

### 325.1 Starting Point

After Session 17 we had:
- Key consistency investigation started (debug test pending)
- Evgeny's previous answers re-analyzed
- Length prefix difference discovered (Contact vs Reply Queue)
- rcAD order analyzed (staying with OUR||PEER)
- MAC Mismatch persisting on Reply Queue E2E Layer 2

### 325.2 Session 18 BREAKTHROUGH

**BUG #18 SOLVED AFTER WEEKS OF DEBUGGING!**

Root Cause: `envelope_len` was calculated from `plain_len - 2` instead of using the actual `raw_len_prefix` value. This included 102 bytes of SMP block-padding (`0x23`) in the envelope data, causing every offset to be wrong and producing MAC mismatches.

**Fix: ONE LINE of code.**

```c
envelope_len = raw_len_prefix;  // statt plain_len - rq_prefix_len
```

**Result: Method 0 (decrypt_client_msg) SUCCESS! 15904 bytes decrypted!**

### 325.3 Session 18 Achievements

1. ✅ Contact Queue vs Reply Queue code analysis completed
2. ✅ ClientMsgEnvelope wire-format fully analyzed (Claude Code #1)
3. ✅ Tuple-encoding + wrapper chain analyzed (Claude Code #2)
4. ✅ Root cause identified: 102 bytes SMP padding in envelope_len
5. ✅ **BUG #18 SOLVED! One line fix!**
6. ✅ E2E Layer 2 Decrypt: 15904 bytes AgentConfirmation decrypted
7. ✅ Decrypted plaintext contains AgentConfirmation + EncRatchetMessage

---

## 326. Evgeny Quotes (Authoritative Reference)

### 326.1 Complete Collection (Sessions 7-17)

These quotes from Evgeny Poberezkin are **authoritative** and must be read before asking any new questions.

| # | Date | Quote | Context |
|---|------|-------|---------|
| 1 | 28.01.2026 | "To your question, most likely A" (peer_ephemeral + our rcv_dh_private) | Reply Queue E2E Key |
| 2 | 28.01.2026 | "you have to combine your private DH key (paired with public DH key in the invitation) with sender's public DH key sent in confirmation header - this is outside of AgentConnInfoReply but in the same message." | Where is the Key |
| 3 | 28.01.2026 | "The key insight you may be missing is that there are TWO separate crypto_box decryption layers before you reach the AgentMsgEnvelope, and they use different keys and different nonces." | Two Crypto Layers |
| 4 | 28.01.2026 | "it does seem like you're indeed missing server to client encryption layer" | Missing Server Encryption |
| 5 | 28.01.2026 | "I think the key would be in PHConfirmation, no?" | PHConfirmation |
| 6 | 26.01.2026 | "A_MESSAGE is a bit too broad error that means 'remote agent is saying something I cannot decrypt or parse', it lacks a particular context though" | A_MESSAGE Error |
| 7 | 26.01.2026 | "claude is surprisingly good at analysing our codebase" / "Opus 4.5 specifically, prior models didn't cope well" | Claude Recommendation |
| 8 | 26.01.2026 | "given that your implementation is in C I'd make an automatic test that tests it against haskell implementation" | Haskell-C Test |
| 9 | 26.01.2026 | "what you did is impressive" / "it seems like you're the first third-party SMP implementation" | Impressed |

---

## 327. Bug #18 Root Cause Analysis

### 327.1 The Discovery: Wrong Wire-Format Assumption

Our code was parsing the ClientMsgEnvelope with completely wrong offset assumptions.

#### What Our Code Assumed (WRONG):

```
[0-7]   = Message Header (Timestamps)
[8-9]   = "T " (PubHeader Tag + Space)
[10-11] = Version
[12]    = maybe_corrId        ← DOES NOT EXIST IN ENVELOPE!
[13]    = maybe_e2e           ← WRONG OFFSET!
[14+]   = SPKI                ← WRONG!
[58+]   = cmNonce             ← WRONG!
[82+]   = cmEncBody           ← WRONG!
```

#### What Haskell Actually Serializes (CORRECT - Claude Code Analysis #1):

```
ClientMsgEnvelope = (PubHeader, cmNonce, Tail cmEncBody)
PubHeader = (phVersion, Maybe phE2ePubDhKey)

WITH e2ePubKey (first message):
[0-1]   = phVersion (00 04)
[2]     = '1' (0x31) = Just
[3-46]  = X25519 SPKI (44 bytes)
[47-70] = cmNonce (24 bytes, raw)
[71+]   = cmEncBody (rest)

WITHOUT e2ePubKey (subsequent messages):
[0-1]   = phVersion (00 04)
[2]     = '0' (0x30) = Nothing
[3-26]  = cmNonce (24 bytes, raw)
[27+]   = cmEncBody (rest)
```

### 327.2 Critical Insight: corrId Does NOT Exist in ClientMsgEnvelope

**corrId is SMP Transport Layer, parsed BEFORE the envelope!**

The correlation identifier is part of the SMP protocol framing, not part of the ClientMsgEnvelope structure. Our code incorrectly assumed corrId was inside the envelope, shifting all subsequent offsets.

### 327.3 The Actual Root Cause: envelope_len Calculation

```
plain_len = 16106 (total decrypted bytes from Layer 1)
raw_len_prefix = 16002 (actual ClientMsgEnvelope length from 2-byte prefix)
Difference: 16106 - 16002 - 2(prefix) = 102 bytes

102 bytes = 0x23 = SMP Block-Padding!
```

**Bug:** `envelope_len = plain_len - 2` (WRONG! Includes 102 bytes padding)
**Fix:** `envelope_len = raw_len_prefix` (CORRECT! Uses actual length from prefix)

### 327.4 Why This Caused MAC Mismatch

With 102 extra bytes included in the envelope data:
- Ciphertext boundaries were wrong
- Nonce extraction was at wrong offset
- MAC verification used wrong data range
- Result: completely different MAC → always mismatch

### 327.5 Contact Queue vs Reply Queue: Why It Worked Before

**Contact Queue:** Parser correctly used `prefix_len` from the length prefix  
**Reply Queue:** Parser incorrectly used `plain_len - 2`, including SMP padding

This explains why Contact Queue E2E worked perfectly while Reply Queue E2E always failed!

---

## 328. Claude Code Analyses

### 328.1 Claude Code Analysis #1: ClientMsgEnvelope Wire-Format

**Question:** What is the exact wire-format of ClientMsgEnvelope?

**Key Findings:**
1. corrId does NOT exist in ClientMsgEnvelope — it's SMP Transport Layer
2. Our entire offset scheme was WRONG
3. No comma separators — fields concatenated directly (`smpEncode a <> smpEncode b`)
4. Structure: `(PubHeader, cmNonce, Tail cmEncBody)`
5. PubHeader: `(phVersion, Maybe phE2ePubDhKey)`

### 328.2 Claude Code Analysis #2: Tuple-Encoding + Wrapper Chain

**Question:** Are there comma separators in Tuple encoding? What's the wrapper structure?

**Key Findings:**
1. **CONFIRMED:** No comma separators! `smpEncode a <> smpEncode b`
2. **CONFIRMED:** Wrapper = `ClientRcvMsgBody {msgTs, msgFlags, msgBody}`
3. Complete chain documented:

```
Layer 1: EncRcvMsgBody (encrypted with rcvDhSecret)
    ↓ decrypt
Layer 2: ClientRcvMsgBody {msgTs :: SysTime, msgFlags :: Word8, msgBody :: Tail ByteString}
    ↓ extract msgBody
Layer 3: ClientMsgEnvelope (PubHeader, cmNonce, Tail cmEncBody)
    ↓ decrypt cmEncBody with e2eDhSecret + cmNonce
Layer 4: ClientMessage / AgentMsgEnvelope
```

---

## 329. Key Consistency Test Results

### 329.1 Debug Test Output

The 3-point logging test from Session 17 revealed:

```
Queue Creation:      e2e_private = XXXX (consistent)
Invitation Encode:   e2e_private = XXXX (consistent)  
Reply Queue Decrypt: e2e_private = XXXX (consistent)
```

**Result: Keys are CONSISTENT across the entire session!**

The key mismatch observed in Session 17 was from different test runs, not from an overwrite bug.

### 329.2 Excluded Theories

| Theory | Status | Evidence |
|--------|--------|----------|
| our_queue.e2e_private gets overwritten | WRONG | 3-point logging: identical |
| Wrong key in Invitation | WRONG | Consistency test: all 3 match |
| corrId SPKI = E2E Key | WRONG | Claude Code: corrId is Transport Layer |
| Contact Queue has E2E Layer 2 | WRONG | Code analysis: only Layer 1! |
| corrId exists in Envelope | WRONG | Claude Code #1: corrId is SMP Transport |
| Byte offsets were correct | **WRONG!** | Session 18: offsets completely wrong |

---

## 330. The One-Line Fix

### 330.1 Before (WRONG)

```c
// main.c - Reply Queue E2E decrypt
size_t envelope_len = plain_len - rq_prefix_len;  // Includes 102 bytes SMP padding!
```

### 330.2 After (CORRECT)

```c
// main.c - Reply Queue E2E decrypt  
size_t envelope_len = raw_len_prefix;  // Use actual length from 2-byte prefix
```

### 330.3 Result

```
Method 0 (decrypt_client_msg): SUCCESS!
Decrypted: 15904 bytes
Content: AgentConfirmation + EncRatchetMessage
```

---

## 331. Decrypted Plaintext Analysis

### 331.1 First Bytes of Decrypted Content

```
[0]     = 0x3a ':' → PrivHeader Type (NEW TYPE — must be identified!)
[1]     = 0xae     → Length byte?
[2-14]  = Ed25519 SPKI (4b 2c 30 2a 30 05 06 03 2b 65 70 03 21 00)
[...]   = 00 07 43 → Agent Version 7, 'C' = AgentConfirmation
[...]   = 30 7b 00 02 → '0' (Nothing) + 0x7b = EncRatchetMessage Start
```

### 331.2 Identified Components

| Offset | Content | Notes |
|--------|---------|-------|
| [0] | PrivHeader ':' (0x3a) | New type — not PHConfirmation='K', not PHEmpty='_' |
| [2-14] | Ed25519 SPKI | OID 1.3.101.112 |
| [...] | Agent Version 7 | 0x00 0x07 |
| [...] | Tag 'C' (0x43) | AgentConfirmation |
| [...] | EncRatchetMessage | 0x7b = 123 bytes? Double Ratchet payload |

### 331.3 Open Questions for Session 19

1. What is PrivHeader ':' (0x3a)? — Not in known list (PHConfirmation='K', PHEmpty='_')
2. AgentConfirmation full parsing → extract e2eEncryption + encConnInfo
3. EncRatchetMessage decryption with Double Ratchet

---

## 332. Contact Queue vs Reply Queue Architecture (Clarified)

### 332.1 Contact Queue (ONLY Layer 1)

```
Incoming MSG on Contact Queue:
  1. SMP Transport decrypt (Server→Recipient)
  2. Direct parse_agent_message() — NO E2E Layer!
  
decrypt_smp_message() → parse_agent_message()
```

**This is why "Contact Queue E2E works" was a FALSE assumption!**
Contact Queue never had a separate E2E layer — only server-level decryption.

### 332.2 Reply Queue (TWO Layers)

```
Incoming MSG on Reply Queue:
  1. SMP Transport decrypt (Server→Recipient) → Layer 1
  2. ClientMsgEnvelope parse + E2E decrypt → Layer 2
  
decrypt_smp_message() → parse ClientMsgEnvelope → E2E decrypt → parse_agent_message()
```

### 332.3 Implications

The "E2E Layer 2 works on Contact Queue" claim from earlier sessions was based on a misunderstanding. Contact Queue only has Layer 1 (server decryption). The E2E Layer 2 only applies to Reply Queue messages, which is where Bug #18 manifested.

---

## 333. SMP Block-Padding Discovery

### 333.1 The 102 Bytes

```
plain_len:        16106 (total decrypted from Layer 1)
raw_len_prefix:   16002 (actual ClientMsgEnvelope length)
prefix_bytes:     2     (length prefix itself)
padding:          102   (16106 - 16002 - 2)
padding_value:    0x23  (SMP block-padding character)
```

### 333.2 Why Padding Exists

SMP protocol pads messages to fixed block sizes for traffic analysis resistance. The padding is added AFTER the ClientMsgEnvelope but BEFORE Layer 1 encryption. The 2-byte length prefix tells the receiver exactly how many bytes are actual content.

### 333.3 Fix Pattern

```c
// ALWAYS use length prefix to determine content boundaries!
// NEVER use decrypted buffer size minus header size!

// WRONG:
envelope_len = plain_len - header_len;  // Includes padding!

// CORRECT:
envelope_len = raw_len_prefix;  // Exact content length from prefix
```

---

## 334. Complete Decryption Chain (Verified)

### 334.1 Layer-by-Layer

```
Layer 0: TLS 1.3 (mbedTLS)
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))
  ↓ Output: [2-byte len prefix][ClientMsgEnvelope][padding 0x23...]
  ↓ CRITICAL: Use len prefix, NOT buffer size!
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)
  ↓ Input: ClientMsgEnvelope = [PubHeader][cmNonce][cmEncBody]
  ↓ PubHeader = [version 2B][maybe 1B][opt. SPKI 44B]
  ↓
Layer 3: AgentMsgEnvelope / ClientMessage
  ↓ Contains: PrivHeader + AgentMessage
  ↓
Layer 4: Double Ratchet (EncRatchetMessage)
  ↓ Inside AgentConfirmation
  ↓
Layer 5: Application Data (ConnInfo, etc.)
```

### 334.2 Status Per Layer

| Layer | Component | Status |
|-------|-----------|--------|
| 0 | TLS 1.3 | ✅ Working since Session 1 |
| 1 | SMP Transport (Server) | ✅ Working since Session 9 |
| 2 | E2E (Sender→Recipient) | ✅ **FIXED in Session 18!** |
| 3 | AgentMsgEnvelope parsing | ⏳ Next step |
| 4 | Double Ratchet | ⏳ After Layer 3 |
| 5 | Application Data | ⏳ After Layer 4 |

---

## 335. Session 18 Mistakes

### 335.1 Mistakes from Previous Sessions (Identified in S18)

1. **Assumed Contact Queue had E2E Layer 2** — It doesn't! Only Layer 1.
2. **Used plain_len for envelope boundaries** — Should use length prefix.
3. **Assumed corrId is inside ClientMsgEnvelope** — It's SMP Transport Layer.
4. **Spent weeks debugging crypto when the problem was offset calculation** — One line fix.

### 335.2 What Went Right in Session 18

1. Systematic Claude Code analysis of wire-format
2. Evidence-based debugging (ESP log analysis)
3. Comparing Contact Queue (working) vs Reply Queue (broken) parsers
4. Single root cause identified and verified before implementing fix

---

## 336. Bug #18 Final Summary

### 336.1 Timeline

| Session | Date | Discovery | Impact |
|---------|------|-----------|--------|
| 12 | Jan 30 | Two separate X25519 keypairs | Foundation |
| 13 | Jan 30 | HSalsa20 difference, MAC position | Understanding |
| 14 | Jan 31-Feb 1 | DH SECRET VERIFIED with Python! | Confidence |
| 15 | Feb 1 | maybe_e2e=Nothing, missing key theory | Wrong theory |
| 16 | Feb 1-3 | Custom XSalsa20, theory disproven | Major discovery |
| 17 | Feb 4 | Key consistency investigation | Narrowing |
| **18** | **Feb 5** | **Wire-format wrong! One-line fix!** | **SOLVED!** |

### 336.2 Total Debug Effort

- **7 sessions** spanning Feb 5 - Jan 30 (reverse chronological)
- **~30 sub-issues** investigated (#18a through #18z, #18aa, #18ab)
- **Countless crypto approaches** tested
- **Multiple theories** disproven
- **Root cause:** 102 bytes SMP padding included in envelope_len
- **Fix:** ONE LINE of code

### 336.3 The Lesson

> "Bug #18 GELÖST. Eine Zeile. 102 Bytes. Wochen des Debuggens."
> "Evgenys Worte sind die Wahrheit. Der Haskell Code ist der Beweis."
> "Keine Theorien. Nur Evidenz. So löst man Bugs."

---

## 337. Bug #18 Sub-Issues Final Status

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | Parsing fix (correct offsets) | DONE |
| #18d | HSalsa20 difference identified | DONE |
| #18e | MAC position difference identified | DONE |
| #18f | 5 crypto approaches tested (S13) | DONE - All fail |
| #18g | SMPConfirmation contains e2ePubKey | FOUND |
| #18h | Handoff theory DISPROVEN (S14) | DONE |
| #18i | Wrong key bug fixed (S14) | DONE |
| #18j | Wrong DH function fixed (S14) | DONE |
| #18k | DH SECRET VERIFIED with Python! (S14) | DONE |
| #18l | maybe_e2e = Nothing discovered (S15) | DONE |
| #18m | Pre-computed secret required (S15) | DONE |
| #18n | Missing App's AgentConfirmation (S15) | DISPROVEN (S16) |
| #18o | app.sndQueue.e2ePubKey identified (S15) | DISPROVEN (S16) |
| #18p | ROOT CAUSE IDENTIFIED (S15) | WRONG (S16) |
| #18q | Session 15 theory DISPROVEN (S16) | DONE |
| #18r | SimpleX custom XSalsa20 discovered (S16) | DONE |
| #18s | simplex_crypto.c implemented (S16) | DONE |
| #18t | Custom XSalsa20 verified (S16) | DONE |
| #18u | Key race condition fixed (S16) | DONE |
| #18v | Wire-format verified correct (S16) | DONE |
| #18w | Problem is Double Ratchet (S16) | IDENTIFIED |
| #18x | rcAD order analyzed (S17) | DONE - staying OUR\|\|PEER |
| #18y | Length prefix fix (S17) | DONE |
| #18z | Key consistency check (S17) | DONE - keys consistent |
| **#18aa** | **Contact Queue has NO E2E Layer 2 (S18)** | **DONE** |
| **#18ab** | **ClientMsgEnvelope wire-format analyzed (S18)** | **DONE** |
| **#18ac** | **Tuple-encoding: no comma separators (S18)** | **DONE** |
| **#18ad** | **Wrapper chain: EncRcvMsgBody→ClientMsgEnvelope (S18)** | **DONE** |
| **#18ae** | **Root cause: 102 bytes SMP padding (S18)** | **DONE** |
| **#18af** | **One-line fix: envelope_len = raw_len_prefix (S18)** | **DONE** |
| **#18ag** | **E2E Layer 2 decrypt SUCCESS: 15904 bytes (S18)** | **✅ SOLVED** |

---

## 338. Next Steps (Session 19)

### 338.1 Immediate: Parse AgentConfirmation

From the decrypted plaintext (15904 bytes):

1. Identify PrivHeader ':' (0x3a) — what type is this?
2. Parse AgentConfirmation structure:
   - e2eEncryption field
   - encConnInfo field
   - smpReplyQueues
3. Extract EncRatchetMessage

### 338.2 After Parsing

1. Implement Double Ratchet decryption for EncRatchetMessage
2. Extract ConnInfo (connection information)
3. App should show "Connected"
4. Enable bidirectional messaging

### 338.3 Long-term

1. Full message send/receive loop
2. Multi-contact support
3. UI on LilyGo T-Deck
4. Production hardening

---

## 339. Session 18 Statistics

| Metric | Value |
|--------|-------|
| Duration | ~1 day |
| Claude Code analyses | 2 |
| Code fixes | 1 (one line!) |
| Key discoveries | 6 |
| Theories disproven | 3 |
| **Bug #18 solved** | **YES! ✅** |
| **Bytes decrypted** | **15904** |

---

## 340. Session 18 Changelog

| Time | Change | Result |
|------|--------|--------|
| 2026-02-05 | Contact Queue vs Reply Queue analysis | Contact Queue has NO E2E Layer 2 |
| 2026-02-05 | Claude Code #1: ClientMsgEnvelope wire-format | corrId NOT in envelope, offsets WRONG |
| 2026-02-05 | Claude Code #2: Tuple-encoding + wrapper | No commas, direct concatenation |
| 2026-02-05 | ESP Log evidence: 16106 vs 16002 | 102 bytes SMP padding identified |
| 2026-02-05 | Fix: envelope_len = raw_len_prefix | **BUG #18 SOLVED!** |
| 2026-02-05 | Method 0 decrypt SUCCESS | 15904 bytes AgentConfirmation |

---

## 341. Session 18 Summary

### What Was Achieved

- **BUG #18 SOLVED** after 7 sessions and weeks of debugging
- Root cause: 102 bytes SMP block-padding included in envelope_len
- Fix: ONE LINE — `envelope_len = raw_len_prefix`
- E2E Layer 2 decrypt: 15904 bytes AgentConfirmation
- ClientMsgEnvelope wire-format fully documented
- Contact Queue architecture clarified (no E2E Layer 2)
- Complete decryption chain verified through Layer 2

### What Was NOT Achieved (Deferred to Session 19)

- PrivHeader ':' (0x3a) not yet identified
- AgentConfirmation not yet fully parsed
- EncRatchetMessage not yet decrypted
- App still not showing "Connected"

### Key Takeaway

```
BUG #18 ROOT CAUSE:
  envelope_len = plain_len - 2     ← WRONG (includes 102B SMP padding)
  envelope_len = raw_len_prefix    ← CORRECT (exact content length)

THE LESSON:
  - ALWAYS use length prefix for content boundaries
  - NEVER assume buffer_size - header = content_size
  - SMP adds block-padding for traffic analysis resistance
  - Compare working code (Contact Queue) with broken code (Reply Queue)

"Eine Zeile. 102 Bytes. Wochen des Debuggens."
```

---

**DOCUMENT CREATED: 2026-02-05 Session 18 v32**  
**Status: BUG #18 SOLVED! E2E Layer 2 Decrypt SUCCESS!**  
**Key Achievement: 15904 bytes AgentConfirmation decrypted**  
**Next: Parse AgentConfirmation, identify PrivHeader ':', decrypt EncRatchetMessage**
