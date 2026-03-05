---
title: LilyGo T-Deck Plus
sidebar_position: 2
---

# LilyGo T-Deck Plus

The LilyGo T-Deck Plus is the current reference hardware for SimpleGo Model 1 Maker. All firmware development and testing happens on this device.

## Hardware Specifications

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 Dual-Core Xtensa LX7, 240 MHz |
| RAM | 512 KB internal SRAM + 8 MB PSRAM |
| Flash | 16 MB SPI Flash |
| Display | 320x240 IPS LCD |
| Input | Physical QWERTY keyboard (I2C 0x55) |
| Storage | MicroSD card slot (SPI2 bus) |
| Connectivity | WiFi 802.11 b/g/n, Bluetooth 5.0 |
| USB | USB-C (programming + serial monitor) |

## GPIO Assignment

| GPIO | Function | Notes |
|------|----------|-------|
| 42 | Display backlight | 16-level pulse-counting protocol |
| I2C 0x55 | Keyboard backlight | I2C control |

## SPI2 Bus Sharing

The display and SD card share the SPI2 bus. This has critical implications for firmware:

:::warning SPI2 Mutex Required
All SD card operations must be serialized with the LVGL mutex. Never access the SD card outside the mutex. Crypto operations must complete before acquiring the mutex -- never hold the mutex during crypto. Target hold time: under 10ms.
:::

Without this discipline, mutex hold time during combined crypto + SD operations reaches 500ms, causing display stuttering and watchdog resets.

## Backlight Control

Display backlight uses GPIO 42 with a 16-level pulse-counting protocol. Keyboard backlight is controlled via I2C at address 0x55.

## Build Command
```powershell
idf.py build flash monitor -p COM6
```

:::warning Full Erase
Only use `idf.py erase-flash` before a rebuild when NVS keys changed or crypto state changed. For all other changes, normal build is sufficient. Unnecessary erase destroys persisted ratchet state and WiFi credentials stored in NVS.
:::

## Known Hardware Constraints

**Hardware AES disabled.** `CONFIG_MBEDTLS_HARDWARE_AES=n` must be set. The ESP32-S3 hardware AES DMA conflicts with PSRAM at the silicon level. Software AES is used throughout.

**PSRAM task stacks cannot write NVS.** Tasks with PSRAM-allocated stacks cannot write to NVS flash due to SPI Flash cache conflict. Any logic requiring NVS writes must run on the main task with internal SRAM stack.

**WPA3 auth threshold.** Use `WIFI_AUTH_WPA2_PSK` as threshold -- not `WIFI_AUTH_WPA_WPA2_PSK`. The latter causes WPA3 SAE authentication failures.

## PSRAM Layout

| Region | Contents | Size |
|--------|----------|------|
| Ratchet state array | 128 contact Double Ratchet states | ~68 KB |
| Message cache | Sliding window, 30 messages | ~120 KB |
| Task stacks (selected) | FreeRTOS task stacks | variable |

Internal SRAM is reserved for tasks that require NVS write access and for interrupt handlers.

## SD Card

Chat history is stored on the SD card as AES-256-GCM encrypted files. One file per contact. Keys are derived per-contact using HKDF-SHA256 from a master key stored in NVS.

The SD card shares SPI2 with the display. See SPI2 Bus Sharing above.
