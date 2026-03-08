# Part 40: Session 43 - Documentation Rebuild + Security Cleanup + Display Name
**Date:** 2026-03-05 to 2026-03-08 | **Version:** v0.1.18-alpha

## Overview

Three major workstreams: professional documentation site on wiki.simplego.dev with Docusaurus 3, removal of all cryptographic material from serial output, and user-configurable display name replacing the hardcoded "ESP32" identity. Additionally discovered Bug #20 (SEND fails after 6+ hours idle) - SHOWSTOPPER for alpha release.

## Workstream 1: Documentation Site (wiki.simplego.dev)

### Docusaurus 3 Setup

Complete rebuild of documentation infrastructure using Docusaurus 3 in the `wiki/` directory. The site reads docs from `../docs` (the main docs folder) without duplication. Deployed via GitHub Actions to GitHub Pages with CNAME wiki.simplego.dev. ~95 placeholder skeleton files deleted. GitHub Actions workflow triggered on changes to docs/** or wiki/**.

Custom CSS matching simplego.dev: background #050A12, accent #45BDD1, Source Sans 3 + JetBrains Mono. SVG logo from simplego.dev. Dark mode forced. Swizzled Footer component replicating simplego.dev layout (5 columns). Navbar: left "Docs" link, right side external links (Home, Product, Crypto, Flash Tool, Network, Pro, GitHub).

Installed `@cmfcmf/docusaurus-search-local` for offline full-text search (1924 documents indexed). Fixed critical mobile menu issue (hamburger sidebar showed no content due to CSS `display: none` on `.navbar__items--right`).

### Content Migration (17 documents)

**hardware/** (8 files): index, hardware-tiers, component-selection, hal-architecture, pcb-design, enclosure-design, hardware-security, adding-new-device.

**security/**: index corrected to 4 encryption layers. SECURITY_MODEL split into security-model.md and threat-model.md. audit-log.md: 6 resolved security issues formally documented.

**architecture/** (6 files): index, system-overview, encryption-layers (X448 verified), memory-layout (PSRAM/NVS constraints), crypto-state, wifi-manager.

**contributing/** (5 files): index, build.md (flash method discipline admonition), project-structure.md, coding-standards.md, license.md (AGPL-3.0 software + CERN-OHL-W-2.0 hardware with rationale).

**reference/** (6 files): constants.md (all critical constants), changelog.md (v0.1.14 through v0.1.16), wire-format.md, crypto-primitives.md, protocol-links.md (rcv-services branch), roadmap.md.

**why-simplego/**: index rewritten without criminal encrypted phone network references. whitespace-analysis.md from hardware security whitepaper. vs-signal.md: platform/identity/polling/encryption comparison. vs-briar.md: architecture comparison table.

### New Documents

**hardware/t-deck-plus.md**: GPIO table (GPIO 42 display backlight, I2C 0x55 keyboard), SPI2 bus sharing constraints, PSRAM layout table, known hardware constraints.

**smp-in-c/** (10 files): World-first documentation for implementing SMP in C outside the official Haskell codebase.

| File | Content |
|------|---------|
| index.md | Guide overview, who it is for, the golden rule (100x reading) |
| overview.md | Haskell vs C comparison, two-router architecture, protocol version |
| transport.md | TLS 1.3 with mbedTLS, block framing, 16KB hard limit, keep-alive |
| queue-lifecycle.md | NEW/KEY/SUB/SEND/ACK/MSG/END commands, state machine, delivery receipts |
| handshake.md | PHConfirmation key location (Evgeny confirmation), HELLO exchange |
| encryption.md | X448 warning, 4 layers, cbNonce construction, HKDF, library choices |
| ratchet.md | PSRAM state array, NVS persistence, PSRAM task stack constraint |
| idempotency.md | Lost response problem, golden rule, implementation pattern in C |
| subscription.md | One socket constraint, PING/PONG, session validation on reconnect |
| pitfalls.md | All silent failure modes discovered during development |

**reference/nvs-key-registry.md**: All known NVS keys organized by namespace.

### External Input: SimpleX Network Technical Architecture

Evgeny Poberezkin provided the official SimpleX Network Technical Architecture document. SimpleGo cited directly: "demonstrates the energy efficiency of resource-based addressing: the device receives packets without continuous polling." Battery runtime figure (20 days) is an estimate pending verification, not included in docs until measured. Architectural explanation integrated into smp-in-c/overview.md, transport.md, why-simplego/index.md, vs-briar.md.

### Broken Links Fixed

All broken links and anchors resolved to zero warnings on build. Fixes across HARDWARE_OVERVIEW.md, protocol-analysis/README.md, 01_SIMPLEX_PROTOCOL_INDEX.md, QUICK_REFERENCE.md (8 anchor fixes), v0.1.14-alpha.md, and 6 root docs. Added frontmatter slug to protocol-analysis/INDEX.md for duplicate route conflict.

---

## Workstream 2: Security Log Cleanup

All security-sensitive data removed from serial output across 3 files:

**smp_parser.c (9 removals):** Key1/Key2 SPKI header printf loops, raw key printf loops, dump_hex calls for SPKI key, raw key, after-key data, before-key data. CRITICAL: decrypted plaintext dump removed.

**smp_tasks.c (2 blocks):** KEY_DEBUG transmission hex dump, KEY response hex dump.

**smp_contacts.c (5 lines + cleanup):** Response hex dump, correlation ID, entity ID, recipient ID, command bytes. Removed orphaned cmd_dump variable.

Verification: all three search patterns confirmed empty results. Remaining finding: additional dump_hex calls in smp_contacts.c using +0000: prefix format (for Session 44).

---

## Workstream 3: Display Name Feature

### Problem
Display name hardcoded as "ESP32" in smp_peer.c AgentConfirmation JSON. Every contact sees "ESP32" instead of the user's chosen name.

### Solution

**Part 1: NVS Storage.** storage_get/set_display_name(), storage_has_display_name() with "user_name" NVS key in smp_storage.c/h.

**Part 2: Dynamic AgentConfirmation.** Hardcoded "ESP32" in smp_peer.c replaced with NVS lookup at connection time.

**Part 3: Settings Editor.** Clickable name row in INFO tab (ui_settings_info.c) with fullscreen overlay keyboard editor.

**Part 4: First-Boot Prompt.** UI_SCREEN_NAME_SETUP screen, one-shot check on navigation to MAIN. User sets their display name before any other interaction.

### Design Decision
No broadcasting to existing contacts. Intentionally deferred to future privacy settings architecture.

### Crash Fix (bonus)
Fixed Guru Meditation LoadProhibited in ui_connect.c (dangling pointers after screen deletion). Same pattern as Session 41 screen lifecycle fix.

---

## BUG #20: SEND Fails After Extended Idle (SHOWSTOPPER)

### Symptoms
After approximately 6+ hours of idle time (device powered on, PING/PONG running, no messages sent), SEND commands fail immediately. Red X on ESP display. 2-3 red error lines in serial log.

### Key Observations
- PING/PONG working correctly (every ~30 seconds, server responds with 5-byte PONG)
- Problem is NOT keep-alive - subscription appears active
- After device reset, everything works immediately
- Failed messages do NOT appear in chat history (failure before SD write)
- Did not exist before recent sessions (possibly Session 41 or 42 changes)
- Occurs reliably after overnight idle (6-8 hours)
- Receiving during idle: unknown (needs testing)

### Possible Causes
1. WiFi Manager interference: background reconnect creates new socket without SMP session recovery
2. Memory leak: slow heap exhaustion causing malloc failure for SEND buffer
3. Session 42 refactoring side effect: state variable lost scope in smp_app_run() helper extraction
4. TLS session timeout: TLS expires even though SMP PING keeps SMP layer alive
5. Ratchet state issue: long idle causes synchronization problem

### Priority
SHOWSTOPPER for Alpha Release. Must be resolved before v0.1.17 release.

---

## Performance Improvements

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| QR code generation | ~1.5s | ~0.6s | 60% faster |
| Connection handshake | ~3.5s | ~2.1s | 40% faster |
| Boot to main screen | ~7s | ~5s | 30% faster |

---

## Critical Constants Verified in Docs

| Constant | Value | Rule |
|----------|-------|------|
| HISTORY_MAX_TEXT | 4096 | SD storage path, never truncate before write |
| HISTORY_MAX_PAYLOAD | 16000 | SMP block, hard limit 16384 bytes |
| HISTORY_DISPLAY_TEXT | 512 | LVGL bubble layer only |
| Encryption layers | 4 | Double Ratchet + Per-Queue NaCl + Server NaCl + TLS 1.3 |
| E2E ratchet curve | X448 | Not X25519 |
| CONFIG_MBEDTLS_HARDWARE_AES | n | DMA/PSRAM conflict, permanent |
| WPA3 threshold | WIFI_AUTH_WPA2_PSK | Not WIFI_AUTH_WPA_WPA2_PSK |

---

## Git Commits (Session 43)

1. `feat(wiki): redesign navbar, sidebar and theme to match simplego.dev`
2. `docs(wiki): add index pages for all documentation sections`
3. `fix(docs): resolve all broken links and anchors across documentation`
4. `fix(wiki): update all navigation links to match simplego.dev URL restructure`
5. `feat(wiki): add local search plugin and fix mobile navigation menu`
6. `chore: remove obsolete SPONSORS.md and update OST files`
7. `security(logging): remove all cryptographic material from serial output`
8. `feat(profile): add NVS-backed display name replacing hardcoded ESP32 identity`
9. `feat(ui): add display name editor to settings INFO tab`
10. `feat(ui): add first-boot display name prompt screen`
11. `feat(ui): integrate first-boot name check into navigation flow`
12. `fix(ui): prevent crash on connect screen reset after screen deletion`
13. `chore(ost): optimize lyrics files for web player integration`

## Lessons Learned

**L230 (HIGH):** Add-Content in PowerShell appends blindly to any file regardless of structure. For structured markdown, always use Set-Content with complete content or precise -replace operations. Never use Add-Content for session documentation files.

**L231:** Session documentation files have strict naming conventions: `NN_PARTMM_SESSION_SS.md` where NN is file sequence, MM is part number, SS is session number. The SESSION_XX_PROTOCOL.md pattern does not exist in this codebase.

**L232:** Official architecture documents from Evgeny must be preserved in evgeny_reference.md. Unverified figures (battery runtime) must not be published until measured. Architectural explanations (resource-based addressing, polling elimination) are verified and can be integrated immediately.

---

*Part 40 - Session 43 Documentation + Security Cleanup + Display Name*
*SimpleGo Protocol Analysis*
*Date: March 5-8, 2026*
*Bugs: 72 total (69 FIXED, 1 identified SPI3, 1 temp fix, 1 SHOWSTOPPER)*
*Lessons: 232 total*
*Milestone 19: Professional Documentation Site*
*OPEN: Bug #20 SEND fails after 6+ hours idle - SHOWSTOPPER*
