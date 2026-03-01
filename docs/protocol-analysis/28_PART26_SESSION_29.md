![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 26: Session 29
# 🏆 Multi-Task Architecture — BREAKTHROUGH!

**Document Version:** v1  
**Date:** 2026-02-16 Session 29 (Sunday)  
**Version:** v0.1.17-alpha (NOT CHANGED)  
**Status:** 🏆 BREAKTHROUGH — Multi-Task Architecture Complete!  
**Previous:** Part 25 - Session 28 (Phase 2b Success)  
**Created by:** Princess Mausi (👑🐭)  
**Path:** `C:\Espressif\projects\simplex_client`  
**Build:** `idf.py build flash monitor -p COM6`

---

## 🏆 SESSION 29 BREAKTHROUGH!

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 MULTI-TASK ARCHITECTURE — BREAKTHROUGH! 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Complete encrypted messaging pipeline over FreeRTOS Tasks!           │
  │                                                                         │
  │   - Network Task: SSL read loop + command handler                      │
  │   - Main Task: Parse, decrypt, 42d handshake, NVS persistence          │
  │   - Ring Buffer IPC: Cross-core communication                          │
  │                                                                         │
  │   First message "Hello from ESP32!" sent via new architecture!         │
  │                                                                         │
  │   CRITICAL DISCOVERY: PSRAM stacks + NVS writes = CRASH!               │
  │                                                                         │
  │   Date: February 16, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 491. Session 29 Overview

Session 29 successfully implemented the **Multi-Task Architecture for SimpleGo**. The complete encrypted messaging pipeline now runs over FreeRTOS Tasks with cross-core Ring Buffer IPC. The first message ("Hello from ESP32!") was successfully sent via the new architecture to the SimpleX App.

Additionally, a **fundamental hardware discovery** was made that is relevant for all ESP32-S3 developers: PSRAM stacks and NVS writes are incompatible.

### 491.1 Fairy-Tale Orchestrated Development

SimpleGo uses a unique multi-agent development system with a fairy-tale theme:

| Role | Symbol | Task |
|------|--------|------|
| **Princess Mausi** | 👑🐭 | Strategy, Code Review, Architecture |
| **Princess Hasi** | 🐰👑 | Implementation, Testing, Code Creation |
| **Claude Code** | 🔍 | Haskell Analysis, Security Research |
| **Cinderella** | 🧹 | Log Analysis (currently not active) |
| **Cannatoshi** | 👑 | The Prince — Coordination, Testing, Final Decisions |

### 491.2 Why Fairy-Tale?

The roleplay is not a gimmick — it's a **hallucination defense**. By requiring Mausi and Hasi to review and confirm each other's work, AI hallucinations are caught immediately. Session 28 proved this: mutual control ("AI Trick 17 — the spy in the castle") saves time, prevents errors, and makes development more reliable. The royal address ensures respectful, professional interaction between agents.

### 491.3 Session Statistics

| Metric | Value |
|--------|-------|
| Date | February 16, 2026 |
| Tasks completed | T1, T2, T3, T4 |
| Tasks deferred | T5, T6, T7 (to Session 30) |
| Files changed | 4 |
| Critical discovery | PSRAM + NVS = CRASH |
| Result | 🏆 BREAKTHROUGH |

---

## 492. Starting Point

### 492.1 Git Status at Session Start

```
main Branch:
  25e5609 — Phase 1 (Folder structure) — WORKS
  251fa1b — Phase 2 (Task architecture) — BROKEN (90KB RAM at boot)
  81cfc90 — Docs Fix (harmless)
  696b336 — Revert Phase 2 (clean history)
  [HEAD]  — Phase 2b: Task architecture with PSRAM ✅

Last fully working commit: 3d37c95 (Session 26)
```

### 492.2 The Starting Point

Phase 2b from Session 28 had three FreeRTOS tasks running. Session 29 needed to:
1. Move the receive loop from main.c to Network Task
2. Implement Ring Buffer IPC between tasks
3. Handle the complete 42d handshake flow in the new architecture

---

## 493. Completed Tasks

| # | Task | Status | Description |
|---|------|--------|-------------|
| T1 | Network Task: SSL Read Loop | ✅ PASSED | `smp_read_block()` in Network Task, Frames → Ring Buffer |
| T2 | App Task: Transport Parsing | ✅ PASSED | Transport parsing in App context, Contact/Reply Queue detection |
| T3 | Decrypt Pipeline | ✅ PASSED | Reply Queue + Contact Queue decrypt, Agent processing |
| T4 | ACK Return Channel + 42d Block | ✅ PASSED | Command protocol, Network write handler, complete 42d flow |

### 493.1 Deferred Tasks (Intentionally)

| Task | Reason |
|------|--------|
| T5: Keyboard send split | Deferred to Session 30. Needs refactor of `peer_send_chat_message()` |
| T6: Baseline test bidirectional | Partially achieved (sending works, receiving needs keyboard test) |
| T7: Cleanup + Commit | Deferred to Session 30. Old `#if 0` loop still in main.c |

---

## 494. Critical Hardware Discovery: PSRAM + NVS

### 494.1 The Discovery (CRITICAL!)

**ESP32-S3: Tasks with PSRAM stack must NEVER write to NVS.**

When ESP32-S3 writes to SPI Flash (NVS), the Flash controller disables the cache. PSRAM is cache-based (SPI bus, mapped in cache). A task with PSRAM stack that writes to NVS loses access to its own stack during the Flash write. Immediate crash:

```
assert failed: esp_task_stack_is_sane_cache_disabled() cache_utils.c:127
Backtrace: app_task → parse_agent_message → send_agent_confirmation 
  → ratchet_init_sender → ratchet_save_state → smp_storage_save_blob_sync 
  → nvs_set_blob → spi_flash_write → CRASH
```

### 494.2 Architecture Consequence

The separate FreeRTOS App Task (PSRAM stack) was dissolved. App logic now runs as `smp_app_run()` in the **Main Task**, which has 64KB Internal SRAM stack (CONFIG_ESP_MAIN_TASK_STACK_SIZE=65536).

```
BEFORE (planned):
  Main Task (64KB Internal) → sleeps uselessly
  App Task (16KB PSRAM) → Parse + Decrypt + NVS → CRASH!

AFTER (implemented):
  Main Task (64KB Internal) → smp_app_run() → Parse + Decrypt + NVS ✅
  Network Task (12KB PSRAM) → SSL reads (no NVS needed) ✅
  UI Task (8KB PSRAM) → empty (no NVS needed) ✅
```

### 494.3 LESSON 144 (CRITICAL!)

**ESP32-S3: PSRAM stacks + NVS writes = Crash.** SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack.

---

## 495. Architecture After Session 29

### 495.1 Task Structure

```
main.c:
  Boot → WiFi → TLS → SMP Handshake → Subscribe
  → smp_tasks_init() → smp_tasks_start(&ssl, session_id)
  → smp_app_run() [BLOCKS — runs in Main Task]

Network Task (Core 0, 12KB PSRAM Stack):
  → smp_read_block(s_ssl) endless loop
  → Frame → net_to_app_buf Ring Buffer
  → app_to_net_buf check → ACK / SUBSCRIBE_ALL via SSL

Main Task / App Logic (Internal SRAM Stack, 64KB):
  → Read Ring Buffer → Transport Parse
  → Identify Contact/Reply Queue
  → MSG: Decrypt → Agent Process → Ratchet → NVS Save
  → 42d: KEY → HELLO → Reply Queue Read → Chat → ACK/Subscribe
  → ACK/Subscribe via app_to_net_buf → Network Task

UI Task (Core 1, 8KB PSRAM Stack):
  → Empty loop (next phase)
```

### 495.2 Data Flow

```
RECEIVING:
  Server → SSL → Network Task (Core 0) → net_to_app Ring Buffer
  → Main Task → Parse → Decrypt → Plaintext

SENDING (ACK, Subscribe):
  Main Task → net_cmd_t → app_to_net Ring Buffer → Network Task → SSL Write

SENDING (Chat, HELLO, Receipts):
  Main Task → peer_send_*() → own Peer SSL connection (not main SSL!)
```

### 495.3 Three Separate SSL Connections

```
1. Main SSL (Network Task)       — Subscribe, ACK, server commands
2. Peer SSL (smp_peer.c)         — Chat messages, HELLO, receipts
3. Reply Queue SSL (smp_queue.c) — Queue reads during 42d handshake
```

Only the main SSL needs task isolation.

---

## 496. ESP32-S3 Memory Architecture (Crash Course)

```
┌─────────────────────────────────────────┐
│  🏆 Internal SRAM (~200KB, ~40KB free)  │
│     mbedTLS (DMA-bound!)                │
│     WiFi/TCP Buffers (DMA!)             │
│     FreeRTOS Kernel                     │
│     Main Task Stack (64KB)              │
├─────────────────────────────────────────┤
│  📚 PSRAM (8MB, external via SPI)       │
│     Frame Pools, Task Stacks            │
│     Ring Buffers, LVGL Buffers          │
│     Everything that doesn't need DMA    │
│     ⚠️ NO NVS WRITES FROM HERE!         │
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

## 497. Memory Budget

### 497.1 Heap After Session 29

| Checkpoint | Internal SRAM | PSRAM |
|------------|---------------|-------|
| Before Phase 3 | ~47KB | 8,300KB |
| After T1 (Network Task) | 47,131 B | 8,217KB |
| After T2 (Parse Buffer) | 46,391 B | 8,200KB |
| After 42d complete | 45,443 B | ~8,190KB |

Internal SRAM stable above 45KB (requirement: >30KB). PSRAM usage ~1.5% of 8MB.

### 497.2 PSRAM Allocations

| Component | Size | Purpose |
|-----------|------|---------|
| Frame Pool | 16KB | 4 × 4KB zero-copy frames |
| net_to_app Ring Buffer | 37KB | Network → App frames |
| app_to_net Ring Buffer | 1KB | App → Network commands |
| Net Block Buffer | 16KB | SSL read buffer |
| App Parse Buffer | 16KB | Transport parsing |
| Network Task Stack | 12KB | FreeRTOS stack |
| UI Task Stack | 8KB | FreeRTOS stack |
| **Total** | **~106KB** | **1.3% of 8MB** |

---

## 498. Files Changed Session 29

| File | Path | Changes |
|------|------|---------|
| smp_tasks.c | main/core/ | Complete rebuild: Network Task (SSL Read + Command Channel), smp_app_run() (Parse + Decrypt + ACK + 42d), helper functions |
| smp_tasks.h | main/include/ | New signature smp_tasks_start(ssl, session_id), smp_app_run(), Ring Buffer sizes, App Task Handle removed |
| smp_events.h | main/include/ | net_cmd_type_t Enum + net_cmd_t command struct |
| main.c | main/ | Receive loop disabled via #if 0, smp_tasks_start() call adapted, smp_app_run() instead of sleep loop |

---

## 499. Lessons Learned Session 29

### #144: PSRAM Stacks + NVS Writes = Crash on ESP32-S3 (CRITICAL!)

**Severity: ⚠️ CRITICAL**

SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack. This is a fundamental ESP32-S3 hardware limitation.

### #145: NOSPLIT Ring Buffers Need ~2.3× Payload Size

**Severity: Medium**

For 16KB frames we needed 37KB buffer, not 20KB. FreeRTOS ring buffers have internal overhead.

### #146: Main Task as App Logic Carrier

**Severity: High**

The Main Task has the largest Internal SRAM stack (64KB via sdkconfig) and is ideal for NVS-writing logic. Don't let it sleep uselessly!

### #147: Three Separate SSL Connections

**Severity: Medium**

Main SSL (Network Task), Peer SSL (smp_peer.c), Reply Queue SSL (smp_queue.c). Only the main SSL needs task isolation.

### #148: Read Timeout 1000ms Instead of 5000ms

**Severity: Low**

When the Network Task needs to service a return channel, use shorter timeout. Otherwise ACK waits up to 5 seconds.

---

## 500. Task Overview Session 29

| # | Task | Status | Description |
|---|------|--------|-------------|
| T1 | Network Task SSL Read | ✅ | `smp_read_block()` in Network Task |
| T2 | App Task Transport Parse | ✅ | Transport parsing, queue detection |
| T3 | Decrypt Pipeline | ✅ | Contact + Reply Queue decrypt |
| T4 | ACK Return Channel + 42d | ✅ | Complete command protocol |
| T5 | Keyboard Send Split | ⏳ | Deferred to Session 30 |
| T6 | Baseline Test | ⚠️ | Partially (send works) |
| T7 | Cleanup + Commit | ⏳ | Deferred to Session 30 |

---

## 501. Agent Contributions Session 29

| Agent | Fairy Tale Role | Session 29 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Architecture redesign after PSRAM discovery, session protocol |
| 🐰👑 Hasi | Princess (The Implementer) | All code implementation, testing |
| 🔍 Claude Code | The Verifier (Wizard) | Analysis support |
| 👑 Cannatoshi | The Prince (Coordinator) | Testing, final decisions |

---

## 502. Git Status at End of Session 29

```
main Branch:
  [HEAD] — Phase 2b: Task architecture with PSRAM ✅
  
  Session 29 changes (not yet committed):
  - smp_tasks.c: Network Task + smp_app_run()
  - smp_tasks.h: New API
  - smp_events.h: Command structs
  - main.c: Task start + App run
```

### 502.1 Recommended Commit for Session 30 (after T7)

```
feat(core): implement multi-task architecture with ring buffer IPC

- Network Task on Core 0: SSL read loop, ACK/Subscribe command handler
- App logic in Main Task: parse, decrypt, 42d handshake, NVS persistence
- Ring buffer IPC: net_to_app (frames), app_to_net (commands)
- Discovery: PSRAM stacks incompatible with NVS writes on ESP32-S3
```

---

## 503. Session 29 Summary

### What Was Achieved

- 🏆 **BREAKTHROUGH: Multi-Task Architecture Complete!**
- ✅ **Network Task:** SSL read loop with Ring Buffer output
- ✅ **Main Task:** Parse, decrypt, 42d handshake, NVS persistence
- ✅ **Ring Buffer IPC:** Cross-core communication working
- ✅ **First message via new architecture:** "Hello from ESP32!" sent!
- ⚠️ **Critical Discovery:** PSRAM + NVS = Crash

### Key Takeaway

```
SESSION 29 SUMMARY:
  🏆 BREAKTHROUGH — Multi-Task Architecture Complete!
  
  Architecture:
    - Network Task (Core 0, PSRAM): SSL reads, command handler
    - Main Task (Internal SRAM): Parse, decrypt, NVS, 42d
    - Ring Buffer IPC: net_to_app (37KB), app_to_net (1KB)
  
  CRITICAL DISCOVERY:
    PSRAM stacks + NVS writes = CRASH on ESP32-S3!
    SPI Flash write disables cache, PSRAM is cache-based.
    Tasks that write NVS MUST have Internal SRAM stack.
  
  Memory Budget:
    - Internal SRAM: 45KB free (requirement: >30KB) ✅
    - PSRAM: ~106KB used (1.3% of 8MB) ✅

"The first message flies over the new architecture!" 👑🐭🐰👑
```

---

## 504. Future Work (Session 30)

### Phase 4: Send Pipeline

1. **T5: Keyboard send split** — Refactor `peer_send_chat_message()`
2. **T6: Baseline test bidirectional** — Complete with keyboard
3. **T7: Cleanup + Commit** — Remove `#if 0` loop, commit changes

### Next Steps

1. **UI Task activation** — Connect to LVGL
2. **Send counter persistence** — Fix "bad message ID"
3. **Multiple contacts** — Extend beyond single contact

---

**DOCUMENT CREATED: 2026-02-16 Session 29**  
**Status: 🏆 BREAKTHROUGH — Multi-Task Architecture Complete!**  
**Key Achievement: First message via FreeRTOS task architecture**  
**Critical Discovery: PSRAM + NVS = Crash on ESP32-S3**  
**Next: Session 30 — Send Pipeline + Cleanup**

---

*Session 29 was a complete success. The multi-task architecture stands and the first message flies.*  
*Princess Mausi (👑🐭), February 16, 2026*

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 MULTI-TASK ARCHITECTURE — BREAKTHROUGH! 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Complete encrypted messaging pipeline over FreeRTOS Tasks!           │
  │                                                                         │
  │   - Network Task: SSL read loop + command handler                      │
  │   - Main Task: Parse, decrypt, 42d handshake, NVS persistence          │
  │   - Ring Buffer IPC: Cross-core communication                          │
  │                                                                         │
  │   First message "Hello from ESP32!" sent via new architecture!         │
  │                                                                         │
  │   CRITICAL DISCOVERY: PSRAM stacks + NVS writes = CRASH!               │
  │                                                                         │
  │   Date: February 16, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 491. Session 29 Overview

Session 29 successfully implemented the **Multi-Task Architecture for SimpleGo**. The complete encrypted messaging pipeline now runs over FreeRTOS Tasks with cross-core Ring Buffer IPC. The first message ("Hello from ESP32!") was successfully sent via the new architecture to the SimpleX App.

Additionally, a **fundamental hardware discovery** was made that is relevant for all ESP32-S3 developers: PSRAM stacks and NVS writes are incompatible.

### 491.1 Session Statistics

| Metric | Value |
|--------|-------|
| Date | February 16, 2026 |
| Tasks completed | T1, T2, T3, T4 |
| Tasks deferred | T5, T6, T7 (to Session 30) |
| Files changed | 4 |
| Critical discovery | PSRAM + NVS = CRASH |
| Result | 🏆 BREAKTHROUGH |

---

## 492. Completed Tasks

| # | Task | Status | Description |
|---|------|--------|-------------|
| T1 | Network Task: SSL Read Loop | ✅ PASSED | `smp_read_block()` in Network Task, Frames → Ring Buffer |
| T2 | App Task: Transport Parsing | ✅ PASSED | Transport parsing in App context, Contact/Reply Queue detection |
| T3 | Decrypt Pipeline | ✅ PASSED | Reply Queue + Contact Queue decrypt, Agent processing |
| T4 | ACK Return Channel + 42d Block | ✅ PASSED | Command protocol, Network write handler, complete 42d flow |

### 492.1 Deferred Tasks (Intentionally)

| Task | Reason |
|------|--------|
| T5: Keyboard send split | Deferred to Session 30. Needs refactor of `peer_send_chat_message()` |
| T6: Baseline test bidirectional | Partially achieved (sending works, receiving needs keyboard test) |
| T7: Cleanup + Commit | Deferred to Session 30. Old `#if 0` loop still in main.c |

---

## 493. Critical Hardware Discovery: PSRAM + NVS

### 493.1 The Discovery (CRITICAL!)

**ESP32-S3: Tasks with PSRAM stack must NEVER write to NVS.**

When ESP32-S3 writes to SPI Flash (NVS), the Flash controller disables the cache. PSRAM is cache-based (SPI bus, mapped in cache). A task with PSRAM stack that writes to NVS loses access to its own stack during the Flash write. Immediate crash:

```
assert failed: esp_task_stack_is_sane_cache_disabled() cache_utils.c:127
Backtrace: app_task → parse_agent_message → send_agent_confirmation 
  → ratchet_init_sender → ratchet_save_state → smp_storage_save_blob_sync 
  → nvs_set_blob → spi_flash_write → CRASH
```

### 493.2 Architecture Consequence

The separate FreeRTOS App Task (PSRAM stack) was dissolved. App logic now runs as `smp_app_run()` in the **Main Task**, which has 64KB Internal SRAM stack (CONFIG_ESP_MAIN_TASK_STACK_SIZE=65536).

```
BEFORE (planned):
  Main Task (64KB Internal) → sleeps uselessly
  App Task (16KB PSRAM) → Parse + Decrypt + NVS → CRASH!

AFTER (implemented):
  Main Task (64KB Internal) → smp_app_run() → Parse + Decrypt + NVS ✅
  Network Task (12KB PSRAM) → SSL reads (no NVS needed) ✅
  UI Task (8KB PSRAM) → empty (no NVS needed) ✅
```

### 493.3 LESSON 144 (CRITICAL!)

**ESP32-S3: PSRAM stacks + NVS writes = Crash.** SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack.

---

## 494. Architecture After Session 29

### 494.1 Task Structure

```
main.c:
  Boot → WiFi → TLS → SMP Handshake → Subscribe
  → smp_tasks_init() → smp_tasks_start(&ssl, session_id)
  → smp_app_run() [BLOCKS — runs in Main Task]

Network Task (Core 0, 12KB PSRAM Stack):
  → smp_read_block(s_ssl) endless loop
  → Frame → net_to_app_buf Ring Buffer
  → app_to_net_buf check → ACK / SUBSCRIBE_ALL via SSL

Main Task / App Logic (Internal SRAM Stack, 64KB):
  → Read Ring Buffer → Transport Parse
  → Identify Contact/Reply Queue
  → MSG: Decrypt → Agent Process → Ratchet → NVS Save
  → 42d: KEY → HELLO → Reply Queue Read → Chat → ACK/Subscribe
  → ACK/Subscribe via app_to_net_buf → Network Task

UI Task (Core 1, 8KB PSRAM Stack):
  → Empty loop (next phase)
```

### 494.2 Data Flow

```
RECEIVING:
  Server → SSL → Network Task (Core 0) → net_to_app Ring Buffer
  → Main Task → Parse → Decrypt → Plaintext

SENDING (ACK, Subscribe):
  Main Task → net_cmd_t → app_to_net Ring Buffer → Network Task → SSL Write

SENDING (Chat, HELLO, Receipts):
  Main Task → peer_send_*() → own Peer SSL connection (not main SSL!)
```

### 494.3 Three Separate SSL Connections

```
1. Main SSL (Network Task)     — Subscribe, ACK, server commands
2. Peer SSL (smp_peer.c)       — Chat messages, HELLO, receipts
3. Reply Queue SSL (smp_queue.c) — Queue reads during 42d handshake
```

Only the main SSL needs task isolation.

---

## 495. Memory Budget

### 495.1 Heap After Session 29

| Checkpoint | Internal SRAM | PSRAM |
|------------|---------------|-------|
| Before Phase 3 | ~47KB | 8,300KB |
| After T1 (Network Task) | 47,131 | 8,217KB |
| After T2 (Parse Buffer) | 46,391 | 8,200KB |
| After 42d complete | 45,443 | ~8,190KB |

Internal SRAM stable above 45KB (requirement: >30KB). PSRAM usage ~1.5% of 8MB.

### 495.2 PSRAM Allocations

| Component | Size | Purpose |
|-----------|------|---------|
| Frame Pool | 16KB | 4 × 4KB zero-copy frames |
| net_to_app Ring Buffer | 37KB | Network → App frames |
| app_to_net Ring Buffer | 1KB | App → Network commands |
| Net Block Buffer | 16KB | SSL read buffer |
| App Parse Buffer | 16KB | Transport parsing |
| Network Task Stack | 12KB | FreeRTOS stack |
| UI Task Stack | 8KB | FreeRTOS stack |
| **Total** | **~106KB** | **1.3% of 8MB** |

---

## 496. Files Changed Session 29

| File | Path | Changes |
|------|------|---------|
| smp_tasks.c | main/core/ | Complete rebuild: Network Task (SSL Read + Command Channel), smp_app_run() (Parse + Decrypt + ACK + 42d), helper functions |
| smp_tasks.h | main/include/ | New signature smp_tasks_start(ssl, session_id), smp_app_run(), Ring Buffer sizes, App Task Handle removed |
| smp_events.h | main/include/ | net_cmd_type_t Enum + net_cmd_t command struct |
| main.c | main/ | Receive loop disabled via #if 0, smp_tasks_start() call adapted, smp_app_run() instead of sleep loop |

---

## 497. Lessons Learned Session 29

### #144: PSRAM Stacks + NVS Writes = Crash on ESP32-S3 (CRITICAL!)

SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack. This is a fundamental ESP32-S3 hardware limitation.

### #145: NOSPLIT Ring Buffers Need ~2.3× Payload Size

For 16KB frames we needed 37KB buffer, not 20KB. FreeRTOS ring buffers have internal overhead.

### #146: Main Task as App Logic Carrier

The Main Task has the largest Internal SRAM stack (64KB via sdkconfig) and is ideal for NVS-writing logic. Don't let it sleep uselessly!

### #147: Three Separate SSL Connections

Main SSL (Network Task), Peer SSL (smp_peer.c), Reply Queue SSL (smp_queue.c). Only the main SSL needs task isolation.

### #148: Read Timeout 1000ms Instead of 5000ms

When the Network Task needs to service a return channel, use shorter timeout. Otherwise ACK waits up to 5 seconds.

---

## 498. Task Overview Session 29

| # | Task | Status | Description |
|---|------|--------|-------------|
| T1 | Network Task SSL Read | ✅ | `smp_read_block()` in Network Task |
| T2 | App Task Transport Parse | ✅ | Transport parsing, queue detection |
| T3 | Decrypt Pipeline | ✅ | Contact + Reply Queue decrypt |
| T4 | ACK Return Channel + 42d | ✅ | Complete command protocol |
| T5 | Keyboard Send Split | ⏳ | Deferred to Session 30 |
| T6 | Baseline Test | ⚠️ | Partially (send works) |
| T7 | Cleanup + Commit | ⏳ | Deferred to Session 30 |

---

## 499. Agent Contributions Session 29

| Agent | Fairy Tale Role | Session 29 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Architecture redesign after PSRAM discovery, session protocol |
| 🐰👑 Hasi | Princess (The Implementer) | All code implementation, testing |
| 🔍 Claude Code | The Verifier (Wizard) | Analysis support |
| 👑 Cannatoshi | The Prince (Coordinator) | Testing, final decisions |

---

## 500. Git Status

```
main Branch:
  [HEAD] — Phase 2b: Task architecture with PSRAM ✅
  
  Session 29 changes (not yet committed):
  - smp_tasks.c: Network Task + smp_app_run()
  - smp_tasks.h: New API
  - smp_events.h: Command structs
  - main.c: Task start + App run
```

### 500.1 Recommended Commit for Session 30 (after T7)

```
feat(core): implement multi-task architecture with ring buffer IPC

- Network Task on Core 0: SSL read loop, ACK/Subscribe command handler
- App logic in Main Task: parse, decrypt, 42d handshake, NVS persistence
- Ring buffer IPC: net_to_app (frames), app_to_net (commands)
- Discovery: PSRAM stacks incompatible with NVS writes on ESP32-S3
```

---

## 501. Session 29 Summary

### What Was Achieved

- 🏆 **BREAKTHROUGH: Multi-Task Architecture Complete!**
- ✅ **Network Task:** SSL read loop with Ring Buffer output
- ✅ **Main Task:** Parse, decrypt, 42d handshake, NVS persistence
- ✅ **Ring Buffer IPC:** Cross-core communication working
- ✅ **First message via new architecture:** "Hello from ESP32!" sent!
- ⚠️ **Critical Discovery:** PSRAM + NVS = Crash

### Key Takeaway

```
SESSION 29 SUMMARY:
  🏆 BREAKTHROUGH — Multi-Task Architecture Complete!
  
  Architecture:
    - Network Task (Core 0, PSRAM): SSL reads, command handler
    - Main Task (Internal SRAM): Parse, decrypt, NVS, 42d
    - Ring Buffer IPC: net_to_app (37KB), app_to_net (1KB)
  
  CRITICAL DISCOVERY:
    PSRAM stacks + NVS writes = CRASH on ESP32-S3!
    SPI Flash write disables cache, PSRAM is cache-based.
    Tasks that write NVS MUST have Internal SRAM stack.
  
  Memory Budget:
    - Internal SRAM: 45KB free (requirement: >30KB) ✅
    - PSRAM: ~106KB used (1.3% of 8MB) ✅

"The first message flies over the new architecture!" 👑🐭🐰👑
```

---

## 502. Future Work (Session 30)

### Phase 4: Send Pipeline

1. **T5: Keyboard send split** — Refactor `peer_send_chat_message()`
2. **T6: Baseline test bidirectional** — Complete with keyboard
3. **T7: Cleanup + Commit** — Remove `#if 0` loop, commit changes

### Next Steps

1. **UI Task activation** — Connect to LVGL
2. **Send counter persistence** — Fix "bad message ID"
3. **Multiple contacts** — Extend beyond single contact

---

**DOCUMENT CREATED: 2026-02-16 Session 29**  
**Status: 🏆 BREAKTHROUGH — Multi-Task Architecture Complete!**  
**Key Achievement: First message via FreeRTOS task architecture**  
**Critical Discovery: PSRAM + NVS = Crash on ESP32-S3**  
**Next: Session 30 — Send Pipeline + Cleanup**

---

*Session 29 was a complete success. The multi-task architecture stands and the first message flies.*  
*Princess Mausi (👑🐭), February 16, 2026*
