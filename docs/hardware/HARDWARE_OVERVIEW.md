# SimpleGo Hardware Specification

> **Version:** 0.1.0-draft  
> **Status:** Planning Phase  
> **Last Updated:** January 2026  
> **License:** Hardware: CERN-OHL-S-2.0 | Software: AGPL-3.0

---

## Executive Summary

SimpleGo represents the **world's first native hardware implementation** of the SimpleX Messaging Protocol (SMP) outside the official Haskell codebase. This document series describes specifications for dedicated encrypted communication devices that operate independently of smartphones.

### Why Dedicated Hardware?

| Smartphone Weakness | SimpleGo Solution |
|---------------------|-------------------|
| Massive attack surface (millions LOC) | Minimal firmware (~50k LOC) |
| Closed-source baseband with network access | No cellular modem OR isolated modem |
| App sandboxing can be bypassed | Bare-metal execution, no OS |
| Keys in software/TEE (extractable) | Hardware Secure Elements |
| No tamper detection | Active tamper monitoring |
| Designed for convenience | Designed for security |

---

## Document Structure

| Document | Description |
|----------|-------------|
| **[HARDWARE_OVERVIEW.md](HARDWARE_OVERVIEW.md)** | This document - architecture overview |
| **[HARDWARE_SECURITY.md](HARDWARE_SECURITY.md)** | Security model, threat analysis, countermeasures |
| **[HARDWARE_TIERS.md](HARDWARE_TIERS.md)** | Three product tiers with full specifications |
| **[HAL_ARCHITECTURE.md](HAL_ARCHITECTURE.md)** | Hardware Abstraction Layer design |
| **[DEVICE_TEMPLATES.md](DEVICE_TEMPLATES.md)** | Guide for new device implementations |
| **[COMPONENT_SELECTION.md](COMPONENT_SELECTION.md)** | Component specifications and sourcing |

---

