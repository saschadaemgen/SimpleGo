![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 3: Sessions 5-6
# wolfSSL X448 Fix, Cryptography Verification, Protocol Compatibility

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-24
**Status:** COMPLETED -- All cryptography verified, encoding complete
**Previous:** Part 2 - Sessions 3-4
**Next:** Part 4 - Session 7
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 5-6 SUMMARY

```
Session 5 implemented the wolfSSL X448 byte reversal fix and achieved
100% cryptographic verification against Python reference implementation.
Every derived key, IV, and DH output now matches byte-for-byte. Session 6
analyzed the contact address handshake flow, verified wire formats against
Haskell source, and fixed the final encoding bugs (port encoding,
smpQueues list count, queueMode Nothing semantics). After Session 6, all
known encoding and cryptography errors are resolved, yet A_MESSAGE
persists, pointing to a remaining format issue.

 3 Bugs Fixed (S5-S6)
 1 Critical crypto fix (wolfSSL byte reversal)
 All cryptography verified (X448, X3DH, HKDF, Root KDF, Chain KDF)
 Wire format confirmed against Haskell source
```

---

## Session 5: wolfSSL X448 Byte Reversal Fix

### The Problem

wolfSSL's X448 implementation (compiled with EC448_BIG_ENDIAN) outputs all keys and DH shared secrets in reversed byte order compared to cryptonite (Haskell) and Python cryptography.

```
Standard (Python/Haskell): 01 02 03 04 05 ... 54 55 56
wolfSSL output:            56 55 54 ... 05 04 03 02 01
```

This affects key generation, key import, and DH computation. The result is that HKDF produces completely different derived keys, causing AES-GCM decryption to fail on the app side.

### The Fix

```c
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}
```

Applied in three locations:

**x448_generate_keypair():** Reverse public and private keys after wolfSSL export.

```c
wc_curve448_export_public(&key, pub_tmp, &pub_len);
wc_curve448_export_private_raw(&key, priv_tmp, &priv_len);
reverse_bytes(pub_tmp, keypair->public_key, 56);
reverse_bytes(priv_tmp, keypair->private_key, 56);
```

**x448_dh():** Reverse inputs before wolfSSL import, reverse output after computation.

```c
reverse_bytes(their_public, their_public_rev, 56);
reverse_bytes(my_private, my_private_rev, 56);
wc_curve448_import_public(their_public_rev, 56, &their_key);
wc_curve448_import_private_raw(my_private_rev, 56, &my_key);
wc_curve448_shared_secret(&my_key, &their_key, secret_rev, &secret_len);
reverse_bytes(secret_rev, shared_secret, 56);
```

### Cryptographic Verification

After the fix, byte-for-byte comparison with Python cryptography:

```
X3DH DH Outputs:
  dh1: 62413115799d7f0a...  Python: 62413115799d7f0a...  MATCH
  dh2: 27d885f054cc7775...  Python: 27d885f054cc7775...  MATCH
  dh3: 8dd161101f1c730f...  Python: 8dd161101f1c730f...  MATCH

X3DH HKDF Output (96 bytes):
  hk:  c65dc5381323839f...  Python: c65dc5381323839f...  MATCH
  rk:  8b30f093a3b5d75b...  Python: 8b30f093a3b5d75b...  MATCH

Root KDF Output (96 bytes):
  new_rk:  de394bc567ae2e70...  Python: de394bc567ae2e70...  MATCH
  ck:      5d473bb5b24acc9d...  Python: 5d473bb5b24acc9d...  MATCH
  next_hk: d3d8fbb361ea2e65...  Python: d3d8fbb361ea2e65...  MATCH

Chain KDF Output (96 bytes):
  mk:        7041ce31dc681820...  Python: 7041ce31dc681820...  MATCH
  header_iv: 708dee3b187dd7ec...  Python: 708dee3b187dd7ec...  MATCH
  msg_iv:    e3b28a0d3df93e3c...  Python: e3b28a0d3df93e3c...  MATCH
```

All cryptography verified correct: X448 DH, X3DH key agreement, HKDF-SHA512 for X3DH/Root/Chain KDF, all derived keys and IVs.

---

## Session 6: Handshake Flow and Protocol Bugs

### Contact Address Handshake Flow (q=c)

```
ESP32 (Contact)         SMP Server              SimpleX App
     |                       |                       |
     | 1. QR-Code/Link       |                       |
     |<----------------------|                       |
     |                       | 2. App scans QR       |
     |                       |<----------------------|
     | 3. agentInvitation    |                       |
     |<----------------------|   (E2E keys + KEM)    |
     | 4. agentConfirmation  |                       |
     |---------------------->|                       |
     | 5. HELLO (E2E)        |                       |
     |---------------------->|                       |
     |                       | 6. App receives       |
     |                       |    Cannot decrypt     |
```

With Contact Address (q=c), the app must successfully decrypt the AgentConfirmation before the connection becomes active. Decryption failure keeps status at "waiting for acceptance".

### Bug: SMPQueueInfo Port Encoding

Port was encoded with space separator (0x20) from an earlier Session 2 fix, but the correct encoding for SMPServer (v2+) uses a length prefix for port.

```c
// Before: buf[p++] = ' ';  // Space (0x20)
// After:  buf[p++] = (uint8_t)port_len;  // Length prefix
```

Note: This reversed the Session 2 fix. The port encoding depends on SMPQueueInfo version. For v2+ standard encoding (smpEncode tuple), port is a ByteString with 1-byte length prefix, not space-separated.

### Bug: smpQueues List Count

Queue list count in AgentConnInfoReply was 1 byte instead of Word16 BE.

```c
// Before: agent_conn_info[aci_len++] = 0x01;  // 1 byte
// After:
agent_conn_info[aci_len++] = 0x00;  // High byte
agent_conn_info[aci_len++] = 0x01;  // Low byte
```

### Bug: queueMode Nothing Semantics

For queueMode, `maybe "" smpEncode queueMode` was confused with standard Maybe encoding. Standard Maybe uses '0' for Nothing, but queueMode uses `maybe ""` which outputs empty string for Nothing.

```haskell
-- Standard Maybe:  Nothing = '0' (0x30)
-- queueMode:       maybe "" smpEncode queueMode
--                  Nothing = "" (empty, no bytes!)
--                  Just QMSubscription = "0"
```

Fix: Remove the '0' byte that was incorrectly appended for queueMode Nothing.

### Version-Dependent Encoding

```haskell
pqRatchetE2EEncryptVersion = VersionE2E 3
```

| Feature | Version 2 (SimpleGo) | Version 3+ (PQ) |
|---------|---------------------|------------------|
| E2E Params | (v, k1, k2) | (v, k1, k2, kem_) |
| MsgHeader | (ver, DHRs, PN, Ns) | (ver, DHRs, KEM, PN, Ns) |
| encodeLarge | Standard ByteString | Large wrapper |

Version 2 is correct for non-PQ communication. No KEM key required.

### Wire Format Verification (Haskell Source)

EncRatchetMessage encoding confirmed:

| Field | Encoding | Verified |
|-------|----------|----------|
| emHeader | Word16 BE length + data | Yes |
| emAuthTag | 16 bytes raw (no prefix) | Yes |
| emBody | Tail = no length prefix | Yes |

EncMessageHeader encoding confirmed:

| Field | Encoding | Verified |
|-------|----------|----------|
| ehVersion | Word16 BE | Yes |
| ehIV | 16 bytes raw | Yes |
| ehAuthTag | 16 bytes raw | Yes |
| ehBody | Word16 BE length + data | Yes |

---

## Status After Session 6

All known encoding and cryptography errors resolved. Server accepts both AgentConfirmation and HELLO with "OK". Wire formats verified against Haskell source. Complete cryptographic pipeline verified against Python. A_MESSAGE persists, indicating a remaining format issue not yet identified.

Verified correct: TLS 1.3 connection, SMP handshake, queue creation, invitation parsing, X3DH key agreement, double ratchet initialization, ratchet encryption with padding, all length prefixes, wire format structure, AES-GCM encryption, all KDF functions.

---

## Consolidated Bug List (Sessions 5-6)

| # | Bug | Session | Root Cause | Fix |
|---|-----|---------|------------|-----|
| 31 | wolfSSL X448 byte order | S5 | EC448_BIG_ENDIAN reverses all keys | reverse_bytes() on import/export/DH |
| 32 | SMPQueueInfo port encoding | S6 | Space instead of length prefix | 1-byte length prefix |
| 33 | smpQueues list count 1B | S6 | Should be Word16 BE | 00 01 |
| 34 | queueMode Nothing sends '0' | S6 | maybe "" vs standard Maybe | Send nothing for Nothing |

**Result after Sessions 5-6:** All cryptography and encoding verified. A_MESSAGE persists as parsing issue.

---

## Lessons Learned (Sessions 1-6)

1. Crypto libraries are not interchangeable: wolfSSL X448 byte order differs from cryptonite/Python despite implementing the same RFC 7748.
2. Python comparison tests are essential: without byte-by-byte comparison, the wolfSSL bug would not have been found.
3. Haskell uses Word16 BE for all ByteString lengths (the most common bug class).
4. Tail types have no length prefix (parser consumes remaining bytes).
5. Maybe Nothing encoding is context-dependent: standard Maybe uses '0', but maybe "" uses empty string.
6. Test all byte order combinations systematically (original, reversed, mixed).
7. Server acceptance ("OK") does not mean the app can parse/decrypt the message.

---

*Part 3 - Sessions 5-6: wolfSSL X448 Fix, Cryptography Verification, Protocol Compatibility*
*SimpleGo Protocol Analysis*
*Original dates: January 24, 2026*
*Rewritten: March 4, 2026 (v2)*
*4 bugs fixed, all cryptography verified, wire format confirmed*
