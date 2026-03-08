---
title: "Security Overview"
sidebar_position: 1
---

![SimpleGo Security Architecture](../../.github/assets/github_header_security_architecture.png)

# Security Overview

SimpleGo's security architecture spans three hardware classes, from off-the-shelf development boards to high-security devices with triple-vendor secure elements. This document provides the high-level overview. For technical depth on any specific topic, the detailed documentation is linked at the bottom.

---

## The Smartphone Problem

Modern smartphones present fundamental security challenges that cannot be fully mitigated through software alone.

| Aspect | Typical Smartphone |
|--------|-------------------|
| Lines of Code | ~50 million (Android/iOS) |
| Running Processes | Hundreds at any time |
| Network Connections | Dozens in background |
| Trusted Parties | OS vendor, carrier, app developers, ad networks |
| Update Control | Vendor-controlled, can be forced |
| Baseband Processor | Closed-source firmware, direct memory access, always active |
| Telemetry | Continuous data collection by multiple parties |

### The Baseband Problem

Every smartphone contains a baseband processor - a separate computer running closed-source firmware that handles cellular communication. This processor runs proprietary firmware that cannot be audited, has direct memory access (DMA) on many devices, remains active whenever the device has cellular signal, and contains known vulnerability classes (29 bugs found by BASECOMP, USENIX Security 2023). It cannot be disabled without losing cellular functionality and operates independently of the main operating system.

Even hardened mobile operating systems like GrapheneOS, while significantly improving security posture, cannot eliminate this architectural limitation. GrapheneOS provides excellent sandboxing, verified boot, and hardened memory allocation - but it still runs on hardware with a closed-source baseband processor.

---

## The Dedicated Hardware Approach

SimpleGo takes a fundamentally different approach: minimize the trusted computing base (TCB) and eliminate the baseband processor entirely.

### Attack Surface Comparison

```
Smartphone (Android/iOS):             SimpleGo (ESP32-S3):
|-- Operating System (~20M lines)     |-- FreeRTOS Kernel (~10K lines)
|-- System Services (~10M lines)      |-- Network Stack (~20K lines)
|-- Browser Engine (~5M lines)        |-- Crypto Libraries (~15K lines)
|-- JavaScript Runtime                +-- SimpleGo Application (~22K lines)
|-- App Framework                         Total: ~67K lines
|-- Hundreds of Apps
|-- Google/Apple Services                 Reduction: ~750x smaller
|-- Baseband Processor (closed)
|-- Bluetooth Stack
|-- NFC Stack
+-- Telemetry Services
    Total: ~50M lines
```

### What This Eliminates

| Threat Vector | Smartphone | SimpleGo |
|---------------|------------|----------|
| Remote Code Execution | Large attack surface | Minimal (WiFi + TLS only) |
| Supply Chain Attacks | Hundreds of dependencies | Few, auditable dependencies |
| Malicious Applications | Always possible | No app installation |
| Browser Exploits | Major risk vector | No browser |
| JavaScript Attacks | Ubiquitous | No JavaScript engine |
| Telemetry and Tracking | Built into OS | None |
| Forced Updates | Vendor-controlled | User-controlled |
| Baseband Attacks | Always possible | No baseband processor |
| Identity Tracking | IMEI, phone number, SIM | No persistent identity (SimpleX Protocol) |

---

## Three Hardware Classes

SimpleGo implements a tiered security model, allowing users to select hardware appropriate to their threat model.

### Hardware Class Comparison

