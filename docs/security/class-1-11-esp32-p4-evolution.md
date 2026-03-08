---
title: "Class 1 - ESP32-P4 Evolution Path"
sidebar_position: 12
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# ESP32-P4 Evolution Path

**Document version:** Session 44 | March 2026
**Applies to:** Guition JC4880P443C_I_W (ESP32-P4 + ESP32-C6 dual-chip board)
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Why the ESP32-P4

The ESP32-P4 is the next-generation Espressif microcontroller, representing a significant upgrade over the ESP32-S3 in three areas that matter for SimpleGo: raw performance (400 MHz RISC-V vs 240 MHz Xtensa), memory (768 KB SRAM + 32 MB PSRAM vs 512 KB + 8 MB), and security features (Key Management Unit, Digital Signature Peripheral, anti-DPA countermeasures, Power Glitch Detector).

The ESP32-P4 has no integrated WiFi or Bluetooth. It is designed as a high-performance application processor that pairs with a wireless companion chip (ESP32-C6 for WiFi 6 + Bluetooth 5.0) over SDIO or SPI. This dual-chip architecture mirrors SimpleGo's security principle of functional separation: the application processor handles computation and cryptography, the wireless chip handles connectivity, and neither has full access to the other's internal state.

SimpleGo has the Guition JC4880P443C_I_W evaluation board on order. This document analyzes the security implications and the firmware migration path.

---

## Security Feature Comparison

### Key Management Unit (KMU)

The ESP32-P4's most significant security addition. The KMU manages cryptographic key material entirely within a hardware security perimeter, ensuring that keys used for AES, HMAC, and Digital Signature operations are never exposed to software in plaintext.

On the ESP32-S3, when the HMAC peripheral derives NVS encryption keys, those derived keys are returned to application RAM. Malicious firmware (without Secure Boot) can read them. On the ESP32-P4, the KMU can route derived keys directly to the crypto accelerators without passing through software-accessible registers. The firmware requests "encrypt this data with the key in slot N" and receives the result, but never sees the key itself.

**Impact for SimpleGo:** In Mode 2 (NVS Vault), the KMU eliminates the "derived keys in RAM" weakness that currently requires Secure Boot (Mode 4) to mitigate on the ESP32-S3. This means Mode 2 on the P4 provides security comparable to Mode 4 on the S3, without the manufacturing complexity of Secure Boot provisioning.

### Anti-DPA Pseudo-Rounds

The ESP32-P4's AES/XTS-AES hardware includes configurable pseudo-round insertion, controlled by the XTS_DPA_PSEUDO_LEVEL eFuse. When enabled, the AES engine randomly inserts dummy encryption rounds before and after the real rounds, using a pseudo-key that does not affect the computation result. This randomizes the power consumption profile, making Correlation Power Analysis (CPA) significantly harder because the attacker cannot reliably identify which power samples correspond to real AES operations.

The strength is configurable (higher levels = more dummy rounds = more security = slower encryption). ESP-IDF provides the CONFIG_MBEDTLS_AES_USE_PSEUDO_ROUND_FUNC_STRENGTH option to select the level.

**Important caveat:** The ESP32-C6 had similar anti-DPA countermeasures that researcher Kevin Courdesses proved ineffective in 2024. The masking had minimal impact, and the clock randomization was defeatable. Whether the P4's implementation is more robust is unknown - no independent security evaluation has been published. SimpleGo's documentation must treat this as "raising the bar" rather than "solving the problem" until independent validation exists.

### Power Glitch Detector

The ESP32-P4 includes a hardware Power Glitch Detector that monitors the power supply for anomalous voltage fluctuations characteristic of fault injection attacks. When a glitch is detected, the chip can trigger a response (reset, alarm, or custom handler).

This directly mitigates the attack class demonstrated by LimitedResults (voltage glitching on ESP32 V1/V2) and partially mitigates electromagnetic fault injection (which induces voltage fluctuations via electromagnetic coupling). The effectiveness against sophisticated EMFI with carefully shaped pulses (as used by Delvaux/TII at WOOT 2024) is uncertain.

