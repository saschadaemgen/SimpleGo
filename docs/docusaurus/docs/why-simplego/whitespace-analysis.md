---
title: "Hardware Security Whitespace"
sidebar_position: 4
---

# Hardware Security Whitespace Analysis

**The combination of triple-layer per-message encryption, bare-metal firmware, no baseband processor, no persistent identity, triple-vendor secure elements, and fully open source code has never appeared in any commercial product, military device, academic prototype, or published concept.**

After exhaustive research across more than 70 devices and platforms spanning consumer, military, criminal, DIY, and mesh-networking domains, the maximum feature overlap found in any single device is three out of six.

## Feature Matrix

| Device | Triple-Layer Encryption | Bare-Metal | No Baseband | No Persistent Identity | Triple-Vendor SEs | Open Source | Score |
|--------|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Meshtastic (T-Deck) | ✗ | ✓ | ✓ | ✗ | ✗ | ✓ | **3/6** |
| MeshCore | ✗ | ✓ | ✓ | ✗ | ✗ | ✓ | **3/6** |
| Reticulum/RNode | ✗ | ✓ | ✓ | ✗ | ✗ | ✓ | **3/6** |
| Armachat | ✗ | ✓ | ✓ | ✗ | ✗ | ✓ | **3/6** |
| Necunos NC_1 | ✗ | ✗ (Linux) | ✓ | ~ | ✗ | ✓ | **2.5/6** |
| Keystone 3 Pro (wallet) | ✗ | ✓ | ✓ | N/A | ✗ (2 vendors) | ~ | **2.5/6** |
| goTenna Pro X2 | ✗ | ✓ | ✓ | ✗ | ✗ | ✗ | **2/6** |
| R&S TopSec Mobile | ✗ | ✓ (crypto only) | ✗ (pairs with phone) | ✗ | ✗ | ✗ | **1/6** |
| Sectra Tiger/S 7401 | ✗ | ✓ (custom) | ✗ | ✗ | ✗ | ✗ | **1/6** |
| GD Sectéra vIPer | ✗ | ✓ (embedded) | N/A | ✗ | ✗ | ✗ | **1/6** |
| Purism Librem 5 | ✗ | ✗ (Linux) | ~ (isolated) | ✗ | ✗ | ~ | **1/6** |
| GSMK CryptoPhone | ✗ (dual, not triple) | ✗ (Android) | ✗ | ✗ | ✗ | ~ | **0.5/6** |
| Bittium Tough Mobile 2C | ~ (types, not per-msg) | ✗ (Android) | ✗ | ✗ | ✗ | ✗ | **0.5/6** |
| GrapheneOS/Pixel | ✗ | ✗ (Android) | ✗ | ✗ | ✗ | ~ | **0.5/6** |
| Boeing Black | ✗ | ✗ (Android) | ✗ | ✗ | ✗ | ✗ | **0/6** |

## The Six Features

### 1. Triple-Layer Per-Message Encryption

SimpleX Protocol implements three cryptographically independent encryption layers per message. Signal uses Double Ratchet + TLS (two layers). Matrix uses Olm/Megolm + TLS. No other messaging protocol or hardware device implements comparable triple-layer per-message architecture.

The closest is the GSMK CryptoPhone, which uses dual parallel symmetric algorithms (AES-256 + Twofish) — two layers, not three. Bittium Tough Mobile 2C achieves three *types* of encryption (disk, VPN, E2E) but these operate at different network stack layers, not as three independent cryptographic envelopes per message.

### 2. Bare-Metal / No Smartphone OS

LoRa mesh devices (Meshtastic, MeshCore, Reticulum, Armachat) achieve this. Military devices like Sectra Tiger run custom embedded firmware. Smartphones — even GrapheneOS — do not.

### 3. No Baseband Processor

LoRa mesh devices have no cellular modem at all. The Necunos NC_1 eliminated the cellular modem entirely. Every other smartphone or cellular device retains a baseband with direct or IOMMU-mediated memory access to the application processor.

### 4. No Persistent Identity

No existing device implements no-identifier messaging. All mesh devices use persistent node identities (MAC-derived). All smartphone apps require phone numbers, usernames, or random persistent IDs. SimpleX Protocol's queue-based architecture with no persistent user identifier is unique in the messaging landscape — and no hardware device has ever implemented it.

### 5. Triple-Vendor Secure Elements

No device in any category uses secure elements from three different manufacturers. The closest is the Keystone 3 Pro hardware wallet, which uses three SE chips but from only two vendors (Microchip ATECC608B + two Maxim/Analog Devices chips). The triple-vendor concept appears to be entirely novel.

The NinjaLab "Eucleak" attack (2024) demonstrated a side-channel vulnerability in Infineon SLE78 secure elements that had been present for 14 years in a CC EAL5+ certified chip. Single-vendor architectures are vulnerable to exactly this scenario.

### 6. Fully Open Source

LoRa mesh devices (Meshtastic, MeshCore, Reticulum) are fully open source. Most military and enterprise devices are proprietary. GSMK CryptoPhone makes source available for review but not as fully open source.

## Conclusion

Three findings emerge definitively from this research.

First, no existing device or published concept combines even four of the six target features. The maximum observed is three — achieved only by LoRa mesh devices that satisfy bare-metal firmware, no baseband, and open source but lack multi-layer encryption, identity-free design, and secure elements entirely.

Second, triple-vendor secure elements appear to be an entirely novel concept — not found in any device, patent filing, or academic paper. The Eucleak attack provides strong empirical justification for this approach.

Third, SimpleX Protocol's triple-layer per-message encryption has no hardware implementation and no comparable architecture in any other messaging protocol.

The existing device landscape clusters in two distant regions: high-assurance military devices with classified algorithms and TEMPEST shielding but running on closed-source, baseband-equipped platforms with mandatory identity binding; and open-source mesh devices with bare-metal firmware and full transparency but zero hardware security features. SimpleGo targets the space between these clusters.

The encrypted communication device market, projected to reach $8.36 billion by 2034, has no entrant approaching this combination.
