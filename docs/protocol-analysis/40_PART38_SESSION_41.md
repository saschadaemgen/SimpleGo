# Part 38: Session 41 — Pre-GitHub Cleanup and Stabilization
**Date:** 2026-03-04 | **Version:** v0.1.18-alpha

## Overview

Pre-GitHub cleanup and stabilization session. Full code review combined with systematic bug fixes, producing the most stable build to date. Eight sub-packets executed: security cleanup, LVGL pool measurements, hardware AES fix, live bubble eviction fix, screen lifecycle diagnosis and fix with dangling pointer protection, comment cleanup across 9 files, and send_agent_confirmation() artifact removal. 11 commits pushed.

## 41a: Pre-Release Security Cleanup

Deleted `simplex_secretbox_open_debug()` (~90 lines) from simplex_crypto.c/h. Replaced brute-force E2E decrypt loop (Methods 0-3) with direct `decrypt_client_msg()` call in smp_e2e.c. Removed KEY_DEBUG ESP_LOG_BUFFER_HEX blocks from smp_tasks.c. Deleted empty `ui_task()` and its xTaskCreatePinnedToCore call. Changed RECV, BLOCK_TX, and DH shared secret logs from LOGI to LOGD. Deleted T6-Diag5 hex dump block from smp_network.c. Applied `mbedtls_platform_zeroize()` for buffer clearing in smp_storage.c (CWE-14 compliance). GitHub security features enabled: CodeQL C/C++, Dependabot, secret push protection, SECURITY.md.

## 41b: LVGL Pool Measurements

Definitive per-bubble LVGL pool cost measurements under production conditions:

| Metric | Value |
|--------|-------|
| Per-bubble cost | 960-1368 bytes (avg ~1150 bytes) |
| BUBBLE_WINDOW_SIZE | 5 (confirmed correct) |
| 5 bubbles consumption | ~5500-6500 bytes from ~59KB pool |
| Memory leak on contact switch | None detected |
| Fragmentation trend | 44% to 48%, stabilizes |

These measurements validate the Session 40 sliding window architecture. 5 bubbles at ~1150 bytes each leaves ample headroom in the ~59KB available LVGL pool.

## 41c: Hardware AES Fix

**Root cause:** ESP-IDF hardware AES accelerator requires contiguous internal SRAM for DMA. At runtime only 9.6KB internal SRAM was free, but 13KB message body decrypts failed because the hardware accelerator could not allocate a contiguous DMA buffer.

**Fix:** `CONFIG_MBEDTLS_HARDWARE_AES=n` in sdkconfig.defaults. Software AES uses CPU and allocates from PSRAM. Negligible performance impact on ESP32-S3 at messaging workloads.

**Build method:** `idf.py fullclean && idf.py erase-flash && idf.py build flash monitor` (sdkconfig change requires fullclean).

## 41d: Live Bubble Eviction Fix

**Root cause:** Live message handler created a new bubble THEN checked if eviction was needed. The pool check rejected the new bubble because eviction had not yet freed space.

**Fix:** Evict FIRST if bubble_count >= BUBBLE_WINDOW_SIZE, THEN create new bubble. Safety margin lowered from 8192 to 4096 bytes (safe because eviction frees 1000-1300 bytes per bubble). Files: ui_chat.c, ui_chat_bubble.c.

## 41e: LVGL Pool Fragmentation Diagnosis

Diagnostic logging added to ui_manager_show_screen() and ui_manager_go_back(). Finding: screens were created on first visit but NEVER deleted. Up to 4 screens existed simultaneously (Main + Contacts + Chat + Settings). After a full navigation sequence: ~14KB permanently consumed by inactive screens. Pool after that sequence: 8576 bytes free (86% used), insufficient for 5 chat bubbles.

## 41f: Screen Lifecycle Fix + Dangling Pointer Protection

**Screen lifecycle fix:** ui_manager.c deletes previous screen after lv_scr_load() in both show_screen() and go_back(). Main screen exempt (permanent), all others recreated on each visit. This is the ephemeral screen pattern: created on enter, destroyed on leave.

**Dangling pointer protection in ui_chat.c:** New ui_chat_cleanup() function nullifies all 6 static LVGL pointers and resets window state. `if (!screen) return` guards added on 4 public functions called by background tasks (prevents crashes when protocol task tries to update a destroyed chat screen).

**Dangling pointer protection in ui_chat_bubble.c:** New chat_bubble_cleanup() zeros the tracked_msgs[] array.

**Result:** Pool consistently at ~42,970 bytes free (31% used), stable across 8 screen visits and 5 contacts. Compared to 8,576 bytes free (86% used) before fix.

