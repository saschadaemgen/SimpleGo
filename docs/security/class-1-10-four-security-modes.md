---
title: "Class 1 - Four Security Modes"
sidebar_position: 10
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Four Security Modes

**Document version:** Session 44 | March 2026
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Overview

Hardware Class 1 supports four progressive security modes on identical hardware and firmware. Each mode builds on the previous by burning additional eFuse blocks. The transition from a lower mode to a higher mode is always possible; downgrading is never possible because eFuse burns are permanent.

| Mode | Name | eFuse Blocks Used | Web Flash | USB Flash | OTA | Provisioned By |
|------|------|-------------------|-----------|-----------|-----|----------------|
| 1 | Open | 0 | Yes | Yes | Yes | Nobody |
| 2 | NVS Vault | 1 | Yes | Yes | Yes | Device (first boot) |
| 3 | Fortress | 2-3 | No | No | Yes | Manufacturer |
| 4 | Bunker | 3-4 | No | No | Signed only | Manufacturer |

---

## Mode 1: Open

**Target audience:** Developers, security researchers, community alpha testers.
**Product tier:** Community Edition (free, open source).

### What is active

Nothing beyond the base firmware. No eFuse blocks consumed. No encryption at rest. No firmware signing. JTAG available for debugging. Full flash read/write access via USB. All standard ESP-IDF development tools work without restriction.

### What is protected

The four SimpleX Protocol encryption layers protect all messages in transit. SD card chat history is encrypted with AES-256-GCM (this is firmware-level encryption, independent of eFuse state). WiFi credentials are stored in NVS but not encrypted.

### What is NOT protected

All private keys (ratchet state, queue keys, handshake keys) are stored in plaintext in NVS flash. A $15 SPI flash reader extracts everything in 5 minutes. RAM contents are accessible via JTAG. Firmware can be replaced with any binary.

### Transition to Mode 2

Automatic on next boot when firmware is built with NVS encryption config enabled. No user action required beyond flashing the updated firmware. The device generates its own HMAC key and burns it to eFuse at first boot.

---

## Mode 2: NVS Vault

**Target audience:** Alpha release users, Kickstarter backers, privacy-conscious community members.
**Product tier:** Kickstarter Standard (device price + free firmware).

### What is active

One eFuse block (BLOCK_KEY1) contains a device-unique HMAC key. NVS partition is encrypted with XTS-AES-256 derived from this key at runtime. The HMAC key is read-protected (invisible to software) and write-protected (cannot be modified).

### What is protected

All cryptographic keys in NVS are encrypted at rest. Flash chip readout (the most common and cheapest physical attack) yields only encrypted data. Each device has a unique key - no master key exists, no fleet-wide compromise is possible. SD card history remains encrypted (independent of eFuse state). All four transit encryption layers active.

### What is NOT protected

Firmware is unencrypted in flash (readable but open source anyway). Firmware can be replaced (no signature verification). JTAG may still be available. The derived NVS encryption keys exist in RAM during operation and are accessible to any firmware running on the device. Side-channel analysis with laboratory equipment ($2,000+) can potentially extract the eFuse HMAC key.

### Web Flash compatibility

Full compatibility. The firmware binary is not encrypted. Users visit simplego.dev/installer, connect via USB, flash. On first boot, the vault activates automatically. No special tooling, no manufacturing step.

### idf.py erase-flash behavior

Erases NVS (all stored keys lost - factory reset). Does NOT erase eFuse key. Next boot creates fresh encrypted NVS with same hardware key. Safe for development.

### Required build command after enabling NVS encryption

`idf.py erase-flash` then `idf.py build flash monitor -p COM6` (erase-flash required because NVS format changes from plaintext to encrypted).

### Transition to Mode 3

Requires manufacturer provisioning via espefuse.py. The flash encryption key must be burned and flash encryption enabled. This is a one-way transition - once the flash is encrypted and the CRYPT_CNT eFuse is set to permanent (0b111), web flash and direct USB flash no longer work.

---

## Mode 3: Fortress

**Target audience:** Pre-configured devices, users who want firmware confidentiality, production deployments.
**Product tier:** Kickstarter Premium or direct purchase from IT and More Systems.

### What is active

Everything in Mode 2, plus: Flash encryption (AES-128-XTS or AES-256-XTS) protecting the entire firmware binary, partition table, and bootloader in flash. PSRAM encryption automatically enabled. Two to three eFuse blocks consumed (1 HMAC + 1-2 Flash Encryption).

### What is protected

Everything in Mode 2, plus: Firmware cannot be read from flash (encrypted). Firmware cannot be modified in flash (modification produces garbage after decryption). PSRAM contents are hardware-encrypted (additional protection for cached messages). The combination of NVS encryption (HMAC) and flash encryption (AES-XTS) creates two independent encryption layers for stored keys.

### What is NOT protected

Firmware is not signature-verified - the device does not check WHO encrypted the firmware, only that it is correctly encrypted. An attacker with the flash encryption key (obtained via side-channel analysis) could encrypt and flash malicious firmware. JTAG may still be available unless explicitly disabled via eFuse.

### Web Flash compatibility

**Not compatible.** Web flash tools cannot perform encrypted writes. Firmware must be pre-encrypted with the device-specific key during manufacturing, or the device must be flashed in Development mode first (consuming a CRYPT_CNT cycle) before being locked to Release mode.

### Update path

OTA only. The device downloads firmware over HTTPS, and the flash hardware encrypts during write. No serial update possible in Release mode.

### Manufacturing workflow

