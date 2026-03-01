# SimpleGo Development Notes

Internal development notes and lessons learned.

---

## Session History

### Session 1-3: Foundation (December 2025)

Goals:
- ESP-IDF project structure
- WiFi connectivity
- Basic TCP sockets

Achievements:
- Project compiles and runs
- WiFi connects successfully
- Network stack operational

---

### Session 4-5: TLS and SMP (January 2026)

Goals:
- TLS 1.3 connection
- Basic SMP handshake

Achievements:
- TLS 1.3 working with mbedTLS
- Connected to SimpleX servers
- SMP handshake implemented

Key Discovery:
- SimpleX uses non-standard length encoding

---

### Session 6: Modular Refactoring (January 21, 2026)

Goals:
- Break monolithic main.c into modules

Achievements:
- Created smp_peer.c, smp_parser.c, smp_network.c
- Created smp_crypto.c, smp_contacts.c, smp_utils.c

---

### Session 7: Double Ratchet (January 22-23, 2026)

Goals:
- X3DH key agreement
- Double Ratchet encryption

Achievements:
- wolfSSL integrated for X448
- X3DH working
- Root KDF and Chain KDF implemented
- Server accepts messages with OK

Major Discoveries:

1. wolfSSL Byte Order:
   - wolfSSL exports X448 keys reversed
   - Must reverse all bytes

2. IV Order in Chain KDF:
   - [64:80] = header_iv
   - [80:96] = message_iv

3. Length Encoding Inconsistency:
   - Some fields use 1-byte
   - Some use Word16 BE
   - Must check each field

---

### Session 8: Bug Fixing (January 24, 2026)

Goals:
- Fix remaining encoding bugs
- Achieve app compatibility

Achievements:
- 12 encoding bugs fixed
- All crypto verified against Python

Current Status:
- Server accepts all messages
- App shows A_MESSAGE error
- Investigating Tail encoding

---

## Key Technical Discoveries

### Discovery #1: Encoding is Context-Dependent

| Field Type | Encoding |
|------------|----------|
| Short ByteString | 1-byte length |
| Long ByteString | 0xFF + Word16 BE |
| Last field | Tail (no prefix) |
| Numeric | Word16 or Word32 BE |

---

### Discovery #2: SPKI Key Format

X448 SPKI (68 bytes):
- Header: 30 42 30 05 06 03 2b 65 6f 03 39 00
- Key: 56 bytes raw

X25519 SPKI (44 bytes):
- Header: 30 2a 30 05 06 03 2b 65 6e 03 21 00
- Key: 32 bytes raw

For AAD: Use RAW keys, not SPKI.

---

### Discovery #3: wolfSSL Byte Order

wolfSSL: Little-endian
SimpleX: Big-endian

Solution: Reverse all key bytes.

---

### Discovery #4: KDF Info Strings

| KDF | Info String | Length |
|-----|-------------|--------|
| X3DH | SimpleXX3DH | 11 bytes |
| Root | SimpleXRootRatchet | 19 bytes |
| Chain | SimpleXChainRatchet | 20 bytes |

---

### Discovery #5: Padding Sizes

| Message | Padded Size |
|---------|-------------|
| AgentConfirmation | 14832 bytes |
| HELLO | 15840 bytes |

---

### Discovery #6: queueMode Encoding

| Value | Encoding |
|-------|----------|
| Nothing | Empty (0 bytes!) |
| Just QMMessaging | M (1 byte) |

---

## TODO List

### High Priority

- Fix Tail encoding for encConnInfo
- Fix Tail encoding for emBody
- Verify complete message flow
- Implement message receiving

### Medium Priority

- Connection retry logic
- Queue management
- Contact storage to NVS
- Message acknowledgment

### Low Priority

- T-Deck display support
- Keyboard input
- Group messaging
- File transfer

---

## References

### Protocol Documentation

- SMP: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md
- Agent: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md

### Haskell Source (Key Files)

- Encoding: src/Simplex/Messaging/Encoding.hs
- Ratchet: src/Simplex/Messaging/Crypto/Ratchet.hs
- Agent: src/Simplex/Messaging/Agent/Protocol.hs

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
