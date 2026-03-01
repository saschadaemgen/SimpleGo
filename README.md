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

SimpleGo is the world's first native C implementation of the [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq) (SMP), built from the ground up for embedded microcontrollers. It is the first third-party implementation outside the official Haskell codebase.

What comes out of this is a new class of device: a dedicated, smartphone-independent messenger with hardware-enforced security that nothing on the market currently provides. Not a modified phone. Not an app. A purpose-built machine whose only job is to send and receive encrypted messages.

> *"This is literally the most private and secure device that is possible."*
> — **Evgeny Poberezkin**, creator of the SimpleX Protocol, at MoneroTopia Conference 2026

## Why This Matters

The encrypted communications market is projected to reach $8.36 billion by 2034. Yet after going through more than 70 devices across consumer, military, and open-source domains, we couldn't find a single product that combines even four of these six security properties:

| Security Property | SimpleGo | Best Existing Device |
|-------------------|:--------:|:-------------------:|
| **Triple-layer per-message encryption** | ✓ | ✗ (max 2 layers) |
| **Bare-metal firmware (no smartphone OS)** | ✓ | ✓ (Meshtastic) |
| **No baseband processor** | ✓ | ✓ (Meshtastic) |
| **No persistent identity** | ✓ | ✗ |
| **Multi-vendor hardware secure elements** | ✓ | ✗ (no device, ever) |
| **Fully open source** | ✓ | ✓ (Meshtastic) |

The maximum overlap we found in any single device is three out of six, achieved only by LoRa mesh devices like Meshtastic and Reticulum. They cover bare-metal firmware, no baseband, and open source, but entirely lack multi-layer encryption, identity-free design, and hardware secure elements.

SimpleGo sits in the gap between two worlds that have never overlapped: high-assurance military devices with classified algorithms and tamper resistance but proprietary, closed-source platforms on one side, and open-source mesh devices with full transparency and no baseband but zero hardware security on the other. No product, prototype, or published concept bridges that gap.

## The Smartphone Problem

Every hardened smartphone on the market retains a cellular baseband processor. That's a secondary computer running proprietary firmware with direct memory access. Academic research (BASECOMP, USENIX Security '23) has found critical exploitable vulnerabilities in these chips, including NAS AKA bypasses and hundreds of undisclosed commands. Even the most security-conscious phones merely attempt to isolate the baseband rather than eliminate it.

| | Smartphone | SimpleGo Device |
|---|---|---|
| **Codebase** | ~50,000,000 lines | ~50,000 lines |
| **Baseband processor** | Closed-source, DMA access, always active | None |
| **Background services** | Hundreds, many with network access | One |
| **Telemetry** | Continuous, by OS vendor and apps | None |
| **Key storage** | Software or TEE | Hardware secure element |
| **Tamper detection** | None | Active monitoring (Tier 2+) |
| **Physical profile** | Obviously a phone | Generic electronics |
| **Disposability** | Impractical ($500+) | Designed for it (from €100) |

SimpleGo eliminates entire categories of attacks by simply not having the attack surface in the first place. No browser means no browser exploits. No app installation means no malware vector. No baseband with DMA means no cellular-based memory attacks. The trusted computing base shrinks by three orders of magnitude.

## Triple-Layer Encryption

SimpleGo implements the full SimpleX encryption architecture. That means three cryptographically independent layers per message, each defending against a different threat:

**Layer 1 — Double Ratchet (end-to-end):** X3DH key agreement followed by Double Ratchet with AES-256-GCM, providing perfect forward secrecy and post-compromise security. Every message uses a unique key. Future versions will add hybrid post-quantum key exchange (CRYSTALS-Kyber + Streamlined NTRU Prime).

**Layer 2 — Per-Queue NaCl (queue isolation):** Each message queue has its own X25519 + XSalsa20-Poly1305 encryption envelope, preventing traffic correlation between queues if TLS is compromised.

**Layer 3 — Server-to-Recipient NaCl (metadata protection):** An additional NaCl encryption layer prevents correlation between incoming and outgoing server traffic, even if TLS is compromised.

Content padding to a fixed 16KB block size is applied at each layer. No other messaging protocol implements comparable per-message triple encryption. Signal uses two layers. Matrix uses two layers. Among hardware devices, the maximum we found was two. SimpleGo is the first hardware implementation of this architecture.

## Hardware Tiers

