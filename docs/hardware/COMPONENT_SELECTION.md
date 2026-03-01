# Component Selection Guide

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.2.0-draft

## Microcontrollers

### Tier 1: ESP32-S3-WROOM-1-N8R8 (~€4.50)

| Feature | Value |
|---------|-------|
| Core | Dual Xtensa LX7 @ 240 MHz |
| Memory | 8 MB Flash, 8 MB PSRAM |
| Security | Secure Boot v2, Flash Encryption |
| Wireless | WiFi 4 + BLE 5.0 |

**Pros:** Low cost, large community, hardware crypto  
**Cons:** No TrustZone, limited side-channel resistance

### Tier 2: STM32U585CIU6 (~€12)

| Feature | Value |
|---------|-------|
| Core | Cortex-M33 @ 160 MHz |
| Memory | 2 MB Flash, 786 KB SRAM |
| Security | TrustZone, OTFDEC, HUK, tamper pins |
| Crypto | AES, SHA, PKA (DPA resistant) |

**Pros:** TrustZone, PSA L3 ready, tamper inputs  
**Cons:** Higher cost, needs external WiFi

### Tier 3: STM32U5A9NJH6Q (~€18)

Same security as U585 with 4 MB Flash, 2.5 MB SRAM, GPU.

### Alternative MCUs

| Part | Features | Price | Use Case |
|------|----------|-------|----------|
| ESP32-C6 | RISC-V, WiFi 6, PSA L2 | ~€3 | Budget WiFi coprocessor |
| nRF5340 | Dual CM33, BLE 5.3, Thread | ~€6 | BLE mesh primary |
| LPC55S69 | Dual CM33, PUF, TrustZone | ~€8 | Alternative to STM32 |

---

## Secure Elements

### ATECC608B (~€0.90) - All Tiers

| Feature | Value |
|---------|-------|
| Algorithms | ECC-P256, SHA-256, AES-128 |
| Key Slots | 16 |
| Interface | I²C |

**Use:** Identity key, TLS client, secure boot verification

### OPTIGA Trust M (~€3) - Tier 2+

| Feature | Value |
|---------|-------|
| Algorithms | ECC P-256/384/521, RSA-2048 |
| Certification | CC EAL6+ |
| Interface | Shielded I²C |

**Use:** Message keys, session management (different vendor)

### NXP SE050 (~€5) - Tier 3

| Feature | Value |
|---------|-------|
| Storage | 50 KB secure |
| Certification | CC EAL6+, FIPS 140-2 L3 (variant) |
| Interface | I²C + NFC provisioning |

**Use:** Backup/recovery, third vendor redundancy

### Comparison Matrix

| Feature | ATECC608B | OPTIGA Trust M | SE050 |
|---------|-----------|----------------|-------|
| Price | €0.90 | €3 | €5 |
| ECC-P256 | ✓ | ✓ | ✓ |
| ECC-P384/521 | ✗ | ✓ | ✓ |
| RSA | ✗ | ✓ | ✓ |
| CC Certification | ✗ | EAL6+ | EAL6+ |
| FIPS 140-2 | ✗ | ✗ | L3 (variant) |

---

## Displays

### LCD Options

| Part | Resolution | Interface | Price | Use Case |
|------|------------|-----------|-------|----------|
| ST7789V 2.4" | 320×240 | SPI | €8 | Tier 1 handheld |
| ILI9341 3.2" + Touch | 320×240 | SPI + I²C | €15 | Tier 2 handheld |
| ILI9488 3.5" | 480×320 | SPI/Parallel | €20 | Tier 3 premium |

### E-Ink Options

| Part | Resolution | Refresh | Price | Use Case |
|------|------------|---------|-------|----------|
| Waveshare 2.9" | 296×128 | 2s full, 0.3s partial | €18 | Tier 1 pager |
| GDEY042T81 4.2" | 400×300 | 1.5s full, 0.3s partial | €28 | Tier 2-3 pager |
| GDEY075T7 7.5" | 800×480 | 2s full | €45 | Desktop/wall mount |

---

## Connectivity

### WiFi Modules

