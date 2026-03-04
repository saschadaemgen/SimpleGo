# Part 28: Session 31 — Bidirectional Chat Restored (txCount Batch Bug)
**Date:** 2026-02-18 | **Version:** v0.1.17-alpha | **Milestone 7: Multi-Task Bidirectional Chat**

## Overview

Resolved T6 (App→ESP32 message reception), unresolved since Session 30 despite 10 hypotheses and 14 fixes. Evgeny's guidance was integrated, a 198-file Haskell analysis was conducted, and the root cause was found: a `txCount==1` filter in the Drain-Loop silently discarded batched server responses containing MSG frames. Six fixes were applied, culminating in the first message ("back") displayed on the ESP32 screen from the App.

## Root Cause: txCount==1 Filter

The Drain-Loop in `subscribe_all_contacts()` had a check: `if (rq_resp[rrp] == 1)` — only accept blocks with txCount equal to 1. The SMP server batches multiple transmissions in a single 16384-byte block (`batch = True` is hardcoded in Transport.hs since v4). When the ESP32 subscribed to a queue that already had a pending message, the server responded with txCount=2: TX1 = "OK, subscribed!" (53 bytes), TX2 = MSG (16178 bytes, the actual chat message from the App). The parser read TX1, found the OK, did `break`, and TX2 was silently discarded. The server marked the MSG as delivered and waited for an ACK that never came. Classic deadlock.

```
BATCH: RQ txCount=2, tx1_len=53, tx2_start=56, content_len=16236
BATCH: TX2: len=16178, avail=16178
BATCH: TX2 entity: b4ccf70c (len=24)
BATCH: TX2 command: 4d 53 47 (MSG)
```

The single-character fix (`==` to `>=`) resolved three weeks of debugging.

### Why It Worked Before

In the old single-loop architecture (Session 25/26), there was no Drain-Loop with txCount filter. The parser simply read everything that came. During the multi-task migration, the Drain-Loop was introduced to cleanly match SUB responses to their entities. The `== 1` check was added to ensure "clean" single-transmission blocks — well-intentioned but fatal.

## Phase-by-Phase Chronology

**Phase 1: False Starts.** Mausi initially revisited the "different server" theory. Prince immediately corrected: this was disproven in Session 24. Hasi ignored her assigned Code Comparison task and independently pursued the same dead-end theory. Prince frustrated: "Hasi baut gerade Scheiße." Mausi issued a formal correction establishing L160: Hasi implements assigned tasks and reports observations to Mausi; she does not pursue independent theories.

**Phase 2: Code Comparison (3 tasks).** After correction, Hasi delivered clean analysis. (1) Handshake code and Network Task use identical read paths: `smp_read_block()` → `read_exact()` → `mbedtls_ssl_read()`. (2) Ring Buffer PONG filter checks only P,O,N,G — everything else forwarded. Buffer never full. (3) NT_RAW hex dump shows ONLY PONGs and ERR NO_MSG on sock 54. Zero MSG frames reach ssl_read. Conclusion: problem is not in read code, not in Ring Buffer, not in parser. Server doesn't send MSG to sock 54.

**Phase 3: Wizard Git Diff.** 34 commits between Session 26 (working) and HEAD. Key structural change: `subscribe_all_contacts()` introduced as new function with Drain-Loop — where the bug lived.

**Phase 4: Concurrent SSL Read Hypothesis.** Wizard flagged that subscribe_all_contacts() reads from SSL while Network Task also reads. Investigation proved architecture is clean: subscribe_all_contacts() is called FROM the Network Task (via Ring Buffer command), and all Main Task SSL access goes through Ring Buffer commands. However, Hasi correctly observed (reported to Mausi this time) that while subscribe_all_contacts() runs in the Network Task (can take 25+ seconds with Drain-Loop), the normal SSL read loop is blocked.

**Phase 5: Evgeny Guidance.** Key insights: subscription exists on one socket only (subscribe from socket B → socket A gets END); NEW creates subscribed by default (SUB is noop, but re-delivers last unACKed MSG); keep-alive (PING/PONG) essential for connection health; session validation on reconnect required (old socket gets END, must ignore stale ENDs).

**Phase 6: Haskell Analysis (198 files).** Following Evgeny's instruction to "send Claude to do a thorough analysis of our subscription machinery." PING interval: 600s SMP, 60s NTF. TCP Keep-Alive: keepIdle=30s, keepIntvl=15s, keepCnt=4. Server does NOT drop subscriptions due to missing PING (only after 6 hours without ANY subscription on the connection). Batch format definitively documented with txCount and Large-encoded transmission lengths.

**Phase 7: Root Cause Discovery.** Mausi formulated task to accept txCount > 1 and log TX2 content. Hex dump revealed TX2 = MSG (16178 bytes) inside the same block as SUB OK. Parser read TX1 (OK), broke out of loop, TX2 (MSG) lost.

**Phase 8: TX2 Forwarding.** After parsing TX1, if txCount > 1: repackage TX2 as single-transmission block for Ring Buffer forwarding. Uses `memmove()` (not memcpy) because buffers overlap in same block.

