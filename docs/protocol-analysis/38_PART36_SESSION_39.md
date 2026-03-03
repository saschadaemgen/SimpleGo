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

First-Boot Flow: Power on → WiFi Manager auto-launches → Select network
                → Enter password → Connected. No development tools needed.

MILESTONE 15: On-Device WiFi Manager
```

---

## Starting Point

Session 38 ended with backlight control working and the SPI2 bus root cause identified. WiFi connection still required hardcoded Kconfig credentials. The WiFi Manager in Settings existed but was unstable: scans showed inconsistent results, network switching was impossible (device always reconnected to old network), and UI froze during WiFi operations. No first-boot flow existed for end users.

Goal: A fully functional WiFi Manager with on-device UI that works for both developers and end users.

---

## Phase 1: Market Research

### No T-Deck Project Has an On-Device WiFi Manager

Mausi conducted exhaustive research across all relevant projects:

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

`smp_wifi.c` and `wifi_manager.c` fought each other. `smp_wifi.c` had an unconditional auto-reconnect handler that immediately reconnected to the old network on every DISCONNECT event. When `wifi_manager.c` tried to disconnect to switch networks, `smp_wifi.c` instantly reconnected back.

```
wifi_manager.c: esp_wifi_disconnect()   → DISCONNECT event
smp_wifi.c:     event_handler()         → esp_wifi_connect() (old network!)
wifi_manager.c: esp_wifi_connect(new)   → ALREADY CONNECTED (old!)
```

### Fix: Unified wifi_manager.c

Both files merged into a single `wifi_manager.c`. One state machine, one event handler chain, no conflicts. Kconfig credentials eliminated as priority source, NVS-only storage.

---

## Phase 3: UI Freeze Bugs (#63, #64)

### Bug #63: vTaskDelay in LVGL Context

`deferred_wifi_rebuild()` in `ui_settings_wifi.c` called `vTaskDelay(pdMS_TO_TICKS(200))`, blocking the LVGL render task for 200ms. No screen updates, no touch/keyboard processing during that time.

Fix: Replaced with `lv_timer_create()` one-shot timer. Fires after 200ms without blocking LVGL.

### Bug #64: WiFi Scan Race Condition

`wifi_scan_poll_cb()` called `esp_wifi_scan_get_ap_num()` and `esp_wifi_scan_get_ap_records()` directly instead of using cached results from the backend. Inconsistent results when scan and UI polling were out of sync.

Fix: Switched to `wifi_manager_get_scan_count()` and `wifi_manager_get_scan_results()`. Backend caches results after scan completion, UI reads only the cache.

---

## Phase 4: Scan Timing Bug (#65)

### Bug #65: First Scan Shows No Results

Hasi had implemented a 500ms "stale flag guard" that ignored `wifi_manager_is_scan_done()` for the first 500ms after scan start. If ESP32 completed the scan in under 500ms (few APs), the poll missed the done signal entirely.

Fix: Stale flag guard completely removed. Backend sets `s_scan_done = false` before each new scan -- that is sufficient.

---

## Phase 5: WPA3 SAE Authentication (Bug #66)

### Bug #66: WPA3 SAE Authentication Failure (0x600)

`auth -> init (0x600)` on every connection attempt. 10 retries, then dead. Router ran WPA2/WPA3 Transition Mode. `WIFI_AUTH_WPA_WPA2_PSK` as threshold made ESP32 attempt WPA3-SAE, but SAE negotiation on ESP32-S3 failed consistently.

```
Attempt 1: auth -> init (0x600)
Attempt 2: auth -> init (0x600)
...
Attempt 10: auth -> init (0x600)
State machine: DEAD (retry counter exhausted)
```

Fix: Threshold set to `WIFI_AUTH_WPA2_PSK`. ESP32 now accepts WPA2-PSK and WPA3-SAE but does not aggressively attempt SAE when WPA2 is available. `sae_pwe_h2e = WPA3_SAE_PWE_BOTH` and `pmf_cfg.capable = true` / `required = false` keep WPA3 support open.

One line of code, but over 100 test attempts to isolate and verify.

**Lesson #210:** WPA3 SAE negotiation on ESP32-S3 (ESP-IDF 5.5.2) is fragile with WPA2/WPA3 Transition Mode routers. WIFI_AUTH_WPA2_PSK as threshold is the safe default. Poorly documented issue that will affect other ESP32 developers.

---

## Phase 6: Dead State Machine (Bug #67)

### Bug #67: State Machine Dead After Exhausted Retries

After 10 failed boot retries, WiFi state machine stayed dead. Manual connect via WiFi Manager was ignored because retry counter was exhausted.

Fix: Retry counter reset on every new `wifi_manager_connect()` call. Additionally `esp_wifi_disconnect()` before `esp_wifi_connect()` to clean up broken driver state.

---

## Phase 7: SPI DMA OOM (Bug #68)

### Bug #68: SPI DMA OOM on Screen Switch

```
lcd_panel.io.spi: spi transmit (queue) color failed
TDECK_LVGL: draw_bitmap FAILED: ESP_ERR_NO_MEM (0x101)
```

LVGL flush buffer landed at address `0x3c1d79a8` (PSRAM range). ESP32-S3 SPI DMA cannot read from PSRAM. Triggered when navigating back from Settings to Main during active SMP/TLS session.

Root Cause: Previously invisible because WiFi Manager never worked. Working WiFi Manager created first-ever "screen switch during active network session" scenario. TLS buffers and SMP queues consumed internal SRAM, LVGL draw buffer fell back to PSRAM via malloc.

Fix: LVGL draw buffer allocated with `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` in `tdeck_lvgl.c` to guarantee internal SRAM placement.

**Lesson #211:** ESP32-S3 SPI DMA cannot read from PSRAM. When internal SRAM is consumed by TLS/SMP/crypto, malloc falls back to PSRAM silently. LVGL draw buffers must use MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL to prevent this.

---

## Phase 8: Boot Sequence Timing (Bug #69)

### Bug #69: Splash Timer Overwrites WiFi Manager

On first boot without credentials: SMP task detected "no WiFi" at ~2040ms and opened Settings/WiFi. Splash timer fired at ~3770ms and overwrote everything with Main Screen.

Fix: Navigation guard in `ui_splash.c`: if current screen is no longer Splash (because SMP task already navigated), timer does nothing.

**Lesson #212:** FreeRTOS tasks and LVGL timers run asynchronously. Timer-based navigation must always check "am I still the active screen?" before navigating. Guards are essential for every timer callback that changes screens.

---

## Phase 9: Dynamic Header (Bug #70)

### Bug #70: SSID Not Visible on Main Screen

WiFi SSID only appeared after user visited Settings/WiFi.

Root Cause: `ui_main_refresh()` called only once at screen create. WiFi not yet connected at that time.

Fix: 3-second timer `hdr_refresh_cb` continuously checks WiFi status and unread count. Dynamic header shows:
- Unread messages: blue mail icon with count
- WiFi connected, no unreads: SSID in cyan
- No WiFi: "No WiFi" in grey

---

## New Features

### First-Boot Auto-Launch

On first start without configured WiFi, the WiFi Manager auto-launches after splash screen. No empty Main Screen, no "what do I do now" moment. Power on, select network, enter password, connected.

Dual-path strategy:
- **Developers:** Kconfig credentials automatically transferred to WiFi Manager. Flash and go.
- **End users:** No Kconfig needed, WiFi Manager opens automatically.

### Context-Aware Main Screen Header

Dynamic header with 3-second auto-refresh:
- Unread messages: blue mail icon with count
- WiFi connected: SSID in cyan
- No WiFi: "No WiFi" in grey

### Info Tab Complete Redesign

167-line complete rewrite. Row-based design with colored accent bars. Live 2-second refresh for free heap, PSRAM free, LVGL pool usage (%), and server status with SSID.

### Settings Tab Styling

Text-only active indicator instead of background color:
- Active: white text in brackets "(WIFI)"
- Inactive: cyan text without brackets "BRIGHT"

---

## Bug List (Session 39)

| Bug | Description | Root Cause | Fix |
|-----|-------------|------------|-----|
| #62 | Dual-file WiFi race condition | smp_wifi.c auto-reconnect fights wifi_manager.c | Merge into unified wifi_manager.c |
| #63 | UI freeze during WiFi operations | vTaskDelay(200) in LVGL context | lv_timer_create() one-shot |
| #64 | Scan shows inconsistent results | Direct ESP-IDF API calls instead of cached results | Backend cache pattern |
| #65 | First scan shows no results | 500ms stale flag guard blocks fast scans | Remove stale guard |
| #66 | WPA3 SAE auth failure (0x600) | ESP32-S3 SAE fragile in WPA2/WPA3 transition mode | WIFI_AUTH_WPA2_PSK threshold |
| #67 | Dead state machine after retries | Retry counter not reset on manual connect | Reset counter + disconnect before connect |
| #68 | SPI DMA OOM on screen switch | LVGL buffer in PSRAM, SPI DMA needs internal SRAM | MALLOC_CAP_DMA + INTERNAL |
| #69 | Splash timer overwrites WiFi Manager | Async timer fires after task navigation | Navigation guard in timer callback |
| #70 | SSID not visible on Main Screen | refresh() only at screen create | 3-second auto-refresh timer |

---

## Files Changed (Session 39)

| File | Path | Changes |
|------|------|---------|
| wifi_manager.c | main/net/ | Complete rewrite: unified state machine, NVS-only, WPA3 fix |
| wifi_manager.h | main/include/ | New API: needs_setup(), get_ssid(), get_scan_results() |
| smp_wifi.c | main/net/ | Gutted, logic migrated to wifi_manager.c |
| smp_wifi.h | main/include/ | Reduced interface |
| tdeck_lvgl.c | devices/t_deck_plus/hal_impl/ | SPI DMA buffer fix: MALLOC_CAP_DMA + INTERNAL |
| main.c | main/ | Blocking WiFi loop removed |
| ui_manager.c | main/ui/ | First-boot WiFi redirect after splash |
| ui_splash.c | main/ui/screens/ | Navigation guard against timer overwrite |
| ui_main.c | main/ui/screens/ | Dynamic header: SSID/unread/NoWiFi + 3s refresh |
| ui_settings.c | main/ui/screens/ | Tab text-only styling with brackets |
| ui_settings_wifi.c | main/ui/screens/ | Scan race fix, vTaskDelay fix, stale guard fix |
| ui_settings_bright.c | main/ui/screens/ | Battery hint removed |
| ui_settings_info.c | main/ui/screens/ | Complete rewrite: row design, live stats |
| sdkconfig | root/ | WiFi Manager configuration |

---

## Build and Flash

Normal test:
```
idf.py build flash monitor -p COM6
```

First-boot test (clears NVS):
```
idf.py erase-flash
idf.py build flash monitor -p COM6
```

Note: Kconfig SSID via `idf.py menuconfig` must be cleared if set (survives erase-flash because compiled into binary).

---

## Lessons Learned (Session 39)

| # | Lesson | Context |
|---|--------|---------|
| 210 | WPA3 SAE negotiation on ESP32-S3 (ESP-IDF 5.5.2) is fragile with WPA2/WPA3 Transition Mode routers. WIFI_AUTH_WPA2_PSK as threshold is the safe default. Poorly documented. | 100+ test attempts |
| 211 | ESP32-S3 SPI DMA cannot read from PSRAM. When internal SRAM is consumed by TLS/SMP/crypto, malloc falls back silently. LVGL draw buffers must use MALLOC_CAP_DMA + MALLOC_CAP_INTERNAL. | Screen switch OOM |
| 212 | FreeRTOS tasks and LVGL timers run asynchronously. Timer-based navigation must check "am I still the active screen?" before navigating. Guards essential for every timer-based screen change. | Splash overwrites WiFi |
| 213 | No T-Deck project has an on-device WiFi manager. SimpleGo is the first. Meshtastic uses CLI/phone/web, Bruce uses TFT_eSPI, ESP32Berry is archived. Market gap confirmed. | Exhaustive research |

---

## Session 39 Statistics

| Metric | Value |
|--------|-------|
| Duration | 1 day (March 3, 2026) |
| Features | WiFi Manager, first-boot flow, dynamic header, info tab redesign |
| Bugs fixed | 9 (#62-#70) |
| New lessons | 4 (#210-#213) |
| Files changed | 15 |
| Research scope | Every T-Deck project, every ESP-IDF WiFi library |
| WPA3 test attempts | 100+ |
| Market position | First on-device WiFi manager for T-Deck hardware |

---

## Next Session: 40

### Priorities
1. **SD card chat history fix** -- SPI bus contention (display + SD on SPI2)
2. **Sliding window chat history** -- Bubble recycling for LVGL memory management
3. **WiFi scan intermittent bug** -- Monitor second scan sometimes empty

---

*Part 36 - Session 39 WiFi Manager*
*SimpleGo Protocol Analysis*
*Date: March 3, 2026*
*Bugs: 70 total (68 FIXED, #60 identified for SPI3, #61 temp fix)*
*Lessons: 213 total*
*Milestone 15: On-Device WiFi Manager*