| Module | Standard | Price | Notes |
|--------|----------|-------|-------|
| ESP32-S3 (integrated) | WiFi 4 | - | Built into Tier 1 MCU |
| ESP32-C6-MINI-1 | WiFi 6 | €4 | Coprocessor for STM32, TWT support |
| ESP32-C3-MINI-1 | WiFi 4 | €2.50 | Budget coprocessor |

---

## LoRa / Sub-GHz Radio

### Point-to-Point & Mesh (Meshtastic Compatible)

Infrastructure-independent communication for field use.

| Module | Chip | Frequency | Power | Range | Price |
|--------|------|-----------|-------|-------|-------|
| E22-900M30S | SX1262 | 868/915 MHz | +30 dBm (1W) | 10-15 km | €8 |
| E22-400M30S | SX1262 | 433 MHz | +30 dBm (1W) | 15-20 km | €8 |
| RFM95W | SX1276 | 868/915 MHz | +20 dBm | 5-10 km | €5 |
| E220-900M22S | LLCC68 | 868/915 MHz | +22 dBm | 5-8 km | €4 |
| Ra-01H | SX1262 | 868/915 MHz | +22 dBm | 8-12 km | €5 |

**Use Case:** 
- Direct device-to-device messaging
- Mesh networks via Meshtastic protocol
- No cellular/internet infrastructure needed
- Emergency/backup communication channel

### LoRaWAN (Network Infrastructure)

For integration with existing LoRaWAN networks or building your own gateway infrastructure.

#### LoRaWAN End Nodes

| Module | Chip | Class | Price | Notes |
|--------|------|-------|-------|-------|
| RAK3172 | STM32WLE5 | A/B/C | €8 | Integrated MCU + LoRa |
| E5-Mini | STM32WLE5 | A/B/C | €12 | Seeed module, FCC/CE certified |
| RN2483A | - | A | €15 | Microchip, mature ecosystem |
| SX1302 | SX1302 | Gateway | €40 | Multi-channel gateway chip |

#### LoRaWAN Gateway Mode

SimpleGo devices can optionally operate as **LoRaWAN Gateways**, bridging LoRa messages to the internet:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SIMPLEGO AS LORAWAN GATEWAY                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│    FIELD DEVICES                    GATEWAY                    CLOUD       │
│                                                                             │
│   ┌───────────┐                 ┌─────────────┐           ┌─────────────┐  │
│   │ SimpleGo  │    LoRa         │  SimpleGo   │   4G/5G   │   SimpleX   │  │
│   │  Pager 1  │ ──────────────► │   Gateway   │ ────────► │   Server    │  │
│   └───────────┘    868 MHz      │   Device    │   or      └─────────────┘  │
│                                 │             │   WiFi                      │
│   ┌───────────┐                 │ ┌─────────┐ │                             │
│   │ SimpleGo  │ ──────────────► │ │SX1302/  │ │                             │
│   │  Pager 2  │                 │ │SX1262   │ │                             │
│   └───────────┘                 │ └─────────┘ │                             │
│                                 │             │                             │
│   ┌───────────┐                 │ ┌─────────┐ │                             │
│   │ SimpleGo  │ ──────────────► │ │ 4G/5G   │ │                             │
│   │  Pager 3  │                 │ │ Modem   │ │                             │
│   └───────────┘                 │ └─────────┘ │                             │
│                                 └─────────────┘                             │
│                                                                             │
│   Range: 2-15 km per gateway (depends on terrain)                          │
│   Capacity: 1000s of end devices per gateway                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Gateway Hardware Options

| Configuration | Components | Cost | Capacity | Use Case |
|---------------|------------|------|----------|----------|
| **Single-Channel** | SX1262 + ESP32 | €15 | ~50 devices | Personal/small group |
| **8-Channel** | SX1302 + ESP32/RPi | €60 | ~1000 devices | Community gateway |
| **Full Gateway** | RAK2287 + RPi | €150 | ~10000 devices | Public infrastructure |

#### LoRaWAN Gateway Modules

