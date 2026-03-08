---
title: "Class 1 - HMAC-Based NVS Encryption"
sidebar_position: 3
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# HMAC-Based NVS Encryption

**Document version:** Session 44 | March 2026
**Applies to:** ESP32-S3 (LilyGo T-Deck Plus), ESP32-P4 (planned)
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Why NVS Encryption is SimpleGo's Most Critical Security Feature

SimpleGo stores cryptographic keys for up to 128 contacts in the ESP32-S3's Non-Volatile Storage (NVS) partition. These keys include Double Ratchet state (X448 DH keys, chain keys, message keys), per-queue NaCl keypairs (X25519), server-to-recipient keypairs, handshake state for ongoing key exchanges, and WiFi credentials. Without NVS encryption, all of this is stored in plaintext on the external SPI flash chip. Anyone with a $15 flash reader, a soldering iron (or just a clip-on probe), and 5 minutes of time can dump the entire NVS partition and read every private key.

The HMAC-based NVS encryption scheme transforms this from a trivial attack to one requiring laboratory equipment and expert knowledge. It is the single largest security improvement possible for SimpleGo Class 1 with zero additional hardware cost.

---

## How the Scheme Works

### The Core Concept

A 256-bit secret key is stored in a read-protected eFuse block. The ESP32-S3's hardware HMAC peripheral uses this key to derive two 256-bit XTS encryption keys at runtime. These derived keys encrypt and decrypt the NVS partition using the XTS-AES-256 algorithm (IEEE P1619). The critical property is that the eFuse key never leaves the hardware security boundary - software cannot read it, JTAG cannot read it, and physically reading the flash chip does not reveal it.

### Step-by-Step Key Derivation

When NVS is initialized, the following hardware operations occur:

```
Step 1: Firmware calls nvs_flash_init() or nvs_flash_secure_init()

Step 2: ESP-IDF checks CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID
        (configured to BLOCK_KEY1 for SimpleGo)

Step 3: Hardware HMAC peripheral performs:
        XTS_KEY_1 = HMAC-SHA256(eFuse_key, 0xAEBE5A5A)  --> 32 bytes
        XTS_KEY_2 = HMAC-SHA256(eFuse_key, 0xCEDEA5A5)  --> 32 bytes

Step 4: Combined 512-bit XTS-AES-256 key = XTS_KEY_1 || XTS_KEY_2

Step 5: All subsequent NVS read/write operations use this key
        for transparent encryption/decryption
```

The magic numbers 0xAEBE5A5A and 0xCEDEA5A5 are hardcoded constants in ESP-IDF's `nvs_sec_provider.c`. They serve as domain separation - ensuring the two derived keys are different even though they come from the same eFuse key. These constants are not secret; their purpose is structural, not cryptographic.

### XTS-AES-256 Encryption per NVS Entry

Each NVS entry (key-value pair) is encrypted as an independent sector using XTS-AES-256. The relative address of the entry within the NVS partition serves as the XTS "tweak" value (sector number). This means:

