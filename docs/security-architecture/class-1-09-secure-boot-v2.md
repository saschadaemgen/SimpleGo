---
title: "Class 1 - Secure Boot V2"
sidebar_position: 9
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Secure Boot V2

**Document version:** Session 44 | March 2026
**Applies to:** ESP32-S3 (Mode 4), ESP32-P4
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## What Secure Boot Protects

Secure Boot ensures that only firmware signed with IT and More Systems' private key can execute on the device. Without Secure Boot, an attacker with physical access can flash malicious firmware that extracts cryptographic keys from RAM, bypasses security checks, or exfiltrates message content. With Secure Boot, any unsigned or incorrectly signed firmware is rejected at boot before any application code runs.

For SimpleGo, Secure Boot is essential in Mode 4 (Bunker) and recommended for Mode 3 (Fortress) production devices. It completes the security chain: the HMAC vault protects keys in flash, runtime hygiene protects keys in RAM, and Secure Boot ensures that only trusted firmware handles those keys.

---

## How Secure Boot V2 Works on ESP32-S3

### The Verification Chain

```
Power On
  |
  v
ROM Bootloader (immutable, in chip silicon)
  |  Reads SECURE_BOOT_EN eFuse
  |  If enabled: verify 2nd-stage bootloader signature
  |    - Compute SHA-256 of public key in signature block
  |    - Compare against digest(s) stored in eFuse BLOCK_KEY3/4/5
  |    - If match: verify RSA-PSS signature over bootloader image
  |    - If verification fails: HALT (device does not boot)
  |
  v
2nd-Stage Bootloader (in encrypted flash)
  |  Reads application image from flash
  |  Verifies application signature using same method
  |  If verification fails: HALT or rollback to previous OTA partition
  |
  v
SimpleGo Application Firmware (verified, trusted)
```

### Signature Algorithm

