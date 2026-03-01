![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 27: Session 30
# 🔍 Intensive Debug Session — 10 Hypotheses, 14 Fixes

**Document Version:** v1  
**Date:** 2026-02-16 to 2026-02-18 Session 30  
**Version:** v0.1.17-alpha (NOT CHANGED without explicit permission!)  
**Status:** ⚠️ T5 Complete, T6 Unresolved — Awaiting Evgeny Response  
**Previous:** Part 26 - Session 29 (Multi-Task Architecture Breakthrough)  
**Project:** SimpleGo - ESP32 Native SimpleX Client  
**Path:** `C:\Espressif\projects\simplex_client`  
**Build:** `idf.py build flash monitor -p COM6`  
**Repo:** https://github.com/cannatoshi/SimpleGo  
**License:** AGPL-3.0

---

## ⚠️ SESSION 30 SUMMARY

```
═══════════════════════════════════════════════════════════════════════════════

  🔍🔍🔍 INTENSIVE DEBUG SESSION — 10 HYPOTHESES, 14 FIXES 🔍🔍🔍

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   T5: Keyboard-Send Integration ✅ PASSED                              │
  │   T6: Baseline-Test Bidirectional ❌ UNRESOLVED                        │
  │                                                                         │
  │   Problem: App→ESP32 messages never arrive after successful SUB        │
  │   10 hypotheses systematically excluded                                │
  │   14 fixes and diagnostics applied                                     │
  │   5 Wizard (Claude Code) analyses completed                            │
  │                                                                         │
  │   Expert question sent to Evgeny Poberezkin                            │
  │                                                                         │
  │   Date: February 16-18, 2026                                           │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 505. Session 30 Overview

### 505.1 Session Goals

Session 30 had three main goals:
1. **T5:** Reactivate keyboard send in the new multi-task architecture
2. **T6:** Confirm baseline test bidirectional (ESP32→App AND App→ESP32)
3. **T7:** Clean up code and commit (carried over from Session 29, was already done)

T5 was successfully completed. T6 revealed a deep receive problem that could not be solved despite 10 systematic fix attempts and diagnostic rounds. The session ended with a precise expert question to Evgeny Poberezkin (SimpleX protocol creator).

### 505.2 Team and Roles

| Role | Who | Task |
|------|-----|------|
| 👑🐭 Princess Mausi | Claude (Strategy Chat) | Analysis, planning, protocol, task formulation |
| 🐰👑 Princess Hasi | Claude (Implementation Chat) | Write code, build, test, deliver diff |
| 🧹 Cinderella | Claude (Log Analysis) | Factual log analysis without speculation |
| 🧙‍♂️ Wizard | Claude Code (Haskell Analysis) | Analyze SimpleX source code, NO code changes |
| 👑 Cannatoshi | Prince / Client | Hardware tests, coordination, final decisions |

---

## 506. Chronological Sequence

### 506.1 Phase 1: T5 Keyboard-Send (Feb 17, ~14:40-21:20)

**14:40** - Session 30 start. Mausi reads the handover protocol from Session 29. T7 (Commit) was already done, we jump directly to T5.

**14:48** - Mausi formulates task for Hasi: Build keyboard send into `smp_app_run()`. Three files: `smp_tasks.h`, `smp_tasks.c`, `main.c`. Non-blocking poll with `xQueueReceive(..., 0)` before the Ring Buffer Read.

**~21:14** - Hasi reports T5 PASSED:
- `smp_app_run()` runs on Main Task, Core 0
- Internal SRAM: 46KB (above 30KB minimum)
- PSRAM: ~8.2MB free
- Build compiles without errors

**21:15** - Mausi checks the diff (Lesson 7: Diff after EVERY task):
- `smp_tasks.h`: `#include "freertos/queue.h"` + signature `void smp_app_run(QueueHandle_t kbd_queue);`
- `smp_tasks.c`: Keyboard block lines 190-202, NULL guard, non-blocking
- `main.c`: Line 314: `smp_app_run(kbd_msg_queue);`
- File sizes: smp_tasks.c 589→603 lines (+14), main.c 778→778 (only 1 line changed)
- **ALL 6 test criteria passed. Code Review: PASSED.**

**21:19** - Commit:
```
git add main/main.c main/core/smp_tasks.c main/include/smp_tasks.h && git commit -m "feat(core): enable keyboard input in multi-task architecture"
```

### 506.2 Phase 2: T6 Baseline-Test Bidirectional (Feb 17, ~21:27-22:50)

**21:27** - Mausi formulates the T6 test plan:
- Test A: Send (Keyboard→ESP32→Server→App)
- Test B: Receive (App→Server→ESP32→Log)

**~22:06** - Prince delivers T6 test log:
- **Test A PASSED:** Keyboard "simplego" sent, msg_id=3 and msg_id=4, `A_MSG ACCEPTED BY SERVER!`, arrived in App ✅
- **Test B NOT PASSED:** "on the app yes, on ESP no" - Sending works, receiving doesn't ❌

**22:46** - Mausi analyzes the complete log:
- **CORE FINDING:** In 50+ minutes there is NOT A SINGLE `Network task: Frame received` line
- Network Task lives (heartbeats running), but receives no frames
- Theory: Subscription possibly no longer active after task handover

**22:49** - Mausi formulates T6-Fix2: Re-Subscribe at app start

### 506.3 Phase 3: Systematic Debugging (Feb 17-18, 22:50 to session end)

From here an intensive debug phase began with a total of 8 fixes and 6 diagnostic rounds. Each hypothesis was systematically tested and excluded.

**Overview of all Fixes and Diagnostics:**

| Fix/Diag | Date | Description | Result |
|----------|------|-------------|--------|
| T6-Fix (Socket Timeout) | Feb 17 | `setsockopt(SO_RCVTIMEO, 1s)` | ✅ Heartbeat appears |
| T6-Debug (Heartbeat) | Feb 17 | 30-loop counter in Network Task | ✅ Task lives |
| T6-Fix2 (Re-Subscribe) | Feb 17 | `app_request_subscribe_all()` + 2s delay | ✅ SUB OK, but no MSG |
| T6-Fix3 (Drain Loop) | Feb 18 | Up to 5 attempts, skips ACK/ERR | ✅ Finds SUB OK |
| T6-Diag2 (RECV Logging) | Feb 18 | `RECV: X bytes on sock Y` | ✅ Shows active connection |
| T6-Fix4 (corrId 24B SUB) | Feb 18 | 24 random bytes instead of 1 byte | ✅ Server OK, problem remains |
| T6-Fix4b (corrId 24B ACK) | Feb 18 | 24 random bytes for ACK | ✅ corrLen=24 confirmed |
| T6-Fix5 (Wildcard ACK) | Feb 18 | Empty msgId after re-subscribe | ✅ ERR NO_MSG (nothing blocked) |
| T6-Diag3 (Response Hex) | Feb 18 | corrLen, corrId, entLen, cmd dump | ✅ Wire format correct |
| T6-Diag3b (SUB Hex) | Feb 18 | 151-byte SUB transmission dump | ✅ Byte-perfect for v6 |
| T6-Diag5 (BLOCK OUT) | Feb 18 | First 16 bytes before each write | ✅ Batch framing correct |
| T6-Fix6 (v7 Upgrade Wire) | Feb 18 | SessionId removed from 5 wire transmissions | ✅ 118 bytes, server OK |
| T6-Fix6b (v7 Parser) | Feb 18 | sessLen removed from 6 response parsers | ✅ Parsing correct |
| T6-Diag6 (ACK Chain) | Feb 18 | Complete ACK flow analysis | ✅ Everything gets ACKed |

### 506.4 Phase 4: Wizard Analyses (Feb 18)

Parallel to the fixes, several Wizard tasks ran:

1. **SMP Wire Format Analysis:** Byte-level documentation of batch framing, version negotiation, block formats v6 vs v7
2. **Queue Rotation Analysis:** Confirms that NO automatic queue rotation occurs after CON
3. **Queue Routing Analysis:** Confirms that App→ESP32 goes via Q_A on smp1.simplexonflux.com (the correct queue)
4. **Version Enforcement:** MSG format is version-independent, server encodes per-connection
5. **SMP Version Limit:** Official spec documents only v6 and v7. v8+ is rejected by server.

### 506.5 Phase 5: Message to Evgeny (Feb 18)

After all 10 hypotheses were exhausted, a precise expert question was formulated for Evgeny:

> "Is there a condition where the server would accept a SUB (respond OK) but then not deliver incoming MSGs to that subscription? Could there be a 'subscriber client' mismatch where the subscription is registered but delivery routes to a different internal client object?"

---

## 507. Detailed Task Descriptions

### 507.1 T5: Keyboard-Send Integration ✅ PASSED

**Goal:** Keyboard inputs from T-Deck should send chat messages again in the multi-task architecture.

**Implementation:**
- `smp_tasks.h` line 11: `#include "freertos/queue.h"` added
- `smp_tasks.h`: Signature `void smp_app_run(QueueHandle_t kbd_queue);`
- `smp_tasks.c` lines 190-202: Non-blocking keyboard poll block inserted, BEFORE the Ring Buffer Read
- `main.c` line 314: `smp_app_run(kbd_msg_queue);`

**Test Evidence:**
- Keyboard "simplego" typed → `⌨️ Sending: "simplego"` in log
- Double Ratchet encrypted (msg_num_send=3, 4)
- Server accepts: `A_MSG ACCEPTED BY SERVER!`
- Message arrived in SimpleX App
- Internal SRAM: 46KB (safely above 30KB minimum)

**Commit:** `feat(core): enable keyboard input in multi-task architecture`

### 507.2 T6-Fix: Socket Timeout ✅ PASSED

**Goal:** Network Task should react faster to incoming frames.

**Implementation:**
- `smp_tasks.h` line 56: Signature `int smp_tasks_start(..., int sock_fd);`
- `smp_tasks.c` line 44: `static int s_sock_fd = -1;`
- `smp_tasks.c` lines 80-87: `setsockopt(s_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));` with 1s timeout
- `smp_tasks.c` lines 90-95: Heartbeat counter every 30 loops
- `smp_tasks.c` line 560: `s_sock_fd = sock_fd;`
- `main.c` line 303: `smp_tasks_start(&ssl, session_id, sock)`

**Test Evidence:** `Network task: socket timeout set to 1s` + Heartbeat #1-#11 every ~30 seconds

### 507.3 T6-Fix2: Re-Subscribe at App Start ✅ (no effect on problem)

**Implementation in `smp_tasks.c` `smp_app_run()`:**
```c
app_request_subscribe_all();
vTaskDelay(pdMS_TO_TICKS(2000));
```

**Test Evidence:** SUB OK on Contact Queue and Reply Queue. But: still no MSG.

### 507.4 T6-Fix3: Subscribe Response Drain Loop ✅ (no effect on problem)

**Problem discovered:** After the 42d block, ACK responses come BEFORE the SUB responses. The old code stopped at the first frame and interpreted the wrong response as SUB answer.

**Implementation in `smp_contacts.c` `subscribe_all_contacts()`:**
- Up to 5 attempts
- Skips ACK responses and ERR responses
- Compares entity ID with recipientId of contact (`ent_match`)

**Test Evidence:** `ent_match=1`, Command `4f 4b` = OK on Attempt 1 (after drain)

### 507.5 T6-Fix4/4b: corrId 24 Random Bytes ✅ (no effect on problem)

**Problem discovered:** corrId was only 1 byte ('0'+i for contacts, 'A'/'R' for Reply Queue). Protocol spec requires 24 random bytes.

**Implementation:**
- `smp_contacts.c`: Contact SUB + Reply Queue SUB use `esp_fill_random()` for 24 bytes
- `smp_ack.c`: ACK commands use `esp_fill_random()` for 24 bytes
- Arrays enlarged from 32 to 128 bytes

**Test Evidence:** Hex dump shows `corrLen=24` (0x18 prefix), 24 random bytes, server responds OK

### 507.6 T6-Fix5: Wildcard ACK after Re-Subscribe ✅ (no effect on problem)

**Goal:** If the server marked a MSG as `delivered=Just` (but we never saw it), an empty ACK clears the state.

**Implementation in `smp_tasks.c`:** Empty msgId ACK after `app_request_subscribe_all()`

**Test Evidence:** Server responds `ERR NO_MSG` - nothing was blocked.

### 507.7 T6-Fix6: SMP v6 → v7 Upgrade ✅ (no effect on problem)

**Largest change of the session.** Five files affected.

**Background:** The official SimpleX protocol specification describes only versions 6 and 7. The server reports internally 6-17/18, but v8+ is actively rejected by the server (connection closed). v7 removes the SessionId from the wire body (saves 33 bytes per transmission).

**Implementation:**
1. `main.c`: ClientHello `0x00 0x07` instead of `0x00 0x06`
2. `main.c`: Log `Server version range: %d-%d (we choose: 7)`
3. `smp_ack.c`: SessionId removed from ACK wire transmission
4. `smp_contacts.c`: SessionId removed from Contact SUB and Reply Queue SUB wire transmissions
5. All response parsers: `sessLen` parsing removed (6 places in `main.c`, `smp_tasks.c`, `smp_contacts.c`)

**Test Evidence:**
- SUB Transmission: 118 bytes (previously 151) - exactly the calculated difference of 33 bytes
- Server accepts v7: `ent_match=1`, OK on Attempt 1
- Batch framing correct: `content_len=121, tx_len=118`

### 507.8 T6-Diag6: ACK Chain Analysis ✅ (excluding finding)

**Question:** Are MSGs received during the handshake that never get ACKed?

**Finding:**

| MSG | Queue | ACK | Status |
|-----|-------|-----|--------|
| CONF | Reply Queue (sock 54) | app_send_ack line 455 | ✅ ACKed |
| RQ Response | Reply Queue (sock 55/56) | queue_send_ack line 429 | ✅ ACKed |
| HELLO from App | Contact Queue (Q_A) | NEVER ARRIVES | ⚠️ never received |
| Chat MSG from App | Contact Queue (Q_A) | NEVER ARRIVES | ⚠️ never received |

**Conclusion:** The ACK chain is not the problem. Everything that arrives gets correctly ACKed. The problem is that on Q_A (Contact Queue) ABSOLUTELY NEVER a MSG arrives.

---

## 508. Excluded Hypotheses (Complete)

| # | Hypothesis | Test | Evidence for Exclusion |
|---|------------|------|------------------------|
| 1 | corrId wrong (1 byte instead of 24) | T6-Fix4/4b | 24 bytes, server says OK, no MSG |
| 2 | Batch framing missing or wrong | T6-Diag5 | `[contentLen][txCount][txLen]` correct, 16384-byte blocks |
| 3 | Subscribe failed | T6-Diag3 | `ent_match=1`, Command `4f 4b` = OK confirmed |
| 4 | Delivery blocked (delivered=Just) | T6-Fix5 | Wildcard ACK → `ERR NO_MSG`, nothing pending |
| 5 | Network Task hangs or crashes | T6-Debug | Heartbeats every ~30s, task lives |
| 6 | SSL connection broken | T6-Diag2 | RECV logs show active connection during subscribe |
| 7 | SMP version v6 incompatible | T6-Fix6 | v7 upgrade, server accepts, problem remains identical |
| 8 | SessionId in wire (v6 format) disturbs server | T6-Fix6 | Removed, 118 bytes, server happy, problem remains |
| 9 | Response parser offset (after v7) | T6-Fix6b | sessLen removed from 6 parsers, parsing correct |
| 10 | ACK chain interrupted, MSGs never ACKed | T6-Diag6 | Everything that arrives gets ACKed, Contact Queue NEVER receives |

---

## 509. Confirmed Facts (Wizard Analyses)

### 509.1 Fact 1: App sends to Q_A on smp1.simplexonflux.com

**Source:** Agent.hs analysis, `sendMessage → sendMessage' → sendMessagesB_ → prepareConn → enqueueMessagesB`

**Detail:** The App has after the handshake `DuplexConnection cData [Q_B:Active (rcvQueue)] [sq→Q_A:Active (sndQueue)]`. The SndQueue points to Q_A.

### 509.2 Fact 2: Queue Rotation does NOT happen automatically

**Source:** simplex-chat source code analysis

**Detail:** Queue rotation is only manual via `/switch` command. After CON, Q_A remains active.

### 509.3 Fact 3: MSG format is version-independent

**Source:** Protocol.hs line 1832-1833

**Detail:** `MSG RcvMessage {msgId, msgBody = EncRcvMsgBody body} -> e (MSG_, ' ', msgId, Tail body)` - no version branching.

### 509.4 Fact 4: Server encodes transport per-connection

**Source:** Server.hs line 1190-1194

**Detail:** `tSend th` uses `THandle.params.thVersion` of the recipient. If ESP32 negotiated v6, MSG comes in v6. If v7, then v7.

### 509.5 Fact 5: SMP Queues are strictly unidirectional

**Source:** Agent.hs duplex handshake analysis

**Detail:**
- App → ESP32 goes via Q_A (smp1.simplexonflux.com), ESP32 is Recipient
- ESP32 → App goes via Q_B (smp19.simplex.im), App is Recipient

### 509.6 Fact 6: SMP versions go up to v7 (for third-party clients)

**Source:** Haskell source code + practical test (v8 → Connection Reset)

**Detail:** Server reports internally 6-17/18, but official spec documents only v6 and v7. `legacyServerSMPRelayVRange` (without ALPN) allows only v6.

### 509.7 Fact 7: Batch framing is mandatory from v4

**Source:** Transport.hs, `batch = True` hardcoded

**Detail:** Format: `[2B contentLen][1B txCount][2B txLen][transmission][padding '#']`. Without batch framing → `TEBadBlock`.

---

## 510. Wizard Analyses (Detail Reference)

### 510.1 Analysis A: SMP Wire Format

**v6 Block (16384 bytes total):**
```
[2B content_length]
[1B tx_count = 0x01]
[2B tx_length]
[1B sigLen = 64]
[64B Ed25519 Signature]
[1B sessIdLen = 32]
[32B SessionId]           ← ONLY in v6, omitted in v7
[1B corrIdLen = 24]
[24B corrId]
[1B entityIdLen = 24]
[24B entityId]
[3B "SUB"]
[padding '#' to 16384]
```

**v7 Block (33 bytes shorter):**
```
[2B content_length]
[1B tx_count = 0x01]
[2B tx_length]
[1B sigLen = 64]
[64B Ed25519 Signature]
[1B corrIdLen = 24]       ← SessionId missing here
[24B corrId]
[1B entityIdLen = 24]
[24B entityId]
[3B "SUB"]
[padding '#' to 16384]
```

**tForAuth (data to be signed, BOTH versions same):**
```
[1B sessIdLen = 32]
[32B SessionId]
[1B corrIdLen]
[corrId bytes]
[1B entityIdLen]
[entityId bytes]
[CMD bytes]
```

### 510.2 Analysis B: Version Negotiation

1. Server sends `SMPServerHandshake` with `smpVersionRange` (e.g. 6-17)
2. Client calculates intersection with own range
3. Client sends `SMPClientHandshake` with a single `smpVersion`
4. Server validates via `compatibleVRange'`
5. Version is stored in `THandleParams.thVersion`
6. ALPN "smp/1" → full server range; without ALPN → `legacyServerSMPRelayVRange` (only v6)

### 510.3 Analysis C: ClientHello Format

```
[2B payload_length]       ← Length of following payload
[0x00 0x07]              ← chosen version (ONE version, not min+max!)
[0x20]                   ← keyHash length (32 bytes)
[32B caHash]             ← SHA256 of server certificate
[padding '#' to 16384]   ← NO batch framing for handshake blocks!
```

### 510.4 Analysis D: Queue Routing (Complete)

**After the duplex handshake:**

ESP32 (Inviting Party / Party A):
```
DuplexConnection
  rcvQueues: [Q_A on smp1, status=Active]      ← receives here from App
  sndQueues: [sq→Q_B on smp19, status=Active]  ← sends here to App
```

App (Joining Party / Party B):
```
DuplexConnection
  rcvQueues: [Q_B on smp19, status=Active]     ← receives here from ESP32
  sndQueues: [sq→Q_A on smp1, status=Active]   ← sends here to ESP32
```

---

## 511. Files Changed (Total Session 30)

| File | Changes | Tasks |
|------|---------|-------|
| `main/main.c` | kbd_queue handover, version 0x07, log corrected, parser sessLen removed | T5, T6-Fix6, T6-Fix6b |
| `main/include/smp_tasks.h` | Signature `smp_app_run(QueueHandle_t)` + `smp_tasks_start(..., int sock_fd)` | T5, T6-Fix |
| `main/core/smp_tasks.c` | Keyboard poll, heartbeat, socket timeout, re-subscribe, wildcard ACK, parser | T5, T6-Debug, T6-Fix, T6-Fix2, T6-Fix5, T6-Fix6b |
| `main/core/smp_contacts.c` | Drain loop, corrId 24B SUB, hex dumps, wire v7, parser | T6-Fix3, T6-Fix4, T6-Diag3/3b, T6-Fix6, T6-Fix6b |
| `main/core/smp_ack.c` | corrId 24B ACK, wire v7 | T6-Fix4b, T6-Fix6 |
| `main/core/smp_network.c` | RECV logging, BLOCK OUT dump | T6-Diag2, T6-Diag5 |

---

## 512. Commits in Session 30

| # | Message | Files |
|---|---------|-------|
| 1 | `feat(core): enable keyboard input in multi-task architecture` | smp_tasks.h, smp_tasks.c, main.c |
| 2+ | All T6 fixes and diags (not yet committed, as problem unresolved) | smp_tasks.c, smp_contacts.c, smp_ack.c, smp_network.c, main.c |

---

## 513. Current Architecture (State End of Session 30)

```
Boot → WiFi → TLS → SMP v7 Handshake → Subscribe → Tasks

┌─────────────────────────────────────────────────────────────┐
│ Network Task (Core 0, 12KB PSRAM Stack)                      │
│   smp_read_block(s_ssl, 1000ms timeout) loop                │
│   1s Socket Timeout via setsockopt()                         │
│   Frame → net_to_app_buf Ring Buffer                         │
│   app_to_net_buf check → ACK/SUBSCRIBE via main SSL          │
│   Heartbeat every ~30 loops                                  │
│   Main SSL: sock 54, smp1.simplexonflux.com                 │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Main Task / smp_app_run() (Internal SRAM Stack, 64KB)        │
│   Wildcard ACK (T6-Fix5)                                     │
│   Re-Subscribe All (T6-Fix2)                                 │
│   Ring Buffer read → Transport Parse → Decrypt               │
│   Keyboard queue poll (non-blocking, T5)                     │
│   42d: KEY + HELLO + Reply Queue Read + Chat                 │
│   ACK/Subscribe via Ring Buffer → Network Task               │
│   Peer SSL: sock 55/56, smp19.simplex.im (dies after idle)  │
│   Reply Queue SSL: temporary connection for 42d              │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ UI Task (Core 1, 8KB PSRAM Stack)                            │
│   Empty loop (future display output)                         │
└─────────────────────────────────────────────────────────────┘
```

**Memory Layout:**
- Internal SRAM: 46KB free (Main Task Stack, mbedTLS, WiFi)
- PSRAM: ~8.2MB free (Ring Buffers, Task Stacks, Frame Pools)

---

## 514. Lessons Learned Session 30

### #149: SMP Versions

**Severity: High**

- Official protocol docs describe ONLY v6 and v7
- Server reports internal range (e.g. 6-17/18), but third-party clients should use max v7
- v8+ is actively rejected by server (Connection Reset, Error -30848/0x7880)
- v7 vs v6: SessionId only in signature (tForAuth), not on the wire
- ClientHello format: `[2B payloadLen][2B version]` - sends ONE version, not min+max
- ALPN "smp/1" enables full server range; without ALPN only v6

### #150: corrId Format

**Severity: Medium**

- Must be 24 random bytes (length prefix 0x18)
- NOT 1 byte as previously implemented
- Server accepts both, but protocol spec requires 24 bytes
- corrId is reused as NaCL nonce (therefore random and unique)

### #151: Drain Loop for Multi-Command

**Severity: Medium**

- After the 42d handshake, responses come in unpredictable order
- ACK response can arrive BEFORE the SUB response
- Drain loop with entity matching (recipientId comparison) solves the problem
- Up to 5 attempts are sufficient

### #152: Batch Framing is Mandatory (from v4)

**Severity: High**

- Every block MUST have `[2B contentLen][1B txCount][2B txLen][transmission][padding '#']`
- Even for single transmissions: txCount=1
- Server parses with `batch=True`, no fallback for non-batched
- Handshake blocks (ClientHello, ServerHello) are the ONLY exception: direct `tPutBlock`, no batch

---

## 515. Remaining Problems

### Problem: App→ESP32 Message Reception ❌ UNRESOLVED

**Symptom:** After successful subscribe (Contact Queue + Reply Queue, both OK) on sock 54: Zero RECV. App shows 1 checkmark.

**Status:** 10 hypotheses systematically excluded. Message sent to Evgeny. Waiting for response.

**Message to Evgeny (sent Feb 18, 2026):**

> Hi Evgeny, I have a puzzling issue with SimpleGo's multi-task architecture and need your insight.
>
> **Setup:** ESP32 connects to smp1.simplexonflux.com, creates a queue (NEW), subscribes (SUB), gets OK. App scans invite, handshake completes (CONF/KEY/HELLO all working). ESP32→App messaging works perfectly.
>
> **Problem:** After the handshake, I re-subscribe to the contact queue and get SUB OK. But when the App sends a chat message, the server accepts it (1 checkmark in App) but never delivers the MSG to ESP32. Zero bytes arrive on the TLS connection after SUB OK. The ESP32 is actively reading with 1-second socket timeouts, heartbeats confirm the connection is alive.
>
> **What I've verified:**
> - SMP v7, batch framing correct (tx_count=1, tx_len matches)
> - SUB transmission: 118 bytes, Ed25519 signed, 24-byte random corrId
> - Server responds OK to SUB (verified via hex dump)
> - Wildcard ACK (empty msgId) returns ERR NO_MSG (no stuck delivery)
> - Both Contact Queue and Reply Queue subscribed on same connection
>
> **My question:** Is there a condition where the server would accept a SUB (respond OK) but then not deliver incoming MSGs to that subscription? Could there be a "subscriber client" mismatch where the subscription is registered but delivery routes to a different internal client object?
>
> **SMP version negotiated:** 7 (server range 6-17 with ALPN smp/1)

---

## 516. Task Overview Session 30

| # | Task | Status | Description |
|---|------|--------|-------------|
| T5 | Keyboard-Send Integration | ✅ | Non-blocking poll in smp_app_run() |
| T6 | Baseline-Test Bidirectional | ❌ | Send works, receive unresolved |
| T6-Fix | Socket Timeout | ✅ | 1s timeout, heartbeat works |
| T6-Fix2 | Re-Subscribe | ✅ | SUB OK, but no MSG |
| T6-Fix3 | Drain Loop | ✅ | Entity matching works |
| T6-Fix4/4b | corrId 24 Bytes | ✅ | Server accepts, problem remains |
| T6-Fix5 | Wildcard ACK | ✅ | ERR NO_MSG, nothing blocked |
| T6-Fix6/6b | v7 Upgrade | ✅ | 118 bytes, server happy |
| T6-Diag6 | ACK Chain | ✅ | Everything ACKed, Contact Queue never receives |

---

## 517. Agent Contributions Session 30

| Agent | Fairy Tale Role | Session 30 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Analysis, planning, task formulation, Evgeny message |
| 🐰👑 Hasi | Princess (The Implementer) | All 14 fixes and diagnostics |
| 🧙‍♂️ Wizard | The Verifier (Claude Code) | 5 Haskell analyses, wire format documentation |
| 🧹 Cinderella | The Log Servant | Factual log analysis |
| 👑 Cannatoshi | The Prince (Coordinator) | Hardware tests, coordination |

---

## 518. Session 31 Priorities

1. **P0:** Wait for Evgeny's response and implement
2. **P1:** Clean up diagnostic logs (RECV, hex dumps, drain loop to ESP_LOGD)
3. **P2:** Peer SSL reconnect (sock 55/56 dies after idle with errno=104 ECONNRESET)
4. **P3:** UI Task with display output for received messages

---

## 519. Session 30 Summary

### What Was Achieved

- ✅ **T5: Keyboard-Send Integration** — Non-blocking poll, messages sent successfully
- ❌ **T6: Baseline-Test Bidirectional** — Send works, receive unresolved
- ✅ **10 hypotheses systematically excluded**
- ✅ **14 fixes and diagnostics applied**
- ✅ **5 Wizard analyses completed**
- ✅ **Expert question sent to Evgeny Poberezkin**
- ✅ **SMP v6 → v7 upgrade** — 33 bytes saved per transmission

### Key Takeaway

```
SESSION 30 SUMMARY:
  🔍 INTENSIVE DEBUG SESSION — 10 Hypotheses, 14 Fixes
  
  T5: Keyboard-Send ✅ PASSED
  T6: Bidirectional ❌ UNRESOLVED
  
  Problem: App→ESP32 messages never arrive after successful SUB
  
  Excluded:
    - corrId format ✅
    - Batch framing ✅
    - Subscribe failure ✅
    - Delivery blocked ✅
    - Network Task crash ✅
    - SSL broken ✅
    - SMP v6/v7 ✅
    - SessionId wire format ✅
    - Response parser ✅
    - ACK chain ✅
  
  Next: Await Evgeny's response

"Session 30 was the most intensive debug session of the project.
 10 hypotheses, 14 fixes/diagnostics, 5 wizard analyses,
 and a letter to the protocol creator." 👑🐭
```

---

## 520. Future Work (Session 31)

### P0: Evgeny Response

1. **Wait for response** — Implement whatever Evgeny suggests
2. **Potential areas:** Server-side subscription state, client object routing

### P1: Cleanup

1. **Diagnostic logs** — Move to ESP_LOGD
2. **Hex dumps** — Remove or make conditional

### P2: Peer SSL Reconnect

1. **errno=104 ECONNRESET** — Handle gracefully
2. **Automatic reconnect** — On idle timeout

### P3: UI Task

1. **Display output** — Show received messages
2. **LVGL integration** — Connect to existing UI

---

**DOCUMENT CREATED: 2026-02-18 Session 30**  
**Status: ⚠️ T5 Complete, T6 Unresolved — Awaiting Evgeny Response**  
**Key Achievement: 10 hypotheses excluded, expert question sent**  
**Problem: App→ESP32 messages never arrive after successful SUB**  
**Next: Session 31 — Await Evgeny Response**

---

*Created by Princess Mausi (👑🐭) on February 18, 2026*  
*Session 30 was the most intensive debug session of the project. 10 hypotheses, 14 fixes/diagnostics, 5 wizard analyses, and a letter to the protocol creator.*
