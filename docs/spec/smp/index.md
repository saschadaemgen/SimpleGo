---
title: "SMP Protocol Overview"
sidebar_position: 1
---

# SMP — Simplex Messaging Protocol

✅ **Status: Complete**

The Simplex Messaging Protocol (SMP) is the core transport protocol of the SimpleX network. It manages unidirectional message queues on relay servers.

## Version History

| Version | Key Changes |
|---------|-------------|
| v1-v3 | Legacy, no longer supported |
| v4 | Notification support |
| v5 | Queue rotation |
| v6 | Delivery receipts |
| v7 | Sender key securing (senderCanSecure) |
| v8 | Private routing |
| v9 | Latest stable |

## Protocol Overview

SMP operates on a simple client-server model where:

1. **Recipient** creates a queue on a server (NEW → IDS)
2. **Recipient** shares the queue address with sender (out-of-band)
3. **Sender** secures the queue (KEY/SKEY)
4. **Sender** sends messages (SEND → MSG)
5. **Recipient** subscribes and receives (SUB → MSG → ACK)

All communication is over TLS 1.3, with additional NaCl encryption layers.
