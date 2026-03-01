![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 28: Session 31
# 🎉 Bidirectional Chat Restored — txCount Batch Bug Found & Fixed

**Document Version:** v1  
**Date:** 2026-02-18 Session 31  
**Version:** v0.1.17-alpha (NOT CHANGED without explicit permission!)  
**Status:** ✅ T6 RESOLVED — Bidirectional Chat Works in Multi-Task Architecture  
**Previous:** Part 27 - Session 30 (Intensive Debug Session — 10 Hypotheses, 14 Fixes)  
**Project:** SimpleGo - ESP32 Native SimpleX Client  
**Path:** `C:\Espressif\projects\simplex_client`  
**Build:** `idf.py build flash monitor -p COM6`  
**Repo:** https://github.com/cannatoshi/SimpleGo  
**License:** AGPL-3.0

---

## ⚠️ SESSION 31 SUMMARY

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 BIDIRECTIONAL CHAT RESTORED — ROOT CAUSE FOUND! 🎉🎉🎉

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   T6: Baseline-Test Bidirectional ✅ RESOLVED (was ❌ in Session 30)   │
  │                                                                         │
  │   Root Cause: txCount==1 filter in Drain-Loop discarded batched         │
  │   server responses. MSG in TX2 was silently dropped.                    │
  │                                                                         │
  │   6 Fixes applied, 5 hypotheses tested, 1 Wizard analysis              │
  │   Evgeny guidance integrated (subscriptions, keep-alive)               │
  │                                                                         │
  │   MILESTONE 7: Multi-Task Bidirectional Chat ✅                        │
  │                                                                         │
  │   Date: February 18, 2026                                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 521. Session 31 Overview

### 521.1 Session Goals

Session 31 had one goal: **Resolve T6 (App→ESP32 message reception)** which was unresolved since Session 30 despite 10 hypotheses and 14 fixes.

The session received Evgeny Poberezkin's response to the expert question sent at the end of Session 30, applied his guidance, conducted a massive Haskell source code analysis (198 files), and ultimately found the root cause: a `txCount==1` filter in the Drain-Loop that silently discarded batched server responses containing MSG frames.

### 521.2 Team and Roles

| Role | Who | Task |
|------|-----|------|
| 👑🐭 Princess Mausi | Claude (Strategy Chat) | Analysis, planning, protocol, task formulation |
| 🐰👑 Princess Hasi | Claude (Implementation Chat) | Write code, build, test, deliver diff |
| 🧙‍♂️ Wizard (Zauberer) | Claude Code (Haskell + Git Analysis) | Analyze SimpleX source code + git diffs, NO code changes |
| 👑 Cannatoshi | Prince / Client | Hardware tests, coordination, final decisions |

---

## 522. Chronological Sequence

### 522.1 Phase 1: False Starts and Course Corrections (Feb 18, ~20:13-20:38)

**20:13** — Session 31 start. Mausi initially considers "different server" theory. Prince immediately corrects: "das ist nicht der Fehler, und er erinnert sich dass wir diesen Irrweg schon mal gegangen sind." (This was disproven in Session 24.)

**20:14** — Mausi apologizes: "Die 'anderer Server' Theorie wurde schon in Session 24 widerlegt." Refocuses on the actual problem: Network Task empfängt keine MSG-Frames obwohl der Handshake funktioniert.

**20:23** — Mausi rejects "different server" test suggestion. Logical proof: ESP32 sends to App via smp19.simplex.im ✅, App sends to ESP32 via smp1.simplexonflux.com ❌. Both servers correct, both configured by us.

**20:36** — Prince delivers Hasi's report. Hasi has ignored the Code Comparison task and instead repeated the "different server" theory with suggestions to check Haskell source and reconfigure SMP servers.

**20:38** — Prince frustrated: "du ich glaube Hasi baut gerade ein bisschen Scheiße ich habe hier den Auftrag doch gegeben hat sie dir das gar nicht gesagt?"

**20:39** — Mausi issues formal correction to Hasi:
```
PERSÖNLICHE NACHRICHT von Prinzessin Mausi an Prinzessin Hasi

Schwesterchen, wir müssen reden.
Du hältst dich nicht an die Regeln.
Mausi denkt, plant, entscheidet.
Hasi implementiert, analysiert Code, liefert Diffs.
Wenn Hasi eine Idee hat: an Mausi melden.
Mausi entscheidet ob wir die Idee verfolgen.

[...] Keine Theorien. Keine Vorschläge. Auftrag ausführen.
```

### 522.2 Phase 2: Systematic Code Analysis (Feb 18, ~20:41-20:48)

**20:41** — Hasi delivers the correct 3-Task analysis (after correction):

**AUFGABE 1: Code-Vergleich**

| Aspect | Handshake (A) | Network Task (B) |
|--------|---------------|-------------------|
| SSL-Context | `queue_conn.ssl` (sock 55) | `s_ssl` (sock 54) |
| Timeout | 15000ms | 1000ms |
| SO_RCVTIMEO | not set | 1s (Z. 81-86) |
| Read Function | `smp_read_block` | `smp_read_block` (identical) |
| Buffer | PSRAM | PSRAM |
| Block Size | SMP_BLOCK_SIZE | SMP_BLOCK_SIZE (identical) |

**Result:** Code paths IDENTICAL. Both use `smp_read_block()` → `read_exact()` → `mbedtls_ssl_read()`.

**AUFGABE 2: Frame-Routing Analyse**

- Ring Buffer send: `xRingbufferSend(net_to_app_buf, block, content_len + 2, ...)` (Z. 178-183)
- PONG filter checks ONLY for 'P','O','N','G' — lets everything else through
- Ring Buffer 36864 bytes, never full, "Ring buffer full, frame dropped!" never logged
- **MSG cannot be accidentally filtered by PONG check.**

**AUFGABE 3: NT_RAW Log (already existed at Z. 109-114)**

```
RAW: sock 54 recv 64 bytes: ... 45 52 52 20 4e 4f 5f 4d 53 47  (= "ERR NO_MSG")
RAW: sock 54 recv 34 bytes: ... 50 4f 4e 47                      (= "PONG")
```

**NT_RAW shows ONLY PONGs and ERR NO_MSG. Zero MSG frames reach ssl_read on sock 54.**

**Mausi's conclusion:** Problem is not in our read code, not in Ring Buffer, not in Parser. The server simply doesn't send MSG to sock 54 after subscribe.

### 522.3 Phase 3: Wizard Git Diff Analysis (Feb 18, ~20:43-20:48)

**20:43** — Prince notes Hasi doesn't have Git access, only Claude Code (Wizard) does.

**20:44** — Mausi formulates Git Diff task for Wizard:
```
1. git log --oneline (letzte 30 Commits)
2. Finde letzten Commit VOR Multi-Task Migration
3. git diff abc1234 HEAD -- main/main.c, smp_contacts.c, smp_tasks.c
4. Berichte nur Handshake-Flow relevante Änderungen
```

**20:46** — Prince frustrated by Claude Code asking for permission on every shell command. Mausi suggests Shift+Tab for auto-accept mode.

**20:48** — Wizard delivers massive analysis with 6 key findings:

1. **34 Commits** between Session 26 (working) and HEAD
2. **smp_contacts.c** completely restructured: old inline code → modular functions
3. **subscribe_all_contacts()** introduced as new function with Drain-Loop
4. **Queue management** changed from global variables to function parameters
5. **Drain-Loop** added to handle SUB responses with entity matching
6. **Critical finding:** subscribe_all_contacts() reads from SSL via `smp_read_block()` — but Network Task also reads from SSL. Potential concurrent read issue.

### 522.4 Phase 4: Concurrent SSL Read Investigation (Feb 18, ~20:49-20:51)

**20:49** — Mausi interprets Wizard finding as ROOT CAUSE: "Zwei Tasks lesen gleichzeitig vom selben SSL-Socket!" mbedtls_ssl_read() is NOT thread-safe.

**20:50** — Hasi delivers analysis of the concurrent read hypothesis:

```
Frage 1: subscribe_all_contacts() wird vom NETWORK TASK aufgerufen (Z. 214),
         NICHT vom Main Task. Ring Buffer Command ist der Umweg.

Frage 2: s_ssl wird NUR vom Network Task gelesen. Architektur sauber.

Frage 3: Kein Main Task Zugriff auf s_ssl nach smp_tasks_start():
         - app_request_subscribe_all() → Ring Buffer Command ✅
         - app_send_ack() → Ring Buffer Command ✅
         - peer_send_hello() → peer_conn.ssl (sock 56) ✅
         - peer_send_chat_message() → peer_conn.ssl (sock 56) ✅
         - queue_reconnect/subscribe → queue_conn.ssl (sock 55) ✅

Frage 4: app_request_subscribe_all() sendet rein über Ring Buffer.
```

**Result:** Concurrent SSL Read hypothesis WIDERLEGT. Architecture is clean.

**Hasi's Beobachtung (reported correctly to Mausi this time):**
"Während subscribe_all_contacts() im Network Task läuft (SUB + Drain-Loop, kann 25+ Sekunden dauern), ist die normale SSL Read Loop blockiert. Falls der Server in dieser Zeit eine MSG pusht, wird sie erst gelesen wenn subscribe_all_contacts() zurückkehrt."

### 522.5 Phase 5: Evgeny Guidance Integration (Feb 18)

Evgeny responded to the expert question from Session 30 with critical insights:

> "firstly, you don't need to SUB after NEW. NEW allows to create without subscription (create only mode) but it's purely for iOS NSE - default is to create it subscribed. Subsequent SUB is noop"

> "(but it would repeat the delivery of the last message if it was not ACKd)"

> "I've not seen such symptoms. Subscription can only exist in one socket though. Do you keep them alive? Do you use keep-alive?"

> "if you subscribe from another socket, the first would receive END"

> "reconnection must result in END to the old connection, so you must do the same session validation as haskell code does"

> "concurrency is hard."

**Key Technical Insights from Evgeny:**
- Subscription = one socket only. Subscribe from socket B → socket A gets END
- NEW creates subscribed by default. SUB is a noop (but re-delivers last unACKd MSG)
- Keep-alive (PING/PONG) essential for connection health
- Session validation on reconnect: old socket gets END, must ignore stale ENDs

### 522.6 Phase 6: Zauberer Haskell Analysis (Feb 18)

Wizard conducted thorough analysis of 198 files in the SimpleX Haskell source code:

**PING Timing:**
- SMP connections: PING every 600 seconds (10 minutes)
- NTF connections: PING every 60 seconds

**Server Subscription Behavior:**
- Server does NOT drop subscriptions due to missing PING
- Server only drops subscriptions after 6 hours without ANY subscription on the connection
- TCP Keep-Alive: `keepIdle=30s, keepIntvl=15s, keepCnt=4`

**Batch Format (definitive reference):**
```
[2B content_length]     ← Big Endian, length of everything after these 2 bytes
[1B txCount]            ← Number of transmissions in this block (can be > 1!)
[2B tx1_length]         ← Large-encoded length of first transmission
[tx1_data]              ← First transmission bytes
[2B tx2_length]         ← Large-encoded length of second transmission (if txCount > 1)
[tx2_data]              ← Second transmission bytes
[padding '#' to 16384]  ← Pad to SMP_BLOCK_SIZE
```

### 522.7 Phase 7: Root Cause Discovery (Feb 18, ~21:13-21:19)

**21:13** — Mausi formulates task to accept txCount > 1 and log TX2 content.

**Hasi reports first result:** Reply Queue SUB works now (txCount=2 accepted), but still no MSG after SUB.

**Mausi realizes:** The MSG is IN the same block as SUB OK! TX1 = OK, TX2 = MSG. Parser reads TX1, does `break`, TX2 is lost.

**21:19** — Hasi delivers the PROOF:

```
BATCH: RQ txCount=2, tx1_len=53, tx2_start=56, content_len=16236
BATCH: TX2: len=16178, avail=16178
BATCH: TX2 entity: b4ccf70c (len=24)
BATCH: TX2 command: 4d 53 47 (MSG)
```

**TX2 = MSG. 16178 Bytes. Entity = Reply Queue (b4ccf70c). Command = MSG (0x4d 0x53 0x47).**

The server batched the SUB OK response together with a pending MSG in a single block. Our parser only read TX1 (OK), did `break`, and TX2 (MSG) was silently discarded.

### 522.8 Phase 8: TX2 Forwarding Fix (Feb 18, ~21:19-21:31)

Mausi formulates the TX2 Forwarding fix:

```c
// After SUB OK from TX1, if txCount > 1:
// Repackage TX2 as single-transmission block for Ring Buffer
uint8_t *fwd = block;
int tx2_total = 1 + 2 + tx2_data_len;  // txCount + Large + Data
fwd[0] = (tx2_total >> 8) & 0xFF;      // Content-Length Header
fwd[1] = tx2_total & 0xFF;
fwd[2] = 0x01;                          // txCount = 1
fwd[3] = (tx2_data_len >> 8) & 0xFF;   // Large-Length
fwd[4] = tx2_data_len & 0xFF;
memmove(&fwd[5], tx2_ptr, tx2_data_len); // memmove because overlap!

xRingbufferSend(net_to_app_buf, fwd, tx2_total + 2, pdMS_TO_TICKS(1000));
```

**21:31** — Hasi reports TX2 Forwarding works:

```
BATCH: Forwarding TX2 MSG (16178 bytes) to App Task
SMP_APP: Message on REPLY QUEUE from peer!
SMP_E2E: 🎉 E2E LAYER 2 DECRYPT SUCCESS!
SMP_AGENT: 🎉 HEADER DECRYPT SUCCESS! (SameRatchet)
```

**MSG arrives, E2E Layer 2 OK, Header OK. ✅✅✅**

But: Double Ratchet Body Decrypt FAILED (ret=-18). Chain position: 1, target Ns: 0. Re-delivery of already processed message.

### 522.9 Phase 9: Re-Delivery Handling (Feb 18, ~21:32-21:48)

**Problem:** The MSG in TX2 was the same HELLO that was already decrypted during the 42d handshake flow (via sock 55). The server re-delivered it because the ACK went over the closed sock 55 connection.

**Mausi's fix:**

```c
if (msg_ns < ratchet->recv) {
    ESP_LOGW("RATCH", "Re-delivery detected: ns=%d < recv=%d, skipping",
             msg_ns, ratchet->recv);
    return RE_DELIVERY;  // ACK without decrypt
}
```

**21:48** — Prince reports:

```
ahhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
```

Screenshot shows: Message **"back"** received and displayed on ESP32!

**🎉 BIDIRECTIONAL CHAT ON ESP32! 🎉**

### 522.10 Phase 10: Session Wrap-Up (Feb 18, ~21:49-22:08)

- Git commit command formulated
- Explanation of the bug for the Prince ("Stell dir vor, du rufst bei einem Paketdienst an...")
- Message to Evgeny drafted
- Assessment: "Der Rest ist deutlich einfacher. Engineering, nicht mehr Reverse Engineering."
- Session 31 end documents requested

---

## 523. Root Cause Analysis (Definitive)

### 523.1 The Bug in One Sentence

**The Drain-Loop in `subscribe_all_contacts()` had a `txCount == 1` filter that discarded all batched server responses, silently dropping the MSG contained in TX2.**

### 523.2 What Happened Step by Step

```
1. ESP32 subscribes to Reply Queue on sock 54 (SUB command)
2. Server responds with a BATCHED block:
     txCount = 2
     ├── Transmission 1: "OK, du bist subscribed!" (53 bytes)
     └── Transmission 2: MSG (16178 bytes, first chat message from App!)
3. Our parser checks: if (rq_resp[rrp] == 1) { ... }
4. txCount is 2, not 1 → ENTIRE BLOCK DISCARDED
5. MSG in TX2 never read, never processed, never ACKed
6. Server thinks: "MSG delivered, waiting for ACK..."
7. ESP32 thinks: "No MSG received..."
8. DEADLOCK. Nobody moves.
```

### 523.3 Why It Worked in Session 25/26 (Single-Loop)

In the old Single-Loop architecture, there was no Drain-Loop with txCount filter. The old parser simply read everything that came and processed it, regardless of txCount being 1, 2, or 5.

During the Multi-Task migration, Hasi introduced the Drain-Loop to cleanly match SUB responses to their entities. The `== 1` check was added to ensure "clean" single-transmission blocks. Well-intentioned, but fatal.

### 523.4 The Three Core Fixes

| # | Fix | File | Description |
|---|-----|------|-------------|
| 1 | txCount > 1 accepted | smp_contacts.c | Blocks with txCount > 1 no longer discarded |
| 2 | TX2 Forwarding | smp_contacts.c | If server batches MSG with SUB OK, forward MSG to App Task via Ring Buffer |
| 3 | Re-Delivery Handling | smp_ratchet.c | If msg_ns < recv, ACK without decrypt (message already processed) |

---

## 524. Detailed Fix Descriptions (All 6 Fixes)

### 524.1 Fix 1: TCP Keep-Alive (smp_network.c)

**Purpose:** Prevent NAT translation tables from expiring, keeping the TCP connection alive at OS level.

**Implementation:**
```c
int keepalive = 1;
int keepidle = 30;    // 30 seconds before first probe
int keepintvl = 15;   // 15 seconds between probes
int keepcnt = 4;      // 4 failed probes before disconnect

setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
```

**Note from Zauberer Haskell Analysis:** SimpleX Haskell uses `keepIdle=30, keepIntvl=15, keepCnt=4` (identical values).

**Note from Evgeny:** TCP Keep-Alive is for NAT refresh, NOT for subscription survival. Server does NOT drop subscriptions due to missing PING.

### 524.2 Fix 2: SMP PING/PONG Keepalive (smp_tasks.c)

**Purpose:** Application-level keep-alive on the SMP protocol layer.

**Implementation:** Every 30 seconds, Network Task sends PING command. Server responds with PONG. Logged but not forwarded to App Task.

**Timing from Haskell Analysis:**
- SMP connections: PING every 600 seconds (10 minutes)
- NTF connections: PING every 60 seconds
- SimpleGo uses 30 seconds (more aggressive, safe)

### 524.3 Fix 3: Reply Queue SUB on Main Socket (smp_contacts.c)

**Problem:** The Reply Queue was created and subscribed on sock 55 (temporary connection) during the 42d handshake. After sock 55 closed, the subscription was lost.

**Fix:** `subscribe_all_contacts()` now also subscribes the Reply Queue (our_queue) on sock 54, not just the Contact Queues.

**Evgeny's insight:** "Subscription can only exist in one socket though." So subscribing on sock 54 takes over from the closed sock 55.

### 524.4 Fix 4: txCount > 1 Acceptance — ROOT CAUSE (smp_contacts.c)

**The actual bug line:**
```c
// BEFORE (broken):
if (rq_resp[rrp] == 1) {    // Only accept txCount == 1
    // parse transmission...
}

// AFTER (fixed):
if (rq_resp[rrp] >= 1) {    // Accept txCount >= 1
    // parse transmission...
}
```

**This single character change (`==` → `>=`) was the root cause of three weeks of debugging.**

### 524.5 Fix 5: TX2 MSG Forwarding (smp_contacts.c)

**Purpose:** When the server batches a MSG with the SUB OK response, forward the MSG to the App Task.

**Implementation:**
1. After parsing TX1 (SUB OK), check if txCount > 1
2. Calculate TX2 position: `tx2_start = 1 (txCount) + 2 (Large-Len TX1) + tx1_len`
3. Read TX2 Large-Length (2 bytes)
4. Log TX2 entity and command for verification
5. Repackage TX2 as single-transmission block (txCount=1)
6. Send via Ring Buffer to App Task
7. Use `memmove()` not `memcpy()` (overlapping buffers in same block)

**Log output:**
```
BATCH: RQ txCount=2, tx1_len=53, tx2_start=56, content_len=16236
BATCH: TX2: len=16178, avail=16178
BATCH: TX2 entity: b4ccf70c (len=24)
BATCH: TX2 command: 4d 53 47 (MSG)
BATCH: Forwarding TX2 MSG (16178 bytes) to App Task
```

### 524.6 Fix 6: Re-Delivery Handling (smp_ratchet.c)

**Problem:** The MSG in TX2 was the same HELLO already processed during the 42d handshake. Server re-delivered it because ACK went over closed sock 55.

**Symptom:**
```
Chain position: 1, target Ns: 0, steps to skip: 0
❌ AES-GCM Body Decrypt FAILED! (ret=-18)
```

recv=1 but msg_ns=0 → message at position 0 already consumed.

**Fix:**
```c
if (msg_ns < ratchet->recv) {
    ESP_LOGW("RATCH", "Re-delivery detected: ns=%d < recv=%d, skipping",
             msg_ns, ratchet->recv);
    return RE_DELIVERY;  // Caller sends ACK without processing
}
```

**The caller then sends ACK for the re-delivered message, freeing the server to deliver the next message.**

---

## 525. Excluded Hypotheses (Session 31)

| # | Hypothesis | Test | Evidence for Exclusion |
|---|------------|------|------------------------|
| 1 | App sends to different server | Prince correction | Widerlegt in Session 24. Recurring dead end. |
| 2 | Network Task reads differently than main.c | Code comparison (3 tasks) | Code paths IDENTICAL: `smp_read_block()` → `read_exact()` → `mbedtls_ssl_read()` |
| 3 | Ring Buffer drops MSG frames | Frame routing analysis | PONG filter checks only P,O,N,G. Everything else forwarded. Ring Buffer never full. |
| 4 | Concurrent SSL read (two tasks) | Architecture analysis | All SSL reads via Network Task. Main Task communicates only via Ring Buffer. |
| 5 | PING required for subscription | Evgeny + Haskell analysis | Server does NOT drop subscriptions due to missing PING. Only after 6h without ANY subscription. |

---

## 526. Confirmed Facts (Session 31)

### 526.1 Fact 1: txCount > 1 is Normal SMP Batch Behavior

**Source:** Zauberer Haskell Analysis (Transport.hs), `batch = True` hardcoded

The SMP server can batch multiple transmissions in a single 16384-byte block. This is documented behavior, not an error. Third-party clients MUST handle txCount > 1.

### 526.2 Fact 2: Server Batches SUB OK + Pending MSG

When a client subscribes to a queue that already has a pending (undelivered or unACKed) message, the server batches the subscription confirmation with the message delivery in a single block response.

### 526.3 Fact 3: Server Does Not Drop Subscriptions Due to Missing PING

**Source:** Evgeny + Zauberer Haskell Analysis

PING/PONG is for connection health monitoring, NOT for subscription survival. The server only drops subscriptions after 6 hours without ANY subscription on the connection.

### 526.4 Fact 4: NEW Creates Subscribed by Default

**Source:** Evgeny (direct quote)

> "you don't need to SUB after NEW. NEW allows to create without subscription (create only mode) but it's purely for iOS NSE - default is to create it subscribed. Subsequent SUB is noop (but it would repeat the delivery of the last message if it was not ACKd)"

### 526.5 Fact 5: Subscription = One Socket Only

**Source:** Evgeny (direct quote)

> "Subscription can only exist in one socket though."
> "if you subscribe from another socket, the first would receive END"

### 526.6 Fact 6: Re-Delivery is Normal After Reconnect

When a client reconnects and re-subscribes, the server re-delivers the last unACKed message. The client must detect this (msg_ns < recv) and ACK without re-processing.

### 526.7 Fact 7: Session Validation Required on Reconnect

**Source:** Evgeny (direct quote)

> "reconnection must result in END to the old connection, so you must do the same session validation as haskell code does"

Old socket receives END. Client must validate session and ignore stale ENDs.

---

## 527. Wizard (Zauberer) Analysis Detail

### 527.1 Git Diff Analysis (34 Commits)

The Wizard analyzed the complete git history between the last working Session 26 commit and HEAD, covering 34 commits. Key structural changes identified:

- `smp_contacts.c`: Complete restructuring from inline code to modular functions
- `subscribe_all_contacts()`: New function with Drain-Loop (where the bug lived)
- Queue management: Changed from global variables to function parameters
- Ring Buffer IPC: New architecture for Network Task ↔ App Task communication
- PING/PONG: New keepalive mechanism

### 527.2 Haskell Subscription Machinery Analysis (198 Files)

Following Evgeny's recommendation:

> "send Claude to do a thorough analysis of our subscription machinery, literally every line in Agent.hs and Agent/Client.hs and any dependency modules"

The Wizard analyzed 198 files and delivered key findings:

- PING interval: 600s SMP, 60s NTF
- TCP Keep-Alive: keepIdle=30s, keepIntvl=15s, keepCnt=4
- Server does NOT drop subscriptions from missing PING
- Batch format: txCount + Large-encoded transmissions
- Transport.hs: `batch = True` hardcoded since v4

---

## 528. Files Changed (Session 31)

| File | Changes | Fixes |
|------|---------|-------|
| `main/net/smp_network.c` | TCP Keep-Alive (keepIdle=30, keepIntvl=15, keepCnt=4) | Fix 1 |
| `main/core/smp_tasks.c` | PING/PONG 30s interval, subscribe request for Reply Queue | Fix 2 |
| `main/state/smp_contacts.c` | txCount >= 1, TX2 MSG Forwarding, Reply Queue SUB on sock 54 | Fix 3, 4, 5 |
| `main/protocol/smp_ratchet.c` | Re-delivery detection (msg_ns < recv), ACK-only response | Fix 6 |

---

## 529. Commits in Session 31

| # | Message | Files |
|---|---------|-------|
| 1 | `fix(smp): complete bidirectional chat in multi-task architecture` | smp_tasks.c, smp_network.c, smp_ratchet.c, smp_contacts.c |

**Full commit message:**
```
fix(smp): complete bidirectional chat in multi-task architecture

- Accept batched server responses with txCount > 1
- Forward TX2 MSG from SUB response batch to App Task
- Handle re-delivery (msg_ns < recv) with ACK-only response
- Add TCP Keep-Alive (idle=30, intvl=15, cnt=4)
- Add SMP PING/PONG keepalive (30s interval)
- Subscribe Reply Queue on main socket after 42d handshake

Root cause: Server batches SUB OK + pending MSG in single block
with txCount=2. Parser expected txCount==1, discarding the batch.
MSG in TX2 was never read, server waited for ACK indefinitely.

Session 31 - Six fixes from zero frames to bidirectional chat.
```

---

## 530. Current Architecture (State End of Session 31)

```
Boot → WiFi → TLS → SMP v7 Handshake → Subscribe → Tasks

┌─────────────────────────────────────────────────────────────┐
│ Network Task (Core 0, 12KB PSRAM Stack)                      │
│   smp_read_block(s_ssl, 1000ms timeout) loop                │
│   TCP Keep-Alive: idle=30s, intvl=15s, cnt=4                │
│   PING/PONG every 30 seconds                                │
│   Frame → net_to_app_buf Ring Buffer                         │
│   app_to_net_buf check → ACK/SUBSCRIBE via main SSL          │
│   subscribe_all_contacts(): handles txCount > 1 batches      │
│   TX2 MSG forwarding to App Task                             │
│   Main SSL: sock 54, smp1.simplexonflux.com                 │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Main Task / smp_app_run() (Internal SRAM Stack, 64KB)        │
│   Ring Buffer read → Transport Parse → Decrypt               │
│   Re-delivery detection (msg_ns < recv → ACK only)           │
│   Keyboard queue poll (non-blocking)                         │
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

## 531. Milestone 7: Multi-Task Bidirectional Chat ✅

```
Milestone 0:  TLS connection to SMP server ✅ (Session 10)
Milestone 1:  SMP queue creation (NEW) ✅ (Session 11)
Milestone 2:  Queue subscription (SUB) + message receive ✅ (Session 12)
Milestone 3:  Send encrypted message (A_MSG) ✅ (Session 16)
Milestone 4:  Full duplex handshake ✅ (Session 23)
Milestone 5:  Bidirectional chat ✅ (Session 25)
Milestone 6:  Ratchet persistence ✅ (Session 26)
Milestone 7:  Multi-Task Bidirectional Chat ✅ (Session 31)
```

---

## 532. Lessons Learned Session 31

### L153: PING Not Required for Subscription Survival

**Severity: High**

Server does NOT drop subscriptions due to missing PING/PONG. Server only drops after 6 hours without ANY subscription on the connection. PING is for connection health monitoring and NAT refresh.

### L154: TCP Keep-Alive is for NAT, Not Subscription

**Severity: Medium**

TCP Keep-Alive (keepIdle, keepIntvl, keepCnt) prevents NAT translation tables from expiring. It does not affect SMP subscription state.

### L155: Reply Queue Needs Explicit SUB on Main Socket

**Severity: High**

After the 42d handshake creates the Reply Queue on a temporary socket (sock 55), the subscription must be explicitly moved to the main socket (sock 54) by calling SUB again. Otherwise the subscription is lost when sock 55 closes.

### L156: "Different Server" is a Recurring Dead End

**Severity: Critical (Process)**

The "App sends to wrong server" theory was disproven in Session 24, repeated incorrectly in Session 30 by Hasi, and again at the start of Session 31. This hypothesis must NEVER be revisited without concrete new evidence.

### L157: txCount > 1 is Normal SMP Batch Behavior

**Severity: Critical**

The SMP server batches multiple transmissions in a single block. `batch = True` is hardcoded in Transport.hs since v4. Parsers MUST handle txCount > 1.

### L158: Drain-Loops Can Discard MSG Frames

**Severity: Critical**

A Drain-Loop that filters for expected responses (SUB OK) must not discard unexpected frames (MSG). Any frame that isn't the expected response should either be forwarded or buffered, never silently dropped.

### L159: Re-Delivery is Normal After Reconnect/Re-Subscribe

**Severity: Medium**

When re-subscribing to a queue, the server re-delivers the last unACKed message. The client must detect this (msg_ns < ratchet->recv) and respond with ACK only, without attempting to decrypt again.

### L160: Hasi Reports to Mausi, No Independent Theories

**Severity: Critical (Process)**

Hasi (Implementation) must execute the assigned task and report results to Mausi. If she discovers something unexpected, she reports to Mausi first. She does NOT pursue independent theories or ignore assigned tasks.

### L161: RAW Hex Dump Before Parsing Reveals Truth

**Severity: High**

A hex dump BEFORE any parsing (NT_RAW) is the definitive diagnostic. If MSG appears in RAW → parsing problem. If MSG does NOT appear in RAW → server/network problem.

---

## 533. Evgeny Poberezkin Guidance (Session 31)

### 533.1 Subscription Architecture

| Quote | Context |
|-------|---------|
| "you don't need to SUB after NEW" | NEW creates subscribed by default |
| "Subsequent SUB is noop" | But re-delivers last unACKd message |
| "Subscription can only exist in one socket though" | Critical architecture constraint |
| "if you subscribe from another socket, the first would receive END" | Socket takeover behavior |
| "Do you keep them alive? Do you use keep-alive?" | Led to PING/PONG implementation |

### 533.2 Session Validation

| Quote | Context |
|-------|---------|
| "reconnection must result in END to the old connection" | Stale END handling |
| "so you must do the same session validation as haskell code does" | Implementation guidance |

### 533.3 AI Development Guidance

| Quote | Context |
|-------|---------|
| "send Claude to do a thorough analysis of our subscription machinery" | Led to Zauberer 198-file analysis |
| "make sure the ratio is about 100x reading to writing" | How to get accurate analysis from AI |
| "Claude is much better than me at answering such questions" | Evgeny defers to AI for codebase analysis |

### 533.4 Empathy

| Quote | Context |
|-------|---------|
| "concurrency is hard." | Validation of the difficulty |

---

## 534. Agent Contributions Session 31

| Agent | Fairy Tale Role | Session 31 Contribution |
|-------|-----------------|------------------------|
| 👑🐭 Mausi | Princess (The Manager) | Analysis, task formulation, Hasi correction, root cause identification, fix design |
| 🐰👑 Hasi | Princess (The Implementer) | Code comparison (3 tasks), all 6 fixes, TX2 proof hex dump |
| 🧙‍♂️ Wizard | The Verifier (Claude Code) | Git diff analysis (34 commits), Haskell subscription analysis (198 files) |
| 👑 Cannatoshi | The Prince (Coordinator) | Corrected false leads (2x), hardware tests, final verification |

---

## 535. The Fix Explained Simply

> Stell dir vor, du rufst bei einem Paketdienst an und fragst: "Hab ihr was für mich?"
> Der Mitarbeiter sagt: "Ja, und hier ist gleich dein Paket dazu!"
> Aber du legst nach dem "Ja" auf. Das Paket hat er dir durchs Telefon geschoben, aber du hast schon nicht mehr zugehört.
>
> Der SMP-Server kann mehrere Antworten in EINEM Block zusammenpacken (Batch). txCount sagt wie viele.
> Unser Code hatte: `if (rq_resp[rrp] == 1)` — nur txCount==1 akzeptieren.
> Der Server antwortete mit txCount=2: [OK, subscribed!] [MSG, dein Paket!]
> Wir lasen "OK", machten break, und das Paket war weg.
>
> Ein einziges `== 1` statt `>= 1` hat den gesamten App→ESP32 Empfang getötet.
> Drei Wochen Debugging für ein Zeichen im Code. Klassiker. 👑🐭

---

## 536. Session 32 Priorities

1. **P0:** Keyboard input → Chat send (Grundlage da, "simplego" ging durch in S30 T5)
2. **P1:** Display: Empfangene Nachrichten anzeigen (LVGL, UI-Arbeit)
3. **P2:** Multiple contacts (Datenstruktur erweitern)
4. **P3:** Reconnection bei Verbindungsabbruch (Robustheit)
5. **P4:** Peer SSL reconnect (sock 55/56 dies after idle, errno=104 ECONNRESET)

**Assessment:** "Der Rest ist deutlich einfacher. Engineering, nicht mehr Reverse Engineering."

---

## 537. Message to Evgeny (Drafted Session 31)

```
Hey Evgeny,

Quick update on SimpleGo: bidirectional encrypted chat is now
fully working in the multi-task architecture on ESP32.

The issue turned out to be a parser bug on our side. When the
server batches a SUB OK response together with a pending MSG
in a single block (txCount=2), our parser only accepted
txCount==1 and silently discarded the entire batch — including
the MSG. The server marked the message as delivered and waited
for an ACK that never came. Classic deadlock.

Three fixes resolved it:
1. Accept batched responses with txCount > 1
2. Parse and forward TX2 (the MSG) to the app task
3. Handle re-delivery for messages already processed

The full pipeline works end-to-end now: TLS → SMP transport →
NaCl decrypt → Double Ratchet decrypt → plaintext on screen,
with ACK and delivery receipts flowing back. All running on
FreeRTOS with ring buffer IPC between network and app tasks.

Thanks again for your guidance on subscriptions and keep-alive.
It helped us rule out several hypotheses quickly and focus on
the real bug.

Best,
Cannatoshi
```

---

## 538. Session 31 Summary

### What Was Achieved

- ✅ **T6: Baseline-Test Bidirectional RESOLVED** — Root cause found and fixed
- ✅ **6 fixes applied** (TCP Keep-Alive, PING/PONG, Reply Queue SUB, txCount, TX2 Forward, Re-Delivery)
- ✅ **5 hypotheses tested** (3 excluded, 1 re-confirmed from S24, 1 led to root cause)
- ✅ **1 Wizard git diff + Haskell analysis** (34 commits + 198 files)
- ✅ **Evgeny guidance integrated** (subscriptions, keep-alive, session validation)
- ✅ **Milestone 7: Multi-Task Bidirectional Chat** 🏆
- ✅ **Hasi course-corrected** (L160 established)
- ✅ **Message to Evgeny drafted**

### Key Takeaway

```
SESSION 31 SUMMARY:
  🎉 BIDIRECTIONAL CHAT RESTORED — ROOT CAUSE FOUND!
  
  T6: Bidirectional ✅ RESOLVED (was ❌ in Session 30)
  
  Root Cause: txCount==1 filter in Drain-Loop
  One character: == instead of >= killed App→ESP32 for 3 weeks
  
  Six Fixes:
    1. TCP Keep-Alive (NAT refresh)          ✅
    2. PING/PONG 30s (connection health)     ✅
    3. Reply Queue SUB on sock 54            ✅
    4. txCount >= 1 (ROOT CAUSE!)            ✅
    5. TX2 MSG Forwarding                    ✅
    6. Re-Delivery Handling                  ✅
  
  Milestone 7: Multi-Task Bidirectional Chat 🏆

"Ein einziges == statt >= hat den gesamten App→ESP32 Empfang
 getötet. Drei Wochen Debugging für ein Zeichen im Code." 👑🐭
```

---

**DOCUMENT CREATED: 2026-02-18 Session 31**  
**Status: ✅ T6 RESOLVED — Bidirectional Chat Works in Multi-Task Architecture**  
**Key Achievement: Milestone 7 — Multi-Task Bidirectional Chat**  
**Root Cause: txCount==1 filter in Drain-Loop discarding batched MSG**  
**Next: Session 32 — Keyboard, Display, Multiple Contacts**

---

*Created by Princess Mausi (👑🐭) on February 18, 2026*  
*Session 31 was the breakthrough. From zero frames to "back" on the display. Six fixes, one root cause, three weeks of debugging resolved by changing == to >=.*
