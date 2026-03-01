---
title: "Security Overview"
sidebar_position: 1
---

# Security

SimpleGo inherits SimpleX's multi-layered security model and adds hardware-level security considerations for embedded deployment.

## Security Architecture

| Layer | Protection |
|-------|-----------|
| **Transport** | TLS 1.3 with server key hash pinning |
| **Server Encryption** | NaCl crypto_box — server cannot read content |
| **E2E Encryption** | Sender-authenticated encryption per queue |
| **Message Encryption** | Double Ratchet with forward secrecy and break-in recovery |
| **Post-Quantum** | SNTRUP761 KEM for quantum resistance |
| **Hardware** | Dedicated device — no app store, no cellular, no OS telemetry |

## Documentation

| Document | Description |
|----------|-------------|
| [Security Model](/SECURITY_MODEL) | Software security model and design principles |
| [Threat Model](/security/threat-model) | What SimpleGo protects against |
| [Encryption Deep Dive](/security/encryption-deep-dive) | All 4+ encryption layers explained |
| [Hardware Security](/security/hardware-security) | ESP32-specific security considerations |
| [Audit Log](/security/audit-log) | Security reviews, findings, and fixes |
