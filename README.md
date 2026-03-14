<p align="center">
  <img src=".github/assets/simplego_banner.png" alt="SimpleGo" width="1500" height="230">
</p>

<h1 align="center">SimpleGo</h1>

<p align="center">
  <strong>The world's first native C implementation of the SimpleX Messaging Protocol.</strong><br>
  Encrypted communication and IoT on dedicated hardware. No smartphone, no cloud, no compromises.
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-AGPL--3.0-blue.svg" alt="License"></a>
  <a href="#license"><img src="https://img.shields.io/badge/Hardware-CERN--OHL--W--2.0-green.svg" alt="Hardware License"></a>
  <a href="#status"><img src="https://img.shields.io/badge/version-0.1.17--alpha-orange.svg" alt="Version"></a>
  <a href="#getting-started"><img src="https://img.shields.io/badge/platform-ESP32--S3-lightgrey.svg" alt="Platform"></a>
  <a href="https://wiki.simplego.dev"><img src="https://img.shields.io/badge/docs-wiki.simplego.dev-blue.svg" alt="Documentation"></a>
</p>

---

SimpleGo is an open-source platform for encrypted communication and secure data transmission on dedicated microcontroller hardware. It combines private messaging with IoT sensor monitoring and remote device control in a single, auditable firmware stack.

Sensitive data belongs on hardware you control, transmitted through channels nobody else can read. Whether that data is a text message, a temperature reading from a medical sensor, or a command to a remote access system.

Built entirely in C. Runs on affordable off-the-shelf hardware. Operates its own relay infrastructure. Small enough to audit.

---

## Encryption Stack

Every message passes through four independent cryptographic layers before it leaves the device.

| Layer | Algorithm | What it protects against |
|:------|:----------|:------------------------|
| **End-to-End** | X3DH (X448) + Double Ratchet + AES-256-GCM | Interception. Every message has its own key. Perfect forward secrecy + post-compromise security. |
| **Per-Queue** | X25519 + XSalsa20 + Poly1305 | Traffic correlation between message queues. Knowledge of Queue A reveals nothing about Queue B. |
| **Server-to-Recipient** | NaCl cryptobox (X25519) | Correlation of incoming and outgoing server traffic, even with full server access. |
| **Transport** | TLS 1.3 (mbedTLS) | Network-level attackers. No downgrade possible. |

All messages are padded to a fixed 16 KB block size at every layer. A network observer sees only equal-sized packets.

Post-quantum key exchange using **sntrup761** (Streamlined NTRU Prime) is integrated and active, providing quantum-resistant encryption from the first message.

---

## Use Cases

**Private Messaging** - Turn on the device, connect to WiFi, scan a QR code, start chatting. No accounts, no phone numbers, no usernames. Your keys never leave your device.

**Medical and Health Monitoring** - Transmit patient data and sensor readings in compliance with data protection regulations. End-to-end encryption ensures sensitive health information cannot be intercepted during transmission.

**Industrial Sensor Networks** - Collect and transmit data from environmental sensors, production equipment, or infrastructure monitoring. Encrypted channels prevent data manipulation and protect operational intelligence. Relevant for water treatment, energy infrastructure, manufacturing, and SCADA environments.

**Building Security and Access Control** - Manage door locks, alarm systems, and surveillance through encrypted channels. Commands and sensor states cannot be intercepted or spoofed.

**Emergency Communication** - Communicate over WiFi and local networks without centralized infrastructure. Planned LoRa support will extend this to long-range off-grid scenarios.

**Agriculture and Environmental Monitoring** - Transmit soil moisture, weather data, and irrigation commands across remote locations. Protect operational data from competitors and unauthorized access.

**Fleet and Asset Tracking** - Monitor location, temperature, and status of sensitive shipments. Prevent third parties from building movement profiles.

**Journalism and Human Rights** - Deploy sensors or communication relays in regions where monitoring is dangerous. Anonymized data transmission protects both source and operator.

---

## Getting Started

### What you need

