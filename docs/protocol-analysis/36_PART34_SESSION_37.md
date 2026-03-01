# SimpleX Protocol Analysis - Part 34: Session 37
# Encrypted Chat History: SD Card, SPI Bus Wars, Progressive Rendering

**Document Version:** v1
**Date:** 2026-02-25 to 2026-02-27 Session 37
**Status:** COMPLETED -- Encrypted SD chat history operational
**Previous:** Part 33 - Session 36 (Contact Lifecycle)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 37 SUMMARY

```
4 Sub-Sessions (37a, 37b, 37c, 37d)
3 Major Features Implemented
2 Bugs Fixed (#58, #59)
2 New Lessons (#203, #204)
11 Files Changed, 2 New Files Created

Feature 1: AES-256-GCM Encrypted Chat History on SD Card
Feature 2: SPI2 Bus Serialization (Display + SD collision fix)
Feature 3: Contact List Redesign (28px cards, search, bottom bar)

Prepared: German Umlaut Fallback Fonts (task created, not yet implemented)

MILESTONE 13: Encrypted Chat History
```

---

## Starting Point

Session 36 had completed the full contact lifecycle: invitation, QR scan, handshake, bidirectional encrypted communication with delivery receipts, contact delete. SimpleGo was a working messenger but with no persistent chat history.

Five priorities for Session 37:
1. Encrypted chat history on SD card
2. German umlauts (LVGL font)
3. Contact list enhancements
4. Server DEL command
5. First message invisible bug

---

## Phase 1: Session 37a -- Encrypted SD Chat History

### Architecture (Mausi Design)

Append-only chat history with AES-256-GCM per-record encryption. One file per contact at /sdcard/simplego/msgs/chat_XX.bin. Each message individually encrypted and appended -- no rewriting needed.

Key design decisions by Prinz:
- Delivery status (checkmarks) initially RAM-only to avoid GCM nonce reuse
- Later revised: persistent via unencrypted file header (last_delivered_idx)
- Receipts arrive in order, so a high-water-mark index is sufficient
- Contact delete also deletes the history file
- 20 messages loaded per batch

### Key Management Architecture

```
Master Key (256-bit random) -- stored in NVS
    |
    +-- HKDF-SHA256("simplego-chat", slot_index)
        |
        +-- Per-Contact Key (256-bit)

GCM Nonce Construction (deterministic, never reused):
    nonce[0..3]  = slot_index (uint32 LE)
    nonce[4..7]  = msg_index  (uint32 LE)
    nonce[8..11] = 0x00000000

Record Format:
    [4B record_len][12B nonce][16B GCM tag][encrypted payload]

File Header (unencrypted):
    [4B magic "SGH1"][4B version][4B msg_count][4B last_delivered_idx]
```

### New Module

```
main/include/smp_history.h  -- API declarations
main/state/smp_history.c    -- Full implementation

Key functions:
  smp_history_init()           -- Mount SD, create dirs, load/generate master key
  smp_history_append()         -- Encrypt + append record to contact file
  smp_history_load()           -- Load + decrypt N most recent messages
  smp_history_delete_contact() -- Remove contact's history file
  smp_history_get_count()      -- Message count from header
```

### Integration Points

- App Task: smp_history_append() called on send AND receive
- UI Chat: smp_history_load() called when opening chat screen
- Contact Delete: smp_history_delete_contact() in delete flow
- Boot: smp_history_init() after SD card mount

---

## Phase 2: Session 37b -- SPI Bus Collision (Bug #58)

### Symptoms

```
assert failed: spi_ll_get_running_cmd
Display tearing/smearing when scrolling
1.5s freeze when opening chat with history
```

### Root Cause

Display and SD card share the SAME SPI2 bus on T-Deck Plus hardware. Concurrent access from LVGL Task (display refresh) and App Task (SD read/write) caused SPI transaction collisions.

```
LVGL Task (Core 1):              App Task (Core 1):
  spi_device_transmit(display)      f_open(sd_file)
        |                               |
        +------ SPI2 BUS ------+--------+
                COLLISION!
```

### Fix: Three-Part Solution

**Fix 1: SPI2 Bus Serialization**
Recursive LVGL mutex wraps ALL SD card operations. Any SD access acquires the same mutex that LVGL uses for display transactions.

