
## Status after Session 49 - 2026-03-21

### Queue Rotation (Session 49 - 4 days)
- QADD/QKEY/QUSE/QTEST protocol operational with live server switch
- 7 QADD format iterations, 3 critical protocol rules discovered
- Multi-server: 21 presets (14 SimpleX, 6 Flux, 1 SimpleGo), radio-button UI
- SEC-07: TLS fingerprint verification at 4 connection points
- Server-switch override protects existing contacts until rotation
- Dual-TLS: ~1,500 bytes SRAM per connection, max 2-3 simultaneous
- Live-switch: no reboot, credentials in RAM, NVS save, reconnect
- Bug #32 closed (subscribe_all removed in S48, restored)

### Firmware
- Post-quantum Double Ratchet (sntrup761, five encryption layers)
- Queue Rotation (QADD/QKEY/QUSE/QTEST, live server switch)
- 21 preset SMP servers, SEC-07 fingerprint verification
- NVS 1 MB, 128 PQ contacts, HMAC vault, device-bound HKDF
- Shared statusbar, Matrix screensaver, animated splash
- 6/6 Security Findings CLOSED

### Known Issues (Queue Rotation)
1. Second rotation crashes (state not reset)
2. RQ SUB non-matching frame (auth keys on new server)
3. Chat 10s delay (RQ retries block App Task)
4. Refresh timer endlessly running
5. CQ E2E peer key only first contact
6. Late-arrival flow for offline contacts

### Open Items
- 6 Queue Rotation issues for Session 50
- Alpha firmware binary for simplego.dev/installer
- Per-contact PQ toggle via Queue Rotation (future)
