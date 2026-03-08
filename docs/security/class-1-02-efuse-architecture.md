---
title: "Class 1 - ESP32-S3 eFuse Architecture"
sidebar_position: 2
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# ESP32-S3 eFuse Architecture

**Document version:** Session 44 | March 2026
**Applies to:** ESP32-S3 (LilyGo T-Deck Plus), with comparison notes for ESP32-P4
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## What are eFuses and Why They Matter

eFuses (electronic fuses) are one-time-programmable (OTP) memory bits physically embedded in the ESP32-S3 silicon. Each eFuse is a microscopic conductor that can be permanently "blown" by passing a high current through it, transitioning the bit from 0 to 1. Once blown, the transition is irreversible - no software, no hardware reset, no power cycle can restore a blown eFuse to 0. This is not encryption that could theoretically be broken. It is physics: a conductor that has been physically destroyed.

The ESP32-S3 uses eFuses for three critical purposes in SimpleGo's security architecture. First, they store cryptographic keys (HMAC keys for NVS encryption, AES keys for flash encryption, RSA key digests for Secure Boot) in a location that can be made permanently unreadable by software. Second, they store one-time configuration bits that permanently enable or disable security features (flash encryption enable, secure boot enable, JTAG disable, download mode disable). Third, they provide a hardware root of trust - because eFuse values are checked by the ROM bootloader (code burned into the chip at manufacturing that cannot be modified), the security chain starts from immutable silicon, not from software that could be tampered with.

Understanding eFuses at the hardware level is essential for SimpleGo's security architecture because every irreversible decision we make - burning an HMAC key, enabling Secure Boot, disabling JTAG - consumes eFuse resources that cannot be recovered. A mistake in eFuse provisioning cannot be undone. This document provides the complete reference for making those decisions correctly.

---

## Block Layout: 4,096 Bits Across 11 Blocks

The ESP32-S3 contains exactly 4,096 eFuse bits organized into 11 blocks. Each block holds 256 bits (32 bytes). The blocks serve different purposes and have different encoding schemes, which affects how they can be written and how errors are handled.

### BLOCK0: System Configuration (256 bits, 4x Redundancy)

BLOCK0 is fundamentally different from all other blocks. Its bits use simple 4x redundancy encoding - each logical bit is stored as four physical copies. A logical bit reads as 1 if any of the four copies is 1. This means BLOCK0 supports incremental writes: you can set individual bits in separate operations without corrupting previously written bits. This is critical because BLOCK0 contains the security configuration bits that are set at different stages of the provisioning process.

Key fields in BLOCK0 for SimpleGo's security:

| Field | Bits | Purpose | SimpleGo Usage |
|-------|------|---------|----------------|
| WR_DIS | 32 | Write-protect individual blocks and fields | Protect eFuse keys after provisioning |
| RD_DIS | 7 | Read-protect BLOCK4-BLOCK10 from software | Hide HMAC and flash encryption keys from firmware |
| DIS_DOWNLOAD_ICACHE | 1 | Disable instruction cache in download mode | Set by flash encryption |
| DIS_DOWNLOAD_DCACHE | 1 | Disable data cache in download mode | Set by flash encryption |
| DIS_DOWNLOAD_MANUAL_ENCRYPT | 1 | Disable manual encrypt in download mode | Set in Release mode |
| SOFT_DIS_JTAG | 3 | Soft-disable JTAG (re-enable via HMAC) | Development flexibility |
| DIS_PAD_JTAG | 1 | Permanently disable pad JTAG | Production devices |
| DIS_USB_JTAG | 1 | Permanently disable USB JTAG | Production devices |
| DIS_DIRECT_BOOT | 1 | Disable legacy SPI boot mode | Set by security features |
| SPI_BOOT_CRYPT_CNT | 3 | Flash encryption enable counter | Odd popcount = enabled |
| SECURE_BOOT_EN | 1 | Enable Secure Boot V2 permanently | Mode 4 (Bunker) |
| KEY_PURPOSE_0 through KEY_PURPOSE_5 | 4 each (24 total) | Assign purpose to each key block | Maps key blocks to functions |
| SECURE_BOOT_KEY_REVOKE0/1/2 | 3 | Revoke individual Secure Boot key slots | Key rotation support |

