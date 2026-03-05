---
title: Connection Handshake
sidebar_position: 5
---

# Connection Handshake

Establishing a SimpleX contact requires an out-of-band invitation exchange followed by a multi-step cryptographic handshake. This is the most complex part of the protocol to implement correctly.

## Overview

Two parties: **Alice** (initiator) and **Bob** (responder).

1. Alice creates a Receive Queue on her server
2. Alice generates an invitation link containing the queue parameters
3. Alice sends the invitation to Bob out-of-band (QR code, link, etc.)
4. Bob creates a Reply Queue on his server
5. Bob sends `PHConfirmation` to Alice Receive Queue
6. Alice extracts Bob Reply Queue parameters from `PHConfirmation`
7. Alice sends `HELLO` to Bob Reply Queue
8. Both sides now have established Double Ratchet state

## PHConfirmation

`PHConfirmation` is the first message Bob sends to Alice. It contains Bob Reply Queue connection parameters and Bob E2E public key for the Double Ratchet.

:::warning Key Location
The E2E key for the Reply Queue is in the `PHConfirmation` message itself. It is not transmitted separately. If you are looking for it elsewhere, that is the bug.

Confirmed directly by Evgeny Poberezkin: *"I think the key would be in PHConfirmation, no?"*
:::

## Reply Queue E2E Decryption

After receiving `PHConfirmation`, Alice decrypts it using the per-queue NaCl layer. The `cmNonce` for this decryption is constructed from the message ID. See [Encryption -> cbNonce Construction](./encryption#cbnonce-construction) for the exact byte layout.

If decryption fails: check the full error in the `A_CRYPTO` constructor -- it contains the actual error, not just the error type.

## HELLO Exchange

After Alice extracts Bob Reply Queue from `PHConfirmation`, she sends `HELLO` to Bob Reply Queue. This completes the handshake and establishes the Double Ratchet for the ongoing conversation.

Both sides store the established ratchet state persistently. See [Double Ratchet](./ratchet) for state persistence details.

## Debugging the Handshake

The handshake involves all four encryption layers simultaneously. When something fails:

1. Check TLS connectivity -- is the server reachable?
2. Check the SMP command/response -- did `NEW` return `OK`?
3. Check the per-queue NaCl layer -- is the queue E2E key correct?
4. Check the Double Ratchet layer -- is the initial key agreement correct?

Failures at layers 2 and 3 are completely silent from the server perspective. The server sees only encrypted blobs. Wrong decryption produces garbage, not an error.
