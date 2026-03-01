# SimpleX Protocol Analysis - Part 24: Session 27
# ⚠️ FreeRTOS Task Architecture Investigation

**Document Version:** v1  
**Date:** 2026-02-14/15 Session 27  
**Status:** ⚠️ Architecture validated, implementation needs restart  
**Previous:** Part 23 - Session 26 (Persistence & Storage)

---

## ⚠️ SESSION 27 SUMMARY

```
═══════════════════════════════════════════════════════════════════════════════

  ⚠️⚠️⚠️ ARCHITECTURE INVESTIGATION — LESSONS LEARNED ⚠️⚠️⚠️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   Phase 1: Folder restructure ✅ WORKS                                 │
  │   Phase 2: FreeRTOS Tasks ❌ BROKE main branch                         │
  │   Phase 3: Network Task Migration — branch polluted by debugging       │
  │                                                                         │
  │   ROOT CAUSE: ~90KB RAM reserved at boot, starved TLS/WiFi             │
  │   SOLUTION: Start tasks AFTER connection, not at boot                  │
  │                                                                         │
  │   Duration: 2 days debugging on wrong branch                           │
  │   Key insight: Always baseline-test main before debugging feature      │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 458. Session 27 Overview

Session 27 attempted to transform the monolithic blocking architecture into a multi-task FreeRTOS system. While the architecture design is sound, the implementation reserved too much RAM at boot time and broke TLS/WiFi connectivity.

### 458.1 Starting Point (After Session 26)

All milestones intact:
- ✅ M1: TLS 1.3 + SMP Handshake
- ✅ M2: Message receive + E2E decrypt
- ✅ M3: Bidirectional chat (ESP32 ↔ App)
- ✅ M4: Delivery receipts (✓✓)
- ✅ M5: Send messages via keyboard
- ✅ M6: Ratchet state persistence (survives reboot)

### 458.2 Session 27 Goals

1. Reorganize source files into subdirectories
2. Implement 3-task FreeRTOS architecture
3. Migrate receive loop to Network Task
4. Enable responsive UI while maintaining network operations

### 458.3 Session 27 Results

| Phase | Description | Result |
|-------|-------------|--------|
| Phase 1 | Folder restructure | ✅ Works perfectly |
| Phase 2 | FreeRTOS Task Architecture | ❌ Broke main branch |
| Phase 3 | Network Task Migration | ⚠️ Branch polluted by debugging |

### 458.4 Session Statistics

| Metric | Value |
|--------|-------|
| Duration | ~2 days |
| Tasks completed | Phase 1 only |
| Commits on main | 3 (1 working, 1 broken, 1 harmless) |
| Commits on branch | 4 |
| Time debugging wrong branch | ~2 days |
| Lessons learned | 17 |

---

## 459. Phase 1: Folder Restructure ✅

### 459.1 Implementation

17 `.c` files reorganized into 6 subdirectories:

```
main/
├── core/          Task management, events, frame pool
├── protocol/      SMP protocol, agent, handshake
├── crypto/        Ratchet, E2E encryption
├── net/           Network, TLS, queue management
├── state/         Storage, contacts, peer state
└── util/          Logging, helpers
```

### 459.2 CMakeLists.txt Change

```cmake
# Before: explicit file list
set(SRCS "main.c" "smp_ratchet.c" ...)

# After: directory-based
set(SRC_DIRS "." "core" "protocol" "crypto" "net" "state" "util")
```

### 459.3 Result

- Build + Flash + Receive test passed
- **Commit:** `25e5609 refactor(structure): reorganize source files into subdirectories`
- **Lesson:** Run cleanup commands BEFORE `git add -A`, not after

---

## 460. Phase 2: FreeRTOS Task Architecture ❌

### 460.1 Architecture Design (Valid)

```
3-Task FreeRTOS System:
├── Network Task (Core 0, Priority 7) — TLS read/write, blocking socket
├── App Task (Core 1, Priority 6) — Crypto, protocol logic, X3DH
└── UI Task (Core 1, Priority 5) — LVGL, keyboard, display