SimpleGo defines three hardware security tiers for different threat models. The design philosophy starts with the highest-specification Tier 3 and creates lower tiers by removing components, which keeps the architecture consistent across the entire product line.

### Tier 1: DIY — *For makers and privacy enthusiasts*

The entry point. Uses off-the-shelf LilyGo T-Deck Plus hardware that anyone can purchase and flash.

| Specification | Detail |
|---------------|--------|
| **Microcontroller** | ESP32-S3 (Dual Xtensa LX7, 240 MHz, 8MB PSRAM) |
| **Secure element** | ATECC608B (Microchip) |
| **Security features** | Secure Boot v2, Flash Encryption, eFuse protection |
| **Connectivity** | WiFi 802.11 b/g/n, Bluetooth 5.0 |
| **Display** | 320×240 LCD with hardware keyboard |
| **Target price** | €100–200 |
| **Threat model** | Protection against casual and opportunistic adversaries |

### Tier 2: Secure — *For journalists, activists, and legal professionals*

Custom PCB with enhanced security architecture and hardware separation between processing and communication.

| Specification | Detail |
|---------------|--------|
| **Microcontroller** | STM32U585 (ARM Cortex-M33 with TrustZone, 160 MHz) |
| **Secure elements** | Dual-vendor: ATECC608B (Microchip) + OPTIGA Trust M (Infineon) |
| **Tamper detection** | Ambient light sensor, battery-backed SRAM, PCB tamper mesh |
| **Connectivity** | WiFi 6, LTE Cat-M/NB-IoT (isolated, no DMA), LoRa |
| **Enclosure** | CNC-milled aluminum with security screws |
| **Target price** | €400–600 |
| **Threat model** | Protection against skilled adversaries with equipment |

The dual-vendor secure element architecture means a vulnerability in one manufacturer's chip (like the Eucleak side-channel attack on Infineon SLE78, disclosed 2024) does not compromise the device. Cryptographic operations are split across vendors so that both must be compromised simultaneously.

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

The triple-vendor secure element concept is entirely novel. No device in any category, whether consumer, military, or academic prototype, has ever used secure elements from three different manufacturers. This architecture ensures that no single supply-chain compromise, manufacturing backdoor, or undiscovered silicon vulnerability can defeat the device.

**Regarding cellular connectivity:** Tier 2 and 3 devices include isolated cellular modules that function purely as data modems through a defined serial interface. Unlike smartphone baseband processors, these modules have no DMA access and no shared memory with the main processor. The cellular module can be physically disabled or removed without affecting WiFi operation.

## Project Status

The core protocol stack is functional. Bidirectional encrypted messaging between ESP32 hardware and the official SimpleX Chat application works, including delivery receipts, multi-contact management, and persistent crypto state across reboots.

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
| Bidirectional messaging | ✅ Complete | ESP32 ↔ SimpleX App confirmed |
| Delivery receipts | ✅ Complete | ✓ sent, ✓✓ delivered |
| Multi-contact architecture | ✅ Complete | 128 contacts, per-contact reply queues in PSRAM |
| Persistent crypto state | ✅ Complete | Survives reboots, 7.5ms verified write |
| FreeRTOS multi-task system | ✅ Complete | Cross-core communication, ring buffers |
| Hardware Abstraction Layer | ✅ Complete | Device-independent protocol layer |
| Contact management UI | ✅ Complete | Long-press menu, delete, info, message counts |
| NTP time synchronization | ✅ Complete | Real timestamps in chat bubbles |
| Display & keyboard backlight | ✅ Complete | 16-level display, I2C keyboard with auto-off |
| SD card chat history | ✅ Complete | Per-contact storage, FAT32, 64GB verified |
| SD card encrypted history | 🔧 In Progress | AES-256-GCM per-contact encryption layer |
| Sliding window chat view | 🔧 In Progress | 8–12 visible bubbles, load older from SD |
| Keep-alive (PING/PONG) | 📋 Planned | |
| WiFi Manager | 📋 Planned | Multi-network NVS storage, scan & connect UI |

### Memory Footprint

The entire multi-contact architecture fits comfortably within the ESP32-S3:

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

The Protocol and Application layers are identical across all devices. Only the HAL implementations change. Adding a new hardware platform means implementing five interface files, and the entire protocol stack and UI come for free.

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

## Supported Platforms

### Currently Active

| Device | MCU | Display | Input | Status |
|--------|-----|---------|-------|--------|
| LilyGo T-Deck Plus | ESP32-S3 | 320×240 LCD | Keyboard, trackball, touch | Active development |

