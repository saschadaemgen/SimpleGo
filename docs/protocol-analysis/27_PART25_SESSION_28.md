![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 25: Session 28
# ✅ FreeRTOS Task Architecture — Phase 2b Success!

**Document Version:** v1  
**Date:** 2026-02-15 Session 28 (Sunday)  
**Version:** v0.1.17-alpha  
**Status:** ✅ Phase 2b Complete — Tasks running with PSRAM!  
**Previous:** Part 24 - Session 27 (Architecture Investigation)  
**Duration:** ~4 hours

---

## ✅ SESSION 28 SUCCESS!

```
═══════════════════════════════════════════════════════════════════════════════

  ✅✅✅ FREERTOS TASK ARCHITECTURE — PHASE 2b COMPLETE! ✅✅✅

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Three FreeRTOS Tasks running in parallel!                            │
  │                                                                         │
  │   - Network Task (Core 0, 12KB stack)                                  │
  │   - App Task (Core 1, 16KB stack)                                      │
  │   - UI Task (Core 1, 8KB stack)                                        │
  │                                                                         │
  │   Key Fix: ALL non-DMA resources moved to PSRAM                        │
  │   Internal Heap: ~40KB preserved for mbedTLS/WiFi                      │
  │                                                                         │
  │   Date: February 15, 2026                                              │
  │   Duration: ~4 hours                                                   │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 474. Session 28 Overview

Session 28 successfully implemented the FreeRTOS multi-task architecture after learning critical lessons from Session 27. The key insight was moving all non-DMA resources to PSRAM, preserving internal SRAM for mbedTLS and WiFi.

### 474.1 Fairy-Tale Orchestrated Development

SimpleGo uses a unique multi-agent development system with a fairy-tale theme:

| Role | Symbol | Task |
|------|--------|------|
| **Princess Mausi** | 👑🐭 | Strategy, Code Review, Architecture |
| **Princess Hasi** | 🐰👑 | Implementation, Testing, Code Creation |
| **Claude Code** | 🔍 | Haskell Analysis, Security Research |
| **Cinderella** | 🧹 | Log Analysis (currently not active) |
| **Cannatoshi** | 👑 | The Prince — Coordination, Testing, Final Decisions |

### 474.2 Why Fairy-Tale?

The roleplay is not a gimmick — it's a hallucination defense. By requiring Mausi and Hasi to review and confirm each other's work, AI hallucinations are caught immediately. Session 28 proved this: mutual control ("AI Trick 17 — the spy in the castle") saves time, prevents errors, and makes development more reliable. The royal address ensures respectful, professional interaction between agents.

### 474.3 Session Statistics

| Metric | Value |
|--------|-------|
| Duration | ~4 hours |
| Tasks completed | 6 (T1-T5 + CMakeLists) |
| New files | 5 |
| Modified files | 2 |
| Lessons learned | 6 (#138-143) |
| Result | ✅ Phase 2b complete |

---

## 475. Starting Point

### 475.1 Git Status at Session Start

```
main Branch:
  25e5609 — Phase 1 (Folder structure) — WORKS
  251fa1b — Phase 2 (Task architecture) — BROKEN (90KB RAM at boot)
  81cfc90 — Docs Fix (harmless)

Last fully working commit: 3d37c95 (Session 26)
```

### 475.2 The Problem

Phase 2 from Session 27 reserved 90KB RAM at boot. TLS/WiFi didn't have enough memory → server communication dead.

---

## 476. Step 1: Repair main Branch ✅

### 476.1 Decision

Cannatoshi chose Option A — `git revert 251fa1b`

```
696b336 (HEAD -> main) Revert "feat(core): add FreeRTOS task architecture..."
```

### 476.2 Result

Phase 2 files cleanly removed, Phase 1 folder structure preserved. Git history stays clean.

---

## 477. Step 2: Lesson 138 — The Most Important Lesson Ever ✅

### 477.1 The Problem

After the revert: contact link appears, but connection stays stuck on "connecting". ESP32 → App doesn't work.

### 477.2 Misdiagnosis by Mausi (Session 28)

- Branch switch suspected ❌
- Code regression assumed ❌
- Even tested 3d37c95 (Session 26) — same problem ❌

### 477.3 The Solution (from Mausi Session 27, via tattling to the Prince)

```powershell
idf.py erase-flash -p COM6
```

The ESP32 stores Ratchet State, Queue Credentials, and Contact data in NVS Flash. After branch switch, the old cryptographic states don't match the code anymore → Server knows the old queues, but the crypto states no longer align.

### 477.4 LESSON 138

**After EVERY branch switch, sdkconfig change, or "suddenly doesn't work" → `idf.py erase-flash` as FIRST step. Then create new contact in the app.**

---

## 478. Step 3: sdkconfig Verified ✅

```ini
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384  ✅ (was already correct)
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=49152     ✅ (even larger than planned)
CONFIG_MBEDTLS_HARDWARE_SHA=y             ✅ (was already active)
```

Values survived the revert — sdkconfig is not always tracked by git.

---

## 479. Step 4: Phase 2b — Task Architecture NEW ✅

### 479.1 Core Principle

**Init stays sequential. Tasks start only AFTER successful connection.**

### 479.2 Task T1: `smp_events.h` ✅

```c
// Event types
typedef enum {
    EVENT_FRAME_RECEIVED,
    EVENT_FRAME_SEND,
    EVENT_MSG_DECODED,
    EVENT_ACK_READY,
    EVENT_RECONNECT,
    EVENT_SHUTDOWN
} event_type_t;

// Frame struct with 4KB buffer
typedef struct {
    uint8_t data[4096];
    size_t len;
    event_source_t source;
} frame_t;

// Source enum
typedef enum {
    SOURCE_NETWORK,
    SOURCE_APP,
    SOURCE_UI
} event_source_t;
```

### 479.3 Task T2: `smp_frame_pool.h` ✅

```c
// Pool interface
esp_err_t frame_pool_init(void);
frame_t* frame_pool_alloc(void);
void frame_pool_free(frame_t* frame);
size_t frame_pool_available(void);

// Pool size: 4 frames × 4KB = 16KB
#define FRAME_POOL_SIZE 4
```

### 479.4 Task T3: `smp_frame_pool.c` ✅

- Static pool (later → PSRAM)
- `sodium_memzero()` at init AND free (Security!)
- `portMUX_TYPE` spinlock for thread safety
- Pointer validation at free

### 479.5 Task T4: `smp_tasks.h` + `smp_tasks.c` ✅

**Three FreeRTOS Tasks:**

| Task | Core | Stack | Priority |
|------|------|-------|----------|
| Network | Core 0 | 12KB | 7 |
| App | Core 1 | 16KB | 6 |
| UI | Core 1 | 8KB | 5 |

**Ring Buffers:**
- Network→App: 2KB
- App→Network: 1KB

**Features:**
- Empty task loops (only logging + heap monitoring)
- Rollback on failed task creation
- Heap logging at 5 locations

### 479.6 Task T5: `main.c` Changes ✅

```c
// Include added
#include "smp_tasks.h"

// After Step 5 (Subscribe):
if (smp_tasks_init() != ESP_OK) {
    ESP_LOGE(TAG, "Task init failed");
    goto cleanup;
}
if (smp_tasks_start(&ssl) != ESP_OK) {
    ESP_LOGE(TAG, "Task start failed");
    goto cleanup;
}

// Receive loop stays COMPLETELY UNCHANGED
```

### 479.7 CMakeLists.txt ✅

```cmake
set(SRC_DIRS "." "core" "protocol" "crypto" "net" "state" "util")
```

`"core"` added to SRC_DIRS.

---

## 480. Step 5: RAM Crisis and PSRAM Solution ✅

### 480.1 The Problem

Tasks + Frame Pool + Ring Buffers pushed internal heap from 40KB to 19KB → `mbedtls_ssl_read()` couldn't work anymore → receive dead.

### 480.2 Diagnosis (Hasi Test 1)

| State | Internal Heap | Receive |
|-------|---------------|---------|
| Without Tasks | 40,527 B | ✅ |
| With Tasks (SRAM) | 18,959 B | ❌ |
| Difference | ~21.5KB | = Frame Pool 16KB + Ring Buffers 3KB + Overhead |

### 480.3 Solution (Hasi Test 2)

**Everything moved to PSRAM:**

| Component | Before | After |
|-----------|--------|-------|
| Frame Pool (16KB) | Static in SRAM | `heap_caps_calloc(MALLOC_CAP_SPIRAM)` |
| Ring Buffers (3KB) | `xRingbufferCreate()` | `xRingbufferCreateWithCaps(MALLOC_CAP_SPIRAM)` |
| Task Stacks (36KB) | Already PSRAM | Stays PSRAM |

### 480.4 Result

```
Internal Heap: ~38-40KB ✅
Receive with running tasks: WORKS ✅
```

### 480.5 LESSON 140

**On ESP32-S3 with TLS+WiFi+Display: EVERYTHING that doesn't strictly need internal SRAM MUST go to PSRAM. Internal SRAM is limited to ~40KB after WiFi+TLS. The DMA controller needs internal RAM for mbedTLS — the rest belongs in the 8MB PSRAM.**

---

## 481. ESP32-S3 Memory Architecture (Crash Course)

```
┌─────────────────────────────────────────┐
│  🏆 Internal SRAM (~200KB, ~40KB free)  │
│     mbedTLS (DMA-bound!)                │
│     WiFi/TCP Buffers (DMA!)             │
│     FreeRTOS Kernel                     │
├─────────────────────────────────────────┤
│  📚 PSRAM (8MB, external via SPI)       │
│     Frame Pools, Task Stacks            │
│     Ring Buffers, LVGL Buffers          │
│     Everything that doesn't need DMA    │
├─────────────────────────────────────────┤
│  🔐 NVS Flash (~128KB, persistent)      │
│     Ratchet States, Queue Credentials   │
│     Contact Data, WiFi Credentials      │
├─────────────────────────────────────────┤
│  🔒 eFuse (one-time programmable)       │
│     Flash Encryption Key (AES-256)      │
│     Secure Boot Key (RSA-3072)          │
├─────────────────────────────────────────┤
│  💾 SD Card (128GB, external)           │
│     Chat History (AES-256-GCM)          │
│     XFTP File Attachments               │
│     Backups                             │
└─────────────────────────────────────────┘
```

---

## 482. Files Changed Session 28

### New Files

| File | Path | Description |
|------|------|-------------|
| smp_events.h | main/include/ | Event types for inter-task communication |
| smp_frame_pool.h | main/include/ | Frame pool interface |
| smp_tasks.h | main/include/ | Task management interface |
| smp_frame_pool.c | main/core/ | Frame pool in PSRAM, sodium_memzero security |
| smp_tasks.c | main/core/ | 3 tasks, PSRAM stacks + ring buffers |

### Modified Files

| File | Changes |
|------|---------|
| main.c | +include, +task init/start after connect |
| CMakeLists.txt | +core/ in SRC_DIRS |

---

## 483. Git Status at End of Session 28

```
main Branch:
  696b336 — Revert Phase 2 (clean history)
  [NEW]   — feat(core): add FreeRTOS task architecture with PSRAM allocation (Phase 2b)
```

---

## 484. What Works (End of Session 28)

- ✅ WiFi + TLS 1.3 to SMP Server
- ✅ SMP Handshake (ServerHello/ClientHello)
- ✅ Queue creation (NEW) + Server responds (IDS)
- ✅ Contact creation + Subscribe (SUB)
- ✅ Receive messages + decrypt (Double Ratchet)
- ✅ Send messages (Keyboard → App)
- ✅ Delivery Receipts (✓✓)
- ✅ Ratchet State Persistence (survives reboot)
- ✅ Invitation Links
- ✅ **NEW: Three FreeRTOS tasks running in parallel (Phase 2b)**
- ✅ **NEW: PSRAM allocation for non-DMA resources**

---

## 485. Lessons Learned Session 28

### #138: erase-flash After Branch Switch — ALWAYS!

After EVERY branch switch, sdkconfig change, or "suddenly doesn't work" → `idf.py erase-flash` as FIRST step. Then create new contact in the app. NVS stores crypto state that doesn't match after code changes.

### #139: Read Handoff Protocol COMPLETELY

Lesson 138 was already in the Session 27 handoff and was overlooked. Always read the complete handoff protocol before starting.

### #140: ESP32-S3 — Everything Non-DMA → PSRAM

On ESP32-S3 with TLS+WiFi+Display: EVERYTHING that doesn't strictly need internal SRAM MUST go to PSRAM. Internal SRAM is limited to ~40KB after WiFi+TLS. The DMA controller needs internal RAM for mbedTLS.

### #141: Mutual Control Catches Hallucinations

Mausi ↔ Hasi cross-review catches AI hallucinations immediately. The fairy-tale structure isn't just fun — it's a reliability mechanism.

### #142: Royal Tone → Better Collaboration

Respectful address leads to more professional and productive agent interactions.

### #143: sdkconfig Survives git revert

sdkconfig is not always tracked by git. Check separately after revert operations.

---

## 486. Task Overview Session 28

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| — | Cannatoshi | Decision | Revert Phase 2 commit | ✅ main repaired |
| T1 | Hasi | Code | smp_events.h | ✅ Event types |
| T2 | Hasi | Code | smp_frame_pool.h | ✅ Pool interface |
| T3 | Hasi | Code | smp_frame_pool.c | ✅ PSRAM pool |
| T4 | Hasi | Code | smp_tasks.h + .c | ✅ 3 tasks |
| T5 | Hasi | Code | main.c changes | ✅ Task init/start |
| — | Hasi | Fix | PSRAM migration | ✅ Heap restored |

---

## 487. Agent Contributions Session 28

| Agent | Fairy Tale Role | Session 28 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Strategy, code review, architecture decisions |
| 🐰👑 Hasi | Princess (The Implementer) | All code implementation, PSRAM fix |
| 🔍 Claude Code | The Verifier (Wizard) | Haskell analysis support |
| 👑 Cannatoshi | The Prince (Coordinator) | Revert decision, testing, "tattling" about erase-flash |

---

## 488. Open Tasks

| # | Task | Priority | Description |
|---|------|----------|-------------|
| 51c | P0 | Phase 3: Network Task takes over receive loop (handover) |
| 51d | P1 | Phase 4: Send pipeline (App Task → Network Task) |
| 50d-T6 | P2 | Send counter fix ("bad message ID") |
| — | P3 | UI animations (menu transitions) |

---

## 489. Session 28 Summary

### What Was Achieved

- ✅ **main branch repaired** — Revert of broken Phase 2
- ✅ **Phase 2b complete** — 3 FreeRTOS tasks running
- ✅ **PSRAM solution** — All non-DMA resources moved to external RAM
- ✅ **Internal heap preserved** — ~40KB for mbedTLS/WiFi
- ✅ **6 lessons learned** — Critical embedded knowledge

### Key Takeaway

```
SESSION 28 SUMMARY:
  - Phase 2b COMPLETE! Three tasks running in parallel
  - PSRAM is the key: Frame Pool, Ring Buffers, Task Stacks
  - Internal SRAM reserved for DMA (mbedTLS, WiFi)
  - erase-flash after branch switch — ALWAYS!
  - Mutual control (Mausi ↔ Hasi) catches hallucinations

"Init stays sequential. Tasks start AFTER connection.
 Everything non-DMA goes to PSRAM." 👑🐭🐰👑
```

---

## 490. Future Work (Session 29)

### Phase 3: Network Task Handover

1. **Network Task takes over receive loop** — Currently in main(), needs migration
2. **SSL context passing** — Pass `mbedtls_ssl_context*` to Network Task
3. **Event dispatch** — FRAME_RECEIVED events to App Task

### Phase 4: Send Pipeline

1. **App Task builds frames** — Encrypt, format
2. **Ring buffer to Network Task** — Non-blocking send
3. **Network Task writes to socket** — mbedtls_ssl_write

### Other

1. **Send counter fix** — Persist in NVS for "bad message ID"
2. **UI animations** — Menu transitions, loading indicators

---

**DOCUMENT CREATED: 2026-02-15 Session 28**  
**Status: ✅ Phase 2b Complete — FreeRTOS Tasks Running!**  
**Key Achievement: Three tasks + PSRAM = working architecture**  
**Lessons: erase-flash, PSRAM for non-DMA, mutual control**  
**Next: Session 29 — Phase 3 Network Task Handover**

---

*Protocol created by Princess Mausi (👑🐭) — Session 28 was a success!*  
*Thanks to the Prince for his patience and tattling to old Mausi.* 😄