```c
// Before any SD operation:
lvgl_port_lock(0);    // Acquire recursive mutex
f_open(&file, path, FA_READ);
f_read(&file, buf, len, &br);
f_close(&file);
lvgl_port_unlock();   // Release mutex
```

**Fix 2: Anti-Tearing -- DMA Draw Buffer**
LVGL draw buffer 1 moved from PSRAM to internal DMA-capable SRAM (~12.8KB). PSRAM access during SPI DMA transfers caused tearing artifacts.

**Fix 3: Chunked Rendering**
Instead of creating all 20 LVGL bubble objects at once (1.5s freeze), render 3 bubbles per LVGL timer tick (50ms each). Total: ~350ms, display stays fluid.

```
OLD: load_history() → create 20 bubbles → 1.5s freeze
NEW: load_history() → queue 20 records → timer creates 3/tick → 350ms fluid
     "Loading..." indicator shown during progressive render
```

**Lesson #203:** SPI2 bus is shared between display AND SD card on T-Deck Plus. Every SD access needs the LVGL mutex. Not two separate buses, one single bus.

**Lesson #204:** Chunked rendering is mandatory for history loading. 20 LVGL objects created at once blocks display for 1.5s. 3 per tick (50ms) = 350ms total, display stays responsive.

---

## Phase 3: Session 37c -- German Umlauts (Design Only)

### Problem

Contact name "Sascha Daemgen" showed boxes instead of "ae". LVGL's built-in Montserrat fonts only contain Basic Latin (U+0020-U+007F). German umlauts (U+00C4 Ae, U+00D6 Oe, U+00DC Ue, U+00E4 ae, U+00F6 oe, U+00FC ue, U+00DF ss) are in Latin-1 Supplement range.

### Solution: LVGL Fallback Font Mechanism

Two approaches evaluated:
1. Complete custom font generation (8 files changed) -- rejected
2. LVGL fallback font mechanism (5 files, ~800 bytes flash) -- chosen

Tiny font with only 7 German special characters, attached as fallback to existing built-in fonts:

```c
// At init, cast away const and attach fallback:
((lv_font_t *)&lv_font_montserrat_14)->fallback = &simplego_umlauts_14;
((lv_font_t *)&lv_font_montserrat_10)->fallback = &simplego_umlauts_10;
```

When LVGL encounters a glyph not in Montserrat, it automatically tries the fallback font. Minimal flash usage, no font rebuild needed.

**Status:** Task created for Hasi, implementation in Session 37d or 38.

---

## Phase 4: Session 37d -- Contact List Redesign

### Problems Identified

- Cards too tall (44px, two lines with useless "X3DH + Double Ratchet" text)
- Bottom bar: labels instead of real buttons, tiny touch targets
- "New" button green instead of cyan (brand color)
- No search, no unread badge, no message count

### Redesign Specification (Mausi)

```
CARD_H: 44px → 28px (single line)
Visible contacts: 3 → 5-6
Second line: removed (info only in long-press popup)
Bottom bar: 3 real lv_btn objects (100x36px touch targets)
Search: overlay with text field + filtered contact list
Colors: all green → cyan
Unread badge: "(3)" right-aligned next to status
```

### Implementation

Contact list completely rewritten following Mausi specification. Single-line cards with contact name and status indicator. Bottom bar with proper button objects and adequate touch targets.

---

## Bug List (Session 37)

| Bug | Description | Root Cause | Fix | Phase |
|-----|-------------|------------|-----|-------|
| #58 | SPI crash + display tearing on SD access | Display + SD share SPI2 bus, no serialization | LVGL mutex for all SD ops + DMA buffer to internal SRAM | 37b |
| #59 | 1.5s display freeze on chat open with history | 20 LVGL objects created synchronously | Chunked rendering: 3 per tick with Loading indicator | 37b |

---

## Additional Fix

Ring buffer NULL-guard added for subscribe_all_contacts race condition. During startup, subscribe loop could fire before ring buffer initialization completed.

---

## Files Changed -- Session 37

