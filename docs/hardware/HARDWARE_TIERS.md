# SimpleGo Hardware Tiers

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.2.0-draft

This document provides detailed specifications for each SimpleGo hardware tier, including connectivity options for cellular and LoRaWAN gateway operation.

---

## Tier 1: SimpleGo DIY (€100-200)

### Overview

Entry-level configuration for makers, developers, and enthusiasts. Basic security with software protections and single Secure Element.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R8 | €4.50 |
| Secure Element | ATECC608B-SSHDA-B | €0.90 |
| Display | ST7789V 2.4" IPS (320×240) | €8 |
| Input | BBQ20KBD compatible keyboard | €15 |
| Battery | 103450 LiPo 2000mAh | €8 |
| Charger | TP4056 module | €1 |

### Security Features

- ESP32-S3 Secure Boot v2 (RSA-3072)
- Flash Encryption (AES-256-XTS)
- ATECC608B for identity key storage
- JTAG disabled via eFuse
- No tamper detection (physical security only)

### Connectivity Options

| Configuration | Modules | Added Cost | Use Case |
|---------------|---------|------------|----------|
| **WiFi Only** | Integrated ESP32-S3 | €0 | Home/office use |
| **WiFi + LoRa** | + E22-900M30S (SX1262) | +€8 | Outdoor, mesh network |
| **WiFi + LTE-M** | + SIM7080G | +€12 | Mobile, low data |
| **WiFi + 4G** | + SIM7600E-H | +€22 | Full mobile broadband |

### E-Ink Pager Variant

| Component | Part | Price |
|-----------|------|-------|
| Display | Waveshare 2.9" E-Ink (296×128) | €18 |
| Input | Rotary encoder + 2 buttons | €3 |

**Total BOM:** €45-120 (depending on connectivity) + PCB + Enclosure

---

## Tier 2: SimpleGo Secure (€400-600)

### Overview

Enhanced security for journalists, activists, and privacy-focused users. Features dual-vendor Secure Elements, active tamper detection, and flexible connectivity including 4G/5G options.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | STM32U585CIU6 (Cortex-M33, TrustZone) | €12 |
| SE Primary | ATECC608B-SSHDA-B | €0.90 |
| SE Secondary | OPTIGA Trust M (CC EAL6+) | €3 |
| Display | ILI9341 3.2" IPS + touch | €15 |
| PMIC | AXP2101 | €3 |
| Battery | 103450 LiPo 3000mAh | €10 |

### Security Features

- STM32 TrustZone isolation
- DPA-resistant crypto accelerator
- Dual-vendor Secure Elements
- 6-layer PCB with security mesh
- Light sensor tamper detection
- Battery-backed SRAM for key zeroization
- Air-gap mode via QR codes

### Tamper System

| Component | Part | Price |
|-----------|------|-------|
| Light Sensor | APDS-9960 | €3 |
| BB-SRAM | 23LC1024 | €2 |
| Supervisor | DS1321 | €4 |

### Connectivity Options

| Configuration | Modules | Added Cost | Battery Life | Use Case |
|---------------|---------|------------|--------------|----------|
| **WiFi Only** | ESP32-C6-MINI-1 | €4 | 2-3 days | Office/home |
| **WiFi + LoRa** | + E22-900M30S | +€8 | 1-2 days | Field, mesh |
| **WiFi + LTE-M** | + nRF9160 | +€22 | 1-2 days | Mobile IoT |
| **WiFi + 4G** | + EG25-G | +€28 | 8-12 hours | Full mobile |
| **WiFi + 5G** | + RM500Q-GL | +€90 | 4-6 hours | Premium mobile |
| **Full Stack** | WiFi + LoRa + 4G | +€40 | 6-10 hours | Maximum flexibility |

### Recommended Configuration: WiFi + LoRa + 4G

