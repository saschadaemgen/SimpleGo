![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 6: Session 9
# Reply Queue Server-Level Decryption & HSalsa20 Key Derivation Fix

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-27
**Status:** COMPLETED -- Reply Queue server decrypt working, A_CRYPTO discovered
**Previous:** Part 5 - Session 8
**Next:** Part 7 - Session 10
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 9 SUMMARY

```
Session 9 fixed the Reply Queue server-level decryption by replacing
crypto_scalarmult (raw X25519 only) with crypto_box_beforenm (X25519
+ HSalsa20 key derivation). The server-side encryption uses NaCl
crypto_box, which requires the HSalsa20-derived key, not the raw DH
output. After the fix, 16106 bytes were successfully decrypted,
revealing the same ClientMsgEnvelope format as Contact Queue messages.
An A_CRYPTO error was discovered in the SimpleX App, indicating the
app cannot decrypt our AgentConfirmation (separate from the Reply
Queue issue).

 1 Bug fixed (HSalsa20 key derivation for server-level decrypt)
 Reply Queue server-level decrypt working (16106 bytes)
 Same message format as Contact Queue confirmed
 A_CRYPTO error in app discovered (header encryption issue)
```

---

## Bug Fix: HSalsa20 Key Derivation

### The Problem

Poly1305 tag verification failed on Reply Queue messages. The shared_secret was verified consistent across all checkpoints (creation, after SUB, at decrypt time), but decryption still failed.

### Root Cause

The Reply Queue code used `crypto_scalarmult` (raw X25519 DH only), while the server encrypts using NaCl `crypto_box` which internally applies HSalsa20 key derivation after the DH step.

```c
// WRONG: Only raw X25519, no HSalsa20
crypto_scalarmult(our_queue.shared_secret,
                  our_queue.rcv_dh_private,
                  our_queue.srv_dh_public);

// CORRECT: X25519 + HSalsa20 key derivation
crypto_box_beforenm(our_queue.shared_secret,
                    our_queue.srv_dh_public,    // note: parameter order changes
                    our_queue.rcv_dh_private);
```

Note the parameter order difference: `crypto_box_beforenm(out, pk, sk)` vs `crypto_scalarmult(out, sk, pk)`.

### NaCl Crypto Layers

```
crypto_scalarmult:   X25519 DH only              -> raw_secret
crypto_box_beforenm: X25519 DH + HSalsa20        -> box_key
crypto_box_easy:     X25519 DH + HSalsa20 + XSalsa20-Poly1305
```

Rule: Always use the same crypto primitive chain as the sender. If the sender uses `crypto_box`, the receiver must use `crypto_box_open`, not raw `crypto_scalarmult` output.

### Haskell Reference (Server.hs:2024, Crypto.hs)

```haskell
encrypt body = RcvMessage msgId' . EncRcvMsgBody $
    C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId') body
```

`cbEncryptMaxLenBS` calls `cryptoBox` which uses XSalsa20-Poly1305 through NaCl's `crypto_box` interface, meaning the key goes through HSalsa20 derivation.

---

## Reply Queue Decrypt: Success

```
Server-level decrypt SUCCESS! (16106 bytes)
First 32 bytes:
  3e 82 00 00 00 00 69 79 2a 97 54 20 00 04 31 2c
  30 2a 30 05 06 03 2b 65 6e 03 21 00 8d 17 1a 24
```

The decrypted bytes show the same structure as Contact Queue messages: length prefix (3e 82 = 16002), timestamp, version string "1,", followed by X25519 SPKI header (`30 2a 30 05 06 03 2b 65 6e 03 21 00`). This confirms identical message format on both queues.

### Simplified Decrypt Code

The manual XSalsa20 + Poly1305 implementation (~60 lines) was replaced with NaCl:

```c
if (crypto_box_open_easy_afternm(server_plain, &resp[p], enc_len,
                                  server_nonce, our_queue.shared_secret) == 0) {
    int plain_len = enc_len - crypto_box_MACBYTES;
    ESP_LOGI(TAG, "Reply Queue decrypt SUCCESS! (%d bytes)", plain_len);
}
```

---

## A_CRYPTO Error Discovered

The SimpleX App console showed `AGENT A_CRYPTO` for the ESP32 contact, with connStatus still "requested". Self-decrypt testing also failed:

```
DEBUG: Testing self-decrypt of encConnInfo...
Header decryption failed (try with full AAD?)
Self-decrypt FAILED!
```

The app cannot decrypt our AgentConfirmation. The server accepts the message (OK response), but cryptographic verification fails in the app. This is a separate issue from the Reply Queue decryption problem, related to Double Ratchet header encryption AAD construction.

---

## Status After Session 9

Working: Reply Queue server-level decryption (16106 bytes), identical message format confirmed on both queues.

Discovered: A_CRYPTO error in app (header encryption AAD issue in our AgentConfirmation). Self-decrypt test also fails.

Remaining: Fix A_CRYPTO error (header encryption AAD), parse Reply Queue agent messages (HELLO, CON, MSG), implement full duplex communication.

---

*Part 6 - Session 9: Reply Queue Server-Level Decryption*
*SimpleGo Protocol Analysis*
*Original date: January 27, 2026*
*Rewritten: March 4, 2026 (v2)*
*HSalsa20 fix, server-level decrypt working, A_CRYPTO discovered*
