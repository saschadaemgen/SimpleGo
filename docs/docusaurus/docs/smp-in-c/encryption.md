---
title: Encryption
sidebar_position: 6
---

# Encryption

SimpleGo implements four cryptographically independent encryption layers. Each layer serves a distinct defensive purpose and operates at a different protocol level.

## Layer 1: Double Ratchet (E2E)

End-to-end encryption between the two communicating parties. Uses the Double Ratchet Algorithm with X3DH initial key agreement.

:::danger X448 not X25519
The Double Ratchet uses **X448** for the Diffie-Hellman ratchet -- not X25519. This is a common source of implementation errors. Using X25519 produces key agreement that appears to work locally but fails to decrypt messages from the Haskell reference implementation.
:::

**Post-quantum extension:** Since SMP v5.6 the Double Ratchet is augmented with hybrid post-quantum key exchange using CRYSTALS-Kyber and Streamlined NTRU Prime. Both classical and post-quantum keys must be handled.

## Layer 2: Per-Queue NaCl Cryptobox (Sender to Destination)

Each message queue has its own independent NaCl cryptobox layer (X25519 + XSalsa20 + Poly1305). Unique to each queue. Protects the sender-to-destination-router leg of the two-router delivery path. Prevents traffic correlation between queues even if TLS is compromised.

## Layer 3: Server-to-Recipient NaCl (Destination to Recipient)

An additional NaCl encryption layer applied when delivering messages to the recipient. Protects the destination-router-to-recipient leg. Prevents correlation between incoming and outgoing server traffic even if TLS is compromised.

## Layer 4: TLS 1.3

The outermost transport layer. Protects all SMP traffic from passive observation and active manipulation at the network level.

## cbNonce Construction

The `cbNonce` for per-queue NaCl encryption is constructed from the message ID. Reference in Haskell source: `Crypto.hs:1396-1402`.
```c
// msgId -> 24-byte nonce
// Bytes 0-7:  message ID encoded as 8 bytes big-endian
// Bytes 8-23: zero-padded
uint8_t nonce[24];
memset(nonce, 0, 24);
// encode msgId big-endian into nonce[0..7]
```

Getting this wrong produces silent decryption failure -- the crypto operation succeeds but returns garbage plaintext. No error is reported.

## e2eDhSecret

The E2E Diffie-Hellman shared secret is computed differently on sender and receiver sides.

| Side | File | Lines |
|------|------|-------|
| Receiver | `Agent.hs` | 3073, 3324, 3338 |
| Sender | `Agent.hs` | 3762 |

## HKDF Key Derivation

HKDF-SHA256 is used for key derivation throughout -- Double Ratchet chain keys, per-queue keys, and SD card AES-256-GCM per-contact keys (derived from a master key using the contact queue ID as context).

## Crypto Libraries

All NaCl operations use libsodium. All TLS and symmetric crypto uses mbedTLS 4.x.

:::danger Hardware AES Disabled
`CONFIG_MBEDTLS_HARDWARE_AES=n` must be set. The ESP32-S3 hardware AES DMA conflicts with PSRAM at the silicon level. All AES operations use software implementation.
:::