| Item | Details |
|:-----|:--------|
| **LilyGo T-Deck Plus** | Available for $50-70 from [lilygo.cc](https://www.lilygo.cc/products/t-deck-plus) or AliExpress |
| **MicroSD card** | Any size, formatted as **FAT32**. Required for encrypted chat history storage. |
| **USB-C cable** | For flashing and serial monitoring |
| **ESP-IDF 5.5.2** | Espressif IoT Development Framework ([download](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/)) |

---

### Installation on Windows

**1. Install ESP-IDF**

Download and run the [ESP-IDF Offline Installer](https://dl.espressif.com/dl/esp-idf/) for version 5.5.2. After installation, open **"ESP-IDF 5.5 PowerShell"** from the Start menu. All following commands are entered there.

**2. Clone the repository**

```powershell
cd C:\Espressif\projects
git clone https://github.com/saschadaemgen/SimpleGo.git
cd SimpleGo
```

**3. Apply mbedTLS patches**

SimpleX relay servers use ED25519 certificates which ESP-IDF's mbedTLS does not support natively. These patches are required for the TLS connection to work. See [patches/README.md](patches/README.md) for details.

```powershell
.\patches\apply_patches.ps1
```

**4. Build**

```powershell
idf.py build
```

**5. Flash**

Connect the T-Deck Plus via USB-C. Check which COM port it uses in the Device Manager.

```powershell
idf.py flash monitor -p COM6
```

Replace `COM6` with your actual port.

**6. First boot**

The device shows a WiFi setup screen. Select your network and enter the password using the keyboard. After connecting, the main screen appears. Insert a FAT32-formatted MicroSD card for encrypted message storage.

---

### Installation on Linux

**1. Install ESP-IDF**

```bash
sudo apt update && sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
source export.sh
```

Note: You need to run `source ~/esp/esp-idf/export.sh` in every new terminal session.

**2. Clone the repository**

```bash
cd ~
git clone https://github.com/saschadaemgen/SimpleGo.git
cd SimpleGo
```

**3. Apply mbedTLS patches**

SimpleX relay servers use ED25519 certificates which ESP-IDF's mbedTLS does not support natively. These patches are required for the TLS connection to work. See [patches/README.md](patches/README.md) for details.

```bash
chmod +x patches/apply_patches.sh
./patches/apply_patches.sh
```

**4. Build**

```bash
idf.py build
```

**5. Set serial port permissions**

```bash
sudo usermod -a -G dialout $USER
```

Log out and log back in for this to take effect.

**6. Flash**

Connect the T-Deck Plus via USB-C. Find the port:

```bash
ls /dev/ttyACM* /dev/ttyUSB*
```

Flash and monitor:

```bash
idf.py flash monitor -p /dev/ttyACM0
```

Replace `/dev/ttyACM0` with your actual port.

**7. First boot**

The device shows a WiFi setup screen. Select your network and enter the password using the keyboard. After connecting, the main screen appears. Insert a FAT32-formatted MicroSD card for encrypted message storage.

---

## Security Modes

SimpleGo supports three security configurations using ESP32-S3 hardware security features. The base configuration (`sdkconfig.defaults`) already includes NVS encryption and eFuse auto-provisioning. The Open and Vault configs are overlays that modify these settings.

| Mode | What it does |
|:-----|:-------------|
| **Default** | Full build with NVS encryption and eFuse auto-provisioning enabled. Production-ready security out of the box. |
| **Open** | Disables NVS encryption and eFuse auto-provisioning. For development and debugging where you need unlimited reflash and NVS access. |
| **Vault** | NVS encryption with HMAC-based eFuse key protection (BLOCK_KEY1). The strongest hardware-backed configuration. |

**Windows:**
```powershell
idf.py build                                                                                    # Default
idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.open" build                 # Open
idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.vault" build                # Vault
```

**Linux:**
```bash
idf.py build                                                                                    # Default
idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.open" build                 # Open
idf.py -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.vault" build                # Vault
```

| Feature | Default | Open | Vault |
|:--------|:-------:|:----:|:-----:|
| Encrypted messaging (4 layers) | Yes | Yes | Yes |
| SD card encryption (AES-256-GCM) | Yes | Yes | Yes |
| Post-quantum key exchange | Yes | Yes | Yes |
| NVS encryption | Yes | No | Yes (HMAC) |
| eFuse auto-provisioning | Yes | No | Yes |
| Reflash | Unlimited | Unlimited | Limited |
| Estimated physical attack cost | ~$15 | ~$15 | ~$30,000 |

> **Warning:** Vault mode permanently burns eFuse fuses on the ESP32-S3. This is irreversible. A wrong configuration will brick the device. Read the full documentation at [wiki.simplego.dev/security](https://wiki.simplego.dev/security) before using Vault mode.

---

## Hardware

SimpleGo is built around a Hardware Abstraction Layer. The entire protocol stack and application logic are device-independent. Adding a new platform means implementing five interface files. Everything above the HAL comes for free.

**Current platform:**

| | |
|:--|:--|
| Device | LilyGo T-Deck Plus |
| MCU | ESP32-S3, dual-core 240 MHz, 8 MB PSRAM |
| Display | 320x240 LCD with touch |
| Input | Physical QWERTY keyboard, trackball |
| Connectivity | WiFi 802.11 b/g/n, WPA3 |
| Storage | MicroSD (AES-256-GCM encrypted) |

Custom PCB designs with triple-vendor hardware secure elements (Microchip ATECC608B + Infineon OPTIGA Trust M + NXP SE050), LoRa connectivity, physical kill switches, and optional LTE are in development for professional and industrial deployments.

---

## Architecture

```
+---------------------------------------------------------------+
|                     APPLICATION LAYER                         |
|          Messaging  /  IoT Sensors  /  Remote Control         |
+---------------------------------------------------------------+
|                      PROTOCOL LAYER                           |
|     4-Layer Encryption  /  Key Management  /  Data Channels   |
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
|   +-- core/           # FreeRTOS task architecture, frame pool
|   +-- crypto/         # X448, AES-256-GCM, NaCl, sntrup761 PQ-KEM
|   +-- include/        # Shared header files
|   +-- net/            # TLS 1.3 transport, WiFi manager
|   +-- protocol/       # SMP protocol, Double Ratchet, handshake
|   +-- state/          # Contact management, history, peer connections
|   +-- ui/             # LVGL screens, themes, custom fonts
|   +-- util/           # Shared utilities
+-- devices/
|   +-- t_deck_plus/    # LilyGo T-Deck Plus HAL implementation
|   +-- template/       # Template for new device ports
+-- components/         # sntrup761, zstd, wolfssl_config
+-- patches/            # mbedTLS ED25519 compatibility patches
+-- docs/               # Protocol analysis, architecture documentation
+-- wiki/               # Docusaurus wiki source (wiki.simplego.dev)
```

---

## Status

Alpha software under active development. Core messaging is functional and tested with multiple simultaneous contacts across devices.

| Component | Status |
|:----------|:-------|
| Encrypted messaging (4-layer) | Working |
| Multi-contact management (128 slots) | Working |
| Post-quantum key exchange (sntrup761) | Working |
| Double Ratchet with X448 | Working |
| Delivery receipts | Working |
| WiFi manager (multi-network, WPA3) | Working |
| Encrypted SD card storage (AES-256-GCM) | Working |
| Screen lock (60s inactivity) | Working |
| Cross-platform build (Windows + Linux) | Working |
| IoT sensor channels | Design phase |
| Remote device control | Design phase |
| LoRa connectivity | Planned |
| Web Serial Installer | Planned |
| Desktop terminal (10" touchscreen) | Planned |

---

## Documentation

| Resource | Link |
|:---------|:-----|
| Full documentation | [wiki.simplego.dev](https://wiki.simplego.dev) |
| Architecture and security model | [wiki.simplego.dev/architecture](https://wiki.simplego.dev/architecture) |
| Hardware specifications | [wiki.simplego.dev/hardware](https://wiki.simplego.dev/hardware) |
| mbedTLS patch documentation | [patches/README.md](patches/README.md) |
| Coding rules | [CODING_RULES.md](CODING_RULES.md) |
| Contributing guidelines | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Protocol analysis journal | [docs/protocol-analysis/](docs/protocol-analysis/) |

Security vulnerabilities should be reported privately via GitHub's [vulnerability reporting](https://github.com/saschadaemgen/SimpleGo/security) feature.

---

## License

| Component | License |
|:----------|:--------|
| Software | [AGPL-3.0](LICENSE) |
| Hardware designs | CERN-OHL-W-2.0 |

## Acknowledgments

[Espressif](https://www.espressif.com/) (ESP-IDF and ESP32 platform) - [LVGL](https://lvgl.io/) (embedded graphics) - [mbedTLS](https://github.com/Mbed-TLS/mbedtls) (TLS and cryptography) - [wolfSSL](https://www.wolfssl.com/) (X448 key agreement) - [libsodium](https://doc.libsodium.org/) (NaCl cryptographic operations) - [PQClean](https://github.com/PQClean/PQClean) (sntrup761 post-quantum cryptography)

---

<p align="center">
  <i>SimpleGo is an independent open-source project by IT and More Systems, Recklinghausen, Germany.</i><br>
  <i>SimpleGo uses the open-source SimpleX Messaging Protocol (AGPL-3.0) for interoperable message delivery.</i><br>
  <i>It is not affiliated with or endorsed by any third party. See <a href="docs/DISCLAIMER.md">docs/DISCLAIMER.md</a> for full legal notices.</i>
</p>

<p align="center">
  <strong>SimpleGo - Encrypted communication and IoT on dedicated hardware.</strong>
</p>
