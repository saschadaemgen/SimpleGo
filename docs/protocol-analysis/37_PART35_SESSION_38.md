# SimpleX Protocol Analysis - Part 35: Session 38
# The SPI2 Bus Hunt: Eight Hypotheses, One Root Cause

**Document Version:** v1
**Date:** 2026-02-28 to 2026-03-01 Session 38
**Status:** COMPLETED -- Root cause identified, fix planned for Session 39
**Previous:** Part 34 - Session 37 (Encrypted Chat History)
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 38 SUMMARY

```
The hardest session yet. Two days of bug hunting, eight false
hypotheses, but the true enemy identified in the end.

4 Features Implemented (all working)
10 Git Commits
2 Bugs Found (#60, #61)
5 New Lessons (#205-#209)
1 Root Cause Identified: SPI2 Bus Sharing

Device runs perfectly without SD card.
Session 39 solves the last puzzle piece.

MILESTONE 14: Backlight Control + SPI Root Cause
```

---

## Starting Point

Session 37 had implemented encrypted chat history on SD card with AES-256-GCM. A display freeze had been observed twice but was not yet reproducible. Session 38 set out to add backlight control and investigate the freeze.

---

## Phase 1: Display and Keyboard Backlight

### Keyboard Backlight (I2C)

Keyboard backlight controlled via I2C at address 0x55 with auto-off timer. Completely independent from SPI bus -- runs on I2C, no interaction with display or SD card.

### Display Backlight (GPIO 42)

Display backlight controlled via GPIO 42 with pulse-counting mechanism (16 brightness levels). Also completely independent from SPI bus -- pure GPIO, no bus contention possible.

### Settings Screen

New settings screen with brightness sliders for both display and keyboard. Preset buttons for quick adjustment. Gear button added in chat header for quick access to settings.

### Boot Sequence

Display starts at 50% brightness on boot. Keyboard backlight initializes with auto-off timer.

**Key insight:** Backlight is completely innocent. I2C (keyboard) and GPIO (display) are independent from SPI2. Temporal correlation with freeze was misleading.

**Commits:**
```
feat(hal): add dedicated keyboard backlight module
feat(keyboard): add backlight control with auto-off timer
feat(hal): add display backlight control via pulse-counting
feat(ui): add settings screen with display and keyboard brightness
feat(ui): add gear button in chat header for backlight control
feat(core): integrate backlight initialization in boot sequence
```

---

## Phase 2: WiFi/LWIP Buffers to PSRAM

WiFi and LWIP buffers moved from internal SRAM to PSRAM, freeing 56KB of internal SRAM. This was initially suspected as a potential fix for the display freeze (more internal memory for LVGL), but turned out to be simply a good optimization unrelated to the freeze.

**Commit:** perf(config): move WiFi/LWIP buffers to PSRAM, free 56KB internal SRAM

---

## Phase 3: The SPI2 Bus Hunt (Bug #60)

### The Freeze

Display freezes completely when loading chat history from SD card. Image frozen, but main loop continues running (heartbeat logs still printing). No crash, no assert, just visual freeze.

### Eight Hypotheses -- Seven Wrong

```
1. DMA Timeout Hypothesis         → Freeze was not in DMA wait path
2. Memory Crash Hypothesis        → ESP32 heap was never the problem
3. DMA Callback Revert            → Freeze identical without callback
4. bubble_draw_cb Hypothesis      → Freeze identical without custom callbacks
5. LVGL Pool 64→192KB             → WiFi init crashes (no internal SRAM left)
6. LVGL Pool 64→96KB              → Freeze continues
7. trans_queue_depth 2→1          → Display artifacts + OOM
8. SD Card Removed                → STABLE! Hours of perfect operation!
```

### The Proof

SD card physically removed from T-Deck Plus. Device runs for hours, 100% stable, no freezes, no artifacts, no crashes. SD card reinserted, freeze returns immediately when history loads.

### Root Cause

Display (ST7789) and SD card share the same SPI2 bus on T-Deck Plus hardware. When SD card is read (chat history loading), the SPI2 bus is blocked. Even with LVGL mutex serialization (implemented in S37), the display rendering path stalls because it must wait for SD operations to complete. With enough messages to load, this wait exceeds LVGL's tolerance and the display freezes.

The S37 mutex fix prevented crashes and tearing, but the fundamental contention remains: one bus, two masters, blocking serialization.

### Fix Plan (Session 39)

Move SD card to SPI3 bus. T-Deck Plus has SPI3 available. Separate buses = zero contention = parallel operation.

