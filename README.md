<p align="center">
  <img src=".github/assets/simplego_banner.png" alt="SimpleGo — Dedicated Secure Communication Devices" width="1500" height="230">
</p>

# SimpleGo

**The first native implementation of the SimpleX Messaging Protocol for dedicated secure communication hardware.**

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Hardware: CERN-OHL-W-2.0](https://img.shields.io/badge/Hardware-CERN--OHL--W--2.0-green.svg)](#license)
[![Version](https://img.shields.io/badge/version-0.1.17--alpha-orange.svg)](#project-status)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-lightgrey.svg)](#supported-platforms)

## What is SimpleGo?

SimpleGo is the world's first native C implementation of the [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq) (SMP), built from the ground up for embedded microcontrollers. It is the first third-party implementation outside the official Haskell codebase — verified and endorsed by the protocol's creator.

The result is a new class of device: a dedicated, smartphone-independent messenger with hardware-enforced security that no existing product provides. Not a modified phone. Not an app. A purpose-built machine whose only job is to send and receive encrypted messages.

> *"This will literally be the most private and secure device that is possible, and it's exciting to see that they chose our network and protocol for it."*
> — **Evgeny Poberezkin**, creator of the SimpleX Protocol, MoneroTopia Conference, February 15, 2026

## Why This Matters

After surveying more than 70 devices across consumer, military, criminal, and open-source domains, the research is unambiguous: no existing device — not a hardened smartphone, not a military radio, not any dedicated hardware messenger — combines all six of the following properties simultaneously. The maximum overlap found in any single device is three. SimpleGo is designed to achieve all six.

These properties are not marketing claims. They are verifiable architectural decisions, each addressing a specific and documented threat:

**Triple-layer per-message encryption.** Every message passes through three independent cryptographic envelopes: a Double Ratchet with perfect forward secrecy, a per-queue NaCl cryptobox that prevents traffic correlation if TLS is compromised, and a server-to-recipient NaCl layer that prevents correlation between incoming and outgoing traffic. No other messaging protocol — and no existing hardware device — implements comparable per-message layering.

**Bare-metal firmware.** SimpleGo runs on a purpose-specific firmware stack with a trusted computing base orders of magnitude smaller than any smartphone OS. There is no browser, no app runtime, no background process that does anything other than send and receive encrypted messages.

**No baseband processor.** Every hardened smartphone on the market contains a baseband processor — a secondary computer running proprietary firmware with direct memory access, no user visibility, and a documented history of critical vulnerabilities. Academic research has found NAS AKA bypasses, hundreds of undisclosed commands, and remotely exploitable bugs in widely deployed baseband chipsets. SimpleGo eliminates this entire attack surface by design.

**No persistent identity.** SimpleX Protocol was designed from the ground up to prevent identity correlation. There are no phone numbers, no usernames, no persistent cryptographic identifiers visible to servers or the network. Every message queue uses one-time invitation links. Servers see only encrypted blobs and cannot construct a communication graph.

**Multi-vendor hardware secure elements.** Tier 2 and Tier 3 devices use secure elements from two and three independent manufacturers respectively. This architecture directly addresses the supply-chain risk demonstrated by the 2024 Eucleak attack, which recovered ECDSA private keys from Infineon SLE78 chips — chips carrying CC EAL5+ certification, embedded in millions of YubiKeys and hardware wallets for 14 years before the vulnerability was disclosed. With multi-vendor secure elements, no single manufacturer's vulnerability is sufficient to compromise the device.

**Fully open source.** The complete firmware, protocol implementation, and hardware designs are published under open licenses. Every cryptographic decision is auditable. There are no proprietary blobs, no closed drivers, no black boxes.

## Encryption Architecture

SimpleGo implements the full SimpleX encryption stack in native C, verified byte-for-byte against the Haskell reference implementation.

**Layer 1 — Double Ratchet (end-to-end):** X3DH key agreement over X448 followed by Double Ratchet with AES-256-GCM. Every message uses a unique derived key. Compromise of any single message key does not affect past or future messages. The architecture is prepared for hybrid post-quantum key exchange using CRYSTALS-Kyber + Streamlined NTRU Prime, scheduled for the production release.

**Layer 2 — Per-Queue NaCl cryptobox (queue isolation):** Each message queue carries its own independent X25519 + XSalsa20-Poly1305 encryption envelope. An attacker who compromises TLS cannot correlate traffic across queues.

**Layer 3 — Server-to-Recipient NaCl (metadata protection):** A third NaCl layer wraps each message server-side, ensuring that even an attacker with access to server infrastructure cannot correlate incoming and outgoing traffic for a given recipient.

All messages are padded to a fixed 16KB block size at each layer, eliminating message-length side channels.

## Keys at Rest

Cryptographic state — ratchet keys, queue credentials, contact data — is persisted to the device across reboots. The complete at-rest security architecture for the production release consists of four independent layers working together:

**eFuse-bound NVS encryption.** The ESP32-S3 eFuse block is a one-time-writable memory region physically embedded in the silicon. During device provisioning, a device-unique AES-256 key is generated by the hardware TRNG and burned permanently into eFuse. This key encrypts the entire NVS flash partition — the vault where all cryptographic material lives. Once burned, the key is inaccessible to any software interface; it is only usable by the AES hardware block for encryption and decryption. Physical extraction requires destructive chip decapping. This replaces the current development-phase plaintext NVS storage and ships together with post-quantum activation.

**Secure Boot.** The firmware boot chain is signed with an RSA-3072 key. Only firmware signed with the correct key can execute on the device. Combined with eFuse write-protection, this prevents both firmware replacement attacks and key extraction through modified firmware.

**Hardware secure elements.** Tier 2 and Tier 3 devices use dedicated secure element chips from independent manufacturers for long-term key storage. These chips perform all cryptographic operations internally and never expose raw key material on any bus.

**Cryptographic erasure on contact delete.** When a contact is removed, all associated NVS keys are overwritten with cryptographic-strength random data before deletion, preventing forensic recovery. This addresses a documented forensic vulnerability — recovering "deleted" data from flash storage was a primary vector in the takedown of EncroChat.

## Hardware Tiers

SimpleGo defines three hardware security tiers for different threat models. The design philosophy starts at the highest specification and creates lower tiers by removing components, keeping the architecture consistent across the entire product line.

### Tier 1: DIY — *For makers and privacy enthusiasts*

Off-the-shelf LilyGo T-Deck Plus hardware, available today, flashable by anyone.

| Specification | Detail |
|---------------|--------|
| **Microcontroller** | ESP32-S3 (Dual Xtensa LX7, 240 MHz, 8MB PSRAM) |
| **Secure element** | ATECC608B (Microchip) |
| **Security features** | Secure Boot v2, Flash Encryption, eFuse protection |
| **Connectivity** | WiFi 802.11 b/g/n, Bluetooth 5.0 |
| **Display** | 320×240 LCD with physical QWERTY keyboard |
| **Target price** | €100–200 |
| **Threat model** | Protection against casual and opportunistic adversaries |

### Tier 2: Secure — *For journalists, activists, and legal professionals*

Custom PCB with hardware separation between processing and communication.

| Specification | Detail |
|---------------|--------|
| **Microcontroller** | STM32U585 (ARM Cortex-M33 with TrustZone, 160 MHz) |
| **Secure elements** | Dual-vendor: ATECC608B (Microchip) + OPTIGA Trust M (Infineon) |
| **Tamper detection** | Ambient light sensor, battery-backed SRAM, PCB tamper mesh |
| **Connectivity** | WiFi 6, LTE Cat-M/NB-IoT (isolated, no DMA), LoRa |
| **Enclosure** | CNC-milled aluminum with security screws |
| **Target price** | €400–600 |
| **Threat model** | Protection against skilled adversaries with equipment |

### Tier 3: Vault — *For high-value targets facing state-level threats*

Maximum security with triple-vendor secure elements and active tamper response.

| Specification | Detail |
|---------------|--------|
| **Microcontroller** | STM32U5A9 (ARM Cortex-M33, 4MB Flash, TrustZone) |
| **Secure elements** | Triple-vendor: ATECC608B (Microchip) + OPTIGA Trust M (Infineon) + SE050 (NXP) |
| **Tamper supervisor** | Maxim DS3645 — 8 monitored inputs, sub-microsecond key zeroization |
| **Tamper response** | Full environmental monitoring (temperature, voltage, light, vibration, mesh continuity) |
| **Connectivity** | WiFi 6, 4G LTE / 5G NR (isolated), LoRa, satellite (optional) |
| **Enclosure** | Potted CNC aluminum — aluminum-filled epoxy prevents physical probing |
| **Manufacturing** | Hand-assembled in Germany, individually serialized |
| **Target price** | €1,000+ |
| **Threat model** | Protection against state-level adversaries with physical access |

## Project Status

The core protocol stack is functional and verified against the official SimpleX Chat application across 7 simultaneous contacts, including delivery receipts, encrypted chat history, and persistent crypto state across reboots.

### Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| TLS 1.3 transport | ✅ Complete | ALPN "smp/1" negotiation |
| SMP handshake | ✅ Complete | Version negotiation verified |
| Queue operations | ✅ Complete | CREATE, SUBSCRIBE, SEND, ACK |
| X3DH key agreement | ✅ Complete | Byte-for-byte verified against reference |
| Double Ratchet | ✅ Complete | Forward secrecy, post-compromise security |
| Triple-layer encryption | ✅ Complete | E2E + per-queue NaCl + server-recipient NaCl |
| Wire format encoding | ✅ Complete | Matches Haskell reference implementation |
| Bidirectional messaging | ✅ Complete | ESP32 ↔ SimpleX App, 7 contacts verified |
| Delivery receipts | ✅ Complete | ✓ sent, ✓✓ delivered |
| Multi-contact architecture | ✅ Complete | 128 contacts, per-contact reply queues in PSRAM |
| Persistent crypto state | ✅ Complete | Survives reboots, verified write |
| WiFi Manager | ✅ Complete | Multi-network storage, scan & connect UI, WPA3 |
| SD card encrypted history | ✅ Complete | AES-256-GCM, HKDF-SHA256 per-contact key derivation |
| Sliding window chat view | ✅ Complete | 5 visible bubbles, scroll loads older from SD |
| Cryptographic erasure on delete | ✅ Complete | NVS keys overwritten on contact removal |
| FreeRTOS multi-task system | ✅ Complete | Cross-core communication, ring buffers |
| Hardware Abstraction Layer | ✅ Complete | Device-independent protocol layer |
| eFuse-bound NVS encryption | 📋 Production | Device-unique AES-256 key burned to eFuse at provisioning |
| Post-quantum crypto (Kyber) | 📋 Production | Ships together with eFuse release |
| Keep-alive (PING/PONG) | 📋 Planned | |

### Memory Footprint

| Resource | Used | Available | Utilization |
|----------|------|-----------|:-----------:|
| PSRAM (contacts, ratchets, queues) | ~158 KB | 8 MB | 1.9% |
| Internal SRAM (TLS, crypto) | ~180 KB | 512 KB | 35% |
| NVS Flash (persistent state) | ~64 KB | 128 KB | 50% |

## Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                         │
│              User Interface · Screen Management               │
├───────────────────────────────────────────────────────────────┤
│                      PROTOCOL LAYER                           │
│    SimpleX SMP · Agent Protocol · Double Ratchet · X3DH       │
├───────────────────────────────────────────────────────────────┤
│               HARDWARE ABSTRACTION LAYER                      │
│  hal_display · hal_input · hal_network · hal_storage          │
├───────────────┬───────────────┬───────────────┬───────────────┤
│  T-Deck Plus  │  T-Deck Pro   │ SimpleGo Tier │   Desktop     │
│  ESP32-S3     │  ESP32-S3     │ STM32 + SE    │   SDL2 Test   │
└───────────────┴───────────────┴───────────────┴───────────────┘
```

The Protocol and Application layers are identical across all devices. Only the HAL implementations change. Adding a new hardware platform means implementing five interface files — the entire protocol stack and UI come for free.

### Source Structure

```
SimpleGo/
├── main/
│   ├── core/           # Protocol implementation (device-independent)
│   ├── crypto/         # Cryptographic operations
│   ├── hal/            # HAL interface headers
│   ├── net/            # Network and TLS transport
│   ├── protocol/       # SMP protocol encoding/decoding
│   ├── state/          # Persistent state management
│   ├── ui/             # User interface (device-independent)
│   └── util/           # Shared utilities
│
├── devices/
│   ├── t_deck_plus/    # LilyGo T-Deck Plus HAL
│   └── template/       # Template for new device ports
│
├── components/         # External libraries (LVGL, mbedTLS)
└── docs/               # Documentation and legal notices
```

## Building from Source

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) 5.5.2 or later
- Python 3.8 or later
- CMake 3.16 or later

### Quick Start

```bash
# Clone the repository
git clone https://github.com/saschadaemgen/SimpleGo.git
cd SimpleGo

# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh          # Linux/macOS
%IDF_PATH%\export.bat                   # Windows

# Configure WiFi and device settings
idf.py menuconfig

# Build, flash, and monitor
idf.py build flash monitor -p /dev/ttyUSB0    # Linux
idf.py build flash monitor -p COM6            # Windows
```

All settings including WiFi credentials, SMP server, and security options are managed through menuconfig. The `sdkconfig` file is excluded from version control.

## Supported Platforms

| Device | MCU | Status |
|--------|-----|--------|
| LilyGo T-Deck Plus | ESP32-S3 | Active development |
| LilyGo T-Deck Pro | ESP32-S3 | Q2 2026 |
| SimpleGo Secure (Tier 2) | STM32U585 | PCB design phase |
| SimpleGo Vault (Tier 3) | STM32U5A9 | PCB planning |

## Kickstarter

SimpleGo will launch a Kickstarter campaign once the Tier 1 prototype reaches feature completeness. Funding will cover custom PCB manufacturing for Tier 2 and Tier 3, secure element components from three independent vendors, an independent security audit as a prerequisite for v1.0, and company formation in Germany.

Follow this repository to be notified when the campaign launches.

## Security

Vulnerabilities should be reported privately via GitHub's private vulnerability reporting feature. Do not open public issues for security concerns.

The codebase has not yet undergone formal security audit. An independent audit is planned as a Kickstarter stretch goal and a prerequisite for the v1.0 release.

**What SimpleGo does not protect against:** compromise of SMP relay servers (mitigated by the protocol's metadata protection architecture, not eliminated), physical attacks on Tier 1 devices (addressed in Tier 2 and 3), and network-level traffic analysis (Private Message Routing is planned for a future release).

## License

| Component | License |
|-----------|---------|
| Software | [AGPL-3.0](LICENSE) |
| Hardware designs | CERN-OHL-W-2.0 |

AGPL-3.0 ensures that any network-facing modifications to SimpleGo must also be published as open source. Hardware designs are published under the CERN Open Hardware License to enable community manufacturing and modification.

## Legal

- [Disclaimer](docs/DISCLAIMER.md) — Warranty and liability
- [Legal Notice](docs/LEGAL.md) — Impressum and regulatory information
- [Trademark Notice](docs/TRADEMARK.md) — SimpleX name usage

## Acknowledgments

- [SimpleX Chat](https://simplex.chat/) — SimpleX Messaging Protocol
- [Espressif](https://www.espressif.com/) — ESP-IDF and ESP32 platform
- [LVGL](https://lvgl.io/) — Embedded graphics library
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) — TLS and cryptography

---

*SimpleGo is an independent project. It is not affiliated with, endorsed by, or connected to SimpleX Chat Ltd. The SimpleX name and protocol are used for interoperability purposes only. This software is provided as-is, without warranty. See [docs/DISCLAIMER.md](docs/DISCLAIMER.md) for full legal notices.*

<p align="center">
  <br>
  <b>SimpleGo — Secure messaging without smartphones.</b>
</p>