Hybrid IPC:
├── Ring Buffers (No-Split) — For SMP frames
└── FreeRTOS Queues — For events

Memory Pool:
├── 8×4KB static blocks (32KB total)
└── Prevents heap fragmentation
```

### 460.2 Implementation

Files created:
- `smp_events.h` — Event types (app_event_t, ui_event_t, net_send_t)
- `smp_frame_pool.c/.h` — 8×4096 static memory pool with pointer validation
- `smp_tasks.c/.h` — 3 FreeRTOS tasks, Ring Buffers (8KB + 4KB), Queues
- `smp_msg_router.c/.h` — Event-loop skeleton
- `main.c` — `frame_pool_init()` + `smp_tasks_init()` + `smp_tasks_start()` at boot
- `sodium_memzero()` — Added at 6 locations for security

**Commit:** `251fa1b feat(core): add FreeRTOS task architecture with event types and frame pool`

### 460.3 The Problem

This commit reserved ~90KB RAM at boot:

| Component | RAM |
|-----------|-----|
| Network Task Stack | 16KB |
| App Task Stack | 32KB |
| UI Task Stack | 10KB |
| Frame Pool | 32KB |
| Ring Buffers | 12KB |
| **Total** | **~90KB** |

This allocation happened BEFORE `smp_connect()`, which starved TLS/WiFi of memory.

### 460.4 Root Cause Discovery

After 2 days debugging on `phase3-network-task` branch:

```
Test: main branch baseline
Result: ALSO BROKEN!

Test: Checkout 3d37c95 (Session 26)
Result: WORKS PERFECTLY

Test: Checkout 25e5609 (Phase 1)
Result: WORKS