| Module | Channels | Interface | Price | Notes |
|--------|----------|-----------|-------|-------|
| RAK2287 | 8 | SPI (mini-PCIe) | €80 | Best for RPi, GPS included |
| RAK5146 | 8 | SPI/USB | €90 | USB version available |
| SX1302CSSXXXGW1 | 8 | Reference design | €100 | Semtech eval board |
| SX1303 + SX1250 | 8 | SPI | €50 | DIY gateway |

#### Gateway Software Stack

```
┌─────────────────────────────────────────┐
│         SimpleGo Application            │
│      (Message routing, encryption)      │
├─────────────────────────────────────────┤
│         ChirpStack Gateway Bridge       │  ← Or custom implementation
│         (LoRaWAN packet forwarder)      │
├─────────────────────────────────────────┤
│         SX1302 HAL / Driver             │
├─────────────────────────────────────────┤
│         SPI Interface                   │
└─────────────────────────────────────────┘
```

**Gateway Mode Features:**
- Forward encrypted SimpleGo messages from LoRa to internet
- Can connect to public TTN (The Things Network) or private server
- Supports Class A, B, C devices
- 8-channel gateway supports 1000+ devices
- Can run on Tier 2/3 devices with cellular backhaul

#### Frequency Plans

| Region | Frequency | Channels | Max Power |
|--------|-----------|----------|-----------|
| EU868 | 863-870 MHz | 8 | +14 dBm (25 mW) |
| US915 | 902-928 MHz | 64+8 | +26 dBm (400 mW) |
| AU915 | 915-928 MHz | 64+8 | +26 dBm |
| AS923 | 923 MHz | 8 | +16 dBm |
| IN865 | 865-867 MHz | 3 | +30 dBm |

---

## Cellular Modules

### LTE Cat-M / NB-IoT (Low Power IoT)

Best for battery-powered devices with infrequent messaging.

| Module | Bands | Data Rate | Price | Notes |
|--------|-------|-----------|-------|-------|
| Quectel BG95-M3 | Global LTE-M/NB-IoT | 375 kbps | ~€18 | GPS included, low power |
| Quectel BG96 | Global LTE-M/NB-IoT | 375 kbps | ~€20 | Mature, well documented |
| **nRF9160 SiP** | Global LTE-M/NB-IoT | 375 kbps | ~€22 | **Integrated MCU + TrustZone, PSA L2** |
| SIM7080G | Global LTE-M/NB-IoT | 375 kbps | ~€12 | Budget option |

**Power Consumption:**
- Idle (PSM): 1-3 µA
- Idle (eDRX): 1-3 mA  
- Active TX: 200-400 mA

### 4G LTE Cat 4 (Full Speed Mobile Broadband)

Best for real-time messaging, media transfer, and gateway operation.

| Module | Bands | Data Rate | Price | Notes |
|--------|-------|-----------|-------|-------|
| **Quectel EC25-E** | LTE Cat 4 (EU) | 150/50 Mbps | ~€25 | Best for Europe |
| **Quectel EG25-G** | LTE Cat 4 (Global) | 150/50 Mbps | ~€28 | Worldwide, **recommended** |
| SIM7600E-H | LTE Cat 4 (EU) | 150/50 Mbps | ~€22 | Good Linux/RPi support |
| SIM7600G-H | LTE Cat 4 (Global) | 150/50 Mbps | ~€30 | All regions |
| Quectel EC21 | LTE Cat 1 | 10/5 Mbps | ~€18 | Lower power than Cat 4 |

**Power Consumption:**
- Idle: 10-20 mA
- Active TX: 500-800 mA
- Peak: 2A (ensure power supply can handle)

### 4G LTE Cat 6/12 (High Speed)

For gateway applications requiring higher bandwidth.

| Module | Bands | Data Rate | Price | Notes |
|--------|-------|-----------|-------|-------|
| Quectel EC200A-EU | LTE Cat 4 | 150/50 Mbps | ~€15 | Cost-optimized |
| Quectel EM06-E | LTE Cat 6 | 300/50 Mbps | ~€40 | M.2 form factor |
| Quectel EM12-G | LTE Cat 12 | 600/150 Mbps | ~€55 | Carrier aggregation |

### 5G Modules (Future-Proof / Maximum Performance)

For Tier 3 premium devices, gateways, and future-proofing.

