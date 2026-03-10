# Part 42: Session 45 - Security Implementation (SEC-01, SEC-02, SEC-04, SEC-05)
**Date:** 2026-03-10 | **Version:** v0.1.18-alpha

## Overview

Four Security Findings closed in one session, two of them CRITICAL. Phase 1 implemented runtime memory protection (SEC-01 PSRAM plaintext wipe + SEC-04 auto-lock with memory wipe). Phase 2 implemented HMAC NVS vault encryption with device-bound HKDF (SEC-02 + SEC-05). After this session, 5 of 6 security findings are closed. Only SEC-06 (post-quantum) remains, intentionally deferred pending Evgeny's sntrup761 confirmation.

## Phase 1: Runtime Memory Protection

### SEC-01 CLOSED (CRITICAL): PSRAM Plaintext Wipe

The biggest open security hole: 30 decrypted messages sat as plaintext in PSRAM (`s_msg_cache`), never zeroed. Physical attacker with JTAG could read all messages while device was powered.

New function `ui_chat_secure_wipe()` in ui_chat.c does two things:

1. `wipe_labels_recursive()` iterates the complete LVGL object tree under msg_container and sets every label text to empty before objects are destroyed. This is best-effort because LVGL does not guarantee its internal pool is overwritten, but better than leaving plaintext in the pool.

2. `sodium_memzero()` on the entire s_msg_cache - all 30 slots, 123,600 bytes. sodium_memzero cannot be optimized away by the compiler, unlike memset (CWE-14).

The wipe function is called at four points: in `ui_chat_cleanup()` when the chat screen is destroyed, in `ui_chat_switch_contact()` before loading a new contact's messages, in `ui_chat_cache_history()` before copying new history data into the cache, and from `ui_manager_lock()` before showing the lock screen.

Serial log on chat exit shows: `SEC-04: Bubble labels wiped` followed by `SEC-01: PSRAM msg cache wiped (123600 bytes)`.

### SEC-04 CLOSED (HIGH): Auto-Lock with Memory Wipe

Two new files: `ui_lock.c` and `ui_lock.h`. The lock screen shows a lock icon and "Press any key to unlock". A hidden LVGL textarea captures keyboard input and calls `ui_manager_unlock()` on any keypress, which returns to the previous screen via `go_back()`.

In `ui_manager.c`, an LVGL timer checks `lv_disp_get_inactive_time()` every 2 seconds. After 60 seconds without input, `ui_manager_lock()` is called, which first executes `ui_chat_secure_wipe()` and then navigates to the lock screen. The SMP connection and PING/PONG continue running during lock.

Minor bug during implementation: `UI_FONT_LG` does not exist in the theme. Used in the lock screen initially, mein Prinz corrected to `UI_FONT_MD`.

Phase 1 commits on main device (COM6): `0faf617` (SEC-01) and `85a4906` (SEC-04).

## Phase 2: HMAC NVS Vault (SEC-02 + SEC-05)

### SEC-02 CLOSED (CRITICAL): HMAC NVS Encryption

All cryptographic keys previously sat as plaintext in NVS flash. Physical attacker reading the flash chip via SPI tap or desolder could extract all private keys.

Implementation uses ESP-IDF's HMAC-based NVS encryption scheme. `nvs_flash_init()` with activated HMAC scheme handles the complete eFuse provisioning automatically. The code in main.c is minimal: it logs the Security Mode (Open or Vault) and catches NVS init errors with a halt instead of fallback to unencrypted.

First boot provisioning sequence (verified on Opfer-T-Deck COM8):
```
NVS Encryption - Registering HMAC-based scheme
Could not find HMAC key in configured eFuse block
Generating NVS encr-keys
BURN BLOCK5 - OK
BURN BLOCK0 - OK
NVS partition nvs is encrypted
```

espefuse.py summary confirms: KEY_PURPOSE_1 = HMAC_UP with status R/- (write-protected), BLOCK_KEY1 shows only question marks (read-protected). BLOCK_KEY0 and BLOCK_KEY2-5 remain empty and available as reserve.

### SEC-05 CLOSED (MEDIUM): Device-Bound HKDF

Previously the HKDF info parameter for SD card encryption was only the slot index (one byte). Same master key on another device with same slot index would produce identical derived keys.

Fix: HKDF info parameter strengthened from single byte to `"simplego-slot-XX-AABBCCDDEEFF"` where the last 12 hex characters are the chip MAC from `esp_efuse_mac_get_default()`. Each device now derives unique per-contact encryption keys even with the same master key.

