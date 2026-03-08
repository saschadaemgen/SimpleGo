---
title: "Class 1 - Runtime Memory Protection"
sidebar_position: 6
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Runtime Memory Protection

**Document version:** Session 44 | March 2026
**Applies to:** All SimpleGo Hardware Classes (Class 1 implementation, concepts extend to Class 2/3)
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## The Principle: What Is Not in Memory Cannot Be Stolen

The HMAC vault (Pillar 1) protects keys at rest in flash storage. But during normal operation, decrypted messages and derived key material exist in volatile RAM. An attacker who can read RAM contents - via JTAG debugging, cold boot attack, or a firmware vulnerability - bypasses the flash encryption entirely.

Runtime memory protection is the discipline of minimizing the time that sensitive data exists in RAM and ensuring it is securely overwritten when no longer needed. This is not optional hardening. It is the second of the three required pillars for Hardware Class 1 security.

---

## Current Vulnerabilities (Session 43 Status)

### SEC-01: Decrypted Messages in PSRAM (CRITICAL)

The `s_msg_cache` in `ui/screens/ui_chat.c` holds up to 30 decrypted messages in PSRAM, totaling approximately 120 KB. These messages are loaded from the AES-256-GCM encrypted SD card for display in the chat sliding window. The cache is never zeroed - not on chat screen exit, not on contact switch, not on display timeout, not on shutdown.

An attacker with JTAG access to a powered device can scan the PSRAM region and read all 30 cached messages in plaintext. If the device has multiple contacts, switching between them leaves the previous contact's messages in the cache alongside the new contact's messages until the cache slots are overwritten by new data.

### SEC-04: No Memory Wipe on Display Timeout (HIGH)

When the display times out (backlight off, apparent "lock" state), no memory clearing occurs. The device appears locked but retains plaintext messages in PSRAM and LVGL label memory. An attacker who gains physical access to a "locked" device has the same access to cached messages as an unlocked device.

---

## Sensitive Data Locations in the ESP32-S3 Memory Map

Understanding where sensitive data lives is prerequisite to protecting it:

| Location | Size | Contains | Current Protection |
|----------|------|----------|-------------------|
| `s_msg_cache` (PSRAM) | ~120 KB (30 messages) | Decrypted message text, sender info, timestamps | **None (SEC-01)** |
| LVGL label buffers (internal pool) | Variable (within 64 KB pool) | Currently displayed message text (5 bubbles) | **None (SEC-04)** |
| `ratchet_state_t` array (PSRAM) | ~68 KB (128 contacts) | Active ratchet keys, chain keys, DH keys | Overwritten on ratchet advance |
| NVS read buffers (stack) | Variable | Ratchet keys during load from NVS | Stack frame reuse (unreliable) |
| TLS session context (internal SRAM) | ~40 KB | TLS session keys, negotiation state | Managed by mbedTLS |
| Frame pool (PSRAM) | 64 KB (4 x 16 KB) | Raw SMP frames during processing | `sodium_memzero()` on release (correct) |
| `tx_ring_buffer` / `rx_ring_buffer` | Variable | Encrypted SMP frames in transit | Ring buffer overwrites (unreliable) |

The frame pool is the only sensitive buffer that is currently correctly zeroed on release, using `sodium_memzero()`. This was implemented correctly from the beginning.

---

## The Fix: sodium_memzero() at Every Transition

### What sodium_memzero() Does

`sodium_memzero(void *ptr, size_t len)` is libsodium's secure memory zeroing function. Unlike `memset()`, it is guaranteed not to be optimized away by the compiler (CWE-14: Compiler Removal of Code to Clear Buffers). It uses volatile writes or platform-specific barriers to ensure the zeroing actually happens.

SimpleGo already links libsodium (used for NaCl encryption layers 2 and 3). No additional dependency is needed. The function is available in every source file via `#include <sodium.h>`.

### SEC-01 Fix: Zeroing s_msg_cache

The fix adds `sodium_memzero()` calls at four transition points in `ui/screens/ui_chat.c`:

**On chat screen cleanup (existing function `ui_chat_cleanup()`):**
```c
void ui_chat_cleanup(void) {
    // Zero all cached messages before freeing
    if (s_msg_cache != NULL) {
        sodium_memzero(s_msg_cache, sizeof(msg_cache_entry_t) * MSG_CACHE_SIZE);
    }
    // Null static LVGL pointers (existing code)
    s_chat_container = NULL;
    s_input_textarea = NULL;
    // ... other pointer nulling
}
```

**On contact switch (when loading a different conversation):**
```c
static void load_contact_messages(int contact_slot) {
    // Zero previous contact's cached messages first
    if (s_msg_cache != NULL) {
        sodium_memzero(s_msg_cache, sizeof(msg_cache_entry_t) * MSG_CACHE_SIZE);
    }
    s_msg_cache_count = 0;
    // Load new contact's messages from encrypted SD card
    // ...
}
```