| Module | Technology | Data Rate | Price | Notes |
|--------|------------|-----------|-------|-------|
| **Quectel RM500Q-GL** | 5G Sub-6 GHz | 2.1 Gbps / 900 Mbps | ~€80-100 | NSA/SA, **recommended** |
| Quectel RM500U-CN | 5G Sub-6 GHz | 2.1 Gbps / 900 Mbps | ~€70 | China bands |
| **Quectel RM520N-GL** | 5G Sub-6 + mmWave | 4.2 Gbps / 900 Mbps | ~€120-150 | Maximum speed |
| SIM8262E-M2 | 5G Sub-6 GHz | 2.4 Gbps / 500 Mbps | ~€90 | M.2 form factor |
| Fibocom FM160-EAU | 5G Sub-6 GHz | 2.1 Gbps / 900 Mbps | ~€85 | Good EU availability |
| Fibocom FG360-EAU | 5G Sub-6 GHz | 2.1 Gbps / 500 Mbps | ~€95 | Industrial grade |

**Power Consumption:**
- Idle: 20-50 mA
- Active TX: 1-2 A
- Peak: 3-4 A (requires robust power design!)

### Cellular Module Comparison

| Type | Latency | Battery Life | Cost | Best For |
|------|---------|--------------|------|----------|
| LTE-M/NB-IoT | 100-500ms | Weeks-Months | €12-22 | Pagers, sensors |
| 4G Cat 1 | 50-100ms | Days | €18-25 | Basic messaging |
| 4G Cat 4 | 30-50ms | Hours-Day | €22-30 | Real-time messaging, small gateway |
| 4G Cat 6/12 | 20-40ms | Hours | €40-55 | High-capacity gateway |
| 5G Sub-6 | 10-20ms | Hours | €70-100 | Premium devices, high-cap gateway |
| 5G mmWave | 1-10ms | Hours | €120-150 | Maximum performance gateway |

### Cellular Security Considerations

