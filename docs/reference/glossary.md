---
title: "Glossary"
sidebar_position: 1
---

# Glossary

## A

**ACK** — Acknowledgment command sent by recipient to confirm message receipt.

**Agent** — The protocol layer that manages duplex connections on top of SMP queues.

**AgentConnInfo** — Connection information exchanged during the agent handshake.

## C

**Contact Queue** — The initial simplex queue created by the acceptor (Bob) for receiving the first message.

**Curve448** — Elliptic curve used for X3DH key agreement in SimpleX (also called Ed448-Goldilocks).

## D

**Double Ratchet** — Algorithm providing forward secrecy and break-in recovery through continuous key rotation.

**Duplex Connection** — A bidirectional connection formed by combining two simplex queues.

## E

**E2E** — End-to-end encryption between sender and recipient.

## H

**Header Key (HK)** — Key used to encrypt Double Ratchet message headers, providing metadata protection.

**HKDF** — HMAC-based Key Derivation Function used for deriving multiple keys from shared secrets.

## K

**KEM** — Key Encapsulation Mechanism, used for post-quantum key agreement (SNTRUP761).

## N

**NaCl** — Networking and Cryptography library, specifically the crypto_box construction (XSalsa20-Poly1305).

## P

**PQ** — Post-Quantum, referring to cryptographic algorithms resistant to quantum computer attacks.

## R

**Reply Queue** — The simplex queue created by the initiator (Alice) in response to the contact queue, enabling bidirectional communication.

**Ratchet** — A cryptographic mechanism that only moves forward, providing forward secrecy.

## S

**SMP** — Simplex Messaging Protocol, the core transport protocol.

**SNTRUP761** — Post-quantum KEM algorithm integrated into SimpleX's key agreement.

## X

**X3DH** — Extended Triple Diffie-Hellman, the initial key agreement protocol.

**XFTP** — SimpleX File Transfer Protocol for large file exchange.
