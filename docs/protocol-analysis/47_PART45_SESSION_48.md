# Part 45: Session 48 - Performance, Shared Statusbar, Splash, Matrix, Reconnect
**Date:** 2026-03-16 to 2026-03-17 | **Version:** v0.1.18-alpha

## Overview

Most extensive session in the project's history. 16 hours, 23 files changed, 4 new modules, 2 deleted. Bug #30 (multi-contact performance degradation) closed with 8.6x QR speedup and O(NxM) to O(1) subscription scaling. Shared statusbar module replacing all per-screen duplicates. Complete splash screen redesign with live boot progress. Matrix screensaver with cyan/blue/purple neon rain. Network auto-reconnect (Bug #31). Configurable lock timer and timezone offset. Pending contact abort. 3 crashes resolved.

## Bug #30 CLOSED: Multi-Contact Performance Degradation

### Aschenputtel Analysis

Structured test across 9 blocks identified a feedback loop: subscribe_all_contacts() triggered on EVERY state change during handshake. 7 calls per handshake with 2 contacts. Each call re-subscribed ALL contacts + reply queues + legacy queue. Re-delivery from redundant SUBs triggered further processing that queued more SUBSCRIBE_ALL. Scaling: O(N x M) where N = contacts and M = calls per handshake.

### Fix (4 Phases)

**Phase 2 - Redundant subscribe_all removed:** Three calls eliminated (smp_tasks.c line 349 after add_contact, line 357 after reply_queue_create, line 1104 after each Reply Queue MSG). Rationale: NEW with subMode='S' subscribes queues immediately on creation. Evgeny confirmed: "subsequent SUB is noop."

**Phase 3 - Dedup guard:** s_subscribe_all_pending flag in app_request_subscribe_all() with reset in NET_CMD_SUBSCRIBE_ALL handler. Safety net for unexpected trigger paths.

**Phase 4 - Boot tests removed:** NTP made non-blocking (no more 4.5s wait). sntrup761 standalone test removed (~2190ms). PQ header wire format test removed (~100ms). HKDF KAT test removed (~70ms). Storage self-test removed (~50ms). Total: ~6910ms boot time saved.

**Phase 5 - Padding log spam:** ~100 lines "########" output in SMP parser capped (smp_parser.c). ~1.7s serial output eliminated.

**Bonus fix:** 42d bitmap boot reset. After reboot s_42d_bitmap was empty, every Reply Queue MSG triggered CONNECT_SCANNED and jumped to contact list. Fix: mark all active contacts as 42d-done at boot.

### Results

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| QR creation (2 contacts) | 5590 ms | ~650 ms | 8.6x faster |
| subscribe_all per handshake | 7 calls | 0 | eliminated |
| SUB commands per handshake | 40 | 2-3 | 93% fewer |
| Boot duration | ~16.2 sec | ~8.7 sec | 7.5s saved |
| Boot tests | ~6.9 sec | 0 sec | eliminated |
| Padding spam | ~1.7 sec | 0 sec | eliminated |
| Scaling 128 contacts | ~11 min | < 2 sec | O(NxM) to O(1) |

## Shared Statusbar Module (ui_statusbar.c/h)

New module replacing all per-screen status bar duplicates.

**Two variants:** FULL (screen name left cyan, clock + WiFi bars + battery placeholder right) and CHAT (back arrow + contact name left, PQ status right, no clock/WiFi).

**Pixel art:** px_rect() helper. 4 WiFi bars with real RSSI (thresholds -50/-65/-75 dBm). Battery 20x11 + 2x5 tip (placeholder). Live UTC clock with "--:--" until NTP.

**10-second global timer:** One LVGL timer for all screens. Updates only FULL variant. Checks lock status.

**Crash guard:** s_bar_parent tracking. statusbar_is_active() checks lv_scr_act() == s_bar_parent on all public writes.

**Pointer architecture:** Main gets ALL widget pointers locally via statusbar_widgets_t. hdr_refresh_cb() writes to local pointers, independent of global state. Prevents Guru Meditation when ephemeral screens overwrite globals.

## Screen Migrations

**Main:** Old bar (~110 lines) replaced by statusbar. Local pointers for permanent timer. **Chat:** 16px bar removed, 26px header kept. 42px to 26px = +17px messages. **Contacts:** 43px to 27px = +19px list. **Connect:** Centered title replaced by Statusbar FULL. **Developer screen DELETED.**

## Splash Screen Redesign