Conclusion: Phase 2 commit (251fa1b) broke main
```

**Git bisect would have saved 2 days.**

---

## 461. Phase 3: Network Task Migration (Branch)

### 461.1 Work Done

Branch `phase3-network-task` created with:
- Receive loop moved from `smp_connect()` to Network Task
- ACK split: `smp_build_ack()` (App) + Network sends via Ring Buffer
- Peer state adaptations for task architecture

### 461.2 Commits on Branch

1. `docs(gfx): add logo and multi-agent header graphics`
2. `feat(core): migrate receive loop to network task with ring buffer IPC`
3. `refactor(net): split ACK build and send for inter-task communication`
4. `fix(state): peer connection fixes and storage adaptations for task architecture`

### 461.3 Branch Status

**Polluted** by debugging experiments:
- Socket timeout experiments
- Non-blocking recv attempts
- sdkconfig changes (back and forth)

Branch should be discarded and Phase 3 restarted after fixing Phase 2.

---

## 462. Peer-Write 16KB Deadlock — SOLVED ✅

### 462.1 Symptom

`mbedtls_ssl_write()` wrote only 4096/16384 bytes, then deadlock.

### 462.2 Root Cause

ESP-IDF defaults:
```ini
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096  # One TLS record = 4096 max
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760     # Exactly one record fits
```

### 462.3 Fix (sdkconfig)

```ini
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768
```

### 462.4 Result

```
Block written: 16384 bytes ✅
```

Works on all three connections.

---

## 463. Command-Write Hang — 2 Days Debugging

### 463.1 Symptom

Handshake write works, but immediately following command write (NEW, SUB) hangs — server doesn't respond.

### 463.2 Debugging Attempts

| # | Attempt | Result | Learning |
|---|---------|--------|----------|
| 1 | `SO_RCVTIMEO=1s` all connections | Write hangs, 15 retries/30s | Timeout makes problem visible, doesn't solve |
| 2 | `smp_read_block()` after ClientHello | Server doesn't respond (ret=-2) | No missing read |
| 3 | Remove reads | No difference | — |
| 4 | `MSG_DONTWAIT` in recv callback | ESP-IDF ignores flag | — |
| 5 | `select(0)` instead of MSG_DONTWAIT | WANT_WRITE instead of WANT_READ | Non-blocking during write broken |
| 6 | Pre-drain before write | WANT_WRITE, 1 retry/30s | send() blocks |
| 7 | `TCP_SND_BUF=49152` | ✅ Writes OK! Server doesn't respond | Buffer too big → async |
| 8 | Remove pre-drain | Reads don't work | — |
| 9 | Remove socket overrides completely | Reads don't work | — |
| 10 | `TCP_SND_BUF=32768` back | WANT_WRITE + TLS -0x7880 | — |
| 11 | Reset `smp_network.c` from main | Writes OK, server doesn't respond | Transport out |
| 12 | Remove Session Tickets DISABLED | No difference | — |
| **13** | **main branch baseline test** | **ALSO BROKEN!** | **Problem was never Phase 3** |
| **14** | **Checkout 3d37c95 (Session 26)** | **WORKS PERFECTLY** | **Phase 2 commit at fault** |
| **15** | **Checkout 25e5609 (Phase 1)** | **WORKS** | **Confirms: Phase 2 at fault** |

### 463.3 Key Insight

**Always baseline-test main before debugging feature branch.**

---

## 464. Claude Code Analyses

### 464.1 Analysis 1: TLS Write Sequence

- Handshake block and command block identically formatted
- No read needed between ClientHello and first command (Haskell confirms)
- Session Tickets: Haskell uses default (noSessionManager = discard)
- `smp_peer.c` already had 1s socket timeout fix
- Copy-paste bug: duplicate `session_tickets DISABLED` in `smp_queue.c`

### 464.2 Analysis 2: main vs phase3 Comparison

- Command encoding byte-identical between branches
- Queue logic (NEW/SUB/KEY/ACK) identical
- 16 differences catalogued, 3 critical:
  1. Session Tickets (main=default, phase3=disabled)
  2. Write Timeout (main=none/infinite, phase3=30s)
  3. ~90KB RAM reservation at boot (only phase3)

### 464.3 Analysis 3: Haskell SMP Client Flow

- Server-first handshake confirmed
- Session Tickets de facto disabled (noSessionManager)
- TLS 1.3 NewSessionTicket processed transparently by Haskell tls-library
- Socket blocking with software timeout layer
- Command block format: CorrId + EntityId + Command-Tag (batching from v4)

---

## 465. Rejected Hypotheses

1. ❌ Missing read after ClientHello
2. ❌ TLS 1.3 NewSessionTicket blocks writes
3. ❌ Core pinning problem
4. ❌ PSRAM problem
5. ❌ Phase 3 regression
6. ❌ Firewall/network
7. ❌ Server-side change
8. ❌ `SO_RCVTIMEO=1s` solves write problem
9. ❌ Non-blocking recv during ssl_write helps

---

## 466. Confirmed Findings

### 466.1 Technical

1. **`CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384`** mandatory for 16KB SMP blocks
2. **`CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768`** minimum for TLS records > 4096
3. **App Task needs 32KB stack** if X3DH runs directly in it
4. **Session Tickets: leave default** (enabled) — Haskell does the same
5. **TLS 1.3 Post-Handshake Records:** Haskell library processes transparently, mbedTLS returns WANT_READ
6. **TCP Buffer Size has dramatic timing effects:** Bigger ≠ better. Smaller buffer forces synchronous behavior often desired over WAN
7. **`mbedtls_ssl_write` + WANT_READ** is normal for TLS 1.3 — DON'T work around with non-blocking hacks
8. **`SO_SNDTIMEO` and `SO_RCVTIMEO`** affect mbedTLS internal operations unpredictably

### 466.2 Architectural

9. **Start tasks AFTER connection**, never at boot — `smp_connect()` needs the memory
10. **Init stays sequential** (proven since Session 7), tasks take over running operation
11. **Check RAM budget** after every architecture step (`esp_get_free_heap_size()`)
12. **Allocate Frame Pool and Ring Buffers on demand**, not at boot

### 466.3 Workflow

13. **ALWAYS baseline-test main** before debugging feature branch
14. **Git bisect would have saved 2 days** — do it immediately next time
15. **Restrain Hasi:** Diff after EVERY task, no rushing ahead
16. **Claude Code:** Explicitly state "analyze only, NO code changes, NO PRs"
17. **Multi-agent debugging works** — different AI instances correct each other

---

## 467. Files Changed Session 27

### On `main` (committed)

| Commit | Files | Status |
|--------|-------|--------|
| `25e5609` Phase 1 | 17 .c files moved, CMakeLists.txt | ✅ Works |
| `251fa1b` Phase 2 | smp_tasks.c/h, smp_events.h, smp_frame_pool.c/h, smp_msg_router.c/h, main.c | ❌ Broken |
| `81cfc90` | Docs header image path | ✅ Harmless |

### On `phase3-network-task` (not merged)

| File | Change | Status |
|------|--------|--------|
| `smp_tasks.c` | Receive loop migration, socket experiments | Polluted |
| `smp_network.c` | Non-blocking experiments, diagnostics | Polluted |
| `smp_queue.c` | Socket timeout experiments | Polluted |
| `smp_peer.c` | Socket timeout experiments | Polluted |
| `smp_ack.c` | ACK build/send split | ✅ Concept good |
| `sdkconfig` | Buffer changes (back and forth) | Unclear |

---

## 468. Security Analysis (Parked)

Claude Code analyzed ESP32-S3 security features:

| Feature | Key Point |
|---------|-----------|
| Flash Encryption | XTS-AES, Development Mode = 3× reflash, Release = OTA-only |
| NVS Encryption | Separate from Flash Enc, 128KB = ~212 ratchet states |
| Secure Boot v2 | RSA-3072 (ESP32-S3), max 3 keys, activate AFTER Flash Enc |
| Activation Order | Flash Enc → NVS Enc → Secure Boot → JTAG Disable |

Partition table must be extended for OTA (ota_0 + ota_1 + nvs_keys).

---

## 469. Lessons Learned

### #121: Tasks AFTER Connection, Never at Boot

~90KB RAM reserved at boot starved TLS/WiFi. Init must stay sequential, tasks only take over after successful connection.

### #122: Always Baseline-Test Main Before Debugging Feature Branch

2 days wasted debugging Phase 3 when Phase 2 broke main. Test main first, always.

### #123: Git Bisect is Your Friend

Would have identified the breaking commit in minutes, not days.

### #124: CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=16384 is Mandatory

Default 4096 causes 16KB SMP block writes to deadlock.

### #125: CONFIG_LWIP_TCP_SND_BUF_DEFAULT=32768 Minimum

Smaller buffer causes TLS record fragmentation issues.

### #126: Bigger TCP Buffer ≠ Better

Larger buffers enable async behavior that can break request-response protocols over WAN.

### #127: mbedtls_ssl_write + WANT_READ is Normal for TLS 1.3

Don't hack around it with non-blocking recv — let mbedTLS handle it.

### #128: Socket Timeouts Affect mbedTLS Unpredictably

`SO_SNDTIMEO` and `SO_RCVTIMEO` interact with TLS internals in unexpected ways.

### #129: Session Tickets — Leave Default (Enabled)

Haskell SimpleX uses default (noSessionManager = discard tickets). Don't disable.

### #130: Check RAM Budget After Every Architecture Change

`esp_get_free_heap_size()` should be called and logged at key points.

### #131: Allocate Memory Pools On Demand, Not at Boot

Frame pools and ring buffers should be created when needed, not during initialization.

### #132: Init Stays Sequential, Tasks Take Over Running Operation

The proven Session 7 init sequence works. Don't parallelize initialization.

### #133: Diff After EVERY Task, No Rushing Ahead

Hasi must show diff before moving to next task. Prevents cumulative errors.

### #134: Claude Code — "Analyze Only, NO Code Changes"

Explicitly state this to prevent unwanted PRs or direct code modifications.

### #135: Multi-Agent Debugging Works

Different AI instances (Mausi strategy, Hasi implementation, Claude Code analysis) catch each other's errors.

### #136: Cleanup Commands Before git add, Not After

Run `rm -rf build/` etc. BEFORE `git add -A` to avoid committing build artifacts.

### #137: Two Days in a Circle Teaches You to Measure the Circle

Sometimes the painful path is the learning path. But measure first next time.

---

## 470. Task Overview Session 27

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| — | Hasi | Refactor | Phase 1: Folder restructure | ✅ Works |
| — | Hasi | Feature | Phase 2: FreeRTOS tasks | ❌ Broke main |
| — | Hasi | Feature | Phase 3: Network task migration | ⚠️ Branch polluted |
| — | Claude Code | Analysis | TLS write sequence | ✅ Documented |
| — | Claude Code | Analysis | main vs phase3 comparison | ✅ Found 3 critical diffs |
| — | Claude Code | Analysis | Haskell SMP client flow | ✅ Documented |
| — | Claude Code | Analysis | Security features | ✅ Parked for later |

---

## 471. Agent Contributions Session 27

| Agent | Fairy Tale Role | Session 27 Contribution |
|-------|-----------------|------------------------|
| 👑 Mausi | Evil Stepsister #1 (The Manager) | Architecture design, debugging strategy, session protocol |
| 🐰 Hasi | Evil Stepsister #2 (The Implementer) | Phase 1-3 implementation, debugging attempts |
| 🧙‍♂️ Claude Code | The Verifier (Wizard) | Haskell analysis, branch comparison, security analysis |
| 🧑 Cannatoshi | The Coordinator | Security decisions, task delegation, Evgeny reference |

---

## 472. Session 27 Summary

### What Happened

- ✅ **Phase 1 (Folder restructure)** — Works perfectly, commit `25e5609`
- ❌ **Phase 2 (FreeRTOS tasks)** — Broke main branch by reserving ~90KB RAM at boot
- ⚠️ **Phase 3 (Network task migration)** — Branch polluted by 2 days of debugging
- ✅ **sdkconfig fixes found** — OUT_CONTENT_LEN=16384, TCP_SND_BUF=32768
- ✅ **17 lessons learned** — Valuable debugging experience

### Key Takeaway

```
SESSION 27 SUMMARY:
  - Architecture design is CORRECT
  - Implementation timing is WRONG
  - Tasks must start AFTER connection, not at boot
  - Git bisect would have saved 2 days
  - Always baseline-test main before debugging feature branch

