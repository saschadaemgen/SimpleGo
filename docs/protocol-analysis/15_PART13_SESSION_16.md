![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 13: Session 16
# SimpleX Custom XSalsa20, Session 15 Theory Disproven, Double Ratchet Problem

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-01 to 2026-02-03
**Status:** COMPLETED -- Custom XSalsa20 implemented, Double Ratchet identified as problem
**Previous:** Part 12 - Session 15
**Next:** Part 14 - Session 17
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 16 SUMMARY

```
Session 16 resolved the contradictions between Sessions 14 and 15,
which analyzed different test runs. Session 15's "missing second
message" theory was disproven by Evgeny's authoritative statement:
"in the same message." A critical discovery was made: SimpleX uses a
non-standard XSalsa20 variant where HSalsa20 is applied with zeros
instead of the nonce prefix, making all previous libsodium
crypto_box/crypto_secretbox attempts fundamentally incompatible.
Custom simplex_crypto.c was implemented and verified. A key race
condition (reply_queue_e2e_peer_public overwritten by parser) was
fixed. Wire-format and AAD were verified correct. The remaining
problem was narrowed to Double Ratchet (rcAD order or X3DH params).

 Session 15 "missing message" theory disproven
 SimpleX non-standard XSalsa20 discovered and implemented
 Key race condition fixed (smp_parser.c overwrite)
 Wire-format, AAD, keys all verified correct
 Problem narrowed to Double Ratchet rcAD/X3DH
```

---

## Session 14/15 Contradiction Resolution

Sessions 14 and 15 produced contradictory claims about byte [14]/[15] interpretation, DH secret validity, and whether a second message was needed. Session 16 resolved this: the sessions analyzed different test runs (different nonces: `b2 1f a2 bc...` vs `96 ef 9b 41...`). Session 14's DH verification was correct for its test run; Session 15's observations were also correct for its run, but the "missing message" conclusion was wrong.

---

## Evgeny's Authoritative Answers (January 28, 2026)

Evgeny Poberezkin provided three decisive hints that override all session theories:

**Hint 1:** "you have to combine your private DH key (paired with public DH key in the invitation) with sender's public DH key sent in confirmation header -- this is outside of AgentConnInfoReply but in the same message."

| Evgeny says | Meaning |
|-------------|---------|
| "your private DH key" | our_queue.e2e_private |
| "sender's public DH key" | App's key in the PubHeader |
| "in the same message" | No second message needed |

**Hint 2:** "The key insight you may be missing is that there are TWO separate crypto_box decryption layers before you reach the AgentMsgEnvelope, and they use different keys and different nonces."

| Layer | Key | Nonce | Purpose |
|-------|-----|-------|---------|
| Layer 1 | rcvDhSecret (server DH) | cbNonce(msgId) | Server-to-Client |
| Layer 2 | e2eDhSecret (E2E DH) | cmNonce (random, from envelope) | Per-Queue E2E |

---

## Non-Standard XSalsa20 Discovery

### The Critical Difference

```
Standard libsodium crypto_secretbox:
  HSalsa20(dh_secret, nonce[0:16])

SimpleX xSalsa20 (Crypto.hs):
  HSalsa20(dh_secret, zeros[16])     -- ZEROS, not nonce!
  HSalsa20(subkey1, nonce[8:24])
  Salsa20(subkey2, nonce[0:8])
```

### Haskell Source (Crypto.hs)

```haskell
xSalsa20 (DhSecretX25519 shared) nonce msg = (rs, msg')
  where
    zero = B.replicate 16 (toEnum 0)           -- 16 ZERO bytes!
    (iv0, iv1) = B.splitAt 8 nonce             -- split at byte 8
    state0 = XSalsa.initialize 20 shared (zero `B.append` iv0)
    state1 = XSalsa.derive state0 iv1
```

Python verification confirmed the subkeys are completely different between standard and SimpleX XSalsa20. All previous crypto_box and crypto_secretbox attempts were fundamentally incompatible.

### Custom Implementation (simplex_crypto.c)

```c
int simplex_secretbox_open(uint8_t *plain, const uint8_t *cipher, size_t len,
                           const uint8_t *nonce, const uint8_t *dh_secret) {
    uint8_t subkey1[32], subkey2[32];
    uint8_t zeros[16] = {0};

    // Step 1: HSalsa20(dh_secret, zeros[16])
    crypto_core_hsalsa20(subkey1, zeros, dh_secret, NULL);

    // Step 2: HSalsa20(subkey1, nonce[8:24])
    crypto_core_hsalsa20(subkey2, &nonce[8], subkey1, NULL);

    // Step 3: Salsa20 decrypt + Poly1305 verify
    // ...
}
```

Verification: round-trip encrypt/decrypt of "Hello from SimpleX!" succeeded with subkeys matching Python byte-for-byte.

---

## Key Race Condition Fix

Claude Code analysis of the SimpleGo codebase found that `reply_queue_e2e_peer_public` was written from two places: main.c (Contact Queue PubHeader SPKI extraction, correct) and smp_parser.c (AgentConnInfoReply parser, overwrites with wrong value). The second write won, corrupting the key. Fix: removed the overwrite in smp_parser.c.

---

## Double Ratchet Identified as Problem

### Self-Decrypt Failure is By Design

Double Ratchet uses asymmetric header keys: the sender encrypts with HKs, the receiver decrypts with HKr. These are different keys derived from the X3DH output. Self-decrypt failure is expected behavior, not a bug.

| Perspective | HKs (Send) | HKr (Receive) |
|-------------|------------|---------------|
| Us (Alice) | hk = kdf[0:32] | nhk = kdf[32:64] |
| Peer (Bob) | nhk = kdf[32:64] | hk = kdf[0:32] |

### The Causal Chain

The app cannot decrypt our AgentConfirmation (which contains encConnInfo with our e2e_public). Evidence: Android shows "Request to connect" (confirmation not understood), Desktop shows "Connecting" (received but still processing).

### X3DH Parameters (from Claude Code Analysis)

```
DH1: dh'(bob_identity, alice_ephemeral)
DH2: dh'(bob_ephemeral, alice_identity)
DH3: dh'(bob_ephemeral, alice_ephemeral)

HKDF:
  Salt  = 64 x 0x00
  IKM   = DH1 || DH2 || DH3
  Info  = "SimpleXX3DH"
  Output = 96 bytes: [0-31] hk, [32-63] nhk, [64-95] root_key
```

### E2E Key Exchange (Separate from XSalsa20 Issue)

e2eDhSecret is simple X25519 DH with no KDF: `e2eDhSecret = X25519.dh(peer_pub, own_priv)`. Used directly as NaCl crypto_box key. The `maybe_e2e` flag indicates whether the DH key is transmitted ('1' + key bytes for first message) or pre-computed ('0' for subsequent messages).

---

## Verified Components

| Component | Status | Evidence |
|-----------|--------|----------|
| Wire-format parsing | Correct | Hex-dump verified |
| Payload AAD (235 bytes) | Correct | Claude Code analysis |
| Header AAD | Correct | Header decrypt works |
| emHeader encoding | Correct | Version, IV, Tag, Body |
| Key consistency | Correct | Creation = Sending = Decrypt |
| Custom XSalsa20 | Verified | Round-trip success |

### Remaining Suspects

| Suspect | Probability |
|---------|-------------|
| rcAD order (our/peer vs peer/our) | High |
| X3DH DH order | Medium |
| HKDF salt/info params | Medium |
| Root key derivation output offsets | Medium |

---

*Part 13 - Session 16: Custom XSalsa20 & Double Ratchet Problem*
*SimpleGo Protocol Analysis*
*Original dates: February 1-3, 2026*
*Rewritten: March 4, 2026 (v2)*
*Non-standard XSalsa20, key race condition, Double Ratchet narrowed*
