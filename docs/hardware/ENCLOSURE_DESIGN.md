# Enclosure Design Guidelines

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

This document covers enclosure design, materials, manufacturing methods, and tamper-evident features for SimpleGo hardware.

---

## Design Philosophy

### Requirements by Tier

| Requirement | Tier 1 | Tier 2 | Tier 3 |
|-------------|--------|--------|--------|
| Protection | Basic drop/scratch | IP54 splash resistant | IP67 waterproof |
| Tamper Evidence | Optional stickers | Serialized screws | Cryptographic verification |
| Material | 3D printed plastic | CNC aluminum | Potted CNC aluminum |
| Assembly | Snap-fit / screws | Gasket + security screws | Epoxy sealed |
| Cost Target | €5-15 | €80-150 | €150-300 |

---

## Tier 1: 3D Printed Enclosure

### Material Options

| Material | Pros | Cons | Use Case |
|----------|------|------|----------|
| **PLA** | Easy to print, biodegradable | Brittle, low heat resistance | Prototypes |
| **PETG** | Stronger, better heat resistance | Slight stringing | Production DIY |
| **ABS** | Durable, heat resistant | Warping, requires enclosure | Higher quality |
| **ASA** | UV resistant, durable | Harder to print | Outdoor use |

**Recommendation:** PETG for DIY production kits

### Design Guidelines

```
┌─────────────────────────────────────────────────────────────┐
│                    ENCLOSURE CROSS-SECTION                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│    Wall thickness: 2.0mm minimum                            │
│    ┌─────────────────────────────────────────────────┐     │
│    │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │     │
│    │ ▓                                             ▓ │     │
│    │ ▓   ┌─────────────────────────────────────┐   ▓ │     │
│    │ ▓   │              PCB                    │   ▓ │     │
│    │ ▓   │         (with standoffs)            │   ▓ │     │
│    │ ▓   └─────────────────────────────────────┘   ▓ │     │
│    │ ▓                                             ▓ │     │
│    │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │     │
│    └─────────────────────────────────────────────────┘     │
│                                                             │
│    PCB standoffs: 3mm height, M2.5 screw bosses            │
│    Display cutout: +0.5mm tolerance                         │
│    Button cutouts: +0.3mm tolerance                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Snap-Fit Design

```
                    ┌───────────────┐
                    │   TOP SHELL   │
                    │               │
    Snap hook ──────┤►  ┌─────┐    │
                    │   │     │◄───┼── Catch
                    │   └──┬──┘    │
                    │      │       │
                    └──────┼───────┘
                           │
                    ┌──────┼───────┐
                    │      │       │
                    │   ┌──┴──┐    │
    Catch ledge ────┤►  │     │    │
                    │   └─────┘    │
                    │               │
                    │ BOTTOM SHELL  │
                    └───────────────┘

    Hook deflection: 0.5-1.0mm
    Hook angle: 30-45°
    Minimum 4 snaps for secure closure
