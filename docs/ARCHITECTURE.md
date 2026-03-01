# SimpleGo Architecture

This document describes the software architecture of SimpleGo, a multi-platform implementation of the SimpleX Messaging Protocol for dedicated secure communication devices.

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [System Overview](#system-overview)
3. [Layer Architecture](#layer-architecture)
4. [Hardware Abstraction Layer](#hardware-abstraction-layer)
5. [Protocol Implementation](#protocol-implementation)
6. [Cryptographic Architecture](#cryptographic-architecture)
7. [User Interface Layer](#user-interface-layer)
8. [Device Implementations](#device-implementations)
9. [Memory Architecture](#memory-architecture)
10. [Build System](#build-system)
11. [Security Architecture](#security-architecture)

---

## Design Philosophy

SimpleGo is built on three core principles:

### Single Codebase, Multiple Devices

The protocol implementation and user interface are written once and shared across all supported hardware platforms. Only the hardware-specific code differs between devices. This approach ensures:

- Protocol bugs are fixed once for all devices
- Security updates propagate automatically
- New features benefit all platforms simultaneously
- Development effort is maximized

### Minimal Trusted Computing Base

Security increases as code size decreases. SimpleGo targets approximately 50,000 lines of code compared to approximately 50,000,000 in a typical smartphone operating system. Every line of code is a potential vulnerability. By minimizing the codebase, we minimize the attack surface.

### Hardware-Enforced Security

Software security alone is insufficient against physical attacks. SimpleGo leverages hardware security features including:

- Secure Elements for key storage
- Hardware cryptographic acceleration
- Tamper detection circuits
- Secure boot chains
- Flash encryption

---

## System Overview

```
+===========================================================================+
|                           SIMPLEGO SYSTEM                                 |
+===========================================================================+
|                                                                           |
|  +---------------------------------------------------------------------+  |
|  |                      APPLICATION LAYER                              |  |
|  |                                                                     |  |
|  |  +-------------------+  +-------------------+  +-----------------+  |  |
|  |  |   Screen Manager  |  |   Event Handler   |  |  State Manager  |  |  |
|  |  +-------------------+  +-------------------+  +-----------------+  |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                    |                                      |
|  +---------------------------------------------------------------------+  |
|  |                       USER INTERFACE LAYER                          |  |
|  |                                                                     |  |
|  |  +-------------+  +-------------+  +-------------+  +------------+  |  |
|  |  |   Screens   |  |   Widgets   |  |   Themes    |  |   Fonts    |  |  |
|  |  +-------------+  +-------------+  +-------------+  +------------+  |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                    |                                      |
|  +---------------------------------------------------------------------+  |
|  |                        PROTOCOL LAYER                               |  |
|  |                                                                     |  |
|  |  +---------------+  +---------------+  +---------------+            |  |
|  |  |  SMP Client   |  | Agent Protocol|  |    Queues     |            |  |
|  |  +---------------+  +---------------+  +---------------+            |  |
|  |                                                                     |  |
|  |  +---------------+  +---------------+  +---------------+            |  |
|  |  | Double Ratchet|  |     X3DH      |  |   Contacts    |            |  |
|  |  +---------------+  +---------------+  +---------------+            |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                    |                                      |
|  +---------------------------------------------------------------------+  |
|  |                    CRYPTOGRAPHIC LAYER                              |  |
|  |                                                                     |  |
|  |  +----------+  +----------+  +----------+  +----------+  +-------+  |  |
|  |  |   X448   |  |  X25519  |  | Ed25519  |  | AES-GCM  |  | HKDF  |  |  |
|  |  +----------+  +----------+  +----------+  +----------+  +-------+  |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                    |                                      |
|  +---------------------------------------------------------------------+  |
|  |                HARDWARE ABSTRACTION LAYER (HAL)                     |  |
|  |                                                                     |  |
|  |  +---------+ +---------+ +---------+ +---------+ +---------+        |  |
|  |  | Display | |  Input  | | Network | | Storage | |  Audio  |        |  |
|  |  +---------+ +---------+ +---------+ +---------+ +---------+        |  |
|  |                                                                     |  |
|  |  +---------+ +---------+                                            |  |
|  |  |  System | | Secure  |                                            |  |
|  |  |         | | Element |                                            |  |
|  |  +---------+ +---------+                                            |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                    |                                      |
|  +=====================================================================+  |
|  |                    DEVICE IMPLEMENTATION LAYER                      |  |
|  +=====================================================================+  |
|  |              |                |                |                    |  |
|  |  +-----------+--+  +----------+---+  +---------+----+  +----------+ |  |
|  |  | T-Deck Plus  |  | T-Deck Pro   |  | SimpleGo     |  | Desktop  | |  |
|  |  | ESP32-S3     |  | ESP32-S3     |  | Secure       |  | Linux    | |  |
|  |  | 320x240 LCD  |  | 320x240 LCD  |  | STM32 + SE   |  | SDL2     | |  |
|  |  | Keyboard     |  | Keyboard     |  | Dual SE      |  | Testing  | |  |
|  |  +--------------+  +--------------+  +--------------+  +----------+ |  |
|  |                                                                     |  |
|  +---------------------------------------------------------------------+  |
|                                                                           |
+===========================================================================+
```

---

## Layer Architecture

### Layer Independence

Each layer communicates only with adjacent layers through defined interfaces:

| Layer | Depends On | Provides To |
|-------|------------|-------------|
| Application | UI, Protocol | User interaction |
| User Interface | HAL (display, input) | Application |
| Protocol | Crypto, HAL (network, storage) | Application |
| Cryptographic | HAL (secure element) | Protocol |
| HAL | Device Implementation | All upper layers |
| Device Implementation | Hardware | HAL |

### Code Sharing

| Layer | Shared Across Devices | Device-Specific |
|-------|----------------------|-----------------|
| Application | 100% | 0% |
| User Interface | 100% | 0% |
| Protocol | 100% | 0% |
| Cryptographic | 100% | 0% |
| HAL Interfaces | 100% (headers) | 0% |
| HAL Implementation | 0% | 100% |

This separation ensures that adding a new device requires only implementing the HAL layer without modifying any other code.

---

## Hardware Abstraction Layer

The HAL defines seven interfaces that abstract hardware functionality:

### hal_common.h

Common types, error codes, and utilities used across all HAL interfaces.

```c
// Error codes
typedef enum {
    HAL_OK = 0,
    HAL_ERR_INVALID_ARG,
    HAL_ERR_NO_MEM,
    HAL_ERR_NOT_FOUND,
    HAL_ERR_TIMEOUT,
    HAL_ERR_NOT_SUPPORTED,
    HAL_ERR_IO,
    HAL_ERR_BUSY,
    HAL_ERR_INVALID_STATE
} hal_err_t;

// Common geometric types
typedef struct { int16_t x; int16_t y; } hal_point_t;
typedef struct { uint16_t width; uint16_t height; } hal_size_t;
typedef struct { int16_t x; int16_t y; uint16_t w; uint16_t h; } hal_rect_t;
```

### hal_display.h

Display initialization, configuration, and rendering.

| Function | Purpose |
|----------|---------|
| `hal_display_init()` | Initialize display hardware |
| `hal_display_get_info()` | Query display capabilities |
| `hal_display_set_orientation()` | Set rotation (0, 90, 180, 270) |
| `hal_display_set_backlight()` | Adjust brightness (0-100) |
| `hal_display_flush()` | Send pixel data to display |
| `hal_display_flush_wait()` | Wait for DMA transfer completion |

Capability flags indicate available features:

| Flag | Description |
|------|-------------|
| `HAL_DISPLAY_CAP_TOUCH` | Touch overlay available |
| `HAL_DISPLAY_CAP_BACKLIGHT` | Adjustable backlight |
| `HAL_DISPLAY_CAP_ROTATION` | Hardware rotation support |
| `HAL_DISPLAY_CAP_DMA` | DMA transfers supported |
| `HAL_DISPLAY_CAP_DOUBLE_BUF` | Double buffering available |

### hal_input.h

Unified input handling for keyboards, touch, encoders, and buttons.

| Function | Purpose |
|----------|---------|
| `hal_input_init()` | Initialize input subsystem |
| `hal_input_get_capabilities()` | Query available input methods |
| `hal_input_register_key_cb()` | Register keyboard callback |
| `hal_input_register_pointer_cb()` | Register touch/trackball callback |
| `hal_input_register_encoder_cb()` | Register rotary encoder callback |

Input capabilities by device:

| Device | Keyboard | Touch | Trackball | Encoder | Buttons |
|--------|----------|-------|-----------|---------|---------|
| T-Deck Plus | Physical QWERTY | Capacitive | Yes | No | No |
| T-Deck Pro | Physical QWERTY | Capacitive | Yes | No | No |
| T-Lora Pager | None | No | No | Yes | Yes |
| SimpleGo Secure | Physical | Capacitive | Optional | No | No |
| Desktop | USB | No | No | No | No |

### hal_storage.h

Persistent storage for settings, keys, and messages.

| Function | Purpose |
|----------|---------|
| `hal_nvs_open()` | Open NVS namespace |
| `hal_nvs_set_*()` | Store values (int, string, blob) |
| `hal_nvs_get_*()` | Retrieve values |
| `hal_nvs_erase_key()` | Delete single key |
| `hal_nvs_erase_all()` | Factory reset |
| `hal_file_open()` | Open file on filesystem |
| `hal_file_read()` | Read from file |
| `hal_file_write()` | Write to file |
| `hal_sd_mount()` | Mount SD card |

Storage is divided into namespaces:

| Namespace | Purpose | Encryption |
|-----------|---------|------------|
| `settings` | User preferences | Optional |
| `network` | WiFi credentials | Yes |
| `identity` | Device identity keys | Yes |
| `contacts` | Contact list | Yes |
| `messages` | Message history | Yes |

### hal_network.h

Network connectivity including WiFi and Ethernet.

| Function | Purpose |
|----------|---------|
| `hal_net_init()` | Initialize network stack |
| `hal_net_wifi_connect()` | Connect to WiFi network |
| `hal_net_wifi_disconnect()` | Disconnect from WiFi |
| `hal_net_wifi_scan()` | Scan for available networks |
| `hal_net_get_ip()` | Get current IP address |
| `hal_net_is_connected()` | Check connection status |

Network events are delivered through callbacks:

| Event | Description |
|-------|-------------|
| `HAL_NET_EVENT_CONNECTED` | Network connection established |
| `HAL_NET_EVENT_DISCONNECTED` | Network connection lost |
| `HAL_NET_EVENT_GOT_IP` | IP address obtained |
| `HAL_NET_EVENT_LOST_IP` | IP address lost |

### hal_audio.h

Audio output for notifications and feedback.

| Function | Purpose |
|----------|---------|
| `hal_audio_init()` | Initialize audio subsystem |
| `hal_audio_play_tone()` | Play single frequency tone |
| `hal_audio_play_sequence()` | Play tone sequence |
| `hal_audio_play_sound()` | Play predefined sound |
| `hal_audio_set_volume()` | Set output volume |
| `hal_audio_stop()` | Stop current playback |

Predefined sounds:

| Sound | Use Case |
|-------|----------|
| `HAL_SOUND_MESSAGE` | New message received |
| `HAL_SOUND_CONNECT` | Connection established |
| `HAL_SOUND_DISCONNECT` | Connection lost |
| `HAL_SOUND_ERROR` | Error occurred |
| `HAL_SOUND_KEYPRESS` | Key pressed (optional) |

### hal_system.h

System-level functions including power management.

| Function | Purpose |
|----------|---------|
| `hal_system_get_info()` | Get system information |
| `hal_system_restart()` | Reboot device |
| `hal_system_get_reset_reason()` | Query last reset cause |
| `hal_power_get_battery_level()` | Get battery percentage |
| `hal_power_is_charging()` | Check charging status |
| `hal_power_enter_light_sleep()` | Enter low-power mode |
| `hal_power_enter_deep_sleep()` | Enter minimal-power mode |
| `hal_watchdog_init()` | Initialize watchdog timer |
| `hal_watchdog_feed()` | Reset watchdog timer |

---

## Protocol Implementation

### SimpleX Messaging Protocol (SMP)

SimpleGo implements the complete SMP client protocol:

```
+-----------------------------------------------------------------------+
|                        SMP PROTOCOL STACK                             |
+-----------------------------------------------------------------------+
|                                                                       |
|  +---------------------------+    +-----------------------------+     |
|  |      Agent Protocol       |    |      Connection Manager     |     |
|  |                           |    |                             |     |
|  |  - Message routing        |    |  - Server connections       |     |
|  |  - Delivery confirmation  |    |  - Reconnection logic       |     |
|  |  - Error handling         |    |  - Multi-server support     |     |
|  +---------------------------+    +-----------------------------+     |
|                |                              |                       |
|  +---------------------------+    +-----------------------------+     |
|  |      Queue Manager        |    |      Message Encoder        |     |
|  |                           |    |                             |     |
|  |  - Queue creation         |    |  - Wire format encoding     |     |
|  |  - Subscription           |    |  - Length prefixes          |     |
|  |  - Message retrieval      |    |  - Padding                  |     |
|  +---------------------------+    +-----------------------------+     |
|                |                              |                       |
|  +---------------------------------------------------------------+   |
|  |                     SMP Commands                               |   |
|  |                                                                 |   |
|  |  NEW    - Create new queue                                     |   |
|  |  KEY    - Provide sender key                                   |   |
|  |  SUB    - Subscribe to queue                                   |   |
|  |  SEND   - Send message to queue                                |   |
|  |  ACK    - Acknowledge message receipt                          |   |
|  |  OFF    - Suspend queue                                        |   |
|  |  DEL    - Delete queue                                         |   |
|  +---------------------------------------------------------------+   |
|                |                              |                       |
|  +---------------------------------------------------------------+   |
|  |                     TLS 1.3 Transport                          |   |
|  |                                                                 |   |
|  |  - ALPN: "smp/1"                                               |   |
|  |  - Server certificate verification                             |   |
|  |  - Session resumption                                          |   |
|  +---------------------------------------------------------------+   |
|                                                                       |
+-----------------------------------------------------------------------+
```

### Message Flow

Sending a message:

```
1. Application creates message
           |
           v
2. Double Ratchet encrypts message body
           |
           v
3. Message header encrypted with header key
           |
           v
4. Agent protocol wraps encrypted message
           |
           v
5. SMP SEND command with encrypted payload
           |
           v
6. TLS 1.3 encrypts entire transmission
           |
           v
7. TCP to SMP server
```

Receiving a message:

```
1. TCP from SMP server
           |
           v
2. TLS 1.3 decrypts transmission
           |
           v
3. SMP response parsed
           |
           v
4. Agent protocol unwraps message
           |
           v
5. Message header decrypted
           |
           v
6. Double Ratchet decrypts message body
           |
           v
7. Application receives plaintext
```

---

## Cryptographic Architecture

### Algorithm Selection

| Purpose | Algorithm | Key Size | Library |
|---------|-----------|----------|---------|
| Ratchet Key Agreement | X448 | 448 bits | wolfSSL |
| Initial Key Agreement | X25519 | 256 bits | libsodium |
| Signing | Ed25519 | 256 bits | libsodium |
| Symmetric Encryption | AES-256-GCM | 256 bits | mbedTLS |
| Key Derivation | HKDF-SHA512 | 512 bits | mbedTLS |
| Hashing | SHA-512 | 512 bits | mbedTLS |

### Key Hierarchy

```
+-----------------------------------------------------------------------+
|                         KEY HIERARCHY                                 |
+-----------------------------------------------------------------------+
|                                                                       |
|  IDENTITY LAYER (Long-term, stored in Secure Element)                 |
|  +-------------------------------------------------------------------+|
|  |  +-------------------+    +-------------------+                   ||
|  |  | Identity Key      |    | Identity Key      |                   ||
|  |  | (Ed25519)         |    | (Ed25519)         |                   ||
|  |  | Signing           |    | Verification      |                   ||
|  |  +-------------------+    +-------------------+                   ||
|  +-------------------------------------------------------------------+|
|                              |                                        |
|  X3DH KEY AGREEMENT                                                   |
|  +-------------------------------------------------------------------+|
|  |  +-------------------+    +-------------------+                   ||
|  |  | Signed Pre-Key    |    | One-Time Pre-Key  |                   ||
|  |  | (X25519)          |    | (X25519)          |                   ||
|  |  +-------------------+    +-------------------+                   ||
|  |                              |                                    ||
|  |                              v                                    ||
|  |  +-----------------------------------------------------------+   ||
|  |  |              Shared Secret (from X3DH)                     |   ||
|  |  +-----------------------------------------------------------+   ||
|  +-------------------------------------------------------------------+|
|                              |                                        |
|  DOUBLE RATCHET (Per-message keys, ephemeral)                         |
|  +-------------------------------------------------------------------+|
|  |                              v                                    ||
|  |  +-------------------+    +-------------------+                   ||
|  |  | Root Key          |--->| Chain Key         |                   ||
|  |  | (256 bits)        |    | (256 bits)        |                   ||
|  |  +-------------------+    +-------------------+                   ||
|  |                              |                                    ||
|  |         +--------------------+--------------------+               ||
|  |         |                    |                    |               ||
|  |         v                    v                    v               ||
|  |  +-------------+      +-------------+      +-------------+        ||
|  |  | Message Key |      | Message Key |      | Message Key |        ||
|  |  | (Msg 1)     |      | (Msg 2)     |      | (Msg N)     |        ||
|  |  +-------------+      +-------------+      +-------------+        ||
|  +-------------------------------------------------------------------+|
|                                                                       |
+-----------------------------------------------------------------------+
```

### Forward Secrecy

The Double Ratchet protocol provides forward secrecy through two mechanisms:

1. **Diffie-Hellman Ratchet**: New X448 key pair generated periodically. Compromise of current keys does not reveal past session keys.

2. **Symmetric Ratchet**: Chain key advanced with each message using HKDF. Previous message keys cannot be derived from current state.

### Post-Compromise Security

If an attacker compromises the current session state, security is restored after the next Diffie-Hellman ratchet step. The attacker cannot decrypt future messages once new key material is exchanged.

---

## User Interface Layer

### LVGL Integration

SimpleGo uses LVGL (Light and Versatile Graphics Library) for the user interface:

| Component | Purpose |
|-----------|---------|
| Screens | Full-screen views (chat, contacts, settings) |
| Widgets | Reusable UI components (message bubbles, status bar) |
| Themes | Visual styling (colors, fonts, spacing) |
| Input Drivers | HAL integration for touch, keyboard, encoder |

### Screen Architecture

```
+-----------------------------------------------------------------------+
|                         SCREEN MANAGER                                |
+-----------------------------------------------------------------------+
|                                                                       |
|  +-------------+  +-------------+  +-------------+  +-------------+   |
|  |   Splash    |  |    Main     |  |    Chat     |  |  Contacts   |   |
|  |   Screen    |  |   Screen    |  |   Screen    |  |   Screen    |   |
|  +-------------+  +-------------+  +-------------+  +-------------+   |
|                                                                       |
|  +-------------+  +-------------+  +-------------+  +-------------+   |
|  |  Settings   |  |   Connect   |  |   QR Code   |  |   About     |   |
|  |   Screen    |  |   Screen    |  |   Screen    |  |   Screen    |   |
|  +-------------+  +-------------+  +-------------+  +-------------+   |
|                                                                       |
+-----------------------------------------------------------------------+
```

### Adaptive Layout

The UI adapts to device capabilities:

| Capability | Adaptation |
|------------|------------|
| Small display (< 200px width) | Compact layout, smaller fonts |
| No touch | Keyboard/encoder navigation highlighted |
| No keyboard | On-screen keyboard activated |
| Low memory | Reduced animation, simpler themes |

---

## Device Implementations

### Directory Structure

```
devices/
|
+-- t_deck_plus/
|   +-- config/
|   |   +-- device_config.h       # Hardware constants and pin mappings
|   +-- hal_impl/
|       +-- hal_display.c         # ST7789V display driver
|       +-- hal_input.c           # Keyboard, trackball, touch
|       +-- hal_audio.c           # MAX98357A I2S audio
|       +-- hal_storage.c         # NVS and SD card
|       +-- hal_network.c         # ESP32 WiFi
|       +-- hal_system.c          # AXP2101 power management
|
+-- t_deck_pro/
|   +-- config/
|   |   +-- device_config.h
|   +-- hal_impl/
|       +-- hal_display.c         # ST7789V display driver
|       +-- hal_input.c           # Keyboard, trackball, touch
|       +-- hal_audio.c           # I2S audio
|       +-- hal_storage.c         # NVS and SD card
|       +-- hal_network.c         # ESP32 WiFi
|       +-- hal_system.c          # Power management
|
+-- t_lora_pager/
|   +-- config/
|   |   +-- device_config.h
|   +-- hal_impl/
|       +-- hal_display.c         # OLED display driver
|       +-- hal_input.c           # Encoder and buttons
|       +-- hal_audio.c           # Buzzer
|       +-- hal_storage.c         # NVS
|       +-- hal_network.c         # ESP32 WiFi + LoRa
|       +-- hal_system.c          # Battery management
|
+-- simplego_secure/
|   +-- config/
|   |   +-- device_config.h
|   +-- hal_impl/
|       +-- hal_secure_element.c  # ATECC608B + OPTIGA integration
|       +-- hal_display.c         # Display driver
|       +-- hal_input.c           # Input handling
|       +-- hal_network.c         # WiFi + LTE
|       +-- ...
|
+-- template/
    +-- config/
    |   +-- device_config.h.template
    +-- hal_impl/
        +-- hal_template.c        # Implementation template
```

### Device Comparison

| Feature | T-Deck Plus | T-Deck Pro | T-Lora Pager | SimpleGo Secure |
|---------|-------------|------------|--------------|-----------------|
| MCU | ESP32-S3 | ESP32-S3 | ESP32-S3 | STM32U585 |
| Display | 320x240 IPS | 320x240 IPS | 128x64 OLED | 320x240 IPS |
| Input | KB + Trackball + Touch | KB + Trackball + Touch | Encoder + Buttons | KB + Touch |
| Audio | I2S Speaker | I2S Speaker | Buzzer | I2S Speaker |
| Secure Element | None | None | None | Dual (ATECC + OPTIGA) |
| Radio | WiFi + LoRa (opt) | WiFi + LoRa (opt) | WiFi + LoRa | WiFi + LTE + LoRa |
| Power | AXP2101 PMU | AXP2101 PMU | Battery ADC | Full PMU |
| Target | Tier 1 Dev | Tier 1 Dev | Tier 1 Compact | Tier 2 Production |

---

## Memory Architecture

### ESP32-S3 Memory Map

```
+-----------------------------------------------------------------------+
|                      FLASH MEMORY (16 MB)                             |
+-----------------------------------------------------------------------+
| Offset      | Size    | Partition      | Contents                     |
|-------------|---------|----------------|------------------------------|
| 0x000000    | 64 KB   | Bootloader     | Second-stage bootloader      |
| 0x010000    | 8 KB    | Partition Table| Partition definitions        |
| 0x012000    | 24 KB   | NVS            | Settings, credentials        |
| 0x018000    | 8 KB    | PHY Init       | WiFi calibration data        |
| 0x01A000    | 4 MB    | Application    | Firmware                     |
| 0x41A000    | 4 MB    | OTA            | Update partition             |
| 0x81A000    | ~7.5 MB | Storage        | Messages, contacts           |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
|                      INTERNAL SRAM (512 KB)                           |
+-----------------------------------------------------------------------+
| Region      | Size    | Usage                                         |
|-------------|---------|-----------------------------------------------|
| IRAM        | 128 KB  | Instruction cache, interrupt handlers         |
| DRAM        | 320 KB  | Heap, stack, static data                      |
| RTC SRAM    | 16 KB   | Deep sleep persistence                        |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
|                      PSRAM (8 MB)                                     |
+-----------------------------------------------------------------------+
| Usage                           | Typical Allocation                  |
|---------------------------------|-------------------------------------|
| LVGL Display Buffers            | 150 KB (320x240x2 double buffer)    |
| LVGL Object Pool                | 256 KB                              |
| Message Cache                   | 512 KB                              |
| Cryptographic Buffers           | 64 KB                               |
| Network Buffers                 | 128 KB                              |
| Application Heap                | Remaining                           |
+-----------------------------------------------------------------------+
```

### Memory Allocation Strategy

| Type | Allocation | Reason |
|------|------------|--------|
| Display buffers | PSRAM | Large, performance not critical |
| Crypto operations | Internal SRAM | Security, speed |
| Network buffers | PSRAM | Large, DMA capable |
| UI objects | PSRAM | Numerous small allocations |
| Stack | Internal SRAM | Speed, reliability |

### Memory Safety

```c
// All sensitive memory is cleared after use
void secure_clear(void *ptr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

// Buffer operations always bounds-checked
hal_err_t safe_copy(void *dst, size_t dst_size, 
                    const void *src, size_t src_size) {
    if (src_size > dst_size) {
        return HAL_ERR_INVALID_ARG;
    }
    memcpy(dst, src, src_size);
    return HAL_OK;
}
```

---

## Build System

### ESP-IDF Integration

SimpleGo uses the standard ESP-IDF build system:

```
simplex_client/
|
+-- CMakeLists.txt              # Project definition (minimal)
+-- Kconfig                     # Configuration options
+-- sdkconfig.defaults          # Default configuration
|
+-- main/
|   +-- CMakeLists.txt          # Component registration
|   +-- Kconfig                 # Component-specific options
|   +-- core/                   # Protocol implementation
|   +-- hal/                    # HAL interface headers
|   +-- ui/                     # User interface
|   +-- include/                # Public headers
|
+-- devices/
|   +-- t_deck_plus/
|   +-- t_embed_cc1101/
|
+-- components/                 # External libraries
    +-- wolfssl/
    +-- lvgl/
```

### Device Selection

Device selection is managed through Kconfig:

```bash
# Interactive configuration
idf.py menuconfig
# Navigate to: SimpleGo Configuration -> Target Device

# Or via sdkconfig
echo "CONFIG_SIMPLEGO_DEVICE_T_DECK_PLUS=y" >> sdkconfig
```

### Build Process

```bash
# Full build
idf.py build

# Build and flash
idf.py flash -p /dev/ttyUSB0

# Build, flash, and monitor
idf.py flash monitor -p /dev/ttyUSB0

# Clean build
idf.py fullclean
idf.py build
```

For detailed build system documentation, see [BUILD_SYSTEM.md](BUILD_SYSTEM.md).

---

## Security Architecture

### Secure Boot

```
+-----------------------------------------------------------------------+
|                       SECURE BOOT CHAIN                               |
+-----------------------------------------------------------------------+
|                                                                       |
|  +------------------+                                                 |
|  | ROM Bootloader   |  Immutable, in silicon                         |
|  | (First Stage)    |                                                 |
|  +--------+---------+                                                 |
|           |                                                           |
|           | Verify signature (RSA-3072 / ECDSA)                       |
|           v                                                           |
|  +------------------+                                                 |
|  | Second Stage     |  Public key hash in eFuse                       |
|  | Bootloader       |                                                 |
|  +--------+---------+                                                 |
|           |                                                           |
|           | Verify signature                                          |
|           v                                                           |
|  +------------------+                                                 |
|  | Application      |  Signed firmware image                          |
|  | Firmware         |                                                 |
|  +--------+---------+                                                 |
|           |                                                           |
|           | Runtime integrity checks                                  |
|           v                                                           |
|  +------------------+                                                 |
|  | Secure Element   |  Verify device identity (Tier 2+)               |
|  | Verification     |                                                 |
|  +------------------+                                                 |
|                                                                       |
+-----------------------------------------------------------------------+
```

### Flash Encryption

All flash contents are encrypted with AES-256-XTS:

| Region | Encryption | Key Storage |
|--------|------------|-------------|
| Bootloader | Yes | eFuse (read-protected) |
| Application | Yes | eFuse (read-protected) |
| NVS | Yes | eFuse (read-protected) |
| OTA Partition | Yes | eFuse (read-protected) |

### Key Storage

| Key Type | Storage Location | Protection |
|----------|------------------|------------|
| Device Identity | Secure Element | Hardware isolation |
| TLS Client Key | Secure Element | Hardware isolation |
| Ratchet Keys | Encrypted NVS | Flash encryption |
| Session Keys | RAM only | Cleared after use |

### Tamper Response (Tier 2+)

```
Tamper Event Detected
         |
         v
+------------------+
| Zeroization      |  < 1 microsecond
| - Clear SRAM     |
| - Clear SE keys  |
| - Clear NVS      |
+------------------+
         |
         v
+------------------+
| Device Brick     |  Optional, configurable
+------------------+
```

---

## References

- [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq)
- [Double Ratchet Algorithm](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH Key Agreement](https://signal.org/docs/specifications/x3dh/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [wolfSSL Manual](https://www.wolfssl.com/documentation/manuals/wolfssl/)

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.16 | January 2026 | Complete rewrite for HAL architecture |
| 0.1.0 | December 2025 | Initial protocol-focused documentation |