The SPI_BOOT_CRYPT_CNT field deserves special attention. It is a 3-bit counter where the encryption state depends on the popcount (number of 1-bits): odd popcount means encryption is enabled, even means disabled. The transitions are: 000 (off, factory default) to 001 (on, first enable) to 011 (off, first disable, development mode only) to 111 (permanently on, Release mode). In Release mode, the bootloader write-protects this field at 0b111, making flash encryption permanent.

### BLOCK1-BLOCK3: System Data (256 bits each, RS Coding)

These blocks use Reed-Solomon RS(44,32) error-correction encoding: 32 bytes of data plus 12 bytes of RS check symbols. The RS coding can correct certain single-byte errors, but a second write to an RS-coded block corrupts the check symbols, degrading error correction capability. In practice, these blocks should be treated as write-once.

| Block | Alias | Contents |
|-------|-------|----------|
| BLOCK1 | MAC_SPI_8M_SYS | Factory MAC address (6 bytes), SPI pad configuration, 8 MHz oscillator calibration |
| BLOCK2 | SYS_DATA_PART1 | ADC calibration data |
| BLOCK3 | USER_DATA | Custom MAC address, user-defined parameters |

SimpleGo does not write to these blocks. The factory MAC address in BLOCK1 is used by the WiFi driver. The ADC calibration in BLOCK2 is used by the hardware RNG entropy source.

### BLOCK4-BLOCK9: Cryptographic Key Storage (256 bits each, RS Coding)

These six blocks are the heart of SimpleGo's hardware security. Each block stores one 256-bit cryptographic key and is independently controlled by read-protection, write-protection, and purpose assignment.

| Block | Alias | ESP-IDF Enum | SimpleGo Allocation |
|-------|-------|-------------|-------------------|
| BLOCK4 | BLOCK_KEY0 | EFUSE_BLK_KEY0 | Reserved for future use |
| BLOCK5 | BLOCK_KEY1 | EFUSE_BLK_KEY1 | **HMAC key for NVS encryption (Mode 2+)** |
| BLOCK6 | BLOCK_KEY2 | EFUSE_BLK_KEY2 | Flash encryption key (Mode 3+) |
| BLOCK7 | BLOCK_KEY3 | EFUSE_BLK_KEY3 | Flash encryption key 2 (AES-256 mode) or Secure Boot digest 0 |
| BLOCK8 | BLOCK_KEY4 | EFUSE_BLK_KEY4 | Secure Boot digest 0 or 1 |
| BLOCK9 | BLOCK_KEY5 | EFUSE_BLK_KEY5 | Reserved (cannot be used for flash encryption XTS keys) |

**Critical hardware limitation:** BLOCK9 (BLOCK_KEY5) cannot be used for XTS-AES flash encryption keys on the ESP32-S3. This is documented in the ESP-IDF source but easy to miss. If you attempt to burn a flash encryption key to BLOCK_KEY5, the encryption will not work correctly.

The allocation shown above is SimpleGo's planned layout. BLOCK_KEY0 is deliberately left free as a reserve - if any provisioning step fails and consumes a block unexpectedly, we have a fallback. BLOCK_KEY1 is assigned to the HMAC key because it is the first key provisioned (Mode 2) and benefits from being in a predictable location.

### BLOCK10: Additional System Data (256 bits, RS Coding)

Reserved for Espressif system parameters. SimpleGo does not use this block.

---

## Read Protection: Making Keys Invisible to Software

Read protection is the mechanism that makes the HMAC vault possible. When a key block's read-protection bit is set in the RD_DIS field of BLOCK0, any software attempt to read that block returns all zeros. But the hardware peripherals - specifically the HMAC accelerator, the AES-XTS engine, and the Digital Signature module - retain full access to the key through dedicated internal buses that bypass the software-readable register interface.

This creates the security property we need: the HMAC key exists in the chip, the HMAC peripheral can use it to derive NVS encryption keys, but no software running on the device (including our own firmware) can ever read the raw key material. An attacker who gains code execution through a firmware vulnerability still cannot extract the HMAC key through software alone.

### How read protection is applied

The RD_DIS field in BLOCK0 has 7 bits, controlling read access for BLOCK4 through BLOCK10:

| RD_DIS Bit | Protects |
|-----------|----------|
| Bit 0 | BLOCK4 (BLOCK_KEY0) |
| Bit 1 | BLOCK5 (BLOCK_KEY1) |
| Bit 2 | BLOCK6 (BLOCK_KEY2) |
| Bit 3 | BLOCK7 (BLOCK_KEY3) |
| Bit 4 | BLOCK8 (BLOCK_KEY4) |
| Bit 5 | BLOCK9 (BLOCK_KEY5) |
| Bit 6 | BLOCK10 (SYS_DATA_PART2) |

