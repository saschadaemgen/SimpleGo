---
title: Roadmap
sidebar_position: 7
---

# Roadmap

## Current: Alpha (v0.1.x)

SimpleGo is in active alpha development on the LilyGo T-Deck Plus reference hardware.

**Completed:**
- Full SMP protocol implementation verified against official SimpleX Chat app
- 128 simultaneous contacts with per-contact Double Ratchet state
- AES-256-GCM encrypted chat history on SD card (HKDF-SHA256 per-contact key derivation)
- WiFi Manager with multi-network support and WPA3
- Delivery receipts (double checkmark)
- Hardware Abstraction Layer
- Zero printf in production code
- 47 source files with AGPL-3.0 license headers

**In progress:**
- Keep-Alive (PING/PONG) implementation
- Contact management UI refinements

## Near-term: Kickstarter Preparation

- eFuse + NVS Flash Encryption (`nvs_flash_secure_init`) combined with post-quantum crypto (CRYSTALS-Kyber)
- Web-based firmware flashing tool (esptool-js at simplego.dev/flash)
- Independent security audit
- CE marking preparation (self-declaration referencing LilyGo test reports)
- GitHub release with web flashing tools

## Medium-term: Model 2 Shield

- Custom PCB design (STM32U585, Cortex-M33 with TrustZone)
- Dual-vendor secure elements (Microchip ATECC608B + Infineon OPTIGA Trust M)
- Active tamper detection (light sensor, battery-backed SRAM, PCB mesh)
- WiFi 6 + 4G LTE + LoRa connectivity
- CNC-milled aluminum enclosure

## Long-term: Model 3 Vault

- Triple-vendor secure elements (+ NXP SE050)
- STM32U5A9 with 4MB Flash
- DS3645 supervisor with sub-microsecond zeroization
- WiFi 6 + 5G NR + LoRa + satellite
- Potted CNC aluminum, hand-assembled in Germany

## Not on the Roadmap

SimpleGo will not add cloud backup, account recovery, phone number registration, app store distribution, or any feature that requires a persistent user identity. These are not missing features -- they are intentional absences.
