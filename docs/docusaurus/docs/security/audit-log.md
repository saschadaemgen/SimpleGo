---
title: Security Audit Log
sidebar_position: 4
---

# Security Audit Log

A record of security-relevant bugs discovered and resolved during SimpleGo development. Maintained for transparency and to document the security improvement trajectory of the project.

## Severity Levels

- **Critical** -- exploitable remotely or causes key material exposure
- **High** -- exploitable locally or causes crypto state corruption
- **Medium** -- degrades security guarantees without full compromise
- **Low** -- minor hardening issues or best-practice violations

---

## Resolved Issues

### SPI2 Mutex Hold During Crypto Operations
- **Severity:** High
- **Component:** SD card / LVGL layer
- **Root cause:** Crypto operations were performed while holding the SPI2 mutex, causing mutex hold times of ~500ms. This blocked display updates and created a timing side-channel for crypto operations.
- **Resolution:** Refactored to complete all crypto operations before acquiring the mutex. Hold time reduced to under 10ms.
- **Session:** 38

### HISTORY_MAX_TEXT Truncation Before SD Write
- **Severity:** Critical
- **Component:** Chat history storage
- **Root cause:** A proposed change would have truncated message text to `HISTORY_DISPLAY_TEXT` (512 chars) before writing to SD card, causing permanent data loss for messages over 512 characters.
- **Resolution:** Established architectural rule -- truncation happens only in the LVGL bubble layer (`HISTORY_DISPLAY_TEXT` = 512). SD write path always uses full `HISTORY_MAX_TEXT` = 4096. Change was caught before implementation via the "Frage VOR Entscheidung" principle.
- **Session:** 36

### Hardware AES DMA Conflict
- **Severity:** High
- **Component:** mbedTLS / PSRAM
- **Root cause:** ESP32-S3 hardware AES DMA conflicts with PSRAM at the silicon level, causing intermittent memory corruption during crypto operations.
- **Resolution:** `CONFIG_MBEDTLS_HARDWARE_AES=n` set permanently. Software AES used throughout.
- **Session:** 12

### WPA3 SAE Authentication Failure
- **Severity:** Medium
- **Component:** WiFi Manager
- **Root cause:** Using `WIFI_AUTH_WPA_WPA2_PSK` as auth threshold caused SAE handshake failures on WPA3 networks.
- **Resolution:** Changed to `WIFI_AUTH_WPA2_PSK` as threshold.
- **Session:** 29

### PSRAM Task Stack NVS Write Failure
- **Severity:** High
- **Component:** Ratchet state persistence
- **Root cause:** Ratchet state NVS writes executed from a task with PSRAM-allocated stack, causing silent write failures due to SPI Flash cache conflict.
- **Resolution:** NVS writes moved to main task with internal SRAM stack.
- **Session:** 31

### Reply Queue E2E Key Location
- **Severity:** Critical
- **Component:** SMP handshake
- **Root cause:** E2E key for Reply Queue was being sought outside `PHConfirmation`, causing handshake failure and inability to establish contact.
- **Resolution:** Key correctly extracted from `PHConfirmation` payload per Evgeny Poberezkin confirmation.
- **Session:** 17

---

## Open Security Items

- Security log categories: 5 log categories still emit security-relevant data in debug builds. Cleanup scheduled for pre-Kickstarter hardening phase.
- eFuse + NVS Flash Encryption: `nvs_flash_secure_init` with eFuse keys not yet implemented. Scheduled together with CRYSTALS-Kyber for Kickstarter phase.
- Private Message Routing: Not yet implemented. Required for production-grade metadata protection. Scheduled post-MVP.
