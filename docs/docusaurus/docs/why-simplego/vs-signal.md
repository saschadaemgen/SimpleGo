---
title: SimpleGo vs Signal
sidebar_position: 4
---

# SimpleGo vs Signal

Signal is the gold standard for encrypted messaging apps. Its cryptographic design -- the Signal Protocol, now used by WhatsApp, Google Messages, and many others -- is sound and well-audited. This is not a criticism of Signal encryption.

The question is what Signal runs on top of.

## The Platform Problem

Signal runs on Android or iOS. Both platforms include a baseband processor with DMA access running closed-source firmware, millions of lines of OS code, hundreds of background services with network access, app store policies that can force updates or removal, and platform-level telemetry running below the app layer.

Signal encrypts your messages. The platform around Signal does not.

## The Identity Problem

Signal requires a phone number to register. A phone number is a persistent identity tied to a real-world identity registered with a telecom provider. Signal knows your phone number. Your contacts know your phone number.

SimpleGo uses the SimpleX Protocol, which has no user identifiers of any kind. No phone numbers. No usernames. No user IDs. Communication uses ephemeral unidirectional queues. No party -- including the server -- can correlate senders and recipients.

## The Polling Problem

Signal runs on a smartphone that must continuously maintain connections to receive push notifications. The underlying network architecture requires endpoint-based addressing -- the phone must be reachable at a known address.

SimpleX resource-based addressing eliminates this entirely. The device opens a connection and receives packets as they arrive. No polling. No persistent reachability requirement.

## Encryption Layers

| System | Per-Message Encryption Layers |
|--------|-------------------------------|
| Signal | 2 (Signal Protocol + TLS) |
| SimpleGo | 4 (Double Ratchet + Per-Queue NaCl + Server NaCl + TLS 1.3) |

## The Hardware Difference

Signal is software running on a general-purpose computer that fits in your pocket. SimpleGo is a dedicated device whose entire purpose is encrypted messaging. It runs ~50,000 lines of C. Signal runs on top of an OS with ~50,000,000 lines of code.

## When Signal is the Right Choice

Signal is excellent for most users. It is free, runs on hardware you already own, and provides strong encryption for everyday use. If your threat model is protecting personal communications from casual surveillance and data brokers, Signal is the right tool.

SimpleGo is for situations where the platform itself cannot be trusted.