| Feature | Class 1 | Class 2 | Class 3 |
|---------|---------|---------|---------|
| **MCU** | ESP32-S3 / ESP32-P4 | STM32U585 | STM32U5A9 |
| **TrustZone** | No (P4: partial via PMS) | Yes | Yes |
| **Secure Elements** | None (eFuse + HMAC) | 1 (ATECC608B) | 3 (ATECC608B + OPTIGA Trust M + SE050) |
| **Secure Boot** | V2 (RSA-3072) | STM32 + SE verification | Multi-stage with triple SE |
| **Flash Encryption** | AES-XTS (128 or 256) | AES-XTS + SE key wrap | AES-XTS + triple SE |
| **Key Storage** | eFuse HMAC-derived | Secure Element (EAL5+) | Triple SE (EAL5+/6+) |
| **NVS Encryption** | HMAC-based (runtime derived) | SE-backed | Triple SE-backed |
| **Tamper Detection** | None (P4: Power Glitch Detect) | Light sensor, PCB mesh | Full environmental suite |
| **Tamper Response** | None | Key zeroization | Supercapacitor-backed wipe |
| **JTAG Protection** | Disable via eFuse | Hardware disabled | Physically removed |
| **Security Modes** | 4 (Open/Vault/Fortress/Bunker) | 2 (Standard/Locked) | 1 (Always maximum) |
| **Duress PIN** | No (planned for P4) | Yes | Yes |
| **Dead Man's Switch** | No | Yes | Yes |
| **Post-Quantum Ready** | Verified feasible | Planned | Planned |
| **Target Users** | Developers, alpha testers, Kickstarter | Journalists, activists | High-risk individuals, organizations |
| **Estimated Price** | EUR 50-100 | EUR 400-600 | EUR 1,000-1,500 |

### Cost to Extract Keys (Physical Attack)

| Attack Method | Class 1 (no vault) | Class 1 (with vault) | Class 2 | Class 3 |
|--------------|-------------------|---------------------|---------|---------|
| Flash chip readout ($15 reader) | All keys exposed | Encrypted (blocked) | Encrypted + SE | Encrypted + triple SE |
| Side-channel analysis ($2,000+) | N/A (flash is easier) | Days to weeks, 1 device | Weeks to months | Requires breaking 3 independent SEs |
| EM fault injection ($30,000+) | N/A | Hours once tuned, 1 device | SE-specific research needed | Must defeat 3 vendors independently |

---

## Hardware Class 1: Four Security Modes

Class 1 is unique in offering four progressive security modes on the same hardware. Each mode activates additional eFuse-based protections. Transitions are one-way (lower to higher only) because eFuse burns are permanent.

| Mode | Name | Key Protection | Firmware Protection | Update Method | Provisioned By |
|------|------|---------------|--------------------|--------------|-|
| 1 | Open | None (plaintext in flash) | None | USB, Web Flash, OTA | Nobody |
| 2 | NVS Vault | HMAC-encrypted (eFuse-derived) | None (open source anyway) | USB, Web Flash, OTA | Device (automatic at first boot) |
| 3 | Fortress | HMAC-encrypted | AES-XTS flash encryption | OTA only | Manufacturer |
| 4 | Bunker | HMAC-encrypted | AES-XTS + RSA-3072 Secure Boot | Signed OTA only | Manufacturer |

Mode 2 (NVS Vault) is the planned default for the Alpha release and Kickstarter devices. Every device generates its own unique 256-bit HMAC key at first boot, burns it into a one-time-programmable eFuse block, and uses the hardware HMAC peripheral to derive NVS encryption keys at runtime. The raw key is hardware-protected and invisible to software. No master key exists. Not even the manufacturer can decrypt a device's stored keys.

---

## Cryptographic Architecture

Every message passes through four cryptographically independent encryption layers. They are nested envelopes, not sequential stages.

| Layer | Algorithm | Library | Protects Against |
|-------|-----------|---------|-----------------|
| 1. Double Ratchet (E2E) | X3DH (X448) + AES-256-GCM + HKDF-SHA512 | wolfSSL + mbedTLS | End-to-end interception. Perfect Forward Secrecy + Post-Compromise Security. |
| 2. Per-Queue NaCl | X25519 + XSalsa20 + Poly1305 | libsodium | Traffic correlation between queues if TLS compromised. |
| 3. Server-to-Recipient NaCl | X25519 + XSalsa20 + Poly1305 | libsodium | Correlation of incoming/outgoing server traffic. |
| 4. TLS 1.3 Transport | TLS 1.3, ALPN smp/1 | mbedTLS | Network-level interception. Transport only. |

