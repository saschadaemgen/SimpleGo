---
title: Overview
sidebar_position: 2
---

# SMP in C: Overview

## Why C on an embedded microcontroller

The SimpleX reference implementation is written in Haskell. Haskell is an excellent choice for a protocol implementation -- its type system makes illegal states unrepresentable, its concurrency model is elegant, and the code is remarkably readable once you understand the language.

C on an ESP32-S3 is a completely different environment:

| Aspect | Haskell (reference) | C on ESP32-S3 |
|--------|---------------------|---------------|
| Memory model | GC, heap-allocated | Manual, bounded PSRAM |
| Concurrency | STM, green threads | FreeRTOS tasks |
| Crypto | Haskell crypto libs | mbedTLS + libsodium |
| Error handling | Either/Maybe monads | Return codes |
| State persistence | In-memory + DB | NVS flash + SD card |
| Network | Haskell TLS | mbedTLS over LwIP |

## What is the same

The protocol itself is identical. Every byte on the wire must match. SMP is a binary protocol with precise framing -- there is no wiggle room. If your cbNonce construction is one byte off, decryption silently fails. If your block framing is wrong, the server closes the connection without explanation.

The four encryption layers are the same. The queue lifecycle commands are the same. The handshake sequence is the same. The server does not know or care what language your client is written in.

## What is different

**No automatic memory management.** Every crypto buffer must be explicitly allocated and zeroed after use. The ratchet state for 128 contacts lives in PSRAM as a static array. Key material is written to NVS flash with strict rules about what can run from which task stack.

**No type-safe state machine.** In Haskell, the compiler enforces that you cannot call SEND before completing the handshake. In C, nothing stops you. The protocol state machine must be implemented explicitly and defensively.

**Hardware constraints change everything.** Hardware AES is disabled because of a DMA conflict with PSRAM. WPA3 requires a specific auth threshold constant that is not the obvious choice. PSRAM-allocated task stacks cannot write to NVS. These constraints do not exist in the Haskell world.

## The SimpleX Network architecture

SimpleX is not just a messaging protocol -- it is a general-purpose packet routing network. Understanding this helps explain why the protocol is designed the way it is.

Each message travels a two-router path: the sending endpoint submits a packet to a first router, which forwards it to a second router, where the receiving endpoint retrieves it. This is why there are separate per-queue encryption layers for each hop -- Layer 2 protects the sender-to-first-router leg, Layer 3 protects the second-router-to-recipient leg.

All stream packets are a fixed size of 16,384 bytes. This is not arbitrary -- uniform packet size prevents traffic analysis based on message length. Routers buffer packets for hours to days, enabling asynchronous delivery between endpoints online at different times.

Critically for embedded hardware: resource-based addressing eliminates polling. The device opens a connection to a router and receives packets as they arrive on its resource address. It does not need to continuously ask "is there anything for me?" This is why SimpleGo can run efficiently on battery -- the protocol architecture makes it possible.

## Protocol version

SimpleGo implements SMP protocol version 6. The protocol has approximately five versioning layers in the full stack. Evgeny Poberezkin guarantees at least one year of backward compatibility for each version, so v6 is stable for production use.

Updated protocol documentation is in the `rcv-services` branch of the simplexmq repository, not in master.

## The implementation journey

SimpleGo was built by reading the Haskell source line by line and translating the logic to C. The key insight is that the Haskell code is the specification -- more precise and more complete than any written document. Before implementing any feature, read the corresponding Haskell code in `Agent.hs`, `Agent/Client.hs`, and `Crypto.hs`. Understand it completely before writing a single line of C.
