# SimpleX Protocol Analysis - Part 33: Session 36
# Contact Lifecycle: Delete, Recreate, Zero Compromise

**Document Version:** v1
**Date:** 2026-02-25 Session 36
**Status:** COMPLETED -- Contact lifecycle fully functional
**Previous:** Part 32 - Session 35 (Multi-Contact Victory)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**Path:** C:\Espressif\projects\simplex_client
**Build:** idf.py build flash monitor -p COM6
**License:** AGPL-3.0

---

## SESSION 36 SUMMARY

```
Complete Contact Lifecycle: Create -> Chat -> Delete -> Recreate
No erase-flash required!

4 Sub-Sessions (36a, 36b, 36c, 36d)
12 Git Commits
7 Bugs Fixed (#51-#57)
10 New Lessons (#193-#202)
10 Files Changed

MILESTONE 12: Contact Lifecycle

Date: February 25, 2026
```

---

## Phase 0: Preparation and Baudrate Fix

UART baudrate raised from 115200 to 921600 (PowerShell one-liner).
With 5000+ log lines per session, this reduces UART overhead from ~39s to ~5s (8x speedup).
All 4 sdkconfig entries changed simultaneously.

---

## Phase 1: Task 35g -- NTP Timestamps + Bug E (ConnInfo displayName)

### NTP Time Sync

SNTP initialization added to main.c after WiFi connect.
Max 10s sync timeout, non-blocking.
Chat bubbles now show real timestamps (Mon | 14:35) instead of "--:--".

### Bug E Fix: Contact Name from ConnInfo JSON

**Problem:** handle_conninfo() in smp_agent.c decompressed the JSON from the phone, logged it, but never extracted the displayName field.

**JSON Format:**
```json
{"v":"1-16","event":"x.info","params":{"profile":{"displayName":"Name","fullName":""}}}
```

**Fix:** strstr() search for "displayName":"" in decompressed JSON, extract name into contact_t, persist to NVS, update UI header. Fallback path for uncompressed JSON also covered.

**Commit:** fix(agent): extract displayName from ConnInfo JSON

---

## Phase 2: Hasi's Autonomous Rebuild

Hasi built significantly more than tasked in the first phase:

- Flow rebuild: No auto-QR, no auto-contact on fresh start
- Connection acceleration: Delays reduced from 6.5s to 2s
- Contact list completely rewritten (665 lines)
- Long-press menu with Delete and Info
- Logging cleanup (heartbeat 5min instead of 30s, hex dumps removed)

**Mausi note:** Hasi built features autonomously instead of executing tasks. For Session 37, roles must be enforced more strictly.

**Commits:**
```
refactor(flow): remove auto-QR and auto-contact on fresh start
perf(tasks): reduce handshake delays from 6.5s to 2s
feat(ui): redesign contacts screen matching chat layout
feat(ui): add long-press context menu with delete and info
chore(log): reduce verbose debug logging in network and app tasks
```

---

## Phase 3: Task 36a -- NVS Key Cleanup (Bug #51)

### Problem

When deleting a contact, orphaned NVS keys remained in flash storage.

### Initial Fix (Hasi)

Hasi implemented only rat_XX deletion.

### Mausi Correction

Hasi had only checked the current NVS contents. Mausi read the source code (smp_peer.c line 1131: smp_storage_save_blob_sync("peer_%02x", ...)) and found all four key families:

```
rat_XX  -- Ratchet state (Root Key, Chain Keys, Header Keys)
peer_XX -- Peer state (Queue IDs, DH Keys, Server Host)
hand_XX -- Handshake state (X3DH Keys)
rq_XX   -- Reply Queue state (Auth Private Key)
```

**Lesson #193:** Always search the source code for where keys are written, not the current NVS contents. 100x Reading Principle.

**Commit:** fix(storage): clean all orphaned NVS keys on contact delete

---

## Phase 4: Task 36b -- NVS Cleanup Completed + Format Fix (Bugs #52, #53)

### 4-Key Cleanup Loop

