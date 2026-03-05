---
title: Why SimpleGo
sidebar_position: 1
---

# Why SimpleGo

## The problem with smartphones

Every smartphone -- no matter how hardened -- contains a baseband processor: a secondary computer running proprietary firmware with direct memory access, permanently connected to the cellular network. Academic research (BASECOMP, USENIX Security 2023) found 29 exploitable bugs in baseband firmware from major vendors. This is not a software problem that patches can solve. It is a hardware architecture problem.

SimpleGo eliminates the baseband entirely. There is no cellular modem in the device. The attack surface does not exist.

## The problem with encrypted messenger apps

Signal, WhatsApp, and similar applications provide strong end-to-end encryption -- but they run on top of Android or iOS, which means millions of lines of OS code, hundreds of background services with network access, app store policies, and platform-level telemetry. The encryption is sound. Everything around it is not.

SimpleGo runs ~50,000 lines of C directly on a microcontroller via FreeRTOS. That is three orders of magnitude less code than a smartphone OS.

## The network architecture advantage

SimpleX is built around resource-based addressing rather than endpoint-based addressing. This is a fundamental architectural difference from conventional messaging.

With endpoint-based addressing (how the Internet normally works), communicating endpoints must exchange network addresses -- exposing each endpoint IP address to its counterparties. Mobile devices lacking fixed IP addresses must continuously poll servers to receive data, consuming energy and bandwidth proportional to polling frequency.

SimpleX resource-based addressing eliminates polling entirely. The device opens a connection to a router and receives packets as they arrive on its resource address. No continuous polling. No IP address exposure. This is why a SimpleGo device can run efficiently on battery -- the protocol architecture makes it possible at the hardware level.

> SimpleGo "demonstrates the energy efficiency of resource-based addressing: the device receives packets without continuous polling."
>
> -- Evgeny Poberezkin, SimpleX Network: Technical Architecture (2026)

## The combination that has never existed

After exhaustive research across more than 70 devices -- consumer, military, mesh networking, and open source -- no single device has ever combined all six of these properties simultaneously:

1. 4-layer per-message encryption
2. Bare-metal firmware (no smartphone OS)
3. No baseband processor
4. No persistent identity
5. Multi-vendor secure elements
6. Fully open source

The maximum found in any existing device is three out of six. SimpleGo is designed to achieve all six.

## Comparisons

- [SimpleGo vs GrapheneOS](./vs-grapheneos) -- why a hardened Android phone is not enough
- [SimpleX vs Matrix](./vs-matrix) -- why the protocol matters as much as the app
- [SimpleGo vs Signal](./vs-signal) -- the app vs the dedicated device
- [SimpleGo vs Briar](./vs-briar) -- mesh networking vs dedicated hardware