The ESP32-S3 uses RSA-PSS (PKCS#1 v2.1, RFC 8017 Section 8.1) with RSA-3072 keys and SHA-256 hash. Each signed image has a 1,216-byte signature block appended, containing the RSA-3072 public key (modulus + exponent), pre-computed Montgomery multiplication constants (for hardware-accelerated verification), and the RSA-PSS signature over the image content.

The ESP32-P4 additionally supports ECDSA-256 and ECDSA-384 for Secure Boot, which produces smaller signature blocks and faster verification. For SimpleGo, RSA-3072 is recommended for compatibility across both S3 and P4 platforms.

### Three Key Slots

Up to three independent RSA-3072 key pairs can be provisioned. Their SHA-256 digests are stored in three eFuse key blocks (SECURE_BOOT_DIGEST0/1/2). Any single valid signature is sufficient for verification - the image does not need to be signed by all three keys.

This enables key rotation: Provision all three keys at manufacturing. Sign firmware with Key 0 initially. If Key 0 is suspected compromised, push an OTA update signed with Key 1, then revoke Key 0 via eFuse. Key 2 remains as a final backup. New keys cannot be added after manufacturing because the eFuse digest slots are permanently assigned.

---

## Key Management: The Most Critical Operational Decision

If all three signing keys are lost, the device can never receive another firmware update. Existing firmware continues running but is frozen forever. This is the single most consequential operational risk in SimpleGo's entire security architecture.

### Key Generation

```bash
# Generate RSA-3072 signing key (do this on a secure, air-gapped machine)
idf.py secure-generate-signing-key --version 2 --scheme rsa3072 signing_key_0.pem
idf.py secure-generate-signing-key --version 2 --scheme rsa3072 signing_key_1.pem
idf.py secure-generate-signing-key --version 2 --scheme rsa3072 signing_key_2.pem
```

### Storage Requirements

| Key | Primary Storage | Backup Storage | Geographic Location |
|-----|----------------|----------------|-------------------|
| Key 0 (active) | Hardware Security Module (HSM) or encrypted USB drive | Separate encrypted USB in fireproof safe | IT and More Systems office, Recklinghausen |
| Key 1 (reserve) | Separate HSM or encrypted USB drive | Separate encrypted USB in bank safe deposit box | Different physical location |
| Key 2 (emergency) | Offline-only encrypted storage | Paper key backup (BIP39 or similar mnemonic) | Third physical location |

The private keys must NEVER exist on a network-connected computer, in cloud storage, in a Git repository, or in any CI/CD pipeline. Signing happens on a dedicated air-gapped machine or via HSM.

### Signing Workflow for OTA Updates

```bash
# Build unsigned firmware
idf.py build

# Sign on air-gapped machine (or via HSM)
idf.py secure-sign-data --version 2 \
    --keyfile signing_key_0.pem \
    --output build/simplego_signed.bin \
    build/simplego.bin

# Upload signed binary to OTA server
scp build/simplego_signed.bin deploy@api.simplego.dev:/firmware/
```

For CI/CD automation without exposing the key, ESP-IDF supports PKCS#11 HSM integration: `pip install 'esptool[hsm]'`. The HSM performs the signing operation internally - the private key never leaves the HSM boundary.

---

## Key Revocation

Individual key slots can be permanently revoked by burning the corresponding eFuse bits:

```bash
# Revoke Key 0 (ONLY after confirming Key 1 works)
espefuse.py -c esp32s3 -p COM6 burn_efuse SECURE_BOOT_KEY_REVOKE0
```

**The safe revocation procedure:**

```
1. Push OTA update signed with Key 1 (the NEW key)
2. Verify device boots successfully with Key 1 signed firmware
3. Test full functionality (messaging, encryption, all features)
4. Only THEN revoke Key 0
5. Verify device still boots (using Key 1)
6. Confirm revocation via espefuse.py summary
```

**Aggressive revocation mode** (CONFIG_SECURE_BOOT_AGGRESSIVE_REVOKE) automatically revokes any key whose signature verification fails cryptographically during boot. This is dangerous because a firmware build accidentally signed with the wrong key would trigger permanent revocation. Not recommended for SimpleGo.

---

## Anti-Rollback Protection

The SECURE_VERSION eFuse field (16 bits) stores the minimum accepted firmware security version. When a new firmware with a higher security version is successfully validated via OTA, the eFuse counter is updated. Older firmware versions are then rejected at boot.

```c
// In firmware's app_description
const esp_app_desc_t app_desc = {
    .secure_version = 3,  // Increment when patching security vulnerabilities
    // ...
};
```

The 16-bit field supports up to 65,535 version increments. At one security update per week, this is sufficient for over 1,200 years of updates.

Anti-rollback prevents an attacker from flashing an older firmware version with known vulnerabilities. Combined with Secure Boot (only signed firmware runs) and Flash Encryption (firmware cannot be modified in flash), this creates a complete firmware integrity chain.

---

## Boot Time Impact

RSA-3072 signature verification takes approximately 10-18 ms per verification at the ROM bootloader's clock speed (lower than the application clock). With two verifications (bootloader + application), the total Secure Boot overhead is approximately 20-40 ms. This is imperceptible in SimpleGo's boot sequence, which takes several seconds for WiFi initialization and TLS handshake.

---

## Combined Provisioning Order

When enabling both Flash Encryption and Secure Boot (Mode 4), the provisioning order is critical due to eFuse protection conflicts (see eFuse Architecture document for full explanation):

```
Step 1: Burn HMAC key for NVS encryption (BLOCK_KEY1, HMAC_UP)
Step 2: Burn Flash Encryption key (BLOCK_KEY2, XTS_AES_128_KEY)
Step 3: Enable Flash Encryption (SPI_BOOT_CRYPT_CNT = 0b111 for Release)
Step 4: First boot - bootloader encrypts flash in-place
Step 5: Burn Secure Boot digest(s) (BLOCK_KEY3/4/5, SECURE_BOOT_DIGEST0/1/2)
Step 6: Enable Secure Boot (SECURE_BOOT_EN = 1)
        --> This write-protects RD_DIS, locking all protection state
Step 7: Burn remaining lockdown eFuses (DIS_DOWNLOAD_MODE, DIS_USB_JTAG, etc.)
Step 8: Verify entire eFuse state via espefuse.py summary
Step 9: Test OTA update cycle end-to-end
Step 10: Only ship device after OTA test succeeds
```

**Steps 5 and 6 MUST come after Steps 2 and 3.** If Secure Boot is enabled before Flash Encryption, the Flash Encryption key cannot be read-protected (because Secure Boot write-protects the RD_DIS register), leaving the encryption key software-readable and defeating its purpose.

---

## What Happens If Things Go Wrong

| Scenario | Consequence | Recovery |
|----------|-------------|----------|
| Secure Boot enabled with correct signed firmware | Normal operation | N/A |
| Secure Boot enabled, firmware not signed | Device does not boot | None - device is permanently bricked |
| Signed firmware, but wrong key | Boot fails | None (unless another key slot has correct digest) |
| OTA update with wrong signature | Update rejected, previous firmware continues | Push correctly signed update |
| All 3 keys revoked | No firmware passes verification | None - device is permanently bricked for updates |
| All 3 keys lost (not revoked) | Device works but can never be updated | None - device frozen at current version |
| JTAG disabled + boot failure | Cannot debug | None - must have working OTA path |

**The golden rule:** Always test the complete provisioning and OTA cycle on a sacrificial device before provisioning production units.

---

## References

| Source | Description |
|--------|-------------|
| ESP-IDF v5.5.3 Secure Boot V2 docs (ESP32-S3) | Complete implementation reference |
| ESP-IDF v5.5.3 Secure Boot V2 docs (ESP32-P4) | P4-specific additions (ECDSA support) |
| RFC 8017 | PKCS#1 v2.1 RSA-PSS specification |
| ESP-IDF Security Features Enablement Workflows | Step-by-step provisioning |
| esptool HSM documentation | PKCS#11 integration for automated signing |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
