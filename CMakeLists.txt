# SimpleGo - Native SimpleX SMP Client for ESP32
# Part of the Sentinel Secure Messenger Suite
#
# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)

# Target: ESP32-S3 (LilyGo T-Deck Plus)
set(IDF_TARGET "esp32s3")

# Include ESP-IDF CMake functions
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name
project(simplex_client)