Extended to both delete paths: remove_contact() and on_popup_delete().

### Bug #52: Format Mismatch

cnt_%02x uses hex format, rat_%02u uses decimal format. Identical for indices 0-9, but diverges from 10 onwards (cnt_0a vs rat_10).

**Fix:** All NVS key formatting unified to %02x (hex).

### Bug #53: Parser Double-Underscore

SimpleX changed separator from single underscore to double underscore. Parser must find the last underscore to correctly split agent message types.

**Fix:** fix(parser): handle double-underscore separator in agent message type detection

### Additional Fixes

- 42d bitmap reset: smp_clear_42d(idx) on contact delete
- LIST_H macro collision with FreeRTOS renamed to CLIST_H

**Commit:** fix(tasks): clear 42d bitmap on contact delete for slot reuse

---

## Security Discussion: NVS Keys and Encryption

### Prince asked: "Is this a security feature?"

### Mausi's Analysis: Yes, critical.

The four NVS key families contain:

| Key | Contents | Compromise Impact |
|-----|----------|-------------------|
| rat_XX | Root Key, Chain Keys, Header Keys | Decrypt past + future messages |
| peer_XX | Queue IDs, DH Keys, Server Host | Identity theft |
| hand_XX | X3DH Handshake Keys | Foundation of encryption |
| rq_XX | Reply Queue Auth Private Key | Send messages as the user |

### Critical Discovery

NVS is currently NOT encrypted. nvs_flash_init() instead of nvs_flash_secure_init(). All crypto keys stored in plaintext in flash.

**Lesson #194:** NVS keys are currently NOT encrypted (nvs_flash_init not nvs_flash_secure_init). All crypto keys in plaintext.

**Lesson #195:** What is not there cannot be stolen. Deletion > trusting encryption. Defense in Depth.

**TODO (Production):** NVS Encryption (nvs_flash_secure_init + eFuse keys) together with post-quantum crypto (CRYSTALS-Kyber) for Kickstarter phase.

---

## Phase 5: Task 36c -- KEY-before-HELLO Race Condition (Bug #54)

### The Smoking Gun

```
(245267) APP: 42d -- SEND_KEY queued (slot=0)
(245767) peer_send_hello starts
(246797) HELLO sent via Peer (sock 56)          <- HELLO FIRST!
(247197) NET Task executes KEY now (sock 54)     <- KEY AFTER!
```

### Root Cause

App Task queued KEY to Net Task via Ring Buffer, waited only 500ms blind, then fired HELLO immediately over a separate Peer connection. Two different sockets, no synchronization.

### Why the Phone Stays on "connecting"

1. Phone receives HELLO
2. Phone wants to respond on Reply Queue
3. KEY has not arrived yet -> Server does not have Phone's auth key
4. Server rejects with ERR AUTH
5. Phone stays on "connecting"

### Fix: FreeRTOS TaskNotification

```c
// s_app_task_handle + NOTIFY_KEY_DONE define
// Net Task sends xTaskNotify() after KEY OK/Fail/Timeout (all 3 paths!)
// App Task waits with xTaskNotifyWait(5000ms) instead of vTaskDelay(500ms)
// No deadlock risk because all paths notify
```

**Key Design Decision:** xTaskNotify must fire on ALL paths (success, failure, timeout) -- otherwise the waiting task hangs forever.

**Evgeny quote confirming:** "concurrency is hard."

**Lesson #196:** TaskNotification is more lightweight than Semaphore for 1:1 task synchronization.

**Lesson #197:** xTaskNotify must send on ALL paths (success, error, timeout) or the waiting task deadlocks.

**Commit:** fix(tasks): synchronize KEY-before-HELLO with FreeRTOS TaskNotification

---

## Phase 6: Task 36d -- UI Cleanup on Delete (Bugs #55, #56, #57)

Three problems solved simultaneously:

### Bug #55: Chat Bubbles Survive Delete

New function ui_chat_clear_contact(int idx) deletes all LVGL bubble objects with matching contact tag.

