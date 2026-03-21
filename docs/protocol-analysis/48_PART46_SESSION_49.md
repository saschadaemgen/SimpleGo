# Part 46: Session 49 - Queue Rotation: From Zero to Working
**Date:** 2026-03-18 to 2026-03-21 | **Version:** v0.1.18-alpha | **Duration:** 4 days (longest session)

## Overview

Session 49 implemented Queue Rotation as a complete feature - the ability to switch SMP servers during live operation without losing contacts. Additionally cleaned up the multi-server infrastructure (21 preset servers, radio-button selection, SEC-07 fingerprint verification at four connection points) and closed Bug #32 (second contact handshake failure). The QADD format alone required 7 test iterations over 2 days before acceptance, uncovering three critical protocol rules undocumented outside the Haskell source.

## Bug #32 CLOSED: Second Contact Handshake Failure

**Symptom:** Second contact stuck at "Waiting for Peer" with timeout.

**Misdiagnosis (3 hours lost):** Hasi analyzed PQ handshake path and built code for PQ in Confirmation. Completely wrong direction - PQ is NOT negotiated in the handshake but later in the Ratchet message exchange.

**Correct diagnosis:** Mein Prinz tested on Session 47 code (git checkout 8fff7a6) - everything worked. Git diff showed: Session 48 had completely removed app_request_subscribe_all().

**Fix:** subscribe_all restored at three points in smp_tasks.c. Point A: after add_contact(). Point B: after reply_queue_create(). Point C: after 42d handshake completion.

## Multi-Server Management

### Server Data Structure

New files smp_servers.c/h with 21 preset servers: 14 SimpleX (Storage+Proxy), 6 Flux (Storage+Proxy), 1 SimpleGo smp.simplego.dev (Storage+Proxy, default). Fingerprints as Base64 in code, runtime decoding. NVS blob "srv_list".

**CORRECTION:** Flux servers have Storage+Proxy, NOT Proxy-only. The code analysis from Presets.hs was wrong. Mein Prinz verified in the SimpleX App directly.

### SEC-07: TLS Fingerprint Verification (4 Connection Points)

Three locations verify TLS certificate hash against stored fingerprint for OUR server: smp_queue.c (queue creation), main.c (boot connection), smp_tasks.c (reconnect). Fourth location in smp_peer.c verifies PEER server fingerprint extracted from smp://FINGERPRINT@host URI by smp_parser.c.

### Server UI Cleanup

Operator structure with use_for_proxy and use_for_recv discovered as fake features (no backend) and removed. Flat radio-button list implemented. One server active at a time, switch with confirmation popup and automatic reboot.

### Server-Switch Override

smp_connect() checks whether existing session exists (rat_00 in NVS). If yes: connect to the queue's server, not the user-selected active server. Protects existing contacts until Queue Rotation migrates them to the new server.

## Dual-TLS Test

Each additional TLS connection costs ~1,500 bytes SRAM. With three simultaneous connections: 6,351 bytes free. 30 seconds stable. Queue Rotation technically feasible but SRAM budget is tight - maximum 2-3 simultaneous TLS connections safe. Three causes sdmmc DMA failures.

## Queue Rotation (Main Work - 4 Phases)

### Phase 0: Protocol Research

Evgeny's PR #1726 (spec/agent/connections.md, commit 7aefcbf) discovered as authoritative source. Queue Rotation uses four Agent Messages over the existing Double Ratchet:

| Message | Tag | Direction | Purpose |
|---------|-----|-----------|---------|
| QADD | "QA" | Us to peer | "I created a new queue on a new server, here's the address" |
| QKEY | "QK" | Peer to us | "Here's my sender key for your new queue" |
| QUSE | "QU" | Us to peer | "I'm now listening on the new queue, switch your send target" |
| QTEST | "QT" | Peer to us | "I sent a test message on the new queue, confirming it works" |

### Phase 1: Infrastructure (11 files)

**smp_rotation.h/c:** State machine per contact (IDLE/QADD_SENT/QKEY_RECEIVED/QUSE_SENT/DONE), NVS persistence, QADD payload builder with per-contact reply queue credentials.

**smp_handshake.h/c:** New send_raw_agent_message() - generalized send path for rotation messages that bypass the normal chat message flow.

**smp_agent.c:** QA/QK/QU/QT tag recognition in the agent message parser.

**smp_tasks.c:** Second TLS connection management, rotation loop (one contact per iteration to manage SRAM budget).

**smp_peer.h/c:** peer_send_raw_agent_msg() for sending rotation messages to peer's server.

