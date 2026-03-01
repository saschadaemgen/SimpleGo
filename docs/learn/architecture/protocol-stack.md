---
title: "Protocol Stack Overview"
sidebar_position: 1
---

# Protocol Stack Overview

✅ **Status: Complete**

The SimpleX protocol is a layered architecture, similar to the OSI model but designed specifically for asynchronous, metadata-private messaging.

## The Stack

```
┌─────────────────────────────────────┐
│  Chat Protocol (x.msg.*, x.grp.*)  │  Application messages
├─────────────────────────────────────┤
│  Agent Protocol                     │  Connection management
├─────────────────────────────────────┤
│  Double Ratchet (E2E Encryption)    │  Per-message keys
├─────────────────────────────────────┤
│  E2E Encryption (NaCl/AES-GCM)     │  Sender authentication
├─────────────────────────────────────┤
│  SMP Protocol                       │  Queue commands
├─────────────────────────────────────┤
│  Server Encryption (NaCl)           │  Server-recipient encryption
├─────────────────────────────────────┤
│  TLS 1.3                            │  Transport security
├─────────────────────────────────────┤
│  TCP                                │  Network transport
└─────────────────────────────────────┘
```

Each layer is documented in detail in the [Specification](/spec) section.