```
┌─────────────────────────────────────────────────────────────┐
│                    TIER 2 CONNECTIVITY                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐                                           │
│   │ ESP32-C6    │ ← WiFi 6 (primary, low power)            │
│   │ WiFi Module │   Target Wake Time for battery savings   │
│   └─────────────┘                                           │
│                                                             │
│   ┌─────────────┐                                           │
│   │ SX1262      │ ← LoRa (backup, no infrastructure)       │
│   │ LoRa Module │   Meshtastic mesh, 10+ km range          │
│   └─────────────┘                                           │
│                                                             │
│   ┌─────────────┐                                           │
│   │ EG25-G      │ ← 4G LTE Cat 4 (always-on option)        │
│   │ 4G Module   │   150 Mbps, global bands                 │
│   └─────────────┘                                           │
│                                                             │
│   Priority: WiFi → 4G → LoRa mesh                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Total BOM:** €190-320 + PCB Assembly + CNC Enclosure

---

## Tier 3: SimpleGo Vault (€1,000+)

### Overview

Maximum security for enterprise, government, and high-value targets. Designed to resist state-level adversaries with physical access. Full connectivity options including 5G and optional LoRaWAN gateway capability.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | STM32U5A9NJH6Q (4MB Flash) | €18 |
| SE Primary | ATECC608B-TNGTLSS | €1.50 |
| SE Secondary | OPTIGA Trust M (EAL6+) | €4 |
| SE Tertiary | SE050E (FIPS 140-2 L3 capable) | €5 |
| Tamper Supervisor | Maxim DS3645 | €22 |
| Display | ILI9488 3.5" IPS + touch | €20 |
| PMIC | BQ25895 + BQ27441 | €8 |
| Battery | Custom pack 4000mAh | €15 |

### Security Features

- Triple-vendor Secure Elements
- Full environmental monitoring (voltage, temperature, clock)
- Sub-microsecond key zeroization
- Active tamper mesh (flex PCB wrap)
- Potted enclosure (aluminum-filled epoxy)
- Duress mode and "Brick Me" PIN
- Isolated cellular modem power domain

### Connectivity Stack

| Interface | Module | Price | Purpose |
|-----------|--------|-------|---------|
| WiFi 6 | ESP32-C6-MINI-1 | €4 | Primary (trusted networks) |
| LoRa P2P/Mesh | E22-900M30S (SX1262) | €8 | Infrastructure-independent |
| 4G LTE | EG25-G | €28 | Always-on mobile data |
| 5G (optional) | RM500Q-GL | €90 | Future-proof, high bandwidth |
| Satellite (optional) | Swarm M138 | €100 | Last resort global coverage |

### 5G Configuration

```
┌─────────────────────────────────────────────────────────────┐
│                    TIER 3 WITH 5G                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐  │
│   │   ESP32-C6  │     │   SX1262    │     │  RM500Q-GL  │  │
│   │   WiFi 6    │     │   LoRa      │     │     5G      │  │
│   └─────────────┘     └─────────────┘     └─────────────┘  │
│         │                   │                   │          │
│         │                   │                   │          │
│         └───────────────────┼───────────────────┘          │
│                             │                              │
│                      ┌──────┴──────┐                       │
│                      │  STM32U5A9  │                       │
│                      │  Main MCU   │                       │
│                      │  TrustZone  │                       │
│                      └─────────────┘                       │
│                                                             │
│   5G Power Requirements:                                   │
│   - Peak: 4A @ 3.7V                                        │
│   - Requires dedicated high-current LDO                    │
│   - Recommend: MP8859 (4A sync buck)                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### LoRaWAN Gateway Mode (Optional)

Tier 3 devices can operate as **LoRaWAN Gateways**, bridging LoRa messages from other SimpleGo devices to the internet:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TIER 3 AS LORAWAN GATEWAY                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   FIELD DEVICES                    GATEWAY                    CLOUD        │
│                                                                             │
│  ┌───────────┐                 ┌─────────────────┐       ┌─────────────┐   │
│  │ SimpleGo  │    LoRa         │  SimpleGo Vault │  5G   │   SimpleX   │   │
│  │ Tier 1/2  │ ──────────────► │  (Gateway Mode) │ ────► │   Server    │   │
│  └───────────┘    868 MHz      │                 │       └─────────────┘   │
│                                │ ┌─────────────┐ │                         │
│  ┌───────────┐                 │ │   SX1302    │ │                         │
│  │ SimpleGo  │ ──────────────► │ │ 8-Channel   │ │                         │
│  │ Tier 1/2  │                 │ │   LoRa GW   │ │                         │
│  └───────────┘                 │ └─────────────┘ │                         │
│                                │ ┌─────────────┐ │                         │
│  ┌───────────┐                 │ │  RM500Q-GL  │ │                         │
│  │ SimpleGo  │ ──────────────► │ │  5G Modem   │ │                         │
│  │ Tier 1/2  │                 │ └─────────────┘ │                         │
│  └───────────┘                 └─────────────────┘                         │
│                                                                             │
│  Gateway Coverage: 2-15 km radius (terrain dependent)                      │
│  Capacity: 1000+ end devices per 8-channel gateway                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Gateway Hardware Add-on