BLOCK0 through BLOCK3 cannot be read-protected. Their contents (MAC address, calibration data, security configuration bits) must remain readable by firmware.

### Automatic protection during key burning

When using `esp_efuse_write_key()` or `espefuse.py burn_key`, the tooling automatically applies appropriate protection based on the key purpose:

| Key Purpose | Read Protection | Write Protection |
|-------------|----------------|-----------------|
| HMAC_UP (NVS encryption) | Automatic | Automatic |
| XTS_AES_128_KEY (flash encryption) | Automatic | Automatic |
| XTS_AES_256_KEY_1/2 | Automatic | Automatic |
| SECURE_BOOT_DIGEST0/1/2 | NOT applied (must remain readable) | Automatic |
| USER | NOT applied | NOT applied |

Secure Boot digest keys are deliberately NOT read-protected because the boot ROM needs to read the public key hash to verify firmware signatures. This is a design requirement, not a weakness - the digest is a SHA-256 hash of the public key, not the private key itself.

### The RD_DIS write-protection conflict

This is the most critical eFuse ordering constraint in the entire provisioning process.

When Secure Boot V2 is enabled, the bootloader write-protects the RD_DIS field in BLOCK0 (by setting WR_DIS bit for RD_DIS). This prevents an attacker from read-protecting the Secure Boot digest blocks, which would cause a denial-of-service (boot would fail if it cannot read the key digest).

However, this write-protection of RD_DIS also means that no further read-protection can be applied to ANY key block. If the flash encryption key has not yet been burned and read-protected at this point, it can never be read-protected afterward - leaving the flash encryption key readable by software and defeating its purpose.

**The mandatory provisioning order is therefore: Flash Encryption first, then Secure Boot.** Specifically:

```
1. Burn HMAC key to BLOCK_KEY1 (auto read-protected)
2. Burn Flash Encryption key to BLOCK_KEY2 (auto read-protected)
3. Enable Flash Encryption (burn SPI_BOOT_CRYPT_CNT)
4. Burn Secure Boot digest to BLOCK_KEY4 (remains readable)
5. Enable Secure Boot (burn SECURE_BOOT_EN)
   --> This write-protects RD_DIS, locking all read-protection state
```

If steps are performed out of order, keys may remain software-readable, fundamentally compromising the security model.

---

## Write Protection: Preventing Key Modification

Write protection ensures that once a key is burned, it cannot be altered. The WR_DIS field in BLOCK0 contains 32 bits controlling write access to various eFuse fields and blocks.

| WR_DIS Bit | Protects |
|-----------|----------|
| Bit 0 | RD_DIS field itself |
| Bit 1 | DIS_ICACHE, DIS_USB_JTAG, other BLOCK0 fields |
| Bit 2 | Various BLOCK0 configuration fields |
| Bit 18 | SPI_BOOT_CRYPT_CNT |
| Bit 20 | BLOCK_KEY0 |
| Bit 21 | BLOCK_KEY1 |
| Bit 22 | BLOCK_KEY2 |
| Bit 23 | BLOCK_KEY3 |
| Bit 24 | BLOCK_KEY4 |
| Bit 25 | BLOCK_KEY5 |

Write protection of key blocks happens automatically during `esp_efuse_write_key()`. The KEY_PURPOSE field for each block is always write-protected regardless of the `--no-write-protect` flag - this prevents an attacker from reassigning a key's purpose after burning.

---

## Hardware Random Number Generator

The quality of the eFuse-burned keys depends entirely on the quality of the random number generator used to create them. A predictable key is worse than no key at all.

### Entropy sources

The ESP32-S3 TRNG is fed by two physical entropy sources:

**Primary source: SAR ADC thermal noise.** The Successive Approximation Register ADC samples thermal noise from the analog circuitry. This source requires that WiFi or Bluetooth be active (or that `bootloader_random_enable()` has been called explicitly) because the ADC's noise properties depend on the RF subsystem being powered.

**Secondary source: RC_FAST_CLK oscillator jitter.** The internal ~8 MHz RC oscillator has inherent frequency jitter due to manufacturing variation and thermal effects. This source is always active when the chip is powered. Espressif's testing has shown that the secondary source alone passes the Dieharder statistical test suite, but they recommend using both sources for cryptographic applications.

### API usage for key generation