**Lesson #205:** SPI2 bus sharing is the root cause of display freeze. Not DMA, not memory, not LVGL pool. SD removed = stable. Correlation (backlight commits) ≠ Causation (SPI2 bus).

**Lesson #208:** When 8 hypotheses fail, remove the suspected hardware. Physical elimination test beats software debugging.

---

## Phase 4: LVGL Heap Discovery (Bug #61)

### The Discovery

LVGL has its OWN memory pool, separate from ESP32's system heap. LV_MEM_SIZE=64KB configured in sdkconfig. This pool is for LVGL objects (buttons, labels, bubbles, containers).

```
ESP32 Heap:  heap_caps_get_free_size()  → System memory
LVGL Pool:   LV_MEM_SIZE (sdkconfig)    → LVGL objects only

These are COMPLETELY SEPARATE!
Monitoring ESP32 heap tells you NOTHING about LVGL pool status.
```

### Impact

64KB LVGL pool supports approximately 8 chat bubbles. More bubbles = pool exhaustion = freeze or crash. This explains why the freeze was always observed in the chat with the most messages.

### Fix: MAX_VISIBLE_BUBBLES

```c
#define MAX_VISIBLE_BUBBLES 5  // Temporary conservative limit, target: 8

// Sliding window: only N most recent bubbles exist as LVGL objects
// Older messages loaded from SD history on scroll-up
```

**Lesson #206:** LVGL has its own heap (LV_MEM_SIZE), separate from ESP32 heap. heap_caps_get_free_size() does NOT show LVGL pool status. 64KB supports ~8 bubbles.

---

## Phase 5: SPI Architecture Decisions

### Synchronous SPI (Stable)

DMA callback mechanism from Session 38f was identified as adding complexity without solving the fundamental SPI2 contention. Removed and replaced with synchronous draw_bitmap() + flush_ready().

New tdeck_lvgl.c written with synchronous architecture. Not yet committed -- waiting for SPI3 fix in Session 39 to commit everything together.

### trans_queue_depth

Must remain at 2. Setting to 1 causes OOM errors and display artifacts (stripes). This is a hard constraint of the ESP-IDF SPI driver.

**Lesson #207:** trans_queue_depth MUST stay at 2 (1 = OOM + artifacts). Synchronous draw_bitmap() is more stable than async DMA callback.

**Commits:**
```
fix(display): sync DMA completion before mutex release, add OOM retry
perf(display): reduce SPI transfer size and queue depth
docs(config): correct SD card pin definitions for T-Deck Plus
```

---

## Bug List (Session 38)

| Bug | Description | Root Cause | Fix | Status |
|-----|-------------|------------|-----|--------|
| #60 | Display freeze on SD history load | SPI2 bus contention (display + SD share bus) | Move SD to SPI3 bus | IDENTIFIED -- fix in S39 |
| #61 | LVGL heap exhaustion with many bubbles | 64KB LVGL pool limit, too many objects | MAX_VISIBLE_BUBBLES sliding window | FIXED (temp limit 5, target 8) |

---

## Device State at Session End

| Component | Status |
|-----------|--------|
| Cryptography (Double Ratchet, X3DH, AES) | STABLE |
| Network (TLS 1.3, SMP, PING/PONG) | STABLE |
| Multi-Contact (5 contacts active) | STABLE |
| Display Backlight (GPIO 42, 16 levels) | STABLE |
| Keyboard Backlight (I2C 0x55, auto-off) | STABLE |
| Settings Screen | STABLE |
| Chat UI (without SD) | STABLE |
| Chat History (SD card) | FREEZE on SD access |
| SD card general | SPI2 conflict with display |

---

## Uncommitted Changes

| File | Change | Reason |
|------|--------|--------|
| main/main.c | MAX_VISIBLE_BUBBLES 5 | Bubble limit (temporary, target 8) |
| tdeck_lvgl.c | Synchronous SPI | DMA callback removed |

These changes will be committed in Session 39 after SPI3 fix is tested.

---

## Git Commits (Session 38)

```
feat(hal): add dedicated keyboard backlight module
feat(keyboard): add backlight control with auto-off timer
feat(hal): add display backlight control via pulse-counting
feat(ui): add settings screen with display and keyboard brightness
feat(ui): add gear button in chat header for backlight control
feat(core): integrate backlight initialization in boot sequence
docs(config): correct SD card pin definitions for T-Deck Plus
perf(config): move WiFi/LWIP buffers to PSRAM, free 56KB internal SRAM
fix(display): sync DMA completion before mutex release, add OOM retry
perf(display): reduce SPI transfer size and queue depth
```