| Component | Part | Price | Notes |
|-----------|------|-------|-------|
| Gateway Concentrator | SX1302 + 2× SX1250 | €50 | 8-channel, all SF |
| Alternative | RAK2287 (mini-PCIe) | €80 | Easier integration |
| GPS (for timing) | L76K or similar | €8 | Required for Class B |

#### Gateway Configurations

| Mode | Hardware | Capacity | Power | Use Case |
|------|----------|----------|-------|----------|
| **Single-Channel** | SX1262 only | ~50 devices | 100 mA | Personal hotspot |
| **8-Channel** | SX1302 + SX1250 | ~1000 devices | 500 mA | Group/community |
| **Full Gateway** | RAK2287 | ~10000 devices | 1A | Public infrastructure |

### Connectivity Summary

| Configuration | Components | Total Added Cost | Power Draw |
|---------------|------------|------------------|------------|
| Standard | WiFi + LoRa + 4G | €40 | 600 mA active |
| Premium | WiFi + LoRa + 5G | €102 | 1.5 A active |
| Gateway | WiFi + LoRa GW + 5G | €160 | 2 A active |
| Ultimate | All + Satellite | €260 | 2.5 A active |

**Total BOM:** €560-1,200 + PCB Assembly + Potted CNC Enclosure

---

## Comparison Matrix

| Feature | Tier 1 (DIY) | Tier 2 (Secure) | Tier 3 (Vault) |
|---------|--------------|-----------------|----------------|
| **MCU** | ESP32-S3 | STM32U585 | STM32U5A9 |
| **TrustZone** | No | Yes | Yes |
| **Secure Elements** | 1 | 2 | 3 |
| **Tamper Detection** | None | Basic | Full |
| **PCB Layers** | 4 | 6 | 8 |
| **WiFi** | WiFi 4 (integrated) | WiFi 6 (module) | WiFi 6 (module) |
| **Cellular Options** | LTE-M / 4G | LTE-M / 4G / 5G | 4G / 5G |
| **LoRa** | P2P / Mesh | P2P / Mesh | P2P / Mesh / Gateway |
| **Satellite** | No | No | Optional |
| **Enclosure** | 3D Printed | CNC Aluminum | Potted CNC |
| **Target Price** | €100-200 | €400-600 | €1,000+ |
| **Threat Model** | Casual | Skilled | State-level |

---

## Connectivity Priority Logic

All tiers use the same connection priority algorithm:

```
┌─────────────────────────────────────────────────────────────┐
│                CONNECTION PRIORITY                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. WiFi (if available and trusted)                        │
│     └── Lowest latency, highest bandwidth, lowest power    │
│                                                             │
│  2. 5G/4G Cellular (if SIM present)                        │
│     └── Always available, metered, moderate power          │
│                                                             │
│  3. LTE-M/NB-IoT (if configured)                           │
│     └── Low power fallback for critical messages           │
│                                                             │
│  4. LoRa Mesh (if peers available)                         │
│     └── No infrastructure, limited bandwidth               │
│                                                             │
│  5. LoRaWAN (if gateway in range)                          │
│     └── Uses existing LoRaWAN infrastructure               │
│                                                             │
│  6. Satellite (Tier 3 only)                                │
│     └── Last resort, high latency, expensive               │
│                                                             │
│  User can override priority in settings                    │
│  "Stealth mode" disables all RF except on-demand           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Power Considerations by Connectivity

| Connectivity | Idle Current | TX Current | Peak Current | Notes |
|--------------|--------------|------------|--------------|-------|
| WiFi only | 20 mA | 150 mA | 400 mA | Best battery life |
| + LoRa | +5 mA | +120 mA | +500 mA | Mesh adds overhead |
| + LTE-M | +3 mA | +250 mA | +600 mA | PSM mode saves power |
| + 4G LTE | +15 mA | +500 mA | +2 A | Continuous connection |
| + 5G | +40 mA | +1.2 A | +4 A | Needs robust power |
| Gateway mode | +100 mA | +500 mA | +1 A | SX1302 always listening |

**Battery Life Estimates (3000mAh):**

| Mode | Tier 1 | Tier 2 | Tier 3 |
|------|--------|--------|--------|
| WiFi only, light use | 3-4 days | 2-3 days | 2-3 days |
| WiFi + LoRa | 2-3 days | 1-2 days | 1-2 days |
| WiFi + 4G | 12-18 hours | 8-12 hours | 8-12 hours |
| WiFi + 5G | N/A | 4-6 hours | 4-6 hours |
| Gateway mode | N/A | N/A | 4-8 hours (mains recommended) |
