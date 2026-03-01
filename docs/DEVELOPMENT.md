# SimpleGo Development Guide

Guide for developers who want to contribute to SimpleGo.

---

## Getting Started

### Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| ESP-IDF | 5.5.2+ | Espressif framework |
| Python | 3.8+ | Build tools |
| Git | 2.30+ | Version control |
| Hardware | ESP32-S3 | T-Deck recommended |

### Clone Repository

Run: git clone https://github.com/cannatoshi/SimpleGo.git

---

## ESP-IDF Installation

### Windows

1. Create directory: mkdir C:\Espressif
2. Clone: git clone --recursive https://github.com/espressif/esp-idf.git
3. Install: .\install.ps1 esp32s3
4. Activate: C:\Espressif\esp-idf\export.ps1

### Linux / macOS

1. Create directory: mkdir -p ~/esp
2. Clone: git clone --recursive https://github.com/espressif/esp-idf.git
3. Install: ./install.sh esp32s3
4. Activate: source ~/esp/esp-idf/export.sh

---

## Project Structure

| Path | Description |
|------|-------------|
| main/ | Application source code |
| main/main.c | Entry point |
| main/smp_*.c | Protocol modules |
| include/ | Header files |
| components/wolfssl/ | X448 cryptography |
| components/kyber/ | Post-quantum (future) |
| docs/ | Documentation |
| CMakeLists.txt | Project build file |
| partitions.csv | Flash partition table |

---

## Build Commands

| Command | Description |
|---------|-------------|
| idf.py set-target esp32s3 | Set target chip |
| idf.py build | Compile project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Serial monitor |
| idf.py build flash monitor -p COM5 | All in one |
| idf.py clean | Clean build |
| idf.py menuconfig | Configuration |

---

## Configuration

### WiFi Credentials

Edit main/main.c:
- Set WIFI_SSID to your network name
- Set WIFI_PASS to your password

### SimpleX Server

Edit main/smp_network.c:
- Set SMP_SERVER_HOST
- Set SMP_SERVER_PORT (default 5223)

---

## Code Style

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Functions | snake_case with prefix | smp_ratchet_init() |
| Variables | snake_case | chain_key |
| Constants | UPPER_SNAKE_CASE | SMP_MAX_MSG_SIZE |
| Types | snake_case with _t | ratchet_state_t |

---

## Debugging

### Log Levels

| Level | Macro | Use Case |
|-------|-------|----------|
| Error | ESP_LOGE | Critical failures |
| Warning | ESP_LOGW | Unexpected |
| Info | ESP_LOGI | Normal operation |
| Debug | ESP_LOGD | Development |
| Verbose | ESP_LOGV | Detailed tracing |

---

## Git Workflow

### Branch Naming

| Type | Format | Example |
|------|--------|---------|
| Feature | feat/description | feat/group-messaging |
| Bugfix | fix/description | fix/kdf-order |
| Documentation | docs/description | docs/crypto-guide |

### Commit Messages

Format: type(scope): description

Types: feat, fix, docs, refactor, test, chore

Example: feat(crypto): add X448 key generation

---

## Troubleshooting

### Build Errors

| Error | Solution |
|-------|----------|
| Component not found | Run idf.py reconfigure |
| Header not found | Check include paths |
| Undefined reference | Add source to CMakeLists.txt |

### Flash Errors

| Error | Solution |
|-------|----------|
| Failed to connect | Check USB cable |
| Wrong chip | Run idf.py set-target esp32s3 |
| Timeout | Hold BOOT button |

---

## Resources

### Documentation

- ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/
- wolfSSL: https://www.wolfssl.com/documentation/
- SimpleX: https://github.com/simplex-chat/simplexmq

### Community

- Issues: https://github.com/cannatoshi/SimpleGo/issues
- Discussions: https://github.com/cannatoshi/SimpleGo/discussions

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
