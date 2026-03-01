# Hardware Abstraction Layer (HAL) Architecture

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

## Design Philosophy

One codebase supports many devices. SimpleX Protocol and UI are written once; only hardware-specific code is adapted per device.

```
┌─────────────────────────────────────────────────────────────────┐
│                    APPLICATION / UI                             │  ← Same for all
│              (Screens, Menus, Chat Views, Settings)             │
├─────────────────────────────────────────────────────────────────┤
│                      CORE / PROTOCOL                            │  ← Same for all
│         (SimpleX SMP, Double Ratchet, X3DH, Crypto)            │
├─────────────────────────────────────────────────────────────────┤
│              HAL (Hardware Abstraction Layer)                   │  ← Interface (same)
│      hal_display, hal_input, hal_network, hal_storage, etc.    │
├───────────────┬───────────────┬───────────────┬─────────────────┤
│  T-Deck Plus  │ T-Embed CC1101│ Raspberry Pi  │ Custom Hardware │ ← Per device
│  320×240 LCD  │  170×320 LCD  │  HDMI/SDL     │    (varies)     │
│  Keyboard     │  Encoder      │  USB Keyboard │                 │
└───────────────┴───────────────┴───────────────┴─────────────────┘
```

---

## Directory Structure

```
simplex_client/
├── main/
│   ├── core/                    ← SimpleX Protocol (100% shared)
│   ├── hal/                     ← Interface Headers (100% shared)
│   └── ui/                      ← LVGL UI (100% shared)
│
├── devices/
│   ├── t_deck_plus/
│   │   ├── config/device_config.h
│   │   └── hal_impl/            ← Device-specific implementations
│   ├── t_embed_cc1101/
│   ├── raspberry_pi/
│   └── simplego_diy/
```

---

## HAL Interfaces

### hal_display.h

```c
int hal_display_init(void);
void hal_display_get_config(hal_display_config_t *config);
void hal_display_flush(const hal_display_area_t *area, const uint8_t *pixels);
void hal_display_set_brightness(uint8_t brightness);
void hal_display_full_refresh(void);  // E-Ink only
```

### hal_input.h

```c
int hal_input_init(void);
void hal_input_get_caps(hal_input_caps_t *caps);
bool hal_input_poll(hal_input_event_t *event);
bool hal_input_wait(hal_input_event_t *event, uint32_t timeout_ms);
```

### hal_network.h

```c
int hal_net_init(void);
int hal_net_wifi_connect(const hal_wifi_config_t *config);
int hal_net_tls_connect(const char *host, uint16_t port, ...);
int hal_net_send(int socket, const uint8_t *data, size_t len, uint32_t timeout_ms);
int hal_net_recv(int socket, uint8_t *buffer, size_t max_len, uint32_t timeout_ms);
```

### hal_secure_element.h

```c
int hal_se_init(void);
int hal_se_generate_keypair(hal_se_slot_t slot, hal_se_curve_t curve);
int hal_se_get_pubkey(hal_se_slot_t slot, uint8_t *pubkey, size_t *pubkey_len);
int hal_se_ecdh(hal_se_slot_t slot, const uint8_t *peer_pubkey, size_t len, uint8_t *shared_secret);
int hal_se_sign(hal_se_slot_t slot, const uint8_t *hash, size_t len, uint8_t *sig, size_t *sig_len);
int hal_se_zeroize(void);  // Emergency erase
```

### hal_storage.h

```c
int hal_storage_init(void);
int hal_storage_write(hal_storage_namespace_t ns, const char *key, const uint8_t *data, size_t len);
int hal_storage_read(hal_storage_namespace_t ns, const char *key, uint8_t *data, size_t *len);
int hal_storage_erase_all(void);  // Factory reset
```

---

## Device Configurations

### T-Deck Plus

```c
#define DEVICE_NAME             "T-Deck Plus"
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define HAS_KEYBOARD            1
#define HAS_TRACKBALL           1
#define HAS_TOUCH               1
#define HAS_WIFI                1
#define HAS_SECURE_ELEMENT      0  // Enable when SE added
```

### T-Embed CC1101

```c
#define DEVICE_NAME             "T-Embed CC1101"
#define DISPLAY_WIDTH           170
#define DISPLAY_HEIGHT          320
#define HAS_ENCODER             1
#define HAS_SUBGHZ              1  // CC1101
#define UI_COMPACT_MODE         1
```

### Raspberry Pi

```c
#define DEVICE_NAME             "SimpleGo Desktop"
#define DISPLAY_CONTROLLER      "SDL2"
#define HAS_KEYBOARD            1  // USB
#define STORAGE_TYPE            "FILESYSTEM"
#define FEATURE_DEBUG_CONSOLE   1
```

---

## Build System

```bash
# T-Deck Plus (ESP-IDF)
SIMPLEGO_DEVICE=t_deck_plus idf.py build flash monitor -p COM5

# Raspberry Pi (CMake)
mkdir build && cd build
cmake -DSIMPLEGO_DEVICE=raspberry_pi ..
make && ./simplego
```

---

## Adding a New Device

1. Create `devices/my_device/config/device_config.h`
2. Implement all HAL interfaces in `devices/my_device/hal_impl/`
3. Build: `SIMPLEGO_DEVICE=my_device idf.py build`
4. Submit PR with documentation

---

## Feature Comparison

| Feature | T-Deck Plus | T-Embed CC1101 | Raspberry Pi | SimpleGo DIY |
|---------|-------------|----------------|--------------|--------------|
| Display | 320×240 LCD | 170×320 LCD | Variable | Configurable |
| Input | KB + Trackball | Encoder + Buttons | USB KB/Mouse | Configurable |
| Radio | LoRa (opt) | CC1101 Sub-GHz | None | LoRa (opt) |
| Secure Element | External | External | USB/Software | ATECC608B |