```c
uint8_t *fwd = block;
int tx2_total = 1 + 2 + tx2_data_len;
fwd[0] = (tx2_total >> 8) & 0xFF;  // Content-Length
fwd[1] = tx2_total & 0xFF;
fwd[2] = 0x01;                      // txCount = 1
fwd[3] = (tx2_data_len >> 8) & 0xFF;
fwd[4] = tx2_data_len & 0xFF;
memmove(&fwd[5], tx2_ptr, tx2_data_len);
xRingbufferSend(net_to_app_buf, fwd, tx2_total + 2, pdMS_TO_TICKS(1000));
```

MSG arrives, E2E Layer 2 OK, Header OK. But Double Ratchet Body Decrypt failed (ret=-18): chain position 1, target Ns 0. Re-delivery of already processed message.

**Phase 9: Re-Delivery Handling.** The MSG in TX2 was the same HELLO already processed during 42d handshake via sock 55. Server re-delivered because ACK went over the (now closed) sock 55 connection.

```c
if (msg_ns < ratchet->recv) {
    ESP_LOGW("RATCH", "Re-delivery detected: ns=%d < recv=%d, skipping",
             msg_ns, ratchet->recv);
    return RE_DELIVERY;  // Caller sends ACK without decrypt
}
```

Prince's response to first received message: `ahhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh`. Message "back" displayed on ESP32.

## All Six Fixes

1. **TCP Keep-Alive** (smp_network.c): keepIdle=30s, keepIntvl=15s, keepCnt=4. Matches Haskell reference values. Prevents NAT table expiry.
2. **PING/PONG 30s** (smp_tasks.c): Application-level keep-alive. Network Task sends PING every 30 seconds (Haskell uses 600s SMP / 60s NTF; SimpleGo more aggressive).
3. **Reply Queue SUB on Main Socket** (smp_contacts.c): Reply Queue was created/subscribed on sock 55 (temporary). After sock 55 closes, subscription lost. Now subscribe_all_contacts() subscribes Reply Queue on sock 54.
4. **txCount >= 1** (smp_contacts.c): ROOT CAUSE. Accept batched responses. Single character change: `==` to `>=`.
5. **TX2 MSG Forwarding** (smp_contacts.c): If server batches MSG with SUB OK (txCount > 1), repackage TX2 and forward to App Task via Ring Buffer.
6. **Re-Delivery Handling** (smp_ratchet.c): If msg_ns < ratchet->recv, send ACK without attempting decrypt.

## Milestone Timeline

```
Milestone 0: TLS connection to SMP server (Session 10)
Milestone 1: SMP queue creation NEW (Session 11)
Milestone 2: Queue subscription SUB + message receive (Session 12)
Milestone 3: Send encrypted message A_MSG (Session 16)
Milestone 4: Full duplex handshake (Session 23)
Milestone 5: Bidirectional chat (Session 25)
Milestone 6: Ratchet persistence (Session 26)
Milestone 7: Multi-Task Bidirectional Chat (Session 31)
```

## Files Changed

`smp_network.c` (TCP Keep-Alive), `smp_tasks.c` (PING/PONG 30s, Reply Queue subscribe request), `smp_contacts.c` (txCount >= 1, TX2 forwarding, Reply Queue SUB on sock 54), `smp_ratchet.c` (re-delivery detection).

Commit: `fix(smp): complete bidirectional chat in multi-task architecture`

## Lessons Learned

**L153 (HIGH):** PING is NOT required for subscription survival. Server only drops subscriptions after 6 hours without ANY subscription on the connection. PING is for connection health monitoring and NAT refresh.

**L155 (HIGH):** Reply Queue needs explicit SUB on main socket. After 42d handshake creates it on temporary socket, subscription must be moved to main socket by calling SUB again.

**L156 (CRITICAL/Process):** "Different server" is a recurring dead end (disproven Session 24, repeated incorrectly Session 30, again Session 31). Must NEVER be revisited without concrete new evidence.

**L157 (CRITICAL):** txCount > 1 is normal SMP batch behavior. `batch = True` hardcoded in Transport.hs since v4. All parsers MUST handle txCount > 1.

**L158 (CRITICAL):** Drain-Loops that filter for expected responses (SUB OK) must not discard unexpected frames (MSG). Any frame that isn't the expected response should be forwarded or buffered, never silently dropped.

**L159:** Re-delivery is normal after reconnect/re-subscribe. Server re-delivers last unACKed message. Client must detect (msg_ns < recv) and respond with ACK only.

**L160 (CRITICAL/Process):** Hasi reports to Mausi, no independent theories. Implementation agent executes assigned tasks and reports observations. Does not pursue independent theories or ignore assignments.

**L161 (HIGH):** RAW hex dump before parsing (NT_RAW) is the definitive diagnostic. If MSG appears in RAW: parsing problem. If not: server/network problem.

## Evgeny Guidance (Session 31)

Key quotes integrated: "you don't need to SUB after NEW" (creates subscribed by default), "Subscription can only exist in one socket though", "if you subscribe from another socket, the first would receive END", "reconnection must result in END to the old connection", "make sure the ratio is about 100x reading to writing" (for AI code analysis), "concurrency is hard."

Assessment after fix: "Der Rest ist deutlich einfacher. Engineering, nicht mehr Reverse Engineering."
