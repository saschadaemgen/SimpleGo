# Part 40: Session 43 — Documentation Site and SMP-in-C Guide
**Date:** 2026-03-05 | **Version:** v0.1.18-alpha

## Overview

Documentation-only session. No firmware changes. Goal: professional documentation stack at docs.simplego.dev with Docusaurus 3, full content migration from the existing docs/ directory, and creation of the smp-in-c/ guide -- the only documentation of its kind for implementing the SimpleX Messaging Protocol outside the official Haskell codebase. Additionally received the official SimpleX Network Technical Architecture document from Evgeny Poberezkin, in which SimpleGo is cited as a demonstration of resource-based addressing energy efficiency on microcontroller hardware.

## Block A: Docusaurus Setup

~95 placeholder skeleton files deleted from docs/ (learn/, implement/, spec/, reference/, why-simplego/, releases/ directories plus individual stubs). Docusaurus 3.7.0 initialized manually in docs/docusaurus/ -- no npm scaffolding, all files created directly. GitHub Actions deployment workflow created at .github/workflows/deploy-docs.yml, triggered on changes to docs/docusaurus/**, builds with Node 20, deploys via peaceiris/actions-gh-pages@v3 to GitHub Pages. CNAME file set to docs.simplego.dev. SimpleGo design system applied: accent #45BDD1, background #050A12, fonts Source Sans 3 (body) and JetBrains Mono (code), card hover effects and border sweeps from simplego.dev. Build confirmed green.

DNS entry required (mein Prinz to set at provider):
```
Type: CNAME  |  Name: docs  |  Value: saschadaemgen.github.io  |  TTL: 3600
```

## Block B: Content Migration

17 documents migrated from existing markdown files into the Docusaurus structure under docs/docusaurus/docs/. Each file received correct Docusaurus frontmatter (title, sidebar_position).

**hardware/** (8 files): index, hardware-tiers, component-selection, hal-architecture, pcb-design, enclosure-design, hardware-security, adding-new-device. Sources: HARDWARE_OVERVIEW, HARDWARE_TIERS, COMPONENT_SELECTION, ENCLOSURE_DESIGN, SECURITY_ARCHITECTURE, HAL_ARCHITECTURE, ADDING_NEW_DEVICE.

**security/**: index corrected to 4 encryption layers throughout. SECURITY_MODEL split into security-model.md and threat-model.md at the Threat Model H2 boundary.

**architecture/** (6 files): index, system-overview, encryption-layers (X448 verified correct), memory-layout (PSRAM/NVS constraints prominent), crypto-state, wifi-manager. Source: ARCHITECTURE.md split by H2 sections.

**contributing/** (5 files): index from DEVELOPMENT.md, build.md from BUILD_SYSTEM.md (flash method discipline admonition added), project-structure.md from TECHNICAL.md, coding-standards.md from DEVNOTES.md. license.md written from scratch (AGPL-3.0 software + CERN-OHL-W-2.0 hardware with rationale).

**reference/** (6 files): constants.md written from scratch with all critical constants. changelog.md consolidates all three release notes (v0.1.14 through v0.1.16) in chronological order. wire-format.md and crypto-primitives.md from WIRE_FORMAT.md and CRYPTO.md. protocol-links.md written from scratch pointing to rcv-services branch. roadmap.md written from scratch.

**why-simplego/**: index rewritten without references to criminal encrypted phone networks (EncroChat, Sky ECC, ANOM, Ghost, Phantom Secure). whitespace-analysis.md added from the hardware security whitepaper.

**README.md** (repo root): rewritten. Positive framing, technical justification for dedicated hardware, current T-Deck Plus status, quick start, link to docs.simplego.dev. No criminal network references.

## Block C: New Documents

**hardware/t-deck-plus.md**: Complete reference page for the current development device. GPIO table (GPIO 42 display backlight 16-level pulse-counting, I2C 0x55 keyboard backlight), SPI2 bus sharing constraints, PSRAM layout table (~68KB ratchet array, ~120KB message cache), known hardware constraints (hardware AES disabled, PSRAM task NVS constraint, WPA3 auth threshold).

**smp-in-c/** (10 files): World-first documentation for implementing SMP in C outside the official Haskell codebase. Written from direct implementation experience, Haskell source analysis, and Evgeny Poberezkin conversations.

| File | Content |
|------|---------|
| index.md | Guide overview, who it is for, the golden rule (100x reading) |
| overview.md | Haskell vs C comparison table, two-router architecture, protocol version |
| transport.md | TLS 1.3 with mbedTLS, block framing, 16KB hard limit, keep-alive |
| queue-lifecycle.md | NEW/KEY/SUB/SEND/ACK/MSG/END commands, state machine, delivery receipts |
| handshake.md | PHConfirmation key location (Evgeny confirmation), HELLO exchange, debug approach |
| encryption.md | X448 warning, 4 layers explained, cbNonce construction, HKDF, library choices |
| ratchet.md | PSRAM state array, NVS persistence, PSRAM task stack constraint, Haskell references |
| idempotency.md | Lost response problem, golden rule, implementation pattern in C |
| subscription.md | One socket constraint, keep-alive PING/PONG, session validation on reconnect |
| pitfalls.md | Consolidated list of all silent failure modes discovered during development |

overview.md and transport.md integrate the SimpleX Network Technical Architecture document received from Evgeny: two-router delivery path explanation, resource-based addressing, elimination of polling as the architectural basis for battery efficiency.

**why-simplego/vs-signal.md**: Platform problem, identity problem, polling problem, encryption layer comparison (2 vs 4), hardware difference. Concludes with when Signal is the right choice.

**why-simplego/vs-briar.md**: Architecture comparison table, network architecture (both avoid polling via different mechanisms), when each is the right choice based on threat model.

**reference/nvs-key-registry.md**: All known NVS keys organized by namespace (simplego, smp), PSRAM constraint reminder, under-construction note.

**security/audit-log.md**: 6 resolved security-relevant bugs formally documented with severity, component, root cause, resolution, session reference. Open items: 5 log categories, eFuse + nvs_flash_secure_init, Private Message Routing.

## External Input: SimpleX Network Technical Architecture

Evgeny Poberezkin provided the official SimpleX Network Technical Architecture document. SimpleGo is cited directly:

> SimpleGo "demonstrates the energy efficiency of resource-based addressing: the device receives packets without continuous polling."

The battery runtime figure (20 days) mentioned in the document is an estimate pending verification -- not included in docs until measured. The architectural explanation (resource-based addressing eliminates polling) is integrated into smp-in-c/overview.md, smp-in-c/transport.md, why-simplego/index.md, and why-simplego/vs-briar.md.

## Critical Constants Verified in Docs

All documentation was checked against these values before commit:

| Constant | Value | Rule |
|----------|-------|------|
| HISTORY_MAX_TEXT | 4096 | SD storage path, never truncate before write |
| HISTORY_MAX_PAYLOAD | 16000 | SMP block, hard limit 16384 bytes |
| HISTORY_DISPLAY_TEXT | 512 | LVGL bubble layer only |
| Encryption layers | 4 | Double Ratchet + Per-Queue NaCl + Server NaCl + TLS 1.3 |
| E2E ratchet curve | X448 | Not X25519 |
| CONFIG_MBEDTLS_HARDWARE_AES | n | DMA/PSRAM conflict, permanent |
| WPA3 threshold | WIFI_AUTH_WPA2_PSK | Not WIFI_AUTH_WPA_WPA2_PSK |

## Commits

| Commit | Block |
|--------|-------|
| `chore(docs): remove placeholder skeleton directories` | A |
| `feat(docs): initialize Docusaurus 3 in docs/docusaurus/` | A |
| `feat(docs): add GitHub Actions deployment workflow for docs.simplego.dev` | A |
| `feat(docs): apply SimpleGo design system theme` | A |
| `feat(docs): add stub pages for all sidebar sections` | A |
| `feat(docs): migrate hardware/ section to Docusaurus` | B |
| `feat(docs): migrate security/ section to Docusaurus` | B |
| `feat(docs): migrate architecture/ section to Docusaurus` | B |
| `feat(docs): migrate contributing/ section to Docusaurus` | B |
| `feat(docs): migrate reference/ section to Docusaurus` | B |
| `feat(docs): add constants reference page with critical values` | B |
| `feat(docs): migrate why-simplego/ section to Docusaurus` | B |
| `docs(readme): rewrite README without criminal network references` | B |
| `feat(docs): add T-Deck Plus hardware reference page` | C |
| `feat(docs): add smp-in-c index, overview, and transport pages` | C |
| `feat(docs): add smp-in-c queue-lifecycle, handshake, and encryption pages` | C |
| `feat(docs): add smp-in-c ratchet, idempotency, subscription, and pitfalls pages` | C |
| `feat(docs): add security audit-log page` | C |
| `feat(docs): add why-simplego vs-signal and vs-briar pages` | C |
| `feat(docs): add nvs-key-registry and roadmap reference pages` | C |

## Open Items for Session 44

- getting-started/ pages (quick-start, flashing, building, faq) still stubs -- need real content
- Keep-Alive (PING/PONG) firmware implementation (first firmware task after documentation)
- eFuse + nvs_flash_secure_init combined with CRYSTALS-Kyber (Kickstarter phase)
- 5 logging categories with security-relevant data (contact links, SUB hex, response hex, block headers, parser bytes) -- pre-Kickstarter
- Private Message Routing (post-MVP)
- Battery runtime measurement to verify the 20-day estimate cited in Evgeny architecture document

## Lessons Learned

**L230 (HIGH):** Add-Content in PowerShell appends blindly to the end of any file regardless of structure. For structured markdown documents, always use Set-Content with the complete new content, or precise -replace operations. Never use Add-Content for session documentation files.

**L231:** Session documentation files in protocol-analysis/ have strict naming conventions: `NN_PARTMM_SESSION_SS.md` where NN is the file sequence number, MM is the part number, SS is the session number. The SESSION_XX_PROTOCOL.md naming pattern does not exist in this codebase.

**L232:** Official protocol documentation from Evgeny (architecture docs, RFC files, branch-specific specs) must be preserved in evgeny_reference.md and integrated into the public docs where appropriate. Battery runtime figures and other unverified claims must not be published until measured on device.

---

*Part 40 - Session 43 Documentation Site and SMP-in-C Guide*
*SimpleGo Protocol Analysis*
*Date: March 5, 2026*
*Bugs: 71 total (69 FIXED, 1 identified for SPI3, 1 temp fix)*
*Lessons: 232 total*
*Milestone 19: Professional Documentation Site*
