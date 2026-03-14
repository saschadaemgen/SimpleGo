# SimpleGo - Apply mbedTLS patches for ED25519/Ed448 compatibility
# Required for SimpleX server connections (servers use ED25519 certificates)
# mbedTLS does not natively support ED25519, these patches add compatibility
#
# Usage: .\patches\apply_patches.ps1
# Run once after installing ESP-IDF

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Find ESP-IDF
$IdfPath = $env:IDF_PATH
if (-not $IdfPath) {
    # Common Windows ESP-IDF locations
    $candidates = @(
        "C:\Espressif\frameworks\esp-idf-v5.5.2",
        "C:\Espressif\frameworks\esp-idf-v5.5.3",
        "C:\esp\esp-idf",
        "$env:USERPROFILE\esp\esp-idf"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            $IdfPath = $c
            break
        }
    }
}

if (-not $IdfPath -or -not (Test-Path $IdfPath)) {
    Write-Host "[FAIL] ESP-IDF not found. Set IDF_PATH or install ESP-IDF first." -ForegroundColor Red
    exit 1
}

$TargetDir = Join-Path $IdfPath "components\mbedtls\mbedtls\library"

if (-not (Test-Path $TargetDir)) {
    Write-Host "[FAIL] mbedTLS library directory not found: $TargetDir" -ForegroundColor Red
    exit 1
}

Write-Host "SimpleGo - Applying mbedTLS ED25519 patches" -ForegroundColor Cyan
Write-Host "ESP-IDF: $IdfPath"
Write-Host "Target:  $TargetDir"
Write-Host ""

# Backup originals
foreach ($f in @("ssl_tls.c", "ssl_tls13_generic.c", "x509_crt.c")) {
    $orig = Join-Path $TargetDir "$f.orig"
    if (-not (Test-Path $orig)) {
        Copy-Item (Join-Path $TargetDir $f) $orig
        Write-Host "[SAVE] Backed up $f -> $f.orig" -ForegroundColor Yellow
    }
}

# Apply patches
Copy-Item (Join-Path $ScriptDir "ssl_tls.c") (Join-Path $TargetDir "ssl_tls.c")
Write-Host "[OK] Patched ssl_tls.c (Ed25519/Ed448 signature algorithms)" -ForegroundColor Green

Copy-Item (Join-Path $ScriptDir "ssl_tls13_generic.c") (Join-Path $TargetDir "ssl_tls13_generic.c")
Write-Host "[OK] Patched ssl_tls13_generic.c (certificate verify skip, cert parse error handling)" -ForegroundColor Green

Copy-Item (Join-Path $ScriptDir "x509_crt.c") (Join-Path $TargetDir "x509_crt.c")
Write-Host "[OK] Patched x509_crt.c (unknown sig alg and PK type handling)" -ForegroundColor Green

Write-Host ""
Write-Host "[OK] All patches applied! You can now build SimpleGo." -ForegroundColor Green
Write-Host ""
Write-Host "To restore originals, run:"
Write-Host "  .\patches\restore_patches.ps1"
