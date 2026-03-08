
## Status after Session 43 - 2026-03-08

### Documentation
- Docusaurus 3 live at wiki.simplego.dev
- GitHub Actions deployment workflow active (triggers on docs/** and wiki/**)
- SimpleGo design system applied (accent #45BDD1, Source Sans 3 + JetBrains Mono)
- smp-in-c/ guide complete: 10 pages, world-first documentation for C implementation of SMP
- hardware/, security/, architecture/, contributing/, reference/, why-simplego/ all migrated
- Offline full-text search (1924 documents indexed)
- README rewritten without criminal network references
- security/audit-log.md: 6 resolved security issues documented

### Firmware (changed in Session 43)
- Security log cleanup: all cryptographic material removed from serial output (3 files, 16 removals)
- Display name: NVS-backed user-configurable name replaces hardcoded "ESP32"
- First-boot prompt: user sets display name before any interaction
- Crash fix: ui_connect.c dangling pointer after screen deletion
- Performance: QR 60% faster, handshake 40% faster, boot 30% faster
- SMP implementation: production-ready alpha
- 128 contacts, delivery receipts, encrypted SD history, WiFi Manager
- 47 source files with AGPL-3.0 headers

### SHOWSTOPPER: Bug #20
- SEND fails after 6+ hours idle (PING/PONG still working)
- Red X on display, 2-3 red error lines in serial
- Device reset fixes immediately
- Must be resolved before alpha release

### Open Items
- Bug #20: SEND after extended idle (SHOWSTOPPER, Session 44 priority)
- Remaining dump_hex calls in smp_contacts.c (+0000: prefix format)
- getting-started/ pages: quick-start, flashing, building, faq still stubs
- eFuse + NVS Flash Encryption: Kickstarter phase
- Private Message Routing: post-MVP
- Battery runtime measurement: verify 20-day estimate
