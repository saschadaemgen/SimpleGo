---
title: "Specification Overview"
sidebar_position: 1
---

# SimpleX Protocol Specification

This specification documents the SimpleX Messaging Protocol as implemented by SimpleGo — derived from reverse-engineering the official Haskell codebase (simplexmq) and verified through native C implementation on ESP32 hardware.

## Scope

This is an **implementation specification**, not an abstract protocol description. It provides:

- **Byte-exact wire formats** with struct definitions
- **Cryptographic operations** with concrete algorithm parameters
- **State machines** for connection and queue management
- **Test vectors** for verification

## Specification Status

| Section | Pages | Status |
|---------|-------|--------|
| SMP Protocol | 12 | Mostly complete |
| Cryptography | 5 | Complete (PQ-KEM draft) |
| Agent Protocol | 3 | Complete |
| XFTP Protocol | 3 | Draft |
| Chat Protocol | 5 | Partial |
| NTF Protocol | 1 | Draft |
| XRCP Protocol | 1 | Planned |

## Conventions

- All integers are big-endian unless noted
- All strings are UTF-8 encoded
- Byte sequences shown as hex: `0x01 0x02`
- Lengths in bytes unless noted
- `||` denotes concatenation

## Normative References

- [simplexmq Haskell source](https://github.com/simplex-chat/simplexmq)
- [SimpleX protocol documentation](https://github.com/simplex-chat/simplexmq/blob/stable/protocol)
- [RFC 8446 — TLS 1.3](https://www.rfc-editor.org/rfc/rfc8446)
- [Signal Double Ratchet specification](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH specification](https://signal.org/docs/specifications/x3dh/)