**Lesson #198:** LVGL bubble objects must be explicitly deleted on contact delete -- they survive the contact.

### Bug #56: QR Code Flashes After Delete

New function ui_connect_reset() hides QR, shows placeholder, sets status to "Generating...".

### Bug #57: Stale QR on "+ New"

ui_connect_reset() also called in on_bar_new() BEFORE smp_request_add_contact().

**Lesson #199:** QR code widget caches last content -- must be explicitly reset on Delete AND before New.

**Commit:** fix(ui): clear chat bubbles and reset QR cache on contact delete

---

## Complete Bug List (Session 36)

| Bug | Description | Root Cause | Fix | Phase |
|-----|-------------|------------|-----|-------|
| E | Contact name shows placeholder | displayName not extracted from ConnInfo JSON | strstr() + NVS persist | 35g |
| #51 | Orphaned NVS keys on delete | Only rat_XX deleted, 3 families missed | 4-key cleanup loop | 36a |
| #52 | NVS key format mismatch | %02u (decimal) vs %02x (hex) diverges at idx 10+ | Unified to %02x | 36b |
| #53 | Parser fails on INVITATION | SimpleX changed _ to __ separator | Find last underscore | 36b |
| #54 | KEY-HELLO race condition | 500ms blind delay, 2 sockets, no sync | FreeRTOS TaskNotification | 36c |
| #55 | Chat bubbles survive delete | LVGL objects not cleared | ui_chat_clear_contact() | 36d |
| #56 | QR code flashes after delete | Widget caches last content | ui_connect_reset() | 36d |
| #57 | Stale QR on "+ New" | No reset before new contact request | ui_connect_reset() in on_bar_new() | 36d |

---

## Additional Fixes (Non-Bug)

| Fix | Description |
|-----|-------------|
| 42d bitmap reset | smp_clear_42d(idx) on contact delete for slot reuse |
| LIST_H rename | FreeRTOS list.h defines LIST_H as include guard -- renamed to CLIST_H |
| UART baudrate | 115200 -> 921600 (8x speedup for 5000+ line logs) |
| Heartbeat interval | 30s -> 5min (reduced log noise) |
| Hex dump removal | Debug hex dumps stripped from production paths |
| Connection delays | 6.5s -> 2s handshake delay reduction |

---

## All Lessons Learned (Session 36)

| # | Lesson | Context |
|---|--------|---------|
| 193 | Always search source code for where NVS keys are written, not current NVS contents. 100x Reading Principle. | 36a: Mausi found 4 key families vs Hasi's 1 |
| 194 | NVS is currently NOT encrypted (nvs_flash_init not nvs_flash_secure_init). All crypto keys in plaintext in flash. | Security audit |
| 195 | What is not there cannot be stolen. Deletion > trusting encryption. Defense in Depth. | Security principle |
| 196 | FreeRTOS TaskNotification is more lightweight than Semaphore for 1:1 task synchronization | KEY-HELLO sync |
| 197 | xTaskNotify must send on ALL paths (success, error, timeout) or the waiting task deadlocks permanently | Deadlock prevention |
| 198 | LVGL bubble objects must be explicitly deleted on contact delete -- they survive the contact | ui_chat_clear_contact |
| 199 | QR code widget caches last content -- must be explicitly reset on Delete AND before New | ui_connect_reset |
| 200 | FreeRTOS list.h defines LIST_H as include guard -- own macros must not collide | CLIST_H rename |
| 201 | SimpleX changed from single underscore to double underscore separator -- parser must find last underscore | Double-underscore bug |
| 202 | UART 115200 at 5000+ log lines = ~39s overhead. 921200 = ~5s. 8x faster, one sdkconfig change. | Baudrate fix |

---

## Files Changed -- Session 36 Overview

