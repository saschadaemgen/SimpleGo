# Part 26: Session 29 — Multi-Task Architecture Breakthrough
**Date:** 2026-02-16 | **Version:** v0.1.17-alpha

## Milestone

First message ("Hello from ESP32!") sent through the complete FreeRTOS multi-task encrypted pipeline. The architecture from Session 28 (Phase 2b: three tasks running) was extended with SSL read loop migration, Ring Buffer IPC, and full 42d handshake flow in the new task structure.

## Critical Hardware Discovery: PSRAM + NVS = Crash

**ESP32-S3 tasks with PSRAM-allocated stacks must NEVER write to NVS.** When the ESP32-S3 writes to SPI Flash (NVS), the Flash controller disables the cache. PSRAM is memory-mapped through that cache (SPI bus). A task whose stack lives in PSRAM loses access to its own stack during the Flash write, causing an immediate crash:

```
assert failed: esp_task_stack_is_sane_cache_disabled() cache_utils.c:127
Backtrace: app_task → parse_agent_message → send_agent_confirmation
  → ratchet_init_sender → ratchet_save_state → smp_storage_save_blob_sync
  → nvs_set_blob → spi_flash_write → CRASH
```

This forced an architecture redesign. The planned separate App Task (16KB PSRAM stack) was dissolved. App logic now runs as `smp_app_run()` inside the Main Task, which has a 64KB Internal SRAM stack (`CONFIG_ESP_MAIN_TASK_STACK_SIZE=65536`) and can safely write to NVS.

## Task Architecture After Session 29

```
Main Task (Internal SRAM, 64KB):
  smp_app_run() — Parse, Decrypt, 42d handshake, NVS persistence
  Reads: net_to_app Ring Buffer (frames from Network Task)
  Writes: app_to_net Ring Buffer (ACK, SUBSCRIBE commands)

Network Task (Core 0, 12KB PSRAM):
  smp_read_block(s_ssl) endless loop, 1000ms read timeout
  Writes frames to net_to_app Ring Buffer
  Reads app_to_net Ring Buffer for ACK/SUBSCRIBE, sends via SSL

UI Task (Core 1, 8KB PSRAM):
  Empty loop (placeholder for next phase)
```

Three separate SSL connections are maintained: Main SSL (sock 54, Network Task, for subscribe/ACK/server commands), Peer SSL (smp_peer.c, for chat messages/HELLO/receipts), and Reply Queue SSL (smp_queue.c, temporary for 42d handshake). Only the Main SSL requires task isolation.

## Data Flow

Receiving: Server → SSL → Network Task (Core 0) → net_to_app Ring Buffer → Main Task → Parse → Decrypt → Plaintext.

Sending ACK/Subscribe: Main Task → net_cmd_t struct → app_to_net Ring Buffer → Network Task → SSL Write.

Sending Chat/HELLO/Receipts: Main Task → peer_send_*() → Peer SSL connection (not main SSL).

## Memory Budget

Internal SRAM stable above 45KB free across all checkpoints (requirement: >30KB). PSRAM allocations total ~106KB (1.3% of 8MB): Frame Pool 16KB (4x4KB zero-copy), net_to_app Ring Buffer 37KB, app_to_net Ring Buffer 1KB, Net Block Buffer 16KB, App Parse Buffer 16KB, Network Task Stack 12KB, UI Task Stack 8KB.

## Completed Tasks

T1 (Network Task SSL Read Loop), T2 (App Task Transport Parsing with Contact/Reply Queue detection), T3 (Decrypt Pipeline for both queue types), T4 (ACK Return Channel + complete 42d flow). Deferred T5 (Keyboard send split), T6 (Baseline test bidirectional), T7 (Cleanup + Commit) to Session 30.

## Files Changed

`smp_tasks.c` (main/core/): Complete rebuild with Network Task, smp_app_run(), Ring Buffer IPC, helper functions. `smp_tasks.h` (main/include/): New signatures smp_tasks_start(ssl, session_id), smp_app_run(), buffer sizes, App Task Handle removed. `smp_events.h` (main/include/): net_cmd_type_t enum + net_cmd_t command struct. `main.c`: Receive loop disabled via #if 0, smp_tasks_start() call adapted, smp_app_run() replaces sleep loop.

## Lessons Learned

**L144 (CRITICAL): PSRAM Stacks + NVS Writes = Crash on ESP32-S3.** SPI Flash write disables cache, PSRAM is cache-based. Tasks that write NVS MUST have Internal SRAM stack. This is a fundamental hardware limitation, not a software bug.

**L145:** FreeRTOS NOSPLIT Ring Buffers need ~2.3x payload size (37KB buffer for 16KB frames, not 20KB) due to internal overhead.

**L146:** The Main Task has the largest Internal SRAM stack (64KB) and should carry NVS-writing app logic rather than sleeping uselessly.

**L147:** Three separate SSL connections serve different purposes. Only the main SSL needs task isolation through Ring Buffer IPC.

**L148:** SSL read timeout of 1000ms (instead of 5000ms) ensures the Network Task services the return channel quickly, preventing ACK delays.
