---
slug: /hardware
sidebar_position: 5
title: Hardware
---

# Hardware

SimpleGo targets three hardware tiers, each addressing a different threat model. The firmware is designed for portability -- one codebase across all tiers.

## Current Platform: LilyGo T-Deck Plus

The development and alpha platform. Off-the-shelf, available for approximately EUR 50-70.

| Property | Value |
|----------|-------|
| MCU | ESP32-S3 Dual-Core 240 MHz |
| RAM | 512 KB SRAM + 8 MB PSRAM |
| Display | 320x240 ST7789V |
| Input | BB Q20 QWERTY keyboard (I2C at 0x55) |
| Connectivity | WiFi 802.11 b/g/n, Bluetooth 5.0 |
| Storage | MicroSD card slot (SPI2 bus shared with display) |
| Backlight | GPIO 42, pulse-counting protocol, 16 levels |

## Three Security Tiers

**Tier 1 -- DIY (EUR 100-500):** ESP32-S3 with single secure element (ATECC608B). Secure Boot v2, Flash Encryption. For developers, makers, and privacy enthusiasts.

**Tier 2 -- Secure (EUR 500-1,500):** Custom PCB with dual-vendor secure elements (ATECC608B + OPTIGA Trust M). Tamper detection, CNC aluminum enclosure. For journalists, activists, legal professionals.

**Tier 3 -- Vault (EUR 1,500-15,000):** Triple-vendor secure elements (+ NXP SE050), sub-microsecond zeroization, potted enclosure, hand-assembled in Germany. For enterprise and high-risk individuals.

## Hardware Documentation

- [Adding a New Device](/ADDING_NEW_DEVICE) -- HAL porting guide

## Coming Soon

Detailed documentation for component selection, PCB design, enclosure design, and the Hardware Abstraction Layer is in preparation.
