# SimpleGo Wire Format Specification

Complete wire format documentation for the SimpleX SMP protocol.

---

## Overview

This document describes the binary encoding format used by SimpleGo.

---

## Length Encoding Strategies

| Strategy | Usage | Format |
|----------|-------|--------|
| Standard | Fields <= 254 bytes | 1 byte containing length |
| Large | Fields > 254 bytes | 0xFF + Word16 BE |
| Tail | Last field in structure | No prefix |

Important: Adding a length prefix to a Tail field causes parsing errors.

---

## Numeric Encodings

### Word16 (Big Endian)

2 bytes, most significant byte first.

| Value | Encoding |
|-------|----------|
| 0 | 0x00 0x00 |
| 1 | 0x00 0x01 |
| 256 | 0x01 0x00 |

### Word32 (Big Endian)

4 bytes, most significant byte first.

---

## AgentConfirmation Message

| Offset | Size | Field | Encoding | Value |
|--------|------|-------|----------|-------|
| 0 | 2 | agentVersion | Word16 BE | 7 |
| 2 | 1 | type | ASCII | C (0x43) |
| 3 | 1 | maybeE2E | ASCII | 1 (0x31) |
| 4 | 2 | e2eVersion | Word16 BE | 2 |
| 6 | 1 | key1Len | 1 byte | 68 (0x44) |
| 7 | 68 | key1 | X448 SPKI | Our ratchet key |
| 75 | 1 | key2Len | 1 byte | 68 (0x44) |
| 76 | 68 | key2 | X448 SPKI | Our ephemeral key |
| 144 | REST | encConnInfo | Tail | Encrypted info |

Total header: 144 bytes
Padding target: 14832 bytes

---

## EncRatchetMessage

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 1 | emHeaderLen | 1 byte (123) |
| 1 | 123 | emHeader | EncMessageHeader |
| 124 | 16 | emAuthTag | Raw bytes |
| 140 | REST | emBody | Tail |

---

## EncMessageHeader (123 bytes)

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 2 | ehVersion | Word16 BE (2) |
| 2 | 16 | ehIV | Raw bytes |
| 18 | 16 | ehAuthTag | Raw bytes |
| 34 | 1 | ehBodyLen | 1 byte (88) |
| 35 | 88 | ehBody | Encrypted MsgHeader |

Total: 2 + 16 + 16 + 1 + 88 = 123 bytes

---

## MsgHeader (88 bytes)

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 2 | msgMaxVersion | Word16 BE (2) |
| 2 | 1 | dhKeyLen | 1 byte (68) |
| 3 | 68 | msgDHRs | X448 SPKI |
| 71 | 4 | msgPN | Word32 BE |
| 75 | 4 | msgNs | Word32 BE |
| 79 | 9 | padding | Zero bytes |

Total: 2 + 1 + 68 + 4 + 4 + 9 = 88 bytes

---

## SMPQueueInfo

| Field | Encoding |
|-------|----------|
| clientVersion | Word16 BE (8) |
| hostCount | 1 byte (1) |
| host | Length-prefixed string |
| port | Length-prefixed string |
| keyHash | 1-byte length + 32 bytes |
| senderId | 1-byte length + N bytes |
| dhPublicKey | 1-byte length (44) + X25519 SPKI |
| queueMode | Optional |

### queueMode Encoding

| Value | Encoding |
|-------|----------|
| Nothing | Empty (0 bytes!) |
| Just QMMessaging | M (1 byte) |
| Just QMSubscription | S (1 byte) |

---

## SPKI Key Formats

### X448 SPKI (68 bytes)

Header: 30 42 30 05 06 03 2b 65 6f 03 39 00 (12 bytes)
+ Raw key (56 bytes)

### X25519 SPKI (44 bytes)

Header: 30 2a 30 05 06 03 2b 65 6e 03 21 00 (12 bytes)
+ Raw key (32 bytes)

---

## Padding Format

Format: [2 bytes: length Word16 BE][content][# padding]

| Message Type | Padded Size |
|--------------|-------------|
| AgentConfirmation | 14832 bytes |
| HELLO | 15840 bytes |

---

## Common Encoding Mistakes

| Mistake | Incorrect | Correct |
|---------|-----------|---------|
| E2E key length | Word16 BE | 1 byte |
| MsgHeader DH key | Word16 BE | 1 byte |
| ehBody length | Word16 BE | 1 byte |
| emHeader size | 124 bytes | 123 bytes |
| prevMsgHash length | 1 byte | Word16 BE |
| Port encoding | Space char | Length byte |
| smpQueues count | 1 byte | Word16 BE |
| queueMode Nothing | 0 byte | Empty |
| Tail field | With prefix | No prefix |
| rcAD keys | SPKI (68B) | Raw (56B) |
| Payload AAD | 236 bytes | 235 bytes |

---

## References

- SimpleX Protocol: https://github.com/simplex-chat/simplexmq
- Agent Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
