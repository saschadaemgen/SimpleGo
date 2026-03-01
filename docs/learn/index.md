---
title: "Introduction to SimpleGo"
sidebar_position: 1
---

# Introduction to SimpleGo

**SimpleGo** is the first known native implementation of the SimpleX Messaging Protocol outside the official Haskell codebase. Built in C for ESP32-S3 microcontrollers, it enables secure, smartphone-free messaging on dedicated hardware devices.

## What is SimpleX?

SimpleX is a messaging protocol designed with a radical approach to privacy: **no user identifiers whatsoever**. Unlike Signal (phone numbers), Matrix (user IDs), or email (addresses), SimpleX uses only pairwise, anonymous message queues. There is no user profile stored on any server.

## What is SimpleGo?

SimpleGo takes this protocol and implements it natively on embedded hardware — specifically the ESP32-S3 platform. This means:

- **No smartphone required** — the device runs the protocol independently
- **Dedicated hardware** — purpose-built for secure messaging
- **Native C implementation** — no Haskell runtime, no garbage collector
- **Constrained environment** — 320KB SRAM, careful memory management

## Who is this documentation for?

| Audience | Start Here |
|----------|------------|
| **Curious about SimpleX** | [Protocol Stack Overview](/learn/architecture/protocol-stack) |
| **Protocol engineers** | [Specification](/spec) |
| **C/Embedded developers** | [Implementation Guide](/implement) |
| **Following the journey** | [Protocol Analysis](/protocol-analysis) |

## Project Status

SimpleGo is under active development. The receive chain (contact queue → TLS → SMP → NaCl → Double Ratchet → plaintext) is fully working. Current focus is on implementing the Reply Queue for bidirectional communication.

:::info License
Specification: [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) · Software: [AGPL-3.0](https://github.com/cannatoshi/SimpleGo/blob/main/LICENSE)
:::
