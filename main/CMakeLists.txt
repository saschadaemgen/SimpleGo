# Use our own lv_conf.h
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h)

# SimpleGo - Main Component
# Professional HAL Architecture
# SPDX-License-Identifier: AGPL-3.0

# Device selection (default: t_deck_plus)
set(SIMPLEGO_DEVICE "t_deck_plus" CACHE STRING "Target device")
set(DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../devices/${SIMPLEGO_DEVICE}")

message(STATUS "========================================")
message(STATUS "  SimpleGo - Building for: ${SIMPLEGO_DEVICE}")
message(STATUS "========================================")

idf_component_register(
    SRC_DIRS
        "."
        "protocol"
        "crypto"
        "net"
        "state"
        "core"
        "util"
        "ui"
        "ui/screens"
        "ui/widgets"
        "ui/fonts"
        "${DEVICE_DIR}/hal_impl"
    INCLUDE_DIRS
        "include"
        "hal"
        "ui"
        "ui/screens"
        "ui/widgets"
        "ui/fonts"
        "ui/themes"
        "${DEVICE_DIR}/config"
        "${DEVICE_DIR}/hal_impl"
        "../components/kem"
        "../components/common"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        mbedtls
        libsodium
        wolfssl_config
        wolfssl__wolfssl
        driver
        esp_lcd
        esp_timer
        lvgl
        zstd
        sdmmc
        fatfs
        vfs
        sntrup761
        efuse
)
