---
title: "Class 1 - Flash Encryption Deep Dive"
sidebar_position: 8
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Flash Encryption Deep Dive

**Document version:** Session 44 | March 2026
**Applies to:** ESP32-S3 (Modes 3 and 4)
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## When Flash Encryption Applies

Flash encryption is relevant for SimpleGo Modes 3 (Fortress) and 4 (Bunker). It is NOT required for Mode 2 (NVS Vault), which uses HMAC-based NVS encryption independently. This distinction is important: the HMAC vault protects NVS key storage without flash encryption. Flash encryption adds firmware confidentiality on top.

For SimpleGo, firmware confidentiality provides limited security value because the firmware is open source (AGPL-3.0). An attacker who reads the firmware learns nothing they could not learn from GitHub. The value of flash encryption for SimpleGo lies in two areas: preventing firmware tampering (an attacker cannot modify the encrypted firmware in flash without the encryption key), and protecting any non-NVS sensitive data that may be stored in flash partitions (OTA staging area, custom data partitions).

---

## How XTS-AES Flash Encryption Works

The ESP32-S3 uses XTS-AES (IEEE 1619-2007) to encrypt the contents of the external SPI flash chip. XTS (XEX-based Tweaked-codebook mode with ciphertext Stealing) is a block cipher mode designed specifically for storage encryption. It encrypts data in 16-byte blocks where each block's encryption depends on both the AES key and the block's physical address on the flash.

### AES-128 vs AES-256 Mode

**AES-128-XTS** uses a single 256-bit key stored in one eFuse key block (KEY_PURPOSE = XTS_AES_128_KEY). The 256 bits are split internally into two 128-bit AES keys: one for the block encryption and one for the tweak computation.

**AES-256-XTS** uses a 512-bit key split across two eFuse key blocks (KEY_PURPOSE = XTS_AES_256_KEY_1 and XTS_AES_256_KEY_2). Each block stores one 256-bit AES key.

**Recommendation for SimpleGo:** AES-128-XTS. The published side-channel attacks extract the full key regardless of key length (CPA targets individual AES rounds, not the key schedule), so AES-256 provides no additional protection against the primary Class 1 threat. AES-128-XTS consumes only one eFuse key block instead of two, preserving blocks for Secure Boot and future use.

### Encryption Granularity

Flash encryption operates on 16-byte blocks. Each block is encrypted with a key derived from the base XTS key and the block's flash address. This means moving a block to a different flash address makes it undecryptable, and identical plaintext at different addresses produces different ciphertext. The CPU's flash cache handles decryption transparently - application code reads flash through memory-mapped regions and receives plaintext without any code changes.

### What Gets Encrypted

| Partition Type | Encrypted | Notes |
|---------------|-----------|-------|
| Bootloader | Yes | Always encrypted when FE enabled |
| Application (factory, OTA) | Yes | All app-type partitions |
| NVS | No (uses own encryption) | HMAC-NVS encryption is independent |
| OTA data | Yes | Partition selection metadata |
| PHY init | Configurable | Marked encrypted in partition table |
| Custom data | Configurable | Only if "encrypted" flag set in partition table |
| SD card | No | External to SPI flash, uses SimpleGo's AES-256-GCM |

### PSRAM Encryption

When flash encryption is enabled on the ESP32-S3, PSRAM encryption is automatically enabled using the same XTS-AES key and hardware block. This is mandatory and cannot be independently disabled. All data written to and read from external PSRAM is transparently encrypted and decrypted by the flash cache controller.

For SimpleGo, this means the `s_msg_cache` in PSRAM is encrypted at the hardware level when flash encryption is active (Mode 3/4). This provides an additional layer of protection for SEC-01 - even if `sodium_memzero()` is not called, the PSRAM contents are encrypted and cannot be read by external probing without the flash encryption key. However, this should not be treated as a substitute for proper zeroing, because the data is still decrypted when accessed by the CPU and could be read via JTAG or a firmware vulnerability.

---

## Development Mode vs Release Mode

### Development Mode

Development mode allows repeated enable/disable cycles of flash encryption, intended for development and testing. The SPI_BOOT_CRYPT_CNT eFuse transitions: 000 (off) to 001 (on) to 011 (off) to 111 (permanently on). Each transition from off to on consumes one bit. Development mode allows at most one disable-reenable cycle before the counter reaches 111 (permanently on).

In Development mode, several protections are deliberately relaxed. UART bootloader encryption remains available (DIS_DOWNLOAD_MANUAL_ENCRYPT is not burned). `idf.py flash` can still write plaintext firmware, which the bootloader will re-encrypt on next boot (consuming a CRYPT_CNT cycle). JTAG may remain available depending on other eFuse settings.

### Release Mode

Release mode sets SPI_BOOT_CRYPT_CNT to 0b111 immediately and write-protects the field, making flash encryption permanent. Additionally, DIS_DOWNLOAD_MANUAL_ENCRYPT is burned (no encrypted writes via UART bootloader), DIS_DOWNLOAD_ICACHE and DIS_DOWNLOAD_DCACHE are burned, and UART Download mode is restricted to Secure Download Mode or disabled entirely.

After Release mode activation, the only way to update firmware is via OTA. There is no serial recovery path.

---

## OTA Updates with Flash Encryption

Over-the-air firmware updates work transparently with flash encryption. The device downloads new firmware (ideally over HTTPS) as plaintext, and the flash hardware encrypts it during the write to the OTA partition. The `esp_partition_write()` function handles this automatically when writing to an encrypted partition.

The update flow:

