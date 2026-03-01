# Technical Documentation

Low-level technical details for SimpleGo development, debugging, and porting.

---

## Table of Contents

1. [Build Environment](#build-environment)
2. [Project Structure](#project-structure)
3. [Hardware Abstraction Layer](#hardware-abstraction-layer)
4. [Module Architecture](#module-architecture)
5. [Configuration System](#configuration-system)
6. [Memory Architecture](#memory-architecture)
7. [Cryptographic Implementation](#cryptographic-implementation)
8. [Network Stack](#network-stack)
9. [Performance Characteristics](#performance-characteristics)
10. [Debugging](#debugging)
11. [Error Handling](#error-handling)
12. [Porting Guide](#porting-guide)
13. [References](#references)

---

## Build Environment

### Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| ESP-IDF | 5.5.2+ | Development framework for ESP32 targets |
| Python | 3.8+ | Build scripts, tools |
| CMake | 3.16+ | Build system |
| Git | 2.x | Version control |
| Ninja | 1.10+ | Build backend (optional, faster) |

### Installation (Windows)

```powershell
# Install ESP-IDF using Espressif installer
# Download from: https://dl.espressif.com/dl/esp-idf/

# After installation, use ESP-IDF PowerShell or CMD
# Default path: C:\Espressif\
```

### Installation (Linux/macOS)

```bash
# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git

# Install tools
cd esp-idf
./install.sh esp32s3

# Set up environment (add to .bashrc/.zshrc)
. ~/esp/esp-idf/export.sh
```

### Build Commands

| Command | Description |
|---------|-------------|
| `idf.py build` | Compile project |
| `idf.py flash -p COM6` | Flash to device |
| `idf.py monitor -p COM6` | Serial monitor (115200 baud) |
| `idf.py flash monitor -p COM6` | Flash and monitor |
| `idf.py menuconfig` | Configuration menu |
| `idf.py clean` | Clean build artifacts |
| `idf.py fullclean` | Full clean (remove build dir) |
| `idf.py size` | Binary size analysis |
| `idf.py size-components` | Size per component |
| `idf.py reconfigure` | Regenerate build files |

### Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| ESP32-S3 | `idf.py set-target esp32s3` | LilyGo T-Deck Plus/Pro, T-Lora Pager |
| ESP32-S2 | `idf.py set-target esp32s2` | Future support |
| Native | See SDL2 HAL | Desktop testing |

---

## Project Structure

```
simplex_client/
├── main/                           # Main application component
│   ├── CMakeLists.txt              # Component build configuration
│   ├── Kconfig.projbuild           # Project-specific Kconfig
│   ├── main.c                      # Entry point
│   │
│   ├── hal/                        # HAL interface headers
│   │   ├── hal_common.h            # Common types, error codes
│   │   ├── hal_display.h           # Display abstraction
│   │   ├── hal_input.h             # Input abstraction
│   │   ├── hal_storage.h           # Storage abstraction
│   │   ├── hal_network.h           # Network abstraction
│   │   ├── hal_audio.h             # Audio abstraction
│   │   └── hal_system.h            # System abstraction
│   │
│   ├── include/                    # Protocol headers
│   │   ├── smp_types.h             # Core type definitions
│   │   ├── smp_crypto.h            # Cryptographic functions
│   │   ├── smp_network.h           # Network functions
│   │   ├── smp_parser.h            # Protocol parser
│   │   ├── smp_peer.h              # Peer connection
│   │   ├── smp_contacts.h          # Contact management
│   │   ├── smp_ratchet.h           # Double Ratchet
│   │   ├── smp_handshake.h         # X3DH handshake
│   │   ├── smp_queue.h             # Queue management
│   │   ├── smp_x448.h              # X448 operations
│   │   └── smp_utils.h             # Utility functions
│   │
│   └── *.c                         # Implementation files
│
├── devices/                        # Device-specific implementations
│   ├── t_deck_plus/
│   │   ├── config/
│   │   │   └── device_config.h     # Pin mappings, hardware config
│   │   └── hal_impl/
│   │       ├── hal_display.c       # ST7789V driver
│   │       ├── hal_input.c         # Keyboard, trackball, touch
│   │       ├── hal_audio.c         # I2S audio
│   │       ├── hal_storage.c       # NVS, SD card
│   │       ├── hal_network.c       # WiFi
│   │       └── hal_system.c        # AXP2101 power management
│   │
│   ├── t_deck_pro/
│   │   └── ...                     # Similar structure
│   │
│   ├── t_lora_pager/
│   │   └── ...                     # Similar structure
│   │
│   └── template/                   # Template for new devices
│       └── ...
│
├── components/                     # External components
│   ├── wolfssl/                    # X448 cryptography
│   └── lvgl/                       # Graphics library (planned)
│
├── docs/                           # Documentation
│   ├── ARCHITECTURE.md
│   ├── SECURITY_MODEL.md
│   ├── TECHNICAL.md
│   ├── BUILD_SYSTEM.md
│   ├── ADDING_NEW_DEVICE.md
│   ├── hardware/                   # Hardware documentation
│   └── release-info/               # Release notes
│
├── CMakeLists.txt                  # Root CMake configuration
├── Kconfig                         # Root Kconfig (optional)
├── sdkconfig.defaults              # Default configuration values
├── partitions.csv                  # Partition table
├── README.md
├── CHANGELOG.md
├── ROADMAP.md
└── LICENSE
```

---

## Hardware Abstraction Layer

The HAL provides a unified interface for device-specific hardware, enabling single codebase for multiple platforms.

### HAL Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                            │
│              (Protocol, UI - device independent)                │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────────┐
│                   HAL Interface Layer                           │
│                                                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐          │
│  │ Display  │ │  Input   │ │ Storage  │ │ Network  │  ...     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────────┐
│                Device Implementation Layer                      │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ T-Deck Plus │  │ T-Deck Pro  │  │ T-Lora Pager│  ...       │
│  │   ESP32-S3  │  │   ESP32-S3  │  │   ESP32-S3  │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

### HAL Interfaces

#### hal_common.h

```c
// Error codes
typedef enum {
    HAL_OK = 0,
    HAL_ERROR,
    HAL_TIMEOUT,
    HAL_BUSY,
    HAL_NOT_SUPPORTED,
    HAL_INVALID_PARAM,
    HAL_NO_MEMORY
} hal_err_t;

// Logging macros
#define HAL_LOGE(tag, fmt, ...)  ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define HAL_LOGW(tag, fmt, ...)  ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define HAL_LOGI(tag, fmt, ...)  ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define HAL_LOGD(tag, fmt, ...)  ESP_LOGD(tag, fmt, ##__VA_ARGS__)
```

#### hal_display.h

```c
// Initialize display hardware
hal_err_t hal_display_init(void);

// Flush buffer to display (LVGL callback compatible)
hal_err_t hal_display_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, 
                            const void *color_data);

// Backlight control (0-100%)
hal_err_t hal_display_set_backlight(uint8_t brightness);

// Display capabilities
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t color_depth;        // Bits per pixel
    bool has_backlight;
    bool has_touch;             // Integrated touch
} hal_display_info_t;

hal_err_t hal_display_get_info(hal_display_info_t *info);
```

#### hal_input.h

```c
// Input event types
typedef enum {
    HAL_INPUT_KEY_PRESS,
    HAL_INPUT_KEY_RELEASE,
    HAL_INPUT_TOUCH_DOWN,
    HAL_INPUT_TOUCH_UP,
    HAL_INPUT_TOUCH_MOVE,
    HAL_INPUT_ENCODER_CW,
    HAL_INPUT_ENCODER_CCW,
    HAL_INPUT_ENCODER_CLICK,
    HAL_INPUT_TRACKBALL
} hal_input_type_t;

typedef struct {
    hal_input_type_t type;
    union {
        char key;                   // For keyboard
        struct { int16_t x, y; } touch;  // For touch/trackball
        int8_t delta;               // For encoder
    };
    uint32_t timestamp;
} hal_input_event_t;

// Initialize input hardware
hal_err_t hal_input_init(void);

// Register callback for input events
typedef void (*hal_input_callback_t)(hal_input_event_t *event);
hal_err_t hal_input_register_callback(hal_input_callback_t callback);

// Poll for input (alternative to callback)
hal_err_t hal_input_poll(hal_input_event_t *event, uint32_t timeout_ms);

// Input capabilities
typedef struct {
    bool has_keyboard;
    bool has_touch;
    bool has_trackball;
    bool has_encoder;
    uint8_t num_buttons;
} hal_input_info_t;

hal_err_t hal_input_get_info(hal_input_info_t *info);
```

#### hal_storage.h

```c
// NVS operations
hal_err_t hal_nvs_init(void);
hal_err_t hal_nvs_open(const char *namespace, hal_nvs_handle_t *handle);
hal_err_t hal_nvs_set_blob(hal_nvs_handle_t handle, const char *key, 
                           const void *data, size_t len);
hal_err_t hal_nvs_get_blob(hal_nvs_handle_t handle, const char *key,
                           void *data, size_t *len);
hal_err_t hal_nvs_erase_key(hal_nvs_handle_t handle, const char *key);
hal_err_t hal_nvs_commit(hal_nvs_handle_t handle);
hal_err_t hal_nvs_close(hal_nvs_handle_t handle);

// Filesystem operations (for devices with SD card)
hal_err_t hal_fs_init(void);
hal_err_t hal_fs_open(const char *path, const char *mode, hal_file_t *file);
hal_err_t hal_fs_read(hal_file_t file, void *buf, size_t len, size_t *read);
hal_err_t hal_fs_write(hal_file_t file, const void *buf, size_t len);
hal_err_t hal_fs_close(hal_file_t file);

// Storage info
typedef struct {
    bool has_nvs;
    bool has_sd_card;
    bool has_internal_fs;
    uint32_t nvs_size;
    uint32_t sd_capacity;       // 0 if no SD
} hal_storage_info_t;

hal_err_t hal_storage_get_info(hal_storage_info_t *info);
```

#### hal_network.h

```c
// WiFi operations
hal_err_t hal_wifi_init(void);
hal_err_t hal_wifi_connect(const char *ssid, const char *password);
hal_err_t hal_wifi_disconnect(void);
hal_err_t hal_wifi_get_status(hal_wifi_status_t *status);

// Network events
typedef enum {
    HAL_NET_EVENT_CONNECTED,
    HAL_NET_EVENT_DISCONNECTED,
    HAL_NET_EVENT_GOT_IP,
    HAL_NET_EVENT_LOST_IP
} hal_net_event_t;

typedef void (*hal_net_callback_t)(hal_net_event_t event);
hal_err_t hal_net_register_callback(hal_net_callback_t callback);

// Network capabilities
typedef struct {
    bool has_wifi;
    bool has_ethernet;
    bool has_cellular;
    bool has_lora;
} hal_network_info_t;

hal_err_t hal_network_get_info(hal_network_info_t *info);
```

#### hal_audio.h

```c
// Audio output
hal_err_t hal_audio_init(void);
hal_err_t hal_audio_play_tone(uint32_t frequency, uint32_t duration_ms);
hal_err_t hal_audio_play_buffer(const int16_t *samples, size_t count);
hal_err_t hal_audio_set_volume(uint8_t volume);  // 0-100%

// Audio capabilities
typedef struct {
    bool has_speaker;
    bool has_buzzer;
    bool has_microphone;
    uint32_t sample_rate;
} hal_audio_info_t;

hal_err_t hal_audio_get_info(hal_audio_info_t *info);
```

#### hal_system.h

```c
// Power management
hal_err_t hal_system_init(void);
hal_err_t hal_system_sleep(hal_sleep_mode_t mode);
hal_err_t hal_system_get_battery(uint8_t *percent, bool *charging);
hal_err_t hal_system_power_off(void);

// System information
typedef struct {
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t uptime_ms;
    int8_t temperature;         // Celsius, if available
    uint16_t vbat_mv;           // Battery voltage in mV
} hal_system_status_t;

hal_err_t hal_system_get_status(hal_system_status_t *status);

// Watchdog
hal_err_t hal_watchdog_init(uint32_t timeout_ms);
hal_err_t hal_watchdog_feed(void);
```

### Device Configuration

Each device has a `device_config.h` defining hardware specifics:

```c
// Example: T-Deck Plus device_config.h

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

// Device identification
#define DEVICE_NAME             "T-Deck Plus"
#define DEVICE_VENDOR           "LilyGo"
#define DEVICE_MCU              "ESP32-S3"

// Display configuration
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define DISPLAY_DRIVER          ST7789V
#define DISPLAY_SPI_HOST        SPI2_HOST
#define DISPLAY_PIN_MOSI        GPIO_NUM_41
#define DISPLAY_PIN_SCLK        GPIO_NUM_40
#define DISPLAY_PIN_CS          GPIO_NUM_12
#define DISPLAY_PIN_DC          GPIO_NUM_11
#define DISPLAY_PIN_RST         GPIO_NUM_42
#define DISPLAY_PIN_BL          GPIO_NUM_46

// Keyboard configuration
#define KEYBOARD_I2C_ADDR       0x55
#define KEYBOARD_I2C_PORT       I2C_NUM_0
#define KEYBOARD_PIN_SDA        GPIO_NUM_18
#define KEYBOARD_PIN_SCL        GPIO_NUM_8
#define KEYBOARD_PIN_INT        GPIO_NUM_47

// Trackball configuration
#define TRACKBALL_I2C_ADDR      0x0A
#define TRACKBALL_PIN_INT       GPIO_NUM_3

// Touch configuration
#define TOUCH_I2C_ADDR          0x14
#define TOUCH_PIN_INT           GPIO_NUM_16
#define TOUCH_PIN_RST           GPIO_NUM_21

// Audio configuration
#define AUDIO_I2S_PORT          I2S_NUM_0
#define AUDIO_PIN_BCLK          GPIO_NUM_7
#define AUDIO_PIN_LRCK          GPIO_NUM_5
#define AUDIO_PIN_DOUT          GPIO_NUM_6

// Power management (AXP2101)
#define PMU_I2C_ADDR            0x34
#define PMU_PIN_INT             GPIO_NUM_4

// SD Card
#define SD_SPI_HOST             SPI3_HOST
#define SD_PIN_MOSI             GPIO_NUM_41
#define SD_PIN_MISO             GPIO_NUM_38
#define SD_PIN_SCLK             GPIO_NUM_40
#define SD_PIN_CS               GPIO_NUM_39

// Capability flags
#define HAS_KEYBOARD            1
#define HAS_TRACKBALL           1
#define HAS_TOUCH               1
#define HAS_SPEAKER             1
#define HAS_SD_CARD             1
#define HAS_BATTERY             1
#define HAS_LORA                0  // Optional module

#endif // DEVICE_CONFIG_H
```

---

## Module Architecture

### Protocol Modules

| Module | File | Purpose | Lines |
|--------|------|---------|-------|
| Types | smp_types.h | Structures, constants | ~80 |
| Globals | smp_globals.c | Global state | ~25 |
| Utils | smp_utils.c | Base64, encoding | ~100 |
| Crypto | smp_crypto.c | Ed25519, X25519 | ~80 |
| X448 | smp_x448.c | X448 with wolfSSL | ~150 |
| Network | smp_network.c | TLS/TCP I/O | ~160 |
| Contacts | smp_contacts.c | Contact + NVS | ~380 |
| Parser | smp_parser.c | Agent Protocol | ~260 |
| Peer | smp_peer.c | Peer connection | ~220 |
| Ratchet | smp_ratchet.c | Double Ratchet | ~400 |
| Handshake | smp_handshake.c | X3DH, HELLO | ~350 |
| Queue | smp_queue.c | SMPQueueInfo | ~150 |
| Main | main.c | Entry point | ~350 |
| **Total** | | | **~2700** |

### Module Dependencies

```
main.c
├── smp_network.c
│   ├── smp_crypto.c
│   └── smp_utils.c
├── smp_parser.c
│   ├── smp_contacts.c
│   │   └── smp_utils.c
│   └── smp_types.h
├── smp_peer.c
│   ├── smp_network.c
│   └── smp_handshake.c
│       ├── smp_ratchet.c
│       │   ├── smp_x448.c
│       │   └── smp_crypto.c
│       └── smp_queue.c
└── smp_globals.c
```

---

## Configuration System

### Kconfig Structure

SimpleGo uses ESP-IDF's Kconfig system for compile-time configuration.

```
main/Kconfig.projbuild
├── Device Selection
│   ├── T-Deck Plus
│   ├── T-Deck Pro
│   ├── T-Lora Pager
│   ├── Raspberry Pi
│   └── Custom
├── SimpleGo Core
│   ├── Max contacts
│   ├── Max messages
│   ├── Message length
│   └── Auto-reconnect
├── Network Configuration
│   ├── WiFi SSID
│   ├── WiFi Password
│   ├── SMP Server
│   └── SMP Port
├── UI Configuration
│   ├── Theme
│   ├── Animations
│   └── Indicators
├── Security Configuration
│   ├── PIN required
│   ├── PIN length
│   ├── Auto-lock
│   └── Wipe on failed
├── Power Management
│   ├── Sleep enable
│   ├── Sleep timeouts
│   └── Battery saver
└── Debug Options
    ├── Debug logging
    ├── Log crypto
    └── Log network
```

### Accessing Configuration

All Kconfig values are available as `CONFIG_*` macros:

```c
#include "sdkconfig.h"

// WiFi credentials
const char *ssid = CONFIG_SIMPLEGO_WIFI_SSID;
const char *pass = CONFIG_SIMPLEGO_WIFI_PASSWORD;

// Device selection
#if defined(CONFIG_SIMPLEGO_DEVICE_T_DECK_PLUS)
    #include "devices/t_deck_plus/config/device_config.h"
#elif defined(CONFIG_SIMPLEGO_DEVICE_T_DECK_PRO)
    #include "devices/t_deck_pro/config/device_config.h"
#endif

// Feature checks
#if CONFIG_SIMPLEGO_DEBUG_LOG
    ESP_LOGD(TAG, "Debug: %s", message);
#endif
```

### sdkconfig.defaults

Default values applied on first build:

```ini
# Target
CONFIG_IDF_TARGET="esp32s3"

# Flash
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y

# PSRAM
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# TLS 1.3
CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=y
CONFIG_MBEDTLS_TLS_CLIENT_ONLY=y

# Stack size
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

---

## Memory Architecture

### ESP32-S3 Memory Map

```
┌─────────────────────────────────────────────────────────────┐
│                    Flash (16 MB)                            │
├─────────────────────────────────────────────────────────────┤
│  0x00000000  ┌─────────────────────────────────────────┐   │
│              │ Bootloader (32 KB)                      │   │
│  0x00008000  ├─────────────────────────────────────────┤   │
│              │ Partition Table (4 KB)                  │   │
│  0x00009000  ├─────────────────────────────────────────┤   │
│              │ NVS (24 KB)                             │   │
│  0x0000F000  ├─────────────────────────────────────────┤   │
│              │ PHY Init (4 KB)                         │   │
│  0x00010000  ├─────────────────────────────────────────┤   │
│              │ Application (up to 4 MB)                │   │
│              │                                         │   │
│  0x00410000  ├─────────────────────────────────────────┤   │
│              │ Storage / SPIFFS (remaining)            │   │
│              │                                         │   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    SRAM (512 KB)                            │
├─────────────────────────────────────────────────────────────┤
│  IRAM (Instruction)   │  128 KB  │  Fast code execution    │
│  DRAM (Data)          │  320 KB  │  Variables, heap        │
│  RTC Memory           │  16 KB   │  Deep sleep retention   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    PSRAM (8 MB)                             │
├─────────────────────────────────────────────────────────────┤
│  Heap extension       │  Used for large allocations        │
│  Display buffers      │  LVGL frame buffers                │
│  Message storage      │  Contact/message database          │
└─────────────────────────────────────────────────────────────┘
```

### Memory Allocation Strategy

| Data Type | Location | Reason |
|-----------|----------|--------|
| Stack | DRAM | Fast access required |
| Crypto keys (temporary) | DRAM | Security, fast zeroing |
| Display buffers | PSRAM | Large size (150+ KB) |
| Message database | PSRAM | Variable size |
| WiFi buffers | DRAM | DMA requirements |
| TLS context | DRAM | Performance |

### Heap Usage Monitoring

```c
// Check available heap
size_t free_heap = esp_get_free_heap_size();
size_t min_free = esp_get_minimum_free_heap_size();

ESP_LOGI(TAG, "Free heap: %d, Min free: %d", free_heap, min_free);

// PSRAM check
size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
ESP_LOGI(TAG, "Free PSRAM: %d", free_psram);
```

### Typical Memory Usage

| Component | DRAM | PSRAM |
|-----------|------|-------|
| FreeRTOS | ~15 KB | - |
| WiFi stack | ~40 KB | - |
| TLS context | ~40 KB | - |
| Application | ~20 KB | - |
| Display buffer | - | ~150 KB |
| **Free** | ~180 KB | ~7.5 MB |

---

## Cryptographic Implementation

### Library Selection

| Algorithm | Library | Reason |
|-----------|---------|--------|
| Ed25519 | libsodium | Well-audited, standard |
| X25519 | libsodium | Well-audited, standard |
| X448 | wolfSSL | Only library with ESP32 support |
| AES-GCM | mbedTLS | Hardware acceleration |
| SHA-512 | mbedTLS | Hardware acceleration |
| HKDF | mbedTLS | Standard implementation |

### X448 Implementation Notes

wolfSSL X448 outputs keys in reverse byte order. SimpleGo includes correction:

```c
// wolfSSL outputs little-endian, SimpleX expects big-endian
void reverse_bytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = tmp;
    }
}

// After wolfSSL X448 operation:
reverse_bytes(shared_secret, 56);
```

### Key Derivation

```c
// X3DH Key Derivation
// info: "SimpleXX3DHKey1"
// salt: 64 zero bytes
hkdf_sha512(shared_secret, sizeof(shared_secret),
            salt_zeros, 64,
            "SimpleXX3DHKey1", 15,
            output, 96);

// Root KDF
// info: "SimpleXRootRatchet"
hkdf_sha512(dh_output, 56,
            current_root_key, 32,
            "SimpleXRootRatchet", 18,
            output, 64);  // 32 new root + 32 chain key

// Chain KDF
// info: "SimpleXChainRatchet"
hkdf_sha512(chain_key, 32,
            NULL, 0,  // No salt
            "SimpleXChainRatchet", 19,
            output, 80);  // 16 header IV + 32 msg key + 16 msg IV + 16 next chain
```

### Performance Benchmarks (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| Ed25519 keygen | ~3 ms |
| Ed25519 sign | ~3 ms |
| Ed25519 verify | ~6 ms |
| X25519 keygen | ~1 ms |
| X25519 DH | ~1 ms |
| X448 keygen | ~15 ms |
| X448 DH | ~15 ms |
| AES-256-GCM (1 KB) | ~0.3 ms |
| SHA-512 (1 KB) | ~0.2 ms |
| HKDF-SHA512 | ~0.5 ms |

---

## Network Stack

### Connection Flow

```
┌─────────────┐
│ WiFi Connect│
└──────┬──────┘
       │
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ DNS Resolve │────►│ smp1.simplexonflux.com → IP    │
└──────┬──────┘     └─────────────────────────────────┘
       │
       ▼
┌─────────────┐
│ TCP Connect │────► Port 5223
└──────┬──────┘
       │
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ TLS 1.3     │────►│ ChaCha20-Poly1305 cipher suite │
│ Handshake   │     │ Certificate verification       │
└──────┬──────┘     └─────────────────────────────────┘
       │
       ▼
┌─────────────┐     ┌─────────────────────────────────┐
│ SMP Handshake────►│ Version negotiation            │
│             │     │ Server key exchange            │
└──────┬──────┘     └─────────────────────────────────┘
       │
       ▼
┌─────────────┐
│ Ready       │
└─────────────┘
```

### TLS Configuration

```c
// mbedTLS configuration for SimpleX
mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3,
                                    MBEDTLS_SSL_MINOR_VERSION_4);  // TLS 1.3

// Cipher suites (TLS 1.3)
int ciphersuites[] = {
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
    MBEDTLS_TLS1_3_AES_256_GCM_SHA384,
    0
};
mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
```

### Buffer Sizes

| Buffer | Size | Purpose |
|--------|------|---------|
| SSL Input | 16 KB | TLS record reception |
| SSL Output | 16 KB | TLS record transmission |
| SMP Command | 4 KB | Protocol command buffer |
| SMP Response | 16 KB | Protocol response buffer |

---

## Performance Characteristics

### Timing Summary

| Operation | Duration |
|-----------|----------|
| Cold boot to WiFi | ~3 sec |
| WiFi reconnect | ~1 sec |
| TLS handshake | ~500 ms |
| SMP handshake | ~200 ms |
| X3DH + Ratchet init | ~50 ms |
| Send message (E2E) | ~30 ms |
| Receive message (E2E) | ~25 ms |

### Power Consumption (T-Deck Plus)

| State | Current |
|-------|---------|
| Active (display on, WiFi) | ~150 mA |
| Active (display off, WiFi) | ~80 mA |
| Light sleep | ~2 mA |
| Deep sleep | ~10 μA |

### Battery Life Estimates (2000 mAh)

| Usage Pattern | Duration |
|---------------|----------|
| Continuous use | ~10 hours |
| Intermittent (5 min/hour) | ~3 days |
| Mostly sleep, hourly check | ~7 days |

---

## Debugging

### Log Levels

| Level | Macro | Typical Use |
|-------|-------|-------------|
| Error | `ESP_LOGE` | Critical failures |
| Warning | `ESP_LOGW` | Unexpected but handled |
| Info | `ESP_LOGI` | Normal operation |
| Debug | `ESP_LOGD` | Development details |
| Verbose | `ESP_LOGV` | Detailed tracing |

### Per-Module Log Control

```c
// In code
esp_log_level_set("SMP_NETWORK", ESP_LOG_DEBUG);
esp_log_level_set("SMP_CRYPTO", ESP_LOG_VERBOSE);
esp_log_level_set("wifi", ESP_LOG_WARN);

// Or via menuconfig
// Component config → Log output → Default log verbosity
```

### Debug Output Example

```
I (1234) SMP_NETWORK: Connecting to smp1.simplexonflux.com:5223
D (1234) SMP_NETWORK: DNS resolved: 123.45.67.89
D (1456) SMP_NETWORK: TCP connected
I (1890) SMP_NETWORK: TLS handshake complete
D (1890) SMP_NETWORK: Cipher: TLS_CHACHA20_POLY1305_SHA256
I (2100) SMP: SMP handshake complete, version 8
D (2100) SMP_CRYPTO: X3DH initiated
D (2150) SMP_CRYPTO: Root key derived
I (2200) SMP: E2E connection established
```

### Common Debug Techniques

```c
// Hex dump
ESP_LOG_BUFFER_HEX_LEVEL(TAG, buffer, len, ESP_LOG_DEBUG);

// Stack high water mark
UBaseType_t stack = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack remaining: %d", stack);

// Heap fragmentation
heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
```

---

## Error Handling

### ESP-IDF Error Codes

| Code | Name | Meaning |
|------|------|---------|
| 0x0000 | ESP_OK | Success |
| 0x0101 | ESP_ERR_NO_MEM | Out of memory |
| 0x0102 | ESP_ERR_INVALID_ARG | Invalid argument |
| 0x0103 | ESP_ERR_INVALID_STATE | Invalid state |
| 0x0104 | ESP_ERR_INVALID_SIZE | Invalid size |
| 0x0105 | ESP_ERR_NOT_FOUND | Not found |

### mbedTLS Error Codes

| Code | Meaning |
|------|---------|
| -0x7780 | SSL handshake failure |
| -0x7200 | Certificate verification failed |
| -0x0010 | GCM authentication failed |
| -0x0042 | Bad input data |
| -0x0044 | Buffer too small |

### Error Handling Pattern

```c
esp_err_t result = some_operation();
if (result != ESP_OK) {
    ESP_LOGE(TAG, "Operation failed: %s (0x%x)", 
             esp_err_to_name(result), result);
    // Cleanup
    return result;
}
```

---

## Porting Guide

### Adding New Device

1. Create device directory:
   ```
   devices/new_device/
   ├── config/
   │   └── device_config.h
   └── hal_impl/
       ├── hal_display.c
       ├── hal_input.c
       └── ...
   ```

2. Add to Kconfig:
   ```
   config SIMPLEGO_DEVICE_NEW_DEVICE
       bool "New Device Name"
       help
           Description of device.
   ```

3. Implement HAL functions for each interface.

4. Test:
   ```bash
   idf.py menuconfig  # Select new device
   idf.py build flash monitor
   ```

See [ADDING_NEW_DEVICE.md](ADDING_NEW_DEVICE.md) for detailed instructions.

---

## References

### ESP-IDF

- Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/
- GitHub: https://github.com/espressif/esp-idf
- API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/

### Cryptography

- wolfSSL: https://www.wolfssl.com/documentation/
- mbedTLS: https://tls.mbed.org/api/
- libsodium: https://doc.libsodium.org/

### SimpleX Protocol

- simplexmq: https://github.com/simplex-chat/simplexmq
- simplex-chat: https://github.com/simplex-chat/simplex-chat
- Protocol docs: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/

### Hardware

- ESP32-S3 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- T-Deck Plus: https://www.lilygo.cc/products/t-deck-plus

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 2.0 | January 2026 | Complete rewrite for HAL architecture |
| 1.0 | January 2026 | Initial technical documentation |