Content padding to fixed 16 KB blocks is applied at every layer. A network observer sees only identical-sized packets regardless of actual message length.

### Key Storage Hierarchy

| Key Type | Storage Location | Protection |
|----------|------------------|------------|
| Ratchet keys (X448 DH, chain keys) | NVS flash | HMAC-encrypted (Class 1 Mode 2+), SE (Class 2+) |
| Per-queue NaCl keypairs (X25519) | NVS flash | HMAC-encrypted (Class 1 Mode 2+), SE (Class 2+) |
| Chat history encryption (AES-256-GCM) | Derived via HKDF | Device-bound (eFuse HMAC binding) |
| TLS session keys | RAM only | Cleared after session by mbedTLS |
| Decrypted messages | PSRAM cache (30 msgs) | sodium_memzero on exit/switch/timeout |

---

## Comparison with Alternatives

### SimpleGo vs GrapheneOS

GrapheneOS is the most secure smartphone operating system available. SimpleGo is not a competitor - it is a complementary security layer for communications that require architectural isolation from general-purpose computing.

| Aspect | GrapheneOS (Pixel) | SimpleGo Class 1 | SimpleGo Class 3 |
|--------|-------------------|-------------------|-------------------|
| TCB Size | ~50M lines | ~67K lines | ~67K lines |
| Baseband | Yes (IOMMU isolated) | None | None |
| Secure Element | Titan M2 (dedicated, SCA-hardened) | eFuse + HMAC | Triple SE (3 vendors, 3 countries) |
| Persistent Identity | IMEI, phone number | None (SimpleX Protocol) | None |
| Encryption Layers | 2 (Signal: DR + TLS) | 4 (DR + 2x NaCl + TLS) | 4 |
| Remote Attack Surface | Large (browser, BT, NFC, USB, baseband) | Minimal (WiFi + TLS only) | Minimal |
| Physical Attack (Lab) | Titan M2 resists well | ESP32 eFuse breakable in hours | Triple SE, months+ |
| Daily Use | Full smartphone | Messaging only | Messaging only |

### SimpleGo vs Criminal Encrypted Phone Networks

Every criminal encrypted phone network shut down to date (EncroChat 2020, Sky ECC 2021, ANOM 2021, Phantom Secure 2018, Ghost 2024) shared the same fatal architecture: modified Android smartphones with centralized server infrastructure. They were compromised through server-side attacks, not by breaking encryption. SimpleX Protocol eliminates the centralized infrastructure. SimpleGo adds hardware key protection that none of these services implemented.

### SimpleGo vs LoRa Mesh Devices (Meshtastic, Reticulum)

LoRa mesh messengers achieve three of SimpleGo's target security features: bare-metal firmware, no baseband, and open source. They lack multi-layer encryption (max 2 layers vs 4), have no key protection (keys in unprotected flash), use persistent node identities, and provide no forward secrecy for group messages.

### SimpleGo vs Military Devices (Sectra Tiger, Sectéra vIPer)

Military Type 1 devices use classified algorithms, TEMPEST shielding, and tamper detection. They are also proprietary, closed-source, extremely expensive, and bound to military identity systems. SimpleGo is open-source, identity-free, and commercially available.

---

## Known Limitations (Honest Assessment)

No security finding is downplayed. This is an honest inventory.

| Limitation | Applies To | Impact | Mitigation |
|-----------|-----------|--------|-----------|
| ESP32-S3 AES hardware vulnerable to CPA side-channel | Class 1 | Flash encryption key extractable with $2,000+ equipment, physical access, hours of work | HMAC uses SHA-256 (different attack surface), per-device unique keys, Class 2+ for higher protection |
| Derived NVS keys in RAM without Secure Boot | Class 1 Mode 2 | Malicious firmware could read keys from RAM | Mode 4 (Secure Boot) prevents unauthorized firmware |
| No DPA countermeasures on ESP32-S3 | Class 1 (S3) | No hardware protection against power analysis | ESP32-P4 adds anti-DPA pseudo-rounds (unproven effectiveness) |
| LVGL pool cannot be securely zeroed | All Classes | Message fragments may persist in UI memory pool | Label clearing + screen destruction, full pool zeroing in Class 3 |
| Post-quantum not yet active | All Classes | X448/X25519 vulnerable to future quantum computers | Verified feasible on hardware, implementation planned |

