# Part 29: Session 32 — "The Demonstration" (From Protocol to Messenger)
**Date:** 2026-02-19/20 | **Version:** v0.1.17-alpha

## Overview

Transformed SimpleGo from a protocol layer visible only in Serial Monitor to a functioning messenger application with visual UI on the T-Deck display. The session bridged three gaps: Keyboard→LVGL→Protocol (sending via hardware keyboard), Protocol→UI (received messages displayed on screen), and auto-navigation to Chat screen after connection. Additionally delivered a delivery status system, multi-contact architecture analysis, navigation stack fix, and 128-contact PSRAM planning. System ran 2+ hours stable without crash.

## Keyboard-to-Chat Integration (7 Steps)

**Architecture:**
```
LVGL Thread (Core 1)                     Main Task (Core 0)

T-Deck HW → LVGL kbd_indev → Textarea
                                | Enter
                          on_input_ready()
                           |          |
             show bubble   |          | send_cb()
             immediately   |          |
                 v          v
         ui_chat_add    kbd_msg_queue → smp_app_run()
         _message()     (FreeRTOS Q)      |
         (outgoing)                       v
                                    peer_send_chat_message()

LVGL Timer (50ms) ← app_to_ui_queue ← smp_notify_ui_message()
     |               (FreeRTOS Q)      (called after decrypt)
     v
ui_chat_add_message(text, false)
```

**Step 1: LVGL Keyboard InDev.** T-Deck I2C Keyboard (0x55) registered via `tdeck_keyboard_register_lvgl()` as LVGL Input Device. Replaces raw keyboard_task; LVGL handles polling in its own tick. (main.c)

**Step 2: Chat Send Callback.** `ui_chat_set_send_callback(chat_send_cb)` connects LVGL with Protocol. Enter key → on_input_ready() → send_cb() → kbd_msg_queue. FreeRTOS Queue (4 items, 256 bytes) as thread-safe bridge. (main.c, ui_chat.c)

**Step 3: UI Event Queue.** New `app_to_ui_queue` (8 events) with 4 event types: MESSAGE (display received text), NAVIGATE (switch screen), SET_CONTACT (set chat header name), DELIVERY_STATUS (update checkmarks). Push functions: smp_notify_ui_message(), smp_notify_ui_navigate(), smp_notify_ui_contact(), smp_notify_ui_delivery_status(). (smp_tasks.h, smp_tasks.c)

**Step 4: Received Messages on Display.** Single line added in `extract_chat_text()` after Ratchet decrypt: `smp_notify_ui_message(text_start, false, 0)`. LVGL Timer (50ms) polls queue, calls ui_chat_add_message(). Cyan bubbles on left for incoming messages. (smp_agent.c, 1 line)

**Step 5: Delivery Status System.** Professional tracking: "..." (Sending) → "✓" (Server OK) → "✓✓" (Delivered) → "✗" (Failed). 16-slot tracking table maps seq → status_label (LVGL pointer). `ui_chat_next_seq()` assigns monotonically increasing sequence numbers. `ui_chat_update_status(seq, status)` updates the checkmark. Colors: dim for ".../✓", green for "✓✓", red for "✗". (ui_chat.h, ui_chat.c, smp_tasks.h, smp_tasks.c)

**Step 6: LVGL Display Refresh Fix.** Bug: received bubbles invisible until screen switch. Root cause: LVGL does not auto-invalidate flex containers in timer callbacks. Fix: `lv_obj_update_layout()` + `lv_obj_invalidate()` after `lv_obj_create()` in timer callback. (ui_chat.c, 2 lines)

**Step 7: Receipt Parsing for Double Checkmarks.** New handler for inner_tag 'V' (Delivery Receipt) in smp_agent.c. Receipt wire format (reverse-engineered): `'V' + count(1B Word8) + [msg_id(8B Big Endian) + hash_len(1B) + hash(NB)]...`. Mapping system: smp_register_msg_mapping(seq, msg_id) after send, smp_notify_receipt_received(msg_id) searches seq via mapping. handshake_get_last_msg_id() as clean encapsulation (replaces extern variable). (smp_agent.c, smp_tasks.h, smp_tasks.c, smp_handshake.h, smp_handshake.c)

## UI Design Handover

Created `ui_design_handover.md` for a separate Claude chat to redesign the Chat Screen with cyberpunk aesthetics ("Kickstarter-Qualität"). Contents: display specs (320x240, ST7789, GT911 Touch, I2C Keyboard), layout grid (Header 20px + Messages 152px + Input 34px + Nav 32px), color palette (Cyan 0x00E5FF, Magenta 0xFF00FF, BG 0x000A0F), font constraints (ONLY montserrat_14/16, Font 12 DISABLED), bubble architecture with delivery status, PUBLIC API (6 functions, immutable), degrees of freedom and restrictions. Design Chat works only on ui_chat.c/ui_theme.c (visuals); protocol work touches only smp_tasks.c/smp_agent.c (logic). No collision possible.