```

### Print Settings

```
Layer height:     0.2mm (0.12mm for fine details)
Infill:           20-30% (gyroid or grid)
Perimeters:       3 minimum
Top/bottom:       4 layers minimum
Supports:         Minimize with design orientation
Print orientation: Largest flat surface on bed
```

### Files to Provide

```
enclosures/tier1/
├── simplego_diy_top.stl
├── simplego_diy_bottom.stl
├── simplego_diy_buttons.stl    # Optional button caps
├── simplego_diy.step           # For modification
├── simplego_diy.f3d            # Fusion 360 source
└── README.md                   # Print instructions
```

---

## Tier 2: CNC Aluminum Enclosure

### Material Selection

| Alloy | Properties | Use Case |
|-------|------------|----------|
| **6061-T6** | Good machinability, anodizes well | Standard choice |
| **6063-T5** | Better surface finish | Premium appearance |
| **7075-T6** | Highest strength | Maximum durability |
| **5052** | Best corrosion resistance | Marine/outdoor |

**Recommendation:** 6061-T6 for balance of cost, machinability, and strength

### Design for CNC

```
┌─────────────────────────────────────────────────────────────────┐
│                    CNC DESIGN GUIDELINES                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  WALL THICKNESS                                                 │
│  ─────────────────                                              │
│  Minimum: 1.5mm (2.0mm recommended)                            │
│  Near features: 2.5mm minimum                                   │
│                                                                 │
│  INTERNAL CORNERS                                               │
│  ─────────────────                                              │
│  Minimum radius: Tool diameter / 2                              │
│  Recommended: R1.5mm for 3mm end mill                          │
│                                                                 │
│      BAD:              GOOD:                                    │
│      ┌────┐            ╭────╮                                   │
│      │    │            │    │                                   │
│      │    │            │    │                                   │
│      └────┘            ╰────╯                                   │
│   Sharp corner      R1.5 radius                                 │
│                                                                 │
│  POCKETS                                                        │
│  ─────────────────                                              │
│  Maximum depth: 4× tool diameter                                │
│  Draft angle: 0° acceptable for CNC (not injection molding)    │
│                                                                 │
│  THREADS                                                        │
│  ─────────────────                                              │
│  Use threaded inserts (brass) for frequent assembly            │
│  Direct threads: M3 minimum, 2× diameter depth                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Gasket Design (IP54)

```
┌─────────────────────────────────────────────────────────────────┐
│                      GASKET GROOVE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Cross-section:                                                 │
│                                                                 │
│        TOP SHELL                                                │
│    ════════════════════                                         │
│           │                                                     │
│    ┌──────┴──────┐                                             │
│    │             │  ← Gasket groove                            │
│    │  ○○○○○○○○○  │  ← O-ring or foam gasket                    │
│    │             │                                              │
│    └──────┬──────┘                                             │
│           │                                                     │
│    ════════════════════                                         │
│       BOTTOM SHELL                                              │
│                                                                 │
│  Groove dimensions for 2mm cord diameter O-ring:               │
│  - Width: 2.5mm                                                │
│  - Depth: 1.5mm                                                │
│  - 25% compression when assembled                              │
│                                                                 │
│  Material: Silicone (temperature resistant)                    │
│            EPDM (chemical resistant)                           │
│            Neoprene (general purpose)                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Surface Finish Options

| Finish | Process | Appearance | Cost |
|--------|---------|------------|------|
| As-machined | None | Tool marks visible | Lowest |
| Bead blasted | Glass bead media | Matte, uniform | Low |
| Brushed | Abrasive belt | Directional lines | Medium |
| **Anodized** | Electrochemical | Color options, durable | Medium |
| Hard anodized | Type III anodizing | Very hard, dark gray | High |
| Cerakote | Ceramic coating | Custom colors, tactical | High |

**Recommendation:** Bead blast + Type II anodize (black or natural)

### Security Screws

```
┌─────────────────────────────────────────────────────────────────┐
│                    SECURITY SCREW OPTIONS                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  TAMPER-RESISTANT:                                              │
│                                                                 │
│    Torx Pin        Tri-Wing       Snake Eye      Spanner       │
│      ╳              ✱               ⊙              ▬           │
│    (T10 pin)      (Nintendo)     (Custom)       (Slot)        │
│                                                                 │
│  SERIALIZED (Tier 2+):                                         │
│                                                                 │
│    Custom screws with laser-engraved serial numbers            │
│    Serial recorded in device OTP memory                        │
│    Visual inspection detects replacement                       │
│                                                                 │
│  RECOMMENDATION:                                                │
│    Tier 2: Torx Pin (T10) + serialized heads                   │
│    Tier 3: Custom profile + serialized + epoxy                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Manufacturing Sources

| Supplier | Region | MOQ | Lead Time | Notes |
|----------|--------|-----|-----------|-------|
| PCBWay CNC | China | 1 | 7-14 days | Good for prototypes |
| Xometry | Global | 1 | 3-10 days | Instant quotes |
| Protolabs | US/EU | 1 | 1-7 days | Fastest turnaround |
| Local shops | Local | 1 | Varies | Best for iteration |
| Fictiv | US | 1 | 5-10 days | Good quality |

