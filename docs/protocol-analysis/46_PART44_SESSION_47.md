# Part 44: Session 47 - UX Overhaul, NVS Resize, PQ UI Integration
**Date:** 2026-03-15 to 2026-03-16 | **Version:** v0.1.18-alpha

## Overview

Most extensive UX session in the project. Seven bugs closed (#22, #23, #24, #26, #29, plus mbedTLS format and nav-stack fixes). NVS partition enlarged from 128 KB to 1 MB (128 contacts x 5.7 KB = 730 KB requirement). QR code connection flow completely redesigned with 16 live status stages. PQ display in chat header (three states: quantum-resistant/negotiating/standard). Global PQ toggle in Settings. Per-contact PQ toggle attempted and abandoned after deep analysis - PQ state machine cannot be unilaterally disabled without destroying the ratchet chain. Solution for the future: Queue Rotation / Address Renewal. BACKLOG.md introduced.

## Phase 1: Emergency Bug #23 - LVGL Stack Overflow

**Problem:** Chat switch caused immediate reboot. ratchet_state_t grew through PQ integration (Session 46) from 520 bytes to ~5.7 KB. ratchet_load_state() placed this as local variable on the stack. LVGL task has only 8 KB stack. After SEC-01 PSRAM wipe on chat switch, the NVS fallback path triggered and blew the stack.

**Codepath:** on_contact_click() -> smp_set_active_contact() -> ratchet_set_active() -> PSRAM slot empty after SEC-01 wipe -> ratchet_load_state() -> ratchet_state_t on stack -> OVERFLOW

**Fix:** Heap allocation in PSRAM via `heap_caps_malloc(sizeof(ratchet_state_t), MALLOC_CAP_SPIRAM)`. sodium_memzero before heap_caps_free on all exit paths (3 error + 1 success). Architecture decision: Option A (heap, immediate fix) over Option B (async delegation to App Task, added to backlog).

## Phase 2: mbedTLS Format-Specifier Fix

Build error in ssl_tls13_generic.c: "SIMPLEX PATCH" debug message with %x for signed int. GCC -Werror=format blocked entire build. Fix: cast `(unsigned int)(-ret)`. Framework patch, not SimpleGo code.

## Phase 3: NVS Partition 1 MB + Bug #26 Contact Delete + Bug #29 Unicode

### NVS Partition Resize (128 KB to 1 MB)

PQ integration increased NVS per contact from 520 bytes to 5.7 KB. 128 contacts x 5.7 KB = 730 KB. Old 128 KB partition held only ~14 contacts. New: 1 MB (730 KB data + 294 KB reserve for wear-leveling, garbage collection, page headers). Factory partition moved to 0x110000 (64 KB aligned). eFuse HMAC (SEC-02, BLOCK_KEY1) remains fully intact.

Security assessment: SimpleGo is at-rest more secure than Android (eFuse unreadable vs SQLCipher crackable on chip-off) and at-runtime more secure (no baseband, no OS, no other processes).

### Bug #26 CLOSED (CRITICAL): Contact Delete Cleanup

Two separate delete codepaths found: remove_contact() in smp_contacts.c (network DEL) and on_popup_delete() in ui_contacts_popup.c (local delete via UI popup). Both had identical gaps. 7 PQ NVS keys remained as ghosts per deleted contact (pq_XX_act, pq_XX_st, pq_XX_opk, pq_XX_osk, pq_XX_ppk, pq_XX_ct, pq_XX_ss). Per deleted contact: ~5.2 KB NVS garbage including 1,795 bytes cryptographic key material.

Fix: New function ratchet_wipe_slot() with sodium_memzero on PSRAM slot, working copy handling when deleted contact is active (active_slot to 0xFF), cleanup in both delete paths. Navigation to contact list after delete.

### Bug #29 CLOSED: Unicode Crashes idf_monitor

24 lines with emojis and box-drawing in smp_contacts.c replaced with ASCII tags ([OK], [FAIL], [NEW], [DEL], [LIST], [CONN]).

Flash method: erase-flash required (partition table changed).

## Phase 4-5: Auto-Open Removal + Homescreen Live-Refresh

Incoming messages no longer auto-open the chat screen. Three lines removed from main.c (UI_EVT_MESSAGE handler). ui_chat_add_message() still called for SD history and PSRAM cache, but LVGL bubble creation skipped when chat screen not active. Homescreen and contact list now refresh on incoming messages via ui_main_refresh() and ui_contacts_refresh().

## Phase 6: Bug #24 CLOSED - Chat Restore After Lock Screen

Chat screen is ephemeral, destroyed on lock. Navigation stack stores only screen IDs, not parameters (which contact was active). Fix: In ui_manager_lock() save active contact index and screen to static variables. In ui_manager_unlock(): if previous screen was CHAT, fully restore (smp_set_active_contact, ui_chat_set_contact, history load).

## Phase 7: Settings Tab Restore + Bug #22 Root Cause

Settings tab now restored after lock/unlock (same mechanism as Phase 6). Bonus: discovered Bug #22 root cause. ui_settings_cleanup() introduced to stop all timers and null pointers before settings screen deletion. WiFi timers ran after screen destroy, accessing destroyed LVGL objects. THIS was the real root cause of Bug #22 (standby freeze), not the LVGL stack overflow from Bug #23.

## Phase 8: QR Code Connection Flow (16 Live Status Stages)

The largest single UX change. Before: user stuck on QR screen, sees nothing, must navigate back manually. After: automatic navigation to contact list with 16 live status stages.

### Backend Events

Three new event types: UI_EVT_CONNECT_SCANNED (first server response), UI_EVT_CONNECT_NAME (contact name received), UI_EVT_CONNECT_DONE (connection complete). Fired from smp_tasks.c and smp_agent.c.

### RAM-Backed Status Array

s_connect[128] array as central truth source. Survives screen switches (user can go to homescreen and back, status persists). On UI_EVT_CONNECT_SCANNED: auto-navigate from QR screen to contact list. New contact shows "(Knock, knock, NEO)" with animated dots (cycle every 1.5s).

Trigger timing optimization: originally triggered on Reply Queue MSG (~9s after scan). Changed to Contact Queue MSG (~1s after scan). 8 seconds faster perceived wait time.

Event-to-UI latency measured: 260ms (144ms screen switch, 34ms queue latency, 90ms refresh).

### 16 Status Stages (All Uppercase, Opacity Pulsing)

1. SCANNED
2. INVITATION RECEIVED
3. CONNECTING TO PEER
4. KEY EXCHANGE
5. X3DH KEY AGREEMENT (~3s visible, X448 is slow)
6. SENDING CONFIRMATION
7. WAITING FOR PEER
8. DECRYPTING
9. DECRYPTING PEER INFO (~3s visible, PQ Kyber)
10. PEER INFO RECEIVED
11. NAME RECEIVED
12. EXCHANGING KEYS
13. SECURING CHANNEL
14. SENDING HELLO
15. WAITING FOR PEER
16. CONNECTED -> status disappears, normal contact

60-second timeout: if no CONNECT_DONE within 60s, status shows "TIMEOUT", cleaned up after 10s.

### Bugs Fixed During QR Flow Implementation

Navigation stack bug: back button led to QR screen instead of previous page. Fix: ui_manager_remove_from_nav_stack(UI_SCREEN_CONNECT). Bitmap reset bug: s_scanned_notified was static local, never reset. Fix: file-scope, clear on contact create and delete. CONNECT_SCANNED Path B fired on EVERY Reply Queue message. Fix: guard with s_handshake_contact_idx.

## Phase 9: PQ UI Integration

### Settings Toggle

PQ toggle in Settings INFO tab. Reads/writes pq_enabled via existing getter/setter. Four-column layout with NVS status (VAULT/OPEN), eFuse status (BURNED/NONE via esp_efuse_get_key_purpose), and PQ toggle. Branding row: "E2EE PQ" (blue) when PQ on, "E2EE" (green) when PQ off. Toggle applies ONLY to new connections.

### Chat Header PQ Status Display

Read-only, NOT clickable. Three states with colors: Blue "E2EE Quantum-Resistant" (pq_active=1 AND pq_kem_state=2), Yellow "PQ Negotiation..." (pq_active=1 AND pq_kem_state=1), Green "E2EE Standard" (pq_active=0). ui_chat_update_pq_status() reads ratchet state only, writes nothing. Called on chat open and after message receipt.

### Per-Contact PQ Toggle: ABANDONED

Three attempts, all failed:

**Attempt 1:** Only set pq_active. Problem: pq_recv_process() resets pq_active to 1 on every incoming PQ message (lines 696, 737, 795 in smp_ratchet.c).

**Attempt 2:** pq_user_disabled flag with guards at three locations. Problem: sending without PQ (pq_active=0) drops pending_ss from root key derivation. Peer still includes pending_ss. Root keys diverge. "decryption error, fix connection" on Android.

**Attempt 3 (Mausi analysis):** PQ state machine rotates keypairs automatically on EVERY incoming message (line 774: new keypair, line 767: delete old secret key). Peer encrypts with our old public key, but we already deleted the secret key. Decapsulation fails.

**Conclusion:** PQ cannot be unilaterally disabled on a running connection without destroying the ratchet chain. SimpleX Chat also does not have this feature.

**Future solution:** Queue Rotation / Address Renewal. Build new connection with same contact using desired PQ setting. Chat history stays linked on SD. Implementation together with planned protocol optimizations.

**Workaround now:** Global toggle for new connections. To switch existing PQ chat: delete contact, change PQ in settings, reconnect.

All failed PQ toggle changes rolled back via git checkout. pq_user_disabled field and guards remain (harmless, prepared for Queue Rotation).

## Bug Status After Session 47

| Bug | Description | Status |
|-----|------------|--------|
| #22 | Standby freeze (settings timer) | CLOSED - timer cleanup was missing |
| #23 | LVGL stack overflow on chat switch | CLOSED - heap instead of stack |
| #24 | Empty chat after lock screen unlock | CLOSED - screen+contact restore |
| #25 | Timer crash in ui_settings_info.c | OPEN (Szenni report) |
| #26 | Contact delete leaves PQ NVS ghosts | CLOSED - all resources cleaned |
| #27 | QR code after kernel panic reboot | OPEN (Szenni report) |
| #28 | NTP sync never works | OPEN (Szenni report) |
| #29 | Unicode emoji crashes idf_monitor | CLOSED - 24 lines ASCII |

Plus 7 additional bugs found and fixed inline during QR flow and PQ UI implementation.

## BACKLOG.md Created

Seven categories: architecture deep-dives (6 entries), post-MVP features (12), Kickstarter production (6), hardware roadmap (4), UI improvements (2), documentation and community (3), business (3). Researched across all past chats.

## Files Changed (25 files)

partitions.csv, smp_ratchet.h, smp_ratchet.c, smp_contacts.c, ui_contacts_popup.c, smp_tasks.h, smp_tasks.c, smp_agent.c, smp_parser.c, smp_peer.c, main.c, ui_manager.h, ui_manager.c, ui_contacts.h, ui_contacts.c, ui_contacts_row.h, ui_contacts_row.c, ui_settings_info.c, ui_settings.c, ui_settings.h, ui_chat.c, ui_chat.h, CMakeLists.txt, patches/ssl_tls13_generic.c, BACKLOG.md

## Lessons Learned

**L251 (CRITICAL):** ratchet_state_t at ~5.7 KB after PQ must NEVER be on a task stack smaller than 16 KB. LVGL task has 8 KB. Use heap_caps_malloc(MALLOC_CAP_SPIRAM) with sodium_memzero on all exit paths.

**L252 (HIGH):** Ephemeral screens need RAM-backing for state that must survive screen switches. Status information belongs in static RAM variables, not LVGL labels. s_connect[128] pattern established.

**L253 (HIGH):** Every LVGL timer created in an ephemeral screen MUST be stopped and deleted on screen destroy. Bug #22 root cause was WiFi timers accessing destroyed LVGL objects. ui_settings_cleanup() pattern established.

**L254 (CRITICAL):** PQ state machine cannot be unilaterally disabled on a running connection. pending_ss is mixed into root key derivation. Unilateral removal causes ratchet chain divergence. Keypair rotation on every incoming message makes it impossible to hold old state. Solution: Queue Rotation / Address Renewal.

**L255 (HIGH):** Trigger timing is critical for UX. Reply Queue MSG (9s) vs Contact Queue MSG (1s) as trigger for CONNECT_SCANNED = 8 seconds perceived difference. Always use the earliest reliable signal.

**L256:** git checkout is your friend. Failed experiments should be immediately reverted, not patched on top.

**L257:** Contact delete must clean ALL state layers including PQ NVS keys. 7 PQ keys (act, st, opk, osk, ppk, ct, ss) per contact = 5.2 KB NVS garbage with 1,795 bytes key material if missed.

---

*Part 44 - Session 47 UX Overhaul, NVS Resize, PQ UI*
*SimpleGo Protocol Analysis*
*Date: March 15-16, 2026*
*Bugs: 78 total (7 closed: #22, #23, #24, #26, #29 + inline fixes)*
*Lessons: 257 total*
*Security: 6/6 CLOSED (unchanged)*
*NVS: 128 KB to 1 MB*
*PQ per-contact toggle: impossible without Queue Rotation*