### Planned

| Device | MCU | Target |
|--------|-----|--------|
| LilyGo T-Deck Pro | ESP32-S3 | Q2 2026 |
| SimpleGo Secure (Tier 2) | STM32U585 | Custom PCB design phase |
| SimpleGo Vault (Tier 3) | STM32U5A9 | Custom PCB planning |

Adding support for new hardware requires implementing the HAL interfaces. See `devices/template/` for a reference implementation.

## Kickstarter

SimpleGo will launch a Kickstarter campaign once the Tier 1 prototype reaches feature completeness. The campaign will offer three backer tiers corresponding to the hardware security tiers.

### Backer Rewards

**Tier 1 — DIY Device** — A fully assembled and tested T-Deck Plus, pre-flashed with SimpleGo firmware, ready to use. Fully open source, so you can build your own or flash updates anytime.

**Tier 2 — Secure Device** — A SimpleGo Secure with custom PCB, dual secure elements, CNC aluminum enclosure, and a lifetime premium features license.

**Tier 3 — Vault Device** — A SimpleGo Vault with triple secure elements, active tamper detection, potted enclosure, hand-assembled in Germany with individual serial numbers, and a lifetime premium features license.

### Campaign Goals

The funding will cover custom PCB design and manufacturing for Tier 2 and Tier 3 devices, establishing the supply chain for secure element components from three independent vendors, commissioning an independent security audit of the protocol implementation, building initial production inventory, and completing German UG company formation and regulatory compliance.

The Kickstarter model fits what SimpleGo is about: direct community funding, full transparency about development progress and costs, and backers who care about privacy as a fundamental right rather than a premium feature.

Follow this repository to be notified when the campaign launches.

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
# or
%IDF_PATH%\export.bat                   # Windows

# Configure WiFi and device settings
idf.py menuconfig

# Build, flash, and monitor
idf.py build flash monitor -p /dev/ttyUSB0    # Linux
idf.py build flash monitor -p COM6            # Windows
```

All settings including WiFi credentials, SMP server, UI theme, and security options are managed through menuconfig. The `sdkconfig` file is excluded from version control via `.gitignore`.

## Security

### Disclosure Policy

Security vulnerabilities should be reported privately. Do not open public issues for security concerns. Contact details for responsible disclosure will be published with the v1.0 release.

### Audit Status

The codebase has not yet undergone formal security audit. An independent audit is planned as a Kickstarter stretch goal and a prerequisite for the v1.0 release. Users should consider this when evaluating the software for sensitive applications.

### What SimpleGo does not protect against

We want to be upfront about the limits:

- Compromise of SMP relay servers (mitigated by the protocol's metadata protection, but not eliminated)
- Physical attacks on Tier 1 devices (Tier 2 and 3 provide physical security)
- Rubber-hose cryptanalysis
- Network-level traffic analysis (mitigation via Private Message Routing is planned)

## License

| Component | License |
|-----------|---------|
| Software | [AGPL-3.0](LICENSE) |
| Hardware designs | CERN-OHL-W-2.0 |

AGPL-3.0 ensures the open-source core remains open, including any network-facing modifications. Anyone running a modified version of SimpleGo as a service must also publish their source code. Tier 1 firmware is fully open, while proprietary additions for Tier 2 and 3 devices (secure element drivers, tamper detection firmware, premium features) are developed separately under a commercial license. Hardware designs are published under the CERN Open Hardware License to enable community manufacturing and modification.

## Legal

- [Disclaimer](docs/DISCLAIMER.md) — Warranty and liability
- [Legal Notice](docs/LEGAL.md) — Impressum and regulatory information
- [Trademark Notice](docs/TRADEMARK.md) — SimpleX name usage

## Acknowledgments

- [SimpleX Chat](https://simplex.chat/) — SimpleX Messaging Protocol
- [Espressif](https://www.espressif.com/) — ESP-IDF and ESP32 platform
- [LVGL](https://lvgl.io/) — Embedded graphics library
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) — TLS and cryptography

*SimpleGo is an independent project. It is not affiliated with, endorsed by, or connected to SimpleX Chat Ltd. The SimpleX name and protocol are used for interoperability purposes only. This software is provided as-is, without warranty. See [docs/DISCLAIMER.md](docs/DISCLAIMER.md) for full legal notices.*

<p align="center">
  <br>
  <b>SimpleGo — Secure messaging without smartphones.</b>
</p>