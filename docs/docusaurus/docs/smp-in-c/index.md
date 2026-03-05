---
title: SMP in C
sidebar_position: 1
---

# Implementing the SimpleX Messaging Protocol in C

This section documents what we learned implementing the SimpleX Messaging Protocol (SMP) from scratch in C on an ESP32-S3 microcontroller.

SimpleGo is the first external implementation of SMP outside the official Haskell reference codebase. There is no other documentation like this. Everything here was learned by reading the Haskell source, the protocol specification, direct conversations with Evgeny Poberezkin (the protocol creator), and thousands of hours of debugging on real hardware.

## Who this is for

If you are implementing SMP in any language other than Haskell, this guide will save you weeks. The protocol specification tells you *what* the protocol does. This guide tells you *how to actually implement it* -- the parts that are obvious in Haskell but completely non-obvious in C, the pitfalls that will silently corrupt your crypto state, and the architectural decisions that only make sense once you understand why they exist.

## What is covered

- [Overview](./overview) -- why C, why embedded, what is different from the Haskell implementation
- [Transport](./transport) -- TLS 1.3 with mbedTLS, block framing, the 16KB hard limit
- [Queue Lifecycle](./queue-lifecycle) -- NEW, KEY, SUB, SEND, ACK in full detail
- [Handshake](./handshake) -- AgentConfirmation, Reply Queue, HELLO exchange
- [Encryption](./encryption) -- X448, NaCl cryptobox, cbNonce construction, HKDF
- [Double Ratchet](./ratchet) -- implementing the ratchet in C with PSRAM state persistence
- [Idempotency](./idempotency) -- the most critical lesson: how to handle lost server responses
- [Subscription](./subscription) -- PING/PONG keep-alive, session validation, END handling
- [Pitfalls](./pitfalls) -- everything that will silently break your implementation

## The golden rule

> "Make sure the ratio is about 100x reading to writing."
>
> -- Evgeny Poberezkin

Read the Haskell source before writing any C. Read it again. The protocol spec is correct but incomplete. The Haskell implementation is the ground truth.
