# SimpleGo - Native SimpleX SMP Client for ESP32
# Part of the Sentinel Secure Messenger Suite
#
# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)

# Target: ESP32-S3 (LilyGo T-Deck Plus)
set(IDF_TARGET "esp32s3")

# Ensure wolfssl finds user_settings.h for HAVE_CURVE448
# Must be set BEFORE include(project.cmake) so wolfssl sees it during configure
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/components/wolfssl_config/include" CACHE STRING "" FORCE)

# Include ESP-IDF CMake functions
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(simplex_client)

# Copy user_settings.h into managed wolfssl component (fresh clone builds)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/wolfssl__wolfssl")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/components/wolfssl_config/include/user_settings.h"
         DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/wolfssl__wolfssl/include/")
    message(STATUS "SimpleGo: copied user_settings.h into managed wolfssl component")
endif()

# Fallback: target include for local component builds
if(TARGET __idf_wolfssl__wolfssl)
    target_include_directories(__idf_wolfssl__wolfssl PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/components/wolfssl_config/include")
endif()
