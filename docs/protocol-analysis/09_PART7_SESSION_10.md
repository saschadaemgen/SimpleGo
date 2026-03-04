![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 7: Session 10
# Reply Queue Three-Layer Analysis & Per-Queue DH Investigation

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-28
**Status:** COMPLETED -- Server-level working, per-queue DH blocked
**Previous:** Part 6 - Session 9
**Next:** Part 8 - Session 11
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 10 SUMMARY

```
Session 10 analyzed the Reply Queue encryption layers and attempted
per-queue DH decryption. Server-level decrypt confirmed working
(16106 bytes). Five key combinations were tested for the per-queue
DH layer, all failing. A critical discovery was made: Contact Queue
uses srv_dh_public for per-queue DH, not the ephemeral key from the
message. A new hypothesis emerged that Reply Queue messages might
have no second crypto_box layer, with the X25519 SPKI being metadata
rather than a decryption key. Double Ratchet decrypt also failed with
an unrealistic emHeader length (189), suggesting the data offset was
wrong. A technical question for Evgeny was formulated.

 Server-level decrypt confirmed working
 5 key combinations tested for per-queue DH (all failed)
 Contact Queue DH key source identified (srv_dh_public)
 Hypothesis: no per-queue layer on Reply Queue
```

---

## Hypothetical Three-Layer Structure

Based on Contact Queue analysis, the Reply Queue was assumed to have:

```
Layer 1: Server-Level XSalsa20-Poly1305
         Key: shared_secret (srv_dh_public + rcv_dh_private)
         Nonce: msgId (24 bytes)

Layer 2: Per-Queue DH (NaCl crypto_box)
         Key: ??? 
         Nonce: ???

Layer 3: Double Ratchet (AES-GCM)
         Key: chain key derived
         IV: from chainKdf
```

Layer 1 works. Layer 2 failed with all tested key combinations. Layer 3 is blocked by Layer 2.

---

## Contact Queue vs Reply Queue Difference

| Aspect | Contact Queue | Reply Queue |
|--------|---------------|-------------|
| Creator | Us | Us |
| Sender | SimpleX App | SimpleX App |
| Server-level decrypt | Working | Working |
| Per-Queue DH key | srv_dh_public | Unknown |

Critical discovery: Contact Queue per-queue DH uses `srv_dh_public` (the server's DH key from the IDS response), not the ephemeral key visible in the decrypted message. This was a key insight for understanding the layer architecture.

---

## Per-Queue DH Test Matrix

| Test | Key Source | Nonce Source | Result |
|------|-----------|--------------|--------|
| 1 | peer_ephemeral + rcv_dh_private | Message offset 60 | Failed |
| 2 | peer_ephemeral + rcv_dh_private | msgId | Failed |
| 3 | srv_dh_public + rcv_dh_private | msgId | Failed |
| 4 | peer_ephemeral (raw, no beforenm) | msgId | Failed |
| 5 | shared_secret (direct) | Message nonce | Failed |

Test 3 (direct on raw data without server decrypt) confirmed no SPKI was present in the raw data, validating that server-level decrypt is correct and necessary.

All sensible key combinations exhausted. This pointed to either a fundamentally different structure or the absence of a second crypto_box layer entirely.

---

## Structure After Server-Level Decrypt

Both Contact Queue and Reply Queue show identical structure:

```
[0-1]    3e 82            Length prefix: 16002
[2-5]    00 00 00 00      Padding
[6-9]    69 7a 0c 8d      Timestamp
[10-13]  54 20 00 04      Header bytes
[14-15]  31 2c            Version "1,"
[16-59]  30 2a 30 05...   X25519 SPKI (44 bytes, different key per message)
[60+]    Data continues
```

The X25519 SPKI at offset 16-59 contains a different key in each message, but its role (whether it is a decryption key or metadata) was unclear at this point.

---

## No Per-Queue Layer Hypothesis

If there is no second crypto_box layer, the X25519 SPKI at offset 16 could be metadata (sender identification or corrId), and Double Ratchet data would start directly after the SPKI. However, attempting Double Ratchet decrypt at offset 60 produced an emHeader length of 189, which is unrealistic (expected ~123-127), indicating the offset was wrong or the data was not yet at the Double Ratchet layer.

---

## Technical Question for Evgeny

A precise technical question was formulated for the SimpleX developer team, asking whether Reply Queue messages have a second per-queue crypto_box layer, which key should be used if yes, and whether Double Ratchet data starts directly after the SPKI if no.

---

*Part 7 - Session 10: Reply Queue Three-Layer Analysis*
*SimpleGo Protocol Analysis*
*Original date: January 28, 2026*
*Rewritten: March 4, 2026 (v2)*
*5 key combinations tested, per-queue DH layer investigation*