## 41g: Comment Cleanup

9 files cleaned: smp_tasks.c, smp_peer.c, smp_e2e.c, smp_network.c, smp_storage.c, ui_manager.c, ui_chat.c, ui_chat_bubble.c, tdeck_display.c. All Session/Auftrag/Phase/T-ref comments removed. German comments translated to English. Extern declarations marked with TODO for header migration. README.md rewritten with accurate Evgeny quote, smartphone comparison table, eFuse/post-quantum roadmap, updated implementation status.

## 41h: send_agent_confirmation() Cleanup

133 lines of debug artifacts removed from smp_peer.c. Deleted: CONF_CMP diagnostic blocks, AUFTRAG-15a/17 blocks, 6 printf hex-dump loops, DEBUG-Check block. All ESP_LOGI/LOGE for genuine state transitions preserved. Zero printf remaining in production code (only snprintf for NVS key formatting). Build-time fix: dead call to deleted simplex_secretbox_open_debug() replaced with simplex_secretbox_open().

## Architecture Verified

Session 41 confirmed the following architecture invariants: LVGL runs on dedicated Core 1 task (tdeck_lvgl.c), not main task. Four encryption layers all correct and verified. SMP v7 protocol implementation complete. Screen lifecycle: Main permanent, all others ephemeral (created on enter, destroyed on leave). PSRAM cache (123KB) persists across chat screen lifecycles (intentional, not a leak).

## Commits Pushed

| Commit | Packet |
|--------|--------|
| `refactor(security): remove debug crypto functions and verbose logging` | 41a |
| `fix(security): use mbedtls_platform_zeroize for buffer clearing` | 41a |
| `docs(security): add vulnerability reporting policy` | 41a |
| `fix(crypto): disable hardware AES to prevent DMA memory allocation failure` | 41c |
| `fix(ui): evict oldest bubble before creating new one in live chat` | 41d |
| `fix(ui): delete inactive screens and prevent dangling pointer crashes` | 41f |
| `refactor(docs): remove session references from smp layer` | 41g |
| `refactor(docs): remove session references from ui layer` | 41g |
| `refactor(docs): remove session references from hal layer` | 41g |
| `fix(security): remove printf hex dumps from send_agent_confirmation` | 41h |
| `docs(readme): rewrite for technical GitHub audience` | 41g |

## Open Items for Session 42

Quality-Pass: smp_handshake.c has 73 printf/LOGW/BUFFER_HEX still present. smp_globals.c needs dissolution into correct modules. 4 extern-TODO markers need header migration. License headers need AGPL-3.0 consistency verification. Re-delivery log should change ESP_LOGE to ESP_LOGW in smp_ratchet.c. smp_app_run() at 450 lines needs splitting.

Production/Kickstarter: eFuse + nvs_flash_secure_init. CRYSTALS-Kyber activation. HKDF Info strengthening with device serial.

## Lessons Learned

**L221 (CRITICAL):** ESP-IDF hardware AES accelerator requires contiguous internal SRAM for DMA. When internal SRAM is fragmented or low (< 13KB contiguous), hardware AES silently fails. CONFIG_MBEDTLS_HARDWARE_AES=n forces software AES which allocates from any heap including PSRAM. Negligible performance impact for messaging workloads.

**L222 (HIGH):** LVGL screens created but never deleted consume pool memory permanently. After Main+Contacts+Chat+Settings: ~14KB consumed by inactive screens, leaving only 8.5KB for bubbles. Ephemeral screen pattern (create on enter, destroy on leave) keeps pool at 31% usage.

**L223 (HIGH):** Background tasks (protocol, network) may call UI update functions after a screen has been destroyed. All public UI functions must have `if (!screen) return` guards. Additionally, all static LVGL pointers must be nullified in a cleanup function called before screen destruction.

**L224:** Bubble eviction order matters. Create-then-evict fails because pool check rejects the new bubble. Evict-then-create succeeds because pool has headroom from the freed bubble (~1.2KB).

**L225:** mbedtls_platform_zeroize() is the correct function for clearing sensitive buffers (CWE-14). Unlike memset(), it is guaranteed not to be optimized away by the compiler.

---

*Part 38 - Session 41 Pre-GitHub Cleanup*
*SimpleGo Protocol Analysis*
*Date: March 4, 2026*
*Bugs: 71 total (69 FIXED, 1 identified for SPI3, 1 temp fix)*
*Lessons: 225 total*
*Milestone 17: Pre-GitHub Stabilization*