**On display timeout (screen lock trigger):**
```c
void simplego_screen_lock(void) {
    // Zero message cache
    if (s_msg_cache != NULL) {
        sodium_memzero(s_msg_cache, sizeof(msg_cache_entry_t) * MSG_CACHE_SIZE);
    }
    s_msg_cache_count = 0;
    // Clear LVGL labels (see SEC-04 fix below)
    // Navigate to lock screen
    ui_manager_navigate(SCREEN_LOCK);
}
```

**On device shutdown / deep sleep:**
```c
void simplego_secure_shutdown(void) {
    // Zero ALL sensitive RAM before power down
    if (s_msg_cache != NULL) {
        sodium_memzero(s_msg_cache, sizeof(msg_cache_entry_t) * MSG_CACHE_SIZE);
    }
    // Zero ratchet state array
    extern ratchet_state_t *ratchet_states;
    if (ratchet_states != NULL) {
        sodium_memzero(ratchet_states, sizeof(ratchet_state_t) * 128);
    }
    // Zero frame pool
    // ... (already done on individual frame release, belt-and-suspenders here)
    // Enter deep sleep or power off
}
```

### SEC-04 Fix: LVGL Label Clearing

LVGL labels containing message text must be explicitly cleared before screen navigation. Simply destroying the LVGL screen is not sufficient because LVGL may reuse the memory pool region without zeroing it.

```c
static void clear_chat_bubbles(void) {
    for (int i = 0; i < VISIBLE_BUBBLE_COUNT; i++) {
        if (s_bubble_labels[i] != NULL) {
            lv_label_set_text(s_bubble_labels[i], "");  // Overwrite label text buffer
            // Note: LVGL internally reallocs the text buffer.
            // The old buffer content remains in the LVGL pool until reused.
            // This is a known LVGL limitation - no way to securely zero freed pool memory.
        }
    }
}
```

**LVGL pool limitation:** LVGL's internal memory pool does not support secure zeroing of freed allocations. When a label's text is changed, the old text buffer is freed back to the pool but not zeroed. The text content persists until the pool region is reused by another allocation. For complete protection, the entire LVGL pool would need to be zeroed on screen lock, which would require reinitializing LVGL - a disruptive operation. For Class 1, the combination of label text clearing (overwrites the live buffer) and screen destruction (releases pool memory) provides reasonable protection. Full pool zeroing is planned for Class 3 (Vault model with supercapacitor-backed zeroization).

### Verification Test

After implementation, this test procedure verifies the fix:

```
1. Send/receive 10+ messages in a conversation
2. Trigger display timeout (or manually call simplego_screen_lock())
3. Connect JTAG debugger to powered device
4. Dump PSRAM region: dump memory 0x3C000000 0x800000
5. Search dump for any sent/received message text
6. Expected result: ZERO occurrences of message text in PSRAM
7. Search LVGL pool region for message text
8. Expected result: ZERO occurrences (or only partial fragments from pool reuse)
```

---

## Additional Zeroing Points

Beyond SEC-01 and SEC-04, the following zeroing operations should be implemented for defense-in-depth:

### NVS key material after load

When ratchet keys are loaded from NVS into the ratchet state array, the intermediate buffers used during the NVS read should be zeroed:

```c
// After loading ratchet state from NVS
uint8_t key_buffer[32];
nvs_get_blob(handle, "ratchet_key", key_buffer, &length);
memcpy(&ratchet_states[slot].chain_key, key_buffer, 32);
mbedtls_platform_zeroize(key_buffer, sizeof(key_buffer));  // Zero the temp buffer
```

### Decrypted SMP frame payload

After an SMP frame is decrypted (all three layers) and the JSON payload is parsed, the decrypted plaintext buffer should be zeroed before the frame is returned to the pool:

```c
// After parsing the decrypted message content
extract_message_text(decrypted_payload, &parsed_message);
sodium_memzero(decrypted_payload, payload_length);  // Zero plaintext
// Frame pool release already calls sodium_memzero (existing, correct)
```

### WiFi credentials in transit

WiFi passwords are passed as strings to `esp_wifi_connect()`. After the connection is established, any local copies should be zeroed. Note that the WiFi driver internally stores credentials - SimpleGo cannot zero the driver's internal copy, but can zero its own buffers.

---

## Future Class 2/3 Features: Duress PIN and Dead Man's Switch

The following features were discussed in Session 27 and Session 36 for the Vault model. They extend the runtime memory protection concept from "zero on transition" to "zero on demand" and "zero on timeout."

### Duress PIN

A secondary unlock code that appears to function normally but triggers immediate and irreversible key destruction:

```
User enters Duress PIN on lock screen
  --> Device appears to unlock normally
  --> In background: sodium_memzero() on ALL sensitive RAM
  --> NVS partition erased and reinitialized (empty)
  --> SD card encryption keys zeroed (history becomes unrecoverable)
  --> Ratchet states destroyed (all contacts must be re-established)
  --> Device shows empty contact list (plausible: "new device" appearance)
```

The Duress PIN is stored as a separate hash in NVS (not in plaintext). The device gives no visual indication that the Duress PIN was entered rather than the real PIN. Forensic analysis of the device afterward shows a legitimately empty device with a functioning vault - indistinguishable from a freshly provisioned unit.

### Dead Man's Switch

Automatic key destruction after a configurable period of inactivity:

```
Last successful unlock: timestamp stored in NVS
Boot sequence checks: (current_time - last_unlock) > configured_timeout?
  If YES:
    --> Same destruction sequence as Duress PIN
    --> Device boots to "welcome / setup" screen
  If NO:
    --> Normal boot
```

The timeout is configurable (24 hours to 30 days). The timer resets on every successful unlock with the real PIN. The Dead Man's Switch protects against the scenario where a device is seized and held in evidence storage without being powered on - the first boot after the timeout period triggers destruction.

**Implementation note:** This requires reliable timekeeping across power cycles. The ESP32-S3's RTC (Real-Time Clock) maintains time during deep sleep if the RTC power domain is active. For Class 3, a coin cell battery backup for the RTC is planned to ensure time continuity even during complete power removal.

### Wipe Code

A separate code (distinct from both the real PIN and the Duress PIN) that triggers a full device wipe including firmware re-flash to factory state. Unlike the Duress PIN which tries to be invisible, the Wipe Code is an explicit "destroy everything" action for when the user knows the device is compromised and does not need plausible deniability.

### Supercapacitor-Backed Zeroization (Class 3 Only)

For Class 3 (Vault), a supercapacitor provides enough energy to complete a full RAM zeroing sequence even if external power is suddenly removed. The target is sub-100-nanosecond zeroization of all sensitive RAM regions. This protects against the "unplug and freeze" cold boot attack variant where the attacker removes power and immediately cools the RAM chips to slow data decay.

The ESP32-S3's SRAM uses static RAM cells (not DRAM), which have faster data decay than DRAM but are still readable for a brief window after power removal (milliseconds at room temperature, longer if cooled). The supercapacitor ensures the zeroing code executes before this window expires.

---

## The EncroChat Lesson

In Session 36, we explicitly discussed the EncroChat forensic failure. When Dutch and French law enforcement compromised EncroChat's servers in 2020, they were able to recover "deleted" data from seized devices because the deletion was not cryptographically secure - data was marked as deleted in the filesystem but not actually overwritten. Forensic tools recovered message content, contact lists, and media that users believed they had deleted.

SimpleGo's approach is the opposite: `sodium_memzero()` physically overwrites every byte with zeros. NVS erase followed by reinitialization creates a fresh encrypted partition. SD card encryption keys, once zeroed from RAM, render the encrypted history permanently unrecoverable (the AES-256-GCM encrypted data on the SD card becomes indistinguishable from random noise without the key).

The difference between "deleted" and "destroyed" is the difference between EncroChat's failure and SimpleGo's design goal.

---

## Implementation Priority

| Task | SEC ID | Priority | Dependency | Estimated Effort |
|------|--------|----------|------------|-----------------|
| `sodium_memzero()` on `s_msg_cache` at all transitions | SEC-01 | Critical | None | 2-3 hours |
| LVGL label clearing on screen lock | SEC-04 | High | SEC-01 | 1-2 hours |
| NVS temp buffer zeroing | New | Medium | None | 1 hour |
| Decrypted payload zeroing | New | Medium | None | 1 hour |
| Screen lock with navigation to lock screen | SEC-04 | High | SEC-01 | 3-4 hours |
| Duress PIN | Future | For Class 2/3 | Screen lock implemented | 1-2 days |
| Dead Man's Switch | Future | For Class 2/3 | RTC battery backup | 1-2 days |
| Supercapacitor zeroization | Future | For Class 3 only | Custom PCB | Hardware design |

SEC-01 and SEC-04 are Session 44 deliverables. The remaining items are documented here for completeness and will be implemented in future sessions as their dependencies are met.

---

## References

| Source | Description |
|--------|-------------|
| libsodium documentation: sodium_memzero | Secure memory zeroing guarantees |
| CWE-14 | Compiler Removal of Code to Clear Buffers |
| Session 42 fix (SEC-03) | `mbedtls_platform_zeroize` replacing `memset` in `smp_storage.c` |
| Session 27 discussion | RAM protection options, Duress PIN concept |
| Session 36 discussion | Secure deletion, EncroChat comparison, Dead Man's Switch |
| EncroChat takedown (2020) | Europol: Operation Venetic forensic recovery |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
