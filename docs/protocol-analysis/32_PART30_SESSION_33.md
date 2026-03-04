# Part 30: Session 34 — Multi-Contact Architecture (Singleton to Per-Contact)
**Date:** 2026-02-23 | **Version:** v0.1.17-alpha

## Overview

8 commits in one session — most productive session in the project's history. Transformed SimpleGo from a single-contact singleton architecture to a full multi-contact system with per-contact reply queues, runtime contact creation, per-contact 42d tracking, and production-ready logging. Open bug: KEY command rejected by server, handed over to Claude Code for Haskell line-by-line analysis.

## Phase 1: Production Cleanup (2 Commits)

**Commit 1: `refactor(ratchet): strip debug dumps, fix index range for 128 contacts`**
Removed all private key dumps from decrypt path (DH secrets, message keys, cleartext). Fixed index range from hardcoded 31 to MAX_CONTACTS. Fixed `ratchet_save_state(0)` to `ratchet_save_state(ratchet_get_active())`. Net result: -126 lines.

**Commit 2: `refactor(cleanup): remove debug dumps and test artifacts for production`**
Removed "Hello from ESP32!" auto-message (test artifact). Removed "FULL KEYS FOR PYTHON TEST" dump blocks. All 32-byte hex dumps reduced to 4-byte fingerprints or ESP_LOGD level. Net result: -80+ lines.

Result: Zero private keys in logs. Zero full key dumps. Production-ready logging with fingerprints only.

## Phase 2: Runtime Add-Contact (1 Commit)

`feat(contacts): runtime add-contact via network command`

```
UI Task → app_request_add_contact() → kbd_msg_queue → smp_app_run()
  → NET_CMD_ADD_CONTACT → app_to_net Ring Buffer → Network Task
  → create queue (NEW) → show QR code
```

NET_CMD_ADD_CONTACT sent via Ring Buffer from Main to Network Task. Public API `app_request_add_contact()` for UI. Network Task handler creates queue and displays QR invitation. Same Ring Buffer command pattern (NET_CMD_*) applies to any operation requiring network access from non-network tasks.

## Phase 3: Per-Contact 42d Tracking (1 Commit)

`feat(handshake): per-contact 42d tracking with bitmap`

Replaced singleton `static bool is_42d_done` with 128-bit bitmap:

```c
static uint32_t handshake_done_bitmap[4] = {0};  // 128 bits, 16 bytes

static inline bool is_42d_done(int idx) {
    return (handshake_done_bitmap[idx / 32] >> (idx % 32)) & 1;
}
static inline void mark_42d_done(int idx) {
    handshake_done_bitmap[idx / 32] |= (1u << (idx % 32));
}
```

All `contacts[0]` references in the 42d block parameterized to `contacts[hs_contact]`. Each contact can run its own independent handshake. 16 bytes for 128 contacts vs 128 bytes for a bool array.

## Phase 4: UI Contact List (1 Commit)

`feat(ui): add-contact button with auto-naming and QR flow`

[+ New Contact] row added to contact list screen. Auto-name generator: "Contact 1", "Contact 2", ... (next available number). Fixed iteration bug: loop used num_contacts instead of MAX_CONTACTS, missing gaps in the contact array. Contact list refreshes on navigation, not just on boot.

## Phase 5: Per-Contact Reply Queue Architecture (2 Commits)

**Data Structure:**
```c
typedef struct {
    uint8_t rcv_id[24];           // Queue receive ID
    uint8_t snd_id[24];           // Queue send ID
    uint8_t rcv_private_key[32];  // Ed25519 private (for signing)
    uint8_t rcv_dh_private[32];   // X25519 private (for E2E)
    uint8_t rcv_dh_public[32];    // X25519 public (sent to peer)
    uint8_t snd_public_key[32];   // Peer's sender auth key
    uint8_t e2e_peer_dh[32];     // Peer's DH public from PHConfirmation
    bool    valid;                // Slot in use
    bool    key_sent;             // KEY command completed
    bool    subscribed;           // SUB completed
    char    server_host[64];      // SMP relay hostname
} reply_queue_t;
```

128 slots in PSRAM via `heap_caps_malloc(128 * sizeof(reply_queue_t), MALLOC_CAP_SPIRAM)`. ~384 bytes per slot, ~49KB total. NVS persistence via `rq_00` through `rq_127`. `reply_queue_create()` sends NEW command over Main SSL connection. `find_reply_queue_by_rcv_id()` for incoming message routing.

