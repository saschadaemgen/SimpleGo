![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 17: Session 20
# Complete Double Ratchet Body Decrypt, Zstd Integration, Peer Profile on ESP32

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-02-06
**Status:** COMPLETED -- TLS to JSON, complete crypto chain working
**Previous:** Part 16 - Session 19
**Next:** Part 18 - Session 21
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 20 SUMMARY

```
Session 20 completed the entire decryption chain from TLS 1.3 to
readable JSON on an ESP32. Bug #19 root cause found: a debug
self-decrypt test in smp_peer.c:347 triggered a spurious DH Ratchet
Step, corrupting header_key_recv. Fix: removed the debug test. DH
Ratchet Step implemented (two rootKdf calls: recv chain + send chain).
Body decrypt succeeded (14832 ciphertext to 8887 bytes plaintext).
ConnInfo tag 'I' identified as AgentConnInfo (peer profile). Zstd
decompression integrated (8881 compressed to 12268 bytes JSON).
Peer profile "cannatoshi" read on ESP32.

 Bug #19 fixed (debug self-decrypt corrupted ratchet state)
 DH Ratchet Step: 2x rootKdf (recv chain + send chain)
 Body decrypt SUCCESS (14832 to 8887 bytes)
 Zstd integrated (v1.5.5, ~117KB flash)
 Peer profile JSON parsed: displayName "cannatoshi"
```

---

## Bug #19: Debug Self-Decrypt Corrupted Ratchet State

Claude Code analysis found the root cause: `smp_peer.c:347` called `ratchet_decrypt()` on our own encrypted message for debug verification. This function has side effects: when the DH key in the decrypted header differs from `dh_peer`, it triggers a DH Ratchet Step. Since the header contained `dh_self.public_key` (our own key), `dh_changed = true` triggered a spurious ratchet step, corrupting five fields:

| Field | Before | After |
|-------|--------|-------|
| header_key_recv | 1c08e86e... (correct) | cf0c74d2... (wrong) |
| root_key | correct from X3DH | corrupted |
| chain_key_recv | unset | corrupted |
| dh_peer | peer's key | our own key |
| msg_num_recv | 0 | reset to 0 |

Call flow showing the corruption point:

```
send_agent_confirmation():
  [309] ratchet_x3dh_sender()    -> header_key_recv correct
  [317] ratchet_init_sender()    -> no change to recv keys
  [335] ratchet_encrypt()        -> no change to recv keys
  [347] ratchet_decrypt() DEBUG  -> header_key_recv CORRUPTED
```

Fix: removed the debug self-decrypt test. Rule: never use production decrypt functions for debug self-tests.

---

## DH Ratchet Step + Body Decrypt

### 6-Step Process

**Step 1: DH Ratchet Step Recv (rootKdf #1)**

```
dh_secret_recv = X448_DH(peer_new_pub, our_old_priv)   // 56 bytes
HKDF_SHA512(salt=root_key, ikm=dh_secret_recv, info="SimpleXRootRatchet", len=96)
  [0-31]  new_root_key_1
  [32-63] recv_chain_key
  [64-95] new_nhk_recv
```

**Step 2: DH Ratchet Step Send (rootKdf #2)**

```
(our_new_priv, our_new_pub) = X448_GENERATE_KEYPAIR()
dh_secret_send = X448_DH(peer_new_pub, our_new_priv)   // 56 bytes
HKDF_SHA512(salt=new_root_key_1, ikm=dh_secret_send, info="SimpleXRootRatchet", len=96)
  [0-31]  new_root_key_2
  [32-63] send_chain_key
  [64-95] new_nhk_send
```

**Step 3: Chain KDF**

```
HKDF_SHA512(salt="", ikm=recv_chain_key, info="SimpleXChainRatchet", len=96)
  [0-31]  next_recv_ck
  [32-63] message_key
  [64-79] iv_body      (NOT header IV!)
  [80-95] iv_header    (ignored during decrypt)
```

Critical correction from earlier sessions: iv1 [64-79] is the body IV, not header IV. During decrypt, the header IV comes from ehIV in the wire format.

**Step 4: AES-256-GCM Decrypt**

```
AAD = rcAD[112] || emHeader[123 raw bytes] = 235 bytes
AES256_GCM_DECRYPT(key=message_key, iv=iv_body, aad=AAD, ct=emBody, tag=emAuthTag)
```

AAD uses the raw emHeader bytes as received, not re-serialized.

**Step 5: unPad**

```
msg_len = BE_uint16(decrypted[0..1])
plaintext = decrypted[2 .. 2+msg_len-1]
```

**Step 6: State Update** (activated in Session 21)

---

## ConnInfo Tags and Zstd Compression

First byte after body decrypt + unPad: 0x49 = 'I' (AgentConnInfo), not 'D' (AgentConnInfoReply).

| Tag | Constructor | Who Sends | Content |
|-----|------------|-----------|---------|
| 'I' | AgentConnInfo | Initiator (or joiner on Reply Queue) | Profile only |
| 'D' | AgentConnInfoReply | Joiner (on Contact Queue) | SMP Queues + Profile |

ConnInfo is wrapped in a compressed batch format:

```
[0]     'I'   AgentConnInfo tag
[1]     'X'   Compressed batch marker
[2]     01    NonEmpty count: 1
[3]     '1'   Zstd compressed (vs '0' for passthrough)
[4-5]   22 b1 Zstd data length: 8881
[6+]    Zstd frame (magic: 28 b5 2f fd)
```

Zstd v1.5.5 integrated as ESP-IDF component (~117KB flash). Standard Level 3, no dictionary, max decompressed 65536 bytes. Result: 8881 bytes compressed to 12268 bytes JSON.

---

## Peer Profile

```json
{
  "v": "1-16",
  "event": "x.info",
  "params": {
    "profile": {
      "displayName": "cannatoshi",
      "fullName": "",
      "image": "data:image/jpg;base64,...",
      "preferences": { "calls": {"allow":"no"}, "files": {"allow":"always"}, ... }
    }
  }
}
```

First time a peer's SimpleX profile has been read on an ESP32 device.

---

## Complete HKDF Chain (All Steps Verified)

```
HKDF #1: X3DH -> (hk, nhk, sk)
  Salt: 64x0x00, IKM: DH1||DH2||DH3, Info: "SimpleXX3DH"
  hk[0-31], nhk[32-63], sk[64-95]

HKDF #2: Root KDF Recv -> (rk1, ck_recv, nhk_recv)
  Salt: sk, IKM: DH(peer_new, our_old), Info: "SimpleXRootRatchet"

HKDF #3: Root KDF Send -> (rk2, ck_send, nhk_send)
  Salt: rk1, IKM: DH(peer_new, our_NEW), Info: "SimpleXRootRatchet"

HKDF #4: Chain KDF Recv -> (ck', mk, iv_body, iv_header)
  Salt: "", IKM: ck_recv, Info: "SimpleXChainRatchet"
```

---

*Part 17 - Session 20: Body Decrypt + Peer Profile*
*SimpleGo Protocol Analysis*
*Original date: February 6, 2026*
*Rewritten: March 4, 2026 (v2)*
*Complete crypto chain TLS to JSON, displayName "cannatoshi" on ESP32*
