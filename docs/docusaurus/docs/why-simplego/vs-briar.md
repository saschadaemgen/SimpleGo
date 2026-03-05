---
title: SimpleGo vs Briar
sidebar_position: 5
---

# SimpleGo vs Briar

Briar is a peer-to-peer encrypted messenger that works without a central server -- messages are routed directly between devices via Tor, WiFi, or Bluetooth. It is designed for extreme conditions where internet infrastructure is unreliable or compromised.

Both Briar and SimpleGo prioritize privacy over convenience. They solve different problems.

## Architecture Differences

| Aspect | Briar | SimpleGo |
|--------|-------|---------|
| Routing | P2P via Tor / WiFi / Bluetooth | Server-routed via SMP relay |
| Identity | Persistent key-based ID | No identity at all |
| Platform | Android app | Dedicated bare-metal device |
| Baseband | Present (Android phone) | Absent by design |
| Offline mesh | Yes (Bluetooth + WiFi direct) | No (requires WiFi) |
| Metadata protection | Strong (Tor routing) | Strong (SimpleX Protocol) |
| Polling required | No (P2P push) | No (resource-based addressing) |

## The Network Architecture

Both Briar and SimpleGo avoid the polling problem that plagues conventional messaging. Briar uses direct P2P connections. SimpleGo uses SimpleX resource-based addressing -- the device opens a connection to a relay and receives packets as they arrive without continuous polling.

## When Briar is the Right Choice

Briar is purpose-built for situations with no reliable internet -- natural disasters, protests, regions with censored or unavailable infrastructure. Its Bluetooth and WiFi direct modes work when no network is available at all. If physical proximity mesh networking matters, Briar has no equivalent.

## When SimpleGo is the Right Choice

SimpleGo is for situations where the hardware itself is the threat surface. Briar still runs on an Android phone with a baseband processor. A sufficiently capable adversary with access to baseband exploits can compromise the device below the app layer.

SimpleGo eliminates the baseband entirely. There is no cellular modem to exploit.

## Encryption

Both systems use strong encryption. Briar uses the Bramble Transport Protocol built on standard primitives. SimpleGo implements the SimpleX Protocol four-layer encryption.

The key difference is not encryption strength -- it is the hardware and platform on which the encryption runs.