**Protocol Wiring:** MSG routing uses per-contact reply queue for decrypt. subscribe_all_contacts() extended with Reply Queue subscription loop. NET_CMD_SEND_KEY command for KEY via Ring Buffer. CONFIRMATION built with per-contact queue IDs and DH public key. E2E peer key stored per-contact from PHConfirmation.

## Phase 6: SMP v7 Signing Fix (1 Commit)

`fix(smp): correct SMP command signing format (1-byte session prefix)`

After Reply Queue creation, SUB commands were rejected. Root cause: SMP v7 command signing uses 1-byte length prefixes for corrId and entityId in the signing buffer, not 2-byte Large-encoded. The wrong prefix length caused signature verification failure on the server, affecting SUB, KEY, and NEW commands simultaneously.

```
WRONG:  [2B corrLen][corrId][2B entLen][entityId][command]  (signed)
RIGHT:  [1B corrLen][corrId][1B entLen][entityId][command]  (signed)
```

NETWORK_TASK_STACK increased from 20KB to 32KB to accommodate buffer allocations inside reply_queue_create().

## Open Bug: KEY Command Rejected

KEY command sequence: ESP32 creates Reply Queue (NEW) → gets rcvId+sndId → Peer sends sender_auth_key in PHConfirmation → ESP32 must send KEY ("this public key is authorized to send on my queue") → only after KEY can phone send messages to ESP32.

Symptoms: Server does not respond OK to KEY. Phone shows "you can't send messages yet." Phone status stuck on "connecting." Possible causes: wire format of KEY body incorrect, wrong signing key, wrong entity ID (rcvId vs sndId), command body structure. Handed over to Claude Code for line-by-line Haskell comparison per Evgeny's 100x-reading recommendation.

## PSRAM Usage After Session 34

| Module | Size | Slots |
|--------|------|-------|
| Ratchet States | 66,560 B | 128 |
| Handshake States | 7,296 B | 128 |
| Contacts DB | 35,200 B | 128 |
| Reply Queue Array | ~49,152 B | 128 |
| **Total** | **~158 KB (1.9%)** | |

All four major data structures support 128 contacts simultaneously in PSRAM.

## What Works / What's Blocked

Working: QR code scan with SimpleX App, INVITATION receive and parse, X3DH Key Agreement (X448), CONFIRMATION send (accepted), PHConfirmation receive and decrypt, Double Ratchet decrypt (ConnInfo Tag='I'), HELLO send to peer, Contact Queue SUB, Legacy Reply Queue SUB (after signing fix), ESP32→Phone messages.

Blocked: KEY command rejected → peer cannot send to Reply Queue. Phone→ESP32 messages: "you can't send messages yet." Phone status stuck on "connecting." Second QR code: stack overflow (32KB fix prepared, not tested).

## Files Changed

New files: `main/protocol/reply_queue.c` and `.h`. Changed (10 files): smp_contacts.c/h, smp_tasks.c/h, smp_events.h, main.c, smp_ratchet.c, smp_queue.c, ui_contacts.c, ui_manager.c.

All 8 commits: refactor(ratchet) strip debug dumps, refactor(cleanup) remove test artifacts, feat(contacts) runtime add-contact, feat(handshake) per-contact 42d bitmap, feat(ui) add-contact button, feat(queue) per-contact reply queue array, feat(queue) wire reply queues into protocol, fix(smp) correct SMP command signing.

## Lessons Learned

**L169 (CRITICAL):** Strip ALL private keys from logs before production. All 32-byte key dumps must be replaced with 4-byte fingerprints or moved to ESP_LOGD (disabled in release builds).

**L170 (HIGH):** Runtime add-contact uses Ring Buffer command pattern. UI triggers intent → Main Task packages NET_CMD_ADD_CONTACT → Ring Buffer → Network Task creates queue. Same pattern for any operation requiring network access from non-network tasks.

**L171:** Per-contact 42d tracking with 128-bit bitmap (uint32_t[4]) uses 16 bytes total. O(1) set/check via bit manipulation.

**L172 (CRITICAL):** SMP v7 signing uses 1-byte length prefixes for corrId and entityId, not 2-byte Large-encoded. Wrong prefix length causes signature verification failure affecting SUB, KEY, and NEW simultaneously.

**L173 (HIGH):** Per-contact reply queue array in PSRAM: ~384 bytes per slot, 128 slots = ~49KB. Combined with other structures, total PSRAM usage ~158KB (1.9%).

**L174 (HIGH):** When a command is rejected without clear error, the only reliable approach is byte-level comparison with the Haskell reference implementation following Evgeny's "100x reading to writing" ratio.

**L175:** Stack size must account for buffer allocations in called functions. reply_queue_create() allocates large buffers on stack; Network Task increased from 20KB to 32KB to prevent overflow.
