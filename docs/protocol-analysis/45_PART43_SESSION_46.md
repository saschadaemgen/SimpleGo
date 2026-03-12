# Part 43: Session 46 - Codename MEGABLAST: Post-Quantum Double Ratchet
**Date:** 2026-03-11 to 2026-03-12 | **Version:** v0.1.18-alpha | **Milestone 22: Post-Quantum Double Ratchet**

## Overview

SEC-06 CLOSED. sntrup761 Post-Quantum Key Encapsulation integrated into the SimpleX Double Ratchet. SimpleGo received its first quantum-resistant message on 2026-03-12 at 09:16 CET. The SimpleX Chat App confirmed "Quantum Resistant" for the SimpleGo contact. This is the first known public implementation of a post-quantum double ratchet messenger on dedicated embedded hardware. All 6 security findings now CLOSED. SimpleGo implements five encryption layers per message.

## Infrastructure Phase (Aufgaben 1-3)

### sntrup761 ESP-IDF Component

PQClean round3 sntrup761 (23 source files, Public Domain) extracted and packaged as ESP-IDF component under `components/sntrup761/`. Two platform glue replacements: `randombytes()` mapped to `esp_fill_random()` (hardware RNG), SHA-512 mapped to `mbedtls_sha512()` (hardware accelerator). Source forked to `github.com/saschadaemgen/sntrup761` for independent source control.

Standalone verification: keygen, encap, decap all PASS. Shared secrets identical on both sides (32 bytes).

### Performance on T-Deck Plus (ESP32-S3, 240 MHz)

| Operation | Measured Time |
|-----------|---------------|
| keygen | 1839-1940 ms |
| encap | 70 ms |
| decap | 151-155 ms |
| Background pre-computation | 1850 ms (hidden from user) |
| Perceptible latency per direction change | ~225 ms (encap + decap) |

Key generation is 10x slower than desktop benchmarks estimated. PQClean "clean" reference is portable C without optimizations. Background pre-computation (Aufgabe 3) became mandatory rather than optional.

### SRAM Discovery and PSRAM Migration

Boot log revealed only 6,371 bytes free internal SRAM. The planned 80 KB SRAM stack for the Crypto-Task was impossible. Migration to PSRAM via `heap_caps_malloc(MALLOC_CAP_SPIRAM)` + `xTaskCreateStaticPinnedToCore()`. Safe because: Crypto-Task writes no NVS (no SPI cache conflict), SHA-512 peripheral is memory-mapped (no DMA dependency), all operations are pure CPU math. Actual stack usage measured at ~16 KB (of 80 KB allocated).

## Integration Phase (Aufgabe 4A-F)

### Teil A: NVS Setting (PQ Toggle)

NVS key `pq_enabled` (uint8_t, default 1 = ON). Lazy-init with cache. Boot log: `SEC-06: Post-Quantum Encryption = ON`. Getter/setter exported. No UI toggle yet (Session 47).

### Teil B: Ratchet-State Extension

`pq_kem_state_t` added to `ratchet_state_t`: 5,123 bytes per contact for own keypair (1158 public + 1763 secret), peer public key (1158), pending ciphertext (1039), pending shared secret (32), and state flags. PSRAM ratchet array grew from 66 KB to 722 KB (128 contacts). Automatic migration of pre-PQ states (520 bytes) with zeroed PQ fields, no erase-flash required.

### Teil C: Wire Format

Header serialization matching SimpleX Haskell implementation byte-for-byte. Four round-trip tests verified:

| Variant | Header Size | KEM Bytes | Result |
|---------|-------------|-----------|--------|
| Non-PQ | 88 | 0x30 (Nothing) | PASS |
| Proposed | 2310 | 31 50 04 86 (Just P, pk_len=1158) | PASS |
| Accepted | 2310 | 31 41 04 0f (Just A, ct_len=1039) | PASS |
| PQ Nothing | 2310 | 0x30 (anti-downgrade padding) | PASS |

Wire format: Maybe-tag 0x30/0x31, Proposed tag 'P' (0x50), Accepted tag 'A' (0x41), Word16 Big-Endian length prefixes for KEMPublicKey and KEMCiphertext. Header padded to 2310 bytes with PQ, 88 without. Anti-downgrade: once PQ active, padding stays at 2310 regardless of current KEM state.

### Teil D: HKDF Key Derivation

Root KDF extended: IKM = DH_secret || KEM_secret (88 bytes combined) or DH_secret only (56 bytes when no PQ). Salt = current root key, Info = "SimpleXRootRatchet", HKDF-SHA512, output 96 bytes split into new root key, chain key, next header key. Four KAT tests verified.

