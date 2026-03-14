# SimpleGo - Restore original mbedTLS files
# Removes SimpleGo ED25519 patches and restores ESP-IDF defaults

$IdfPath = $env:IDF_PATH
if (-not $IdfPath) {
    $candidates = @(
        "C:\Espressif\frameworks\esp-idf-v5.5.2",
        "C:\Espressif\frameworks\esp-idf-v5.5.3",
        "C:\esp\esp-idf",
        "$env:USERPROFILE\esp\esp-idf"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $IdfPath = $c; break }
    }
}

$TargetDir = Join-Path $IdfPath "components\mbedtls\mbedtls\library"

foreach ($f in @("ssl_tls.c", "ssl_tls13_generic.c", "x509_crt.c")) {
    $orig = Join-Path $TargetDir "$f.orig"
    if (Test-Path $orig) {
        Copy-Item $orig (Join-Path $TargetDir $f)
        Write-Host "[OK] Restored $f" -ForegroundColor Green
    } else {
        Write-Host "[WARN] No backup found for $f" -ForegroundColor Yellow
    }
}

Write-Host "[OK] Original mbedTLS files restored." -ForegroundColor Green
