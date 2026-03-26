
## Status after Session 50 - 2026-03-26

### Queue Rotation Multi-Fix (Session 50)
- Unlimited consecutive rotations working (4 verified with PQ crypto)
- 6 fixes: cache timing, conditional auth/DH backup, snd_id semantics, Phase 1b keys
- Root cause: stale CQ E2E peer key cache filled during rotation by incoming QTEST
- Cache invalidation now at BOTH rotation_start() AND after Phase 1b key write
- 1 day lost on Mausi error (wrong bug classification accepted without verification)

### Firmware
- Post-quantum Double Ratchet (sntrup761, five encryption layers)
- Queue Rotation: unlimited consecutive with live server switch
- 21 preset SMP servers, SEC-07 fingerprint at 4 TLS points
- NVS 1 MB, 128 PQ contacts, HMAC vault, device-bound HKDF
- Shared statusbar, Matrix screensaver, animated splash
- 6/6 Security Findings CLOSED

### Remaining Rotation Issues
- Late-arrival flow (offline contacts during rotation)
- Legacy RQ SUB FAILED after rotation
- sdmmc DMA errors during tight SRAM windows
- Refresh timer doesn't stop after DONE

### Open Items
- GoChat reference dump (layer-by-layer confirmation)
- Alpha firmware binary for simplego.dev/installer
- Rotation error handling (timeouts, retries)