### QADD Format Debugging (2 Days)

Seven variants tested before the app accepted QADD:

| Test | Format | Result |
|------|--------|--------|
| v1 | v6-v7, count=2 | App silent |
| v2 | v8-v16, count=2 | App silent |
| v3 | v8-v16, count=1 | App silent |
| v4 | v8-v16, count=1, 'T' flag | App silent |
| v5 | **v1-v4**, count=1 | **A_QUEUE error - breakthrough!** |
| v6 | v1-v4, replacedSndQueue=Nothing | "adding queue without switching is not supported" |
| v7 | v1-v4, **per-contact snd_id** | **QKEY received!** |

### Three Critical Protocol Rules Discovered

**Rule 1: SMP Client Versions are v1-v4, not v6-v17.** These are two completely separate numbering systems. The server reports its SMP transport versions (v6-v17), but the clientVRange field in QADD must be v1-v4 (agent-level client versions). Using v6+ caused silent rejection.

**Rule 2: replacedSndQueue = Nothing is forbidden.** Haskell line 3403 in qAddMsg: "adding queue without switching is not supported." The replacedSndQueue field MUST contain the old queue's sender ID. Nothing is explicitly rejected.

**Rule 3 (THE BUG): Global our_queue.snd_id instead of per-contact reply_queue.snd_id.** rotation_build_qadd_payload() was using the global our_queue.snd_id for the replacedSndQueue field across ALL contacts. The app searches (server, snd_id) tuples in its SndQueues using findQ and found no match because each contact has its own unique snd_id. Fix: use reply_queue_get(contact_idx)->snd_id (field: snd_id[QUEUE_ID_SIZE], length: snd_id_len from reply_queue.h).

**Additional finding:** findQ in Haskell compares only (SMPServer, SenderId) tuples. The keyHash is NOT compared by sameSrvAddr. This means the server address + queue ID is sufficient for queue identification.

### Phase 2: QKEY + KEY

App responds with QKEY after correct QADD containing the sender's public authentication key for the new queue. KEY command on the new server registers this sender key, authorizing the peer to send messages. Worked on first attempt after QADD was accepted.

### Phase 3: QUSE + QTEST (2 more format iterations)

**QUSE format fix:** Initial implementation sent only "QU" (2 bytes). Correct format: "QU" + count(1 byte) + SMPServer + SenderId + 'T' (primary flag). The SMPServer and SenderId tell the peer which queue to switch to, and 'T' marks it as the primary queue.

**QTEST direction fix:** SimpleGo was SENDING QTEST to the peer. But the protocol is asymmetric - the APP sends QTEST to US on the new queue. QTEST builder completely removed from SimpleGo, QTEST receive handler implemented instead. The test message confirms the new queue is operational.

### Phase 4: Live-Switch (Instead of Reboot)

Original plan was reboot after credential migration. Mein Prinz correctly rejected this - too complex, too many edge cases (late-arriving messages after reboot, second TLS during boot, etc.).

**Live-Switch implementation:** Credentials overwritten in RAM, NVS saved, old connection closed, connect to new server, subscribe. No reboot, no migration screen.

