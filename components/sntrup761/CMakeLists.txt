# sntrup761 ESP-IDF Component
#
# Post-quantum KEM based on Streamlined NTRU Prime 761.
# Source: PQClean (round3 tag), crypto_kem/sntrup761/clean/
# License: Public Domain (PQClean), AGPL-3.0 (SimpleGo wrapper)
#
# Platform dependencies:
#   - esp_hw_support: esp_fill_random() for hardware RNG
#   - mbedtls: mbedtls_sha512() for SHA-512 (hardware accelerated on ESP32-S3)

# Collect all PQClean source files
file(GLOB PQCLEAN_SRCS "pqclean/*.c")

# Platform glue + SimpleGo wrapper + crypto task + test
set(PLATFORM_SRCS
    "platform/randombytes_esp32.c"
    "platform/sha2_esp32.c"
    "sntrup761.c"
    "pq_crypto_task.c"
    "test/test_sntrup761.c"
)

idf_component_register(
    SRCS ${PQCLEAN_SRCS} ${PLATFORM_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "pqclean" "platform"
    REQUIRES mbedtls esp_hw_support libsodium freertos esp_timer
)

# Suppress warnings in PQClean code (public domain, we don't modify)
# PQClean uses some patterns that trigger -Wsign-compare etc.
set_source_files_properties(${PQCLEAN_SRCS} PROPERTIES
    COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter"
)
