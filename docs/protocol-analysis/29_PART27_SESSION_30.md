# Part 27: Session 30 — Intensive Debug Session (10 Hypotheses, 14 Fixes)
**Date:** 2026-02-16 to 2026-02-18 | **Version:** v0.1.17-alpha

## Overview

T5 (Keyboard Send Integration) completed successfully. T6 (Bidirectional Baseline Test) revealed that App-to-ESP32 messages never arrive after successful SUB, despite 10 systematically tested hypotheses and 14 fixes/diagnostics. The session ended with an expert question to Evgeny Poberezkin and an SMP v6-to-v7 protocol upgrade.

## T5: Keyboard Send Integration (PASSED)

Keyboard input reactivated in the multi-task architecture. Non-blocking poll with `xQueueReceive(..., 0)` inserted before the Ring Buffer read in `smp_app_run()`. Changes: `smp_tasks.h` (+queue.h include, signature change to accept QueueHandle_t), `smp_tasks.c` (14 lines: keyboard poll block at lines 190-202), `main.c` (1 line: pass kbd_msg_queue to smp_app_run). Test confirmed: keyboard "simplego" typed, msg_id=3/4, `A_MSG ACCEPTED BY SERVER!`, arrived in SimpleX App. Internal SRAM: 46KB free. Committed as `feat(core): enable keyboard input in multi-task architecture`.

## T6: Bidirectional Test — The Receive Problem

Sending works (ESP32→App). Receiving fails (App→ESP32). In 50+ minutes of logging, NOT A SINGLE `Network task: Frame received` line appeared. Network Task heartbeats confirmed the task was alive.

### Systematic Fix Attempts (all confirmed working, none solved the receive problem)

**Socket Timeout (T6-Fix):** `setsockopt(SO_RCVTIMEO, 1s)` on sock 54. Network Task now reacts faster, heartbeat appears every ~30 seconds.

**Re-Subscribe (T6-Fix2):** `app_request_subscribe_all()` with 2s delay at app start. SUB OK on both Contact Queue and Reply Queue, but still no MSG.

**Drain Loop (T6-Fix3):** After 42d handshake, responses arrive in unpredictable order (ACK before SUB). Drain loop with up to 5 attempts and entity ID matching (`ent_match`) finds SUB OK correctly.

**corrId 24 Bytes (T6-Fix4/4b):** corrId was only 1 byte ('0'+i for contacts, 'A'/'R' for Reply Queue). Protocol spec requires 24 random bytes (corrId is reused as NaCl nonce). Fixed with `esp_fill_random()` for both SUB and ACK commands. Server accepts, problem remains.

**Wildcard ACK (T6-Fix5):** Empty msgId ACK after re-subscribe to clear any server-side delivered-but-unACKed state. Server responds ERR NO_MSG — nothing was blocked.

**SMP v6→v7 Upgrade (T6-Fix6/6b):** Largest change of the session, 5 files affected. v7 removes SessionId from wire body (saves 33 bytes per transmission). SessionId remains only in tForAuth (signature computation). ClientHello changed from `0x00 0x06` to `0x00 0x07`. SessionId removed from 5 wire transmissions, sessLen parsing removed from 6 response parsers. SUB transmission drops from 151 to 118 bytes. Server accepts v7. Problem persists identically.

**ACK Chain Analysis (T6-Diag6):** Complete audit of all ACKs. CONF on Reply Queue: ACKed. RQ Response: ACKed. HELLO from App on Contact Queue: NEVER ARRIVES. Chat MSG on Contact Queue: NEVER ARRIVES. Conclusion: everything that arrives gets correctly ACKed. The problem is that Contact Queue NEVER receives a MSG.

### 10 Excluded Hypotheses

