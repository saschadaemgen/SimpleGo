![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 15: Session 18
# Bug #18 Solved: SMP Block-Padding in envelope_len, E2E Layer 2 Decrypt Success

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-05
**Status:** COMPLETED -- Bug #18 solved, 15904 bytes decrypted
**Previous:** Part 14 - Session 17
**Next:** Session 19 (AgentConfirmation parsing)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 18 SUMMARY

```
Session 18 solved Bug #18 after seven sessions of debugging (Sessions
12-18, spanning January 30 to February 5). The root cause was a
single line: envelope_len was calculated from plain_len - 2 instead
of using the actual raw_len_prefix value from the 2-byte length
prefix. This included 102 bytes of SMP block-padding (0x23) in the
envelope data, corrupting all subsequent offsets and producing MAC
mismatches. Claude Code analyses revealed that corrId does not exist
in ClientMsgEnvelope (it is SMP Transport Layer), and Contact Queue
has no E2E Layer 2 (only server-level decryption). The one-line fix
produced 15904 bytes of decrypted AgentConfirmation content.

 Root cause: 102 bytes SMP padding in envelope_len
 Fix: envelope_len = raw_len_prefix (one line)
 E2E Layer 2 decrypt success: 15904 bytes
 corrId is SMP Transport, not in ClientMsgEnvelope
 Contact Queue has no E2E Layer 2 (earlier assumption wrong)
```

---

## Root Cause: SMP Block-Padding in envelope_len

### The Numbers

```
plain_len:      16106 (total decrypted bytes from Layer 1)
raw_len_prefix: 16002 (actual ClientMsgEnvelope length from 2-byte prefix)
prefix_bytes:   2     (the length prefix itself)
padding:        102   (16106 - 16002 - 2 = SMP block-padding, value 0x23)
```

### The Bug

```c
// WRONG: includes 102 bytes of SMP padding
size_t envelope_len = plain_len - rq_prefix_len;

// CORRECT: uses actual content length from prefix
size_t envelope_len = raw_len_prefix;
```

SMP protocol pads messages to fixed block sizes for traffic analysis resistance. The padding is added after the ClientMsgEnvelope content but before Layer 1 encryption. The 2-byte length prefix tells the receiver exactly how many bytes are actual content. Using `plain_len - header` instead of the length prefix value included the padding in the envelope data, shifting all field boundaries and producing MAC mismatches in every decrypt attempt.

### Why Contact Queue Worked

Contact Queue parser correctly used the length prefix value for content boundaries. Reply Queue parser incorrectly used buffer size minus header size. This is why Contact Queue E2E appeared to work while Reply Queue E2E always failed.

---

## ClientMsgEnvelope Wire-Format (Claude Code Analysis)

corrId does not exist in ClientMsgEnvelope. It is part of the SMP Transport Layer, parsed before the envelope. Fields are concatenated directly (no comma separators): `smpEncode a <> smpEncode b`.

### With e2ePubKey (first message on a queue)

```
[0-1]   phVersion (Word16 BE, e.g. 00 04)
[2]     '1' (0x31) = Just
[3-46]  X25519 SPKI (44 bytes)
[47-70] cmNonce (24 bytes, raw)
[71+]   cmEncBody (Tail, rest of data)
```

### Without e2ePubKey (subsequent messages)

```
[0-1]   phVersion (Word16 BE, e.g. 00 04)
[2]     '0' (0x30) = Nothing
[3-26]  cmNonce (24 bytes, raw)
[27+]   cmEncBody (Tail, rest of data)
```

### Complete Wrapper Chain

```
Layer 1 decrypt output: [2-byte len prefix][ClientMsgEnvelope][padding 0x23...]
                        Use len prefix, NOT buffer size!

ClientRcvMsgBody: {msgTs :: SysTime, msgFlags :: Word8, msgBody :: Tail ByteString}

ClientMsgEnvelope (inside msgBody): (PubHeader, cmNonce, Tail cmEncBody)

PubHeader: (phVersion, Maybe phE2ePubDhKey)
```

---

## Contact Queue vs Reply Queue Architecture (Corrected)

### Contact Queue: Only Layer 1

```
Incoming MSG on Contact Queue:
  1. SMP Transport decrypt (Server-to-Recipient)
  2. Direct parse_agent_message() -- NO E2E Layer
```

The earlier claim that "Contact Queue E2E works" was a misunderstanding. Contact Queue only has server-level decryption (Layer 1). There is no separate E2E layer.

### Reply Queue: Two Layers

```
Incoming MSG on Reply Queue:
  1. SMP Transport decrypt (Server-to-Recipient) -- Layer 1
  2. ClientMsgEnvelope parse + E2E decrypt       -- Layer 2
  3. parse_agent_message()
```

---

## Decrypted Content (15904 bytes)

```
[0]     0x3a ':' -- PrivHeader type (not PHConfirmation='K', not PHEmpty='_')
[2-14]  Ed25519 SPKI (OID 1.3.101.112)
[...]   00 07 43 -- Agent Version 7, 'C' = AgentConfirmation
[...]   EncRatchetMessage (Double Ratchet payload)
```

PrivHeader ':' (0x3a) is a new type not previously encountered. The content contains an AgentConfirmation with an EncRatchetMessage inside, which requires Double Ratchet decryption in the next session.

---

## Complete Decryption Chain Status

| Layer | Component | Status |
|-------|-----------|--------|
| 0 | TLS 1.3 | Working (Session 1) |
| 1 | SMP Transport (rcvDhSecret + cbNonce) | Working (Session 9) |
| 2 | E2E (e2eDhSecret + cmNonce) | Working (Session 18) |
| 3 | AgentMsgEnvelope parsing | Next step |
| 4 | Double Ratchet (EncRatchetMessage) | After Layer 3 |
| 5 | Application data (ConnInfo) | After Layer 4 |

---

## Bug #18 Timeline

| Session | Date | Discovery |
|---------|------|-----------|
| 12 | Jan 30 | Two separate X25519 keypairs |
| 13 | Jan 30 | HSalsa20 difference, MAC position |
| 14 | Jan 31-Feb 1 | DH secret verified with Python |
| 15 | Feb 1 | maybe_e2e=Nothing, missing key theory (wrong) |
| 16 | Feb 1-3 | Custom XSalsa20, theory disproven |
| 17 | Feb 4 | Key consistency verified, length prefix fix |
| 18 | Feb 5 | Root cause: 102 bytes SMP padding. One-line fix. |

Seven sessions, approximately 30 sub-issues investigated, multiple theories disproven. Root cause: one line of code, 102 bytes of padding included in envelope length.

---

## Evgeny Quotes Reference (Sessions 7-17)

| Date | Quote | Context |
|------|-------|---------|
| Jan 28 | "most likely A" (peer_ephemeral + our rcv_dh_private) | Reply Queue key |
| Jan 28 | "sender's public DH key sent in confirmation header -- outside of AgentConnInfoReply but in the same message" | Key location |
| Jan 28 | "TWO separate crypto_box decryption layers...different keys and different nonces" | Two layers |
| Jan 28 | "it does seem like you're indeed missing server to client encryption layer" | Missing layer |
| Jan 28 | "I think the key would be in PHConfirmation, no?" | Key hint |
| Jan 26 | "A_MESSAGE...lacks a particular context though" | Error context |
| Jan 26 | "claude is surprisingly good at analysing our codebase" / "Opus 4.5 specifically" | Tool recommendation |
| Jan 26 | "make an automatic test that tests it against haskell implementation" | Testing advice |
| Jan 26 | "what you did is impressive" / "first third-party SMP implementation" | Recognition |

---

*Part 15 - Session 18: Bug #18 Solved*
*SimpleGo Protocol Analysis*
*Original date: February 5, 2026*
*Rewritten: March 4, 2026 (v2)*
*One-line fix, 102 bytes SMP padding, E2E Layer 2 success*
