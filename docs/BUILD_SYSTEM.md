# SimpleGo Build System

## ESP-IDF Build System Basics

SimpleGo verwendet das ESP-IDF Build System. Dieses hat eine **strikte Struktur** die eingehalten werden muss.

### Die zwei CMakeLists.txt Dateien

```
simplex_client/
├── CMakeLists.txt          ← ROOT (nur 10 Zeilen!)
└── main/
    └── CMakeLists.txt      ← COMPONENT (hier werden Sources registriert)
```

**WICHTIG:** Diese beiden Dateien haben KOMPLETT unterschiedliche Aufgaben!

#### Root CMakeLists.txt

Die ROOT `CMakeLists.txt` darf **NUR** folgendes enthalten:

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(simplex_client)
```

Das ist ALLES! Keine Source-Files, keine `idf_component_register()`, nichts!

**Warum?**
- ESP-IDF managed das komplette Build intern
- `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` lädt das ESP-IDF System
- `project()` startet den eigentlichen Build-Prozess
- ESP-IDF scannt dann automatisch alle Verzeichnisse nach Components

#### Component CMakeLists.txt (main/)

Die Component `main/CMakeLists.txt` ist wo die eigentliche Registrierung passiert:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "smp_crypto.c"
        ...
    INCLUDE_DIRS
        "include"
        "hal"
    REQUIRES
        nvs_flash
        esp_wifi
        ...
)
```

**`idf_component_register()` darf NUR in Component-Verzeichnissen aufgerufen werden!**

### Warum unsere Device-CMakeLists nicht funktionierte

Wir hatten versucht, in der ROOT CMakeLists.txt folgendes zu machen:

```cmake
# ❌ FALSCH - Das geht nicht in der ROOT!
idf_component_register(
    SRCS ${ALL_SOURCES}
    ...
)
```

**Error:**
```
CMake Error: Called idf_component_register from a non-component directory.
```

ESP-IDF erkennt: "Hey, das ist die ROOT CMakeLists.txt, keine Component - VERBOTEN!"

## Device-Selection: Der richtige Weg

### Option 1: Kconfig (EMPFOHLEN für ESP-IDF)

Wir nutzen das ESP-IDF Kconfig System:

**Kconfig (im Root):**
```kconfig
menu "SimpleGo Configuration"
    choice SIMPLEGO_DEVICE
        prompt "Target Device"
        default SIMPLEGO_DEVICE_T_DECK_PLUS
        
        config SIMPLEGO_DEVICE_T_DECK_PLUS
            bool "LilyGo T-Deck Plus"
            
        config SIMPLEGO_DEVICE_T_EMBED_CC1101
            bool "LilyGo T-Embed CC1101"
    endchoice
endmenu
```

**Dann in main/CMakeLists.txt:**
```cmake
# Device-spezifische Sources basierend auf Kconfig
if(CONFIG_SIMPLEGO_DEVICE_T_DECK_PLUS)
    set(DEVICE_DIR "${CMAKE_SOURCE_DIR}/devices/t_deck_plus")
elseif(CONFIG_SIMPLEGO_DEVICE_T_EMBED_CC1101)
    set(DEVICE_DIR "${CMAKE_SOURCE_DIR}/devices/t_embed_cc1101")
endif()

file(GLOB HAL_IMPL_SOURCES "${DEVICE_DIR}/hal_impl/*.c")

idf_component_register(
    SRCS
        "main.c"
        ${HAL_IMPL_SOURCES}
        ...
)
```

**Auswahl über menuconfig:**
```bash
idf.py menuconfig
# → SimpleGo Configuration → Target Device → [T-Deck Plus]
```

### Option 2: Separate Components

Jedes Device als eigene ESP-IDF Component:

```
components/
├── hal_t_deck_plus/
│   ├── CMakeLists.txt
│   └── hal_display.c
├── hal_t_embed_cc1101/
│   ├── CMakeLists.txt
│   └── hal_display.c
```

Dann per Kconfig auswählen welche Component included wird.

### Option 3: Build Varianten (sdkconfig)

Mehrere sdkconfig Dateien:

```bash
# T-Deck Plus Build
cp sdkconfig.t_deck_plus sdkconfig
idf.py build

# T-Embed Build
cp sdkconfig.t_embed_cc1101 sdkconfig
idf.py build
```

## Aktuelle Projekt-Struktur

```
simplex_client/
├── CMakeLists.txt              # Minimal! Nur project setup
├── Kconfig                     # Device-Auswahl + Optionen
├── sdkconfig.defaults          # Default Konfiguration
│
├── main/
│   ├── CMakeLists.txt          # Component registration
│   ├── main.c                  # Entry point
│   ├── include/                # Protocol headers
│   │   └── smp_*.h
│   ├── hal/                    # HAL interface headers
│   │   └── hal_*.h
│   └── smp_*.c                 # Protocol implementation
│
├── devices/
│   ├── t_deck_plus/
│   │   ├── config/
│   │   │   └── device_config.h # Hardware-Konstanten
│   │   └── hal_impl/
│   │       └── hal_*.c         # HAL Implementierungen
│   └── t_embed_cc1101/
│       ├── config/
│       └── hal_impl/
│
└── components/                 # Externe Libraries
    ├── kem/
    ├── common/
    └── wolfssl/
```

## Build Commands

```bash
# Standard Build (Default Device aus Kconfig)
idf.py build

# Menuconfig für Device-Auswahl
idf.py menuconfig

# Full Rebuild nach Device-Wechsel
idf.py fullclean
idf.py build

# Flash und Monitor
idf.py flash monitor -p COM5
```

## Häufige Fehler

### "Called idf_component_register from a non-component directory"

**Ursache:** `idf_component_register()` in der ROOT CMakeLists.txt
**Lösung:** Nur in `main/CMakeLists.txt` oder anderen Component-Verzeichnissen verwenden

### "Component xyz not found"

**Ursache:** REQUIRES listet eine Component die nicht existiert
**Lösung:** Component-Namen prüfen, eventuell `managed_components` oder `components` checken

### "Header not found"

**Ursache:** Include-Pfad fehlt in INCLUDE_DIRS
**Lösung:** Pfad zu INCLUDE_DIRS in der Component CMakeLists.txt hinzufügen

## Nächste Schritte

1. **Kconfig vervollständigen** mit allen Device-Optionen
2. **main/CMakeLists.txt anpassen** für Device-abhängige Source-Auswahl
3. **HAL Implementierungen** für jedes Device fertigstellen
4. **sdkconfig.defaults** pro Device erstellen

## Referenzen

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [ESP-IDF Kconfig](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)
- [ESP-IDF Components](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-configuration)
