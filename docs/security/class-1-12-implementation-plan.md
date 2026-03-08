---
title: "Class 1 - Implementation Plan"
sidebar_position: 13
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Implementation Plan

**Document version:** Session 44 | March 2026
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Purpose

This document translates the Hardware Class 1 security architecture into concrete implementation tasks for Prinzessin Hasi (implementation instance). Each task specifies exactly which files to modify, what code to add, how to test, and what build command to use. Tasks are ordered by dependency - each phase must be completed and verified before the next begins.

---

## Phase 1: Runtime Memory Protection (SEC-01 + SEC-04)

**Priority:** Critical. Must be completed before any other security work.
**Dependency:** None (no eFuse changes, no config changes).
**Build command:** `idf.py build flash monitor -p COM6` (normal build, no erase-flash needed).

### Task 1A: sodium_memzero on s_msg_cache

**File:** `ui/screens/ui_chat.c`

**Changes:**

Add `sodium_memzero()` call in `ui_chat_cleanup()` before nulling static pointers. Add `sodium_memzero()` call at the beginning of the contact-switch / message-load function, before loading new contact's messages. Add `sodium_memzero()` call in a new `simplego_screen_lock()` function.

**Test procedure:**
1. Send 10 messages in a conversation
2. Navigate away from chat screen
3. Verify via serial log that cleanup was called
4. (Advanced) Connect JTAG, dump PSRAM at 0x3C000000, search for message text - expect zero hits

### Task 1B: LVGL Label Clearing

**File:** `ui/screens/ui_chat.c`, `ui/screens/ui_chat_bubble.c`

**Changes:**

Before destroying chat screen, iterate all visible bubble labels and call `lv_label_set_text(label, "")` to overwrite the text buffer. Then proceed with normal screen destruction.

**Test procedure:**
1. View chat with messages
2. Navigate to contacts screen
3. Verify LVGL pool usage returns to baseline (should already work from Session 41f lifecycle fix)

### Task 1C: Screen Lock with Memory Wipe

**Files:** `ui/ui_manager.c`, `ui/screens/ui_chat.c`, new file `ui/screens/ui_lock.c`

**Changes:**

Implement display timeout handler that calls `simplego_screen_lock()`. This function zeros s_msg_cache, clears LVGL labels, and navigates to a lock screen. The lock screen requires any keyboard input to unlock (full PIN-based unlock is a future enhancement).

**Test procedure:**
1. View chat with messages
2. Wait for display timeout (or manually trigger)
3. Verify lock screen is displayed
4. Verify PSRAM message cache is zeroed
5. Press any key to unlock
6. Verify chat reloads messages from encrypted SD card

**Build type:** Normal build. No erase-flash required.

---

## Phase 2: HMAC NVS Vault (SEC-02 + SEC-05)

**Priority:** Critical. Core security feature for Alpha release.
**Dependency:** Phase 1 should be completed first (defense-in-depth), but is not a hard blocker.
**Build command:** `idf.py erase-flash` then `idf.py build flash monitor -p COM6` (erase-flash REQUIRED because NVS format changes).

### Task 2A: sdkconfig Changes

**File:** `sdkconfig` (via `idf.py menuconfig`)

**Changes:**

```
CONFIG_NVS_ENCRYPTION=y
CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC=y
CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID=1
```

Menuconfig path: Component config -> NVS -> Enable NVS encryption -> HMAC peripheral-based scheme -> Key ID = 1.

### Task 2B: NVS Initialization Update

**File:** `main.c` (or dedicated `core/simplego_security.c`)

**Changes:**

Replace the existing `nvs_flash_init()` call with error-handling code that supports the migration from unencrypted to encrypted NVS:

```c
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS format change detected, erasing for encrypted init");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);
```

When the sdkconfig options are set, `nvs_flash_init()` internally handles HMAC key provisioning (generates and burns key if not present) and secure initialization.

**Verification:**