"Simple" (white) + "Go" (cyan) Montserrat 28. "private by design" tagline fade-in. 4px cyan progress bar with real boot steps. 9 status calls at real boot locations (10%-90%). progress(100) in app_init_run(). Dynamic final: "eFuse sealed. 5 layers. Quantum-ready." (Vault+PQ). Thread safety: volatile flags cross-core, 100ms poll timer. Old static 2s timer removed.

## Matrix Screensaver

LVGL Canvas RGB565 (~153 KB PSRAM). 40x30 grid, 8x8 bitmap font (48 chars, 384 bytes). 20 FPS. Hardware RNG. Staggered starts, shimmer effect. Cyan (50%), Blue (30%), Purple (20%). NVS key "disp_lock" (0=Simple, 1=Matrix). SEC-04 wipe ALWAYS before both lock types.

## Lock Timer Configurable

7 levels: 5s/10s/20s/30s/60s/5min/15min. NVS key "lock_timer" (uint8_t 0-6, default 4=60s). Settings UI: Sim|Mtx toggle + Timer [-][+].

## Timezone UTC Offset

NVS key "tz_offset" (int8_t, -12 to +14, default 0). Affects statusbar clock, main clock, chat bubble timestamps. Internal: everything UTC, SD history UTC. Privacy: no cities, no countries.

## Pending Contact Abort

New Contact -> Back without scan = dead contact. Two-stage: (1) UI: active=false + num_contacts--. (2) App Task deferred: NVS delete + save. First approach (Network Task) crashed: PSRAM-stack NVS conflict. Changed to volatile flag pattern.

## Bug #31 CLOSED: Network Auto-Reconnect

After ~4 hours: errno=104 ECONNRESET. Server docs confirm subscribed clients NOT disconnected for inactivity. Two-stage: Network Task signals s_reconnect_needed, App Task (SRAM) does TCP+TLS+SMP handshake+subscribe_all, Network Task resumes. Exponential backoff 2s-60s, WiFi check.

## 3 Crashes Resolved

SPI2 assert (animated splash + SD init on SPI2 -> LVGL mutex). NVS cache error (remove_contact on PSRAM stack -> deferred to App Task). MMU entry fault (WiFi replaces splash, timer continues -> active screen guard).

## NTP + Chat Timestamps

ntp_sync_callback() non-blocking. Chat bubble timestamps now use same TZ offset as statusbar clock.

## Bug Status After Session 48

| Bug | Status |
|-----|--------|
| #22-#24, #26, #29 | CLOSED (S47) |
| #25 | CLOSED (S48, confirmed) |
| #27 | OPEN (Szenni) |
| #28 | PARTIAL (NTP works, TZ offset settable) |
| #30 | CLOSED (S48) |
| #31 | CLOSED (S48) |

## Files (23 changed, 4 new, 2 deleted)

main.c, smp_tasks.c/h, smp_events.h, smp_storage.c/h, smp_parser.c, ui_statusbar.c/h (NEW), ui_screensaver.c/h (NEW), ui_manager.c/h, ui_theme.c/h, ui_main.c, ui_contacts.c, ui_connect.c, ui_chat.c, ui_splash.c, ui_settings_info.c, ui_developer.c/h (DELETED).

## Lessons Learned

**L258 (CRITICAL):** subscribe_all_contacts() must ONLY run at boot and after reconnect. NEW with subMode='S' subscribes immediately. Redundant calls create O(NxM) feedback loops.

**L259 (HIGH):** PSRAM-stack tasks must NOT do NVS ops AND must NOT do mbedTLS handshakes. Pattern: volatile flag + App Task deferred work. Proven 3x in this session.

**L260 (HIGH):** Permanent screens need local widget pointers. Ephemeral screens overwrite globals. Timer writes to local pointers only.

**L261:** LVGL animations collide with SD init on SPI2. Mutex lock around SD init.

**L262:** Server does NOT disconnect subscribed clients for inactivity. errno=104 is network/NAT. Auto-reconnect with exponential backoff.

**L263:** Orphaned server queues auto-cleaned. No DEL needed for aborted contacts.

**L264:** Canvas buffer (~153 KB PSRAM) acceptable after SEC-04 wipe. Allocated/freed per lock/unlock cycle.

---

*Part 45 - Session 48: The Most Extensive Session*
*16 hours, 23 files, 4 new modules, 3 crashes resolved*
*Bug #30: O(NxM) to O(1), 8.6x QR speedup*
*Bug #31: Network auto-reconnect*
*From 11-minute subscription loop to Matrix neon rain*
*Bugs: 80 total | Lessons: 264 total*