**Cost Estimate (Tier 2 enclosure):**
- Prototype (qty 1): €80-150
- Small batch (qty 10): €40-80 each
- Production (qty 100): €20-40 each

---

## Tier 3: High-Security Enclosure

### Potted Assembly

```
┌─────────────────────────────────────────────────────────────────┐
│                    POTTED ENCLOSURE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Cross-section:                                                 │
│                                                                 │
│    ┌─────────────────────────────────────────────────────┐     │
│    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │
│    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │
│    │▓▓   ┌───────────────────────────────────────┐   ▓▓│     │
│    │▓▓   │                PCB                    │   ▓▓│     │
│    │▓▓   │  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐ │   ▓▓│     │
│    │▓▓   │  │ MCU │  │ SE1 │  │ SE2 │  │ SE3 │ │   ▓▓│     │
│    │▓▓   │  └─────┘  └─────┘  └─────┘  └─────┘ │   ▓▓│     │
│    │▓▓   └───────────────────────────────────────┘   ▓▓│     │
│    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │
│    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │
│    └─────────────────────────────────────────────────────┘     │
│                                                                 │
│    ▓▓▓ = Aluminum-filled epoxy potting compound                │
│                                                                 │
│    Properties:                                                  │
│    - Drilling creates conductive aluminum debris               │
│    - Debris causes short circuits (tamper detection)           │
│    - Thermal conductivity assists heat dissipation             │
│    - Chemical resistance to common solvents                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Potting Compounds

| Product | Type | Thermal Conductivity | Notes |
|---------|------|---------------------|-------|
| **MG Chemicals 832TC** | Epoxy | 1.0 W/m·K | Good balance |
| Loctite Stycast 2651 | Epoxy | 0.8 W/m·K | Industry standard |
| Custom (Al-filled) | Epoxy | 1.5+ W/m·K | Maximum security |
| Silicone (RTV) | Silicone | 0.2 W/m·K | Reworkable (avoid) |

**Potting Process:**
1. Clean and dry PCB assembly
2. Apply conformal coat to sensitive areas
3. Place in enclosure mold
4. Mix potting compound (observe pot life)
5. Pour slowly to avoid air bubbles
6. Vacuum degas if available
7. Cure per manufacturer specs (typically 24h @ 25°C)

### Active Tamper Mesh Wrap

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLEX PCB MESH WRAP                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Flexible PCB wraps entire enclosure:                          │
│                                                                 │
│    ┌──────────────────────────────────────────────────────┐    │
│    │  ╔════════════════════════════════════════════════╗  │    │
│    │  ║░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║  │    │
│    │  ║░░  ┌──────────────────────────────────────┐  ░░║  │    │
│    │  ║░░  │                                      │  ░░║  │    │
│    │  ║░░  │         ALUMINUM ENCLOSURE           │  ░░║  │    │
│    │  ║░░  │                                      │  ░░║  │    │
│    │  ║░░  │         (with potted PCB)            │  ░░║  │    │
│    │  ║░░  │                                      │  ░░║  │    │
│    │  ║░░  └──────────────────────────────────────┘  ░░║  │    │
│    │  ║░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║  │    │
│    │  ╚════════════════════════════════════════════════╝  │    │
│    └──────────────────────────────────────────────────────┘    │
│                                                                 │
│    ░░░ = Flex PCB with continuous security mesh                │
│    ═══ = Connection to internal tamper supervisor              │
│                                                                 │
│  Flex PCB specifications:                                       │
│    - Material: Polyimide (Kapton)                              │
│    - Thickness: 0.1mm                                          │
│    - Copper: 18µm (0.5 oz)                                     │
│    - Mesh trace: 0.1mm width, 0.2mm spacing                    │
│    - Wraps all 6 sides of enclosure                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### EMI Shielding (Optional)

For maximum security, add Faraday cage:

```
Construction options:

