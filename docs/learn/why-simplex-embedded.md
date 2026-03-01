---
title: "Why SimpleX on Embedded Hardware"
sidebar_position: 2
---

# Why SimpleX on Embedded Hardware

✅ **Status: Complete**

## The Problem

Modern secure messaging requires a smartphone — a device that is:

- Constantly connected to cellular networks (trackable)
- Running a complex OS with a large attack surface
- Tied to an identity (SIM card, Apple ID, Google account)
- Dependent on app stores controlled by corporations

## The Solution

A dedicated messaging device that:

- Connects only via WiFi (no cellular tracking)
- Runs bare-metal firmware (minimal attack surface)
- Has no identity — just cryptographic keys
- Is fully open-source and auditable

## Why SimpleX specifically?

SimpleX is the only messaging protocol designed from the ground up without user identifiers. This makes it uniquely suited for hardware implementation because:

1. **No registration** — the device needs no phone number or email
2. **No central server** — any SMP relay server works
3. **Pairwise queues** — each connection is independent
4. **Forward secrecy** — Double Ratchet with header encryption

## Why ESP32?

The ESP32-S3 provides the right balance of capability and constraint:

| Feature | ESP32-S3 |
|---------|----------|
| CPU | Dual-core 240 MHz Xtensa LX7 |
| RAM | 512KB SRAM + 8MB PSRAM |
| Flash | 16MB |
| WiFi | 802.11 b/g/n |
| Crypto | Hardware AES, SHA, RSA acceleration |
| Cost | ~$5 per chip |
| Power | Deep sleep < 10μA |
