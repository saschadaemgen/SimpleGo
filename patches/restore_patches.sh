#!/bin/bash
# SimpleGo - Restore original mbedTLS files
# Removes SimpleGo ED25519 patches and restores ESP-IDF defaults

set -e

if [ -z "$IDF_PATH" ]; then
    if [ -d "$HOME/esp/esp-idf" ]; then
        IDF_PATH="$HOME/esp/esp-idf"
    elif [ -d "$HOME/.espressif/esp-idf" ]; then
        IDF_PATH="$HOME/.espressif/esp-idf"
    fi
fi

TARGET_DIR="$IDF_PATH/components/mbedtls/mbedtls/library"

for f in ssl_tls.c ssl_tls13_generic.c x509_crt.c; do
    if [ -f "$TARGET_DIR/$f.orig" ]; then
        cp "$TARGET_DIR/$f.orig" "$TARGET_DIR/$f"
        echo "[OK] Restored $f"
    else
        echo "[WARN] No backup found for $f"
    fi
done

echo "[OK] Original mbedTLS files restored."