| File | Path | Changes |
|------|------|---------|
| tdeck_lvgl.c | main/ | DMA draw buffer moved to internal SRAM |
| main.c | main/ | History init, chunked render timer |
| smp_tasks.c | main/core/ | History append on send/receive, ring buffer guard |
| smp_tasks.h | main/include/ | History API declarations |
| smp_history.h | main/include/ | NEW: Chat history module header |
| smp_agent.c | main/protocol/ | History integration for incoming messages |
| smp_contacts.c | main/state/ | History delete on contact delete |
| smp_history.c | main/state/ | NEW: AES-256-GCM encrypted history implementation |
| ui_chat.c | main/ui/screens/ | Chunked history loading, Loading indicator |
| ui_chat.h | main/ui/screens/ | Progressive render API |
| ui_contacts.c | main/ui/screens/ | Complete redesign |

---

## Git Commits (Session 37)

```
feat(history): add AES-256-GCM encrypted chat history on SD card

- Per-contact encryption via HKDF-SHA256 derived keys from master key
- Append-only file format with persistent delivery status in header
- Chunked loading (3 msgs per tick) for smooth progressive rendering
- SPI2 bus serialization via recursive LVGL mutex (display + SD share bus)
- DMA draw buffer moved to internal SRAM for anti-tearing
- Loading indicator on chat open
- History deleted on contact delete
- Ring buffer NULL-guard fix for subscribe_all_contacts race

feat(ui): redesign contacts list with single-line cards and search
```

---

## Known Bugs (End of Session 37)

| Bug | Description | Priority | Notes |
|-----|-------------|----------|-------|
| - | Display freeze (image freezes, loop continues) | P2 | Observed twice, always in chat with most messages |
| - | German umlauts (boxes instead of umlauts) | P2 | Hasi task created, not yet implemented |
| - | Server DEL on contact delete | P3 | Not started |
| - | First message invisible on fresh contact | P4 | Not started |
| - | SPI display glitches (rare) | P4 | Ongoing observation |

---

## Security Architecture: Chat History Encryption

```
Threat Model:
  - SD card physically extracted → encrypted records, no plaintext
  - Master key in NVS → protected by NVS encryption (TODO: nvs_flash_secure_init)
  - Per-contact keys via HKDF → compromising one contact doesn't expose others
  - GCM nonce deterministic → no random number generator dependency
  - Nonce never reused → slot_index + msg_index is unique per record

Encryption Stack:
  Layer 1: AES-256-GCM per record (confidentiality + authenticity)
  Layer 2: HKDF key isolation per contact (compartmentalization)
  Layer 3: Master key in NVS (TODO: encrypted NVS for production)

What is NOT encrypted:
  - File header (msg_count, last_delivered_idx) — metadata only
  - File names (chat_XX.bin) — contact index visible
  - File sizes — message count estimatable from file size

Production TODO:
  - NVS encryption (nvs_flash_secure_init + eFuse)
  - Randomized file names (hide contact index)
  - Fixed-size padding (hide message lengths)
```

---

## Lessons Learned (Session 37)

| # | Lesson | Context |
|---|--------|---------|
| 203 | SPI2 bus is shared between display AND SD card on T-Deck Plus. Every SD access needs the LVGL mutex. Not two separate buses — one single bus. | SPI crash on SD history read |
| 204 | Chunked rendering is mandatory for history loading. 20 LVGL objects at once blocks display for 1.5s. 3 per tick (50ms) = 350ms total, display stays responsive. | Chat open freeze |

---

## Session 37 Statistics

| Metric | Value |
|--------|-------|
| Sub-sessions | 4 (37a, 37b, 37c, 37d) |
| Git commits | 2 |
| Bugs fixed | 2 (#58, #59) |
| New lessons | 2 (#203, #204) |
| New files | 2 (smp_history.h, smp_history.c) |
| Files changed | 11 |
| Encryption | AES-256-GCM with HKDF-SHA256 key derivation |
| Master key | 256-bit random, NVS persistent |
| History format | Append-only, per-contact, chunked loading |

---

*Part 34 - Session 37 Encrypted Chat History*
*SimpleGo Protocol Analysis*
*Date: February 25-27, 2026*
*Bugs: 59 total (all FIXED)*
*Lessons: 204 total*
*Milestone 13: Encrypted Chat History*