```
┌─────────────────────────────────────────────────────────────────┐
│              CELLULAR MODULE ISOLATION                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ⚠️  ALL cellular modules have CLOSED-SOURCE baseband firmware  │
│      Treat the modem as UNTRUSTED / potentially hostile         │
│                                                                 │
│  REQUIRED MITIGATIONS:                                          │
│                                                                 │
│  1. Physical isolation                                          │
│     ┌─────────────┐      ┌─────────────┐                       │
│     │   MAIN MCU  │ UART │   MODEM     │                       │
│     │  (trusted)  │◄────►│ (untrusted) │                       │
│     │             │      │             │                       │
│     │  TLS 1.3    │      │  Baseband   │                       │
│     │  endpoint   │      │  firmware   │                       │
│     └─────────────┘      └─────────────┘                       │
│           │                    │                                │
│     Separate power        Can be powered                       │
│     domain                down when not needed                 │
│                                                                 │
│  2. Network-level security                                      │
│     - TLS 1.3 for ALL data (assume network is hostile)         │
│     - Certificate pinning for SimpleX servers                  │
│     - No sensitive data in SMS/USSD                            │
│                                                                 │
│  3. Best practice: nRF9160                                      │
│     - TrustZone isolation between app and modem                │
│     - PSA Level 2 certified                                    │
│     - App processor can't access modem internals               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### SIM Options

| Type | Description | Use Case |
|------|-------------|----------|
| Nano-SIM slot | Physical SIM card | Standard, user-swappable |
| eSIM (eUICC) | Embedded, remote provisioning | No physical access needed |
| Dual SIM | Two slots or SIM + eSIM | Redundancy, travel |
| Multi-IMSI SIM | Single SIM, multiple profiles | Global roaming |

**Recommendations:**
- Tier 1-2: Nano-SIM slot (simple, user-controlled)
- Tier 3: eSIM + Nano-SIM slot (redundancy + remote provisioning)

### Antenna Requirements

| Technology | Antenna Size | Bands | Notes |
|------------|--------------|-------|-------|
| LoRa | Small PCB or whip | 433/868/915 MHz | External SMA recommended |
| LTE-M/NB-IoT | Small PCB antenna | 700-2100 MHz | Compact designs OK |
| 4G LTE | Medium, external recommended | 700-2700 MHz | Consider external SMA |
| 5G Sub-6 | Medium-Large | 600-6000 MHz | External antenna recommended |
| 5G mmWave | Antenna array | 24-40 GHz | Specialized design required |

---

## Satellite Communication (Last Resort)

For truly infrastructure-independent messaging.

| Module | Network | Coverage | Message Size | Price | Notes |
|--------|---------|----------|--------------|-------|-------|
| Swarm M138 | Swarm | Global | 192 bytes | ~€100 | VHF, low power |
| RockBLOCK 9603 | Iridium | Global | 340 bytes | ~€200 | Proven, higher power |
| L-band modules | Starlink | TBD | TBD | TBD | Future option |

**Use Case:** Tier 3 emergency fallback, remote areas, censorship circumvention

---

## Tamper Detection

| Component | Function | Price |
|-----------|----------|-------|
| DS3645 | Tamper supervisor (8 inputs) | €22 |
| APDS-9960 | Light + proximity sensor | €3 |
| 23LC1024 | Battery-backed SRAM | €2 |
| DS1321 | SRAM supervisor | €4 |

---

## Power Management

| Component | Function | Price |
|-----------|----------|-------|
| TP4056 | Basic Li-Ion charger | €0.50 |
| AXP2101 | Full PMIC + fuel gauge | €3 |
| BQ25895 | High-current charger (5A) | €4 |
| MP2759 | USB-C PD sink controller | €2 |
| Tadiran TL-5902 | 20-year backup battery | €8 |

### Power Budget Examples

| Configuration | Idle | Active | Peak | Battery Life (3000mAh) |
|---------------|------|--------|------|------------------------|
| WiFi only | 20 mA | 150 mA | 400 mA | 2-3 days active |
| WiFi + LoRa | 25 mA | 250 mA | 500 mA | 1-2 days active |
| WiFi + LTE-M | 25 mA | 300 mA | 600 mA | 1-2 days active |
| WiFi + 4G LTE | 35 mA | 600 mA | 2 A | 8-12 hours active |
| WiFi + 5G | 50 mA | 1.2 A | 4 A | 4-6 hours active |
| Gateway (LoRa + 4G) | 100 mA | 800 mA | 3 A | 4-8 hours (needs mains power) |

**Note:** 5G and gateway operation require robust power design - minimum 4A capability!

---

## Sourcing Guidelines

### Authorized Distributors Only

- DigiKey, Mouser, Arrow, Farnell, RS Components

### Specialized Sources

| Component Type | Recommended Source |
|----------------|-------------------|
| General ICs | DigiKey, Mouser |
| Quectel modules | DigiKey, Quectel Direct |
| SIMCom modules | SIMCom Direct, Mouser |
| Nordic (nRF) | DigiKey, Nordic Direct |
| LoRa modules | EBYTE Direct, DigiKey |
| RAK modules | RAKwireless Store |

### Never Source From

- AliExpress (for production)
- eBay, unknown brokers
- Unauthorized resellers (counterfeit risk)

---

## Quick Reference

```
MCUs:
  ESP32-S3-WROOM-1-N8R8      Tier 1
  STM32U585CIU6              Tier 2
  STM32U5A9NJH6Q             Tier 3

Secure Elements:
  ATECC608B-SSHDA-B          All tiers (primary)
  SLS32AIA010MH              Tier 2+ (OPTIGA)
  SE050E                     Tier 3 (NXP)

LoRa:
  E22-900M30S (SX1262)       Point-to-point, mesh
  RAK3172                    LoRaWAN end node
  RAK2287 / SX1302           LoRaWAN gateway (8-ch)

Cellular:
  BG95-M3 / nRF9160          LTE-M/NB-IoT (low power)
  EG25-G / SIM7600G-H        4G LTE Cat 4 (global)
  RM500Q-GL                  5G Sub-6 (premium)
  RM520N-GL                  5G Sub-6 + mmWave (max)

Tamper:
  DS3645+                    Tier 3 supervisor
  APDS-9960                  Light sensor
  23LC1024-I/SN              Battery-backed SRAM
```
