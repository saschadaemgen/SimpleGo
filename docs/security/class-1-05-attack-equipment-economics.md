---
title: "Class 1 - Attack Equipment Economics"
sidebar_position: 6
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Attack Equipment Economics

**Document version:** Session 44 | March 2026
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Why This Document Exists

Security claims without cost context are meaningless. Saying "requires physical access and laboratory equipment" could mean anything from a $15 SPI reader to a $400,000 certification lab. This document provides concrete, verifiable price points for every tool referenced in SimpleGo's threat model, enabling users, investors, and security researchers to make their own informed assessment of SimpleGo Class 1's practical security level.

All prices are based on publicly available retail listings, published research papers, and manufacturer product pages as of early 2026. Where exact prices are unavailable, ranges are given with sources noted.

---

## The Baseline: Attacking SimpleGo WITHOUT the Vault

Before the HMAC vault is enabled (Mode 1, current Alpha state), attacking SimpleGo's stored keys requires:

| Item | Price | Source |
|------|-------|--------|
| CH341A USB SPI flash programmer | $5-15 | AliExpress, Amazon |
| SOIC-8 test clip (no desoldering needed) | $3-8 | AliExpress |
| flashrom or esptool.py software | Free | Open source |

**Total: $8-23. Time: 5 minutes. Skill: Beginner.**

The attacker clips onto the SPI flash chip, runs `esptool.py read_flash`, and dumps the entire NVS partition in plaintext. All ratchet keys, queue keys, handshake state, and WiFi credentials are immediately readable. No specialized knowledge required - there are YouTube tutorials.

This is the attack that the HMAC vault (Mode 2) is specifically designed to prevent.

---

## Attacking SimpleGo WITH the Vault: Four Tiers of Equipment

### Tier 1: Hobbyist / Student ($400-600)

The minimum viable setup for electromagnetic fault injection experiments. Not sufficient for reliable key extraction from ESP32-S3, but capable of learning the techniques and potentially achieving results with significant time investment.

| Item | Model | Price | Purpose |
|------|-------|-------|---------|
| EMFI tool | PicoEMP (DIY from open-source design) | $50-60 | EM pulse generation, 310V fixed |
| EMFI tool (alt) | Electronic Cats Faulty Cat v2.1 | $120 | EMFI + voltage glitch + SWD/JTAG detect |
| Capture board | ChipWhisperer-Nano | $250 | Basic power analysis, 10-bit ADC |
| Target boards | 5x ESP32-S3-DevKitC | $50 | Practice and parameter tuning |
| Accessories | Probes, wires, clips | $30-50 | Physical connections |

**Total: $400-530**

**Capabilities:** Basic EMFI experiments. Simple Power Analysis (SPA) to observe gross power patterns. Cannot perform high-resolution CPA needed for AES key extraction (ADC resolution and sample rate too low). Suitable for learning, not for production attacks against ESP32 security features.

**Time to extract ESP32-S3 HMAC key:** Unknown. Likely impossible with this equipment due to insufficient measurement resolution. PicoEMP has demonstrated successful fault injection against simpler targets but published ESP32 attacks used more capable equipment.

### Tier 2: Security Researcher ($1,600-5,000)

The setup that could realistically reproduce the Ledger Donjon CPA attack against ESP32 AES hardware, with patience.

| Item | Model | Price | Purpose |
|------|-------|-------|---------|
| Capture + glitch | ChipWhisperer-Husky | $500-700 | 12-bit ADC, 200 MS/s, clock glitching |
| Oscilloscope | PicoScope 3206D (200 MHz) | $700-900 | Timing analysis, trigger setup |
| EMFI injector | PicoEMP or SiliconToaster (DIY) | $50-150 | EM pulse generation |
| Positioning | CNC 3018 Pro (modified for probe) | $200-300 | XYZ automated probe positioning |
| Target boards | 10x ESP32-S3-DevKitC | $100 | Parameter optimization |
| Analysis software | ChipWhisperer + Lascar (open source) | Free | CPA computation, trace analysis |
| EM probe set | Custom ferrite-core probes, 0.5-2mm | $50-100 | Focused EM injection/measurement |
| Accessories | Shunt resistors, amplifiers, connectors | $100-200 | Measurement chain |

**Total: $1,700-2,550 (basic) to $5,000 (with better oscilloscope)**

**Capabilities:** Full Correlation Power Analysis (CPA) against AES operations. ChipWhisperer-Husky's 12-bit ADC at 200 MS/s is sufficient for AES S-box leakage detection. The CNC 3018 enables automated probe positioning for systematic chip surface scanning. Lascar (Ledger Donjon's open-source SCA library) processes 100,000 traces with 10,000 samples each in approximately 25 seconds.

