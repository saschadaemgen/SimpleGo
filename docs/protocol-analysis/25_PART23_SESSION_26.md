![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 23: Session 26
# 🗄️ Persistence & Storage Architecture

**Document Version:** v1  
**Date:** 2026-02-14 Session 26 (Valentine's Day Part 2)  
**Status:** ✅ MILESTONE 6: Ratchet State Persistence!  
**Previous:** Part 22 - Session 25 (Bidirectional Chat + Receipts)

---

## 🗄️ MILESTONE 6 ACHIEVED!

```
═══════════════════════════════════════════════════════════════════════════════

  🗄️🗄️🗄️ RATCHET STATE PERSISTENCE! 🗄️🗄️🗄️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   ESP32 survives reboot without losing cryptographic state!            │
  │                                                                         │
  │   - Ratchet state restored from NVS flash                              │
  │   - Queue credentials persisted                                        │
  │   - Peer connection state saved                                        │
  │   - Messages decrypt after power cycle!                                │
  │                                                                         │
  │   Date: February 14, 2026                                              │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Write-Before-Send: 7.5ms verified                                    │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 439. Session 26 Overview

Session 26 focused on giving the ESP32 persistent memory — the ability to survive power loss and reboots without losing cryptographic state. This was achieved through a custom NVS-based storage module, culminating in Milestone 6 (Ratchet State Persistence). Additionally, the first steps toward a real messenger UI were taken with keyboard integration and a send-flow prototype.

### 439.1 Session Statistics

| Metric | Value |
|--------|-------|
| Duration | ~6 hours |
| Tasks completed | 50a, 50a-CC, 50b, 50c |
| Tasks in progress | 50d (Tasks 1-5 of 6) |
| Milestones reached | Milestone 6 🏆 |
| New files created | 6 |
| Files modified | 8 |
| Lines added | ~1200 (estimated) |
| NVS write timing | 7.5ms (verified) |
| Contacts supported | 150+ |
| Ratchet state size | 520 bytes |
| NVS partition size | 128KB (expanded from 24KB) |

---

## 440. Storage Architecture Design

### 440.1 The Problem

Before Session 26, the ESP32 stored everything in RAM. Every reboot meant complete amnesia: ratchet state gone, queue credentials gone, contact data gone. For a Kickstarter product this is unacceptable. The core challenge was threefold:

**Messages:** Chat history needed persistence (limited by flash capacity).

**Connection Data:** Queue IDs, server addresses, auth keys needed to survive reboots to reconnect.

**Ratchet State:** The Double Ratchet must stay synchronized between ESP32 and the SimpleX app. If ESP32 forgets its state but the app continues, cryptographic desync occurs and the conversation is permanently dead.

### 440.2 SQLite vs Custom Format Decision

SimpleX Chat uses SQLite on mobile devices (8GB RAM, 256GB storage). For the ESP32-S3 with 512KB SRAM and 8MB flash, SQLite would consume 300-500KB of code size and significant RAM for query parsing, B-tree operations, and page cache.

**Decision:** Custom format using ESP-IDF native NVS (Non-Volatile Storage) for crypto-critical data plus flat files on SD card for message history.

Rationale:
- NVS is a key-value store with built-in wear-leveling, optimized for flash, minimal RAM usage
- SD card provides simple encrypted file storage, portable between devices
- No SQL overhead, no query parser, no B-tree in RAM

### 440.3 Storage Architecture

```
NVS (Internal Flash, encrypted via ESP32 Flash Encryption)
├── Identity Keys
├── Ratchet State (per contact, 520 bytes each)
├── Queue Credentials (~300 bytes per contact)
├── Peer Connection State
└── Device Config

SD Card (External MicroSD, AES-encrypted files)
├── Message History
├── Contact Profiles
├── File Attachments (future XFTP)
└── Exportable & portable between devices

Future: Secure Element (Tier 2/3 devices)
└── Identity Keys migrate here for hardware protection
```

### 440.4 Capacity Analysis

**NVS (128KB partition):**

| Data per contact | Size |
|------------------|------|
| Ratchet State (ratchet_state_t) | 520 bytes (measured) |
| Queue Credentials | ~300 bytes (estimated) |
| Total per contact | ~820 bytes |
| **128KB NVS capacity** | **~150+ contacts** |

**SD Card (128GB MicroSD, maximum supported):**

| Message type | Average size | Capacity on 128GB |
|--------------|--------------|-------------------|
| Text messages | ~500 bytes | ~256 million |
| Voice messages (1 min, Opus codec) | ~1 MB | ~128,000 |
| Photos (JPEG compressed) | ~3 MB | ~42,000 |
| Documents (PDF/Office) | ~500 KB | ~256,000 |

**Realistic mixed usage scenario:** 50 texts + 5 photos + 3 voice messages + 1 document per day = ~18.5MB/day. A 128GB card lasts approximately **19 years**.

ESP32 supports SDHC (up to 64GB) and SDXC (128GB+) cards via SPI interface.

---

## 441. Evgeny's Golden Rule: Write-Before-Send

### 441.1 Origin

From the evgeny_reference.md "Lost Responses" section, Evgeny Poberezkin (SimpleX founder) stated:

> "Whatever you do for networking, make sure to handle lost responses — that was the biggest learning."
> "For us it was a large source of bugs."

### 441.2 The Pattern

```
Generate key → Persist to flash → THEN send → If response lost → Retry with SAME key
```

If ESP32 generates a key, sends it to the server, but the response is lost (WiFi glitch, timeout), the ESP32 doesn't know if the server processed the command. If it generates a NEW key and retries, server state != ESP32 state = cryptographic desync.

Solution: Persist the key BEFORE sending. If response is lost, retry with the SAME key. This makes the operation idempotent.

### 441.3 Implementation

The `smp_storage_save_blob_sync()` function implements this pattern:
1. Write to NVS
2. Commit (flush to flash)
3. Verify read-back (read the data back and compare byte-for-byte)
4. Only then return success

Measured performance: **7.5ms** for a 256-byte verified write. This is negligible compared to network latency (typically 50-200ms).

### 441.4 Affected Operations

This pattern applies to all operations where keys are generated before a network send:
- SKEY operations (queue creation)
- LKEY operations (link generation)
- Ratchet state after encrypt (before the encrypted message is sent to server)

---

## 442. Two-Phase Init Architecture

### 442.1 The SPI Bus Conflict

The T-Deck shares one SPI bus (SPI2_HOST) between the display and the SD card slot. When the storage module tried to initialize the SPI bus for SD card access before the display, it conflicted:

```
E (1659) spi: spi_bus_initialize(810): SPI bus already initialized.
E (1659) SMP: Display init failed: ESP_ERR_INVALID_STATE
```

The display went black because the bus was already claimed by our SD card init.

### 442.2 The Solution

Two-Phase Init architecture where the display "owns" the SPI bus:

**Phase 1 (before display):** Initialize NVS only. No SPI access, no bus conflict. This is the critical path for Write-Before-Send — NVS must be ready before any network operations.

**Phase 2 (after display):** Mount SD card on the existing SPI bus that the display already initialized. No own `spi_bus_initialize()` call — just add the SD device to the existing bus with its own CS pin.

```c
app_main() {
    nvs_flash_init();
    smp_storage_init();        // Phase 1: NVS only, instant ready
    smp_storage_print_info();
    smp_storage_self_test();

    tdeck_display_init();      // Display owns SPI bus
    tdeck_lvgl_init();
    // ... UI setup ...

    smp_storage_init_sd();     // Phase 2: SD mounts on existing bus
}
```

### 442.3 T-Deck SPI Pin Layout

```
Display:  SPI2_HOST, SCLK=GPIO40, MOSI=GPIO41, MISO=GPIO38, CS=GPIO12
SD Card:  SPI2_HOST (shared), CS=GPIO39
```

The display and SD card share SCLK, MOSI, MISO but have separate CS (Chip Select) pins. The SPI protocol handles bus sharing automatically via CS.

### 442.4 HAL Compatibility

If a different device has no display driver (e.g. headless gateway), Phase 2 can initialize the SPI bus itself. The Two-Phase approach is HAL-compatible across all hardware tiers.

---

## 443. NVS Partition Expansion

### 443.1 Change

The NVS partition was expanded from 24KB (0x6000) to 128KB (0x20000) in `partitions.csv`:

**Before:**
```
nvs,      data, nvs,     0x9000,   0x6000,
phy_init, data, phy,     0xf000,   0x1000,
factory,  app,  factory, 0x10000,  0x1F0000,
```

**After:**
```
nvs,      data, nvs,     0x9000,   0x20000,
phy_init, data, phy,     0x29000,  0x1000,
factory,  app,  factory, 0x30000,  0x1D0000,
```

### 443.2 Rationale

24KB was insufficient for storing ratchet states (520 bytes each), queue credentials, skipped message keys, and contact data for multiple contacts. 128KB provides comfortable capacity for 150+ contacts.

The factory app partition was reduced from 0x1F0000 to 0x1D0000, still providing 1.8MB for the application binary (current binary is ~1.6MB).

### 443.3 Measured NVS Statistics

After boot with the expanded partition:
```
Entries: used=447, free=3585, total=4032, ns_count=6
```

---

## 444. Self-Test Suite

### 444.1 Test A: NVS Basic Roundtrip

Writes 256 bytes of random data to NVS, verifies exists(), reads back, compares byte-for-byte, deletes, verifies deletion.

**Result: PASSED** ✅

### 444.2 Test B: Write-Before-Send Timing

Uses `smp_storage_save_blob_sync()` to write 256 bytes with full verify-read-back cycle. Measures total time including write, commit, and verification.

**Result: PASSED — 7.5ms** ✅

Assessment thresholds:
- Under 5ms: Excellent
- Under 20ms: Good (acceptable for Write-Before-Send)
- Over 20ms: Slow (may impact real-time feel)

### 444.3 Test C: SD Card Roundtrip

Writes, verifies exists, reads back, compares, deletes test file on SD card.

**Result: SKIPPED** (no SD card inserted during testing — this is expected and non-fatal)

---

## 445. Milestone 6: Ratchet State Persistence

### 445.1 What Was Implemented

**Ratchet State Save/Load (smp_ratchet.c):**
- `ratchet_save_state(contact_idx)` — Saves ratchet_state_t (520 bytes) to NVS key "rat_XX"
- `ratchet_load_state(contact_idx)` — Loads from NVS with 3-fold validation (size, length, initialized flag)
- Uses save_blob_sync everywhere (7.5ms overhead is negligible, unified codepath is safer)
- Loads into local variable first, validates, then memcpy to global state (prevents corruption)

**Save-Points inserted at 4 locations:**

| Point | Location | When | Why |
|-------|----------|------|-----|
| R2 | ratchet_init_sender() | After initialized=true | Initial state after X3DH must be saved |
| R3 | ratchet_encrypt() | After chain_key advance, before return | Evgeny's Rule: persist BEFORE network send |
| R4/R5 | ratchet_decrypt_body() | After ADVANCE or SAME state update | State changed after successful decrypt |

R4 and R5 are combined into one save call after the if/else block since both ADVANCE and SAME modes converge at the same point.

**Queue Credentials Save/Load (smp_queue.c):**
- `queue_save_credentials()` / `queue_load_credentials()`
- Includes E2E key pair
- NVS key: "queue_our"

**Contact Credentials Save/Load (smp_contacts.c):**
- `contact_save_credentials(idx)` / `contact_load_credentials(idx)`
- NVS key: "cont_XX"
- Already had save_contacts_to_nvs() — extended with full credential set

**Boot Sequence (main.c):**
```c
if (smp_storage_exists("rat_00") && smp_storage_exists("queue_our")) {
    // Restore previous session
    ratchet_load_state(0);
    queue_load_credentials();
    contact_load_credentials(0);
    // Skip handshake, go directly to subscribe + message loop
} else {
    // Fresh start — full handshake
    clear_all_contacts();
    // Normal flow
}
```

### 445.2 The Historic Moment

```
Ratchet state restored: 'rat_00' (520 bytes) | send=1 recv=1     ✅
E2E peer key restored (valid=1)                                    ✅
E2E LAYER 2 DECRYPT SUCCESS!                                      ✅
HEADER DECRYPT SUCCESS! (AdvanceRatchet)                           ✅
PHASE 2b BODY DECRYPT SUCCESS!                                    ✅
"Test Two"                                                         ✅
Ratchet state saved: 'rat_00' | send=0 recv=1                     ✅
```

The message "Test Two" was sent from the SimpleX app AFTER the ESP32 had been rebooted. The ESP32 restored its ratchet state from NVS, re-subscribed to the existing queues, and decrypted the message without any new handshake or key exchange.

### 445.3 Deprecated Function Warning

The older `ratchet_decrypt()` function (not used in current flow, superseded by `ratchet_decrypt_body()`) now logs a deprecation warning if called:

```c
ESP_LOGW(TAG, "DEPRECATED: ratchet_decrypt() called — use ratchet_decrypt_body() instead");
```

This helps detect if any code path still uses the old function.

---

## 446. Peer Persistence (Task 50c)

### 446.1 The Problem

After achieving Milestone 6, delivery receipts failed after reboot:

```
E (26935) SMP_PEER: peer_send_receipt: no saved peer host/port!
```

The peer SMP server information (host, port, send ID, auth keys) was only stored in RAM.

### 446.2 Solution

- `peer_save_state()` / `peer_load_state()` implemented
- Peer server host, port, SndID, and auth keys persisted in NVS key "peer_00"
- Added to boot restore sequence

### 446.3 Result

Delivery receipts (✓✓) now work after reboot, even after multiple consecutive reboots. Verified with two sequential reboot tests — both showed ✓✓ in the SimpleX app.

---

## 447. Keyboard Integration & Send Flow (Task 50d, partial)

### 447.1 Keyboard Driver

T-Deck keyboard communicates via I2C at address 0x55. A FreeRTOS task polls at 50ms intervals. Each keypress returns one byte. The T-Deck firmware handles debouncing internally.

New files:
- `devices/t_deck_plus/hal_impl/tdeck_keyboard.c` + `.h`

### 447.2 Chat Screen

New chat screen with Cyberpunk theme, message bubbles, and textarea.

New files:
- `main/ui/screens/ui_chat.c` + `.h`

### 447.3 Send Flow Status

Keyboard input collected → message encrypted → sent via SMP SEND command → app receives message. This works but has issues:

**Working:** Keyboard → Serial Monitor → Enter → Message arrives in SimpleX app ✅

**Not working:**
- Chat screen never displayed (UI shows "No active Chats")
- Typed text only visible in Serial Monitor, not on display
- Main loop blocks up to 5 seconds in smp_read_block()
- No message bridge from SMP thread to LVGL display
- "bad message ID" after reboot (send counter not persisted)

### 447.4 "bad message ID" Problem

After reboot, the SMP server rejects messages from the ESP32 because the message ID counter restarts at an unexpected value. The counter must be persisted in NVS alongside the queue credentials.

### 447.5 Architecture Assessment

The current codebase is still a "test laboratory" — everything runs in a single FreeRTOS task with a blocking main loop. For a real messenger, three separate tasks are needed:

1. **SMP Receive Task** (blocking socket read)
2. **SMP Send Task** (queue-based, processes outgoing messages)
3. **LVGL UI Task** (display updates, touch input, keyboard input)

This architectural transformation is planned for Session 27.

---

## 448. Hardware & Platform Analysis

### 448.1 ESP32 Family Comparison

| Chip | CPU | RAM | Clock | Notes |
|------|-----|-----|-------|-------|
| ESP32-S3 (current) | Dual Xtensa LX7 | 512KB + 8MB PSRAM | 240 MHz | T-Deck platform |
| ESP32-P4 (new 2025) | Dual RISC-V | 768KB + 32MB PSRAM | 400 MHz | ~3x performance, no built-in WiFi |
| ESP32-C6 | Single RISC-V | 512KB | 160 MHz | WiFi 6, lower power |

### 448.2 STM32 Comparison (Tier 2/3)

| Chip | CPU | RAM | Clock | Key advantage |
|------|-----|-----|-------|---------------|
| STM32U585 (Tier 2) | Cortex-M33 | 786KB | 160 MHz | TrustZone hardware isolation |
| STM32U5A9 (Tier 3) | Cortex-M33 | 2.5MB + 4MB Flash | 160 MHz | Massive storage, DPA-resistant crypto |

STM32 advantages over ESP32 are primarily security (TrustZone, DPA-resistant hardware crypto, deterministic timing) rather than raw performance.

### 448.3 Group Chat Feasibility

SimpleX groups are mesh-based (no central server). Each member has pairwise connections to every other member.

| Group size | Connections on ESP32 | NVS requirement | RAM for queues | Feasible? |
|------------|----------------------|-----------------|----------------|-----------|
| 5 members | 4 | ~3.3 KB | ~16 KB | Yes, easy |
| 10 members | 9 | ~7.4 KB | ~36 KB | Yes |
| 20 members | 19 | ~15.6 KB | ~76 KB | Tight |
| 50 members | 49 | ~40 KB | ~196 KB | Not recommended |

**Practical limit for ESP32: groups up to 10 members.**

### 448.4 XFTP File Transfer Feasibility

XFTP splits files into chunks (64-256KB), encrypts each chunk individually, uploads to XFTP servers. ESP32 has sufficient processing power for chunk handling with PSRAM. SD card serves as local storage.

**Practical file size limit on ESP32: ~10-20MB per file.** Larger files are better handled on STM32 platforms.

Voice messages are feasible: Opus codec runs on ESP32, T-Deck has a built-in microphone. One minute of Opus audio is approximately 1MB.

---

## 449. Evgeny Communication

### 449.1 Update Sent

A progress update was sent to Evgeny Poberezkin covering:
- Milestone 6 achievement (Ratchet State Persistence)
- His golden rule (persist before send) credited as key to the implementation
- SD card as encrypted portable message store
- XFTP file transfer plans with voice message potential
- Capacity numbers (150+ contacts, 256M texts on 128GB, 19 years mixed use)
- Three-tier storage architecture scaling

### 449.2 Advisory Board Status

Evgeny's advisory board offer (~0.5% equity) remains open. No formal acceptance yet. The relationship positions SimpleGo as a potential official hardware partner for the SimpleX ecosystem.

---

## 450. Files Changed Session 26

### New Files

| File | Path | Description |
|------|------|-------------|
| smp_storage.c | main/smp_storage.c | Storage module (NVS + SD, two-phase init) |
| smp_storage.h | main/include/smp_storage.h | Storage API header |
| tdeck_keyboard.c | devices/t_deck_plus/hal_impl/ | I2C keyboard driver |
| tdeck_keyboard.h | devices/t_deck_plus/hal_impl/ | Keyboard driver header |
| ui_chat.c | main/ui/screens/ | Chat screen with Cyberpunk theme |
| ui_chat.h | main/ui/screens/ | Chat screen header |

### Modified Files

| File | Changes |
|------|---------|
| main.c | Two-phase storage init, FreeRTOS keyboard task, session restore logic |
| smp_ratchet.c | Added ratchet_save_state/load_state, 4 save-call insertions, deprecation warning |
| smp_ratchet.h | New function declarations for save/load |
| smp_peer.c | Added peer_save_state/load_state |
| smp_queue.c | Added queue_save_credentials/load_credentials |
| smp_contacts.c | Extended credential save/load |
| CMakeLists.txt | Added sdmmc, fatfs, vfs dependencies, smp_storage.c source |
| partitions.csv | NVS expanded from 24KB to 128KB |

---

## 451. NVS Key Schema

```
rat_XX       Ratchet State for contact index XX (520 bytes)
queue_our    Our queue credentials (rcv_id, snd_id, auth keys, DH secrets)
cont_XX      Contact credentials for index XX
peer_XX      Peer connection state (host, port, snd_id, auth keys)
test_rt      Self-test key (created and deleted during test A)
test_wbs     Self-test key (created and deleted during test B)
```

XX = two-digit contact index (00-31, extensible to 99).
Maximum NVS key length: 15 characters.

---

## 452. Lessons Learned

### #113: Write-Before-Send is Architectural, Not Optional

Evgeny's pattern is not a "nice to have" — it prevents state desync bugs that plagued SimpleX development for months. The 7.5ms overhead for a verified write is negligible compared to the hours of debugging that unhandled lost responses would cause.

### #114: ESP32 is Not a Smartphone — Don't Use Smartphone Patterns

SQLite on ESP32 would be wasteful. NVS + SD card is the correct embedded architecture. Custom formats beat general-purpose databases when you know exactly what you're storing.

### #115: SPI Bus Sharing Requires Ownership Model

On embedded systems with shared buses, one device must "own" the bus initialization. Other devices attach to the existing bus. Two-Phase Init is the clean pattern for this.

### #116: Validate Before You Memcpy

Loading persistent state into a local variable first, validating size/length/flags, then memcpy-ing to global state prevents corruption from truncated or garbage data. This pattern should be used everywhere persistent data is loaded.

### #117: Unified Save Strategy Beats Dual Codepaths

Using save_blob_sync (with verify) everywhere instead of having sync and non-sync variants for different save-points reduces complexity. The 7.5ms cost is always acceptable.

### #118: Test Laboratory != Product Architecture

A blocking main loop works for protocol testing but cannot support a responsive UI. The transition to a multi-task FreeRTOS architecture is inevitable and should not be delayed further.

### #119: SD Card Portability is a Product Feature

The ability to swap an encrypted SD card between SimpleGo devices and migrate chat history is a Kickstarter-worthy feature. Security through password-based encryption similar to SimpleX Chat's database encryption approach.

### #120: Role Discipline in Multi-Agent Workflow

When Mausi (strategy) starts writing code, the workflow breaks down. Each agent must stay in their lane: Mausi coordinates, Hasi implements, Aschenputtel analyzes. Crossing roles leads to confusion and missed handoffs.

---

## 453. Task Overview Session 26

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| 50a | Hasi | Feature | NVS storage module implementation | ✅ Two-phase init working |
| 50a-CC | Claude Code | Analysis | Storage architecture design | ✅ NVS + SD card design |
| 50b | Hasi | Feature | Ratchet state persistence | ✅ **MILESTONE 6!** |
| 50c | Hasi | Feature | Peer state persistence | ✅ Receipts work after reboot |
| 50d | Hasi | Feature | Keyboard + send flow | ⚠️ Partial (Tasks 1-5 of 6) |

---

## 454. Agent Contributions Session 26

| Agent | Fairy Tale Role | Session 26 Contribution |
|-------|-----------------|------------------------|
| 👑 Mausi | Evil Stepsister #1 (The Manager) | Storage architecture design, Write-Before-Send pattern |
| 🐰 Hasi | Evil Stepsister #2 (The Implementer) | NVS module, ratchet/peer/queue persistence, keyboard driver |
| 🧙‍♂️ Claude Code | The Verifier (Wizard) | Capacity analysis, platform comparison, group chat feasibility |
| 🧑 Cannatoshi | The Coordinator | Task distribution, Evgeny communication, Git commits |

---

## 455. Open Questions for Session 27

1. **SEND Message-ID Sequence:** How exactly does the SMP server track message IDs? Per-queue counter? Server-assigned? Client-assigned? This must be understood to fix the "bad message ID" problem.

2. **Agent Protocol A_MSG Wrapping (Outbound):** What is the exact binary format for wrapping a plaintext message into an Agent Protocol A_MSG for sending? We can parse incoming A_MSG but need the inverse for sending.

3. **Connection Rotation:** How and when does SimpleX rotate queue connections for forward secrecy? Does this affect our persistent state?

4. **Queue Rotation:** Similar to connection rotation — do queues get replaced over time? How should the ESP32 handle this?

5. **FreeRTOS Task Stack Sizes:** What are appropriate stack sizes for SMP Receive, SMP Send, and LVGL UI tasks? Need to balance RAM usage with stack overflow prevention.

6. **LVGL Thread Safety:** What mutex pattern should we use for LVGL when multiple tasks need to update the display?

---

## 456. Session 26 Summary

### What Was Achieved

- 🗄️ **MILESTONE 6: Ratchet State Persistence!**
- ✅ **NVS Storage Module** — Two-phase init, 7.5ms verified writes
- ✅ **Partition Expanded** — 24KB → 128KB (150+ contacts)
- ✅ **Write-Before-Send** — Evgeny's golden rule implemented
- ✅ **Peer Persistence** — Receipts work after reboot
- ⚠️ **Keyboard Integration** — Partial (UI not connected)

### Key Takeaway

```
SESSION 26 SUMMARY:
  - 🗄️ RATCHET STATE PERSISTENCE ACHIEVED!
  - ESP32 survives reboot without losing crypto state
  - Write-Before-Send: 7.5ms verified writes
  - NVS capacity: 150+ contacts
  - SD card: 19 years of mixed usage on 128GB
  - Delivery receipts work after multiple reboots
  - Keyboard works but UI architecture needs refactor

"From volatile RAM to persistent flash — the ESP32 remembers."
"Evgeny's golden rule: persist before you send." 🐭🐰🧙‍♂️
```

---

## 457. Future Work (Session 27)

### Phase 1: Fix "bad message ID"
- Persist send counter in NVS
- Understand SMP message ID sequence

### Phase 2: Multi-Task Architecture
- SMP Receive Task (blocking socket read)
- SMP Send Task (queue-based outgoing)
- LVGL UI Task (display, touch, keyboard)

### Phase 3: Complete Chat UI
- Connect keyboard input to chat screen
- Message bridge between SMP and LVGL tasks
- Real-time message display

---

**DOCUMENT CREATED: 2026-02-14 Session 26**  
**Status: ✅ MILESTONE 6: Ratchet State Persistence!**  
**Key Achievement: ESP32 survives reboot without losing crypto state**  
**Open: Message ID persistence, multi-task architecture**  
**Next: Session 27 — Messenger Architecture & Send Protocol**
