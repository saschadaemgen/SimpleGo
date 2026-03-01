![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 21: Session 24
# 🏆 First Chat Message from a Microcontroller!

**Document Version:** v38  
**Date:** 2026-02-11/13 Session 24  
**Status:** ✅ First A_MSG Sent — MILESTONE #2!  
**Previous:** Part 20 - Session 23 (CONNECTED)

---

## 🏆 MILESTONE #2 ACHIEVED!

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER! 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   SimpleX App shows: "Hello from ESP32!"                               │
  │                                                                         │
  │   The world's first chat message sent from a microcontroller           │
  │   through the complete SimpleX encryption stack.                       │
  │                                                                         │
  │   Date: February 11, 2026                                              │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App         │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 411. Session 24 Overview

### 411.1 Starting Point

After Session 23 we had:
- ✅ "Connected" status in SimpleX App
- ✅ Complete handshake: SKEY → CONF → KEY → HELLO → CON
- ✅ Receive chain: TLS → SMP → E2E → Ratchet → Zstd → JSON
- ❌ No chat messages sent or received
- ❌ Assumption: App's HELLO was received on Q_B (WRONG!)

### 411.2 Session 24 Goals

1. Send first chat message (A_MSG)
2. Receive chat messages on Reply Queue (Q_B)
3. Achieve bidirectional communication

### 411.3 Session 24 Results

| Goal | Result |
|------|--------|
| Send A_MSG | ✅ **MILESTONE! "Hello from ESP32!" displayed in app** |
| Q_B Ratchet Decrypt | ✅ Tag 'I' ConnInfo successfully decrypted |
| Session 23 Correction | ✅ "HELLO on Q_B" was false positive |
| Receive messages | ❌ App sends to Q_B but ESP32 doesn't receive |
| Bidirectional | ❌ App doesn't activate connection — format error suspected |

### 411.4 Session Statistics

| Metric | Value |
|--------|-------|
| Start | 2026-02-11 |
| End | 2026-02-13 |
| Tasks | 43a-b, 44a, 45a-m |
| Claude Code Sessions | 3 (simplexmq, simplex-chat, SimpleGo) |
| Agents | Mausi (Evil Stepsister #1), Hasi (Evil Stepsister #2), Aschenputtel (Cinderella), Claude Code (The Verifier) |

---

## 412. Phase 1: A_MSG Wire Format Analysis (Task 43a)

### 412.1 Claude Code Analysis — Complete A_MSG Byte Layout

The complete A_MSG wire format was documented through Haskell source analysis:

```
Layer 1: ClientMsgEnvelope (SMP-level)
  [2B phVersion][1B '0' Nothing][24B CbNonce][Tail: cmEncBody]

Layer 2: ClientMessage (after SMP decrypt)
  [1B '_' PHEmpty][Tail: AgentMsgEnvelope]

Layer 3: AgentMsgEnvelope
  [2B agentVersion][1B 'M' envelope tag][Tail: encAgentMessage]

Layer 4: AgentMessage (after Ratchet decrypt + unPad)
  [1B 'M' AgentMessage tag]
  [8B sndMsgId Int64 BE]
  [1B prevMsgHash len][0|32B hash data]
  [1B 'M' A_MSG tag]
  [rest: msgBody as Tail, raw bytes]
```

### 412.2 Key Discoveries

**MsgFlags is NOT in the encrypted payload:**
```
MsgFlags (notification=True/False) travels separately as SEND command parameter.
NOT embedded inside the encrypted agent message.
```

**prevMsgHash on first message:**
```
Database initializes last_snd_msg_hash to empty blob.
First message: prevMsgHash = "" → encoded as single byte 0x00.
Subsequent: SHA-256 (32 bytes) → 0x20 + 32 bytes hash data.
```

**sndMsgId encoding:**
```
Int64 = 2×Word32 big-endian (8 bytes total)
NOT Word16 as might be assumed from other fields.
```

**Padding:**
```
Same as HELLO: 15840 bytes for non-PQ connections.
Format: [2B len BE][AgentMessage bytes][0x23 '#' padding]
```

---

## 413. Phase 2: Q_B Ratchet Decrypt (Task 43b)

### 413.1 Session 23 Correction — Critical Discovery

**Session 23 claimed:** "App's HELLO received on Q_B" ✅  
**Session 24 discovered:** That was a **false positive!**

Evidence from Aschenputtel's log analysis:
```
E2E Decrypt on Q_B produced 15904 bytes.
A real HELLO without Ratchet would be ~3-5 bytes.
15904 bytes = clearly Ratchet-encrypted content.
```

Session 23 code had no Ratchet decrypt on Q_B — it interpreted a random `0x48` ('H') in the Ratchet ciphertext as HELLO.

### 413.2 Actual Q_B Content — Tag 'I' (AgentConnInfo)

After implementing full Ratchet decrypt on Q_B:

```
Q_B Ratchet Decrypt:
  AdvanceRatchet (NHKr) — first message from peer on new chain
  PN=0, Ns=0 — first message, none skipped
  emHeaderLen=2346 (PQ-Kyber active! Graceful degradation worked)
  Body: 8887 bytes → Zstd decompress → 12268 bytes JSON
  Content: AgentConnInfo Tag 'I' with peer profile
    - displayName, fullName, preferences
    - Embedded JPEG profile image
```

### 413.3 Corrected Handshake Flow

```
Session 23 believed:              Session 24 corrected:
  Step 6c: Q_B HELLO ✅            Step 6c: Q_B Tag 'I' ConnInfo ✅
                                   Step 6c: Q_B HELLO ❌ never received
```

The "Connected" status was correct because:
- The **App** shows Connected after receiving **our HELLO on Q_A**
- The App doesn't need us to acknowledge her Q_B messages

---

## 414. Phase 3: First A_MSG — MILESTONE! (Task 44a)

### 414.1 First Attempt — Raw UTF-8 (FAILED)

```
msgBody = "Hello from ESP32!" (raw UTF-8 bytes)
Server response: OK ✅ (server accepted)
App console: "error parsing chat message: not enough input"
App display: Nothing shown
```

**Diagnosis:** Server doesn't validate content — just forwards blindly. App successfully decrypted all layers (Ratchet + E2E worked!), but couldn't parse msgBody as ChatMessage JSON.

### 414.2 Fix — ChatMessage JSON Format

The msgBody must be wrapped in SimpleX ChatMessage JSON:

```json
{"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"Hello from ESP32!"}}}
```

This matches the same JSON format used in AgentConfirmation for profile data.

### 414.3 Second Attempt — JSON Format (SUCCESS! 🏆)

```
msgBody = ChatMessage JSON
Server response: OK ✅
Windows notification: "ESP32: Hello from ESP32!" ✅
App display: Message bubble with "Hello from ESP32!" ✅
```

### 414.4 Complete A_MSG Byte Example

```
AgentMessage Plaintext (29 bytes):
Offset  Hex                         Field
0       4D                          AgentMessage tag 'M'
1-8     00 00 00 00 00 00 00 01     sndMsgId = 1
9       00                          prevMsgHash len = 0 (first)
10      4D                          AMessage tag 'M' (A_MSG)
11-28   7B 22 76 22 3A 22 31 22...  ChatMessage JSON

Encrypt chain (identical to HELLO):
1. Pad to 15840 bytes
2. Ratchet-Encrypt (chain_key_send)
3. AgentMsgEnvelope wrap ([2B version][1B 'M'][ciphertext])
4. ClientMessage wrap ([1B '_'][envelope])
5. E2E Encrypt (crypto_box)
6. ClientMsgEnvelope wrap ([2B version][1B '0'][24B nonce][ciphertext])
7. SEND on Q_A with MsgFlags = 'T' (notification=True)
```

---

## 415. Phase 4: ACK Protocol Analysis (Tasks 45c-45h)

### 415.1 The Missing ACK Theory

After A_MSG worked, the next goal was receiving messages on Q_B. Initial symptom: Q_B listen loop received nothing (timeout after 30s).

**Hypothesis:** Missing ACK after receiving ConnInfo MSG blocks server delivery.

### 415.2 Claude Code ACK Analysis (Task 45c/45g)

Comprehensive analysis of SMP ACK protocol:

```
SMP Flow Control:
  Server delivers MSG → sets delivered = Just (msgId, ts)
  Client MUST send ACK → sets delivered = Nothing
  Without ACK → Server blocks ALL further MSG delivery

ACK Wire Format:
  "ACK " + [1B len][N bytes msgId]
  Signed with rcv_private_auth_key (Recipient Command)

ACK Response:
  Queue empty → OK
  Next MSG pending → MSG (immediate delivery)
```

### 415.3 Agent-Level ACK Timing (Task 45h)

```
Confirmation (Tag 'D' or 'I') → ACK sofort (internal, automatic)
HELLO                         → ACK sofort + Delete
A_MSG                         → ACK deferred (waits for app/user)
```

Key insight: The Haskell agent ACKs confirmation messages immediately and automatically. Our code needed to do the same.

### 415.4 SMP Response Multiplexing Problem

**Discovery:** On subscribed connections, responses (OK) and notifications (MSG) can interleave:

```
ESP32 sends: ACK
Server sends: OK    ← could come
Server sends: MSG   ← could ALSO come, at ANY time

Our code: smp_read_block() → expects OK → gets MSG → 💥
```

### 415.5 queue_subscribe() Bug (Tasks 45e-45h)

**First fix attempt (Claude Code):** SMP Transport Parser — calculated header offsets to find command tag. **Failed** because offset calculation was wrong (345 instead of ~10).

**Working fix:** Simple scan approach — search for "OK", "END", "MSG" in response buffer:

```c
// Scan for OK
for (int i = 0; i < content_len - 1; i++) {
    if (resp[i] == 'O' && resp[i+1] == 'K') return true;
}
// Scan for MSG (buffer it for later)
for (int i = 0; i < content_len - 2; i++) {
    if (resp[i] == 'M' && resp[i+1] == 'S' && resp[i+2] == 'G') {
        pending_msg = store(block);
        return true;
    }
}
```

Added `pending_msg` buffer struct for MSG caught during ACK/SUB reads, returned by subsequent `queue_read_raw()` calls.

---

## 416. Phase 5: Bidirectional Investigation (Tasks 45i-45m)

### 416.1 Timing Theory — Disproved (45i)

**Hypothesis:** Q_B TLS connection dies during handshake processing.

**Test:** Fresh reconnect + subscribe directly before listen loop.  
**Result:** Connection alive, subscription confirmed — still no data. **Timing theory disproved.**

### 416.2 Queue ID Verification — All Correct (45l)

Aschenputtel performed byte-for-byte comparison across two separate test runs:

```
╔════════════════════════════════════════════════════════════════════════════╗
║  ALL QUEUE IDs CONSISTENT — NO MISMATCH!                                   ║
╚════════════════════════════════════════════════════════════════════════════╝

Check              IDS Response    Tag 'D' Sent    Match?
sndId              4249f302...     4249f302...     ✅ IDENTICAL
rcvId (all SUBs)   c93e8104...     consistent      ✅
Server             simplexonflux   simplexonflux   ✅
keyHash            c505bfb9...     c505bfb9...     ✅
e2e_public         860dfcca...     860dfcca...     ✅
```

### 416.3 Raw TLS Read Test — Server Empty (45m)

**Definitive test:** Direct `mbedtls_ssl_read()` after subscribe — zero bytes. Re-SUB after timeout — "OK" (queue empty, no pending MSG).

```
Conclusion: Server genuinely has ZERO messages for Q_B.
The App is NOT sending to our Reply Queue.
```

### 416.4 Root Cause Analysis

```
Evidence chain:
  ✅ Queue IDs correct (Aschenputtel proved)
  ✅ Connection alive (45i proved)
  ✅ App sends on Q_B per Haskell source (Claude Code proved)
  ✅ Server accepts App's messages (one checkmark in App)
  ❌ Server has zero messages for ESP32's Q_B
  
Possible explanations:
  A) App doesn't treat our Contact as fully active
     → Format error in AgentConfirmation or HELLO
     → App silently discards and doesn't respond
  
  B) App sends to Q_B but server delivers to wrong subscriber
     → Unlikely given Queue ID verification
  
  C) One checkmark = server accepted SEND, but on different queue
     → App might have created its own queue pair
```

**Most likely: Option A — Format error in our AgentConfirmation or HELLO causes the App to not fully activate the connection.**

Evidence: "Hello from ESP32!" shows with only **one checkmark** (server accepted, app didn't confirm receipt), and App's outgoing messages also show one checkmark.

### 416.5 Connection Theory — DISPROVED (45n)

The late-session hypothesis that `subscribe_all_contacts()` and `queue_conn` created competing subscriptions was **disproved:**

```
45n test: Added PHEmpty handler in Main Receive Loop
Result: Reply Queue SUB on main connection returns END (empty)
→ Server has zero messages on BOTH connections
→ NOT a wrong-connection issue
```

### 416.6 Final Root Cause Analysis (End of Session 24)

```
ALL connection theories eliminated:
  ❌ Wrong TLS connection    → 45n: main ssl also empty
  ❌ Queue ID mismatch       → 45l: byte-identical
  ❌ Dead connection         → 45i: fresh, still empty
  ❌ ACK blocking            → 45m: server genuinely empty

FACT: Server has ZERO messages for Q_B
FACT: App shows "test" with ONE checkmark (queued locally, never sent?)
FACT: "Hello from ESP32!" displayed but no delivery receipt

CONCLUSION: App does NOT mark our Contact as Active.
It queues messages locally but never sends SEND to server.

CAUSE: Format error in AgentConfirmation, HELLO, or A_MSG.
App receives our messages, partially processes them
(displays "Hello from ESP32!"), but connection state
never transitions to Active.

NEXT STEP (Session 25):
  Haskell analysis: What does App validate after receiving
  AgentConfirmation + HELLO? What conditions for sndStatus=Active?
```

---

## 417. Task Overview Session 24

| # | Agent | Type | Description | Result |
|---|-------|------|-------------|--------|
| 43a | Claude Code | Analysis | A_MSG Wire Format (simplexmq) | ✅ Complete byte layout |
| 43b | Hasi | Code | Q_B Ratchet Decrypt | ✅ Tag 'I' ConnInfo decrypted |
| 44a | Hasi | Code | Send first A_MSG | ✅ **MILESTONE: "Hello from ESP32!"** |
| 45a | Hasi | Debug | Listen for App messages on Q_B | ❌ Timeout, nothing received |
| 45b | Hasi | Debug | Diagnose Q_B silence | ✅ Ratchet OK, connection OK |
| 45c | Claude Code | Analysis | ACK Protocol (simplexmq) | ✅ Complete ACK documentation |
| 45e | Hasi | Code | ACK handling + pending_msg buffer | ✅ Implemented |
| 45f | Claude Code | Analysis | SimpleGo flow analysis | ✅ Missing ACK identified |
| 45g | Claude Code | Analysis | ACK agent-level logic (simplexmq) | ✅ ACK timing documented |
| 45h | Claude Code | Analysis | ACK handshake flow (simplex-chat) | ✅ Complete MSG/ACK chain |
| 45i | Hasi | Code | Fresh reconnect before listen | ✅ Timing theory disproved |
| 45k | Claude Code | Analysis | Queue routing (simplexmq) | ✅ App sends on Q_B confirmed |
| 45l | Aschenputtel | Log | Queue ID byte-for-byte comparison | ✅ All IDs match |
| 45m | Hasi | Code | Raw TLS read test | ✅ Server confirmed empty |
| 45n | Hasi | Code | Main connection listen test | ✅ Also empty — connection theory disproved |

---

## 418. Critical Discoveries Session 24

### 418.1 Discovery 1: Session 23 False Positive

```
Session 23: "HELLO received on Q_B" → FALSE!
Reality: 15904 bytes after E2E = Ratchet-encrypted ConnInfo
The code had no Ratchet decrypt → random byte matched 'H'
```

### 418.2 Discovery 2: msgBody Must Be ChatMessage JSON

```
Raw UTF-8 text → "error parsing chat message: not enough input"
ChatMessage JSON → App displays message correctly

Required format:
  {"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"..."}}}
```

### 418.3 Discovery 3: SMP ACK Flow Control

```
Server delivers MSG → blocks until ACK received
Missing ACK = queue backs up, no further delivery
ACK is a Recipient Command (signed with rcv_private_auth_key)
ACK response can be OK (empty) or MSG (next message)
```

### 418.4 Discovery 4: PQ-Kyber Active in Wild

```
App sends with emHeaderLen=2346 (Post-Quantum Kyber headers)
Standard: emHeaderLen=124
Our graceful degradation works — body decrypt succeeds despite PQ
```

### 418.5 Discovery 5: App Doesn't Fully Activate Connection

```
ESP32 sends "Hello from ESP32!" → App shows it (one checkmark)
App sends "test" → one checkmark (server accepted)
ESP32 Q_B → empty
→ App doesn't treat connection as fully bidirectional
→ Likely format error in our messages causes silent discard
```

---

## 419. Files Changed (Session 24)

| File | Tasks | Changes |
|------|----------|---------|
| `main/main.c` | 43b, 44a, 45a-m | Q_B decrypt, A_MSG send, listen loop, diagnostics |
| `main/smp_queue.c` | 45e, 45g, 45h | ACK command, pending_msg buffer, queue_subscribe fix |
| `main/include/smp_queue.h` | 45e | ACK function declarations |
| `main/smp_handshake.c` | 44a | ChatMessage JSON format, send_chat_message() |
| `main/include/smp_handshake.h` | 44a | Function declarations |
| `main/smp_peer.c` | 43b | Q_B E2E key handling |
| `main/include/smp_peer.h` | 43b | Peer declarations |

---

## 420. Git Commits Session 24

| Commit | Message |
|--------|---------|
| c7f02ce | `feat(queue): add Reply Queue ratchet decrypt and AgentConnInfo parsing` |
| 6199ade | `feat(msg): send first A_MSG chat message over SimpleX protocol` |
| 3d6cdb6 | `feat(queue): add ACK handling and SMP response multiplexing for subscribed connections` |
| 7c9efe7 | `fix(queue): replace broken SMP transport parser with scan-based response detection` |

---

## 421. Evgeny Contact

**2026-02-08:** Evgeny reached out again, relationship good.

**Relevant Evgeny tips for Session 25:**
- "Whatever you do for networking, make sure to handle lost responses — that was the biggest learning"
- Idempotent commands: Key erst speichern, dann senden
- JSON ChatMessage Format: "easier to work with the same format" (even for MCU)
- Widget Architecture RFC published — ESP32 = Widget Producer, not Consumer

---

## 422. Multi-Agent Workflow Notes

### 422.1 Agent Roster Session 24

```
👑 Mausi         — Evil Stepsister #1 (Strategy, Protocol, Tasks)
🐰 Hasi          — Evil Stepsister #2 (Code, Builds, Tests)
🧹 Aschenputtel  — Cinderella (Log Analysis)
🧙‍♂️ Claude Code   — The Verifier (Haskell Analysis, NO Git access)
🧑 Cannatoshi    — The Coordinator (Task distribution, Git)
```

### 422.2 Workflow Lessons Learned

**Claude Code Git Access = Chaos:**
- Created branch instead of committing to main
- Left `pending_msg` undeclared (struct in wrong position)
- Build error cost debugging time
- **Rule for Session 25:** Claude Code analyzes only, never commits

**Aschenputtel Value Proven:**
- Byte-for-byte Queue ID verification eliminated false leads
- Log analysis confirmed false positive from Session 23
- Separate analysis chat keeps main strategy chat clean

---

## 423. Session 24 Summary

### What Was Achieved

- 🏆 **First chat message from a microcontroller!** "Hello from ESP32!"
- ✅ **Q_B Ratchet decrypt working** — PQ-Kyber graceful degradation
- ✅ **Session 23 correction** — "HELLO on Q_B" was false positive (ConnInfo)
- ✅ **ACK protocol fully documented** — flow control, wire format, agent timing
- ✅ **queue_subscribe() fixed** — handles OK, END, MSG responses
- ✅ **pending_msg buffer** — catches MSG during ACK/SUB reads
- ✅ **Queue IDs verified** — byte-for-byte match, no mismatch
- ❌ **Bidirectional not achieved** — App doesn't send to Q_B

### Open Bug for Session 25 — Format Error

```
ROOT CAUSE: App does NOT fully activate our connection.
Server has zero messages because App never sends.
All connection/queue theories eliminated.

Likely: Format error in AgentConfirmation or HELLO causes
App to not transition sndStatus to Active.

Next step: Haskell source analysis — what does App validate?
What conditions must be met for sndStatus=Active?
```

### Key Takeaway

```
SESSION 24 SUMMARY:
  - 🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER!
  - ChatMessage JSON format discovered (not raw UTF-8)
  - Session 23 "HELLO on Q_B" was a false positive
  - ACK protocol is critical SMP flow control
  - App doesn't fully activate our connection (doesn't send)
  - All connection/queue theories eliminated
  - Format error in AgentConfirmation or HELLO suspected
  - Queue IDs are all correct — bug is in message format

"From Connected to Communicating — one direction at a time."
"The message arrives, but the conversation hasn't started yet." 🐭🐰🧹
```

---

## 424. Future Work (Session 25)

### Phase 1: Code Refactoring
- main.c from 2400 lines → ~150 lines
- Extract: smp_msg_handler.c, smp_agent_handler.c, smp_chat.c
- Clean architecture for bug analysis

### Phase 2: Bidirectional Bug Fix
- Haskell analysis: What does App validate after AgentConfirmation + HELLO?
- Identify format mismatch
- Fix and test

### Phase 3: Full Bidirectional Communication
- Third milestone: Receive messages from App
- Message persistence on ESP32
- UI integration on T-Deck display

---

**DOCUMENT CREATED: 2026-02-13 Session 24 v38**  
**Status: ✅ First A_MSG Sent — MILESTONE #2!**  
**Key Achievement: First chat message from a microcontroller**  
**Open: Bidirectional communication (App → ESP32)**  
**Next: Session 25 — Refactoring + Bug Fix**