---

## Detailed Documentation

For technical depth on any specific topic, see the Security Architecture documents:

### Hardware Class 1 (ESP32-S3 / ESP32-P4)

| # | Document | Description |
|---|----------|-------------|
| 01 | [Overview and Threat Model](./class-1-01-overview-and-threat-model.md) | Three security pillars, honest limitations, device comparisons |
| 02 | [ESP32-S3 eFuse Architecture](./class-1-02-efuse-architecture.md) | 11 blocks, 6 key blocks, read/write protection, espefuse.py reference |
| 03 | [HMAC-Based NVS Encryption](./class-1-03-hmac-nvs-encryption.md) | The vault mechanism, first-boot provisioning, migration, SEC-05 resolution |
| 04 | [Known Vulnerabilities and Attack Research](./class-1-04-known-vulnerabilities.md) | 8 CVEs/attacks cataloged, ESP32-S3 applicability matrix |
| 05 | [Attack Equipment Economics](./class-1-05-attack-equipment-economics.md) | Four equipment tiers ($400 to $400K), tool names and prices |
| 06 | [Runtime Memory Protection](./class-1-06-runtime-memory-protection.md) | sodium_memzero, screen lock, Duress PIN and Dead Man's Switch concepts |
| 07 | [Post-Quantum Readiness](./class-1-07-post-quantum-readiness.md) | sntrup761 vs ML-KEM, ESP32 benchmarks, SimpleX compatibility |
| 08 | [Flash Encryption Deep Dive](./class-1-08-flash-encryption.md) | XTS-AES, Dev/Release modes, OTA updates, PSRAM encryption |
| 09 | [Secure Boot V2](./class-1-09-secure-boot-v2.md) | RSA-3072 chain of trust, key management, anti-rollback |
| 10 | [Four Security Modes](./class-1-10-four-security-modes.md) | Open/Vault/Fortress/Bunker complete reference |
| 11 | [ESP32-P4 Evolution Path](./class-1-11-esp32-p4-evolution.md) | Key Management Unit, anti-DPA, dual-chip architecture |
| 12 | [Implementation Plan](./class-1-12-implementation-plan.md) | Concrete tasks, code examples, test procedures |

### Hardware Class 2 (STM32U585 + Single Secure Element)

| # | Document | Description |
|---|----------|-------------|
| 01 | [Overview and Architecture](./class-2-01-overview.md) | Coming soon - ATECC608B integration, TrustZone, key lifecycle |

### Hardware Class 3 "Vault" (STM32U5A9 + Triple Secure Elements)

| # | Document | Description |
|---|----------|-------------|
| 01 | [Overview and Architecture](./class-3-01-overview.md) | Coming soon - Triple-vendor SE, tamper detection, zeroization |

---

## The Fundamental Advantage

The security advantage of a dedicated messaging device is not about having better encryption than a smartphone - Signal's Double Ratchet on GrapheneOS is excellent. The advantage is architectural: 67,000 lines of auditable code instead of 50 million. No baseband processor. No browser engine. No app ecosystem. No persistent identity. Four independent encryption layers that protect against different attacker models simultaneously.

The most secure system is the simplest one that does its job correctly.

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 3.0 | March 2026 | Complete restructure: terminology updated to Hardware Class 1/2/3, four security modes added, CVE research integrated, honest limitations section added, detailed documentation linked (12 Class 1 documents) |
| 2.0 | January 2026 | Complete rewrite for three-tier architecture |
| 1.0 | January 2026 | Initial security model documentation |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
