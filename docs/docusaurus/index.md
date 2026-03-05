---
title: "Hardware Overview"
sidebar_position: 1
---

# Hardware

SimpleGo is the **world's first native hardware implementation** of the SimpleX Messaging Protocol (SMP) outside the official Haskell codebase. This section covers dedicated encrypted communication devices that operate independently of smartphones.

## Why Dedicated Hardware?

| Smartphone Weakness | SimpleGo Solution |
|---------------------|-------------------|
| Massive attack surface (millions LOC) | Minimal firmware (~50k LOC) |
| Closed-source baseband with network access | No cellular modem OR isolated modem |
| App sandboxing can be bypassed | Bare-metal execution, no OS |
| Keys in software/TEE (extractable) | Hardware Secure Elements |
| No tamper detection | Active tamper monitoring |
| Designed for convenience | Designed for security |

## Product Line

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SIMPLEGO PRODUCT LINE                             │
├─────────────────┬─────────────────────┬─────────────────────────────────────┤
│  TIER 1: DIY    │   TIER 2: SECURE    │      TIER 3: VAULT                  │
│  €100-200       │     €400-600        │        €1000+                       │
├─────────────────┼─────────────────────┼─────────────────────────────────────┤
│ ESP32-S3 based  │ STM32U5 TrustZone   │ STM32U5 + Triple SE                 │
│ Single ATECC608B│ Dual SE (2 vendors) │ Maxim DS3645 tamper IC              │
│ Secure boot     │ PCB tamper mesh     │ Full environmental monitoring        │
│ 3D printed case │ CNC aluminum case   │ Potted CNC enclosure                │
│ WiFi only       │ WiFi + LoRa         │ WiFi + Cellular + LoRa              │
├─────────────────┼─────────────────────┼─────────────────────────────────────┤
│ Makers,         │ Journalists,        │ Enterprise, Government,              │
│ Developers,     │ Activists           │ High-risk Individuals                │
│ Enthusiasts     │                     │                                      │
└─────────────────┴─────────────────────┴─────────────────────────────────────┘
```

Each tier is available in multiple form factors: **Handheld** (color LCD + physical keyboard), **Pager** (E-Ink, ultra-low power), and **Desktop** (USB terminal interface).

## Core Architecture: Dual-Chip Security Model

The foundation of SimpleGo security is the separation of application processing from key storage. The main MCU executes code, handles networking, and renders the UI — but **never holds private keys**. All cryptographic material lives exclusively in dedicated Secure Elements.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                                   │
│                SimpleX Protocol / UI / Network Stack                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                    ┌─────────────────────────────────┐                      │
│                    │        MAIN MCU                  │                     │
│                    │  • Code execution                │                     │
│                    │  • Network handling              │                     │
│                    │  • UI rendering                  │                     │
│                    │  • NEVER holds private keys      │                     │
│                    └───────────────┬─────────────────┘                      │
│                          I²C Bus (SCP03 encrypted on Tier 3)                │
│      ┌─────────────────────────────┼─────────────────────────────┐         │
│      ▼                             ▼                             ▼         │
│ ┌─────────────┐           ┌─────────────┐           ┌─────────────┐       │
│ │ ATECC608B   │           │OPTIGA Trust │           │  NXP SE050  │       │
│ │ (Microchip) │           │ M (Infineon)│           │   (NXP)     │       │
│ │ • Identity  │           │ • Session   │           │ • Backup    │       │
│ │ • TLS auth  │           │ • Ratchet   │           │ • Recovery  │       │
│ │ • Boot verify│          │ • Ephemeral │           │ • Escrow    │       │
│ └─────────────┘           └─────────────┘           └─────────────┘       │
│    All Tiers                 Tier 2+                   Tier 3              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Why this matters:** Security researchers demonstrated voltage glitching attacks against MCU-only designs (e.g. Trezor Model T) with 100% success rate in under 15 minutes using ~$100 equipment. Coldcard Mk4 answered this with dual Secure Elements from different manufacturers — even if one SE has a backdoor, the attacker cannot reconstruct the full key. SimpleGo adopts this proven architecture for messaging security.

## Hardware Abstraction Layer

SimpleGo supports multiple hardware platforms through a HAL that keeps the protocol and UI code 100% shared across all devices:

```
┌─────────────────────────────────────────────────────────────────┐
│              APPLICATION / UI (100% shared)                     │
├─────────────────────────────────────────────────────────────────┤
│              CORE / PROTOCOL (100% shared)                      │
├─────────────────────────────────────────────────────────────────┤
│              HAL Interface Layer (interface only)               │
├────────────────┬────────────────┬───────────────────────────────┤
│   T-Deck Plus  │  SimpleGo DIY  │     Raspberry Pi              │
│   320×240 LCD  │  E-Ink 2.9"    │     SDL2 Window               │
│   ESP32-S3     │  ESP32-S3+SE   │     Linux Host                │
└────────────────┴────────────────┴───────────────────────────────┘
```

## Documentation

| Document | Description |
|----------|-------------|
| [Hardware Tiers](hardware-tiers) | Full specs for all three tiers |
| [Component Selection](component-selection) | Component choices and sourcing |
| [HAL Architecture](hal-architecture) | Hardware Abstraction Layer design |
| [PCB Design](pcb-design) | PCB guidelines, layer stackups, security mesh |
| [Enclosure Design](enclosure-design) | Enclosure materials and manufacturing |
| [Hardware Security](hardware-security) | Security architecture and tamper response |
| [Adding a New Device](adding-new-device) | Porting guide for new hardware |
| [T-Deck Plus](t-deck-plus) | Current development device reference |

## Development Status

| Phase | Status |
|-------|--------|
| Protocol Implementation (TLS, SMP, X3DH, Double Ratchet) | ✅ Complete |
| HAL interface definitions | ✅ Complete |
| T-Deck Plus basic implementation | ✅ Complete |
| Complete HAL implementations (input, audio, storage) | 🔧 In Progress |
| LVGL UI integration | 🔧 In Progress |
| Final component selection for custom PCB | 📋 Planned |
| Schematic capture + PCB layout | 📋 Planned |
| Prototype manufacturing | 📋 Planned |
| Third-party security audit | 📋 Planned |
