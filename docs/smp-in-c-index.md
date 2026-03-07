---
slug: /smp-in-c
sidebar_position: 4
title: Implementing SMP in C
---

# Implementing the SimpleX Messaging Protocol in C

This is the definitive guide to implementing the SimpleX Messaging Protocol (SMP) in native C on microcontrollers. This knowledge exists nowhere else -- SimpleGo is the world's first implementation outside the official Haskell reference codebase.

Everything documented here was reverse-engineered from the Haskell source, verified byte-for-byte against the official SimpleX Chat App, and battle-tested on ESP32-S3 hardware.

## Protocol Reference

- [Crypto Primitives](/CRYPTO) -- X448, NaCl, XSalsa20, cbNonce construction, HKDF
- [Protocol Operations](/PROTOCOL) -- SMP handshake, queue lifecycle, message flow
- [Wire Format](/WIRE_FORMAT) -- 16 KB block framing, TLV encoding, content padding

## Key Implementation Insights

**Non-standard XSalsa20:** SimpleX uses `HSalsa20(key, zeros[16])` instead of the standard `HSalsa20(key, nonce[0:16])`. Without this exact knowledge, decryption fails silently.

**X448 byte order:** Haskell's `cryptonite` library outputs X448 keys in reversed byte order. You must explicitly reverse bytes for protocol compatibility.

**Lost Response Handling (from Evgeny Poberezkin):** All state-changing commands must be idempotent. Keys are persisted to flash BEFORE the command is sent. If the response is lost, the same stored key is reused on retry.

```
Generate key -> Persist to flash -> Send command -> [response lost] -> Retry with SAME key
```

**Subscription rules:** `NEW` creates the queue already subscribed. Subsequent `SUB` is a noop (but re-delivers the last unACKed message). Subscription can only exist on one socket -- subscribing from a second socket causes the first to receive `END`.

## Coming Soon

Detailed per-topic guides covering transport, queue lifecycle, handshake, encryption details, ratchet implementation, idempotency patterns, subscription management, and common pitfalls are in preparation.
