# mbedTLS Patches for SimpleGo

## Why are these patches needed?

SimpleX relay servers use **ED25519 certificates** for TLS connections. The ESP-IDF version of mbedTLS does not support ED25519 signature verification. Without these patches, the TLS handshake fails with error `-0x7780` (fatal alert from server).

These patches modify three mbedTLS source files to gracefully handle ED25519 certificates when `MBEDTLS_SSL_VERIFY_NONE` is set (which SimpleGo uses, since server identity is verified through key hash pinning at the SMP protocol level, not through the TLS certificate chain).

## What do the patches change?

### ssl_tls.c
- Adds Ed25519 and Ed448 to the TLS 1.3 signature algorithms extension in the ClientHello message
- Without this, the server rejects the connection because the client does not advertise support for the server's signature algorithm

### ssl_tls13_generic.c
- Skips certificate signature verification when auth mode is VERIFY_NONE (allows Ed25519-signed certificates to pass through)
- Ignores certificate parse errors for unknown signature algorithms (Ed25519/Ed448)

### x509_crt.c
- Handles unknown signature algorithms (Ed25519/Ed448) in certificate parsing without returning an error
- Handles unknown public key types in certificate parsing without returning an error

## How to apply

### Linux / macOS
```bash
cd SimpleGo
chmod +x patches/apply_patches.sh
./patches/apply_patches.sh
```

### Windows (PowerShell)
```powershell
cd SimpleGo
.\patches\apply_patches.ps1
```

## How to restore originals

### Linux / macOS
```bash
./patches/restore_patches.sh
```

### Windows (PowerShell)
```powershell
.\patches\restore_patches.ps1
```

## Compatibility

These patches are tested with ESP-IDF v5.5.2 and v5.5.3. When upgrading ESP-IDF, re-run the apply script. The original files are backed up as `.orig` so you can always restore them.

## Security note

These patches do NOT reduce security. SimpleGo verifies server identity through **key hash pinning** at the SMP protocol level (the server's public key hash is embedded in the server address). The TLS certificate chain is not used for server authentication. Setting `VERIFY_NONE` with these patches simply prevents mbedTLS from rejecting a valid connection due to an unsupported signature algorithm.
