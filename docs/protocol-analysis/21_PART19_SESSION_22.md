![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 19: Session 22
# E2E Version Fix, KEM Parser, NHK Promotion, Reply Queue Flow Discovery

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-07
**Status:** COMPLETED -- App responds for first time, Reply Queue flow identified
**Previous:** Part 18 - Session 21
**Next:** Part 20 - Session 23
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 22 SUMMARY

```
Session 22 fixed five bugs (#27-#31) and made a fundamental protocol
discovery. Bug #27 (E2E version_min=2 in smp_x448.c while smp_ratchet.c
used v3) caused the app to expect v2 format for our v3 HELLO. Fixing
this made the app respond for the first time. Bug #28 added dynamic
KEM parsing for SNTRUP761 (2310 bytes vs 88-byte fixed headers). Bug
#29 made body decrypt pointer arithmetic dynamic. Bug #30 corrected
HKs/NHKs initialization and promotion (three connected issues). Bug
#31 fixed header decrypt try-order (HKr then NHKr). The fundamental
discovery: modern SimpleX (v2 + senderCanSecure) needs NO HELLO.
Instead, AgentConnInfo must be sent on the Reply Queue.

 Bug #27: E2E version_min 2->3 (app breaks silence!)
 Bug #28: Dynamic KEM parser for SNTRUP761
 Bug #29: Dynamic body decrypt offsets
 Bug #30: HKs/NHKs init + promotion chain corrected
 Bug #31: Header decrypt try-order (HKr, NHKr)
 Discovery: Modern protocol needs Reply Queue flow, not HELLO
```

---

## Bug #27: E2E Version Mismatch (Critical)

`smp_x448.c` sent `version_min = 2` in the AgentConfirmation, but `smp_ratchet.c` encrypted HELLO in v3 format. The app initialized its ratchet with v2, then could not parse v3 messages.

```c
// WRONG (smp_x448.c):
buf[p++] = 0x02;  // version_min = 2, no KEM Nothing

// CORRECT:
buf[p++] = 0x03;  // version_min = 3
// After key2:
buf[p++] = 0x30;  // KEM Nothing ('0')
```

Result: app breaks silence and responds for the first time.

---

## Bug #28: KEM Parser for SNTRUP761

App responds with v3 + SNTRUP761 KEM (public key 1158 bytes, ciphertext 1039 bytes, shared secret 32 bytes). Parser had fixed offsets for 88-byte headers and crashed on PQ responses.

Fix: dynamic KEM handling that reads the KEM tag ('0'=Nothing, '1'=Just) and skips the appropriate number of bytes before reading PN/Ns fields.

SimpleX uses SNTRUP761 (not Kyber) for post-quantum. When we send v3 + KEM Nothing ("PQ-capable but not yet active"), the app responds with KEM Proposed. Replying with KEM Nothing triggers graceful fallback to pure DH.

---

## Bug #29: Body Decrypt Pointer Arithmetic

With v3+PQ, emHeader grows from 123/124 bytes to 2346 bytes. Hardcoded `EM_HEADER_SIZE` caused 2GB malloc failures. Fix: read ehVersion to determine size dynamically, then calculate emAuthTag and emBody offsets from the actual header size.

---

## Bug #30: HKs/NHKs Init + Promotion (Three Issues)

**30a:** `next_header_key_send` was a local variable in `ratchet_init_sender()`, never saved to ratchet state.

**30b:** `ratchet_x3dh_sender()` stored nhk (NHKr) incorrectly in `header_key_recv` instead of `next_header_key_recv`.

**30c:** After DH Ratchet Step, KDF output was set directly as HKs instead of proper NHKs-to-HKs promotion.

Correct promotion sequence:

```c
// After DH Ratchet Step:
memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32);  // NHKs->HKs
memcpy(ratchet_state.next_header_key_send, kdf_output + 64, 32);  // new NHKs
```

---

## Bug #31: Header Decrypt Try-Order

The Double Ratchet requires trying keys in order: first HKr (SameRatchet), then NHKr (AdvanceRatchet). If NHKr succeeds, promote NHKr to HKr and trigger full DH ratchet step.

```c
if (try_header_decrypt(header_key_recv, ...)) {
    decrypt_mode = SAME_RATCHET;
} else if (try_header_decrypt(next_header_key_recv, ...)) {
    decrypt_mode = ADVANCE_RATCHET;
    memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
}
```

---

## Protocol Discovery: No HELLO Needed

Modern SimpleX (v2 with `senderCanSecure = True`, `QMMessaging`) does not require HELLO. Instead, AgentConnInfo must be sent on the Reply Queue. The `smpReplyQueues` are inside the ratchet-decrypted AgentConnInfoReply with tag 'D' (innermost layer). We already send this in our AgentConfirmation but were missing the Reply Queue connection flow.

Missing steps identified:

1. Parse Reply Queue Info from our sent AgentConfirmation (tag 'D')
2. Establish second TLS connection to Reply Queue server
3. SMP handshake on Reply Queue
4. Send SKEY on Reply Queue
5. Send AgentConnInfo on Reply Queue
6. App receives, triggers CON

---

## v2/v3 Format Differences (Complete Reference)

| Component | v2 | v3 |
|-----------|----|----|
| emHeader length prefix | 1 byte (Word8) | 2 bytes (Word16 BE) |
| emHeader size | 123 bytes | 124 bytes |
| ehBody length prefix | 1 byte (Word8) | 2 bytes (Word16 BE) |
| MsgHeader has KEM | No | Yes ('0' = Nothing) |
| MsgHeader contentLen | 79 | 80 |
| MsgHeader padding | 7 bytes | 6 bytes |
| v3+PQ emHeader | N/A | up to 2346 bytes |

---

*Part 19 - Session 22: E2E v3, KEM, NHK, Reply Queue*
*SimpleGo Protocol Analysis*
*Original date: February 7, 2026*
*Rewritten: March 4, 2026 (v2)*
*5 bugs fixed, app responds, Reply Queue flow discovered*
