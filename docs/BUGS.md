# SimpleGo Bug Tracker

Documentation of all bugs discovered and fixed during SimpleGo development.

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| Length Encoding | 5 | All Fixed |
| Size Calculation | 2 | All Fixed |
| KDF Output Order | 2 | All Fixed |
| Byte Order | 1 | Fixed |
| Format Encoding | 2 | All Fixed |
| Total | 12 | All Fixed |

---

## Bug #1: E2E Key Length Prefix

Category: Length Encoding
Severity: Critical
Status: Fixed

Description: E2E encryption keys used Word16 BE length prefix instead of 1-byte.

Incorrect: 0x00 0x44 (Word16)
Correct: 0x44 (1 byte)

---

## Bug #2: prevMsgHash Length Prefix

Category: Length Encoding
Severity: Critical
Status: Fixed

Description: prevMsgHash field used 1-byte length instead of Word16 BE.

Incorrect: 0x20 (1 byte)
Correct: 0x00 0x20 (Word16 BE)

---

## Bug #3: MsgHeader DH Key Length

Category: Length Encoding
Severity: Critical
Status: Fixed

Description: DH key in MsgHeader used Word16 BE instead of 1-byte.

Incorrect: 0x00 0x44 (Word16)
Correct: 0x44 (1 byte)

---

## Bug #4: ehBody Length Prefix

Category: Length Encoding
Severity: Critical
Status: Fixed

Description: ehBody field used Word16 BE instead of 1-byte.

Incorrect: 0x00 0x58 (Word16)
Correct: 0x58 (1 byte)

---

## Bug #5: emHeader Size Calculation

Category: Size Calculation
Severity: Critical
Status: Fixed

Description: EncMessageHeader calculated as 124 bytes instead of 123.

Incorrect: ehBodyLen as Word16 (2 bytes) = 124 total
Correct: ehBodyLen as 1 byte = 123 total

---

## Bug #6: Payload AAD Size

Category: Size Calculation
Severity: Critical
Status: Fixed

Description: Payload AAD calculated as 236 bytes instead of 235.

Incorrect: 112 + 124 = 236
Correct: 112 + 123 = 235

---

## Bug #7: Root KDF Output Order

Category: KDF Output Order
Severity: Critical
Status: Fixed

Description: Root KDF output bytes split in wrong order.

Incorrect order:
- [0:32] = chain_key
- [32:64] = new_root_key

Correct order:
- [0:32] = new_root_key
- [32:64] = chain_key
- [64:96] = next_header_key

---

## Bug #8: Chain KDF IV Order

Category: KDF Output Order
Severity: Critical
Status: Fixed

Description: header_iv and message_iv were swapped.

Incorrect:
- [64:80] = message_iv
- [80:96] = header_iv

Correct:
- [64:80] = header_iv (FIRST)
- [80:96] = message_iv (SECOND)

---

## Bug #9: wolfSSL X448 Byte Order

Category: Byte Order
Severity: Critical
Status: Fixed

Description: wolfSSL exports X448 keys in reversed byte order.

Solution: Reverse all bytes after key generation and DH operations.

---

## Bug #10: SMPQueueInfo Port Encoding

Category: Format Encoding
Severity: Critical
Status: Fixed

Description: Port field separated with space instead of length prefix.

Incorrect: 0x20 (space character)
Correct: Length byte followed by port string

---

## Bug #11: smpQueues List Count

Category: Format Encoding
Severity: Critical
Status: Fixed

Description: smpQueues count encoded as 1 byte instead of Word16 BE.

Incorrect: 0x01 (1 byte)
Correct: 0x00 0x01 (Word16 BE)

---

## Bug #12: queueMode Nothing Encoding

Category: Format Encoding
Severity: Medium
Status: Fixed

Description: queueMode Nothing encoded as 0 byte instead of empty.

Incorrect: Send 0x30 (character 0)
Correct: Send nothing (0 bytes)

---

## Current Investigation

### Potential Bug #13: Tail Field Length Prefix

Status: Investigating
Severity: Unknown

Hypothesis: A_MESSAGE error may be caused by adding length prefixes to Tail-encoded fields.

Fields under investigation:
- encConnInfo in AgentConfirmation
- emBody in EncRatchetMessage

Evidence:
- All crypto verified correct
- Server accepts messages
- App fails with A_MESSAGE error

---

## Bug Discovery Timeline

| Date | Bugs | Description |
|------|------|-------------|
| Jan 20, 2026 | #1-#6 | Wire format analysis |
| Jan 21, 2026 | #7-#8 | KDF verification |
| Jan 22, 2026 | #9 | wolfSSL byte order |
| Jan 23, 2026 | #10-#12 | SMPQueueInfo fixes |
| Jan 24, 2026 | #13? | Tail encoding |

---

## Lessons Learned

1. Length encoding varies by context - verify each field
2. KDF output order matters - check specification
3. Library byte order may differ - test with known vectors
4. Tail encoding has no prefix - last field needs no length
5. Verify each layer separately - isolate issues

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
