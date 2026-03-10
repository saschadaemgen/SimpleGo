
## Status after Session 45 - 2026-03-10

### Security Implementation (Session 45)
- SEC-01 CLOSED: sodium_memzero on PSRAM cache (123,600 bytes, 4 call sites)
- SEC-02 CLOSED: HMAC NVS vault (eFuse BLOCK_KEY1, HMAC_UP, auto-provisioned)
- SEC-03 CLOSED: mbedtls_platform_zeroize in smp_storage.c (Session 42)
- SEC-04 CLOSED: Auto-lock screen (60s timeout, memory wipe before lock)
- SEC-05 CLOSED: Device-bound HKDF (chip MAC in info parameter)
- SEC-06 DEFERRED: Post-quantum (sntrup761, pending Evgeny confirmation)
- **5 of 6 security findings CLOSED**

### Firmware
- SMP implementation: production-ready alpha
- 128 contacts, delivery receipts, encrypted SD history, WiFi Manager
- Display name: NVS-backed, first-boot prompt
- Auto-lock with secure memory wipe (PSRAM + LVGL labels)
- NVS vault mode: HMAC-encrypted keys at rest
- Device-bound HKDF: per-contact SD keys tied to chip MAC
- 47 source files with AGPL-3.0 headers

### Documentation
- Docusaurus 3 live at wiki.simplego.dev
- security-architecture/: 15 files covering Class 1 (3,243 lines)
- smp-in-c/ guide: 10 pages, world-first for C implementation of SMP

### Bugs
- Bug #20: SEND after 6+ hours idle (KNOWN, Alpha acceptable)
- Bug #21: SD card phantom counter after erase-flash (LOW)
- WiFi rebuild_timer_cb crash on erase-flash + first-boot (pre-existing, auto-recovers)

### Open Items
- SEC-06: Post-quantum (confirm sntrup761 with Evgeny)
- Alpha firmware binary for simplego.dev/installer
- sdkconfig.defaults management (93 KB, git strategy needed)
- ARCHITECTURE_AND_SECURITY.md update with closed SEC findings
