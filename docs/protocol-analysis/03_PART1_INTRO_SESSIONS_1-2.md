![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 1: Sessions 1-2
# A_VERSION Debugging, Protocol Encoding, and Initial Cryptography

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-22 to 2026-01-23
**Status:** COMPLETED -- A_VERSION resolved, A_MESSAGE investigation ongoing
**Next:** Part 2 - Sessions 3-4
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 1-2 SUMMARY

```
Sessions 1-2 established the foundation of the SimpleX protocol reverse
engineering effort. Starting from raw Haskell source analysis, every
protocol layer was documented, 18 bugs were found and fixed, and the
A_VERSION error was fully resolved. The A_MESSAGE error (parsing failure)
remained as the primary target for subsequent sessions.

18 Bugs Fixed (S1-S2)
 3 Protocol layers fully documented (SMP, Agent, Double Ratchet)
 3 Version checks analyzed and satisfied
 5 Encoding rules reverse-engineered from Haskell source
```

---

## SimpleX Protocol Stack

```
SimpleX Messaging Protocol
|
+-- Transport Layer (TLS 1.3)
|   ALPN: "smp/1"
|
+-- SMP Protocol Layer
|   Commands: NEW, KEY, SUB, SEND, ACK, OFF, DEL
|   Server Responses: IDS, MSG, OK, ERR, END
|   ClientMsgEnvelope (for SEND)
|       PubHeader
|           phVersion (Word16 BE) -- SMP Client Version 1-4
|           phE2ePubDhKey (Maybe X25519)
|       cmNonce (24 bytes)
|       cmEncBody (encrypted)
|
+-- Encryption Layer (crypto_box)
|   ClientMessage (after decryption)
|       PrivHeader
|           PHConfirmation = 'K' + Ed25519 SPKI (no length prefix)
|           PHEmpty = '_'
|       Body (AgentMsgEnvelope)
|
+-- Agent Protocol Layer
|   AgentMsgEnvelope Types:
|       AgentConfirmation (Tag 'C')
|       AgentInvitation
|       AgentMsgEnvelope (Tag 'M')
|       AgentRatchetKey
|   AgentConfirmation Structure:
|       agentVersion (Word16 BE) -- Agent Version 2-7
|       'C' Tag
|       e2eEncryption_ (Maybe E2ERatchetParams)
|           E2ERatchetParams
|               e2eVersion (Word16 BE) -- E2E Version 2-3
|               e2ePubKey1 (X448 SPKI)
|               e2ePubKey2 (X448 SPKI)
|               e2eKEM (optional, v3+)
|       encConnInfo (Ratchet-encrypted)
|           'I' = AgentConnInfo (initiating party)
|           'D' = AgentConnInfoReply (joining party)
|
+-- Double Ratchet Layer
|   X3DH Key Agreement
|   Header Encryption
|   Message Encryption (ChaCha20-Poly1305)
|
+-- Chat Protocol Layer (JSON)
    {"event":"x.info","params":{...}}
```

---

## The Three A_VERSION Checks

The SimpleX app performs three independent version checks on received AgentConfirmation messages. Any failure throws A_VERSION.

### Check 1: SMP Client Version (Agent.hs:2707)

```haskell
unless (phVer `isCompatible` clientVRange || phVer <= agreedClientVerion)
  . throwE $ AGENT A_VERSION
```

Checks phVer from the unencrypted PubHeader of ClientMsgEnvelope. Must be within SMP Client Version range 1-4.

### Check 2: Agent Version + SMP Client Version (Agent.hs:2908)

```haskell
let compatible =
      (agentVersion `isCompatible` smpAgentVRange || agentVersion <= agreedAgentVersion)
        && (phVer `isCompatible` smpClientVRange || phVer <= agreedClientVerion)
unless compatible $ throwE $ AGENT A_VERSION
```

Combined check. agentVersion comes from the decrypted AgentConfirmation body.

### Check 3: E2E Encryption Version (Agent.hs:2913)

```haskell
unless (e2eVersion `isCompatible` e2eEncryptVRange) (throwE $ AGENT A_VERSION)
```

Checks e2eVersion from E2ERatchetParams inside the AgentConfirmation.

### Version Ranges

| Protocol Layer | Range | Current | ESP32 Sends |
|----------------|-------|---------|-------------|
| SMP Client (phVer) | 1-4 | 4 | 0x00 0x04 |
| SMP Agent (agentVersion) | 2-7 | 7 | 0x00 0x07 |
| E2E Encryption (e2eVersion) | 2-3 | 3 | 0x00 0x02 |

### SMP Agent Version Features

| Version | Feature |
|---------|---------|
| 2 | Duplex handshake |
| 3 | Ratchet sync |
| 4 | Delivery receipts |
| 5 | Post-quantum double ratchet |
| 6 | Secure reply queues |
| 7 | Ratchet on confirmation |

### SMP Client Version Features

| Version | Feature |
|---------|---------|
| 1 | Initial version |
| 2 | Server hostnames |
| 3 | Send auth key |
| 4 | Short links (current) |

---

## SMP Encoding Rules

These encoding rules were reverse-engineered from Encoding.hs and are fundamental to all protocol interactions.

### Maybe Type (Encoding.hs:114-115)

```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
```

Uses ASCII characters, not binary: Nothing = '0' (0x30), Just x = '1' (0x31) + encoded value.

### ByteString Length (Encoding.hs:100-104)

```haskell
instance Encoding ByteString where
  smpEncode s = B.cons (lenEncode $ B.length s) s

lenEncode :: Int -> Char
lenEncode = w2c . fromIntegral  -- Single byte
```

1-byte length prefix for standard ByteStrings.

### Large Type (Encoding.hs:136)

```haskell
instance Encoding Large where
  smpEncode (Large s) = smpEncode @Word16 (fromIntegral $ B.length s) <> s
```

2-byte (Word16 BE) length prefix for Large wrapper.

### Tail Type

No length prefix. Consumes all remaining bytes.

### Tuple Encoding

Sequential concatenation. `smpEncode (a, b, c)` = `smpEncode a <> smpEncode b <> smpEncode c`.

---

## Wire Format Analysis

### ClientMsgEnvelope

```haskell
smpEncode ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody} =
    smpEncode (cmHeader, cmNonce, Tail cmEncBody)
```

```
[2B phVersion BE][Maybe-Tag][44B X25519 SPKI (if '1')][24B Nonce][Encrypted Body]

Example:
  00 04        SMP v4
  31           '1' = Just (DH key present)
  30 2a 30 05  X25519 SPKI header...
  ...          24 bytes nonce
  ...          ciphertext
```

### ClientMessage (after crypto_box decryption)

```haskell
smpEncode = \case
  PHConfirmation k -> "K" <> smpEncode k
  PHEmpty -> "_"
```

```
['K'][44B Ed25519 SPKI Auth Key][AgentMsgEnvelope body...]
```

### AgentConfirmation

```haskell
smpEncode AgentConfirmation {agentVersion, e2eEncryption_, encConnInfo} =
  smpEncode (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)
```

```
[2B Agent Version BE]['C'][Maybe-Tag '1'][E2E Params][encConnInfo...]
```

### E2ERatchetParams (v2, without PQ)

```haskell
smpEncode (E2ERatchetParams v k1 k2 kem_)
  | otherwise = smpEncode (v, k1, k2)  -- v2: no KEM
```

```
[2B E2E Version BE][1B len=68][68B X448 SPKI Key1][1B len=68][68B X448 SPKI Key2]
```

### EncRatchetMessage (v2)

```haskell
encodeEncRatchetMessage v EncRatchetMessage {emHeader, emBody, emAuthTag} =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)
```

```
[1B emHeader-len][emHeader...][16B emAuthTag][emBody... (Tail)]
```

### EncMessageHeader (v2)

```haskell
smpEncode EncMessageHeader {ehVersion, ehIV, ehAuthTag, ehBody} =
    smpEncode (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody
```

```
[2B ehVersion][16B ehIV][16B ehAuthTag][1B ehBody-len][88B ehBody]
= 2 + 16 + 16 + 1 + 88 = 123 bytes
```

IV and AuthTag have no length prefix (they are fixed-size types with direct encoding).

### MsgHeader (v2)

```haskell
encodeMsgHeader v MsgHeader {msgMaxVersion, msgDHRs, msgKEM, msgPN, msgNs}
  | otherwise = smpEncode (msgMaxVersion, msgDHRs, msgPN, msgNs)  -- v2: no KEM
```

```
[2B msgMaxVersion][1B len=68][68B X448 SPKI][4B msgPN BE][4B msgNs BE]
Total before padding: 79 bytes, padded to 88 bytes
```

### SMPQueueInfo (v4)

```haskell
smpEncode (SMPQueueInfo clientVersion SMPQueueAddress {smpServer, senderId, dhPublicKey, queueMode})
  | clientVersion >= shortLinksSMPClientVersion = addrEnc <> maybe "" smpEncode queueMode
  where
    addrEnc = smpEncode (clientVersion, smpServer, senderId, dhPublicKey)
```

```
[2B clientVersion][host_count][host_len][host...][' '][port][keyHash_len][keyHash]
[senderId_len][senderId][dhPublicKey_len][dhPublicKey]['0' (queueMode Nothing)]
```

Port uses space separator (0x20), not length prefix. queueMode requires '0' (Nothing) for v4+.

---

## Haskell Source Map

```
~/simplexmq/src/Simplex/Messaging/
|
+-- Protocol.hs          SMP Protocol Layer
|   ClientMsgEnvelope (Line 1067)
|   PubHeader (Line 1074), PrivHeader (Line 1093)
|   ClientMessage (Line 1091)
|   SMPQueueInfo encoding
|   VersionSMPC definitions (Line 277-293)
|
+-- Encoding.hs          Encoding Rules
|   Maybe Encoding (Line 114-115)
|   ByteString Encoding (Line 100-104)
|   Large Encoding (Line 136)
|
+-- Crypto.hs            Cryptographic Functions
|   CbNonce (Line 1350, 24 bytes)
|   cbEncrypt (Line 1268)
|   APublicAuthKey encoding (no length prefix)
|   IV/AuthTag encoding (no length prefix)
|
+-- Crypto/Ratchet.hs    Double Ratchet
|   E2ERatchetParams (Line ~239)
|   e2eEncryptVRange (Line 145-155)
|   EncRatchetMessage (Line 773)
|   chainKdf, pqX3dh, initSndRatchet
|
+-- Agent.hs             Agent Protocol Logic
|   A_VERSION checks (Lines 2707, 2908, 2913)
|   A_MESSAGE checks (Lines 2780, 2813, 2897)
|   processClientMsg (Line 2704)
|   smpConfirmation (Line 2900)
|
+-- Agent/Protocol.hs    Agent Protocol Types
    AgentMsgEnvelope, AgentMessage
    SMPQueueInfo, smpAgentVRange (Line 286-308)
```

---

## ESP32 Project Structure (Sessions 1-2)

```
C:\Espressif\projects\simplex_client\main\
|
+-- main.c              Main program, WiFi, message loop
+-- smp_peer.c          AgentConfirmation, HELLO
+-- smp_queue.c         Queue creation (NEW, KEY, SUB), SMPQueueInfo encoding
+-- smp_handshake.c     SKEY, handshake completion
+-- smp_ratchet.c       Double Ratchet encryption
+-- smp_x448.c          X448 DH, E2E params
+-- smp_crypto.c        crypto_box, signatures
+-- smp_parser.c        Message parsing
+-- smp_contacts.c      Contact management
+-- smp_network.c       TLS connection
+-- smp_utils.c         Base64, encoding helpers
+-- smp_globals.c       Global state, SPKI headers
```

---

## X3DH Key Agreement Analysis

### Key Assignment

| Haskell | ESP32 Variable | Description |
|---------|----------------|-------------|
| spk1 | our_key1 | Our first keypair |
| spk2 | our_key2 | Our second keypair |
| rk1 | peer_key1 | Peer's first public key (from invitation) |
| rk2 | peer_key2 | Peer's second public key (from invitation) |

### DH Operations (as Sender/Joiner)

From pqX3dhSnd in Ratchet.hs:

```haskell
pqX3dhSnd spk1 spk2 spKem_ (E2ERatchetParams v rk1 rk2 rKem_) = do
  let initParams = pqX3dh (publicKey spk1, rk1) (dh' rk1 spk2) (dh' rk2 spk1) (dh' rk2 spk2)
```

| DH | Haskell | ESP32 C |
|----|---------|---------|
| DH1 | dh' rk1 spk2 | x448_dh(peer_key1, our_key2->private_key, dh1) |
| DH2 | dh' rk2 spk1 | x448_dh(peer_key2, our_key1->private_key, dh2) |
| DH3 | dh' rk2 spk2 | x448_dh(peer_key2, our_key2->private_key, dh3) |

### X3DH KDF

```haskell
(hk, nhk, sk) =
  let salt = B.replicate 64 '\0'   -- 64 null bytes
   in hkdf3 salt dhs "SimpleXX3DH"
```

Output: 96 bytes split into sndHK (32), rcvNextHK (32), ratchetKey (32). Uses HKDF-SHA512 (not SHA256).

### AssocData

```haskell
assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1
```

112 bytes: our_key1_public (56 bytes raw X448) concatenated with peer_key1 (56 bytes raw X448). Used as AAD for both header and payload AES-GCM encryption.

---

## KDF Functions

### kdf_root

```c
hkdf_sha512(root_key, 32, dh_out, 56,
            (const uint8_t *)"SimpleXRootRatchet", 18,
            kdf_output, 96);
// Output: header_key (32) + chain_key (32) + next_root_key (32)
```

### kdf_chain (chainKdf)

```haskell
chainKdf :: RatchetKey -> (RatchetKey, Key, IV, IV)
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)
```

```c
hkdf_sha512(NULL, 0, chain_key, 32,
            (const uint8_t *)"SimpleXChainRatchet", 19,
            kdf_output, 96);
// Output: next_chain_key (32) + message_key (32) + msg_iv (16) + header_iv (16)
```

IVs are derived from KDF, not generated randomly. This was a critical bug in the original implementation.

---

## Payload AAD Analysis

Two encryption phases with different AAD:

**Phase 1 (Header Encryption):**
AAD = rcAD (112 bytes) = our_key1_public || peer_key1

**Phase 2 (Payload Encryption):**
AAD = rcAD + emHeader (112 + 123 = 235 bytes)

```haskell
(emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
```

The recipient can verify this because emHeader is the received encrypted blob (not the decrypted content), which is available before payload decryption begins.

---

## Consolidated Bug List (Sessions 1-2)

| # | Bug | Session | Root Cause | Fix |
|---|-----|---------|------------|-----|
| 1 | A_VERSION (2x) | S1 | PrivHeader had length prefix after 'K' | Removed length prefix |
| 2 | CryptoInvalidMsgError | S1 | crypto_box missing 2-byte payload length prefix | Added padding format |
| 3 | SMPQueueInfo clientVersion=14 | S1 | Hardcoded version outside valid range | Changed to 4 |
| 4 | IV/AuthTag had length prefixes | S1 | Assumed ByteString encoding for fixed types | Removed length prefixes |
| 5 | MsgHeader X448 raw instead of SPKI | S1 | Sent 0x01+raw(56B) instead of SPKI(68B) | X448 SPKI format |
| 6 | X3DH DH3 = DH2 (copied) | S2 | Placeholder memcpy never replaced | Proper DH3 computation |
| 7 | X3DH Salt NULL pointer | S2 | Passed NULL instead of 64 zero bytes | uint8_t salt[64] = {0} |
| 8 | X3DH Output 32 instead of 96 bytes | S2 | Only extracted root_key, missed hk+nhk | 96 bytes: hk+nhk+sk |
| 9 | HKDF used SHA256 instead of SHA512 | S2 | Wrong hash algorithm | MBEDTLS_MD_SHA512 |
| 10 | kdf_root info string wrong | S2 | "SimpleXRatchet" (14B) | "SimpleXRootRatchet" (18B) |
| 11 | kdf_chain info string wrong | S2 | "SimpleXChain" (12B) | "SimpleXChainRatchet" (19B) |
| 12 | kdf_chain output 64 instead of 96 | S2 | IVs generated randomly instead of from KDF | 96 bytes with IVs |
| 13 | ratchet_init_sender overwrote key | S2 | generate_keypair called after memcpy | Removed generate_keypair |
| 14 | emHeader 125 instead of 123 bytes | S2 | IV/AuthTag incorrectly length-prefixed | Direct encoding |
| 15 | Port had length prefix instead of space | S2 | Protocol uses space separator at this point | buf[p++] = ' ' |
| 16 | queueMode missing for v4+ | S2 | Parser expects Maybe field after dhPublicKey | buf[p++] = '0' |
| 17 | AssocData (AAD) missing from AES-GCM | S2 | No AAD passed to header/payload encryption | 112 bytes rcAD |
| 18 | Payload AAD 112 instead of 235 bytes | S2 | Missing emHeader in payload AAD | rcAD + emHeader = 235B |

**Result after Sessions 1-2:** A_VERSION fully resolved. A_MESSAGE (parsing error) remains as primary target.

---

## ratchet_state_t Structure (after Session 2)

```c
typedef struct {
    uint8_t root_key[32];
    uint8_t header_key_send[32];
    uint8_t header_key_recv[32];
    uint8_t chain_key_send[32];
    uint8_t chain_key_recv[32];
    x448_keypair_t dh_self;
    uint8_t dh_peer[56];
    uint32_t msg_num_send;
    uint32_t msg_num_recv;
    uint32_t prev_chain_len;
    bool initialized;
    uint8_t assoc_data[112];  // our_key1_public || peer_key1
} ratchet_state_t;
```

---

## X448 SPKI Header

12-byte ASN.1 header for X448 SubjectPublicKeyInfo encoding:

```
30 42 30 05 06 03 2b 65 6f 03 39 00

30 42       SEQUENCE, 66 bytes
30 05       SEQUENCE, 5 bytes (AlgorithmIdentifier)
06 03       OID, 3 bytes
2b 65 6f    OID 1.3.101.111 = X448
03 39 00    BIT STRING, 57 bytes (0 unused bits)
```

Total SPKI key: 12 + 56 = 68 bytes. Encoded as ByteString with 1-byte length prefix: [0x44][68 bytes].

---

## Disproven Theories

These hypotheses were investigated and eliminated during Sessions 1-2:

1. phVersion missing from PubHeader -- disproven, correctly sent as 0x00 0x04
2. Nonce encoding wrong -- disproven, correctly sent as 24 raw bytes
3. PrivHeader key needs length prefix -- disproven, SPKI directly after 'K'
4. E2E Params encoding wrong -- confirmed correct
5. AAD size (112 vs 235) causes A_MESSAGE -- disproven, problem is elsewhere

---

*Part 1 - Sessions 1-2: A_VERSION Debugging, Protocol Encoding, Initial Cryptography*
*SimpleGo Protocol Analysis*
*Original dates: January 22-23, 2026*
*Rewritten: March 4, 2026 (v2, consolidated from 2299 lines)*
*18 bugs fixed, A_VERSION resolved, A_MESSAGE ongoing*
