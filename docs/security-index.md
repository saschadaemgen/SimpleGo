---
slug: /security
sidebar_position: 6
title: Security
---

# Security

SimpleGo's security architecture is built on the principle that every layer must be independently defensible. Compromising one layer must not weaken any other.

## Four Encryption Layers Per Message

| Layer | Algorithm | Protects Against |
|-------|-----------|-----------------|
| 1. Double Ratchet (E2E) | X3DH (X448) + AES-256-GCM | End-to-end interception. PFS + post-compromise security. |
| 2. Per-Queue NaCl | X25519 + XSalsa20 + Poly1305 | Traffic correlation between queues |
| 3. Server-to-Recipient NaCl | NaCl cryptobox | Correlation of server I/O frames |
| 4. TLS 1.3 | mbedTLS, ALPN `smp/1` | Network-level attackers |

Content padding to 16 KB fixed blocks at every layer. A network attacker sees only equal-sized packets.

## No Persistent Identity

No user IDs, phone numbers, or usernames. Communication uses ephemeral unidirectional queues. No party -- including the server -- can correlate senders and recipients.

## No Baseband Processor

No cellular modem with DMA access running proprietary firmware. This eliminates the entire class of baseband vulnerabilities documented in academic research (BASECOMP, BaseMirror, Eucleak).

## Security Documentation

- [Full Security Model](/SECURITY_MODEL) -- comprehensive threat model, attacker profiles, and defense analysis

## Known Vulnerabilities

SimpleGo maintains an honest, public inventory of all known security gaps. No finding is downplayed or hidden. See the [full Security Model](/SECURITY_MODEL) for the complete list including SEC-01 through SEC-06.

## Coming Soon

Threat model breakdown, hardware security architecture, and audit log documentation are in preparation.