Each entry is encrypted with a unique effective key (derived from the XTS key plus the entry's position). Moving an encrypted entry to a different position within the partition makes it unreadable. Copying the entire NVS partition to another device with a different eFuse key makes all entries unreadable.

The encryption happens at the NVS library level, not at the flash driver level. This means only the NVS partition is encrypted this way - other partitions (application firmware, OTA data) are only encrypted if flash encryption (a separate feature) is also enabled.

---

## Configuration

### Required sdkconfig settings

```
# Enable NVS encryption
CONFIG_NVS_ENCRYPTION=y

# Select HMAC-based protection scheme
CONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=1
CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC=y

# Specify which eFuse key block holds the HMAC key
# Value 0-5 maps to BLOCK_KEY0 through BLOCK_KEY5
# SimpleGo uses 1 (BLOCK_KEY1)
CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID=1
```

The menuconfig path is: Component config -> NVS -> Enable NVS encryption -> NVS Encryption Keys Protection Scheme -> HMAC peripheral-based scheme -> HMAC key ID storing the NVS encryption key.

### No separate key partition required

Unlike the flash-encryption-based NVS scheme, the HMAC scheme does NOT require a separate `nvs_keys` partition in the partition table. The encryption keys are derived at runtime from the eFuse, not stored in flash. This simplifies the partition table and eliminates one potential attack surface (the key partition itself).

SimpleGo's existing partition table does not need modification for HMAC NVS encryption. The only change is replacing `nvs_flash_init()` with the secure initialization path.

---

## First-Boot Provisioning

### Automatic provisioning (recommended for Mode 2)

When the CONFIG options above are set and the firmware calls `nvs_flash_init()`, ESP-IDF handles provisioning automatically:

```
First boot (no HMAC key in eFuse):
  1. nvs_flash_init() detects no key at CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID
  2. Generates 256-bit random key via hardware RNG
  3. Burns key to specified eFuse block with HMAC_UP purpose
  4. Sets read-protection and write-protection on the block
  5. Derives XTS keys via HMAC peripheral
  6. Initializes NVS partition with encryption

Subsequent boots (HMAC key present):
  1. nvs_flash_init() detects existing key
  2. Derives XTS keys via HMAC peripheral
  3. Opens encrypted NVS partition normally
```

This automatic flow is ideal for SimpleGo Mode 2 (NVS Vault) because it requires zero user interaction. The user flashes firmware via the web installer, powers on the device, and the vault is created automatically. No manufacturing step, no host PC provisioning, no special tooling.

### Manual provisioning (recommended for Mode 3/4)

For production devices where the provisioning process must be controlled and auditable, the key can be burned from the host PC before flashing firmware:

```bash
# Generate key on a secure workstation
python -c "import os; open('hmac_key.bin','wb').write(os.urandom(32))"

# Burn to device
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY1 hmac_key.bin HMAC_UP

# Verify
espefuse.py -c esp32s3 -p COM6 summary

# SECURELY DELETE the key file from the workstation
# The key should exist ONLY in the device's eFuse
shred -vfz hmac_key.bin
```

For Mode 3/4, manual provisioning is preferred because it integrates with the flash encryption and Secure Boot provisioning workflow that must happen in a specific order (see eFuse Architecture document, Section "Read Protection").

### Programmatic provisioning from firmware

For advanced use cases where the firmware manages its own provisioning:

```c
#include "esp_efuse.h"
#include "esp_random.h"
#include "mbedtls/platform_util.h"

esp_err_t simplego_provision_hmac_key(void) {
    // Check if key already exists
    if (!esp_efuse_key_block_unused(EFUSE_BLK_KEY1)) {
        ESP_LOGI(TAG, "HMAC key already provisioned in BLOCK_KEY1");
        return ESP_OK;
    }

    // Ensure hardware RNG has full entropy
    bootloader_random_enable();

    // Generate 256-bit key
    uint8_t key[32];
    esp_fill_random(key, sizeof(key));

    bootloader_random_disable();

    // Burn to eFuse (automatically sets purpose, read-protect, write-protect)
    esp_err_t err = esp_efuse_write_key(
        EFUSE_BLK_KEY1,
        ESP_EFUSE_KEY_PURPOSE_HMAC_UP,
        key,
        sizeof(key)
    );

    // CRITICAL: Zero the key from RAM immediately
    mbedtls_platform_zeroize(key, sizeof(key));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to provision HMAC key: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "BLOCK_KEY1 may be consumed. Check espefuse summary.");
    }

    return err;
}
```

Note the use of `mbedtls_platform_zeroize()` instead of `memset()` - this prevents the compiler from optimizing away the zeroing operation as a "dead store" (CWE-14). This was the exact vulnerability fixed in SimpleGo's `smp_storage.c` during Session 42 (SEC-03).

---

## Migration from Unencrypted NVS

SimpleGo currently uses `nvs_flash_init()` in `main.c`. Switching to encrypted NVS is a one-way migration: existing unencrypted NVS data cannot be read after encryption is enabled.

### Migration procedure

```
1. Build firmware with NVS encryption config enabled
2. Flash firmware to device: idf.py build flash -p COM6
3. On first boot:
   a. Firmware attempts nvs_flash_init() (now secure variant)
   b. NVS partition format is incompatible (was plaintext, now expects encrypted)
   c. ESP-IDF returns ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND
   d. Firmware must call nvs_flash_erase() then retry nvs_flash_init()
   e. HMAC key is provisioned (if not already present)
   f. Fresh encrypted NVS is created
4. All previously stored keys (ratchet state, queue keys) are LOST
5. Device must re-establish all contacts from scratch
```

This is equivalent to a factory reset. For Alpha testing this is acceptable - testers expect to reconfigure their devices. For production, the migration should be documented clearly: "Enabling the hardware vault erases all existing contacts. Back up your connection links before upgrading."

### Recommended error handling in main.c

```c
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS format incompatible, erasing for fresh encrypted init");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);
```

This pattern is already common in ESP-IDF examples and handles both the migration case and the case where NVS has been corrupted.

---

## What idf.py erase-flash Does to the Vault

Understanding the interaction between `idf.py erase-flash` and the HMAC vault is essential for development workflow:

`idf.py erase-flash` erases the entire external SPI flash chip, including the NVS partition, the application firmware, the partition table, and the bootloader. It does NOT touch eFuses - they are physically separate from flash.

After erase-flash, when new firmware is flashed and boots, the HMAC key is still present in BLOCK_KEY1 (it was never erased). The firmware calls `nvs_flash_init()`, detects the HMAC key, derives the XTS keys, and creates a fresh encrypted NVS partition. All previous NVS data is gone (it was erased), but the vault mechanism continues working with the same hardware key.

This means `idf.py erase-flash` is a safe operation during development. It does not break the vault. It simply performs a factory reset of all stored data while preserving the hardware identity.

---

## Security Analysis

### What the HMAC vault protects against

**Flash chip readout:** An attacker who desolders the flash chip or uses a clip-on SPI probe reads only encrypted NVS data. The XTS-AES-256 encryption is computationally infeasible to break without the eFuse-derived key.

**Firmware-level key theft (with Secure Boot):** If Secure Boot is enabled (Mode 4), only signed firmware can execute. An attacker cannot flash a custom firmware that dumps the derived XTS keys from RAM. The HMAC key itself is hardware-protected regardless.

**Cross-device attacks:** Each device has a unique HMAC key generated from its own hardware RNG. Compromising one device's key reveals nothing about any other device's key. There is no master key, no shared secret, no fleet-wide vulnerability.

### What the HMAC vault does NOT protect against

**Derived keys in RAM (without Secure Boot):** The HMAC peripheral derives the XTS keys and makes them available to the NVS library in application RAM. Without Secure Boot, an attacker who flashes malicious firmware can read these derived keys from RAM. The eFuse key itself remains protected, but the derived keys are sufficient to decrypt NVS contents. This is why Mode 4 (Bunker with Secure Boot) is recommended for high-security deployments.

**Side-channel analysis:** The HMAC peripheral performs SHA-256 operations using the eFuse key. While no published side-channel attack targets the ESP32-S3's HMAC peripheral specifically, the conservative assumption is that a researcher with physical access and appropriate equipment could eventually extract the key. The HMAC peripheral likely shares the same unprotected design philosophy (no masking, no clock randomization) as the AES accelerator that has been broken.

**Hardcoded challenge values:** The derivation uses fixed constants (0xAEBE5A5A, 0xCEDEA5A5). This means the derivation is deterministic: any firmware running on the device derives identical XTS keys. The HMAC key provides device-binding, not firmware-binding. Combined with Secure Boot, this is sufficient (only authorized firmware runs). Without Secure Boot, it means any firmware can derive the keys.

### Comparison with flash-encryption-based NVS scheme

| Property | HMAC Scheme | Flash Encryption Scheme |
|----------|-------------|------------------------|
| Requires flash encryption enabled | No | Yes |
| Requires nvs_keys partition | No | Yes (4 KB, must be marked encrypted) |
| Keys stored in flash | Never | Yes (in nvs_keys partition, encrypted by FE) |
| Standalone operation | Yes | No (depends on flash encryption) |
| eFuse blocks consumed | 1 (HMAC key) | 0 (uses flash encryption key) |
| Key derivation | Hardware HMAC at runtime | Stored keys decrypted by FE at boot |
| ESP-IDF version | 5.0+ | 4.0+ |
| Default in IDF v6.0 | Will become default for HMAC-capable SoCs | Legacy |

The HMAC scheme is superior for SimpleGo because it works independently, has a smaller attack surface (no key partition to protect), and aligns with the planned IDF v6.0 default.

---

## SEC-05 Resolution: Device-Bound SD Card Encryption

The HMAC vault resolves SEC-05 (weak HKDF info parameter for SD card encryption) as a side effect.

Currently, the HKDF info parameter for per-contact SD card encryption keys uses only the contact slot index: `simplego-slot-XX`. This means the same master key on another device with the same slot index produces identical derived keys - a cloned SD card could be read on a different SimpleGo device if the master key were also copied.

With the HMAC vault active, the device has a unique hardware identity derived from the eFuse key. The HKDF info parameter changes to include a device-specific component:

```c
// Before (SEC-05 vulnerable):
// info = "simplego-slot-03"

// After (SEC-05 resolved):
// info = "simplego-slot-03-HMAC(device_binding_constant)"
// where the HMAC uses the same eFuse key but with a different purpose
```

The implementation uses `esp_hmac_calculate()` with a binding constant to derive a device-specific identifier from the eFuse key, then includes this identifier in the HKDF info string. The SD card is now cryptographically bound to the specific physical device. Removing the SD card and inserting it into another SimpleGo produces decryption failures because the HKDF-derived keys are different.

---

## Performance Considerations

### Boot time impact

The HMAC key derivation at boot (two SHA-256-HMAC operations via hardware peripheral) takes approximately 1-2 microseconds. This is completely negligible in SimpleGo's boot sequence, which is dominated by WiFi initialization (~2 seconds), LVGL setup (~500 ms), and TLS handshake (~1-3 seconds).

### NVS read/write performance

NVS encryption adds one AES-XTS operation per NVS read and one per NVS write. The ESP32-S3's hardware AES accelerator handles this transparently. No benchmark data specific to HMAC-NVS is published by Espressif, but the flash I/O latency (SPI bus speed, flash write time) dominates over the AES computation time by orders of magnitude. In practice, there is no perceptible performance difference between encrypted and unencrypted NVS operations.

### Memory overhead

The derived XTS keys (64 bytes) are held in RAM for the lifetime of the NVS session. The `nvs_sec_cfg_t` structure that holds the security configuration is approximately 128 bytes. Total RAM overhead for NVS encryption: under 256 bytes. Negligible against 512 KB SRAM and 8 MB PSRAM.

---

## Future Considerations: ESP-IDF v6.0

ESP-IDF v6.0 (upcoming) will make the HMAC scheme the default NVS encryption method for SoCs with the HMAC peripheral (which includes all chips from ESP32-S2 onward). The flash-encryption-based scheme will be deprecated for new designs. This confirms that SimpleGo's choice of the HMAC scheme is aligned with Espressif's long-term direction.

The migration to IDF v6.0 should be transparent for SimpleGo: the same CONFIG options, the same eFuse layout, the same API calls. The main change is that `nvs_flash_init()` will default to secure initialization when it detects an HMAC key in eFuse, potentially simplifying our initialization code.

---

## References

| Source | Description |
|--------|-------------|
| ESP-IDF v5.5.3 NVS Encryption docs | Complete API and scheme documentation |
| ESP-IDF Security Features Enablement Workflows | Step-by-step HMAC NVS provisioning |
| ESP32 Forum: NVS partition encryption keys in HMAC mode | Community discussion on HMAC vs FE scheme |
| GitHub Issue #13909 | Bug: HMAC key burn fails when Secure Boot already enabled |
| GitHub Issue #17052 | IDF v6.0 breaking changes: HMAC becomes default |
| ESP-IDF nvs_sec_provider.c source | HMAC challenge constants and derivation logic |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