After first boot with new config, run `espefuse.py -c esp32s3 -p COM6 summary` and verify:
- BLOCK_KEY1 shows purpose = HMAC_UP
- BLOCK_KEY1 is read-protected (R/- in summary)
- BLOCK_KEY1 is write-protected (shows WR_DIS bit set)

### Task 2C: HKDF Device Binding (SEC-05)

**File:** `state/smp_history.c`

**Changes:**

Update the HKDF info parameter for SD card per-contact key derivation to include a device-specific component. Use `esp_hmac_calculate()` with a binding constant to derive a device identifier from the eFuse HMAC key:

```c
// Derive device-specific binding from HMAC key
uint8_t device_binding[32];
esp_hmac_calculate(HMAC_KEY1, "simplego-device-binding", 23, device_binding);

// Include in HKDF info: "simplego-slot-XX-AABBCCDD..."
char info[80];
snprintf(info, sizeof(info), "simplego-slot-%02d-%08x%08x",
         slot_index,
         *(uint32_t*)&device_binding[0],
         *(uint32_t*)&device_binding[4]);
```

**Test procedure:**
1. Flash device, establish contact, send messages
2. Remove SD card, insert in second SimpleGo device
3. Verify second device cannot decrypt chat history (different device binding)

**Build type:** `idf.py erase-flash` required (NVS format change + new HKDF parameters invalidate existing SD card keys).

### CRITICAL WARNING FOR PHASE 2

**Test on a sacrificial T-Deck first.** The eFuse burn in Task 2B is irreversible. If the automatic provisioning has a bug (wrong key block, wrong purpose), BLOCK_KEY1 is consumed. The device is not bricked (firmware is unencrypted, reflash works), but the burned block cannot be reused. SimpleGo would need to use BLOCK_KEY0 or another block instead.

**Recommended test sequence:**
1. Use `espefuse.py --virt` (virtual mode) to simulate the provisioning
2. Flash firmware to a test T-Deck
3. Monitor serial output for provisioning log messages
4. Run `espefuse.py summary` to verify correct state
5. Test `idf.py erase-flash` followed by reflash (verify vault survives)
6. Only after all tests pass: announce Alpha release

---

## Phase 3: Alpha Release Preparation

**Priority:** High. Enables public testing.
**Dependency:** Phase 2 completed and verified.

### Task 3A: Firmware Binary for Web Installer

**File:** Build output

**Changes:**

Build the release firmware binary with NVS encryption enabled. Prepare the binary for simplego.dev/installer (ESP Web Tools). Include version number, build date, and SHA-256 hash in the download page.

### Task 3B: Alpha Warning and Version Display

**File:** `ui/screens/ui_splash.c`, `ui/screens/ui_settings_info.c`

**Changes:**

Display "ALPHA - NOT FOR PRODUCTION USE" on splash screen. Show firmware version, build date, and security mode (Open / NVS Vault / Fortress / Bunker) on the device info screen.

### Task 3C: Documentation Update

**Files:** `ARCHITECTURE_AND_SECURITY.md`, `docs/protocol-analysis/STATUS.md`, `docs/protocol-analysis/BUG_TRACKER.md`

**Changes:**

Update SEC finding status:
- SEC-01: CLOSED (Session 44, sodium_memzero implemented)
- SEC-02: CLOSED (Session 44, HMAC NVS vault implemented)
- SEC-04: CLOSED (Session 44, screen lock with memory wipe)
- SEC-05: CLOSED (Session 44, device-bound HKDF)
- SEC-06: Status unchanged (DEFERRED, verified feasible, see Post-Quantum Readiness doc)

---

## Phase 4: Future - Flash Encryption and Secure Boot (Mode 3/4)

**Priority:** For Kickstarter production devices.
**Dependency:** Phase 2 fully stable, OTA infrastructure operational.

### Task 4A: Flash Encryption Provisioning Script

Create a manufacturing script that automates the Mode 3 provisioning process:

```bash
#!/bin/bash
# simplego_provision_mode3.sh
# WARNING: Irreversible eFuse operations

PORT=$1
FIRMWARE=$2

echo "=== SimpleGo Mode 3 Provisioning ==="
echo "Port: $PORT"
echo "Firmware: $FIRMWARE"
echo ""
echo "This will PERMANENTLY enable flash encryption."
echo "Web flash and USB flash will NO LONGER WORK."
echo ""
read -p "Type PROVISION to continue: " confirm
[ "$confirm" != "PROVISION" ] && exit 1

# Step 1: Verify HMAC key already present (from Mode 2 first boot)
espefuse.py -c esp32s3 -p $PORT summary | grep "KEY_PURPOSE_1" | grep "HMAC_UP"
[ $? -ne 0 ] && echo "ERROR: HMAC key not provisioned" && exit 1

# Step 2: Generate and burn flash encryption key
espsecure.py generate_flash_encryption_key /tmp/fe_key.bin
espefuse.py -c esp32s3 -p $PORT burn_key BLOCK_KEY2 /tmp/fe_key.bin XTS_AES_128_KEY

# Step 3: Enable flash encryption permanently
espefuse.py -c esp32s3 -p $PORT burn_efuse SPI_BOOT_CRYPT_CNT 7

# Step 4: Flash firmware (will be encrypted on first boot)
idf.py -p $PORT flash

# Step 5: Cleanup
shred -vfz /tmp/fe_key.bin

echo "=== Provisioning complete. Device will encrypt flash on next boot. ==="
echo "=== DO NOT interrupt power during first boot (~60 seconds). ==="
```

### Task 4B: Secure Boot Provisioning (Mode 4)

Create signing key management infrastructure and provisioning script for Mode 4. This requires the HSM or secure key storage infrastructure described in the Secure Boot V2 document.

### Task 4C: OTA Update Server

Implement the signed OTA update infrastructure on api.simplego.dev:
- Firmware hosting with version management
- Ed25519 or RSA-3072 signature verification endpoint
- Device authentication (chip ID based)
- Rollback protection (version comparison)

---

## Phase Summary

| Phase | SEC Findings Closed | Deliverable | Build Type |
|-------|-------------------|-------------|------------|
| 1 | SEC-01, SEC-04 | Memory protection, screen lock | Normal build |
| 2 | SEC-02, SEC-05 | HMAC vault, device-bound SD encryption | erase-flash required |
| 3 | (none, prep) | Alpha release binary, documentation update | Normal build |
| 4 | (future) | Flash encryption, Secure Boot, OTA server | Manufacturing provisioning |

After Phase 3, SimpleGo has four of six SEC findings closed, the HMAC vault protecting all stored keys, runtime memory hygiene preventing RAM-based key theft, and a publicly available Alpha firmware on simplego.dev. Post-quantum (SEC-06) remains deferred as planned, with verified feasibility documented.

---

## Session 44 Deliverables (Hasi Tasks)

To summarize what Hasi should implement in this session:

| Task | File(s) | Description | Build |
|------|---------|-------------|-------|
| 1A | ui/screens/ui_chat.c | sodium_memzero on s_msg_cache (4 call sites) | Normal |
| 1B | ui/screens/ui_chat.c, ui_chat_bubble.c | LVGL label clearing before screen destroy | Normal |
| 1C | ui/ui_manager.c, new ui_lock.c | Screen lock with memory wipe on timeout | Normal |
| 2A | sdkconfig | Enable NVS encryption config options | menuconfig |
| 2B | main.c | Replace nvs_flash_init with migration-safe init | erase-flash |
| 2C | state/smp_history.c | HKDF device binding via esp_hmac_calculate | erase-flash |

**Task sequencing:** 1A, 1B, 1C can be done in parallel (independent changes). 2A must be done before 2B. 2C depends on 2B (needs working HMAC). Recommended: implement Phase 1 first, verify, then Phase 2.

**Commit format (Conventional Commits):**
```
feat(security): SEC-01 secure zeroing of message cache on all transitions
feat(security): SEC-04 screen lock with PSRAM and LVGL memory wipe
feat(security): SEC-02 HMAC-based NVS encryption with first-boot provisioning
feat(security): SEC-05 device-bound HKDF for SD card encryption
```

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
