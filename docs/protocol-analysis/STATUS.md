
## Status after Session 47 - 2026-03-16

### UX Overhaul (Session 47)
- QR connection flow: 16 live status stages with auto-navigation and opacity pulsing
- Auto-open removed: incoming messages no longer hijack current screen
- Lock screen restore: chat and settings tab preserved across lock/unlock
- NVS partition: 128 KB to 1 MB (128 PQ contacts at 5.7 KB each)
- PQ chat header: Blue (quantum-resistant) / Yellow (negotiating) / Green (standard)
- Global PQ toggle in Settings (new connections only)
- Per-contact PQ toggle: impossible without Queue Rotation (abandoned after 3 attempts)
- BACKLOG.md introduced (7 categories, 36 entries)
- 25 files changed, 7 bugs closed

### Firmware
- Post-quantum Double Ratchet (sntrup761, five encryption layers)
- 128 contacts with full PQ state (NVS 1 MB)
- AES-256-GCM encrypted SD history, WiFi Manager
- Display name, auto-lock with memory wipe, HMAC NVS vault
- 16-stage connection flow with live status
- 6/6 Security Findings CLOSED

### Bugs
- #22 CLOSED: Standby freeze (settings timer cleanup)
- #23 CLOSED: LVGL stack overflow (heap allocation)
- #24 CLOSED: Empty chat after lock (screen restore)
- #25 OPEN: Timer crash ui_settings_info.c (Szenni)
- #26 CLOSED: PQ NVS ghost cleanup on delete
- #27 OPEN: QR after kernel panic (Szenni)
- #28 OPEN: NTP sync (Szenni)
- #29 CLOSED: Unicode emoji crash

### Open Items
- Aufgabe 2d events re-commit (lost in git checkout)
- Per-contact PQ via Queue Rotation (future)
- Boot test removal, stability optimization
- Alpha firmware binary for simplego.dev/installer
