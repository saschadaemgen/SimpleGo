
## Status after Session 48 - 2026-03-17

### Performance (Session 48)
- Bug #30 CLOSED: subscribe feedback loop eliminated, O(NxM) to O(1)
- QR creation: 5590ms to 650ms (8.6x faster)
- Boot: 16.2s to 8.7s (7.5s saved, all boot tests removed)
- 128 contacts scaling: 11 minutes to < 2 seconds

### New Modules (Session 48)
- Shared statusbar (FULL + CHAT variants, pixel-art WiFi/battery, global timer)
- Splash screen (live progress 9 stages, dynamic final message, cross-core safe)
- Matrix screensaver (canvas RGB565 ~153 KB, 20 FPS, cyan/blue/purple)
- Lock timer configurable (5s/10s/20s/30s/60s/5min/15min)
- Timezone UTC offset (-12 to +14, privacy-first, no cities)
- Pending contact abort (two-stage: UI immediate + App Task deferred)

### Infrastructure (Session 48)
- Bug #31 CLOSED: Network auto-reconnect (exponential backoff 2s-60s)
- NTP non-blocking callback
- Developer screen deleted, theme cleanup (~120 lines removed)
- 3 crashes resolved (SPI2 splash, NVS PSRAM-stack, MMU WiFi-splash)
- 23 files changed, 4 new modules, 2 deleted

### Firmware
- Post-quantum Double Ratchet (sntrup761, five encryption layers)
- NVS 1 MB, 128 PQ contacts, HMAC vault, device-bound HKDF
- 16-stage QR connection flow, PQ chat header
- Shared statusbar, Matrix screensaver, animated splash
- Auto-lock with memory wipe, configurable timer
- 6/6 Security Findings CLOSED

### Bugs
- #25 CLOSED (S48), #30 CLOSED (S48), #31 CLOSED (S48)
- #27 OPEN (Szenni: QR after panic reboot)
- #28 PARTIAL (NTP works, TZ settable)

### Open Items
- Events re-commit from S47 git checkout
- Row-update optimization (no full contact list rebuild)
- Alpha firmware binary for simplego.dev/installer
- Multi-server management (hardcoded server removal)