**DH Key Dilemma:** After rotation, receiving needs NEW DH keys (new server), but sending needs OLD DH keys (peer's server unchanged). Solution: reply_queue_t extended with peer_dh_secret, peer_dh_public, has_peer_dh fields. rotation_complete() saves old keys before overwriting.

**Auth Key Fix:** rotation_complete() initially overwrote rcv_auth and rcv_dh fields. But the peer-send path needs those for signing messages to the peer's server. Fix: overwrite ONLY IDs and server-DH fields, keep auth/DH keys for peer-send intact.

### End Result

Bidirectional chat after live server switch: sending, receiving, delivery receipts, SD card history, post-quantum crypto - all working after live-switch. No reboot required.

## Known Issues for Session 50

| # | Problem | Severity |
|---|---------|----------|
| 1 | Second rotation crashes | HIGH - state/keys not reset after cleanup |
| 2 | RQ SUB "Non-matching frame" | MEDIUM - reply queue auth keys wrong on new server, 30s boot delay |
| 3 | Chat display 10s delay | MEDIUM - RQ SUB retries block App Task after rotation |
| 4 | Refresh timer runs endlessly | LOW - 1s timer doesn't stop after rotation DONE |
| 5 | CQ E2E peer key only for first contact | LOW - multi-contact needs per-contact keys |
| 6 | Late-arrival flow (Fix 5) | HIGH - offline contacts need second TLS to old server |

## Files Changed/Created (16 files)

| File | Status |
|------|--------|
| smp_rotation.h (main/include/) | NEW |
| smp_rotation.c (main/state/) | NEW |
| smp_servers.h (main/include/) | REPLACED (cleaned) |
| smp_servers.c (main/state/) | REPLACED (cleaned) |
| smp_handshake.h/c (main/include/, main/protocol/) | MODIFIED (+send_raw_agent_message) |
| smp_agent.c (main/protocol/) | MODIFIED (+QA/QK/QU/QT tags) |
| smp_tasks.c (main/core/) | MODIFIED (rotation loop, second TLS, CQ decrypt) |
| smp_peer.h/c (main/include/, main/state/) | MODIFIED (+peer_send_raw_agent_msg, +peer_dh) |
| smp_contacts.c (main/state/) | MODIFIED (RQ SUB skip) |
| reply_queue.h (main/include/) | MODIFIED (+peer_dh fields) |
| ui_settings_info.c (main/ui/) | MODIFIED (radio-button, rotation start) |
| ui_contacts.c (main/ui/) | MODIFIED (status texts, refresh timer) |
| ui_chat_bubble.c (main/ui/screens/) | MODIFIED (null-check) |
| main.c (main/) | MODIFIED (get_active, Bug A guard) |

## Evgeny References (Session 49)

No direct contact. PR #1726 (spec/agent/connections.md, commit 7aefcbf) was the authoritative source for Queue Rotation protocol. Without this documentation, weeks of Haskell reverse-engineering would have been needed.

Confirmed rules from earlier sessions: Write-Before-Send (all keys to NVS before network call), "adding queue without switching is not supported" (qAddMsg line 3403), ERR AUTH = signature mismatch only (Server.md), subscribed clients never idle-disconnected.

## Lessons Learned

**L258 (CRITICAL):** PQ negotiation happens in the Ratchet message exchange, NOT in the handshake. Confirmation sends PQ=0. Misdiagnosis cost 3 hours.

**L259 (CRITICAL):** subscribe_all after handshake completion is needed for the next contact. Removing it (Session 48 optimization) broke second contact creation.

**L260 (HIGH):** No fake features. No UI toggle without a working backend. Operator structure with use_for_proxy/use_for_recv was pure theater.

**L261 (HIGH):** Code analyses can be wrong. Verify against the live app. Flux server capabilities were incorrectly read from Presets.hs.

**L262 (HIGH):** git checkout + test is the fastest bug diagnosis. Testing Session 47 code immediately proved the regression.

**L263 (MEDIUM):** ESP32-S3 SRAM limits to 2-3 simultaneous TLS connections. ~1,500 bytes per connection. Three connections leave only 6,351 bytes. sdmmc DMA failures at three simultaneous connections.

**L264 (CRITICAL):** SMP Client Version Range is v1-v4, not v6-v17. These are separate numbering systems (agent-level vs transport-level). Always verify against Haskell version constants.

**L265 (HIGH):** findQ compares only Host + Port + QueueId. KeyHash is NOT compared by sameSrvAddr. Server address + queue ID is sufficient for identification.

**L266 (CRITICAL):** replacedSndQueue must contain the per-contact reply_queue snd_id, not a global ID. Each contact has its own unique snd_id. Using a global ID causes findQ to fail for all contacts except the one that happens to match.

**L267 (HIGH):** QTEST is asymmetric. The app sends QTEST to us on the new queue, not the other way around. Building and sending QTEST was wasted effort.

**L268 (CRITICAL):** Auth/DH keys for peer-send must NOT be overwritten during rotation. Receiving uses new server keys, sending uses old peer keys. reply_queue_t needs separate peer_dh fields.

**L269 (HIGH):** Live-switch instead of reboot for server changes. No reboot, no migration screen, no edge cases with late arrivals during restart.

**L270 (HIGH):** Hasi must deliver complete ready-to-use files, not construction instructions. Partial code snippets waste time and introduce integration bugs.

---

*Part 46 - Session 49: Queue Rotation from Zero to Working*
*SimpleGo Protocol Analysis*
*Date: March 18-21, 2026*
*Duration: 4 days (longest session)*
*First SMP Queue Rotation implementation outside Haskell*
*QADD/QKEY/QUSE/QTEST operational with live server switch*
*Bugs: 81 total (Bug #32 closed)*
*Lessons: 270 total (13 new: L258-L270)*
*21 preset servers, SEC-07 fingerprint verification at 4 points*