## Multi-Contact Architecture Analysis

**Finding 1: Receive path already multi-contact capable.** `find_contact_by_recipient_id(entity_id, entLen)` in smp_tasks.c already routes incoming messages correctly. Also subscribe_all_contacts() and decrypt_smp_message(contact, ...) already work with the correct contact_t.

**Finding 2: Send path hardcoded to contacts[0].** 7 locations in smp_tasks.c reference contacts_db.contacts[0]. Lines 441 (wildcard ACK) and 462 (keyboard send) need active contact routing. Lines 581/613/661/676 (42d handshake) stay on contacts[0] because 42d always runs with the first scanned contact.

**Solution:** Active Contact Index via `smp_set_active_contact(int idx)` / `smp_get_active_contact()`. UI flow: tap contact → smp_set_active_contact(idx) → ui_chat_set_contact(name).

## Navigation Stack Bug

**Symptom:** Chat→Back→Contacts OK. Contacts→Back→Chat instead of Main. Endless ping-pong between Chat and Contacts. **Root cause:** `ui_manager.c` uses ONE variable `prev_screen`. After navigation to Chat, prev_screen=Chat, so Back from Contacts goes to Chat instead of Main. **Fix:** Replace with 8-deep navigation stack. show_screen() pushes current, go_back() pops. Splash never pushed, no duplicates, stack overflow handled with shift-left.

## 128-Contact PSRAM Planning

Cannatoshi: "Eine Kontaktliste mit mindestens 100 Usern, sonst brauchen wir gar nicht auf Kickstarter antanzen."

Current: Global singleton `ratchet_state_t`, ~530 bytes per state. Cannatoshi's PSRAM array idea: `ratchet_state_t ratchets[128]` directly in PSRAM. 128 x ~530 bytes = ~68KB (0.8% of 8MB). No swap, no save/load dance on contact switch, NVS only at boot and after each message, zero latency on contact switch.

Five problems identified: MAX_CONTACTS=10 with NVS blob too small, ratchet_save_state hardcoded to index 0, ratchet swap on message from different contact, 42d handshake hardcoded to contacts[0], ratchet index range only 0-31.

Performance: NVS load (530B) 1-3ms, NVS save (530B) 5-20ms, contact lookup (128x memcmp) <0.1ms, LVGL list 128 entries 50ms one-time, TLS send ~200ms. 128 contacts run smoothly on ESP32-S3.

## Files Changed

Implemented and tested: main.c (keyboard_task disabled, LVGL kbd_indev, chat_send_cb, ui_poll_timer_cb), ui_chat.h/c (delivery status tracking 16 slots, bubble with status label, LVGL refresh fix), smp_tasks.h/c (UI event types, delivery status enum, notify prototypes, mapping table, receipt matching), smp_agent.c (receipt handler for 'V', msg_id parsing 8B BE), smp_handshake.h/c (handshake_get_last_msg_id()).

Commits: `feat(ui): integrate LVGL keyboard with chat send/receive`, `feat(core): add UI event bridge types and delivery status enum`, `feat(ui): add delivery status tracking to chat bubbles`, `fix(ui): force LVGL layout update on incoming messages`, `feat(protocol): parse delivery receipts for double-check status`.

## Lessons Learned

**L162 (HIGH):** FreeRTOS Queue as thread-safe UI bridge. LVGL functions may only be called from the LVGL thread. Queue + Timer pattern (50ms poll) is the clean cross-task solution. Never call lv_obj_create() from the protocol task directly.

**L163 (HIGH):** LVGL does not auto-invalidate flex containers in timer callbacks. Must manually call lv_obj_update_layout() + lv_obj_invalidate() after lv_obj_create(). Without this, new elements are invisible until next screen switch.

**L164:** Receipt wire format: 'V' + count(Word8) + [msg_id(8B Big Endian) + hash_len(1B) + hash(NB)].

**L165:** Encapsulate msg_id access via function (handshake_get_last_msg_id()), not extern variable. Prevents accidental modification.

**L166 (HIGH):** Receive path is already multi-contact capable. Only the send path (7 locations referencing contacts[0]) needs modification.

**L167:** Navigation stack instead of single prev_screen variable. 8-deep stack prevents infinite ping-pong in three-level navigation.

**L168 (HIGH):** PSRAM ratchet array eliminates swap latency. ratchet_state_t ratchets[128] uses ~68KB (0.8% of 8MB). NVS only at boot (load all) and after each message (save one).