---

## Irrweg-Chronologie (The False Path Chronicle)

This section preserves all eight wrong hypotheses in order, as required by the documentation principle "nothing shortened, only extended":

```
Hypothesis 1: DMA Timeout
  Theory: DMA transfer timeout causes display driver to hang
  Test: Added timeout monitoring and fallback paths
  Result: Freeze was NOT in DMA wait code path
  Verdict: WRONG

Hypothesis 2: Memory Crash
  Theory: ESP32 heap exhaustion triggers undefined behavior
  Test: Monitored heap_caps_get_free_size() continuously
  Result: ESP32 heap was never low; LVGL has separate pool
  Verdict: WRONG (led to Lesson #206)

Hypothesis 3: DMA Callback
  Theory: Async DMA callback corrupts state or misses completion
  Test: Completely removed DMA callback, reverted to synchronous
  Result: Freeze identical with synchronous SPI
  Verdict: WRONG

Hypothesis 4: bubble_draw_cb
  Theory: Custom LVGL draw callbacks cause rendering corruption
  Test: Removed all custom draw callbacks from bubble creation
  Result: Freeze identical without any custom callbacks
  Verdict: WRONG

Hypothesis 5: LVGL Pool 64→192KB
  Theory: LVGL pool too small, needs 3x increase
  Test: Increased LV_MEM_SIZE to 192KB
  Result: WiFi init CRASHES — not enough internal SRAM left
  Verdict: WRONG (but revealed SRAM constraint)

Hypothesis 6: LVGL Pool 64→96KB
  Theory: Moderate LVGL pool increase might help
  Test: Increased LV_MEM_SIZE to 96KB
  Result: Freeze continues unchanged
  Verdict: WRONG

Hypothesis 7: trans_queue_depth 2→1
  Theory: SPI transaction queue causes timing issues
  Test: Reduced queue depth from 2 to 1
  Result: Display artifacts (stripes) and OOM errors
  Verdict: WRONG (and destructive — revealed hard constraint)

Hypothesis 8: SD Card Removal
  Theory: SPI2 bus contention between display and SD card
  Test: Physically removed SD card from T-Deck Plus
  Result: Device runs HOURS, 100% stable, zero issues
  Verdict: CORRECT — SPI2 bus sharing IS the root cause
```

---

## Lessons Learned (Session 38)

| # | Lesson | Context |
|---|--------|---------|
| 205 | SPI2 bus sharing is the root cause of display freeze. Not DMA, not memory, not LVGL pool. SD removed = stable. Correlation (backlight commits) ≠ Causation (SPI2 bus). | 8 hypotheses, 1 correct |
| 206 | LVGL has its own heap (LV_MEM_SIZE), separate from ESP32 heap. heap_caps_get_free_size() does NOT show LVGL pool status. 64KB supports ~8 bubbles. | Memory investigation |
| 207 | trans_queue_depth MUST stay at 2 (1 = OOM + display artifacts). Synchronous draw_bitmap() is more stable than async DMA callback. | SPI architecture |
| 208 | When 8 hypotheses fail, remove the suspected hardware. Physical elimination test beats software debugging. | Root cause methodology |
| 209 | WiFi/LWIP buffers can safely run from PSRAM, freeing ~56KB internal SRAM. No performance impact observed. | Memory optimization |

---

## Session 38 Statistics

| Metric | Value |
|--------|-------|
| Duration | 2 days (Feb 28 - Mar 1) |
| Git commits | 10 |
| Features implemented | 4 (display backlight, keyboard backlight, settings, WiFi→PSRAM) |
| Bugs found | 2 (#60, #61) |
| False hypotheses | 8 (7 wrong, 1 correct) |
| New lessons | 5 (#205-#209) |
| Internal SRAM freed | 56KB (WiFi/LWIP → PSRAM) |
| Root cause | SPI2 bus sharing (display + SD card) |

---

## Next Session: 39

### Priorities
1. **SD card on SPI3 bus** -- Root cause fix for display freeze
2. **Sliding window chat history** -- 8 visible bubbles, load older on scroll
3. **WiFi Manager** -- User-friendly network configuration

All three Hasi tasks prepared and ready.

---

*Part 35 - Session 38 The SPI2 Bus Hunt*
*SimpleGo Protocol Analysis*
*Date: February 28 - March 1, 2026*
*Bugs: 61 total (59 FIXED, 1 identified, 1 temp fix)*
*Lessons: 209 total*
*Milestone 14: Backlight Control + SPI Root Cause*
