# SimpleGo Cryptography Documentation

Complete cryptographic specification for the SimpleGo Double Ratchet implementation.

---

## Overview

SimpleGo implements end-to-end encryption using the Double Ratchet algorithm with X3DH key agreement.

### Cryptographic Components

| Component | Algorithm | Library | Purpose |
|-----------|-----------|---------|---------|
| Key Agreement | X3DH | Custom | Initial shared secret |
| DH Ratchet | X448 (Curve448) | wolfSSL | Forward secrecy |
| Key Derivation | HKDF-SHA512 | mbedTLS | Key expansion |
| Symmetric Encryption | AES-256-GCM | mbedTLS | Message encryption |
| Per-Queue Encryption | X25519 | libsodium | Queue-level encryption |
| Signatures | Ed25519 | libsodium | Authentication |

---

## X3DH Key Agreement

Extended Triple Diffie-Hellman establishes the initial shared secret.

### Keys Involved

| Key | Name | Owner | Type | Size |
|-----|------|-------|------|------|
| spk1 | Semi-permanent Key | Peer | X448 Public | 56 bytes |
| rk1 | Ratchet Key | Peer | X448 Public | 56 bytes |
| sk1 | Ephemeral Secret | Us | X448 Private | 56 bytes |
| pk1 | Ephemeral Public | Us | X448 Public | 56 bytes |
| rpk1 | Our Ratchet Key | Us | X448 Key Pair | 56+56 bytes |

### DH Calculations

The sender performs three Diffie-Hellman operations:

- dh1 = X448_DH(sk1, spk1) - Our ephemeral x Peer semi-permanent
- dh2 = X448_DH(sk1, rk1) - Our ephemeral x Peer ratchet
- dh3 = X448_DH(rpk1_priv, spk1) - Our ratchet x Peer semi-permanent
- ikm = dh1 || dh2 || dh3 (168 bytes total)

### HKDF Derivation

Input:
- salt = 64 zero bytes
- ikm = dh1 || dh2 || dh3 (168 bytes)
- info = SimpleXX3DH (11 bytes)

Output: 96 bytes
- [0:32] = header_key
- [32:64] = next_header_key
- [64:96] = root_key

---

## Double Ratchet

The Double Ratchet provides forward secrecy through continuous key evolution.

### Root KDF

Derives new keys when the DH ratchet advances.

Input:
- salt = root_key (32 bytes)
- ikm = dh_output (56 bytes)
- info = SimpleXRootRatchet (19 bytes)

Output: 96 bytes
- [0:32] = new_root_key
- [32:64] = chain_key
- [64:96] = next_header_key

### Chain KDF

Derives per-message keys and IVs.

Input:
- salt = empty (0 bytes)
- ikm = chain_key (32 bytes)
- info = SimpleXChainRatchet (20 bytes)

Output: 96 bytes
- [0:32] = message_key
- [32:64] = new_chain_key
- [64:80] = header_iv (FIRST!)
- [80:96] = message_iv (SECOND!)

Important: The IV order is critical. header_iv comes before message_iv.

---

## KDF Parameters Summary

| KDF | Salt | IKM | Info | Output |
|-----|------|-----|------|--------|
| X3DH | 64 x 0x00 | dh1+dh2+dh3 (168B) | SimpleXX3DH (11B) | 96 bytes |
| Root | root_key (32B) | dh_output (56B) | SimpleXRootRatchet (19B) | 96 bytes |
| Chain | empty (0B) | chain_key (32B) | SimpleXChainRatchet (20B) | 96 bytes |

---

## AES-256-GCM Encryption

SimpleX uses AES-256-GCM with 16-byte IVs.

### Parameters

| Parameter | Size |
|-----------|------|
| Key | 32 bytes |
| IV | 16 bytes |
| Auth Tag | 16 bytes |
| AAD | Variable |

### Header AAD (rcAD) - 112 bytes

rcAD = our_ratchet_key_raw (56 bytes) || peer_ratchet_key_raw (56 bytes)

Important: Use RAW keys (56 bytes each), not SPKI-encoded keys (68 bytes).

### Payload AAD - 235 bytes

payload_aad = rcAD (112 bytes) || emHeader (123 bytes)

Important: Size is 235 bytes (112 + 123), not 236.

---

## SPKI Key Format

### X448 SPKI (68 bytes)

Header (12 bytes): 30 42 30 05 06 03 2b 65 6f 03 39 00
+ Raw key (56 bytes)

### X25519 SPKI (44 bytes)

Header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
+ Raw key (32 bytes)

---

## wolfSSL Byte-Order Issue

wolfSSL exports X448 keys in reversed byte order.

Problem:
- wolfSSL output: [byte_55][byte_54]...[byte_0]
- SimpleX expects: [byte_0][byte_1]...[byte_55]

Solution: Reverse all bytes after key generation and DH operations.

---

## Verification Results

All cryptographic operations verified against Python reference implementations.

| Component | Status |
|-----------|--------|
| X448 Key Generation | Verified |
| X448 Diffie-Hellman | Verified |
| HKDF-SHA512 (X3DH) | Verified |
| HKDF-SHA512 (Root KDF) | Verified |
| HKDF-SHA512 (Chain KDF) | Verified |
| AES-256-GCM | Verified |
| Wire Format | Verified |

---

## Common Issues and Solutions

### Issue #1: wolfSSL Byte Order

Symptom: DH shared secret does not match
Solution: Reverse all key bytes

### Issue #2: IV Order in Chain KDF

Symptom: Decryption fails
Solution: header_iv is bytes [64:80], message_iv is bytes [80:96]

### Issue #3: SPKI vs Raw Keys in AAD

Symptom: Auth tag mismatch
Solution: Use raw keys (56 bytes) for AAD, not SPKI (68 bytes)

### Issue #4: Wrong IV Size

Symptom: Encryption produces garbage
Solution: SimpleX uses 16-byte IVs, not 12-byte

### Issue #5: Payload AAD Size

Symptom: Auth tag verification fails
Solution: payload_aad = 112 + 123 = 235 bytes

---

## References

- Signal Protocol: https://signal.org/docs/
- SimpleX Protocol: https://github.com/simplex-chat/simplexmq
- Double Ratchet: https://signal.org/docs/specifications/doubleratchet/
- X3DH: https://signal.org/docs/specifications/x3dh/

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
