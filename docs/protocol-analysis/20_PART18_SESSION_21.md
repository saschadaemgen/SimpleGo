![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 18: Session 21
# HELLO Format Debugging, 7 Bugs Fixed, v3 EncRatchetMessage, KEY Command

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-06/07
**Status:** COMPLETED -- v3 format implemented, app still cannot decrypt HELLO
**Previous:** Part 17 - Session 20
**Next:** Part 19 - Session 22
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 21 SUMMARY

```
Session 21 activated the ratchet state update (previously LOG ONLY),
implemented the 4-header-key architecture (HKs/NHKs/HKr/NHKr) with
NHK-to-HK promotion, and fixed seven bugs in HELLO message format
(#20-#26). The KEY command was implemented (optional for unsecured
queues). RSYNC was identified as the app's crypto error indicator.
The root cause was a v2/v3 format mismatch: smp_ratchet.c encrypted
in v2 format while the app expected v3. Bug #26 implemented v3
EncRatchetMessage format (2-byte length prefixes, KEM Nothing field).
Server accepts v3 HELLO, but app still cannot decrypt (suspects:
HKs/NHKs promotion or E2E version in AgentConfirmation).

 Ratchet state update activated
 4-header-key architecture (HKs/NHKs/HKr/NHKr)
 7 bugs fixed (#20-#26)
 KEY command implemented
 v3 EncRatchetMessage format implemented
 App still "Connecting..." (RSYNC crypto error)
```

---

## Ratchet State Architecture

### SameRatchet vs AdvanceRatchet

| Mode | Trigger | Operations |
|------|---------|------------|
| SameRatchet | Same DH key (dh_changed=false) | chainKdf only |
| AdvanceRatchet | New DH key (dh_changed=true) | 2x rootKdf + chainKdf |

### 4 Header Key Slots

| Key | Name | Usage |
|-----|------|-------|
| HKs | header_key_send | Encrypt our outgoing headers |
| NHKs | next_header_key_send | Becomes HKs after our DH ratchet |
| HKr | header_key_recv | Decrypt incoming headers |
| NHKr | next_header_key_recv | Becomes HKr after peer's DH ratchet |

Initial assignment from X3DH: HKs = hk [0-31], NHKr = nhk [32-63]. HKr and NHKs are set through promotion during ratchet steps.

---

## Bugs #20-#26: HELLO Format

### Bug #20: PrivHeader for HELLO

```
WRONG:  '_' (0x5F) = PHEmpty
RIGHT:  0x00 = No PrivHeader (regular messages have no PrivHeader)
```

PrivHeader uses a custom scheme (not standard Maybe): 'K'=PHConfirmation, '_'=PHEmpty, 0x00=none.

### Bug #21: AgentVersion for AgentMessage

```
WRONG:  agentVersion = 2
RIGHT:  agentVersion = 1 (AgentMessage uses v1, AgentConfirmation uses v7)
```

### Bug #22: prevMsgHash Encoding

```
WRONG:  Raw empty bytes
RIGHT:  Word16 prefix [0x00 0x00] for empty hash (Large encoding)
```

### Bug #23: cbEncrypt Padding

```
WRONG:  Encrypt unPad output (raw plaintext)
RIGHT:  Encrypt padded plaintext (pad first, then encrypt)
```

Receiver does: decrypt then unPad. Sender must: pad then encrypt.

### Bug #24: DH Key for HELLO

```
WRONG:  rcv_dh_public (receiver's key) for HELLO encryption
RIGHT:  snd_dh_public (sender's key for reply queue)
```

### Bug #25: PubHeader Nothing

```
WRONG:  Field missing entirely
RIGHT:  '0' (0x30) = Nothing (standard Maybe encoding)
```

### Bug #26: v3 EncRatchetMessage Format

Root cause of RSYNC: app expected v3 format (2-byte length prefixes), but our code sent v2 (1-byte prefixes). The `encodeLarge` function switches at v>=3.

| Component | v2 | v3 |
|-----------|----|----|
| emHeader length prefix | 1 byte | 2 bytes (Word16 BE) |
| emHeader size | 123 bytes | 124 bytes |
| ehBody length prefix | 1 byte | 2 bytes (Word16 BE) |
| MsgHeader has KEM | No | Yes ('0' = Nothing) |
| MsgHeader contentLen | 79 | 80 |

v3 EncRatchetMessage layout:

```
[00 7C]      emHeader length (Word16 BE = 124)
[124 bytes]  emHeader
[16 bytes]   payload AuthTag
[N bytes]    encrypted payload (Tail)

emHeader:
  [00 03]      ehVersion (v3)
  [16 bytes]   ehIV
  [16 bytes]   ehAuthTag
  [00 58]      ehBody length (Word16 BE = 88)
  [88 bytes]   encrypted MsgHeader

MsgHeader (v3, padded to 88):
  [00 50]      contentLen (80)
  [00 02]      msgMaxVersion
  [44]         DH key length (68)
  [68 bytes]   msgDHRs SPKI
  [30]         KEM Nothing ('0')
  [4 bytes]    msgPN (Word32 BE)
  [4 bytes]    msgNs (Word32 BE)
  [6 bytes]    '#' padding
```

---

## HELLO Wire Format

```
AgentMessage (HELLO):
  [Word16 BE]  agentVersion = 1
  [Word16 BE]  SMP version
  [Word16 BE]  prevMsgHash length = 0 (empty)
  [Tail]       'H' + '0' (HELLO + AckMode_Off)

Wrapped in ClientMessage:
  PrivHeader = 0x00 (none)
  PubHeader = '0' (Nothing)
  Padded to 15840 bytes
```

---

## KEY Command

```
Body: "KEY " + 0x2C + peer_sender_auth_key[44 bytes Ed25519 SPKI]
Signed with: rcv_auth_private (recipient command)
Response: OK
```

Reply Queues are unsecured: SEND works without KEY authorization. KEY is optional for functionality but important for security hardening.

---

## RSYNC: Crypto Error Indicator

When the app fails to decrypt a received message, it triggers a RSYNC (Ratchet Sync) event. `chatItemNotFoundByContactId` = RSYNC with no existing chat item for this contact. This confirmed the app could not decrypt our HELLO.

---

*Part 18 - Session 21: HELLO Format + v3*
*SimpleGo Protocol Analysis*
*Original dates: February 6-7, 2026*
*Rewritten: March 4, 2026 (v2)*
*7 bugs fixed, v3 format, KEY command, app still cannot decrypt*
