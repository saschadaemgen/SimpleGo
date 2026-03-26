---
slug: /
sidebar_position: 1
title: SimpleGo Documentation
---

# SimpleGo Documentation

Welcome to the SimpleGo technical documentation -- the world's first native C implementation of the SimpleX Messaging Protocol on bare-metal microcontrollers.

## What is SimpleGo?

SimpleGo is a dedicated encrypted messaging device. No smartphone, no Android, no Linux, no baseband processor. Just an ESP32-S3 microcontroller running FreeRTOS with four independent encryption layers per message.

**Current hardware:** LilyGo T-Deck Plus (ESP32-S3, 320x240 display, QWERTY keyboard)

**Current status:** Beta v0.2.0 -- bidirectional encrypted messaging verified against the official SimpleX Chat App, 7 parallel contacts stable.

## Documentation Sections

**[Getting Started](/getting-started)** -- Flash a device, build from source, first steps.

**[Architecture](/architecture)** -- FreeRTOS task model, memory layout, inter-task communication.

**[SMP in C](/smp-in-c)** -- The definitive guide to implementing the SimpleX Messaging Protocol in C. This knowledge exists nowhere else.

**[Hardware](/hardware)** -- T-Deck Plus details, hardware tiers, PCB design, adding new devices.

**[Security](/security)** -- Threat model, four encryption layers, known vulnerabilities, audit log.

**[Reference](/reference/constants)** -- Constants, wire format, crypto primitives, changelog.

## Quick Links

- [GitHub Repository](https://github.com/saschadaemgen/SimpleGo)
- [Flash Tool](https://simplego.dev/flash.html) -- flash firmware directly from your browser
- [Product Overview](https://simplego.dev/product.html)
- [SimpleX Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)

---

*SimpleGo -- IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
