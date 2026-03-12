
## Status after Session 46 - 2026-03-12 (Codename MEGABLAST)

### WORLD FIRST: Post-Quantum Double Ratchet on Dedicated Hardware
- First quantum-resistant message received: 2026-03-12, 09:16 CET
- SimpleX Chat App confirms "Quantum Resistant" for SimpleGo contact
- sntrup761 KEM integrated into X448 Double Ratchet (hybrid PQ)
- Five encryption layers per message (was four before MEGABLAST)
- **6/6 Security Findings CLOSED**

### Security: ALL FINDINGS CLOSED
- SEC-01 CLOSED: sodium_memzero on PSRAM cache (S45)
- SEC-02 CLOSED: HMAC NVS vault, eFuse BLOCK_KEY1 (S45)
- SEC-03 CLOSED: mbedtls_platform_zeroize (S42)
- SEC-04 CLOSED: Auto-lock 60s + memory wipe (S45)
- SEC-05 CLOSED: Device-bound HKDF, chip MAC (S45)
- SEC-06 CLOSED: sntrup761 post-quantum KEM (S46 MEGABLAST)

### Firmware
- SMP implementation with post-quantum double ratchet
- 128 contacts (14 with PQ state due to NVS capacity)
- AES-256-GCM encrypted SD history, WiFi Manager
- Display name with first-boot prompt, auto-lock screen
- HMAC NVS vault, device-bound HKDF
- sntrup761 background keygen (1.85s, hidden from user)
- PQ header: 2310 bytes, anti-downgrade padding
- 47 source files + 60 sntrup761 component files

### Memory After MEGABLAST
- Flash: 1.85 MB (+30 KB sntrup761)
- PSRAM ratchet: 722 KB (was 66 KB, +656 KB PQ fields)
- PSRAM crypto task: 80 KB
- PSRAM free: 7.21 MB

### Bugs
- Bug #20: SEND after 6+ hours idle (KNOWN)
- Bug #21: SD phantom counter (LOW)
- Bug #22: Standby freeze returning from lock (NEW, not PQ-related)

### Open Items
- Bug #22 investigation (Session 47)
- NVS partition resize for full 128 PQ contacts (Kickstarter)
- sntrup761 boot test removal (after stability confirmed)
- PQ keygen optimization (deferred, pre-computation sufficient)
- Alpha firmware binary for simplego.dev/installer