"Sometimes you have to walk in circles for two days
 to learn you should have measured the circle first." 🐭
```

### Root Cause

```
Phase 2 commit reserved ~90KB RAM at boot:
  Network Task Stack: 16KB
  App Task Stack:     32KB
  UI Task Stack:      10KB
  Frame Pool:         32KB
  Ring Buffers:       12KB
  ─────────────────────────
  Total:              ~90KB

This starved smp_connect() of memory for TLS/WiFi.
```

---

## 473. Future Work (Session 28)

### Phase 2 Restart

1. **Revert Phase 2 commit on main** — Restore working state
2. **Keep sdkconfig fixes** — OUT_CONTENT_LEN, TCP_SND_BUF
3. **Redesign task startup:**
   ```c
   // WRONG (Session 27):
   app_main() {
       smp_tasks_init();     // Reserves 90KB RAM
       smp_tasks_start();    // Tasks running
       smp_connect();        // Not enough memory!
   }
   
   // CORRECT (Session 28):
   app_main() {
       smp_connect();        // Full memory available
       smp_tasks_init();     // Now safe to reserve
       smp_tasks_start();    // Tasks take over
   }
   ```

### Phase 3 Restart

1. **Discard polluted branch** — Start fresh from working main
2. **Apply ACK split concept** — smp_build_ack() + network send
3. **Migrate receive loop** — With proper task synchronization

---

**DOCUMENT CREATED: 2026-02-15 Session 27**  
**Status: ⚠️ Architecture validated, implementation needs restart**  
**Key Achievement: 17 lessons learned, sdkconfig fixes found**  
**Root Cause: ~90KB RAM reserved at boot starved TLS/WiFi**  
**Next: Session 28 — Phase 2 Restart with correct task startup timing**