1. corrId wrong (1 byte instead of 24) — fixed, server says OK, no MSG
2. Batch framing missing or wrong — hex dump shows correct format
3. Subscribe failed — ent_match=1, Command OK confirmed
4. Delivery blocked (delivered=Just) — Wildcard ACK returns ERR NO_MSG
5. Network Task crash — heartbeats every ~30s
6. SSL connection broken — RECV logs show active connection
7. SMP v6 incompatible — v7 upgrade, problem identical
8. SessionId in wire disturbs server — removed, server happy
9. Response parser offset — sessLen removed from 6 parsers, parsing correct
10. ACK chain interrupted — everything ACKed, Contact Queue never receives

### Wizard (Claude Code) Analyses

5 Haskell source analyses completed in parallel:

**SMP Wire Format:** Byte-level documentation of v6 vs v7 block formats. v6 includes `[1B sessIdLen][32B SessionId]` between signature and corrId. v7 omits this, saving 33 bytes. tForAuth (signed data) includes SessionId in both versions.

**Version Negotiation:** Server sends smpVersionRange (e.g. 6-17). Client sends single chosen version in ClientHello: `[2B payloadLen][2B version]` — one version, not min+max. ALPN "smp/1" enables full server range; without ALPN only v6 via `legacyServerSMPRelayVRange`.

**Queue Routing:** After duplex handshake, App→ESP32 goes via Q_A on smp1.simplexonflux.com (ESP32 is recipient), ESP32→App goes via Q_B on smp19.simplex.im (App is recipient). Both servers correct.

**Queue Rotation:** NO automatic rotation after CON. Only manual via `/switch` command.

**Version Limits:** Official spec documents only v6 and v7. Server internally ranges 6-17/18 but actively rejects v8+ with Connection Reset.

## Message to Evgeny

After exhausting all hypotheses:

> "Is there a condition where the server would accept a SUB (respond OK) but then not deliver incoming MSGs to that subscription? Could there be a 'subscriber client' mismatch where the subscription is registered but delivery routes to a different internal client object?"

## Architecture at End of Session 30

```
Network Task (Core 0, 12KB PSRAM):
  smp_read_block(s_ssl, 1000ms timeout) loop
  1s Socket Timeout via setsockopt()
  Heartbeat every ~30 loops
  subscribe_all_contacts() with Drain-Loop
  Main SSL: sock 54, smp1.simplexonflux.com

Main Task / smp_app_run() (Internal SRAM, 64KB):
  Wildcard ACK + Re-Subscribe All at start
  Ring Buffer read → Transport Parse → Decrypt
  Keyboard queue poll (non-blocking)
  Peer SSL: sock 55/56, smp19.simplex.im (dies after idle)

UI Task (Core 1, 8KB PSRAM):
  Empty loop (future display output)
```

Memory: Internal SRAM 46KB free, PSRAM ~8.2MB free.

## Files Changed

`main.c` (kbd_queue handover, version 0x07, parser sessLen removed), `smp_tasks.h` (signatures updated), `smp_tasks.c` (keyboard poll, heartbeat, socket timeout, re-subscribe, wildcard ACK, parser), `smp_contacts.c` (drain loop, corrId 24B, wire v7, parser), `smp_ack.c` (corrId 24B, wire v7), `smp_network.c` (RECV logging, BLOCK OUT dump).

## Lessons Learned

**L149 (HIGH): SMP Versions.** Official protocol documents only v6 and v7. Server reports internal range (6-17/18) but rejects v8+. ClientHello sends ONE version, not min+max. ALPN "smp/1" enables full range; without ALPN only v6.

**L150:** corrId must be 24 random bytes (length prefix 0x18), not 1 byte. corrId is reused as NaCl nonce, therefore must be random and unique.

**L151:** After the 42d handshake, responses arrive in unpredictable order. Drain loop with entity matching (recipientId comparison) handles this with up to 5 attempts.

**L152 (HIGH):** Batch framing is mandatory from v4. Every block: `[2B contentLen][1B txCount][2B txLen][transmission][padding '#']`. Even single transmissions need txCount=1. Server parses with batch=True, no fallback. Only exception: handshake blocks (ClientHello, ServerHello) use direct tPutBlock without batch.
