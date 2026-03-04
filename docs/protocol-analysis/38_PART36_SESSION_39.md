![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 36: Session 39
# WiFi Manager: The First On-Device WiFi Setup for T-Deck

**Document Version:** v1
**Date:** 2026-03-03 Session 39
**Status:** COMPLETED -- First functional on-device WiFi manager for T-Deck hardware
**Previous:** Part 35 - Session 38 (The SPI2 Bus Hunt)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 39 SUMMARY

```
After analyzing every relevant T-Deck project (Meshtastic, MeshCore,
Bruce, ESP32Berry, Armachat, ESPP), no single project has an on-device
WiFi manager. SimpleGo is the first.

1 Major Feature: Complete WiFi Manager with On-Device UI
9 Bugs Fixed (#62-#70)
4 New Lessons (#210-#213)
15 Files Changed
10+ Commits

First-Boot Flow: Power on > WiFi Manager auto-launches > Select network
                > Enter password > Connected. No development tools needed.

MILESTONE 15: On-Device WiFi Manager
```

---

## Starting Point

Session 38 ended with backlight control working and the SPI2 bus root cause identified. WiFi connection still required hardcoded Kconfig credentials. The WiFi Manager in Settings existed but was unstable: scans showed inconsistent results, network switching was impossible (device always reconnected to old network), and UI froze during WiFi operations. No first-boot flow existed for end users.

Goal: A fully functional WiFi Manager with on-device UI that works for both developers and end users.

---

## Phase 1: Market Research

### No T-Deck Project Has an On-Device WiFi Manager

Exhaustive research across all relevant projects:

| Project | WiFi Setup Method | On-Device UI? |
|---------|------------------|---------------|
| Meshtastic (9,000 commits) | CLI, phone app, web UI | No |
| LilyGo Factory Firmware | Hardware examples only | No |
| ESP32Berry | LVGL WiFi Manager (Arduino) | Yes, but archived May 2024 |
| Bruce Firmware | Best WiFi Manager (TFT_eSPI) | Yes, but not LVGL |
| MichMich Provisioner | Web portal | No |
| Espressif wifi_provisioning | BLE or web portal | No |
| bdcabreran WiFi Panel | ESP-IDF 5.x + LVGL scan | Partial |
| ESPP Framework | T-Deck HAL abstraction | No |

### Architecture Decision

Combine ESP32Berry UI patterns (LVGL list, keyboard input, same hardware) with Bruce backend intelligence (multi-network concept, auto-connect, retry logic). Adapted for ESP-IDF 5.5.2 + LVGL 9.

---

## Phase 2: The Dual-File War (Bug #62)

### Problem: Race Condition Between Two WiFi Files

smp_wifi.c and wifi_manager.c fought each other. smp_wifi.c had an unconditional auto-reconnect handler that immediately reconnected to the old network on every DISCONNECT event. When wifi_manager.c tried to disconnect to switch networks, smp_wifi.c instantly reconnected back.

### Fix: Unified wifi_manager.c

Both files merged into a single wifi_manager.c. One state machine, one event handler chain, no conflicts. Kconfig credentials eliminated as priority source, NVS-only storage.

---

## Phase 3: UI Freeze Bugs (#63, #64)

### Bug #63: vTaskDelay in LVGL Context

deferred_wifi_rebuild() called vTaskDelay(pdMS_TO_TICKS(200)), blocking the LVGL render task for 200ms. Fix: Replaced with lv_timer_create() one-shot timer.

### Bug #64: WiFi Scan Race Condition

wifi_scan_poll_cb() called ESP-IDF APIs directly instead of using cached results from the backend. Fix: Switched to wifi_manager_get_scan_count() and wifi_manager_get_scan_results().

---

## Phase 4: Scan Timing Bug (#65)

500ms stale flag guard ignored wifi_manager_is_scan_done() for the first 500ms after scan start. If ESP32 completed the scan in under 500ms, the poll missed the done signal. Fix: Guard removed entirely.

---

## Phase 5: WPA3 SAE Authentication (Bug #66)

auth -> init (0x600) on every connection attempt. Router ran WPA2/WPA3 Transition Mode. WIFI_AUTH_WPA_WPA2_PSK threshold made ESP32 attempt WPA3-SAE, but SAE negotiation on ESP32-S3 failed consistently. Fix: Threshold set to WIFI_AUTH_WPA2_PSK. One line of code, 100+ test attempts to isolate.

---

## Phase 6: Dead State Machine (Bug #67)

After 10 failed boot retries, state machine stayed dead. Manual connect ignored. Fix: Retry counter reset on every new wifi_manager_connect() call, plus esp_wifi_disconnect() before esp_wifi_connect().

---

## Phase 7: SPI DMA OOM (Bug #68)

LVGL flush buffer landed in PSRAM (0x3c1d79a8). ESP32-S3 SPI DMA cannot read from PSRAM. Previously invisible because WiFi Manager never worked. Fix: LVGL draw buffer allocated with MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL.

---

## Phase 8: Boot Sequence Timing (Bug #69)

SMP task detected no WiFi at ~2040ms and opened Settings/WiFi. Splash timer fired at ~3770ms and overwrote everything. Fix: Navigation guard in ui_splash.c.

---

## Phase 9: Dynamic Header (Bug #70)

WiFi SSID only appeared after visiting Settings/WiFi. ui_main_refresh() called only once at screen create. Fix: 3-second timer hdr_refresh_cb for continuous status.

---

## New Features

- First-boot auto-launch: WiFi Manager opens automatically on fresh device
- Context-aware main header: SSID / unread count / "No WiFi" with 3s refresh
- Info tab redesign: 167-line rewrite with row-based design and live stats
- Settings tab text-only styling: active "(WIFI)" vs inactive "BRIGHT"

---

## Bug List (Session 39)

| Bug | Description | Fix |
|-----|-------------|-----|
| #62 | Dual-file WiFi race condition | Merge into unified wifi_manager.c |
| #63 | UI freeze from vTaskDelay | lv_timer_create() one-shot |
| #64 | Scan inconsistent results | Backend cache pattern |
| #65 | First scan no results | Remove stale guard |
| #66 | WPA3 SAE auth failure (0x600) | WIFI_AUTH_WPA2_PSK threshold |
| #67 | Dead state machine after retries | Reset counter + disconnect before connect |
| #68 | SPI DMA OOM on screen switch | MALLOC_CAP_DMA + INTERNAL |
| #69 | Splash timer overwrites WiFi Manager | Navigation guard |
| #70 | SSID not visible on Main Screen | 3-second auto-refresh timer |

---

## Lessons Learned (Session 39)

| # | Lesson |
|---|--------|
| 210 | WPA3 SAE on ESP32-S3 is fragile with transition mode routers. WIFI_AUTH_WPA2_PSK is the safe default. |
| 211 | ESP32-S3 SPI DMA cannot read from PSRAM. LVGL draw buffers must use MALLOC_CAP_DMA + INTERNAL. |
| 212 | Timer-based navigation must check "am I still the active screen?" before acting. |
| 213 | No T-Deck project has an on-device WiFi manager. SimpleGo is the first. |

---

*Part 36 - Session 39 WiFi Manager*
*SimpleGo Protocol Analysis*
*Date: March 3, 2026*
*Bugs: 70 total (68 FIXED, #60 identified for SPI3, #61 temp fix)*
*Lessons: 213 total*
*Milestone 15: On-Device WiFi Manager*