### Teil E: PQ State Machine

Three receive cases in `pq_recv_process()`:

**Fall 1 (Nothing received):** Generate keypair, set state to proposed.

**Fall 2 (Proposed received):** Encapsulate against peer key, store shared secret in `pending_ss` (NOT in current receive-kdf_root - this was Bug 1's root cause), store ciphertext in `pending_ct`, set state to accepted.

**Fall 3 (Accepted received):** Decapsulate with own SK (shared secret into receive-kdf_root), store new peer key, generate new keypair, encapsulate for next round (new shared secret into pending_ss).

Send side: pending_ss fed into send-kdf_root on next outgoing ratchet step. Critical rule: pq_support only transitions Off to On, never back (anti-downgrade).

### Teil F: NVS Persistence

PQ state persisted separately from classical ratchet state (NVS blob limit ~4000 bytes, full ratchet_state_t is 5640 bytes). Classical part saved as `rat_XX` (517 bytes via offsetof). PQ fields saved individually: pq_XX_act, pq_XX_st, pq_XX_opk, pq_XX_osk, pq_XX_ppk, pq_XX_ct, pq_XX_ss. Write-Before-Send at every state transition.

## Debugging Phase (6 Bugs)

### Bug 1: AES-GCM Body Decrypt Failed (CRITICAL)

State machine logic error: upon receiving Proposed, encap shared secret was immediately fed into receive-kdf_root. The sender had NOT used any KEM secret at this point. Different root keys, body decrypt failure. Fix: encap result stored in `pending_ss`, fed into send-kdf_root on next outgoing ratchet step. `*kem_ss_valid = false` for Fall 2.

### Bug 2: Heap Crash in ratchet_encrypt (CRITICAL)

PQ header is 2346 bytes vs 124 non-PQ. All callers allocated based on old header size. Buffer overflow, heap corruption. Fix: `ratchet_encrypt` reduces `padded_msg_len` internally by 2222 bytes when PQ active. Total output unchanged. No caller changes needed.

### Bug 3: NVS Blob Limit (HIGH)

`ratchet_state_t` grew from 520 to 5640 bytes, exceeding NVS blob limit (~4000 bytes). Fix: `RATCHET_CLASSICAL_SIZE = offsetof(ratchet_state_t, pq)` saves only classical part. PQ fields persisted separately.

### Bug 4: Crypto-Task Result Not Returned (HIGH)

FreeRTOS queue copies `pq_request_t` by value. Crypto-Task writes result to its local copy, never reaches caller. Fix: result passed as pointer. Crypto-Task writes through pointer to caller stack. Safe because caller blocks on semaphore.

### Bug 5: WiFi Settings Crash (MEDIUM, Pre-Existing)

NULL guard on `s_wifi_list` in `rebuild_timer_cb`.

### Bug 6: Ring Buffer Assert on Early Add Contact (MEDIUM, Pre-Existing)

NULL guard on `app_to_net_buf` in `smp_request_add_contact`.

## Haskell Code Analysis

Comprehensive analysis via Claude Code of simplexmq Haskell codebase: `protocol/pqdr.md` (PQDR specification), `src/Simplex/Messaging/Crypto/Ratchet.hs` (ratchet implementation), `src/Simplex/Messaging/Crypto.hs` (KEM primitives). Key findings: MsgHeader contains optional `ARKEMParams` with RKParamsProposed (public key only) and RKParamsAccepted (ciphertext + public key). HKDF uses simple concatenation of DH and KEM secrets as IKM. Header encrypted as whole block with AES-256-GCM, padded to fixed 2310 bytes.

## Five Encryption Layers After MEGABLAST

| Layer | Algorithm | Purpose |
|-------|-----------|---------|
| 1a | X448 Double Ratchet + AES-256-GCM | Classical E2E with PFS |
| 1b | sntrup761 KEM (hybrid with 1a) | Post-quantum resistance for every ratchet step |
| 2 | NaCl cryptobox (X25519 + XSalsa20 + Poly1305) | Per-queue traffic correlation protection |
| 3 | NaCl cryptobox (server-to-recipient) | Server traffic correlation protection |
| 4 | TLS 1.3 | Transport security |

## Memory Impact

| Resource | Before | After |
|----------|--------|-------|
| Flash (firmware) | 1.82 MB | 1.85 MB (+30 KB sntrup761) |
| PSRAM ratchet array | 66 KB | 722 KB (+656 KB PQ fields) |
| PSRAM crypto task | 0 | 80 KB (new task) |
| PSRAM free after boot | 8.05 MB | 7.21 MB |
| Internal SRAM | unchanged | unchanged |

## Files Changed

| File | Lines | Description |
|------|-------|-------------|
| components/sntrup761/ (60 files) | 2886 | New: PQClean sntrup761 ESP-IDF Component |
| smp_ratchet.h | 401 | pq_kem_state_t, parsed_msg_header_t, PQ constants |
| smp_ratchet.c | 1710 | PQ State Machine, Header Serialize, HKDF, NVS |
| smp_agent.c | 778 | PQ Header parse, ratchet_set_recv_pq() |
| main.c | 719 | Crypto-Task init, PQ NVS load, boot tests |
| CMakeLists.txt | +1 | sntrup761 in REQUIRES |
| smp_tasks.c | 1345 | Ring buffer NULL guard (Bug 6) |
| ui_settings_wifi.c | 693 | WiFi rebuild NULL guard (Bug 5) |

## Security Status: 6/6 CLOSED

| ID | Severity | Status | Session |
|----|----------|--------|---------|
| SEC-01 | CRITICAL | CLOSED | 45 |
| SEC-02 | CRITICAL | CLOSED | 45 |
| SEC-03 | HIGH | CLOSED | 42 |
| SEC-04 | HIGH | CLOSED | 45 |
| SEC-05 | MEDIUM | CLOSED | 45 |
| SEC-06 | MEDIUM | **CLOSED** | **46** |

## Commits

```
chore(build): remove auto-generated sdkconfig from tracking
feat(crypto): add sntrup761 PQClean component with ESP-IDF platform glue
feat(crypto): switch sntrup761 crypto task to PSRAM stack
feat(ratchet): add PQ toggle setting and extend ratchet state for sntrup761
feat(ratchet): implement PQ header serialization matching SimpleX wire format
feat(ratchet): integrate sntrup761 shared secret into HKDF root key derivation
feat(ratchet): implement PQ KEM state machine for ratchet direction changes
feat(ratchet): SEC-06 CLOSED - post-quantum double ratchet operational
```

## Known Issues

Bug #22: Standby freeze when returning from lock screen. Not PQ-related. Session 47.

PQ keygen performance (1.85s) acceptable with pre-computation but could be optimized. PQClean avx2/aarch64 not portable to Xtensa. Manual optimization deferred.

NVS capacity reduced to ~14 contacts with PQ state. Partition resize planned for Kickstarter phase.

sntrup761 standalone test runs at every boot (adds ~2 seconds). Remove once stability confirmed.

## Lessons Learned

**L243 (CRITICAL):** PQClean "clean" sntrup761 keygen on ESP32-S3 takes 1.85 seconds, not 50-200 ms from desktop benchmarks. Background pre-computation is mandatory, not optional.

**L244 (HIGH):** ESP32-S3 has only 6 KB free internal SRAM after all tasks running. Any new task requiring significant stack must use PSRAM. SHA-512 hardware accelerator works from PSRAM stacks (memory-mapped, not DMA).

**L245 (CRITICAL):** When receiving KEM Proposed, the encap shared secret must NOT be fed into receive-kdf_root. The sender has not used any KEM secret. Secret goes into pending_ss, fed into SEND-kdf_root on next outgoing ratchet step.

**L246 (HIGH):** PQ header adds 2222 bytes (2346 vs 124). ratchet_encrypt must reduce padded_msg_len internally to stay within 16 KB SMP block. Internal adjustment cleaner than changing all callers.

**L247 (HIGH):** NVS blob limit is ~4000 bytes. ratchet_state_t at 5640 bytes exceeds this. Split into classical part (offsetof pq field) and separate PQ NVS keys.

**L248:** FreeRTOS queue copies structs by value. If receiver needs to write back results, use pointer to caller memory. Caller must be blocked (semaphore) to keep pointed memory valid.

**L249 (HIGH):** When PQ breaks messaging completely, do NOT revert the entire feature. Diagnose which specific operation produces the mismatch (which side feeds KEM secret into kdf_root at which step), fix that operation, test again.

**L250:** Fork critical crypto dependencies into own repository (github.com/saschadaemgen/sntrup761). Upstream changes cannot break your build.

---

*Part 43 - Session 46 Codename MEGABLAST*
*SimpleGo Protocol Analysis*
*Date: March 11-12, 2026*
*First quantum-resistant message received: 2026-03-12, 09:16 CET*
*Bugs: 74 total (Bug #22 standby freeze new)*
*Lessons: 250 total*
*Security: 6/6 Findings CLOSED*
*Milestone 22: Post-Quantum Double Ratchet*
*The first quantum-resistant dedicated hardware messenger in the world.*
