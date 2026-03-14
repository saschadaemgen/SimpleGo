#!/bin/bash
# SimpleGo - Apply mbedTLS patches for ED25519/Ed448 compatibility
# Required for SimpleX server connections (servers use ED25519 certificates)
# mbedTLS does not natively support ED25519, these patches add compatibility
#
# Usage: ./patches/apply_patches.sh
# Run once after installing ESP-IDF

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find ESP-IDF
if [ -z "$IDF_PATH" ]; then
    if [ -d "$HOME/esp/esp-idf" ]; then
        IDF_PATH="$HOME/esp/esp-idf"
    elif [ -d "$HOME/.espressif/esp-idf" ]; then
        IDF_PATH="$HOME/.espressif/esp-idf"
    else
        echo "[FAIL] ESP-IDF not found. Set IDF_PATH or install ESP-IDF first."
        exit 1
    fi
fi

TARGET_DIR="$IDF_PATH/components/mbedtls/mbedtls/library"

if [ ! -d "$TARGET_DIR" ]; then
    echo "[FAIL] mbedTLS library directory not found: $TARGET_DIR"
    exit 1
fi

echo "SimpleGo - Applying mbedTLS ED25519 patches"
echo "ESP-IDF: $IDF_PATH"
echo "Target:  $TARGET_DIR"
echo ""

# Backup originals
for f in ssl_tls.c ssl_tls13_generic.c x509_crt.c; do
    if [ ! -f "$TARGET_DIR/$f.orig" ]; then
        cp "$TARGET_DIR/$f" "$TARGET_DIR/$f.orig"
        echo "[SAVE] Backed up $f -> $f.orig"
    fi
done

# Apply patches
cp "$SCRIPT_DIR/ssl_tls.c" "$TARGET_DIR/ssl_tls.c"
echo "[OK] Patched ssl_tls.c (Ed25519/Ed448 signature algorithms)"

cp "$SCRIPT_DIR/ssl_tls13_generic.c" "$TARGET_DIR/ssl_tls13_generic.c"
echo "[OK] Patched ssl_tls13_generic.c (certificate verify skip, cert parse error handling)"

cp "$SCRIPT_DIR/x509_crt.c" "$TARGET_DIR/x509_crt.c"
echo "[OK] Patched x509_crt.c (unknown sig alg and PK type handling)"

echo ""
echo "[OK] All patches applied! You can now build SimpleGo."
echo ""
echo "To restore originals, run:"
echo "  ./patches/restore_patches.sh"
