<p align="center">
  <img src=".github/assets/simplego_banner.png" alt="SimpleGo" width="1500" height="230">
</p>

# SimpleGo

**Encrypted communication and IoT on dedicated hardware. No smartphone, no cloud, no compromises.**

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Hardware: CERN-OHL-W-2.0](https://img.shields.io/badge/Hardware-CERN--OHL--W--2.0-green.svg)](#license)
[![Version](https://img.shields.io/badge/version-0.1.18--alpha-orange.svg)](#status)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-lightgrey.svg)](#getting-started)
[![Docs](https://img.shields.io/badge/docs-wiki.simplego.dev-blue.svg)](https://wiki.simplego.dev)

## What is SimpleGo?

SimpleGo is an open-source platform for encrypted communication and secure data transmission on dedicated microcontroller hardware. It combines private messaging with IoT sensor monitoring and remote device control in a single, auditable firmware stack.

The core idea is simple: sensitive data belongs on hardware you control, transmitted through channels nobody else can read. Whether that data is a text message, a temperature reading from a medical sensor, or a command to a remote access system.

SimpleGo runs on affordable off-the-shelf hardware, operates its own relay infrastructure, and is built entirely in C with a codebase small enough to audit.

## Use Cases

### Private Messaging

Turn on the device, connect to WiFi, scan a QR code, and start chatting. Messages are end-to-end encrypted with perfect forward secrecy. There are no accounts, no phone numbers, and no usernames. Your keys never leave your device.

### Medical and Health Monitoring

Transmit patient data, sensor readings from wearable monitors, or equipment status in compliance with data protection regulations. End-to-end encryption ensures that sensitive health information cannot be intercepted or correlated to an individual during transmission.

### Industrial Sensor Networks

Collect and transmit data from environmental sensors, production equipment, or infrastructure monitoring systems. Encrypted channels prevent data manipulation and protect operational intelligence from interception. Relevant for water treatment, energy infrastructure, manufacturing, and any environment where SCADA security matters.

### Agriculture and Environmental Monitoring

Transmit soil moisture, weather station data, and irrigation commands across remote locations. Encrypted communication protects operational data from competitors and prevents unauthorized access to automated systems.

### Building Security and Access Control

Manage door locks, alarm systems, and surveillance infrastructure through encrypted channels. Unencrypted building automation is an open invitation. SimpleGo ensures that commands and sensor states cannot be intercepted or spoofed.

### Emergency and Disaster Communication

When cellular networks fail, SimpleGo devices can communicate over WiFi and local networks without depending on centralized infrastructure. Planned LoRa support will extend this to long-range off-grid scenarios.

### Fleet and Asset Tracking

Monitor location, temperature, and status of sensitive shipments. End-to-end encryption prevents logistics providers or third parties from building movement profiles or accessing cargo information.

### Journalism and Human Rights

Deploy environmental sensors or communication relays in regions where monitoring is dangerous. Anonymized data transmission protects both the source and the operator.

## Features

### Communication
- End-to-end encrypted messaging with perfect forward secrecy
- No accounts, no phone numbers, no persistent identity
- 128 simultaneous contacts with individual encryption keys
- Encrypted chat history on SD card (AES-256-GCM)
- Delivery receipts (double checkmarks)
- Compatible with existing SMP-based messaging apps
- Independent relay servers operated by SimpleGo

### Hardware
- Physical QWERTY keyboard and 320x240 color display
- WiFi manager with multi-network support and WPA3
- Hardware Abstraction Layer for easy porting to new devices
- Runs on affordable off-the-shelf microcontroller boards ($50-70)

### IoT and Sensors (in development)
- Encrypted sensor data transmission
- Remote device monitoring and control
- Configurable data channels with per-sensor encryption keys
- LoRa support for long-range off-grid deployments (planned)

### Security
- Multiple independent encryption layers per message
- All traffic padded to fixed block size (no metadata leakage)
- Keys stored on-device with hardware-backed encryption (production release)
- Bare-metal firmware with no OS, no browser, no background services
- 22,000 lines of C across 47 source files (fully auditable)
- Open source under AGPL-3.0

## Getting Started

SimpleGo runs on the [LilyGo T-Deck Plus](https://www.lilygo.cc/products/t-deck-plus), available for around $50-70.

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) 5.5.2 or later
- Python 3.8+
- CMake 3.16+

### Build and Flash

```bash
git clone https://github.com/saschadaemgen/SimpleGo.git
cd SimpleGo

# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh          # Linux/macOS
%IDF_PATH%\export.bat                   # Windows

# Configure
idf.py menuconfig

# Build, flash, and run
idf.py build flash monitor -p /dev/ttyUSB0    # Linux
idf.py build flash monitor -p COM6            # Windows
```

All settings including WiFi credentials, server configuration, and security options are managed through menuconfig.

## Hardware

SimpleGo is built around a Hardware Abstraction Layer (HAL). The entire protocol stack and application logic are device-independent. Adding a new hardware platform means implementing five interface files. Everything above the HAL comes for free.

### Current Platform

| | |
|---|---|
| **Device** | LilyGo T-Deck Plus |
| **MCU** | ESP32-S3, dual-core 240 MHz, 8 MB PSRAM |
| **Display** | 320x240 LCD with touch |
| **Input** | Physical QWERTY keyboard, trackball |
| **Connectivity** | WiFi 802.11 b/g/n |
| **Storage** | MicroSD for encrypted data storage |

### Planned Platforms

Custom PCB designs with hardware secure elements, LoRa connectivity, and optional LTE are in development for professional and industrial deployments. See [Hardware Documentation](https://wiki.simplego.dev/hardware) for details.

## Architecture

```
+---------------------------------------------------------------+
|                     APPLICATION LAYER                         |
|          Messaging / IoT Sensors / Remote Control             |
+---------------------------------------------------------------+
|                      PROTOCOL LAYER                           |
|         Encryption / Key Management / Data Channels           |
+---------------------------------------------------------------+
|               HARDWARE ABSTRACTION LAYER                      |
|       hal_display / hal_input / hal_network / hal_storage     |
+---------------+---------------+---------------+---------------+
|  T-Deck Plus  |  T-Deck Pro   |  Custom PCB   |   Desktop     |
|  ESP32-S3     |  ESP32-S3     |  STM32 + SE   |   SDL2 Test   |
+---------------+---------------+---------------+---------------+
```

## Project Structure

```
SimpleGo/
+-- main/
|   +-- core/           # Protocol implementation
|   +-- crypto/         # Cryptographic operations
|   +-- hal/            # HAL interface headers
|   +-- net/            # Network and TLS transport
|   +-- protocol/       # Protocol encoding/decoding
|   +-- state/          # Persistent state management
|   +-- ui/             # User interface
|   +-- util/           # Shared utilities
+-- devices/
|   +-- t_deck_plus/    # LilyGo T-Deck Plus HAL
|   +-- template/       # Template for new device ports
+-- components/         # External libraries
+-- docs/               # Documentation
```

## Status

This is alpha software under active development. The core messaging stack is functional and tested with multiple simultaneous contacts. IoT and sensor functionality is in the design phase.

| Component | Status |
|-----------|--------|
| Encrypted messaging | Working |
| Multi-contact (128) | Working |
| Delivery receipts | Working |
| WiFi manager | Working |
| Encrypted data storage | Working |
| Persistent crypto state | Working |
| IoT sensor channels | Design phase |
| Remote device control | Design phase |
| LoRa connectivity | Planned |
| Post-quantum key exchange | Planned |

## Documentation

- [wiki.simplego.dev](https://wiki.simplego.dev) - Full documentation
- [Architecture](https://wiki.simplego.dev/architecture) - System design
- [Security](https://wiki.simplego.dev/security) - Security model
- [Hardware](https://wiki.simplego.dev/hardware) - Device specifications and porting
- [Protocol Analysis](docs/protocol-analysis/) - Implementation journal

## Contributing

See [DEVELOPMENT.md](docs/DEVELOPMENT.md) for build instructions and [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

Security vulnerabilities should be reported privately via GitHub's vulnerability reporting feature.

## License

| Component | License |
|-----------|---------|
| Software | [AGPL-3.0](LICENSE) |
| Hardware | CERN-OHL-W-2.0 |

## Acknowledgments

- [Espressif](https://www.espressif.com/) for ESP-IDF and the ESP32 platform
- [LVGL](https://lvgl.io/) for the embedded graphics library
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) for TLS and cryptography
- [wolfSSL](https://www.wolfssl.com/) for X448 key agreement
- [libsodium](https://doc.libsodium.org/) for NaCl cryptographic operations

---

*SimpleGo is an independent open-source project by IT and More Systems, Recklinghausen, Germany. SimpleGo uses the open-source SimpleX Messaging Protocol (AGPL-3.0) for interoperable message delivery. It is not affiliated with or endorsed by any third party. See [docs/DISCLAIMER.md](docs/DISCLAIMER.md) for full legal notices.*

<p align="center">
  <b>SimpleGo - Encrypted communication and IoT on dedicated hardware.</b>
</p>