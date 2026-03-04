![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 4: Session 7
# AES-GCM Verification, Tail Encoding Discovery, Encoding Refinement

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-24
**Status:** COMPLETED -- All crypto verified, Tail encoding discovered
**Previous:** Part 3 - Sessions 5-6
**Next:** Part 5 - Sessions 8-10
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 7 SUMMARY

```
Session 7 verified AES-GCM encryption with 16-byte IVs (non-standard
GCM IV length requiring GHASH transformation), confirmed mbedTLS
produces identical output to Python/OpenSSL. Deep research established
that SimpleGo is the first native (non-Haskell) SMP protocol
implementation worldwide. The Tail encoding pattern was discovered,
explaining that certain fields (encConnInfo, emBody) must have no
length prefix. The largeP parser's tolerance for both 1-byte and
2-byte length prefixes was documented. A_MESSAGE was confirmed as a
parsing error (not crypto), narrowing the remaining bug to format.

 0 New bugs fixed (verification and analysis session)
 AES-GCM with 16-byte IV verified (mbedTLS == Python/OpenSSL)
 First native SMP implementation confirmed
 Tail encoding pattern documented
 1-byte vs 2-byte length prefix tolerance discovered
```

---

## AES-GCM with 16-Byte IV Verification

SimpleX uses 16-byte IVs derived from chainKdf, which is non-standard for GCM (standard is 12 bytes). Non-12-byte IVs require a GHASH transformation before use as the counter block.

### cryptonite GHASH Implementation

```c
// From cryptonite_aes.c:
void cryptonite_aes_gcm_init(aes_gcm *gcm, aes_key *key, uint8_t *iv, uint32_t len)
{
    if (len == 12) {
        // 12-byte IV: use directly + 0x01 counter
        block128_copy_bytes(&gcm->iv, iv, 12);
        gcm->iv.b[15] = 0x01;
    } else {
        // 16-byte IV: GHASH transformation
        for (; len >= 16; len -= 16, iv += 16) {
            block128_xor(&gcm->iv, (block128 *) iv);
            cryptonite_gf_mul(&gcm->iv, gcm->htable);
        }
        // ... length encoding and final GHASH ...
        cryptonite_gf_mul(&gcm->iv, gcm->htable);
    }
}
```

### Verification Result

Test data from ESP32 (real session values) compared with Python AES-GCM (OpenSSL backend):

```
ESP32 ciphertext:  6754c746fd4f6ab97a6d5dda619968df...
Python ciphertext: 6754c746fd4f6ab97a6d5dda619968df...
CIPHERTEXT MATCH

ESP32 tag:  7cedadbf54e873107ba6fc3c822272f4
Python tag: 7cedadbf54e873107ba6fc3c822272f4
AUTHTAG MATCH
```

Conclusion: mbedTLS and cryptonite/OpenSSL produce identical output for AES-GCM with 16-byte IVs. The GHASH transformation is implemented identically. AES-GCM is definitively not the cause of A_MESSAGE.

---

## A_MESSAGE vs A_CRYPTO Distinction

```haskell
data AgentErrorType
  = A_MESSAGE      -- Parsing error (format wrong)
  | A_CRYPTO       -- Crypto error (decryption failed)
  | A_VERSION      -- Version incompatible
```

The error is A_MESSAGE (parsing failure), not A_CRYPTO (decryption failure). This means the crypto_box layer successfully decrypts the ClientMessage, but the parser fails on the content inside. The problem is format/encoding, not cryptography.

---

## SimpleGo: First Native SMP Implementation

Deep research confirmed that SimpleGo is the first third-party implementation that speaks the native binary SMP protocol. All other non-Haskell "implementations" use WebSocket wrappers or FFI bindings to the Haskell library.

| Implementation | Type | Native Binary SMP |
|----------------|------|-------------------|
| SimpleX Apps (Haskell) | Official | Yes |
| SimpleGo (ESP32) | Native C | Yes |
| libsimplex (various) | WebSocket wrapper | No |
| Other SDKs | FFI binding | Indirect |

---

## Tail Encoding Discovery

### The Pattern

```haskell
instance StrEncoding AgentConfirmation where
  strEncode AgentConfirmation {..} =
    smpEncode (version, 'C', Just '1', e2e, Tail encConnInfo)
--                                          ^^^^
--                                          Tail = NO LENGTH PREFIX
```

Tail means the parser consumes all remaining bytes. Adding a length prefix before a Tail field causes the parser to interpret the prefix bytes as data, leading to A_MESSAGE.

### AgentConfirmation Layout (corrected)

```
Offset  Bytes  Description
0-1     2      agentVersion (Word16 BE)
2       1      'C' (type tag)
3       1      '1' (Maybe Just = e2e present)
4-5     2      e2eVersion (Word16 BE)
6       1      key1Len (1 byte = 68)
7-74    68     key1 (SPKI X448)
75      1      key2Len (1 byte = 68)
76-143  68     key2 (SPKI X448)
144+    REST   encConnInfo (TAIL, no length prefix)
```

### EncRatchetMessage Layout (corrected)

```
Offset  Bytes  Description
0       1      emHeaderLen (1 byte = 124)
1-124   124    emHeader (EncMessageHeader)
125-140 16     emAuthTag (raw, no length prefix)
141+    REST   emBody (TAIL, no length prefix)
```

---

## Flexible Length Encoding (0xFF Flag)

SimpleX uses a flexible length encoding scheme:

| Length Range | Encoding | Example |
|-------------|----------|---------|
| 0-254 | 1 byte | Length 68 = 0x44 |
| 255+ | 0xFF + Word16 BE | Length 300 = FF 01 2C |
| Tail | No prefix | Parser consumes rest |

The largeP parser is tolerant of both formats:

```haskell
largeP :: Parser ByteString
largeP = do
  len1 <- peekWord8'
  if len1 < 32
    then unLarge <$> smpP   -- First byte < 32: treat as 2-byte (Large)
    else smpP               -- First byte >= 32: treat as 1-byte
```

This means values like emHeader length (124 = 0x7C, which is >= 32) are correctly parsed with 1-byte prefix, while values that start with a byte < 32 (like 0x00 in Word16 BE) are parsed as 2-byte. Both formats work for the same field.

### Relevance

| Field | Length | Encoding |
|-------|--------|----------|
| SPKI key | 68 | 1 byte (0x44) |
| MsgHeader | 88 | 1 byte (0x58) |
| emHeader | 124 | 1 byte (0x7C) |
| encConnInfo | ~15000 | Tail (no prefix) |

---

## 1-Byte vs 2-Byte Length Hypothesis

Session 7 tested whether reverting Word16 BE lengths to 1-byte lengths would fix A_MESSAGE. Result: no improvement. The largeP parser accepts both formats, confirming that the Word16 BE encoding used in earlier sessions was correct.

```c
// Confirmed correct (kept):
em_header[hp++] = 0x00;
em_header[hp++] = 0x58;  // ehBody-len (Word16 BE)

output[p++] = 0x00;
output[p++] = 0x7C;  // emHeader-len (Word16 BE)
```

---

## Exclusion List (after Session 7)

All of the following have been definitively excluded as causes of A_MESSAGE:

| Hypothesis | Evidence |
|------------|----------|
| AES-GCM encryption wrong | Python byte-for-byte match |
| 16-byte IV handling wrong | Python match, GHASH identical |
| X448 DH wrong | Python match (after byte reversal) |
| X3DH HKDF wrong | Python match |
| Root KDF wrong | Python match |
| Chain KDF wrong | Python match |
| 1-byte vs 2-byte lengths | largeP parser is tolerant |
| All 12 encoding bugs | Fixed, server accepts |
| Wire format structure | Verified against Haskell source |

Remaining hypothesis: X3DH parameter asymmetry (sender vs receiver key assignment) or Tail field handling.

---

## X3DH Parameter Asymmetry Hypothesis

The app initializes as receiver with `initRcvRatchet`:

```haskell
initRcvRatchet rks RcvE2ERatchetParams {..} = Ratchet {..}
  where
    rcNHKr = sndHK  -- Header Key for RECEIVING
```

For correct decryption, the sender's header key (hk from pqX3dhSnd) must equal the receiver's expected header key (sndHK from pqX3dhRcv). This requires the DH calculations to be symmetric:

```
Sender (pqX3dhSnd):
  DH1 = dh'(rk1, spk2)    DH2 = dh'(rk2, spk1)    DH3 = dh'(rk2, spk2)

Receiver (pqX3dhRcv):
  DH1 = dh'(sk2, rpk1)    DH2 = dh'(sk1, rpk2)    DH3 = dh'(sk2, rpk2)
```

Where sk1/sk2 are the sender's keys and rpk1/rpk2 are the receiver's keys. The DH outputs must be identical for the HKDF to produce matching header keys.

---

## Debug Methodology

### Python Comparison Test Template

```python
from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey

def test_all_combinations(our_priv, our_pub, peer_pub, wolfssl_dh_output):
    combinations = [
        ("orig pub, orig priv", peer_pub, our_priv),
        ("rev pub, orig priv", reverse(peer_pub), our_priv),
        ("orig pub, rev priv", peer_pub, reverse(our_priv)),
        ("rev pub, rev priv", reverse(peer_pub), reverse(our_priv)),
    ]
    for name, pub, priv in combinations:
        priv_key = X448PrivateKey.from_private_bytes(priv)
        pub_key = X448PublicKey.from_public_bytes(pub)
        shared = priv_key.exchange(pub_key)
        if reverse(shared) == wolfssl_dh_output:
            print(f"{name}: MATCH!")
```

### Key Debug Insights

1. Auth-Tag success does not mean parsing success: decryption can work but parser still fails on the decrypted content.
2. Error localization matters: A_MESSAGE comes from the parser layer, not the crypto layer.
3. Server acceptance ("OK") only means the message was received, not that the content is valid.
4. Always compare crypto outputs byte-by-byte with a reference implementation in a different language.

---

*Part 4 - Session 7: AES-GCM Verification, Tail Encoding, Encoding Refinement*
*SimpleGo Protocol Analysis*
*Original dates: January 24, 2026*
*Rewritten: March 4, 2026 (v2)*
*All cryptography verified, Tail encoding discovered, first native SMP confirmed*