1. Conductive paint (internal)
   - Nickel or copper-based paint
   - Apply to internal surfaces
   - Connect to ground

2. EMI gasket (at seams)
   - Beryllium copper fingers
   - Conductive foam
   - Spiral wound gasket

3. Metal mesh (external)
   - Fine copper or stainless mesh
   - Soldered to ground
   - Under outer shell

Effectiveness: >60dB attenuation at 1GHz
```

---

## Tamper Evidence

### Visual Indicators

```
┌─────────────────────────────────────────────────────────────────┐
│                    TAMPER-EVIDENT FEATURES                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. HOLOGRAPHIC SEALS                                           │
│     ┌────────────────────┐                                      │
│     │ ✦ SIMPLEGO ✦      │  Serial number visible               │
│     │   ══════════      │  Void pattern if removed             │
│     │   SN: 00001234    │  Unique per device                   │
│     └────────────────────┘                                      │
│                                                                 │
│  2. SERIALIZED SCREWS                                           │
│     Laser-engraved serial on screw heads                       │
│     Recorded in device memory at manufacture                   │
│     Visual mismatch = tampering                                │
│                                                                 │
│  3. ULTRASONIC WELDING (Tier 3)                                │
│     Plastic housing permanently bonded                         │
│     Cannot be opened without visible damage                    │
│     Used in combination with metal shell                       │
│                                                                 │
│  4. GLITTER NAIL POLISH (DIY)                                  │
│     Random glitter pattern over screw heads                    │
│     Photograph pattern at setup                                │
│     Compare before use                                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Cryptographic Verification (Tier 3)

```
┌─────────────────────────────────────────────────────────────────┐
│              CRYPTOGRAPHIC ENCLOSURE VERIFICATION               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  At manufacture:                                                │
│                                                                 │
│  1. Record in SE:                                               │
│     - Screw serial numbers                                     │
│     - Hologram serials                                         │
│     - Enclosure serial                                         │
│     - Hash of photo (optional)                                 │
│                                                                 │
│  2. Sign with device identity key                              │
│                                                                 │
│  At verification:                                               │
│                                                                 │
│  1. Device displays expected serials                           │
│  2. User visually compares                                     │
│  3. Device confirms data is authentic (signed)                 │
│  4. Any mismatch = potential tampering                         │
│                                                                 │
│  Cannot be forged without:                                      │
│  - Access to identity private key (in SE)                      │
│  - Matching physical serials                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Ergonomics and Usability

### Handheld Form Factor

```
┌─────────────────────────────────────────────────────────────────┐
│                    ERGONOMIC GUIDELINES                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Dimensions (T-Deck style):                                     │
│    Width:  110-130mm (comfortable grip)                        │
│    Height: 70-85mm                                             │
│    Depth:  15-25mm                                             │
│                                                                 │
│  Weight targets:                                                │
│    Tier 1: <150g                                               │
│    Tier 2: <250g                                               │
│    Tier 3: <400g                                               │
│                                                                 │
│  Features:                                                      │
│    - Rounded edges (R3mm minimum)                              │
│    - Textured grip areas                                       │
│    - Lanyard attachment point                                  │
│    - One-handed operation possible                             │
│                                                                 │
│  Hand grip zones:                                               │
│                                                                 │
│    ┌─────────────────────────────────────┐                     │
│    │        DISPLAY AREA                 │                     │
│    │                                     │                     │
│    ├─────────────────────────────────────┤                     │
│    │░░░░│    KEYBOARD AREA    │░░░░│                          │
│    │░░░░│                     │░░░░│  ░░ = Grip zones         │
│    │░░░░│                     │░░░░│                          │
│    └─────────────────────────────────────┘                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Pager Form Factor