```c
#include "esp_random.h"

// For cryptographic key generation, ensure WiFi/BT is active OR call:
bootloader_random_enable();

uint8_t hmac_key[32];
esp_fill_random(hmac_key, sizeof(hmac_key));  // 256 bits of true random

bootloader_random_disable();  // Re-enable normal clocking

// The esp_random() API internally throttles reads to 15-75 KHz
// to ensure sufficient entropy accumulation between samples.
// esp_fill_random() handles this automatically for buffer fills.
```

**Critical warning:** If neither WiFi/BT nor `bootloader_random_enable()` is active, `esp_random()` degrades to a pseudo-random number generator (PRNG) seeded from the secondary source. For SimpleGo's first-boot key generation, the firmware must ensure the primary entropy source is active before generating the HMAC key. The recommended approach is to call `bootloader_random_enable()` in the early boot code before NVS initialization.

### Verification

After generating a key, there is no way to verify its randomness after the fact (this is a fundamental property of randomness). However, the firmware can verify that the entropy source was active by checking the return value of `bootloader_random_enable()` and by reading the `RNG_DATA_REG` register multiple times to confirm the values change.

---

## espefuse.py Command Reference for SimpleGo

The `espefuse.py` tool (part of esptool) is the primary interface for eFuse operations during development and manufacturing. All commands require a physical USB connection to the device.

### Reading current eFuse state

```bash
# Full eFuse summary (recommended first step before any burns)
espefuse.py -c esp32s3 -p COM6 summary

# Check specific key block status
espefuse.py -c esp32s3 -p COM6 get_custom_mac
```

The summary output shows every eFuse field, its current value, and whether it is read/write protected. Always run this before any burn operation to verify the current state.

### Burning an HMAC key for NVS encryption

```bash
# Generate a random key file (32 bytes = 256 bits)
# Use a secure system with quality entropy for production keys
python -c "import os; open('hmac_key.bin','wb').write(os.urandom(32))"

# Burn to BLOCK_KEY1 with HMAC_UP purpose
# This automatically: sets KEY_PURPOSE_1 = HMAC_UP,
# write-protects BLOCK_KEY1, read-protects BLOCK_KEY1
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY1 hmac_key.bin HMAC_UP

# Verify the burn (key value will show as all zeros due to read protection)
espefuse.py -c esp32s3 -p COM6 summary
```

### Burning a flash encryption key

```bash
# For AES-128 (uses 1 key block):
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY2 fe_key.bin XTS_AES_128_KEY

# For AES-256 (uses 2 key blocks, virtual purpose handles the split):
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY2 fe_key_512.bin XTS_AES_256_KEY
# This automatically splits the 512-bit key across BLOCK_KEY2 and BLOCK_KEY3
```

### Virtual (dry-run) mode for testing

```bash
# Simulate eFuse operations without touching hardware
espefuse.py -c esp32s3 --virt burn_key BLOCK_KEY1 hmac_key.bin HMAC_UP

# Virtual mode is essential for testing provisioning scripts
# before running them on real hardware where mistakes are permanent
```

### Confirmation safety

Every destructive operation requires typing "BURN" to confirm:

```
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY1 hmac_key.bin HMAC_UP

=== Run "burn_key" command ===
Burn keys to blocks:
- BLOCK_KEY1 -> [key data] (HMAC_UP)

Attention! This operation is irreversible.
Type 'BURN' (all capitals) to continue:
```

For automated manufacturing scripts, `--do-not-confirm` bypasses this prompt. Use with extreme caution - there is no undo.

### Programmatic eFuse operations from firmware

For SimpleGo's first-boot provisioning (Mode 2), the HMAC key is burned from the running firmware rather than from the host PC:

```c
#include "esp_efuse.h"
#include "esp_efuse_table.h"

// Check if BLOCK_KEY1 is available
if (esp_efuse_key_block_unused(EFUSE_BLK_KEY1)) {
    // Generate key from hardware RNG
    uint8_t key[32];
    bootloader_random_enable();
    esp_fill_random(key, sizeof(key));
    bootloader_random_disable();

    // Burn key with purpose and protection
    esp_err_t err = esp_efuse_write_key(
        EFUSE_BLK_KEY1,
        ESP_EFUSE_KEY_PURPOSE_HMAC_UP,
        key,
        sizeof(key)
    );

    // Securely zero the key from RAM immediately after burning
    memset(key, 0, sizeof(key));  // Use mbedtls_platform_zeroize() in production

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HMAC key provisioned successfully");
    } else {
        ESP_LOGE(TAG, "HMAC key provisioning failed: %s", esp_err_to_name(err));
    }
} else {
    ESP_LOGI(TAG, "HMAC key already provisioned");
}
```

