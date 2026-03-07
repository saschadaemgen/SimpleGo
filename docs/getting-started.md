---
slug: /getting-started
sidebar_position: 2
title: Getting Started
---

# Getting Started

## Option 1: Flash a Pre-Built Firmware

The fastest way to get started is the [SimpleGo Web Flash Tool](https://simplego.dev/flash.html). Connect a LilyGo T-Deck Plus via USB and flash directly from your browser -- no toolchain required.

## Option 2: Build from Source

**Prerequisites:** ESP-IDF 5.5.2, Git, Python 3.8+

```bash
git clone https://github.com/saschadaemgen/SimpleGo.git
cd SimpleGo
idf.py build flash monitor -p COM6
```

**Important flash rules:**

Normal build (`idf.py build flash monitor -p COM6`) for: UI changes, bug fixes, new features, layout changes.

Full erase (`idf.py erase-flash` then build) required for: new or changed NVS keys, crypto state changes, NVS encryption changes. Note that erase-flash clears NVS but Kconfig credentials survive (compiled into binary).

## Hardware

**Supported:** LilyGo T-Deck Plus (ESP32-S3, 8 MB PSRAM, 320x240 ST7789V, BB Q20 QWERTY keyboard)

See [Hardware](/hardware) for details on all three device tiers.

## Next Steps

- [Architecture Overview](/architecture) -- understand the system design
- [SMP in C Guide](/smp-in-c) -- deep dive into the protocol implementation
- [Security Model](/security) -- threat model and encryption layers