```
┌─────────────────────────────────────────────────────────────────┐
│                    PAGER FORM FACTOR                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Dimensions:                                                    │
│    Width:  60-80mm                                             │
│    Height: 90-110mm                                            │
│    Depth:  12-18mm                                             │
│                                                                 │
│  Features:                                                      │
│    - Belt clip attachment                                      │
│    - Lanyard hole                                              │
│    - Minimal controls (encoder + 2-3 buttons)                  │
│    - E-Ink display (always visible)                            │
│                                                                 │
│    Front view:          Side view:                              │
│    ┌───────────┐        ┌────┐                                 │
│    │           │        │    │                                 │
│    │  E-INK    │        │    │ ← Belt clip                     │
│    │  DISPLAY  │        │    │                                 │
│    │           │        │    │                                 │
│    ├───────────┤        │    │                                 │
│    │ ○   ◎   ○ │        └────┘                                 │
│    └───────────┘                                               │
│     Buttons  Encoder                                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Thermal Management

### Heat Dissipation

```
┌─────────────────────────────────────────────────────────────────┐
│                    THERMAL DESIGN                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Heat sources:                                                  │
│    - MCU: 0.3-0.8W (peak during crypto)                        │
│    - WiFi: 0.2-0.5W (during TX)                                │
│    - LoRa: 0.5-1.0W (during TX, if 1W PA)                      │
│    - Display: 0.1-0.3W (backlight)                             │
│    - Total peak: ~2-3W                                         │
│                                                                 │
│  Thermal path:                                                  │
│                                                                 │
│    ┌─────────┐                                                 │
│    │   MCU   │                                                 │
│    └────┬────┘                                                 │
│         │ Thermal pad                                          │
│         ▼                                                       │
│    ┌─────────┐                                                 │
│    │   PCB   │  (2oz copper, thermal vias)                     │
│    └────┬────┘                                                 │
│         │ Thermal interface material                           │
│         ▼                                                       │
│    ┌─────────┐                                                 │
│    │ ENCLOSURE│  (aluminum = heatsink)                         │
│    └────┬────┘                                                 │
│         │ Convection                                           │
│         ▼                                                       │
│       AIR                                                       │
│                                                                 │
│  Design rules:                                                  │
│    - Place hot components near enclosure contact points        │
│    - Use thermal vias under QFN/BGA (3×3 array minimum)        │
│    - 0.5mm thermal pad between PCB and enclosure               │
│    - Aluminum enclosure acts as heatsink (Tier 2+)             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Connector and Port Placement

### Standard Layout

```
┌─────────────────────────────────────────────────────────────────┐
│                    PORT PLACEMENT                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  TOP VIEW:                                                      │
│    ┌─────────────────────────────────────┐                     │
│    │              DISPLAY                │                     │
│    │                                     │                     │
│    └─────────────────────────────────────┘                     │
│                                                                 │
│  RIGHT SIDE:          BOTTOM:            LEFT SIDE:             │
│    ┌───┐              ┌─────────┐         ┌───┐                │
│    │   │              │         │         │   │                │
│    │ ○ │ ← Power      │ ▭ USB-C │         │ ○ │ ← Headphone    │
│    │   │   button     │         │         │   │    (optional)  │
│    │   │              │ ▯ SD    │         │   │                │
│    │ ○ │ ← Volume     │  Card   │         │   │                │
│    │   │   (optional) │         │         │   │                │
│    └───┘              └─────────┘         └───┘                │
│                                                                 │
│  USB-C on bottom:                                               │
│    - Allows charging while standing                            │
│    - Cable doesn't interfere with grip                         │
│    - Access while in case/holster                              │
│                                                                 │
│  Avoid:                                                         │
│    - Ports on top (blocks display)                             │
│    - Ports on back (blocks grip)                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Waterproofing Ports (IP67)

```
USB-C waterproofing options:

1. Port plug (included)
   - Silicone plug attached by tether
   - User removes to charge
   - IP67 when inserted

2. Sealed magnetic connector
   - Magnetic pogo pins
   - Gasket around connector
   - Always sealed

3. Wireless charging only
   - No exposed ports
   - Qi charging coil internal
   - Maximum sealing
