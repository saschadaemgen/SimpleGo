# SimpleGo Protocol Documentation

Complete documentation of the SimpleX SMP protocol implementation.

---

## Overview

SimpleGo implements the SimpleX Messaging Protocol (SMP) for secure, decentralized messaging.

---

## Protocol Stack

| Layer | Protocol | Description |
|-------|----------|-------------|
| 5 | Application | User messages, contacts |
| 4 | Agent Protocol | E2E encryption, connections |
| 3 | SMP | Queue-based message delivery |
| 2 | TLS 1.3 | Transport security |
| 1 | TCP | Network transport |

---

## SMP Protocol

### Key Concepts

| Concept | Description |
|---------|-------------|
| Queue | Unidirectional message channel |
| Sender | Party that sends to queue |
| Recipient | Party that receives from queue |
| Relay | Server that hosts queues |

### SMP Commands

| Command | Direction | Description |
|---------|-----------|-------------|
| NEW | Client to Server | Create new queue |
| SUB | Client to Server | Subscribe to queue |
| SEND | Client to Server | Send message |
| ACK | Client to Server | Acknowledge receipt |
| OFF | Client to Server | Suspend queue |
| DEL | Client to Server | Delete queue |

### SMP Responses

| Response | Description |
|----------|-------------|
| OK | Command succeeded |
| ERR | Command failed |
| MSG | Message delivered |
| NMSG | New message notification |

---

## Agent Protocol

### Connection States

| State | Description |
|-------|-------------|
| NEW | Created, not confirmed |
| PENDING | Waiting for confirmation |
| CONFIRMED | Ready for messaging |
| ESTABLISHED | Fully established |
| DELETED | Connection deleted |

### Agent Messages

| Message | Description |
|---------|-------------|
| CONF | Connection confirmation |
| INFO | Connection information |
| HELLO | Initial greeting |
| MSG | User message |
| ACK | Message acknowledgment |

---

## E2E Encryption Protocol

### Key Exchange (X3DH)

| Step | Operation |
|------|-----------|
| 1 | Sender generates ephemeral X448 key |
| 2 | Sender performs 3 DH operations |
| 3 | Sender derives keys via HKDF |
| 4 | Sender sends public keys to recipient |
| 5 | Recipient performs same DH |
| 6 | Both have same shared secrets |

### Double Ratchet

| Ratchet | Trigger | Derives |
|---------|---------|---------|
| Root | DH ratchet step | Root key, chain key, header key |
| Chain | Each message | Message key, chain key, IVs |

### Message Encryption

| Layer | Key | Content |
|-------|-----|---------|
| Header | header_key | MsgHeader |
| Body | message_key | Message content |

---

## Connection Establishment

### Step 1: Create Invitation

Recipient:
1. Generate X448 key pairs
2. Create queue on SMP server
3. Build invitation with server and keys
4. Share invitation

### Step 2: Accept Invitation

Sender:
1. Parse invitation
2. Connect to SMP server
3. Create sender queue
4. Perform X3DH
5. Send AgentConfirmation

### Step 3: Confirm Connection

Recipient:
1. Receive AgentConfirmation
2. Decrypt connection info
3. Perform X3DH
4. Initialize ratchet
5. Connection ready

---

## Message Format

### EncRatchetMessage

| Field | Size | Description |
|-------|------|-------------|
| emHeaderLen | 1 byte | 123 |
| emHeader | 123 bytes | Encrypted header |
| emAuthTag | 16 bytes | Body auth tag |
| emBody | Variable | Encrypted body (Tail) |

### EncMessageHeader (123 bytes)

| Field | Size | Description |
|-------|------|-------------|
| ehVersion | 2 bytes | Version (2) |
| ehIV | 16 bytes | Header IV |
| ehAuthTag | 16 bytes | Header auth tag |
| ehBodyLen | 1 byte | 88 |
| ehBody | 88 bytes | Encrypted MsgHeader |

### MsgHeader (88 bytes)

| Field | Size | Description |
|-------|------|-------------|
| msgMaxVersion | 2 bytes | Version (2) |
| dhKeyLen | 1 byte | 68 |
| msgDHRs | 68 bytes | X448 SPKI key |
| msgPN | 4 bytes | Previous chain count |
| msgNs | 4 bytes | Message number |
| padding | 9 bytes | Zero padding |

---

## Error Handling

### SMP Errors

| Error | Description |
|-------|-------------|
| AUTH | Authentication failed |
| NO_QUEUE | Queue does not exist |
| QUOTA | Queue quota exceeded |
| NO_MSG | No message available |
| LARGE_MSG | Message too large |

### Agent Errors

| Error | Description |
|-------|-------------|
| A_DUPLICATE | Duplicate connection |
| A_PROHIBITED | Operation not allowed |
| A_MESSAGE | Message parsing error |
| A_CRYPTO | Cryptographic error |

---

## Security Properties

### Confidentiality

- AES-256-GCM encryption
- Keys from DH shared secrets
- Forward secrecy via Double Ratchet

### Integrity

- GCM authentication tags
- Message counters prevent replay
- Hash chains link messages

### Authentication

- X3DH mutual authentication
- Ed25519 signatures
- TLS server authentication

### Privacy

- No user identifiers
- Random queue IDs
- Servers cannot read content

---

## References

- SMP Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md
- Agent Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md
- Double Ratchet: https://signal.org/docs/specifications/doubleratchet/

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