## Product Line Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SIMPLEGO PRODUCT LINE                             │
├─────────────────────┬─────────────────────┬─────────────────────────────────┤
│    TIER 1: DIY      │   TIER 2: SECURE    │      TIER 3: VAULT              │
│    €100-200         │     €400-600        │        €1000+                   │
├─────────────────────┼─────────────────────┼─────────────────────────────────┤
│ • ESP32-S3 based    │ • STM32U5 TrustZone │ • STM32U5 + Triple SE           │
│ • Single ATECC608B  │ • Dual SE (2 vendors)│ • Maxim DS3645 tamper IC       │
│ • Secure boot       │ • PCB tamper mesh   │ • Full environmental monitor   │
│ • 3D printed case   │ • CNC aluminum case │ • Potted CNC enclosure          │
│ • WiFi only         │ • WiFi + LoRa       │ • WiFi + Cellular + LoRa       │
│                     │ • Air-gap via QR    │ • Satellite backup (optional)  │
├─────────────────────┼─────────────────────┼─────────────────────────────────┤
│ Makers, Developers  │ Journalists,        │ Enterprise, Government,         │
│ Privacy Enthusiasts │ Activists           │ High-risk Individuals           │
└─────────────────────┴─────────────────────┴─────────────────────────────────┘
```

### Form Factors

Each tier is available in multiple form factors:

- **Handheld**: Color LCD, physical keyboard, full-featured
- **Pager**: E-Ink display, ultra-low power, discreet
- **Desktop**: USB-connected, secure terminal interface

---

## Core Architecture

### Dual-Chip Security Model

The foundation of SimpleGo security is **separation of application processing from key storage**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                                   │
│                SimpleX Protocol / UI / Network Stack                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                    ┌─────────────────────────────────┐                      │
│                    │        MAIN MCU                 │                      │
│                    │   (Application Processor)       │                      │
│                    │                                 │                      │
│                    │  • Code execution               │                      │
│                    │  • Network handling             │                      │
│                    │  • UI rendering                 │                      │
│                    │  • NEVER holds private keys     │                      │
│                    └───────────────┬─────────────────┘                      │
│                                    │                                        │
│                          I²C Bus (SCP03 encrypted on Tier 3)                │
│                                    │                                        │
│      ┌─────────────────────────────┼─────────────────────────────┐         │
│      │                             │                             │         │
│      ▼                             ▼                             ▼         │
│ ┌─────────────┐           ┌─────────────┐           ┌─────────────┐       │
│ │ ATECC608B   │           │OPTIGA Trust │           │  NXP SE050  │       │
│ │ (Microchip) │           │ M (Infineon)│           │   (NXP)     │       │
│ │             │           │             │           │             │       │
│ │ • Identity  │           │ • Session   │           │ • Backup    │       │
│ │ • TLS auth  │           │ • Ratchet   │           │ • Recovery  │       │
│ │ • Boot verify│          │ • Ephemeral │           │ • Escrow    │       │
│ └─────────────┘           └─────────────┘           └─────────────┘       │
│    All Tiers                 Tier 2+                   Tier 3             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why This Matters

**The Trezor Lesson**: Security researchers demonstrated voltage glitching attacks against Trezor Model T (MCU-only design) with **100% success rate** in under 15 minutes using ~$100 equipment. The extracted seed phrase allowed complete fund theft.

**Coldcard's Answer**: Coldcard Mk4 uses dual Secure Elements from different manufacturers. Even if one SE has a backdoor, the attacker cannot reconstruct the full key.

**SimpleGo adopts this proven architecture** for messaging security.

---

## Hardware Abstraction Layer

SimpleGo supports multiple hardware platforms through a **Hardware Abstraction Layer (HAL)**:

```
┌─────────────────────────────────────────────────────────────────┐
│                    APPLICATION / UI                             │  ← 100% Shared
│              (Screens, Menus, Chat Views, LVGL)                 │
├─────────────────────────────────────────────────────────────────┤
│                    CORE / PROTOCOL                              │  ← 100% Shared
│         (SimpleX SMP, Double Ratchet, X3DH, Crypto)             │
├─────────────────────────────────────────────────────────────────┤
│              HAL (Hardware Abstraction Layer)                   │  ← Interface Only
│      hal_display, hal_input, hal_network, hal_secure, etc.      │
├────────────────┬────────────────┬───────────────────────────────┤
│   T-Deck Plus  │  SimpleGo DIY  │     Raspberry Pi              │  ← Device
│   320×240 LCD  │  E-Ink 2.9"    │     SDL2 Window               │    Specific
│   Keyboard     │  Buttons       │     USB Keyboard              │
│   ESP32-S3     │  ESP32-S3+SE   │     Linux Host                │
└────────────────┴────────────────┴───────────────────────────────┘
```

**Benefits:**
- **One codebase, many devices**: Protocol bugs fixed once, all devices benefit
- **Easy extensibility**: New device = implement HAL, core untouched
- **Community friendly**: Fork, add your board, submit PR
- **Desktop development**: Raspberry Pi/SDL version for testing without hardware

See **[HAL_ARCHITECTURE.md](HAL_ARCHITECTURE.md)** for complete specifications.

---

## Project Directory Structure

```
simplex_client/
├── main/
│   ├── core/                    # SimpleX Protocol (100% shared)
│   │   ├── smp/                 # SMP client
│   │   ├── crypto/              # Encryption, Double Ratchet
│   │   ├── agent/               # Agent protocol
│   │   └── transport/           # TLS, connections
│   │
│   ├── hal/                     # HAL Headers (interfaces)
│   │   ├── hal_display.h
│   │   ├── hal_input.h
│   │   ├── hal_network.h
│   │   ├── hal_secure.h
│   │   ├── hal_storage.h
│   │   ├── hal_power.h
│   │   └── hal_audio.h
│   │
│   └── ui/                      # LVGL UI (100% shared)
│       ├── screens/
│       ├── widgets/
│       └── themes/
│
├── devices/                     # Device implementations
│   ├── t_deck_plus/
│   │   ├── config/device_config.h
│   │   └── hal_impl/
│   ├── simplego_diy/
│   ├── simplego_secure/
│   └── raspberry_pi/
│
├── docs/hardware/               # This documentation
│
└── tools/
    ├── provisioning/            # SE key provisioning
    └── testing/                 # Test suites
```

---

## Development Status

### Phase 1: Protocol Implementation ✅
- [x] TLS 1.3 connection to SMP servers
- [x] SMP handshake and command protocol
- [x] X3DH key agreement
- [x] Double Ratchet encryption
- [ ] Agent Protocol parsing (in progress)
- [ ] Bidirectional messaging

### Phase 2: HAL & Multi-Device
- [x] HAL interface definitions
- [x] T-Deck Plus basic implementation
- [ ] Complete device template system
- [ ] Additional device ports

### Phase 3: Custom Hardware
- [ ] Final component selection
- [ ] Schematic capture
- [ ] Security-focused PCB layout
- [ ] Prototype manufacturing
- [ ] Third-party security audit

---

## Quick Links

- **Security details**: [HARDWARE_SECURITY.md](HARDWARE_SECURITY.md)
- **Product specifications**: [HARDWARE_TIERS.md](HARDWARE_TIERS.md)
- **Add new device**: [DEVICE_TEMPLATES.md](DEVICE_TEMPLATES.md)
- **Component info**: [COMPONENT_SELECTION.md](COMPONENT_SELECTION.md)

---

## References

- [SimpleX Protocol](https://github.com/simplex-chat/simplexmq)
- [Coldcard Security Model](https://blog.coinkite.com/understanding-mk4-security-model/)
- [Precursor Hardware](https://betrusted.io)
- [STM32U5 Series](https://www.st.com/stm32u5)
- [ATECC608B](https://www.microchip.com/ATECC608B)