### Digital Signature Peripheral

The ESP32-P4's Digital Signature (DS) peripheral produces hardware-accelerated RSA digital signatures using a private key that is derived from an HMAC key in eFuse. The private key is never accessible to software. This enables hardware-backed TLS client authentication - the device can prove its identity to a server (or to SimpleGo's App Store) without ever exposing the authentication key.

**Impact for SimpleGo App Store:** A P4-based device can authenticate to simplego.dev with a hardware-backed certificate that is unforgeable without physical access to the specific chip. This is stronger than software-based license keys and enables per-device authentication without a central database of shared secrets.

### ECDSA Secure Boot

The ESP32-P4 supports ECDSA-256 and ECDSA-384 for Secure Boot in addition to RSA-3072. ECDSA produces smaller signatures (64-96 bytes vs 384 bytes for RSA-3072), faster verification, and smaller key sizes. For SimpleGo, this primarily matters for boot speed (marginal improvement) and eFuse efficiency (ECDSA digests are smaller, potentially allowing more keys in fewer blocks).

---

## Hardware Specifications

| Property | ESP32-S3 (current) | ESP32-P4 (planned) | Improvement |
|----------|-------------------|-------------------|-------------|
| CPU architecture | Dual Xtensa LX7 | Dual RISC-V (RV32IMAFCZc) | Open ISA, better tooling |
| Clock speed | 240 MHz | 400 MHz | 1.67x faster |
| Internal SRAM | 512 KB | 768 KB | 1.5x more |
| PSRAM | 8 MB (external SPI) | 32 MB (in-package) | 4x more, lower latency |
| TCM (Tightly Coupled Memory) | None | 8 KB zero-wait | Ultra-fast crypto buffers |
| WiFi/BT | Integrated | External (ESP32-C6 via SDIO) | Security: functional separation |
| Display interface | SPI (ST7789V) | MIPI-DSI + parallel | Higher resolution possible |
| USB | USB-OTG 1.1 FS | USB OTG 2.0 HS | Faster firmware upload |
| GPIO count | 45 | 55 | More expansion options |
| Key Management Unit | No | Yes | Keys never in software RAM |
| Digital Signature | No | Yes | Hardware TLS client auth |
| Anti-DPA | No | Yes (configurable) | Harder side-channel analysis |
| Power Glitch Detect | No | Yes | Fault injection detection |
| Secure Boot | RSA-3072 only | RSA-3072 + ECDSA-256/384 | More algorithm options |

---

## The Dual-Chip Architecture

The Guition board pairs the ESP32-P4 (application processor) with an ESP32-C6 (wireless controller) connected via SDIO interface. This creates a natural security boundary:

```
ESP32-P4 (Application Processor)
  - All cryptographic operations
  - Key management (KMU)
  - NVS encryption (HMAC)
  - Double Ratchet, NaCl layers
  - UI rendering (LVGL)
  - SD card access
  - eFuse key storage
      |
      | SDIO bus (data only, no DMA to P4 internal memory)
      |
ESP32-C6 (Wireless Controller)
  - WiFi 6 (802.11ax)
  - Bluetooth 5.0 LE
  - TLS termination (optional, or pass-through to P4)
  - No access to P4's NVS, eFuse, or internal SRAM
```

The ESP32-C6 runs ESP-AT or ESP-Hosted firmware, acting as a network interface. It handles the WiFi association, TCP/IP stack, and optionally TLS. The P4 sends plaintext commands ("connect to this server", "send this data") and receives responses. The C6 never sees decrypted SimpleX message content because all four encryption layers are processed on the P4.

**Security advantage:** Even if the ESP32-C6 is fully compromised (firmware replaced, all traffic intercepted), the attacker sees only TLS-encrypted data destined for SMP servers. The three inner encryption layers (Double Ratchet, Per-Queue NaCl, Server-to-Recipient NaCl) are applied by the P4 before data reaches the C6. This is architectural isolation, not software isolation - the C6 physically cannot access the P4's memory.

---

## Firmware Migration Path

