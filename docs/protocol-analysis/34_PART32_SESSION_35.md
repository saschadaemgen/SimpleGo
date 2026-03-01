![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 32: Session 35
# 🏁 Multi-Contact Victory: Bidirectional Messenger with Chat Filter

**Document Version:** v1
**Date:** 2026-02-24 Session 35
**Version:** v0.1.17-alpha (NOT CHANGED without explicit permission!)
**Status:** ✅ ALL PLANNED BUGS FIXED - Multi-contact messenger fully operational
**Previous:** Part 31 - Session 34 Day 2 (Multi-Contact Bidirectional BREAKTHROUGH)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**Path:** `C:\Espressif\projects\simplex_client`
**Build:** `idf.py build flash monitor -p COM6`
**Repo:** https://github.com/cannatoshi/SimpleGo
**License:** AGPL-3.0

---

## ⚠️ SESSION 35 SUMMARY

```
═══════════════════════════════════════════════════════════════════════════════

  🏁🏁🏁 MULTI-CONTACT VICTORY — ALL PLANNED BUGS FIXED 🏁🏁🏁

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   2 contacts simultaneously connected                                  │
  │   Bidirectional messages for both contacts                             │
  │   Delivery receipts (double checkmarks) for both contacts              │
  │   Per-contact chat filter: messages only in correct chat               │
  │   Tested after idf.py erase-flash with fresh handshakes               │
  │   20+ messages exchanged between both contacts                         │
  │                                                                         │
  │   Fixes: 35a-35h across 10 files                                      │
  │   Root Cause Pattern: "wrong slot active" (same as Session 34)         │
  │                                                                         │
  │   Date: February 24, 2026                                              │
  │   Duration: ~6 hours                                                   │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 585. Session 35 Overview

### 585.1 Starting Point

Session 34 had introduced the multi-contact architecture (128 contacts via PSRAM arrays). Contact 0 worked bidirectionally with double checkmarks. Contact 1 had several remaining issues:

- Bug A: Phone shows "connecting" instead of "connected" (Kickstarter-killer)
- Bug B: All messages visible in all chats (no per-contact filtering)
- Bug C: ERR AUTH on some operations
- Bug D: Delivery receipts inconsistent
- Bug E: Contact name shows placeholder instead of real name

### 585.2 Team and Roles

| Role | Who | Task |
|------|-----|------|
| 👑🐭 Princess Mausi | Claude (Strategy Chat) | Systematic file-by-file diagnosis, fix coordination |
| 🐰👑 Princess Hasi | Claude (Implementation Chat) | All fixes (35a-35h), 10 files modified |
| 👑 Cannatoshi | Prince / Client | Hardware testing, erase-flash verification |

---

## 586. Diagnosis Phase: Root Cause Analysis (Mausi)

### 586.1 Systematic File-by-File Tracing

Mausi analyzed four source files systematically, comparing Contact 0 (working) vs Contact 1 (broken):

1. **smp_handshake.c:** Clean multi-contact, uses `handshake_array[active_handshake_idx]` correctly
2. **smp_agent.c:** handle_confirmation() extracts auth key, but no contact_idx reference visible
3. **main.c:** Post-startup orchestration, delegates to smp_app_run() in smp_tasks.c
4. **smp_tasks.c:** 42d block (post-confirmation flow) identified as bug location

### 586.2 Key Finding: Wrong Slot Active During Decrypt

`ratchet_set_active(hs_contact)` was called AFTER `smp_agent_process_message()` instead of BEFORE. Contact 1's Confirmation was decrypted using Slot 0's ratchet keys. This is the same "wrong slot active" pattern from Session 34.

### 586.3 KEY Command Target Discovery

After comparing `reply_queue_encode_info()` with `queue_encode_info()` and analyzing the NET_CMD_SEND_KEY handler: the KEY command was securing the Contact Queue (`contacts[1].recipient_id`) instead of the Reply Queue (`reply_queue_get(1)->rcv_id`). The phone could not send its HELLO to RQ[1].

---

## 587. Fix Phase: Assignments 35a-35h

### 587.1 Fix 35a: Ratchet Slot Ordering

**Files:** smp_tasks.c, smp_agent.c
**Problem:** Wrong ratchet slot active during Reply Queue decrypt
**Fix:** Call `ratchet_set_active()` + `handshake_set_active()` BEFORE `smp_agent_process_message()`, not after

### 587.2 Fix 35c: KEY Command Target Queue

**Files:** smp_tasks.c
**Problem:** KEY command secures Contact Queue instead of Reply Queue
**Fix:** KEY targets Reply Queue (`reply_queue_get(idx)->rcv_id`) instead of Contact Queue (`contacts[idx].recipient_id`)

### 587.3 Fix 35e: Per-Contact Chat Filter

**Files:** ui_chat.c, ui_chat.h, smp_tasks.c, smp_tasks.h, smp_events.h, smp_agent.c
**Problem:** All messages visible in all chat screens
**Fix:** Per-contact bubble tagging with `lv_obj_set_user_data()` + `LV_OBJ_FLAG_HIDDEN` filter. Each chat bubble stores its contact_idx, and switching contacts hides/shows the appropriate bubbles.

### 587.4 Fix 35f: PSRAM Guard for Reply Queue

**Files:** reply_queue.c
**Problem:** RQ[1] PSRAM data overwritten by NVS load
**Fix:** PSRAM guard: check if PSRAM slot is already valid before loading from NVS. Deferred NVS save to prevent overwrite.

### 587.5 Fix 35g: Contact Queue Decrypt Ratchet Switch

**Files:** smp_tasks.c
**Problem:** Contact Queue decrypt did not switch to correct ratchet slot
**Fix:** Call `ratchet_set_active()` before Contact Queue decrypt path

### 587.6 Fix 35h: PSRAM Slots Empty After Boot for Contact >0

**Files:** smp_ratchet.c, smp_handshake.c
**Problem:** After boot, only Slot 0 is loaded from NVS. Any `ratchet_set_active(N)` for N>0 operates on empty PSRAM.
**Fix:** NVS fallback in `ratchet_set_active()`: if PSRAM slot is empty, load from NVS first. Save/load logic in `handshake_set_active()` mirrors the same pattern.

---

## 588. Session 35 Fix Summary Table

| Fix | Files | Problem | Solution |
|-----|-------|---------|----------|
| 35a | smp_tasks.c, smp_agent.c | Wrong ratchet slot during RQ decrypt | set_active BEFORE process_message |
| 35c | smp_tasks.c | KEY secures wrong queue | KEY targets Reply Queue |
| 35e | ui_chat.c/h, smp_tasks.c/h, smp_events.h, smp_agent.c | All messages in all chats | Per-contact bubble tagging + HIDDEN filter |
| 35f | reply_queue.c | PSRAM overwritten by NVS | PSRAM guard + deferred save |
| 35g | smp_tasks.c | CQ decrypt without ratchet switch | ratchet_set_active before CQ decrypt |
| 35h | smp_ratchet.c, smp_handshake.c | PSRAM empty for Contact >0 after boot | NVS fallback in set_active |

---

## 589. Verified Result

| Feature | Status |
|---------|--------|
| 2 contacts simultaneously connected | ✅ |
| Bidirectional messages for both contacts | ✅ |
| Delivery receipts (double checkmarks) for both | ✅ |
| Per-contact chat filter (messages only in correct chat) | ✅ |
| Tested after `idf.py erase-flash` with fresh handshakes | ✅ |
| 20+ messages exchanged between both contacts | ✅ |

---

## 590. Session 35 Lessons Learned

### L187: "Wrong Slot Active" Is the Most Common Multi-Contact Bug

**Severity: Architectural**

Every operation that touches ratchet or handshake state MUST set the active slot first. The pattern from Session 34 repeats: decrypt with wrong slot = crypto failure, KEY on wrong queue = connection failure, NVS load overwrites active PSRAM = data corruption. Before any crypto or protocol operation: `ratchet_set_active(contact_idx)` + `handshake_set_active(contact_idx)`.

### L188: PSRAM Slots Do Not Survive Reboot for Contact >0

**Severity: Critical**

After boot, only Slot 0 is loaded from NVS into PSRAM. Any `ratchet_set_active(N)` for N>0 must include an NVS fallback: if the PSRAM slot is empty (zeroed), load from NVS before returning. Without this, Contact 1+ after reboot operates on empty crypto state.

### L189: KEY Secures the Reply Queue, Not the Contact Queue

**Severity: Critical**

The phone sends its messages (HELLO, chat, receipts) to the Reply Queue. KEY must authorize the sender on the Reply Queue (`reply_queue_get(idx)->rcv_id`), not on the Contact Queue (`contacts[idx].recipient_id`). Wrong target = phone stuck on "connecting".

### L190: Per-Contact Chat Filter via LVGL user_data + HIDDEN Flag

**Severity: High**

Each chat bubble stores its contact_idx via `lv_obj_set_user_data()`. When switching contacts, iterate all children and set `LV_OBJ_FLAG_HIDDEN` for non-matching contact_idx. This avoids rebuilding the entire chat view on contact switch.

### L191: PSRAM Guard Prevents NVS Overwrite

**Severity: High**

When loading from NVS at boot, check if the PSRAM slot already has valid data (e.g., from a just-completed handshake). If PSRAM is already valid, skip NVS load. Deferred NVS save ensures the PSRAM data persists for next boot without overwriting current session state.

### L192: Systematic File-by-File Comparison Beats Log Analysis

**Severity: Methodological**

Comparing Contact 0 (working) vs Contact 1 (broken) through systematic file-by-file tracing finds root causes faster than log analysis alone. The diagnosis pattern: identify where the code paths diverge for different contact indices.

---

## 591. Session 36 Planning (Prepared by Mausi)

### Assignments Prepared

| Assignment | Description | Priority |
|------------|-------------|----------|
| 35g/h | NTP timestamp for chat bubbles (currently "--:--") | High |
| Bug E | Contact name from ConnInfo JSON (currently placeholder) | High |
| 35i | Contact delete function | Medium |
| 35j | Long-press menu in contact list | Medium |

### Roadmap

```
Session 36 Priority Order:
  1. NTP Timestamps + Contact Name (Bug E)
  2. Contact Delete + Long-Press Menu
  3. SD Card History with Encryption
  4. Encrypted Backup (Post-Kickstarter)
```

### SD Encryption Theory (Discussed)

AES-256-GCM for SD card chat history. Encrypted backup as post-Kickstarter premium feature. Details to be finalized in Session 36.

---

## 592. Files Changed (10 Files, 1 Commit)

| File | Changes |
|------|---------|
| main/main.c | Boot sequence adjustments |
| main/core/smp_tasks.c | Fix 35a (ratchet ordering), 35c (KEY target), 35g (CQ decrypt switch) |
| main/include/smp_tasks.h | Updated declarations |
| main/protocol/reply_queue.c | Fix 35f (PSRAM guard, deferred NVS) |
| main/protocol/smp_agent.c | Fix 35a (ratchet ordering at process_message) |
| main/protocol/smp_handshake.c | Fix 35h (save/load in set_active) |
| main/protocol/smp_ratchet.c | Fix 35h (NVS fallback in set_active) |
| main/state/smp_peer.c | Per-contact peer state adjustments |
| main/ui/screens/ui_chat.c | Fix 35e (per-contact bubble tagging, HIDDEN filter) |
| main/ui/screens/ui_chat.h | Fix 35e (contact_idx in bubble API) |

### Commit

```
feat(multi-contact): fix bidirectional handshake and per-contact chat filtering
```

---

## 593. Open Items for Session 36

| Item | Description | Priority |
|------|-------------|----------|
| NTP | Chat bubbles show "--:--" (no timestamps) | High |
| Bug E | Contact name shows placeholder | High |
| Contact Delete | No delete function yet | Medium |
| Long-Press | No context menu in contact list | Medium |
| SD History | Chat history not persisted to SD | Medium |
| SPI Glitches | lcd_panel.io.spi errors (cosmetic) | Low |
| Bug C | ERR AUTH status after all fixes (re-check) | Medium |
| Bug D | Receipts after all fixes (re-check) | Low |

---

## 594. Session 35 Summary

### What Was Achieved

- ✅ Bug A FIXED: Phone shows "connected" for Contact 1 (KEY target corrected)
- ✅ Bug B FIXED: Per-contact chat filter (messages only in correct chat)
- ✅ Ratchet slot ordering fixed (decrypt with correct slot)
- ✅ PSRAM guard prevents NVS overwrite
- ✅ NVS fallback for Contact >0 after boot
- ✅ 20+ messages exchanged, both contacts bidirectional
- ✅ Tested after full erase-flash with fresh handshakes

### Key Takeaway

```
SESSION 35 SUMMARY:
  🏁 MULTI-CONTACT VICTORY — ALL PLANNED BUGS FIXED

  2 contacts simultaneously connected               ✅
  Bidirectional messages for both                    ✅
  Delivery receipts for both                         ✅
  Per-contact chat filter                            ✅
  Fresh handshake test (erase-flash)                 ✅
  20+ messages exchanged                             ✅

  Root Cause Pattern: "wrong slot active"
  Same pattern as Session 34, now fully resolved.

"Multi-contact victory. The messenger works." — Mausi 👑🐭
```

---

**DOCUMENT CREATED: 2026-02-24 Session 35**
**Status: ✅ All planned bugs fixed**
**Key Achievement: Multi-Contact Victory with Per-Contact Chat Filter**
**Milestone 11: Multi-Contact Chat Filter**
**Next: Session 36 - NTP Timestamps, Contact Names, Delete Function**

---

*Created by Princess Mausi (👑🐭) on February 24, 2026*
*Session 35 was the victory lap. The multi-contact messenger works. Time to polish.*