```

---

## Bill of Materials (Enclosure)

### Tier 1 BOM

| Item | Description | Source | Cost |
|------|-------------|--------|------|
| Top shell | PETG 3D print | Self/service | €3-5 |
| Bottom shell | PETG 3D print | Self/service | €3-5 |
| Screws | M2×8 Phillips (×4) | Hardware store | €1 |
| Inserts | M2 brass heat-set (×4) | AliExpress | €1 |
| Feet | Silicone bumpers (×4) | Amazon | €2 |
| **Total** | | | **€10-14** |

### Tier 2 BOM

| Item | Description | Source | Cost |
|------|-------------|--------|------|
| Top shell | 6061-T6 CNC | Xometry/PCBWay | €40-60 |
| Bottom shell | 6061-T6 CNC | Xometry/PCBWay | €40-60 |
| Anodizing | Black Type II | Included | - |
| Gasket | Silicone O-ring | McMaster-Carr | €5 |
| Screws | Torx Pin M2.5×8 (×6) | Security Fasteners | €8 |
| Inserts | M2.5 brass threaded (×6) | DigiKey | €3 |
| Tamper seals | Holographic (×2) | Custom print | €5 |
| **Total** | | | **€101-141** |

### Tier 3 BOM

| Item | Description | Source | Cost |
|------|-------------|--------|------|
| Outer shell | 7075-T6 CNC | Protolabs | €80-120 |
| Inner frame | 6061-T6 CNC | Protolabs | €40-60 |
| Hard anodize | Type III | Included | - |
| Flex mesh PCB | Custom 6-side wrap | PCBWay flex | €30-50 |
| Potting compound | Al-filled epoxy, 200g | Specialized | €20 |
| Gaskets | EPDM, multi-point | Custom | €15 |
| Security screws | Custom + serialized | Specialty | €20 |
| Tamper seals | Crypto-verified | Custom | €10 |
| **Total** | | | **€215-295** |

---

## File Deliverables

### Complete Enclosure Package

```
enclosures/
├── tier1_diy/
│   ├── cad/
│   │   ├── simplego_diy.step       # Universal CAD
│   │   ├── simplego_diy.f3d        # Fusion 360
│   │   └── simplego_diy.FCStd      # FreeCAD
│   ├── stl/
│   │   ├── top_shell.stl
│   │   ├── bottom_shell.stl
│   │   └── button_caps.stl
│   ├── drawings/
│   │   └── assembly.pdf
│   └── README.md
│
├── tier2_secure/
│   ├── cad/
│   │   ├── simplego_secure.step
│   │   └── simplego_secure.f3d
│   ├── drawings/
│   │   ├── top_shell_drawing.pdf
│   │   ├── bottom_shell_drawing.pdf
│   │   └── gasket_drawing.pdf
│   ├── manufacturing/
│   │   └── cnc_notes.md
│   └── README.md
│
├── tier3_vault/
│   ├── cad/
│   │   └── ... (NDA may apply)
│   ├── flex_pcb/
│   │   └── tamper_mesh_wrap.kicad_pcb
│   └── README.md
│
└── common/
    ├── hardware/
    │   ├── screws.md
    │   ├── inserts.md
    │   └── gaskets.md
    └── suppliers.md
```

---

## Design Checklist

### Before Manufacturing

- [ ] PCB dimensions match enclosure cavity
- [ ] Display cutout aligned with PCB
- [ ] Button/switch cutouts correct size and position
- [ ] USB-C port accessible
- [ ] Antenna keepout area clear
- [ ] Mounting holes align with PCB
- [ ] Gasket groove dimensions correct
- [ ] Thermal pad contact points defined
- [ ] Assembly sequence documented
- [ ] Tolerance analysis complete

### For Security (Tier 2+)

- [ ] Tamper seal locations defined
- [ ] Screw serials can be recorded
- [ ] Light sensor window designed
- [ ] Mesh flex PCB fits
- [ ] Potting volume calculated
- [ ] No external probe points
- [ ] EMI considerations addressed
