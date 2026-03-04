![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 22: Session 25
# Bidirectional Encrypted Chat and Delivery Receipts

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-13/14
**Status:** COMPLETED -- Milestones 3, 4, 5 achieved
**Previous:** Part 21 - Session 24
**Next:** Part 23 - Session 26
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 25 SUMMARY

```
Session 25 achieved three milestones in one session: first app
message decrypted on ESP32, bidirectional encrypted chat, and
delivery receipts. Major refactoring reduced main.c from 2440 to
611 lines (75% reduction) with four new modules. The bidirectional
bug was solved by brute-force nonce offset scanning: regular Q_B
messages use nonce at offset 13, not 14. A ratchet state persistence
bug (pointer vs copy) was fixed to enable SameRatchet decrypt for
message #1 and beyond. Delivery receipts required count=Word8 and
rcptInfo=Word16 (not Word16 and Word32 as initially implemented).

 Refactoring: main.c 2440 -> 611 lines, 4 new modules
 Milestone 3: First app message decrypted on ESP32
 Milestone 4: Bidirectional encrypted chat (Feb 14, 00:25)
 Milestone 5: Delivery receipts (double checkmarks)
 8 bugs fixed (5 critical, 3 high)
```

---

## Refactoring

main.c reduced from 2440 to 611 lines. Four new modules extracted:

| Module | Purpose |
|--------|---------|
| smp_ack.c/h | ACK handling (consolidated from 3x duplicate) |
| smp_wifi.c/h | WiFi initialization |
| smp_e2e.c/h | E2E envelope decryption (consolidated from 2x duplicate) |
| smp_agent.c/h | Agent protocol layer (PrivHeader, tag dispatch, ratchet orchestration) |

Runtime fix during refactoring: `malloc(256)` changed to `malloc(eh_body_len + 16)` to prevent heap overflow with PQ headers (2346 bytes).

---

## Bidirectional Bug: Nonce Offset

The app sends on Q_B (Reply Queue), not Q_A. X-ray diagnostics showed E2E decrypt always failed because the parser read the nonce at the wrong offset.

Brute-force scan revealed the correct offset:

```
BRUTE-FORCE NONCE OFFSET SCAN
Decrypt OK at nonce_offset=13! pt_len=16000
```

Regular Q_B messages have format `[12B header][nonce@13][ciphertext]`. Byte[12] was a random nonce byte that happened to look like ASCII '0', which the parser misidentified as a corrId tag.

---

## Ratchet State Persistence Bug

Only message #0 (AdvanceRatchet) decrypted. Messages #1+ (SameRatchet) failed because ratchet state was copied by value instead of by pointer:

```c
// WRONG: chain_key_recv updated in local copy, never written back
ratchet_state_t rs = *ratchet_get_state();

// CORRECT: updates persist in global state
ratchet_state_t *rs = ratchet_get_state();
```

Additional fix: Chain KDF skip calculation changed from relative to absolute (`skip_from = msg_num_recv`).

---

## Delivery Receipts

Wire format for A_RCVD ('V'):

```
'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]

AMessageReceipt:
  agentMsgId (8B Int64 BE)
  msgHash (1B len + 32B SHA256)
  rcptInfo (Word16 Large prefix)
```

Two encoding errors found by byte-for-byte comparison with the app's own receipt (90 bytes vs 87 bytes): count was Word16 instead of Word8 (+1 byte), rcptInfo was Word32 instead of Word16 (+2 bytes). App parsed count=0x00 as zero receipts.

Receipts only sent if connAgentVersion >= v4.

---

## Complete Bug List

| Bug | Severity | Root Cause | Fix |
|-----|----------|-----------|-----|
| Heap overflow PQ headers | Critical | malloc(256) too small | malloc(eh_body_len + 16) |
| txCount hardcoded | Critical | Dropped MSG after re-SUB | Read as variable counter |
| Nonce offset wrong | Critical | Offset 14 instead of 13 | Brute-force confirmed 13 |
| Ratchet state copy | Critical | Pointer vs value copy | Use pointer to global state |
| Chain KDF skip relative | Critical | Wrong msg_num calculation | Absolute from msg_num_recv |
| Receipt count=Word16 | High | App reads Word8, gets 0 | Changed to Word8 |
| Receipt rcptInfo=Word32 | High | 3 bytes too many | Changed to Word16 |
| NULL contact Reply Queue | High | contact=NULL not checked | NULL guard added |

---

## Milestone Timeline

| # | Milestone | Date | Session |
|---|-----------|------|---------|
| 1 | CONNECTED | 2026-02-08 | 23 |
| 2 | First A_MSG | 2026-02-11 | 24 |
| 3 | App message decrypted on ESP32 | 2026-02-14 | 25 |
| 4 | Bidirectional encrypted chat | 2026-02-14 00:25 | 25 |
| 5 | Delivery receipts | 2026-02-14 ~10:00 | 25 |

---

*Part 22 - Session 25: Bidirectional Chat + Receipts*
*SimpleGo Protocol Analysis*
*Original dates: February 13-14, 2026*
*Rewritten: March 4, 2026 (v2)*
*Three milestones in one Valentine's session*
