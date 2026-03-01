# Security Model

This document describes SimpleGo's security architecture across three hardware tiers, from development platforms to high-security devices.

---

## Table of Contents

1. [The Smartphone Problem](#the-smartphone-problem)
2. [The Dedicated Hardware Approach](#the-dedicated-hardware-approach)
3. [Three-Tier Security Architecture](#three-tier-security-architecture)
4. [Tier 1: Development Platforms](#tier-1-development-platforms)
5. [Tier 2: SimpleGo Secure](#tier-2-simplego-secure)
6. [Tier 3: SimpleGo Vault](#tier-3-simplego-vault)
7. [Cryptographic Architecture](#cryptographic-architecture)
8. [Key Storage Hierarchy](#key-storage-hierarchy)
9. [Secure Boot Chain](#secure-boot-chain)
10. [Tamper Detection and Response](#tamper-detection-and-response)
11. [Threat Model](#threat-model)
12. [Comparison with Alternatives](#comparison-with-alternatives)
13. [Operational Security](#operational-security)
14. [Security Roadmap](#security-roadmap)

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

Every smartphone contains a baseband processor - a separate computer running closed-source firmware that handles cellular communication. This processor:

- Runs proprietary firmware that cannot be audited
- Has direct memory access (DMA) on many devices
- Remains active whenever the device has cellular signal
- Contains known vulnerabilities (Qualcomm, MediaTek, Samsung)
- Cannot be disabled without losing cellular functionality
- Operates independently of the main operating system

Even hardened mobile operating systems like GrapheneOS, while significantly improving security posture, cannot address this architectural limitation. GrapheneOS provides excellent sandboxing, verified boot, and hardened memory allocation - but it still runs on hardware with a closed-source baseband processor.

---

## The Dedicated Hardware Approach

SimpleGo takes a fundamentally different approach: minimize the trusted computing base (TCB).

### Attack Surface Comparison

```
Smartphone (Android/iOS):             SimpleGo (ESP32/STM32):
├── Operating System (~20M lines)     ├── FreeRTOS Kernel (~10K lines)
├── System Services (~10M lines)      ├── Network Stack (~20K lines)
├── Browser Engine (~5M lines)        ├── Crypto Libraries (~15K lines)
├── JavaScript Runtime                └── SimpleGo Application (~5K lines)
├── App Framework                         Total: ~50K lines
├── Hundreds of Apps
├── Google/Apple Services                 Reduction: 1000x smaller
├── Baseband Processor (closed)
├── Bluetooth Stack
├── NFC Stack
└── Telemetry Services
    Total: ~50M lines
```

### What This Eliminates

| Threat Vector | Smartphone | SimpleGo |
|---------------|------------|----------|
| Remote Code Execution | Large attack surface | Minimal attack surface |
| Supply Chain Attacks | Hundreds of dependencies | Few, auditable dependencies |
| Malicious Applications | Always possible | No app installation |
| Browser Exploits | Major risk vector | No browser |
| JavaScript Attacks | Ubiquitous | No JavaScript engine |
| Telemetry and Tracking | Built into OS | None |
| Forced Updates | Vendor-controlled | User-controlled |
| Baseband Attacks | Always possible | No baseband processor |

---

## Three-Tier Security Architecture

SimpleGo implements a tiered security model, allowing users to select hardware appropriate to their threat model.

```
+------------------------------------------------------------------+
|                    SECURITY TIER OVERVIEW                        |
+------------------------------------------------------------------+
|                                                                  |
|  TIER 1: Development         TIER 2: Secure       TIER 3: Vault |
|  ~~~~~~~~~~~~~~~~~~~~        ~~~~~~~~~~~~~~       ~~~~~~~~~~~~~ |
|                                                                  |
|  Target: Developers,         Target: Privacy      Target: High  |
|  enthusiasts, DIY            conscious users      risk users    |
|                                                                  |
|  Hardware: LilyGo            Hardware: Custom     Hardware:     |
|  T-Deck Plus/Pro,            PCB with dual        Custom PCB    |
|  T-Lora Pager                Secure Elements      triple SE     |
|                                                                  |
|  Security: Software          Security: Hardware   Security:     |
|  encryption, ESP32           key storage,         Full tamper   |
|  secure boot                 TrustZone, basic     detection,    |
|                              tamper detection     zeroization   |
|                                                                  |
|  Price: ~EUR 50-100          Price: ~EUR 400-600  Price: ~EUR   |
|                                                   1000-1500     |
+------------------------------------------------------------------+
```

### Tier Comparison Matrix

| Feature | Tier 1 | Tier 2 | Tier 3 |
|---------|--------|--------|--------|
| **MCU** | ESP32-S3 | STM32U585 | STM32U5A9 |
| **TrustZone** | No | Yes | Yes |
| **Secure Elements** | None | 2 (ATECC608B + OPTIGA) | 3 (+ NXP SE050) |
| **Secure Boot** | ESP32 | STM32 + SE verification | Multi-stage with SE |
| **Flash Encryption** | AES-256-XTS | AES-256-XTS | AES-256-XTS + SE |
| **Key Storage** | eFuse (software) | Secure Element | Triple SE redundancy |
| **Tamper Detection** | None | Light sensor, mesh | Full environmental |
| **Tamper Response** | None | Key zeroization | Immediate wipe |
| **JTAG Protection** | Disable via eFuse | Hardware disabled | Physically removed |
| **Enclosure** | Off-shelf or 3D print | CNC aluminum | Potted aluminum |
| **Cellular** | None | LTE Cat-M (isolated) | 4G/5G (isolated) |
| **Target Users** | Developers, hobbyists | Journalists, activists | High-risk individuals |
| **Estimated Price** | EUR 50-100 | EUR 400-600 | EUR 1000-1500 |

---

## Tier 1: Development Platforms

Tier 1 uses off-the-shelf development boards, providing good security through software measures and ESP32 hardware features.

### Supported Devices

| Device | Display | Input | Notes |
|--------|---------|-------|-------|
| LilyGo T-Deck Plus | 320x240 LCD | Keyboard, trackball, touch | Primary development platform |
| LilyGo T-Deck Pro | 320x240 LCD | Keyboard, trackball, touch | Enhanced T-Deck |
| LilyGo T-Lora Pager | 128x64 OLED | Encoder, buttons | Compact with LoRa |

### ESP32-S3 Security Features

#### Secure Boot v2

```
Boot Chain:
┌─────────────────────────────────────────────────────────────┐
│  1. ROM Bootloader (immutable, in silicon)                  │
│     │                                                       │
│     ▼                                                       │
│  2. Second-stage bootloader (signature verified)            │
│     │  - RSA-3072 or ECDSA-256 signature                   │
│     │  - Public key hash in eFuse                          │
│     ▼                                                       │
│  3. Application (signature verified)                        │
│     │  - Same key or separate app signing key              │
│     ▼                                                       │
│  4. Execute only if all signatures valid                    │
└─────────────────────────────────────────────────────────────┘
```

#### Flash Encryption

| Property | Value |
|----------|-------|
| Algorithm | AES-256-XTS |
| Key Storage | eFuse (hardware, not software readable) |
| Scope | All flash partitions (configurable) |
| Performance | Hardware accelerated, minimal overhead |

#### eFuse Security Options

| eFuse | Purpose | Recommendation |
|-------|---------|----------------|
| SECURE_BOOT_EN | Enable secure boot | Enable for production |
| FLASH_CRYPT_CNT | Flash encryption state | Set for production |
| JTAG_DISABLE | Disable JTAG debugging | Enable for production |
| UART_DOWNLOAD_DIS | Disable UART bootloader | Consider for production |
| DIS_USB_JTAG | Disable USB-JTAG | Enable for production |

#### Hardware Crypto Acceleration

| Algorithm | Hardware Support |
|-----------|-----------------|
| AES | Yes (128/256-bit) |
| SHA | Yes (SHA-1/224/256/384/512) |
| RSA | Yes (up to 4096-bit) |
| ECC | Yes (ECDSA, ECDH) |
| RNG | Hardware TRNG |

### Tier 1 Limitations

| Limitation | Impact | Mitigation |
|------------|--------|------------|
| No Secure Element | Keys in eFuse, not ideal | Enable flash encryption |
| No TrustZone | Single security domain | Careful code separation |
| No tamper detection | Physical attacks possible | Physical security practices |
| eFuse key extraction | Possible with equipment | Consider disposable |

---

## Tier 2: SimpleGo Secure

Tier 2 introduces custom hardware with dedicated Secure Elements and ARM TrustZone.

### Hardware Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     SimpleGo Secure PCB                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │   STM32U585 │    │  ATECC608B  │    │ OPTIGA Trust│         │
│  │   (Main MCU)│◄──►│  (Microchip)│    │  M (Infineon│         │
│  │             │    │             │    │             │         │
│  │  TrustZone  │    │ - ECDH      │    │ - RSA/ECC   │         │
│  │  Cortex-M33 │    │ - ECDSA     │    │ - Key store │         │
│  │  160MHz     │    │ - SHA-256   │    │ - Monotonic │         │
│  │  2MB Flash  │    │ - AES-128   │    │   counter   │         │
│  │  786KB RAM  │    │ - RNG       │    │ - Shielded  │         │
│  └─────────────┘    └─────────────┘    │   connection│         │
│         │                              └─────────────┘         │
│         │                                                       │
│  ┌──────┴──────────────────────────────────────────────┐       │
│  │                   Secure Bus                         │       │
│  └──────────────────────────────────────────────────────┘       │
│         │              │              │                         │
│  ┌──────┴─────┐ ┌──────┴─────┐ ┌──────┴─────┐                  │
│  │   Display  │ │  Network   │ │   Input    │                  │
│  │  (ST7789V) │ │ (WiFi/LTE) │ │ (Keyboard) │                  │
│  └────────────┘ └────────────┘ └────────────┘                  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────┐       │
│  │              Tamper Detection Mesh                   │       │
│  │  - Light sensor (enclosure open detection)          │       │
│  │  - PCB trace mesh (drilling detection)              │       │
│  │  - Voltage monitoring (glitch detection)            │       │
│  └─────────────────────────────────────────────────────┘       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### STM32U585 Security Features

#### TrustZone

ARM TrustZone creates hardware-enforced separation between secure and non-secure worlds.

```
┌─────────────────────────────────────────────────────────────┐
│                    Cortex-M33 with TrustZone                │
├────────────────────────────┬────────────────────────────────┤
│      Secure World          │       Non-Secure World         │
│                            │                                │
│  - Cryptographic keys      │  - User interface              │
│  - Key derivation          │  - Network stack               │
│  - Signature verification  │  - Message display             │
│  - Secure storage access   │  - General application         │
│  - SE communication        │                                │
│                            │                                │
│  Isolated memory regions   │  Cannot access secure memory   │
│  Secure peripherals only   │  Limited peripheral access     │
└────────────────────────────┴────────────────────────────────┘
```

#### Secure Storage (OTFDEC)

| Feature | Description |
|---------|-------------|
| On-the-fly decryption | Hardware decryption during code execution |
| Region-based | Up to 4 encrypted regions |
| Key protection | Keys in secure world only |
| Performance | No execution speed penalty |

### Dual Secure Element Strategy

Using two Secure Elements from different vendors provides:

| Benefit | Explanation |
|---------|-------------|
| Vendor diversity | Single vendor compromise doesn't expose all keys |
| Algorithm diversity | Different implementations, different potential bugs |
| Key splitting | Critical keys can be split across both SEs |
| Redundancy | Failure of one SE doesn't brick device |

#### ATECC608B (Microchip)

| Feature | Capability |
|---------|------------|
| Key Storage | 16 key slots |
| Algorithms | ECDH (P-256), ECDSA, SHA-256, AES-128 |
| RNG | NIST SP 800-90A/B/C compliant |
| Monotonic Counter | Yes (for anti-replay) |
| Secure Boot | Hardware-based verification |

#### OPTIGA Trust M (Infineon)

| Feature | Capability |
|---------|------------|
| Key Storage | Multiple key objects |
| Algorithms | RSA (1024-2048), ECC (P-256/384), SHA-256 |
| Shielded Connection | Encrypted communication with host |
| Lifecycle Management | Locked production state |
| Common Criteria | EAL6+ certified |

### Tamper Detection (Tier 2)

| Sensor | Detects | Response |
|--------|---------|----------|
| Light sensor | Enclosure opening | Alert, optional key wipe |
| PCB mesh | Drilling, probing | Key zeroization |
| Voltage monitor | Glitch attacks | Reset, log event |
| Temperature | Extreme conditions | Suspend operation |

### Isolated Cellular Connectivity

Tier 2 devices can include LTE Cat-M connectivity that is fundamentally different from smartphone basebands:

| Aspect | Smartphone Baseband | SimpleGo LTE Module |
|--------|--------------------|--------------------|
| Memory Access | Direct DMA to main memory | Serial interface only (UART/SPI) |
| Firmware | Closed, cannot audit | Module firmware isolated |
| Control | Runs independently | MCU controls power and data |
| Data Path | Shares memory with apps | Data passes through MCU |
| Disable | Cannot fully disable | Hardware power control |

```
┌─────────────────────────────────────────────────────────────┐
│  Smartphone Baseband (Dangerous)                            │
│                                                             │
│  ┌─────────┐     DMA      ┌─────────────────────────────┐  │
│  │Baseband │◄────────────►│  Main CPU + Memory          │  │
│  │Processor│              │  (Full access to everything)│  │
│  └─────────┘              └─────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  SimpleGo Isolated LTE (Secure)                             │
│                                                             │
│  ┌─────────┐   UART/AT    ┌─────────────────────────────┐  │
│  │  LTE    │◄────────────►│  STM32 MCU                  │  │
│  │ Module  │   Commands   │  (Controls all data flow)   │  │
│  └─────────┘              └─────────────────────────────┘  │
│       │                            │                        │
│    [Power]◄────────────────────────┘                        │
│    MCU can cut power to module                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Tier 3: SimpleGo Vault

Tier 3 represents maximum security for high-risk users facing sophisticated adversaries.

### Hardware Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      SimpleGo Vault PCB                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │  STM32U5A9  │  │ATECC608B │  │ OPTIGA   │  │ NXP SE050│     │
│  │             │  │(Microchip)│  │ Trust M  │  │          │     │
│  │  TrustZone  │  │          │  │(Infineon)│  │          │     │
│  │  Cortex-M33 │  │  SE #1   │  │  SE #2   │  │  SE #3   │     │
│  │  160MHz     │  │          │  │          │  │          │     │
│  │  4MB Flash  │  └──────────┘  └──────────┘  └──────────┘     │
│  │  2.5MB RAM  │       │             │             │            │
│  └─────────────┘       └─────────────┴─────────────┘            │
│         │                          │                            │
│         │              ┌───────────┴───────────┐                │
│         │              │  Tamper Supervisor    │                │
│         │              │  (Maxim DS3645)       │                │
│         │              │                       │                │
│         │              │  - Battery-backed     │                │
│         │              │  - Active mesh monitor│                │
│         │              │  - Zeroization control│                │
│         │              └───────────────────────┘                │
│         │                          │                            │
│  ┌──────┴──────────────────────────┴───────────────────────┐   │
│  │                  Active Tamper Mesh                      │   │
│  │                                                          │   │
│  │   ┌────┐  ┌────┐  ┌────┐  ┌────┐  ┌────┐  ┌────┐       │   │
│  │   │Mesh│──│Mesh│──│Mesh│──│Mesh│──│Mesh│──│Mesh│       │   │
│  │   │ 1  │  │ 2  │  │ 3  │  │ 4  │  │ 5  │  │ 6  │       │   │
│  │   └────┘  └────┘  └────┘  └────┘  └────┘  └────┘       │   │
│  │                                                          │   │
│  │   Full PCB coverage, break detection = instant wipe     │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                 Environmental Sensors                     │   │
│  │  - Temperature (high/low)     - Vibration                │   │
│  │  - Light (enclosure)          - X-ray detection          │   │
│  │  - Voltage (glitch)           - Magnetic field           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Triple Secure Element Architecture

| SE | Vendor | Role | Keys Stored |
|----|--------|------|-------------|
| ATECC608B | Microchip | Primary signing | Identity key (share 1) |
| OPTIGA Trust M | Infineon | Secondary operations | Identity key (share 2) |
| SE050 | NXP | Backup and verification | Identity key (share 3) |

#### Key Splitting (Shamir's Secret Sharing)

Critical keys are split across all three Secure Elements using 2-of-3 threshold scheme:

```
Identity Key Generation:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Original Key ────► Shamir Split (2-of-3 threshold)        │
│                           │                                 │
│              ┌────────────┼────────────┐                   │
│              ▼            ▼            ▼                   │
│         ┌────────┐   ┌────────┐   ┌────────┐              │
│         │Share 1 │   │Share 2 │   │Share 3 │              │
│         │ATECC608│   │OPTIGA  │   │SE050   │              │
│         └────────┘   └────────┘   └────────┘              │
│                                                             │
│  Key Recovery: Any 2 shares can reconstruct                │
│  Compromise: Single SE compromise = no key exposure        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Tamper Supervisor (Maxim DS3645)

Dedicated security IC that monitors tamper sensors independently of main MCU:

| Feature | Description |
|---------|-------------|
| Battery Backup | Continues monitoring when main power off |
| Mesh Monitoring | Detects breaks in PCB security traces |
| Temperature Monitoring | Detects freeze attacks |
| Voltage Monitoring | Detects glitch attacks |
| Zeroization | Can wipe secure memory without MCU |
| Tamper Log | Records events with timestamp |

### Environmental Sensors (Tier 3)

| Sensor | Attack Detected | Response |
|--------|-----------------|----------|
| Temperature (low) | Freeze attack (slow SRAM) | Immediate zeroization |
| Temperature (high) | Fault injection | Suspend, alert |
| Light | Enclosure breach | Zeroization |
| Voltage | Glitch injection | Reset, increment counter |
| X-ray | IC imaging attempt | Zeroization |
| Magnetic | EM fault injection | Alert, log |
| Vibration | Physical manipulation | Alert, log |
| Mesh continuity | Drilling, probing | Immediate zeroization |

### Enclosure Security

| Feature | Implementation |
|---------|---------------|
| Material | CNC machined aluminum |
| Sealing | Epoxy potting of PCB |
| Tamper Evidence | Holographic seals with serial numbers |
| Light Seal | Complete light blocking |
| RF Shielding | Optional Faraday cage |

---

## Cryptographic Architecture

All tiers use the same cryptographic protocols, differing only in key storage.

### Protocol Stack

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│                 (Messages, Contacts, UI)                    │
├─────────────────────────────────────────────────────────────┤
│                    Agent Protocol                           │
│            (Connection management, routing)                 │
├─────────────────────────────────────────────────────────────┤
│                  Double Ratchet Protocol                    │
│         (Forward secrecy, post-compromise security)         │
├─────────────────────────────────────────────────────────────┤
│                   X3DH Key Agreement                        │
│            (Initial key exchange with PFS)                  │
├─────────────────────────────────────────────────────────────┤
│                     SMP Protocol                            │
│              (Server communication, queues)                 │
├─────────────────────────────────────────────────────────────┤
│                      TLS 1.3                                │
│                 (Transport security)                        │
└─────────────────────────────────────────────────────────────┘
```

### Algorithm Selection

| Purpose | Algorithm | Parameters |
|---------|-----------|------------|
| Identity Keys | Ed25519 | 256-bit |
| Key Agreement | X448 | 448-bit |
| Ephemeral DH | X448 | 448-bit |
| Symmetric Encryption | AES-256-GCM | 256-bit key, 128-bit tag |
| Key Derivation | HKDF-SHA512 | 512-bit intermediate |
| Message Authentication | Included in GCM | 128-bit tag |
| Random Generation | Hardware TRNG | Platform-specific |

### Forward Secrecy

The Double Ratchet protocol provides forward secrecy through continuous key rotation:

```
Message 1: Key_1 ──► Encrypt ──► Send ──► Key_1 deleted
Message 2: Key_2 ──► Encrypt ──► Send ──► Key_2 deleted
Message 3: Key_3 ──► Encrypt ──► Send ──► Key_3 deleted
    ...

Compromise of current key cannot decrypt past messages.
```

### Post-Compromise Security

If a key is compromised, security is restored after the next DH ratchet step:

```
Compromise ──► Attacker has current keys
    │
    ▼
DH Ratchet ──► New DH exchange
    │
    ▼
New Keys ──► Attacker locked out
```

---

## Key Storage Hierarchy

### Tier 1 (ESP32)

```
┌─────────────────────────────────────────────────────────────┐
│  eFuse Block (One-time programmable)                        │
│  ├── Secure Boot Public Key Hash                            │
│  ├── Flash Encryption Key (hardware-only access)            │
│  └── Custom Keys (limited slots)                            │
├─────────────────────────────────────────────────────────────┤
│  Encrypted NVS Partition                                    │
│  ├── Identity Keys (encrypted at rest)                      │
│  ├── Ratchet State                                          │
│  └── Contact Keys                                           │
└─────────────────────────────────────────────────────────────┘
```

### Tier 2/3 (STM32 + Secure Elements)

```
┌─────────────────────────────────────────────────────────────┐
│  Secure Elements (Hardware protected)                       │
│  ├── Identity Key (split across SEs in Tier 3)             │
│  ├── Device Attestation Key                                 │
│  └── Recovery Key (encrypted backup)                        │
├─────────────────────────────────────────────────────────────┤
│  TrustZone Secure World (STM32)                            │
│  ├── Session Keys (temporary)                               │
│  ├── Ratchet State (current only)                          │
│  └── Key Derivation Functions                               │
├─────────────────────────────────────────────────────────────┤
│  Encrypted Flash (OTFDEC)                                   │
│  ├── Ratchet History (limited)                             │
│  ├── Contact Public Keys                                    │
│  └── Message Database                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Secure Boot Chain

### Tier 1 (ESP32)

```
┌─────────────┐
│ Power On    │
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ ROM Boot    │────►│ Immutable, burned into silicon  │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ Verify 2nd  │────►│ Check RSA/ECDSA signature       │
│ Stage Boot  │     │ against eFuse public key hash   │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ Verify App  │────►│ Check application signature     │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐
│ Execute     │
└─────────────┘
```

### Tier 2/3 (STM32 + SE)

```
┌─────────────┐
│ Power On    │
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ ROM Boot    │────►│ STM32 internal ROM              │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ Secure Boot │────►│ Verify bootloader hash          │
│ (STM32)     │     │ using internal HASH peripheral  │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ SE Verify   │────►│ ATECC608B verifies app          │
│             │     │ signature using stored key      │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ TrustZone   │────►│ Initialize secure world first   │
│ Init        │     │ before non-secure world         │
└──────┬──────┘     └─────────────────────────────────┘
       ▼
┌─────────────┐
│ Execute     │
└─────────────┘
```

---

## Tamper Detection and Response

### Response Levels

| Level | Trigger | Response |
|-------|---------|----------|
| Alert | Vibration, magnetic field | Log event, notify user |
| Suspend | Temperature out of range | Pause operations, require unlock |
| Zeroize | Mesh break, light sensor, repeated failures | Immediate key destruction |

### Zeroization Process

```
Tamper Event Detected
        │
        ▼
┌───────────────────┐
│ Tamper Supervisor │
│ (Battery-backed)  │
└────────┬──────────┘
         │
         ▼
┌───────────────────┐     ┌───────────────────┐
│ Assert Zeroize    │────►│ SE Secure Erase   │
│ Signal            │     │ Commands          │
└────────┬──────────┘     └───────────────────┘
         │
         ▼
┌───────────────────┐
│ Overwrite RAM     │
│ (Multiple passes) │
└────────┬──────────┘
         │
         ▼
┌───────────────────┐
│ Erase Flash Keys  │
└────────┬──────────┘
         │
         ▼
┌───────────────────┐
│ Device Bricked    │
│ (Requires recover)│
└───────────────────┘
```

---

## Threat Model

### What SimpleGo Protects Against

| Threat | Tier 1 | Tier 2 | Tier 3 |
|--------|--------|--------|--------|
| Mass surveillance | Yes | Yes | Yes |
| Network monitoring | Yes | Yes | Yes |
| Server compromise | Yes | Yes | Yes |
| Device theft (locked) | Partial | Yes | Yes |
| Physical flash extraction | Partial | Yes | Yes |
| Supply chain attacks | Partial | Partial | Yes |
| Cold boot attacks | No | Partial | Yes |
| Glitch/fault injection | No | Partial | Yes |
| Sophisticated lab attack | No | No | Partial |

### What SimpleGo Does NOT Fully Protect Against

| Threat | Limitation | Mitigation |
|--------|------------|------------|
| Device access while unlocked | Keys in memory | Lock when not in use |
| Nation-state with physical access | Advanced equipment | Tier 3 + disposal |
| Implementation bugs | Code quality | Audits, fuzzing |
| Side-channel attacks | Timing, power analysis | Tier 2+ hardware |
| Rubber hose cryptanalysis | Physical coercion | Duress PIN (Tier 2+) |

---

## Comparison with Alternatives

### SimpleGo vs GrapheneOS

| Aspect | GrapheneOS | SimpleGo Tier 1 | SimpleGo Tier 3 |
|--------|------------|-----------------|-----------------|
| TCB Size | ~50M lines | ~50K lines | ~50K lines |
| Baseband | Yes (closed) | No | No |
| Secure Element | Yes (Titan M) | No | Triple SE |
| TrustZone | Yes | No | Yes |
| Tamper Detection | No | No | Yes |
| Disposable | No (~$500+) | Yes (~$60) | Difficult (~$1200) |
| App Ecosystem | Full Android | None | None |
| Audit Surface | Large | Small | Small |

### SimpleGo vs Hardware Wallets (Ledger, Trezor)

| Aspect | Hardware Wallets | SimpleGo Tier 2/3 |
|--------|------------------|-------------------|
| Purpose | Cryptocurrency | General messaging |
| Secure Element | Yes | Yes (dual/triple) |
| Display | Small, limited | Full LCD |
| Input | Buttons | Keyboard |
| Connectivity | USB only | WiFi, cellular, LoRa |
| Communication | Transaction signing | Bidirectional messaging |

---

## Operational Security

### Physical Security Recommendations

| Tier | Recommendation |
|------|----------------|
| All | Store device securely when not in use |
| All | Enable secure boot and flash encryption |
| All | Verify contacts through secondary channel |
| Tier 1 | Consider device disposable if compromised |
| Tier 2+ | Check tamper seals periodically |
| Tier 2+ | Use duress PIN feature |
| Tier 3 | Maintain physical possession at all times |

### Network Security Recommendations

| Recommendation | Purpose |
|----------------|---------|
| Use trusted WiFi networks | Prevent local network attacks |
| Consider Tor (when implemented) | Traffic analysis resistance |
| Verify server fingerprints | Prevent MITM |
| Minimize online time | Reduce exposure window |

---

## Security Roadmap

### Completed

| Feature | Tier | Status |
|---------|------|--------|
| Double Ratchet encryption | All | Complete |
| X3DH key agreement | All | Complete |
| TLS 1.3 transport | All | Complete |
| ESP32 secure boot documentation | Tier 1 | Complete |
| HAL security abstractions | All | Complete |

### In Progress

| Feature | Tier | Status |
|---------|------|--------|
| ESP32 flash encryption guide | Tier 1 | In progress |
| Secure Element HAL interface | Tier 2/3 | Design |

### Planned

| Feature | Tier | Target |
|---------|------|--------|
| ATECC608B integration | Tier 2/3 | Q2 2026 |
| OPTIGA Trust M integration | Tier 2/3 | Q2 2026 |
| TrustZone implementation | Tier 2/3 | Q3 2026 |
| Tamper detection system | Tier 2/3 | Q3 2026 |
| SE050 integration | Tier 3 | Q4 2026 |
| Tamper supervisor | Tier 3 | Q4 2026 |
| Security audit | All | Q4 2026 |
| Tor integration | All | Research |

---

## Summary

SimpleGo's three-tier security architecture provides appropriate protection for different threat models:

| If Your Concern Is... | Recommended Tier |
|-----------------------|------------------|
| General privacy, hobbyist use | Tier 1 |
| Targeted surveillance, journalism | Tier 2 |
| Nation-state adversaries, high-risk activism | Tier 3 |

The fundamental security advantage comes from **simplicity**: a 50,000 line codebase with no baseband processor, no browser, no app ecosystem, and no telemetry presents a dramatically smaller attack surface than any smartphone platform.

**The most secure system is often the simplest one.**

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 2.0 | January 2026 | Complete rewrite for three-tier architecture |
| 1.0 | January 2026 | Initial security model documentation |
