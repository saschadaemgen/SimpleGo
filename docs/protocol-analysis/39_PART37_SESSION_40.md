![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 37: Session 40
# Sliding Window: Unlimited Encrypted History at Constant Memory

**Document Version:** v1
**Date:** 2026-03-03 to 2026-03-04 Session 40
**Status:** COMPLETED
**Previous:** Part 36 - Session 39 (WiFi Manager)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 40 SUMMARY

```
Session 40 solved the fundamental problem of displaying encrypted chat
history on the T-Deck Plus: a fixed 64KB LVGL memory pool shared with
all UI elements, where each chat bubble costs ~1.2KB and the system
crashed at 8 bubbles.

The solution is a three-stage sliding window architecture that enables
unlimited encrypted chat history at constant memory consumption.

3 Packages (40a, 40b, 40c)
3 Git Commits
1 Bug Fixed (#71: scroll re-entrancy)
7 New Lessons (#214-#220)
7 Files Changed

Three-Stage Pipeline:
  SD Card (unlimited, encrypted) > PSRAM Cache (30 msgs) > LVGL Window (5 bubbles)

MILESTONE 16: Sliding Window Chat History
```

---

## Starting Point

Session 39 delivered the WiFi Manager. The SD chat history from Session 37 functioned but had two unresolved issues: Bug #60 (SPI2 display freeze during SD access) and Bug #61 (LVGL heap exhaustion with many bubbles, temporary fix MAX_VISIBLE_BUBBLES=5). Users could never see older messages beyond the initial five.

---

## Architecture: Three-Stage Message Pipeline

```
Stage 1: SD Card (unlimited, AES-256-GCM)
  Full message text (up to 16KB per SMP block)
  Per-contact HKDF-SHA256 key derivation
  Read on-demand via smp_history_load_recent()

        | load (decrypt OUTSIDE SPI mutex)
        v

Stage 2: PSRAM Cache (30 messages, ~135KB)
  history_message_t[30] with full 4096-byte text
  Allocated once, reused across chat switches
  Serves scroll-up and scroll-down requests

        | render window slice
        v

Stage 3: LVGL Bubble Window (5 bubbles, ~6KB of 64KB pool)
  Text truncated to 512 chars for display only
  Scroll up: delete bottom, insert top
  Scroll down: delete top, append bottom
  Pool never above 55%, stable across all test cycles
```

### Critical Data Flow Rules

These rules were established after a correction where Claude incorrectly truncated text before SD storage. They must not regress:

- HISTORY_MAX_TEXT = 4096 bytes (SD storage, unchanged)
- HISTORY_MAX_PAYLOAD = 16000 bytes (SD limit per SMP block; hard 16,384-byte SMP block limit, no chunking, no XFTP fallback for text)
- HISTORY_DISPLAY_TEXT = 512 chars (UI-only truncation in LVGL bubble layer ONLY)
- Truncation happens ONLY at the LVGL bubble layer, never before SD storage

### SMP Message Size Limits

| Parameter | Value | Source |
|-----------|-------|--------|
| SMP Transport Block | 16,384 bytes | Transport.hs:152 |
| Max encoded message | 15,602 bytes | Protocol.hs:668 |
| Max with PQ (Kyber) | 13,380 bytes | PQ header 2,345B vs 123B |
| Effective text payload | ~15,530 bytes | After JSON overhead (56-199B) |

---

## Package 40a: Crypto-Separation from SPI Mutex

**Commit:** `refactor(history): separate crypto operations from SPI mutex scope`

### Problem

SD card operations held the LVGL/SPI2 mutex during AES-GCM encryption and decryption, blocking display rendering for hundreds of milliseconds. This was the operational manifestation of Bug #60 (SPI2 bus contention identified in Session 38).

### Solution: Two-Pass Architecture

**Append (write message to SD):**
```
Pass 1 (Mutex): fopen > fread header (get msg_count) > fclose
CPU (no mutex): derive_key > build_plaintext > build_nonce > AES-GCM encrypt
Pass 2 (Mutex): fopen > fseek end > fwrite record > fwrite header > fclose
```

**Load (read messages from SD):**
```
CPU (no mutex): derive_key
Pass 1 (Mutex): fopen > fread header > skip-loop > fread ALL raw records
                into PSRAM buffer > fclose
CPU (no mutex): for each record: AES-GCM decrypt in-place > parse to
                history_message_t
Free: release PSRAM buffer
```

**New file special case:** No Pass 1 needed, msg_count = 0 directly, single mutex pass for header + record write.

**Design decision:** File handle closed between passes. An open handle across mutex release would be FATFS-unsafe on ESP-IDF. Safety over performance.

**Result:** Mutex hold time reduced from ~500ms to < 10ms per operation.

### Files Changed
- `main/include/smp_history.h` (123 lines, +HISTORY_MAX_PAYLOAD, +HISTORY_DISPLAY_TEXT)
- `main/state/smp_history.c` (763 lines, two-pass crypto separation for append and load)

---

## Package 40b: LVGL Memory Profiling and Dynamic Bubble Limit

**Commit:** `feat(ui): add LVGL memory profiling and dynamic bubble limit`

### Problem

No visibility into actual LVGL pool consumption per bubble. Historical crashes at 8 bubbles were based on estimates, not measurements.

### Measurement Results

| Metric | Value |
|--------|-------|
| Average bubble cost | ~1.2KB (significantly less than estimated 3-4KB) |
| LVGL pool total | ~61KB (not 65536; TLSF allocator overhead ~3KB) |
| Fixed UI cost | ~28KB (status bar, header, input area, textarea) |
| Available for bubbles | ~25KB (33KB minus 8KB safety reserve) |
| Theoretical maximum | ~20 bubbles |
| Operational limit | 5 bubbles (conservative) |

### Safety Mechanisms

- 8KB margin check before every new bubble (skips creation if insufficient)
- Text truncation to 512 display characters with "..." suffix
- Bubble count tracking with public API (get_count, reset_count, decrement_count)

### Files Changed
- `main/ui/screens/ui_chat_bubble.c` (557 lines, pool monitor, truncation, remove helpers)
- `main/ui/screens/ui_chat_bubble.h` (124 lines, count API, remove helpers API)
- `main/ui/screens/ui_chat.c` (integration)

---

## Package 40c: Sliding Window with Bidirectional Scroll

**Commit:** `feat(ui): implement sliding window for chat bubbles with bidirectional scroll`

### Problem

Static MAX_VISIBLE_BUBBLES (5, originally 8) meant users could never see older messages. The limit was also not correctly enforced (logged "showing 5 of 12" but rendered all 12).

### Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| MSG_CACHE_SIZE | 30 | PSRAM ring cache |
| BUBBLE_WINDOW_SIZE | 5 | Simultaneous LVGL bubbles |
| SCROLL_LOAD_COUNT | 2 | Bubbles per scroll trigger |
| SCROLL_TOP_THRESHOLD | 10px | Trigger for scroll-up |
| SCROLL_BTM_THRESHOLD | 10px | Trigger for scroll-down |

### Chat Open Flow

1. smp_history_load_recent() reads up to 20 messages from encrypted SD
2. ui_chat_cache_history() copies batch into PSRAM cache, calculates initial window
3. Setup guard (s_window_setup) blocks scroll events during construction
4. Progressive render: 3 bubbles per 50ms timer tick (smooth UX)
5. ui_chat_window_render_done() clears guard, activates scroll handling

### Scroll-Up (load older messages)

1. on_scroll_cb detects scroll_y <= 10 with s_window_start > 0
2. s_window_busy guard prevents re-entrant triggers from lv_obj_scroll_to_y()
3. Remove 2 newest bubbles from bottom
4. Measure content height before insertion
5. Insert 2 older bubbles at top via lv_obj_move_to_index(bubble, 0)
6. Measure content height after insertion, correct scroll position by difference
7. Result: user stays at the same messages, no visual jump

### Scroll-Down (load newer messages)

Symmetric reverse. Remove oldest at top, append newer at bottom, correct scroll position.

### Live Messages

New incoming messages are added to the PSRAM cache and s_window_end incremented. If bubble count exceeds BUBBLE_WINDOW_SIZE, the oldest bubble is removed.

### Bug #71: Scroll Re-Entrancy

lv_obj_scroll_to_y() inside load_older_messages() synchronously fired a new LV_EVENT_SCROLL, which triggered load_newer_messages() in the same frame (7 bubbles instead of 5). Fix: s_window_busy flag prevents re-entrant scroll handling.

### Files Changed
- `main/ui/screens/ui_chat.c` (867 lines, PSRAM cache, scroll handler, window management, live-msg fix)
- `main/ui/screens/ui_chat.h` (138 lines, cache API, window accessors, render_done)
- `main/ui/screens/ui_chat_bubble.c` (557 lines, remove helpers)
- `main/ui/screens/ui_chat_bubble.h` (124 lines, remove helpers API)
- `main/main.c` (655 lines, MAX_VISIBLE_BUBBLES removed, window-based progressive render)

---

## Test Results

**Test 1 (slow scroll, both directions):**
```
[10..15) > [8..13) > [6..11) > [4..9) > [2..7) > [0..5)   UP
[0..5)   > [2..7) > [4..9) > [6..11) > [8..13) > [10..15) DOWN
Always 5 bubbles. Pool 52-55%. Zero crashes.
```

**Test 2 (fast scroll, both directions):**
Same path, same results. No double triggers, no pool overflow.

**Test 3 (multiple chat open/close cycles):**
Fragmentation starts at 14-21% on re-open. Normalizes to 0-8% after first bubbles. No degradation across cycles.

---

## Bug List (Session 40)

| Bug | Description | Root Cause | Fix |
|-----|-------------|------------|-----|
| #71 | Scroll re-entrancy (7 bubbles instead of 5) | lv_obj_scroll_to_y() fires synchronous LV_EVENT_SCROLL | s_window_busy guard flag |

---

## All Delivered Files

| File | Lines | Changes |
|------|-------|---------|
| smp_history.h | 123 | +HISTORY_MAX_PAYLOAD, +HISTORY_DISPLAY_TEXT |
| smp_history.c | 763 | Two-pass crypto separation for append and load |
| ui_chat_bubble.c | 557 | Pool monitor, truncation, remove_oldest/newest helpers |
| ui_chat_bubble.h | 124 | Count API, remove helpers API |
| ui_chat.c | 867 | PSRAM cache, scroll handler, window management, live-msg fix |
| ui_chat.h | 138 | Cache API, window accessors, render_done |
| main.c | 655 | MAX_VISIBLE_BUBBLES removed, window-based progressive render |

---

## Git Commits (Session 40)

```
refactor(history): separate crypto operations from SPI mutex scope
  Two short mutex passes for append, single PSRAM-buffered read pass for
  load. Crypto runs outside SPI lock.

feat(ui): add LVGL memory profiling and dynamic bubble limit
  Pool safety check (8KB margin), text truncation to 512 chars, per-bubble
  cost logging, bubble count tracking.

feat(ui): implement sliding window for chat bubbles with bidirectional scroll
  PSRAM cache (30 msgs), BUBBLE_WINDOW_SIZE=5, scroll-up/down loads
  older/newer messages with position correction. Re-entrancy guard prevents
  double triggers. Replaces static MAX_VISIBLE_BUBBLES.
```

---

## Lessons Learned (Session 40)

| # | Lesson |
|---|--------|
| 214 | LVGL v9 with LV_STDLIB_BUILTIN has a fixed 64KB pool (TLSF, effectively ~61KB). This pool is NOT the ESP32 system heap and NOT PSRAM. |
| 215 | A single 15KB message would consume nearly the entire LVGL pool. Text truncation in the bubble display layer is essential for survival, not optional. |
| 216 | lv_obj_scroll_to_y() inside an LV_EVENT_SCROLL callback fires a synchronous new scroll event. Re-entrancy guard (busy flag) is mandatory. |
| 217 | Crypto operations (AES-GCM, HKDF) are pure CPU work and do not belong inside SPI mutex blocks. Separation reduces mutex hold time from ~500ms to < 10ms. |
| 218 | File handles must be closed between mutex passes. Open handles across mutex release are FATFS-unsafe on ESP-IDF. |
| 219 | HISTORY_MAX_TEXT (storage) and HISTORY_DISPLAY_TEXT (UI) must be separate constants. Conflation causes data loss or pool overflow. |
| 220 | Per-bubble LVGL pool cost is predictable (~1.2KB) when text truncation is active. Without truncation, cost varies by factor 10+. |

---

## Flash Method

Normal build for all changes: `idf.py build flash monitor -p COM6`
No erase-flash required. No new NVS keys. No crypto state changes.

---

*Part 37 - Session 40 Sliding Window Chat History*
*SimpleGo Protocol Analysis*
*Date: March 3-4, 2026*
*Bugs: 71 total (69 FIXED, #60 identified for SPI3, #61 temp fix)*
*Lessons: 220 total*
*Milestone 16: Sliding Window Chat History*