**Time to extract ESP32-S3 flash encryption key:** If the published Ledger Donjon methodology transfers directly (which is expected per Espressif's advisory), approximately 300,000 traces at maybe 5-10 traces per second means 8-17 hours of measurement collection, plus analysis time. With parameter optimization on practice chips, total project time is days to weeks.

**Time to extract ESP32-S3 HMAC key:** Unknown. The HMAC peripheral uses SHA-256, not AES. CPA methodology would need adaptation. No published work exists. Estimated: significantly harder, possibly weeks to months if feasible at all with this equipment.

### Tier 3: Professional Lab ($10,000-30,000)

The setup used in published ESP32 research papers and by professional hardware security assessment firms.

| Item | Model | Price | Purpose |
|------|-------|-------|---------|
| EMFI platform | NewAE ChipSHOUTER (CW520) | $3,300-4,000 | Programmable 0-500V EM injection |
| Oscilloscope | Keysight DSOX3054T (500 MHz, 5 GS/s) | $5,000-7,000 | High-bandwidth power measurement |
| Positioning | ChipShover (XYZ with 10um precision) | $500-1,000 | Automated scanning |
| Positioning (alt) | Thorlabs XYZ stage | $2,000-10,000 | Sub-micron precision |
| Current probe | Keysight N2893A or Tektronix CT1 | $1,000-2,000 | Low-noise current measurement |
| Analysis platform | ChipWhisperer-Pro or custom | $1,000-3,000 | High-speed capture and streaming |
| Target boards | 50x ESP32-S3-DevKitC | $500 | Extensive parameter exploration |
| Software | ChipWhisperer Pro + custom scripts | $0-1,000 | Full SCA toolchain |

**Total: $13,300-28,000**

**Capabilities:** Everything in Tier 2, plus: higher measurement bandwidth allows capturing faster transients, programmable EM pulse voltage enables fine-tuning for specific chip specimens, higher trace collection rate (hundreds per second with streaming), and the ability to characterize the HMAC peripheral's power signature with sufficient resolution to develop novel SHA-256 CPA attacks.

This is the equipment class used by Ledger Donjon (Tektronix MSO54 oscilloscope), Raelize (Riscure EM-FI probe), and Delvaux/TII (Teledyne LeCroy WavePro 804HD) in their published ESP32 research.

**Time to extract ESP32-S3 flash encryption key:** Hours (established methodology).
**Time to extract ESP32-S3 HMAC key:** Days to weeks (novel research required, but equipment is sufficient).

### Tier 4: Certification Lab ($100,000-400,000+)

Equipment used for Common Criteria, EMVCo, and FIPS certification testing. This is what evaluates the Secure Elements planned for Hardware Class 2 and 3.

| Item | Model | Price | Purpose |
|------|-------|-------|---------|
| SCA platform | Keysight Inspector SC4 (ex-Riscure) | $20,000-100,000+ | Industry-standard evaluation |
| Laser FI | AlphaNov/esDynamic LFI station | $50,000-200,000 | Transistor-level fault injection |
| Oscilloscope | Keysight UXR (110 GHz) or similar | $15,000-40,000 | Maximum bandwidth capture |
| EM probe station | Custom with shielded chamber | $5,000-20,000 | Controlled EM environment |
| Decapsulation | Chemical + plasma etcher | $10,000-30,000 | Die exposure for laser/probing |
| Microprobe station | Cascade or similar | $50,000-100,000+ | Direct die contact probing |

**Total: $150,000-490,000+**

**Capabilities:** Everything above, plus: laser fault injection at individual transistor level, die-level probing after chemical decapsulation, thermal analysis, photon emission analysis. This is the equipment needed to attack dedicated Secure Elements like the ATECC608B, OPTIGA Trust M, and SE050 planned for SimpleGo Hardware Class 2 and 3.

**Relevance to SimpleGo Class 1:** Massive overkill. A Tier 2 or 3 setup is sufficient to attack ESP32-S3 eFuse-based security. This tier is relevant only as context for understanding what it takes to attack the dedicated Secure Elements in higher hardware classes.

---

## The Most Important Number: Cost Ratio

| Attack Target | Equipment Cost | What You Get |
|--------------|---------------|-------------|
| SimpleGo Mode 1 (no vault) | $15 | ALL keys from ALL contacts, plaintext |
| SimpleGo Mode 2 (HMAC vault) | $2,000-5,000 | Keys from ONE device only, days of work |
| SimpleGo Mode 3 (+ Flash Enc.) | $2,000-5,000 | Keys + firmware from ONE device, days of work |
| SimpleGo Mode 4 (+ Secure Boot) | $10,000-30,000 | Keys from ONE device, needs novel EMFI to bypass boot |
| SimpleGo Class 2 (single SE) | $30,000-100,000 | Requires SE-specific research, weeks to months |
| SimpleGo Class 3 (triple SE) | $200,000+ | Must break 3 independent SEs from 3 vendors |

**The vault (Mode 2) raises the cost of attack by a factor of 130-330x and limits the blast radius from "all contacts" to "one device."** This is the core value proposition of Hardware Class 1 security: transforming a trivial $15 attack into a multi-thousand-dollar research project that yields information about exactly one device.

---

## Practice Requirements: The Hidden Cost

Published papers do not emphasize the skill development time, but it is substantial. Side-channel analysis and fault injection are not plug-and-play tools. The attacker must:

**Learn the theory.** CPA requires understanding of statistical correlation, Hamming weight models, and AES internals. This is graduate-level cryptography and electrical engineering. Textbooks, courses, and months of study for someone starting from zero.

**Practice on known targets.** The ChipWhisperer ecosystem provides tutorial targets (CW308 boards) for practicing CPA against known AES implementations with known keys. Getting a first successful key extraction on a tutorial target typically takes days of setup and experimentation. Progressing from tutorial targets to real-world ESP32 targets requires adapting probing techniques, trigger timing, and analysis parameters.

**Optimize for the specific target.** Every individual chip specimen has slightly different characteristics (manufacturing variation, board layout, bypass capacitor placement). Parameters that work on one ESP32 board may need adjustment for another. The published papers report "optimized" results - they do not report the hours of parameter tuning that preceded the successful extraction.

**A realistic estimate for a skilled hardware security researcher attacking a SimpleGo Mode 2 device for the first time:** 1-2 weeks including equipment setup, parameter optimization on practice chips, trace collection, and analysis. For a graduate student learning the techniques from scratch: months.

---

## Open-Source Attack Tools

The democratization of hardware security research means the knowledge barrier is lowering over time. All major tools have open-source implementations:

| Tool | Developer | License | Purpose |
|------|-----------|---------|---------|
| ChipWhisperer | NewAE Technology | GPLv3 | Full SCA/FI platform with Jupyter notebooks |
| PicoEMP | NewAE Technology | BSD-3 | Low-cost EMFI tool, Raspberry Pi Pico based |
| SiliconToaster | Ledger Donjon | LGPLv3 | Programmable 0-1000V EM injector |
| Faulty Cat | Electronic Cats | Open hardware | EMFI + voltage glitch + debug detection |
| Lascar | Ledger Donjon | LGPLv3 | CPA/DPA/template attacks, Python library |
| SCALib | SIMPLE-Crypto (UCLouvain) | AGPLv3 | State-of-art evaluation: LDA, SNR, belief propagation |
| Rainbow | Ledger Donjon | LGPLv3 | Fault injection simulation via Unicorn emulation |
| findus | SySS GmbH | Open source | EMFI experiment management |

The availability of these tools means that the cost barrier is primarily in the measurement hardware (oscilloscopes, probes, positioning), not in the software or methodology. A motivated researcher with $2,000 in hardware and access to published papers has a credible path to attacking ESP32-S3 eFuse security.

---

## Conclusion: Honest Positioning

SimpleGo Hardware Class 1 with the HMAC vault provides strong protection against the most common and accessible attack vector (flash readout) at zero additional hardware cost. It does not provide protection against a determined attacker with $2,000+ in equipment, physical access, and weeks of dedicated effort.

This is the honest truth, and it should be communicated clearly to users. The comparison point is not "is this unbreakable?" (nothing is) but "how much does it cost to break, and what does the attacker get?" For a $50-70 device using a $10 microcontroller, raising the attack cost from $15 to $2,000+ while limiting the result to a single device is a remarkable value proposition. And for users who need more, Hardware Class 2 and 3 exist.

---

## References

| Source | Description |
|--------|-------------|
| newae.com/products | ChipSHOUTER, ChipWhisperer-Husky, PicoEMP pricing |
| electroniccats.com/store | Faulty Cat v2.1 pricing |
| github.com/newaetech/chipshouter-picoemp | PicoEMP open-source design files |
| github.com/Ledger-Donjon/silicon-toaster | SiliconToaster design files |
| github.com/Ledger-Donjon/lascar | Lascar SCA library |
| ledger.com/blog/compact-em | Ledger Donjon EMFI setup description with CNC pricing |
| IACR 2022/301 | "How Practical are Fault Injection Attacks, Really?" (cost survey) |
| eprint.iacr.org/2023/090 | Ledger Donjon ESP32-V3 attack equipment description |
| usenix.org WOOT 2024 | Delvaux et al. equipment description |
| courk.cc | Courdesses ESP32-C3/C6 attack setup |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
