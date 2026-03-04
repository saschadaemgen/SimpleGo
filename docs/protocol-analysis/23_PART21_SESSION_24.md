![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 21: Session 24
# First Chat Message from a Microcontroller

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-11/13
**Status:** COMPLETED -- "Hello from ESP32!" displayed in SimpleX App
**Previous:** Part 20 - Session 23
**Next:** Part 22 - Session 25
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 24 SUMMARY

```
Session 24 sent the first chat message from a microcontroller through
the complete SimpleX encryption stack. Session 23's "HELLO received
on Q_B" was corrected as a false positive (15904 bytes = ratchet-
encrypted ConnInfo, not a 3-5 byte HELLO). Full ratchet decrypt on
Q_B succeeded: AdvanceRatchet, PQ-Kyber headers (emHeaderLen=2346),
tag 'I' ConnInfo with peer profile. First A_MSG attempt with raw
UTF-8 failed ("error parsing chat message"); wrapping in ChatMessage
JSON succeeded. App showed "Hello from ESP32!" but bidirectional
communication was not achieved: server had zero messages for Q_B.
App does not fully activate connection (format error suspected).

 Session 23 false positive corrected (Q_B content was ConnInfo)
 Q_B ratchet decrypt working (PQ-Kyber graceful degradation)
 First A_MSG sent: "Hello from ESP32!"
 ChatMessage JSON format discovered (not raw UTF-8)
 ACK protocol documented (SMP flow control)
 Bidirectional blocked: app does not send to Q_B
```

---

## Session 23 Correction

Session 23 claimed "App's HELLO received on Q_B". Log analysis proved this was a false positive: E2E decrypt on Q_B produced 15904 bytes (a real HELLO would be 3-5 bytes). The code had no ratchet decrypt on Q_B and interpreted a random 0x48 ('H') in ratchet ciphertext as HELLO. After implementing full ratchet decrypt, the actual content was tag 'I' (AgentConnInfo with peer profile).

---

## A_MSG Wire Format

```
Layer 1: ClientMsgEnvelope (SMP-level)
  [2B phVersion][1B '0' Nothing][24B CbNonce][Tail cmEncBody]

Layer 2: ClientMessage (after SMP decrypt)
  [1B '_' PHEmpty][Tail AgentMsgEnvelope]

Layer 3: AgentMsgEnvelope
  [2B agentVersion][1B 'M' envelope tag][Tail encAgentMessage]

Layer 4: AgentMessage (after ratchet decrypt + unPad)
  [1B 'M' AgentMessage tag]
  [8B sndMsgId Int64 BE]
  [1B prevMsgHash len][0 or 32B hash]
  [1B 'M' A_MSG tag]
  [rest: msgBody as Tail]
```

Key details: MsgFlags (notification) travels as SEND command parameter, not in the encrypted payload. First message has prevMsgHash = empty (0x00). sndMsgId is Int64 (8 bytes), not Word16. Padding: 15840 bytes for non-PQ connections.

---

## ChatMessage JSON Format

Raw UTF-8 text fails with "error parsing chat message: not enough input". The msgBody must be ChatMessage JSON:

```json
{"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"Hello from ESP32!"}}}
```

SEND command uses MsgFlags = 'T' (notification=True).

---

## SMP ACK Protocol

```
Server delivers MSG -> blocks until ACK received
Missing ACK = queue backs up, no further delivery
ACK is a Recipient Command (signed with rcv_private_auth_key)

ACK timing per message type:
  Confirmation (tag 'D'/'I') -> ACK immediately (automatic)
  HELLO                      -> ACK immediately + delete
  A_MSG                      -> ACK deferred (waits for user)

ACK response:
  Queue empty -> OK
  Next MSG pending -> MSG (immediate delivery)
```

On subscribed connections, OK and MSG responses can interleave. The SMP parser was fixed to scan for "OK"/"END"/"MSG" patterns with a pending_msg buffer for unexpected MSG during ACK/SUB reads.

---

## Bidirectional Investigation

All connection theories were eliminated:

| Theory | Test | Result |
|--------|------|--------|
| Wrong TLS connection | Main SSL listen | Also empty |
| Queue ID mismatch | Byte-for-byte comparison | All identical |
| Dead connection | Fresh reconnect before listen | Still empty |
| ACK blocking | Implemented ACK | Server genuinely empty |

Server has zero messages for Q_B. App shows "Hello from ESP32!" with one checkmark (server accepted, no delivery receipt). App's outgoing "test" also shows one checkmark. Conclusion: app does not fully activate connection due to format error in AgentConfirmation or HELLO.

---

*Part 21 - Session 24: First Chat Message*
*SimpleGo Protocol Analysis*
*Original dates: February 11-13, 2026*
*Rewritten: March 4, 2026 (v2)*
*"Hello from ESP32!" -- first chat message from a microcontroller*