```
1. Device connects to update server via HTTPS
2. Downloads new firmware binary (plaintext in RAM)
3. Writes to OTA partition via esp_partition_write()
4. Flash hardware encrypts each 16-byte block during write
5. Device reboots into new firmware (decrypted transparently on read)
6. If boot fails, rollback to previous OTA partition
```

For additional transport security, ESP-IDF provides the `esp_encrypted_img` component for pre-encrypted OTA images. The firmware is encrypted with an RSA-3072 key on the server side, transmitted encrypted, decrypted on the device, then re-encrypted by the flash hardware during write. This adds protection against compromised CDN or download infrastructure.

---

## Web Flash Compatibility

**ESPTool.js (the browser-based flasher used on simplego.dev) does NOT support encrypted flash operations.** It can only write plaintext firmware via the Web Serial API.

| Mode | Web Flash | idf.py flash | OTA |
|------|-----------|-------------|-----|
| Development | Works (bootloader re-encrypts on next boot, uses CRYPT_CNT) | Works | Works |
| Release | **Does not work** | **Does not work** | Works (only method) |

This is the fundamental reason why Mode 3/4 devices must be provisioned during manufacturing, not by the end user via web flash. The manufacturing workflow is:

```
1. Flash plaintext firmware via esptool
2. Burn flash encryption key via espefuse.py
3. Enable flash encryption (burn SPI_BOOT_CRYPT_CNT)
4. First boot: bootloader encrypts all partitions in-place (~1 min)
5. (Optional) Burn Secure Boot keys and enable
6. Burn release mode eFuses (lock everything down)
7. Device is sealed - only OTA updates from this point
```

---

## Manufacturing Workflow for Pre-Encrypted Firmware

For production at scale, pre-encrypting firmware on the host avoids the first-boot encryption delay and provides a more controlled process:

```bash
# Generate encryption key (or use per-device unique key)
espsecure.py generate_flash_encryption_key fe_key.bin

# Burn key to device
espefuse.py -c esp32s3 -p COM6 burn_key BLOCK_KEY2 fe_key.bin XTS_AES_128_KEY

# Pre-encrypt each binary with correct flash offset
espsecure.py encrypt_flash_data --aes_xts \
    --keyfile fe_key.bin \
    --address 0x0 \
    --output bootloader_enc.bin \
    build/bootloader/bootloader.bin

espsecure.py encrypt_flash_data --aes_xts \
    --keyfile fe_key.bin \
    --address 0x10000 \
    --output app_enc.bin \
    build/simplego.bin

# Flash pre-encrypted binaries
esptool.py -c esp32s3 -p COM6 write_flash \
    0x0 bootloader_enc.bin \
    0x10000 app_enc.bin

# Enable flash encryption
espefuse.py -c esp32s3 -p COM6 burn_efuse SPI_BOOT_CRYPT_CNT 7

# IMPORTANT: Delete key file from workstation immediately
shred -vfz fe_key.bin
```

The `--address` parameter is critical: XTS-AES ciphertext is address-dependent. If the offset does not match the flash write address exactly, the firmware will not decrypt correctly and the device will fail to boot with "flash read err" errors.

---

## Performance Impact

**First boot encryption:** Up to 60 seconds for large partitions (observed approximately 11 seconds for a 1 MB application). Do not interrupt power during this process - incomplete encryption corrupts the flash contents.

**Subsequent boots:** No measurable overhead. The XTS-AES decryption runs in the flash cache hardware, operating at the same speed as unencrypted flash reads.

**Runtime read performance:** No measurable overhead for memory-mapped flash access. The hardware decryption pipeline runs concurrently with the flash SPI transfer.

**Flash write performance:** Minimal overhead. The encryption hardware runs concurrently with the flash write operation. NVS writes (which are frequent in SimpleGo for ratchet state persistence) use their own HMAC-derived encryption and are not affected by flash encryption settings.

**Minimum write size:** 16 bytes (one AES block), 16-byte aligned. This is handled transparently by `esp_partition_write()` but is relevant if implementing custom low-level flash operations.

---

## Known Limitations

**ECB mode heritage:** Although XTS uses tweaking to differentiate blocks at different addresses, adjacent 32-byte block pairs (two consecutive 16-byte AES blocks) within the same tweak value are encrypted with AES in ECB mode. Identical 32-byte plaintext pairs at the same flash address produce identical ciphertext. This can leak information about flash contents to an attacker who can read the encrypted flash multiple times. In practice, this is a minor concern for SimpleGo because NVS data changes frequently (ratchet advancement) and firmware binary contains few identical 32-byte blocks.

**Not a substitute for NVS encryption:** Flash encryption protects the NVS partition's binary representation in flash, but the NVS library accesses data through the flash cache (which decrypts transparently). A firmware vulnerability that can read NVS data through the API still works regardless of flash encryption. The HMAC-based NVS encryption provides an additional layer that encrypts data at the NVS library level, protecting against API-level access without the correct HMAC-derived keys.

**Side-channel vulnerability:** As documented in [Known Vulnerabilities](./class-1-04-known-vulnerabilities.md), the AES hardware used for flash encryption has been broken via CPA with approximately 300,000 power measurements. An attacker who extracts the flash encryption key can decrypt the entire flash contents, including the encrypted NVS partition (though the NVS data has an additional HMAC encryption layer).

---

## References

| Source | Description |
|--------|-------------|
| ESP-IDF v5.5.2 Flash Encryption docs (ESP32-S3) | Complete flash encryption reference |
| IEEE 1619-2007 | XTS-AES standard specification |
| ESP-IDF Security Features Enablement Workflows | Step-by-step provisioning for FE + SB |
| NCC Group: Hardware Security by Design - ESP32 | Independent analysis of ESP32 flash encryption |
| espsecure.py documentation | Pre-encryption and key generation tools |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