**Known issue (GitHub #13909):** When Secure Boot is already enabled, `esp_efuse_write_key()` may fail because writing the KEY_PURPOSE bits to BLOCK0 conflicts with Secure Boot's write protection of certain BLOCK0 regions. Resolution: Always burn the HMAC key before enabling Secure Boot, or provision the key from the host via espefuse.py during manufacturing.

---

## eFuse Budget Planning for SimpleGo

With six key blocks and multiple security features competing for them, careful allocation is essential. Each security mode uses a different number of blocks, and the allocation must be planned to support upgrades from lower to higher modes.

### Mode 1: Open (0 blocks used)

No eFuse blocks consumed. All six blocks remain available for future use. The device can transition to any other mode at any time.

### Mode 2: NVS Vault (1 block used)

| Block | Purpose | Status |
|-------|---------|--------|
| BLOCK_KEY0 | Free | Reserved as safety spare |
| BLOCK_KEY1 | HMAC_UP (NVS encryption) | **Burned at first boot** |
| BLOCK_KEY2 | Free | Available for Mode 3 upgrade |
| BLOCK_KEY3 | Free | Available for Mode 3/4 |
| BLOCK_KEY4 | Free | Available for Mode 4 |
| BLOCK_KEY5 | Free | Available (but not for flash encryption) |

The device can still transition to Mode 3 or 4 because the remaining blocks are available for flash encryption and Secure Boot keys.

### Mode 3: Fortress with AES-128 (2 blocks used)

| Block | Purpose | Status |
|-------|---------|--------|
| BLOCK_KEY0 | Free | Reserved as safety spare |
| BLOCK_KEY1 | HMAC_UP (NVS encryption) | Burned |
| BLOCK_KEY2 | XTS_AES_128_KEY (flash encryption) | **Burned** |
| BLOCK_KEY3 | Free | Available for Mode 4 |
| BLOCK_KEY4 | Free | Available for Mode 4 |
| BLOCK_KEY5 | Free | Available |

### Mode 3: Fortress with AES-256 (3 blocks used)

| Block | Purpose | Status |
|-------|---------|--------|
| BLOCK_KEY0 | Free | Reserved as safety spare |
| BLOCK_KEY1 | HMAC_UP (NVS encryption) | Burned |
| BLOCK_KEY2 | XTS_AES_256_KEY_1 (flash encryption, first half) | **Burned** |
| BLOCK_KEY3 | XTS_AES_256_KEY_2 (flash encryption, second half) | **Burned** |
| BLOCK_KEY4 | Free | Available for Mode 4 |
| BLOCK_KEY5 | Free | Available |

### Mode 4: Bunker (3-4 blocks used for AES-128, 4-5 for AES-256)

| Block | Purpose (AES-128 variant) | Purpose (AES-256 variant) |
|-------|--------------------------|--------------------------|
| BLOCK_KEY0 | Free (spare) | Free (spare) |
| BLOCK_KEY1 | HMAC_UP | HMAC_UP |
| BLOCK_KEY2 | XTS_AES_128_KEY | XTS_AES_256_KEY_1 |
| BLOCK_KEY3 | SECURE_BOOT_DIGEST0 | XTS_AES_256_KEY_2 |
| BLOCK_KEY4 | Free or SECURE_BOOT_DIGEST1 | SECURE_BOOT_DIGEST0 |
| BLOCK_KEY5 | Free | Free or SECURE_BOOT_DIGEST1 |

The AES-128 variant of Mode 4 is recommended for SimpleGo Class 1 because it leaves more key blocks available for future use (additional Secure Boot key slots for key rotation, or the JTAG re-enable key for field debugging). AES-128-XTS provides more than sufficient security for NVS and firmware encryption - the published side-channel attacks extract the key regardless of key length, so AES-256 does not provide meaningful additional protection against the primary Class 1 threat.

---

## Comparison: ESP32-S3 vs ESP32-P4 eFuse Systems

The ESP32-P4 maintains the same fundamental eFuse architecture (11 blocks, 6 key blocks, RS coding) but adds critical security features that are not available on the ESP32-S3:

| Feature | ESP32-S3 | ESP32-P4 |
|---------|----------|----------|
| eFuse blocks total | 11 | 11 |
| Key blocks | 6 (BLOCK_KEY0-KEY5) | 6 (BLOCK_KEY0-KEY5) |
| Bits per block | 256 | 256 |
| RS error correction | Yes | Yes |
| Read/write protection | Yes | Yes |
| Key purpose assignment | Yes | Yes |
| HMAC peripheral | Yes | Yes |
| Digital Signature peripheral | No | **Yes** |
| Key Management Unit | No | **Yes** |
| Anti-DPA pseudo-rounds | No | **Yes (XTS_DPA_PSEUDO_LEVEL eFuse)** |
| Power Glitch Detector | No | **Yes** |
| ECDSA Secure Boot | No (RSA-3072 only) | **Yes (RSA-3072 + ECDSA-256/384)** |
| BLOCK_KEY5 flash enc. restriction | Cannot use for XTS keys | To be verified |

The Key Management Unit (KMU) in the ESP32-P4 is the most significant difference. On the ESP32-S3, the HMAC-derived NVS encryption keys exist in application RAM after derivation - malicious firmware (without Secure Boot) could read them. On the ESP32-P4, the KMU can manage key material entirely within the hardware security perimeter, ensuring keys used for cryptographic operations are never exposed to software in plaintext. This eliminates the "Secure Boot is required to protect derived keys" constraint that exists on the ESP32-S3.

The anti-DPA pseudo-rounds (controlled by the XTS_DPA_PSEUDO_LEVEL eFuse) add random dummy AES rounds before and after real operations, randomizing the power consumption profile. This directly addresses the Ledger Donjon CPA attack methodology. However, the ESP32-C6 had similar countermeasures that were proven ineffective by researcher Courk in 2024, so the P4's implementation remains unproven until independently tested.

---

## Failure Modes and Recovery

### Recoverable situations

| Situation | Recovery |
|-----------|----------|
| Wrong key burned to a block | Use a different block (the wrong block is permanently consumed) |
| Flash encryption enabled in Development mode | Disable by burning SPI_BOOT_CRYPT_CNT to next even popcount (limited attempts) |
| NVS data corrupted after enabling HMAC encryption | `idf.py erase-flash` clears NVS; eFuse key persists, new encrypted NVS created on next boot |
| Firmware crashes after enabling flash encryption | Reflash via UART in Development mode (counts against SPI_BOOT_CRYPT_CNT limit) |

### Unrecoverable situations (device permanently affected)

| Situation | Consequence |
|-----------|-------------|
| SECURE_BOOT_EN burned without valid signed firmware | Device will not boot. No recovery possible. |
| SPI_BOOT_CRYPT_CNT at 0b111 (permanent) with lost encryption key | Flash is permanently encrypted with unknown key. Device is a paperweight. |
| All Secure Boot key slots burned and revoked | No firmware can pass signature verification. Device will not boot. |
| JTAG disabled (DIS_PAD_JTAG=1 or DIS_USB_JTAG=1) | JTAG permanently unavailable. No hardware debugging possible. |
| DIS_DOWNLOAD_MODE burned with broken firmware | No UART recovery possible. Must have working OTA or device is bricked. |

### SimpleGo's mitigation strategy

For the Alpha release (Mode 2), the risk is minimal: only one eFuse block is consumed (HMAC key), and the worst case is consuming a second block if the first attempt fails. The firmware remains flashable via USB, JTAG remains available for debugging, and the device can always be recovered by reflashing.

For production devices (Mode 3/4), provisioning should be performed on a sacrificial test unit first, with the complete eFuse state verified via `espefuse.py summary` after each step. The provisioning script should run in virtual mode (`--virt`) before execution on real hardware. A second T-Deck should be available as backup in case the first device is bricked during provisioning development.

---

## References

| Source | Description |
|--------|-------------|
| ESP-IDF v5.5.3 eFuse Manager docs | Complete API reference for esp_efuse functions |
| ESP32-S3 Technical Reference Manual, Chapter 4 | eFuse controller hardware specification |
| espefuse.py burn_key docs | Command-line reference for key burning |
| GitHub espressif/esp-idf esp_efuse_table.csv | Raw eFuse field definitions for ESP32-S3 |
| ESP-IDF Security Features Enablement Workflows | Step-by-step provisioning guides with eFuse ordering |
| GitHub Issue #13909 | Known bug: HMAC key burn fails when Secure Boot already enabled |
| GitHub Issue #17052 | Upcoming IDF v6.0 breaking changes affecting NVS encryption defaults |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
