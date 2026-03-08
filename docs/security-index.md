---
slug: /security
sidebar_position: 6
title: Security
---

# Security

SimpleGo's security architecture is built on the principle that every layer must be independently defensible. Compromising one layer must not weaken any other.

## Four Encryption Layers Per Message

| Layer | Algorithm | Protects Against |
|-------|-----------|-----------------|
| 1. Double Ratchet (E2E) | X3DH (X448) + AES-256-GCM | End-to-end interception. PFS + post-compromise security. |
| 2. Per-Queue NaCl | X25519 + XSalsa20 + Poly1305 | Traffic correlation between queues |
| 3. Server-to-Recipient NaCl | NaCl cryptobox | Correlation of server I/O frames |
| 4. TLS 1.3 | mbedTLS, ALPN `smp/1` | Network-level attackers |

Content padding to 16 KB fixed blocks at every layer. A network attacker sees only equal-sized packets.

## No Persistent Identity

No user IDs, phone numbers, or usernames. Communication uses ephemeral unidirectional queues. No party - including the server - can correlate senders and recipients.

## No Baseband Processor

No cellular modem with DMA access running proprietary firmware. This eliminates the entire class of baseband vulnerabilities documented in academic research (BASECOMP, BaseMirror).

## Three Hardware Classes

SimpleGo implements security across three hardware tiers, from development boards to high-security devices with triple-vendor secure elements.

| Feature | Class 1 (ESP32) | Class 2 (STM32 + SE) | Class 3 (STM32 + Triple SE) |
|---------|-----------------|---------------------|---------------------------|
| Key Storage | eFuse HMAC-derived | Secure Element (EAL5+) | Triple SE (EAL5+/6+) |
| Security Modes | 4 (Open/Vault/Fortress/Bunker) | 2 | 1 (always maximum) |
| Cost to Read Keys | $2,000+ with vault | $30,000+ | $200,000+ (3 vendors) |
| Target | Alpha testers, Kickstarter | Journalists, activists | High-risk individuals |

## Security Documentation

- [Security Overview](./security/security-overview.md) - complete threat model, three hardware classes, honest limitations, comparison with alternatives
- [Known Vulnerabilities](./security/class-1-04-known-vulnerabilities.md) - every published CVE and attack against ESP32 family, with ESP32-S3 applicability assessment
- [Attack Equipment Economics](./security/class-1-05-attack-equipment-economics.md) - what it costs to break a SimpleGo device, from $15 to $400,000
- [Four Security Modes](./security/class-1-10-four-security-modes.md) - Open, Vault, Fortress, Bunker explained

### Hardware Class 1 Deep Dive (12 Documents)

| # | Document |
|---|----------|
| 01 | [Overview and Threat Model](./security/class-1-01-overview-and-threat-model.md) |
| 02 | [ESP32-S3 eFuse Architecture](./security/class-1-02-efuse-architecture.md) |
| 03 | [HMAC-Based NVS Encryption](./security/class-1-03-hmac-nvs-encryption.md) |
| 04 | [Known Vulnerabilities and Attack Research](./security/class-1-04-known-vulnerabilities.md) |
| 05 | [Attack Equipment Economics](./security/class-1-05-attack-equipment-economics.md) |
| 06 | [Runtime Memory Protection](./security/class-1-06-runtime-memory-protection.md) |
| 07 | [Post-Quantum Readiness](./security/class-1-07-post-quantum-readiness.md) |
| 08 | [Flash Encryption Deep Dive](./security/class-1-08-flash-encryption.md) |
| 09 | [Secure Boot V2](./security/class-1-09-secure-boot-v2.md) |
| 10 | [Four Security Modes](./security/class-1-10-four-security-modes.md) |
| 11 | [ESP32-P4 Evolution Path](./security/class-1-11-esp32-p4-evolution.md) |
| 12 | [Implementation Plan](./security/class-1-12-implementation-plan.md) |

### Hardware Class 2 and 3

- [Class 2 Overview](./security/class-2-01-overview.md) - ATECC608B integration (coming soon)
- [Class 3 Overview](./security/class-3-01-overview.md) - Triple-vendor Secure Elements (coming soon)

## Known Vulnerabilities

SimpleGo maintains an honest, public inventory of all known security gaps. No finding is downplayed or hidden.

| ID | Severity | Description | Status |
|----|----------|-------------|--------|
| SEC-01 | Critical | Decrypted messages in PSRAM never zeroed | Open (Session 45) |
| SEC-02 | Critical | NVS keys plaintext (no HMAC vault yet) | Open (Session 45) |
| SEC-03 | High | memset instead of zeroize in smp_storage.c | Closed (Session 42) |
| SEC-04 | High | No memory wipe on display timeout | Open (Session 45) |
| SEC-05 | Medium | HKDF info parameter lacks device binding | Open (resolves with SEC-02) |
| SEC-06 | Medium | Post-quantum not yet active | Deferred (verified feasible) |

See [Known Vulnerabilities](./security/class-1-04-known-vulnerabilities.md) for the complete analysis including all published ESP32 CVEs and attack research.
