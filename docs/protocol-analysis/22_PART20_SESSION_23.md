![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 20: Session 23
# CONNECTED: First SimpleX Connection on a Microcontroller

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-07/08
**Status:** COMPLETED -- ESP32 connected, historic milestone
**Previous:** Part 19 - Session 22
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 23 SUMMARY

```
Session 23 achieved the first SimpleX connection on a microcontroller.
The Session 22 assumption that the app sends tag 'D' with Reply Queue
Info was wrong: the app sends tag 'I' (AgentConnInfo, profile only).
We send tag 'D' (AgentConnInfoReply with Reply Queue). The app sends
PHConfirmation 'K', confirming Legacy Path (requires KEY + HELLO).
After reconnecting TLS to the Reply Queue (timeout during processing),
sending SUB + KEY, then HELLO on Contact Queue, the app responded
with HELLO on Reply Queue. Both sides reached CON status.

Date: February 8, 2026 ~17:36 UTC
Platform: ESP32-S3 (LilyGo T-Deck)
Result: "ESP32 - Connected" in SimpleX App

 Role clarification: ESP32=Bob (accepting), App=Alice (initiating)
 Tag correction: App sends 'I' not 'D', we send 'D'
 Legacy Path confirmed (PHConfirmation 'K')
 KEY + HELLO exchange completed
 Zero new bugs (all 31 from earlier sessions sufficient)
```

---

## Role Clarification

Session 22 assumed the app sends tag 'D' (AgentConnInfoReply with Reply Queue Info). Hex dump logging proved this never triggers. The app sends tag 'I' (AgentConnInfo, profile only).

```
ESP32 = Accepting Party (Bob)
  Creates Reply Queue (Q_B)
  Sends tag 'D' (AgentConnInfoReply with Q_B info + profile)
  Sends HELLO on Contact Queue (Q_A)

App = Initiating Party (Alice)
  Creates Contact Queue (Q_A) via Invitation
  Sends tag 'I' (AgentConnInfo, profile only)
  Sends HELLO on Reply Queue (Q_B)
```

The Reply Queue Info was sent BY US in our AgentConfirmation, not received from the app.

---

## Legacy vs Modern Path

The received AgentConfirmation contains PrivHeader 'K' (PHConfirmation), confirming Legacy Path. Session 22's "no HELLO needed" discovery applies to Modern Path (PHEmpty '_') only.

| Path | PrivHeader | Requirements |
|------|-----------|--------------|
| Legacy | 'K' (PHConfirmation) | KEY command + HELLO exchange |
| Modern | '_' (PHEmpty) | Only ACK, CON immediate |

---

## Verified Handshake Flow

```
Step   Queue   Direction      Content                           
1.     --      App            NEW -> Q_A, Invitation QR         
2a.    Q_A     ESP32->App     SKEY (register sender auth)       
2b.    Q_A     ESP32->App     CONF tag 'D' (Q_B + profile)     
3.     --      App            processConf -> CONF event         
4.     --      App            LET/accept confirmation           
5a.    Q_A     App            KEY on Q_A (senderKey)            
5b.    Q_B     App->ESP32     SKEY on Q_B                       
5c.    Q_B     App->ESP32     Tag 'I' (app profile)             
6a.    Q_B     ESP32          Reconnect + SUB + KEY             
6b.    Q_A     ESP32->App     HELLO                             
6c.    Q_B     App->ESP32     HELLO                             
7.     --      Both           CON                               
```

---

## KEY Command Implementation

```
Body: "KEY " + 0x2C + peer_sender_auth_key[44 bytes Ed25519 SPKI]
Signed with: rcv_private_auth_key (recipient command)
```

KEY is a recipient command: we authorize the sender (app) to send messages on our queue. The `peer_sender_auth_key` was already stored from the received AgentConfirmation's PHConfirmation.

---

## TLS Timeout and Reconnect

The Reply Queue TLS connection timed out during AgentConfirmation processing. Solution:

```
1. Reconnect TLS to Reply Queue server
2. Send SUB (re-subscribe to queue)
3. Send KEY command
4. App IMMEDIATELY responds with message on Q_B
```

Sequence is critical: KEY first (authorize sender), then HELLO (complete handshake). Reversing the order causes the app to fail (not yet authorized).

---

## Complete Protocol Stack (Verified Working)

```
RECEIVE CHAIN (Contact Queue Q_A):
Layer 0:  TLS 1.3 (mbedTLS)
Layer 1:  SMP Transport (rcvDhSecret + cbNonce)
Layer 2:  E2E (e2eDhSecret + cmNonce)
Layer 2.5: unPad
Layer 3:  ClientMessage Parse
Layer 4:  EncRatchetMessage Parse (dynamic KEM)
Layer 5:  Double Ratchet Header Decrypt
Layer 6:  Double Ratchet Body Decrypt
Layer 7:  ConnInfo Parse + Zstd
Layer 8:  Peer Profile JSON

SEND CHAIN:
Layer 9a: Reply Queue Reconnect + SUB
Layer 9b: KEY Command on Q_B
Layer 9c: HELLO on Contact Queue Q_A
Layer 10: App receives HELLO, sets sndStatus Active
Layer 11: App sends HELLO on Q_B
Layer 12: CON -- CONNECTED
```

---

## Session 22 Correction

```
Session 22 assumed:
  "Modern SimpleX needs no HELLO, app sends Reply Queue in tag 'D'"

Session 23 discovered:
  App sends tag 'I' (no Reply Queue info)
  WE already sent Reply Queue in our tag 'D'
  Legacy Path (PHConfirmation 'K') requires KEY + HELLO
  HELLO IS needed for Legacy Path
```

---

## Project Statistics at Connection

| Metric | Value |
|--------|-------|
| Sessions to connection | 23 (January 17 to February 8, 2026) |
| Total bugs fixed | 31 |
| New bugs in Session 23 | 0 |
| Protocol layers implemented | 12 |
| Encryption layers per message | 4 (TLS + SMP + E2E + Double Ratchet) |

---

*Part 20 - Session 23: CONNECTED*
*SimpleGo Protocol Analysis*
*Original dates: February 7-8, 2026*
*Rewritten: March 4, 2026 (v2)*
*First SimpleX connection on a microcontroller*
*"The journey of 31 bugs ends with zero new ones."*