| File | Path | Changes |
|------|------|---------|
| main.c | main/ | NTP init, flow rebuild (no auto-QR/contact) |
| smp_agent.c | main/protocol/ | Bug E: displayName from ConnInfo JSON |
| smp_parser.c | main/protocol/ | Double-underscore separator fix |
| smp_tasks.c | main/core/ | 42d bitmap reset, KEY-HELLO TaskNotification, delay reduction, logging |
| smp_contacts.c | main/state/ | 4-key NVS cleanup in remove_contact() |
| ui_contacts.c | main/ui/screens/ | Complete redesign, long-press menu, 4-key NVS cleanup, chat clear, QR reset |
| ui_chat.c | main/ui/screens/ | ui_chat_clear_contact(), dynamic header |
| ui_chat.h | main/ui/screens/ | Declaration ui_chat_clear_contact() |
| ui_connect.c | main/ui/screens/ | ui_connect_reset() |
| ui_connect.h | main/ui/screens/ | Declaration ui_connect_reset() |

---

## Git Commits (Session 36)

```
feat(ntp): add SNTP time sync after WiFi connect
fix(agent): extract displayName from ConnInfo JSON
refactor(flow): remove auto-QR and auto-contact on fresh start
perf(tasks): reduce handshake delays from 6.5s to 2s
feat(ui): redesign contacts screen matching chat layout
feat(ui): add long-press context menu with delete and info
chore(log): reduce verbose debug logging in network and app tasks
fix(parser): handle double-underscore separator in agent message type detection
fix(storage): clean all orphaned NVS keys on contact delete
fix(tasks): clear 42d bitmap on contact delete for slot reuse
fix(tasks): synchronize KEY-before-HELLO with FreeRTOS TaskNotification
fix(ui): clear chat bubbles and reset QR cache on contact delete
```

---

## Open Bugs (after Session 36)

| Bug | Description | Priority | Target |
|-----|-------------|----------|--------|
| - | Umlauts not rendered (LVGL font) | P2 | Session 37 |
| - | Server DEL missing on UI delete | P3 | Session 37+ |
| - | First message invisible on fresh contact | P4 | Session 37+ |
| - | SPI display glitches | P4 | Later |

---

## Next Session: 37

### Main Goals
1. Encrypted chat history on SD card (architecture + implementation)
2. LVGL font rebuild with umlauts (Latin-1 Supplement)
3. Contact list enhancements (last message, timestamp, counter, unread badge)

### Dependencies
- Contact list enhancements depend on SD history (needs data)
- SD infrastructure already fully prepared
- Font rebuild is independent and can run in parallel

---

## Architecture Insight: Contact Lifecycle Pattern

```
SESSION 36 PATTERN: Every lifecycle operation must clean ALL state layers

  Create Contact:
    -> NVS: rat_XX, peer_XX, hand_XX, rq_XX (4 key families)
    -> PSRAM: ratchet slot, handshake slot, contact struct, reply queue
    -> UI: contact list entry, chat bubbles, QR code
    -> Network: SMP queues on server

  Delete Contact:
    -> NVS: erase all 4 key families (%02x format!)
    -> PSRAM: zero slots, clear contact struct, clear reply queue
    -> Bitmap: smp_clear_42d(idx)
    -> UI: clear bubbles, reset QR, remove list entry
    -> Network: TODO -- server DEL command (Session 37+)

  Recreate Contact:
    -> All slots clean, no orphaned state
    -> New handshake proceeds as if first contact ever
    -> No erase-flash required!
```

---

## Session 36 Statistics

| Metric | Value |
|--------|-------|
| Sub-sessions | 4 (36a, 36b, 36c, 36d) |
| Git commits | 12 |
| Bugs fixed | 7 (#51-#57) + Bug E |
| New lessons | 10 (#193-#202) |
| Files changed | 10 |
| Lines rewritten | 665+ (contact list alone) |
| UART speedup | 8x (115200 -> 921600) |
| Handshake speedup | 3.25x (6.5s -> 2s) |

---

*Part 33 - Session 36 Contact Lifecycle*
*SimpleGo Protocol Analysis*
*Date: February 25, 2026*
*Bugs: 57 total (all FIXED) + Bug E*
*Lessons: 202 total*
*Milestone 12: Contact Lifecycle*
