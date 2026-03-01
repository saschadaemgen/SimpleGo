# PCB Design Guidelines

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

## Design Tools

**Recommended:** KiCad 8.0+ (open source, aligns with project philosophy)

---

## Layer Stackups

### Tier 1: 4-Layer

```
Layer 1: Signal (TOP)
Layer 2: GND
Layer 3: Power
Layer 4: Signal (BOTTOM)
```

### Tier 2: 6-Layer with Security Mesh

```
Layer 1: Signal (TOP)
Layer 2: SECURITY MESH        ← Tamper detection
Layer 3: GND
Layer 4: Power
Layer 5: SECURITY MESH        ← Tamper detection
Layer 6: Signal (BOTTOM)
```

### Tier 3: 8-Layer High Security

```
Layer 1: Signal (TOP)
Layer 2: SECURITY MESH TOP
Layer 3: GND
Layer 4: Signal (Internal)
Layer 5: Power
Layer 6: GND
Layer 7: SECURITY MESH BOTTOM
Layer 8: Signal (BOTTOM)
```

With blind vias (L1-L3, L6-L8) and buried vias (L3-L6).

---

## Security Mesh Design

**Parameters:**
- Trace width: 0.15mm
- Trace spacing: 0.3mm
- Layer: Inner layer 2 or 7
- Pattern: Serpentine with randomized corners
- Coverage: All sensitive components + 5mm margin

**Monitoring:**
- Connected to tamper supervisor inputs
- Checked every 100ms
- Any break/short triggers zeroization

**KiCad Plugin:** github.com/SebastianGo662/tamper-mesh-kicad

---

## Component Placement

```
┌─────────────────────────────────────────────────────┐
│                    SECURITY ZONE                    │
│    ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐     │
│    │  MCU  │  │  SE1  │  │  SE2  │  │  SE3  │     │
│    └───────┘  └───────┘  └───────┘  └───────┘     │
│    ┌─────────────────────────────────────────┐     │
│    │         TAMPER SUPERVISOR               │     │
│    └─────────────────────────────────────────┘     │
│    ═══════════ SECURITY MESH COVERAGE ═══════     │
└─────────────────────────────────────────────────────┘

┌─────────────────┐  ┌─────────────────┐
│   WIFI MODULE   │  │   LORA MODULE   │
└─────────────────┘  └─────────────────┘

┌─────────────────────────────────────────────────────┐
│    PMIC    BATTERY    USB-C    REGULATORS           │
└─────────────────────────────────────────────────────┘
```

**Guidelines:**
- Group MCU + all SEs + tamper IC together
- RF isolation from sensitive analog
- Clean power flow: Battery → PMIC → Regulators → Loads
- Debug header accessible for dev, removable for production

---

## Manufacturing Specifications

### Tier 1 (JLCPCB Standard)

| Parameter | Value | Cost (qty 5) |
|-----------|-------|--------------|
| Layers | 4 | €2-5 |
| Min trace | 0.127mm | |
| Finish | HASL | |

### Tier 2 (Advanced)

| Parameter | Value | Cost (qty 10) |
|-----------|-------|---------------|
| Layers | 6 | €40-80 |
| Min trace | 0.1mm | |
| Via-in-pad | Yes | |
| Finish | ENIG | |
| Impedance | Controlled | |

### Tier 3 (Premium/Domestic)

| Parameter | Value | Cost (qty 10) |
|-----------|-------|---------------|
| Layers | 8 | €150-300 |
| Min trace | 0.075mm | |
| Blind/buried vias | Yes | |
| Finish | ENIG + selective gold | |
| Manufacturer | Domestic (EU/US) | |

---

## Assembly Options

### Hand Assembly (Tier 1)

| Package | Difficulty |
|---------|------------|
| 0603 | Easy |
| 0402 | Medium (magnification) |
| QFN | Medium (hot air) |
| BGA | Hard (stencil + oven) |

### Professional Assembly (Tier 2+)

**JLCPCB SMT:** Min 2 boards, setup €8, BGA capable  
**PCBWay Turnkey:** Component sourcing included, authorized distributors

---

## Design Checklist

### Schematic
- [ ] Power supply voltages correct
- [ ] Decoupling capacitors on all power pins
- [ ] ESD protection on external interfaces
- [ ] I²C addresses don't conflict

### PCB
- [ ] DRC clean
- [ ] Footprints verified against datasheets
- [ ] Antenna keepout respected
- [ ] Security mesh coverage complete (Tier 2+)
- [ ] Test points accessible

### Manufacturing
- [ ] Gerbers visually verified
- [ ] BOM exported with all fields
- [ ] Assembly drawing created