### Build System Challenges

**sdkconfig.defaults structure:** Initial approach of minimal handwritten defaults was wrong. Deleting sdkconfig and regenerating from incomplete defaults caused missing LVGL modules (lv_qrcode, lv_font_montserrat_10) and build failures. Solution: export the complete working sdkconfig as sdkconfig.defaults (93 KB, intentionally large as single source of truth).

**Windows PowerShell semicolon issue:** `-D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.vault"` does not work in PowerShell - the semicolon causes the vault file to be ignored. Pragmatic solution: append vault lines via `Add-Content` directly to sdkconfig.defaults. Mode switching requires editing the four lines at the end, then `del sdkconfig` and `fullclean`.

**Spurious esp_efuse.h include:** Added to main.c despite not calling any eFuse functions directly (nvs_flash_init handles everything internally). Caused linker error because efuse was not in CMakeLists.txt PRIV_REQUIRES. Same mistake made twice due to not re-exporting the corrected file.

Phase 2 commits prepared, pending push.

## Security Status After Session 45

| ID | Severity | Description | Status |
|----|----------|-------------|--------|
| SEC-01 | CRITICAL | PSRAM plaintext wipe (s_msg_cache) | **CLOSED (Session 45)** |
| SEC-02 | CRITICAL | HMAC NVS vault (eFuse BLOCK_KEY1) | **CLOSED (Session 45)** |
| SEC-03 | HIGH | mbedtls_platform_zeroize in smp_storage.c | CLOSED (Session 42) |
| SEC-04 | HIGH | Auto-lock with memory wipe (60s timeout) | **CLOSED (Session 45)** |
| SEC-05 | MEDIUM | Device-bound HKDF (chip MAC in info) | **CLOSED (Session 45)** |
| SEC-06 | MEDIUM | Post-quantum (sntrup761/Kyber) | DEFERRED (pending Evgeny) |

5 of 6 closed. Only SEC-06 remains, intentionally deferred.

## Known Issues

**WiFi rebuild_timer_cb crash:** Pre-existing bug, not a Phase 2 problem. Occurs in ui_settings_wifi.c:602 when name-setup and WiFi-setup screens are simultaneously active during erase-flash + first-boot. Device reboots automatically and runs normally after.

**sdkconfig.defaults size (93 KB):** Intentionally large (complete config as source of truth). Makes git diffs noisy. Consider .gitignore for the file and version only overlay files.

**sdkconfig.defaults.open / .vault:** Exist in project root but not used via -D flag due to Windows limitation. Manual append workflow documented.

## Files Changed

Phase 1: `ui_chat.c`, `ui_chat.h` (modified), `ui_lock.c`, `ui_lock.h` (new), `ui_manager.c`, `ui_manager.h` (modified).

Phase 2: `sdkconfig.defaults` (replaced), `sdkconfig.defaults.open` (new), `sdkconfig.defaults.vault` (new), `Kconfig.projbuild` (modified), `main.c` (modified), `smp_history.c` (modified).

## Lessons Learned

**L238 (CRITICAL):** sodium_memzero() must be called at every cache transition point, not just on screen destroy. Four call sites identified: cleanup, contact switch, history load, and screen lock. Missing any one leaves a window where plaintext persists in PSRAM.

**L239 (HIGH):** LVGL label text wipe is best-effort only. LVGL's internal memory pool does not guarantee overwrite on object deletion. Setting label text to empty string before deletion is the closest approximation, but LVGL pool fragments may retain text. True LVGL pool zeroing would require a custom allocator.

**L240 (HIGH):** ESP-IDF HMAC NVS encryption is fully automatic after sdkconfig activation. nvs_flash_init() handles eFuse detection, key generation, block burning, and partition encryption on first boot. No manual espefuse.py commands needed for provisioning.

**L241:** Windows PowerShell does not correctly handle semicolons in `-D SDKCONFIG_DEFAULTS="a;b"`. The second file is silently ignored. Workaround: merge overlay content into the main defaults file via script or manual append.

**L242:** A complete working sdkconfig exported as sdkconfig.defaults (93 KB) is safer than a minimal handwritten one. Incomplete defaults cause missing LVGL modules and other hard-to-diagnose build failures when sdkconfig is regenerated.

---

*Part 42 - Session 45 Security Implementation*
*SimpleGo Protocol Analysis*
*Date: March 10, 2026*
*Bugs: 73 total (unchanged)*
*Lessons: 242 total*
*Security: 5/6 findings CLOSED*
*Milestone 21: Runtime Security + HMAC NVS Vault*
