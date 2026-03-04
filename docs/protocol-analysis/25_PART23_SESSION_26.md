![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 23: Session 26
# Persistence and Storage Architecture

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-14
**Status:** COMPLETED -- Milestone 6: Ratchet State Persistence
**Previous:** Part 22 - Session 25
**Next:** Part 24 - Session 27
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 26 SUMMARY

```
Session 26 gave the ESP32 persistent memory. A custom NVS-based
storage module with Two-Phase Init (NVS before display, SD card after
display) handles the T-Deck's shared SPI bus. Ratchet state (520
bytes per contact), queue credentials, and peer connection state are
persisted in NVS with 7.5ms verified writes implementing Evgeny's
"Write-Before-Send" pattern. NVS partition expanded from 24KB to
128KB (150+ contacts). Messages decrypt after power cycle. Delivery
receipts work after multiple reboots. Keyboard driver integrated
(I2C at 0x55), chat screen prototype created, but UI not yet
connected to message pipeline.

 Milestone 6: Ratchet State Persistence (survives reboot)
 NVS storage module with Two-Phase Init
 Write-Before-Send pattern: 7.5ms verified writes
 NVS partition: 24KB -> 128KB (150+ contacts)
 Peer persistence: receipts work after reboot
 Keyboard driver integrated (partial UI)
```

---

## Storage Architecture

SQLite is too heavy for ESP32-S3 (300-500KB code, significant RAM). Custom architecture uses NVS for crypto-critical data plus SD card for message history.

```
NVS (Internal Flash, 128KB)
  Identity keys, ratchet state (520B/contact), queue credentials,
  peer connection state, device config

SD Card (External MicroSD, AES-encrypted files)
  Message history, contact profiles, file attachments

Future: Secure Element (Tier 2/3)
  Identity key migration for hardware protection
```

Capacity: NVS supports 150+ contacts. A 128GB SD card at mixed usage (50 texts + 5 photos + 3 voice + 1 document per day) lasts approximately 19 years.

---

## Two-Phase Init

The T-Deck shares SPI2_HOST between display (CS=GPIO12) and SD card (CS=GPIO39). Initializing SD first claims the bus and breaks the display.

**Phase 1 (before display):** NVS only. No SPI access. Ready for Write-Before-Send before any network operations.

**Phase 2 (after display):** SD card mounts on the existing SPI bus that the display initialized. No own `spi_bus_initialize()` call.

```c
app_main() {
    nvs_flash_init();
    smp_storage_init();        // Phase 1: NVS only
    tdeck_display_init();      // Display owns SPI bus
    smp_storage_init_sd();     // Phase 2: SD on existing bus
}
```

---

## Write-Before-Send (Evgeny's Golden Rule)

```
Generate key -> Persist to flash -> THEN send -> If response lost -> Retry with SAME key
```

`smp_storage_save_blob_sync()` writes to NVS, commits, and verifies read-back (byte-for-byte comparison). Measured: 7.5ms for 256-byte verified write, negligible compared to 50-200ms network latency.

Applies to: SKEY/LKEY operations, ratchet state after encrypt (before SEND).

---

## Ratchet State Persistence

Save-points at four locations:

| Point | Location | When |
|-------|----------|------|
| R2 | ratchet_init_sender() | After initialized=true |
| R3 | ratchet_encrypt() | After chain_key advance, before return |
| R4/R5 | ratchet_decrypt_body() | After ADVANCE or SAME state update |

Load sequence at boot:

```c
if (smp_storage_exists("rat_00") && smp_storage_exists("queue_our")) {
    ratchet_load_state(0);       // 520 bytes, 3-fold validation
    queue_load_credentials();
    contact_load_credentials(0);
    // Skip handshake, subscribe directly
}
```

Loads into local variable first, validates size/length/flags, then memcpy to global state (prevents corruption from truncated data).

---

## NVS Key Schema

```
rat_XX       Ratchet State for contact index XX (520 bytes)
queue_our    Our queue credentials
cont_XX      Contact credentials for index XX
peer_XX      Peer connection state (host, port, snd_id, auth keys)
```

XX = two-digit contact index (00-99). Maximum NVS key length: 15 characters.

---

## NVS Partition Change

```
Before: nvs 0x9000 0x6000 (24KB)
After:  nvs 0x9000 0x20000 (128KB)
```

Factory app partition reduced from 0x1F0000 to 0x1D0000 (1.8MB, sufficient for current ~1.6MB binary).

---

## Self-Test Results

| Test | Description | Result |
|------|-------------|--------|
| A | NVS basic roundtrip (write, read, compare, delete) | PASSED |
| B | Write-Before-Send timing (256B verified write) | PASSED (7.5ms) |
| C | SD card roundtrip | SKIPPED (no card inserted) |

---

## Platform Feasibility

Group chat (SimpleX mesh, pairwise connections): practical limit 10 members on ESP32 (9 connections, ~36KB RAM, ~7.4KB NVS).

XFTP file transfer: feasible up to 10-20MB per file. Voice messages possible (Opus codec, T-Deck has microphone, ~1MB per minute).

---

*Part 23 - Session 26: Persistence and Storage*
*SimpleGo Protocol Analysis*
*Original date: February 14, 2026*
*Rewritten: March 4, 2026 (v2)*
*ESP32 survives reboot without losing cryptographic state*