```
1. Flash plaintext firmware via esptool
2. Burn HMAC key (if not already present from Mode 2)
3. Burn Flash Encryption key
4. Enable Flash Encryption
5. First boot: bootloader encrypts flash in-place (~60 seconds)
6. Verify device boots and functions correctly
7. Test OTA update cycle
8. Ship device
```

### Transition to Mode 4

Requires burning Secure Boot digest(s) and enabling SECURE_BOOT_EN eFuse. Must be done AFTER flash encryption is fully enabled and tested (eFuse ordering constraint).

---

## Mode 4: Bunker

**Target audience:** Journalists, activists, organizations requiring maximum protection, the Vault product line.
**Product tier:** SimpleGo Vault (premium pricing, manufactured and configured by IT and More Systems).

### What is active

Everything in Mode 3, plus: Secure Boot V2 (RSA-3072) ensuring only firmware signed with IT and More Systems' private key executes. JTAG permanently disabled. USB download mode permanently disabled. Anti-rollback protection via SECURE_VERSION eFuse. Three to four eFuse blocks consumed.

### What is protected

Everything in Mode 3, plus: No unsigned firmware can execute (Secure Boot). No firmware downgrade possible (anti-rollback). No debug access (JTAG disabled). No serial recovery (download mode disabled). The derived NVS encryption keys in RAM are now protected because only trusted firmware (which does not leak them) can run. This closes the "malicious firmware reads RAM" gap that exists in Modes 1-3.

### What is NOT protected

Side-channel analysis can still extract the flash encryption key and potentially the HMAC key. Electromagnetic fault injection may bypass the boot verification (as demonstrated on ESP32-V3 at WOOT 2024). State-level adversaries with sustained physical access and laboratory equipment can compromise the device. These limitations are inherent to the ESP32-S3 silicon and are addressed in Hardware Class 2 and 3 with dedicated secure elements.

### Web Flash compatibility

**Not compatible.** No serial interface available for flashing.

### Update path

Signed OTA only. The firmware must be signed with the RSA-3072 private key corresponding to one of the three provisioned digest slots. Unsigned or incorrectly signed firmware is rejected.

### Manufacturing workflow

```
1-7. Same as Mode 3
8. Burn Secure Boot digests for all 3 key slots
9. Enable Secure Boot (SECURE_BOOT_EN)
10. Burn lockdown eFuses:
    - DIS_PAD_JTAG = 1
    - DIS_USB_JTAG = 1
    - DIS_DOWNLOAD_MODE = 1 (or enable Secure Download Mode)
11. Test signed OTA update end-to-end
12. Test anti-rollback (attempt to flash older version, verify rejection)
13. Final espefuse.py summary verification
14. Ship device
```

### The signing key responsibility

Losing all three signing keys means the device can never be updated. This is an acceptable tradeoff for maximum security (no backdoor update path exists), but requires rigorous key management. See [Secure Boot V2](./class-1-09-secure-boot-v2.md) for detailed key storage and rotation procedures.

---

## Mode Comparison Matrix

| Feature | Mode 1 | Mode 2 | Mode 3 | Mode 4 |
|---------|--------|--------|--------|--------|
| NVS key encryption | No | **Yes** | **Yes** | **Yes** |
| Firmware encryption | No | No | **Yes** | **Yes** |
| PSRAM encryption | No | No | **Yes** | **Yes** |
| Firmware signing | No | No | No | **Yes** |
| JTAG available | Yes | Yes | Configurable | **Disabled** |
| USB flash | Yes | Yes | No | **No** |
| Web flash | Yes | Yes | No | **No** |
| OTA updates | Yes | Yes | Yes | **Signed only** |
| Anti-rollback | No | No | No | **Yes** |
| Flash readout protection | No | NVS only | **Full** | **Full** |
| Cost to read keys (flash) | $15 | $2,000+ | $2,000+ | $2,000+ |
| Cost to run malicious FW | $0 | $0 | $2,000+ (need FE key) | **Impossible** (need signing key) |
| Reversible | Yes | Partially | No | **No** |
| Provisioned by | Nobody | Device (auto) | Manufacturer | Manufacturer |

---

## Business Model Alignment

| Mode | Product | Distribution | Revenue Model |
|------|---------|-------------|--------------|
| 1 | Community Edition | GitHub source, self-build | None (community building) |
| 2 | Alpha/Kickstarter Standard | Web flash on simplego.dev | Device sale + Kickstarter pledge |
| 3 | Pre-configured devices | Shipped by IT and More Systems | Device sale + premium margin |
| 4 | Vault Edition | Shipped by IT and More Systems | Premium product, highest margin |

Mode 1 builds the community. Mode 2 is the Kickstarter revenue driver. Modes 3 and 4 are the sustainable business - devices that require manufacturing-time provisioning create a natural barrier that justifies premium pricing while providing genuine security value.

The SimpleGo App Store (planned, HTTPS-based on simplego.dev) works across all modes: plugins are distributed as signed binaries bound to the device's eFuse chip ID. In Modes 1 and 2, the binding relies on software verification (bypassable by a skilled user). In Modes 3 and 4, the binding is hardware-enforced (Secure Boot prevents firmware modification to bypass checks).

---

## References

| Source | Description |
|--------|-------------|
| [eFuse Architecture](./class-1-02-efuse-architecture.md) | eFuse budget and block allocation per mode |
| [HMAC NVS Encryption](./class-1-03-hmac-nvs-encryption.md) | Mode 2 vault mechanism |
| [Flash Encryption](./class-1-08-flash-encryption.md) | Mode 3 firmware encryption |
| [Secure Boot V2](./class-1-09-secure-boot-v2.md) | Mode 4 firmware signing |
| [Attack Equipment Economics](./class-1-05-attack-equipment-economics.md) | Cost-to-break per mode |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
