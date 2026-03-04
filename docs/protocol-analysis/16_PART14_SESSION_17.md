![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 14: Session 17
# Evgeny's Previous Answers Re-Analyzed, Key Consistency Verified

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-04
**Status:** COMPLETED -- Key consistency confirmed, length prefix fix applied
**Previous:** Part 13 - Session 16
**Next:** Part 15 - Session 18
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 17 SUMMARY

```
Session 17 realized that Evgeny had already answered the Reply Queue
E2E question on January 28, 2026. The Claude Code analyses confirmed:
rcAD order is OUR||PEER (empirically better despite theory suggesting
PEER||OUR), Reply Queue uses two crypto_box layers with different
keys/nonces, and both keypairs (server DH + E2E) are generated at
queue creation and never changed. A Reply Queue length prefix
difference was discovered and fixed (Reply Queue has 2-byte length
prefix before ClientMsgEnvelope, Contact Queue does not). Key
consistency was verified across the entire session via 3-point
logging. MAC mismatch persisted, indicating wrong key rather than
wrong offset.

 Evgeny's Jan 28 answers re-analyzed
 rcAD order: staying OUR||PEER (tested, better empirically)
 Reply Queue length prefix fix applied
 Key consistency verified (3-point logging: all identical)
```

---

## Evgeny's Previous Answers (January 28, 2026)

Session 17 discovered that the question sent to Evgeny had already been answered weeks earlier. Key quotes:

"To your question, most likely A" (Option A = peer_ephemeral + our rcv_dh_private)

"you have to combine your private DH key (paired with public DH key in the invitation) with sender's public DH key sent in confirmation header -- this is outside of AgentConnInfoReply but in the same message."

"The key insight you may be missing is that there are TWO separate crypto_box decryption layers before you reach the AgentMsgEnvelope, and they use different keys and different nonces."

Rule established: always search past Evgeny conversations completely before asking new questions.

---

## Claude Code Analysis Results

### rcAD Order

```
rcAD = sk1 || rk1 = JOINER_KEY || INITIATOR_KEY
     = APP_KEY    || ESP32_KEY
     = PEER       || OUR
```

Theory suggests PEER||OUR, but testing showed OUR||PEER works better empirically. Decision: staying with OUR||PEER.

### Reply Queue Two-Layer Architecture

| Layer | DH Secret | Nonce | Purpose |
|-------|-----------|-------|---------|
| Layer 1 | rcvDhSecret | cbNonce(msgId) | Server-to-Recipient |
| Layer 2 | e2eDhSecret | cmNonce (random, from envelope) | Sender-to-Recipient E2E |

cmNonce is not calculated; it is transmitted directly in the ClientMsgEnvelope.

### Two Keypairs at Queue Creation

| Keypair | Purpose | Field | Layer |
|---------|---------|-------|-------|
| A (server) | SMP Transport | rcvDhSecret | Layer 1 |
| B (E2E) | Per-queue encryption | e2ePrivKey | Layer 2 |

Both are generated at queue creation and never changed during the connection lifetime.

---

## Reply Queue Length Prefix Fix

Contact Queue messages have no length prefix before the ClientMsgEnvelope content. Reply Queue messages have a 2-byte length prefix (e.g. 0x3E82 = 16002). This difference was discovered and the offset was adjusted (+2 bytes for Reply Queue).

---

## Key Consistency Verification

3-point logging was added to track `our_queue.e2e_private` at queue creation, invitation encode, and Reply Queue decrypt time. All three logged identical values, confirming the key is consistent across the entire session. The key mismatch observed in earlier Session 17 logs was from different test runs, not from an overwrite bug.

### MAC Mismatch Data

```
Expected MAC: ed319f1858168cfc18ceeb255d414f77
Computed MAC: f48d300da052597801d9c3a721b9e86a
```

Completely different MACs indicate a wrong key or wrong data boundaries, not a minor offset error.

---

## Status After Session 17

Working: TLS, SMP handshake, queue creation, Contact Queue decrypt (Layer 1 only), X3DH, Double Ratchet header encrypt, server accepts messages, Reply Queue Layer 1 decrypt, length prefix handling.

Not working: Reply Queue Layer 2 (E2E) decrypt. MAC mismatch persists.

---

*Part 14 - Session 17: Evgeny's Answers & Key Consistency*
*SimpleGo Protocol Analysis*
*Original date: February 4, 2026*
*Rewritten: March 4, 2026 (v2)*
*Re-analyzed Evgeny, length prefix fix, key consistency confirmed*
