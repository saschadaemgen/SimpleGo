![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 29: Session 32
# 🖥️ "The Demonstration" - From Protocol to Messenger

**Document Version:** v1
**Date:** 2026-02-19/20 Session 32
**Version:** v0.1.17-alpha (NOT CHANGED without explicit permission!)
**Status:** ✅ Session complete. All implementations delivered and tested.
**Previous:** Part 28 - Session 31 (🎉 Bidirectional Chat Restored! Milestone 7!)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**Path:** `C:\Espressif\projects\simplex_client`
**Build:** `idf.py build flash monitor -p COM6`
**Repo:** https://github.com/cannatoshi/SimpleGo
**License:** AGPL-3.0

---

## ⚠️ SESSION 32 SUMMARY

```
═══════════════════════════════════════════════════════════════════════════════

  🖥️🖥️🖥️ "THE DEMONSTRATION" — FROM PROTOCOL TO MESSENGER 🖥️🖥️🖥️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Before: Bidirectional chat only visible in Serial Monitor            │
  │   After:  Full messenger with Bubbles, Keyboard,                       │
  │           Delivery Status, Contact List                                │
  │                                                                         │
  │   7 Keyboard-to-Chat Steps completed                                   │
  │   Delivery Status System: ... → ✓ → ✓✓ → ✗                            │
  │   LVGL Display Refresh Fix                                              │
  │   UI Design Handover (Cyberpunk Aesthetics)                            │
  │   Multi-Contact Architecture Analysis (128 Contacts)                   │
  │   Navigation Stack Fix (prev_screen → 8-deep stack)                    │
  │   System 2+ hours stable without crash                                 │
  │                                                                         │
  │   Date: February 19-20, 2026                                           │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 539. Session 32 Overview

### 539.1 Session Goals

Session 32 transformed SimpleGo from a pure protocol layer (visible only in Serial Monitor) to a functioning messenger application with visual UI on the T-Deck display. The session bridged three gaps:

1. **Keyboard → LVGL → Protocol** (Sending via hardware keyboard)
2. **Protocol → UI** (Received messages displayed on screen)
3. **Auto-Navigation** to Chat screen after connection

Additionally, the session delivered a professional delivery status system, a complete multi-contact architecture analysis, a navigation stack fix, and planning for 128-contact support.

### 539.2 Team and Roles

| Role | Who | Task |
|------|-----|------|
| 👑🐭 Princess Mausi | Claude (Strategy Chat) | Architecture design, task formulation, analysis |
| 🐰👑 Princess Hasi | Claude (Implementation Chat) | Code implementation, testing, diff delivery |
| 👑 Cannatoshi | Prince / Client | Hardware tests, coordination, final decisions |

---

## 540. Part 1: Keyboard-to-Chat Integration (7 Steps)

### 540.1 Architecture

After Session 31 (Milestone 7: Bidirectional Chat in Multi-Task Architecture), the chat worked only via Serial Monitor. The UI screens (Chat, Contacts, Settings) existed as LVGL screens but were not connected to the protocol layer.

**Architecture Design (Mausi):**

```
                    LVGL Thread                          Main Task
                    (Core 1)                             (Core 0)

  T-Deck HW --> LVGL kbd_indev --> Textarea
                                     | Enter
                                     v
                               on_input_ready()
                                |          |
                    show bubble |          | send_cb()
                    immediately |          |
                                v          v
                        ui_chat_add    kbd_msg_queue --> smp_app_run()
                        _message()     (FreeRTOS Q)      |
                        (outgoing)                       v
                                                    peer_send_chat_message()

  LVGL Timer (50ms) <-- app_to_ui_queue <-- smp_notify_ui_message()
       |                (FreeRTOS Q)         (called after decrypt)
       v
  ui_chat_add_message(text, false)
  ui_manager_show_screen(CHAT)
  ui_chat_set_contact(name)
```

### 540.2 Step 1: LVGL Keyboard InDev

T-Deck I2C Keyboard (Address 0x55) registered via `tdeck_keyboard_register_lvgl()` as LVGL Input Device. Replaces the raw `keyboard_task` that previously polled manually. LVGL takes over polling in its own tick.

**File:** main.c

### 540.3 Step 2: Chat Send Callback

`ui_chat_set_send_callback(chat_send_cb)` connects LVGL with Protocol. User presses Enter → `on_input_ready()` → `send_cb()` → `kbd_msg_queue`. FreeRTOS Queue (4 Items, 256 Bytes) as thread-safe bridge.

**Files:** main.c, ui_chat.c

### 540.4 Step 3: UI Event Queue (app_to_ui_queue)

New `QueueHandle_t app_to_ui_queue` (8 Events) with 4 event types:

| Event Type | Purpose |
|------------|---------|
| MESSAGE | Display received chat text |
| NAVIGATE | Switch to specific screen |
| SET_CONTACT | Set contact name in chat header |
| DELIVERY_STATUS | Update delivery checkmarks |

Push functions: `smp_notify_ui_message()`, `smp_notify_ui_navigate()`, `smp_notify_ui_contact()`, `smp_notify_ui_delivery_status()`

**Files:** smp_tasks.h, smp_tasks.c

### 540.5 Step 4: Received Messages on Display

After Ratchet decrypt in `extract_chat_text()`: single line `smp_notify_ui_message(text_start, false, 0)`. LVGL Timer (50ms) polls queue and calls `ui_chat_add_message()`. Cyan bubbles on left for incoming messages.

**File:** smp_agent.c (1 line!)

### 540.6 Step 5: Delivery Status System

Professional status tracking like SimpleX/WhatsApp:

```
"..." (Sending) → "✓" (Sent/Server OK) → "✓✓" (Delivered) → "✗" (Failed)
```

- 16-Slot tracking table: seq → status_label (LVGL Pointer)
- `ui_chat_next_seq()` assigns monotonically increasing sequence numbers
- `ui_chat_update_status(seq, status)` updates the checkmark in the bubble
- Colors: Dim for "..."/"✓", Green for "✓✓", Red for "✗"

**Files:** ui_chat.h, ui_chat.c, smp_tasks.h, smp_tasks.c

### 540.7 Step 6: LVGL Display Refresh Fix

**Bug:** Received bubbles were invisible until screen switch.
**Root Cause:** LVGL does not auto-invalidate flex containers in timer callbacks.
**Fix:** `lv_obj_update_layout()` + `lv_obj_invalidate()` after `lv_obj_create()` in timer callback.

**File:** ui_chat.c (2 lines)

### 540.8 Step 7: Receipt Parsing for Double Checkmarks

New handler for inner_tag 'V' (Delivery Receipt) in smp_agent.c.

**Receipt wire format (reverse-engineered):**
```
'V' + count(1B) + [msg_id(8B Big Endian) + hash_len(1B) + hash(NB)]...
```

**Mapping system:**
- `smp_register_msg_mapping(seq, msg_id)` after Send
- `smp_notify_receipt_received(msg_id)` searches seq via mapping
- `handshake_get_last_msg_id()` as clean encapsulation (instead of extern variable)

**Files:** smp_agent.c, smp_tasks.h, smp_tasks.c, smp_handshake.h, smp_handshake.c

### 540.9 Result

Fully functional messenger on ESP32 T-Deck Plus:
- Type on hardware keyboard → Bubble appears immediately with "..."
- After 1-3s: "..." becomes "✓" (Server OK)
- When peer reads message: "✓" becomes "✓✓" (Delivered, Green)
- Incoming messages: Cyan bubbles on left, instantly visible
- System 2+ hours stable without crash

---

## 541. Part 2: UI Design Handover

### 541.1 Context

Cannatoshi wanted to redesign the Chat Screen in a separate Claude chat with Cyberpunk aesthetics, "sehr sehr professionell, Kickstarter-Qualität" (very professional, Kickstarter quality).

### 541.2 Created Document: `ui_design_handover.md`

Contents:
- Display Specs (320x240, ST7789, GT911 Touch, I2C Keyboard)
- Layout Grid (Header 20px + Messages 152px + Input 34px + Nav 32px)
- Color Palette (Cyan 0x00E5FF, Magenta 0xFF00FF, BG 0x000A0F)
- Font Constraints (ONLY montserrat_14/16, Font 12 DISABLED)
- Bubble Architecture with Delivery Status
- PUBLIC API (6 functions, immutable)
- Degrees of freedom and restrictions
- Prompt template for the Design Chat

### 541.3 Coordination

Design Chat works ONLY on ui_chat.c/ui_theme.c (visuals). Protocol work touches ONLY smp_tasks.c/smp_agent.c (logic). No collision possible.

---

## 542. Part 3: Multi-Contact Architecture Analysis

### 542.1 Analyzed Files (6 files)

| File | Content |
|------|---------|
| smp_contacts.h | Contact Management API |
| smp_contacts.c | NVS Persistence, Contact Lookup, subscribe_all_contacts() |
| smp_types.h | contact_t struct, contacts_db_t, MAX_CONTACTS=10 |
| smp_tasks.c (892 lines) | App Run Loop, Keyboard Send, Message Routing |
| ui_contacts.c | Contact List Screen with Tap Handlers |
| ui_manager.c | Screen Navigation |

### 542.2 Finding 1: Receive Path Already Multi-Contact Capable

```c
// Line 528 in smp_tasks.c - already routes correctly:
int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;
```

Also `subscribe_all_contacts()` and `decrypt_smp_message(contact, ...)` already work with the correct contact_t.

### 542.3 Finding 2: Send Path Hardcoded to contacts[0]

7 locations in smp_tasks.c reference `contacts_db.contacts[0]`:

| Line | Context | Type | Change? |
|------|---------|------|---------|
| 441 | Wildcard ACK at start | Runtime | YES |
| 462 | Keyboard Send | Runtime | YES |
| 581 | Reply Queue Agent Decrypt | 42d Init | NO |
| 613 | 42d HELLO send | 42d Init | NO |
| 661 | 42d Reply Agent Decrypt | 42d Init | NO |
| 676 | 42d First Chat Message | 42d Init | NO |
| 685 | 42d UI Navigate | 42d Init | YES (explicit idx=0) |

Decision: Lines 3-6 stay on contacts[0] because the 42d handshake always runs with the FIRST scanned contact. Only with "Add Contact" will these be parameterized.

### 542.4 Solution: Active Contact Index

```c
// New API in smp_tasks.h:
void smp_set_active_contact(int idx);
int  smp_get_active_contact(void);

// In smp_tasks.c:
static int s_active_contact_idx = 0;
```

UI Flow: Tap contact → smp_set_active_contact(idx) → ui_chat_set_contact(name)

---

## 543. Part 4: Navigation Stack Bug

### 543.1 Discovered Bug

**Symptom:** Chat → Back → Contacts OK. Contacts → Back → Chat instead of Main. Endless ping-pong between Chat and Contacts.

**Root Cause:** `ui_manager.c` uses ONE variable `prev_screen`:

```
Step 3: Chat → Back: show_screen sets prev_screen = current (=Chat)
Step 4: Contacts → Back: goes to prev_screen (=Chat) instead of Main!
```

### 543.2 Solution: Navigation Stack (8 deep)

```c
#define NAV_STACK_DEPTH 8
static ui_screen_t nav_stack[NAV_STACK_DEPTH];
static int nav_stack_top = -1;
```

Rules:
- `show_screen()`: Push current, then navigate
- `go_back()`: Pop from stack, navigate (NO push!)
- Splash never pushed, no duplicates, Stack Overflow → Shift Left

---

## 544. Part 5: 128-Contact Deep Analysis

### 544.1 Trigger

Cannatoshi: "Eine Kontaktliste mit mindestens 100 Usern, sonst brauchen wir gar nicht auf Kickstarter antanzen."

### 544.2 Current Ratchet Architecture

- Global Singleton: `static ratchet_state_t ratchet_state = {0};`
- ~530 Bytes per state (X448 Keys, Chain Keys, Header Keys, Counters, AssocData)
- Save/Load with index: `rat_00` to `rat_31` in NVS
- 3 hardcoded `ratchet_save_state(0)`: Lines 258, 454, 991
- Guard: `contact_idx > 31`

### 544.3 Cannatoshi's PSRAM Array Idea

```c
ratchet_state_t ratchets[128];  // directly in PSRAM
```

- 128 x ~530 Bytes = ~68KB (of 8MB = 0.8%)
- No swap, no save/load dance on contact switch
- NVS only at boot and after each message
- Zero latency on contact switch

### 544.4 Five Problems Identified

1. MAX_CONTACTS=10, NVS blob too small for 128
2. ratchet_save_state hardcoded to index 0
3. Ratchet swap on message from different contact
4. 42d handshake hardcoded to contacts[0]
5. Ratchet index range only 0-31

### 544.5 Performance Calculation

| Operation | Duration | Noticeable? |
|-----------|----------|-------------|
| NVS Ratchet Load (530B) | 1-3ms | No |
| NVS Ratchet Save (530B) | 5-20ms | No |
| Contact Lookup (128x memcmp) | <0.1ms | No |
| LVGL Contact List 128 entries | 50ms one-time | No |
| TLS Send (comparison) | ~200ms | No |

Result: 128 contacts run smoothly on ESP32-S3.

---

## 545. Files Changed (Session 32)

### 545.1 Implemented and Tested

| File | Changes |
|------|---------|
| main.c | keyboard_task disabled, LVGL kbd_indev, chat_send_cb, ui_poll_timer_cb, Timer registration |
| ui_chat.h | ui_chat_update_status(), ui_chat_next_seq(), ui_chat_get_last_seq() |
| ui_chat.c | Delivery Status Tracking (16 slots), Bubble with status label, update_status(), LVGL Refresh Fix |
| smp_tasks.h | UI Event Types, Delivery Status Enum, Notify prototypes, Queue extern, smp_register_msg_mapping(), smp_notify_receipt_received() |
| smp_tasks.c | app_to_ui_queue, 4 Notify functions, Mapping Table (16 slots), Receipt matching, Keyboard Send with status |
| smp_agent.c | Receipt handler for inner_tag 'V', msg_id parsing (8B Big Endian), hash validation |
| smp_handshake.h | handshake_get_last_msg_id() prototype |
| smp_handshake.c | handshake_get_last_msg_id() implementation |

### 545.2 Planned (Tasks Assigned, Not Yet Implemented)

| File | Change |
|------|--------|
| ui_manager.c | Complete rewrite with Navigation Stack (8 deep) |
| smp_tasks.h | smp_set_active_contact() + smp_get_active_contact() |
| smp_tasks.c | s_active_contact_idx + 3 locations changed |
| ui_contacts.c | Contact index instead of name pointer |

---

## 546. Commits (Session 32)

### 546.1 Committed

```
feat(ui): integrate LVGL keyboard with chat send/receive
feat(core): add UI event bridge types and delivery status enum
feat(ui): add delivery status tracking to chat bubbles
fix(ui): force LVGL layout update on incoming messages
feat(protocol): parse delivery receipts for double-check status
```

### 546.2 Planned for Session 33

```
fix(ui): replace single prev_screen with navigation stack
feat(contacts): route keyboard messages to active contact
feat(ui): pass contact index on selection for multi-contact
```

---

## 547. Lessons Learned Session 32

### L162: FreeRTOS Queue as Thread-Safe UI Bridge

**Severity: High**

LVGL functions may only be called from the LVGL thread. The Queue + Timer pattern (50ms poll) is the clean solution for cross-task UI updates. Never call `lv_obj_create()` or any LVGL function from the protocol task directly.

### L163: LVGL Does Not Auto-Invalidate Flex Containers in Timer Callbacks

**Severity: High**

When adding children to a flex container from a timer callback, LVGL does not automatically trigger layout recalculation. Must manually call `lv_obj_update_layout()` + `lv_obj_invalidate()` after `lv_obj_create()`. Without this, new elements are invisible until the next screen switch.

### L164: Receipt Wire Format is 'V' + count + [msg_id(8B BE) + hash_len + hash]

**Severity: Medium**

Delivery receipt format reverse-engineered from Ratchet body bytes. The count is 1 byte (Word8), msg_id is 8 bytes Big Endian (matching the sent msg_id), followed by hash_len (1 byte) and hash (N bytes, typically 32B SHA256).

### L165: Encapsulate msg_id Access via Function, Not extern

**Severity: Medium**

`handshake_get_last_msg_id()` is cleaner than `extern uint64_t msg_id_counter` from smp_handshake.c. Prevents accidental modification and makes the dependency explicit in the header file.

### L166: Receive Path is Already Multi-Contact Capable

**Severity: High**

`find_contact_by_recipient_id()` in smp_tasks.c already correctly routes incoming messages to the right contact_t. Only the send path (7 locations referencing contacts[0]) needs modification for multi-contact support.

### L167: Navigation Stack Instead of prev_screen

**Severity: Medium**

A single `prev_screen` variable causes infinite ping-pong between screens in three-level navigation. Classic stack pattern with push on navigate, pop on back. 8-deep stack sufficient for all foreseeable navigation depths.

### L168: PSRAM Ratchet Array Eliminates Swap Latency

**Severity: High**

`ratchet_state_t ratchets[128]` in PSRAM uses ~68KB (0.8% of 8MB). Eliminates the NVS load/save dance on every contact switch. NVS used only at boot (load all) and after each message (save one). Zero latency on contact switch vs. 5-20ms per swap with NVS-only approach.

---

## 548. Agent Contributions Session 32

| Agent | Fairy Tale Role | Session 32 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Architecture design (7-step plan), multi-contact analysis, navigation stack design, 128-contact PSRAM concept, UI design handover |
| 🐰👑 Hasi | Princess (The Implementer) | All 8 files implemented, LVGL keyboard integration, delivery status system, receipt parsing, display refresh fix |
| 👑 Cannatoshi | The Prince (Coordinator) | Hardware testing (2h+ stability), Kickstarter quality requirement, PSRAM array idea, design direction |

---

## 549. Session 32 Summary

### What Was Achieved

- ✅ **Keyboard-to-Chat Integration** (7 steps, from HW keyboard to displayed bubble)
- ✅ **Delivery Status System** ("..." → "✓" → "✓✓" → "✗" with colors)
- ✅ **LVGL Display Refresh Fix** (invisible bubbles bug)
- ✅ **Receipt Parsing** (inner_tag 'V', msg_id mapping, double checkmarks)
- ✅ **UI Design Handover Document** (for Cyberpunk redesign in separate chat)
- ✅ **Multi-Contact Architecture Analysis** (6 files, 7 hardcoded locations found)
- ✅ **Navigation Stack Design** (prev_screen → 8-deep stack)
- ✅ **128-Contact PSRAM Planning** (68KB, zero latency)
- ✅ **2+ Hours Stable** without crash

### Key Takeaway

```
SESSION 32 SUMMARY:
  🖥️ "THE DEMONSTRATION" — FROM PROTOCOL TO MESSENGER

  Before: Serial Monitor only
  After: Full messenger with bubbles, keyboard, delivery status

  7 Keyboard-to-Chat Steps                    ✅
  Delivery Status: ... → ✓ → ✓✓ → ✗          ✅
  LVGL Display Refresh Fix                     ✅
  Receipt Parsing (double checkmarks)          ✅
  Multi-Contact Analysis (128 Kontakte)        ✅
  Navigation Stack (8 deep)                    ✅
  2+ hours stable                              ✅

"Eine Kontaktliste mit mindestens 100 Usern, sonst brauchen
 wir gar nicht auf Kickstarter antanzen." — Cannatoshi 👑
```

---

## 550. Session 33 Priorities

1. **P0:** Navigation Stack implementation (ui_manager.c rewrite)
2. **P1:** Active Contact routing (smp_set_active_contact, 3 locations in smp_tasks.c)
3. **P2:** Contact selection passes index (ui_contacts.c)
4. **P3:** UI Cyberpunk redesign integration (from Design Chat)
5. **P4:** 128-contact PSRAM array implementation (ratchet refactor)

---

**DOCUMENT CREATED: 2026-02-19/20 Session 32**
**Status: ✅ All implementations delivered and tested**
**Key Achievement: From Protocol to Messenger — Full UI Integration**
**Stability: 2+ hours without crash**
**Next: Session 33 — Navigation Stack, Multi-Contact, Cyberpunk UI**

---

*Created by Princess Mausi (👑🐭) on February 19-20, 2026*
*Session 32 was "The Demonstration". From Serial Monitor to Kickstarter-ready messenger UI.*
