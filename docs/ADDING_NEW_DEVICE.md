# Adding a New Device to SimpleGo

This guide explains how to add support for a new hardware device to SimpleGo.

## Prerequisites

- Familiarity with your target hardware (GPIO pins, peripherals)
- ESP-IDF development environment (for ESP32 devices)
- Hardware schematic or documentation

## Step 1: Create Device Directory

```bash
# Copy template
cp -r devices/template devices/your_device_id

# Use lowercase with underscores
# Examples: my_esp32_board, custom_messenger, diy_deck
```

## Step 2: Configure device_config.h

Edit `devices/your_device_id/config/device_config.h`:

### Device Identification

```c
#define DEVICE_NAME             "Your Device Name"
#define DEVICE_ID               "your_device_id"
#define DEVICE_MANUFACTURER     "You/Company"
#define DEVICE_MCU              "ESP32-S3"
```

### Display Configuration

Find your display controller and pins from the schematic:

```c
#define DISPLAY_TYPE            "ST7789"      // or ILI9341, SSD1306, etc.
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          320
#define DISPLAY_ROTATION        0             // 0, 90, 180, 270

// SPI pins - CRITICAL: Get these from your schematic!
#define DISPLAY_PIN_MOSI        11
#define DISPLAY_PIN_SCLK        12
#define DISPLAY_PIN_CS          10
#define DISPLAY_PIN_DC          13
#define DISPLAY_PIN_RST         9             // -1 if tied to board reset
#define DISPLAY_PIN_BL          15            // -1 if always on
```

### Input Configuration

Determine your input methods:

**For Physical Keyboard:**
```c
#define KEYBOARD_ENABLED        true
#define KEYBOARD_I2C_ADDR       0x55
#define KEYBOARD_PIN_INT        46
```

**For Rotary Encoder:**
```c
#define ENCODER_ENABLED         true
#define ENCODER_PIN_A           4
#define ENCODER_PIN_B           5
#define ENCODER_PIN_BTN         0
```

**For Touch Screen:**
```c
#define TOUCH_ENABLED           true
#define TOUCH_TYPE              "GT911"
#define TOUCH_I2C_ADDR          0x5D
```

### Feature Flags

Enable only features your device supports:

```c
#define FEATURE_KEYBOARD        0     // 1 = available, 0 = not available
#define FEATURE_ENCODER         1
#define FEATURE_TOUCH           0
#define FEATURE_AUDIO           1
#define FEATURE_BATTERY         1
```

## Step 3: Implement HAL Functions

Create implementations in `devices/your_device_id/hal_impl/`:

### Required HAL Files

| File | Required | Notes |
|------|----------|-------|
| `hal_display_xxx.c` | Yes | Display driver |
| `hal_input_xxx.c` | Yes | Input handling |
| `hal_storage_xxx.c` | Usually | NVS wrapper |
| `hal_network_xxx.c` | Usually | WiFi wrapper |
| `hal_audio_xxx.c` | Optional | Only if audio |
| `hal_system_xxx.c` | Optional | Power management |

### Display Implementation

Implement these functions (see existing implementations for reference):

```c
hal_err_t hal_display_init(const hal_display_config_t *config);
hal_err_t hal_display_deinit(void);
const hal_display_info_t *hal_display_get_info(void);
hal_err_t hal_display_set_backlight(uint8_t level);
hal_err_t hal_display_flush(const hal_rect_t *area, const uint8_t *color_data);
```

### Input Implementation

Based on your input type:

```c
hal_err_t hal_input_init(void);
const hal_input_info_t *hal_input_get_info(void);
int hal_input_poll(void);
void hal_input_set_key_callback(hal_key_callback_t cb, void *user_data);
```

## Step 4: Register Device in Build System

### Add to Kconfig

Edit `Kconfig`:

```kconfig
config SIMPLEGO_DEVICE_YOUR_DEVICE
    bool "Your Device Name"
    help
        Description of your device.
```

And update the device name mapping:

```kconfig
config SIMPLEGO_DEVICE_NAME
    string
    default "your_device_id" if SIMPLEGO_DEVICE_YOUR_DEVICE
```

### Add to CMakeLists.txt

Add to `SUPPORTED_DEVICES`:

```cmake
set(SUPPORTED_DEVICES
    t_deck_plus
    t_embed_cc1101
    your_device_id    # Add here
    custom
)
```

## Step 5: Build and Test

```bash
# Set device
export SIMPLEGO_DEVICE=your_device_id

# Configure
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Common Issues

### Display Not Working

1. Check SPI pins match schematic
2. Verify CS pin is correct
3. Try different `DISPLAY_ROTATION` values
4. Check if display needs inversion (`DISPLAY_INVERT`)

### Touch/Keyboard Not Responding

1. Verify I2C address (use I2C scanner)
2. Check INT pin configuration
3. Verify pull-up resistors

### Build Errors

1. Ensure all required HAL functions are implemented
2. Check `device_config.h` for typos
3. Verify feature flags match implementations

## Testing Checklist

- [ ] Display initializes and shows splash screen
- [ ] Backlight can be adjusted
- [ ] Input methods respond correctly
- [ ] WiFi connects
- [ ] NVS read/write works
- [ ] Audio plays (if available)
- [ ] Battery level reads (if available)
- [ ] Sleep/wake works

## Example: Minimal Device

For a minimal device with just display and buttons:

```c
// device_config.h
#define DEVICE_NAME             "Minimal Device"
#define DEVICE_ID               "minimal"
#define DEVICE_MCU              "ESP32-S3"

// Display
#define DISPLAY_TYPE            "ST7789"
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          240
#define DISPLAY_PIN_MOSI        11
// ... rest of display pins

// Input - just buttons
#define KEYBOARD_ENABLED        false
#define ENCODER_ENABLED         false
#define TOUCH_ENABLED           false
#define BUTTON_COUNT            2
#define BUTTON_A_PIN            0
#define BUTTON_B_PIN            1

// Features
#define FEATURE_KEYBOARD        0
#define FEATURE_BUTTONS         1
#define FEATURE_AUDIO           0
```

## Getting Help

- Check existing device implementations for reference
- Open an issue on GitHub
- Join SimpleX community for discussion

## Contributing

If your device works, please contribute back:

1. Clean up code and add comments
2. Update this documentation if needed
3. Submit a pull request