SimpleGo's C codebase (21,863 lines across 47 files) is designed to be portable across ESP32 variants. The Hardware Abstraction Layer (HAL) in `devices/t_deck_plus/hal_impl/` encapsulates all hardware-specific code:

| HAL Component | ESP32-S3 (T-Deck Plus) | ESP32-P4 (Guition) | Migration Effort |
|--------------|----------------------|-------------------|-----------------|
| Display driver | SPI, ST7789V, 320x240 | MIPI-DSI or parallel, TBD resolution | New driver needed |
| Keyboard | I2C BB Q20 at 0x55 | TBD (board-dependent) | New driver if different input |
| Touch | Capacitive I2C | TBD | New driver |
| Backlight | GPIO 42, pulse protocol | TBD | Simple GPIO change |
| WiFi | Internal esp_wifi API | SDIO to ESP32-C6 (ESP-Hosted) | Significant API change |
| SD card | SPI, shared SPI2 bus | SDIO 3.0 (dedicated) | Driver change, faster I/O |
| LVGL | LVGL v9 (unchanged) | LVGL v9 (unchanged) | No change |
| Crypto (mbedTLS) | Software AES with HW accel | Software AES with HW accel + KMU | Config changes for KMU |
| Crypto (libsodium) | Standard build | Standard build | No change |
| Crypto (wolfSSL) | X448 standard | X448 standard | No change |
| NVS | nvs_flash_secure_init | nvs_flash_secure_init | No change (same HMAC scheme) |
| eFuse | Same API, same block structure | Same API, same block structure | No change |

The core protocol code (`protocol/`, `state/`, `core/`) requires zero changes. The UI code (`ui/`) requires zero changes (LVGL abstracts the display). Only the HAL implementation layer needs a new device target.

**Estimated migration effort:** 2-3 weeks for a new `devices/guition_p4/hal_impl/` directory with display, input, and WiFi drivers. The cryptographic and protocol code is identical.

---

## What the P4 Does NOT Fix

The ESP32-P4 does not have a dedicated certified Secure Element. Its key storage is still eFuse-based. While the KMU adds a layer of key isolation, the eFuse key material is still physically present in the silicon and potentially extractable via advanced side-channel analysis. The P4 is a better Class 1 - it is not a replacement for the dedicated ATECC608B/OPTIGA/SE050 secure elements planned for Class 2 and 3.

The P4's anti-DPA countermeasures are unproven against independent security research. The ESP32-C6 had similar features that were broken. Until a researcher publishes a successful or failed attack against the P4's specific implementation, the security improvement should be described as "expected" rather than "proven."

The P4 also does not fix the protocol-level concerns: post-quantum readiness still requires software implementation (sntrup761 or ML-KEM), and runtime memory protection still requires sodium_memzero() discipline in the firmware.

---

## Marketing Positioning

The ESP32-P4 variant of SimpleGo Class 1 can be positioned as:

"Same firmware, same protocol, same four encryption layers. Faster processor (400 MHz vs 240 MHz), more memory (32 MB vs 8 MB), and hardware security features that make side-channel attacks significantly harder: a Key Management Unit that keeps your keys invisible even to the firmware, a Power Glitch Detector that fights fault injection attacks, and anti-DPA countermeasures in the encryption hardware. All of this in a $15 chip."

This is an honest, verifiable claim. The "significantly harder" is defensible based on the hardware features, even without proven effectiveness, because any additional countermeasure increases attacker cost. The claim does not say "impossible to break."

---

## References

| Source | Description |
|--------|-------------|
| espressif.com/en/products/socs/esp32-p4 | Official P4 product page |
| ESP32-P4 preliminary datasheet v0.5 | Hardware specifications |
| ESP-IDF v5.5 Security Overview (ESP32-P4) | P4 security feature documentation |
| ESP-IDF Kconfig reference (ESP32-P4) | XTS_DPA_PSEUDO_LEVEL, anti-DPA configuration |
| Espressif ESP32-H2 v1.2 security upgrade blog | First chip with anti-DPA pseudo-rounds (reference) |
| Courk 2024 (courk.cc) | ESP32-C6 DPA countermeasures proven ineffective |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
