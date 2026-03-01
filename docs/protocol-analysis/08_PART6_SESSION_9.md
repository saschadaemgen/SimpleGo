![SimpleGo](../gfx/sg_multi_agent_ft_header.png)

# SimpleX Protocol Analysis - Part 6: Session 9
# Reply Queue Decryption & A_CRYPTO Error Investigation

**Document Version:** v23  
**Date:** 2026-01-27 Session 9  
**Status:** Reply Queue Decrypt WORKING, A_CRYPTO Error in App  
**Previous:** Part 5 - Session 8 Breakthrough (AgentConfirmation works)

---

## 192. Session 9 Overview (2026-01-27)

### 192.1 Starting Point
After the Session 8 breakthrough where AgentConfirmation was accepted:
- Contact "ESP32" appears in SimpleX App
- App sends messages back to Reply Queue
- Reply Queue messages arrive encrypted
- **Problem:** Cannot decrypt Reply Queue messages

### 192.2 Session 9 Goals
1. Debug Reply Queue XSalsa20-Poly1305 decryption
2. Understand why Poly1305 tag verification fails
3. Fix any cryptographic mismatches

---

## 193. Initial Debug: Poly1305 Tag Mismatch

### 193.1 The Symptom
```
I (36051) SMP:    Decrypting REPLY QUEUE message...
I (36141) SMP:       Received tag: 60cccafc...
I (36141) SMP:       Computed tag: 165f637e...
E (36151) SMP:       Poly1305 tag mismatch!
```

### 193.2 Verified Correct Components
We systematically verified:
- **msgId as nonce:** 24 bytes, correctly zero-padded
- **Ciphertext pointer:** Correctly positioned after MSG header
- **enc_len calculation:** `content_len - p` is correct

### 193.3 The Mystery
Despite all parameters appearing correct, tag computation always failed.

---

## 194. Critical Discovery: Inconsistent shared_secret

### 194.1 Debug Logging Added
We added `DEBUG shared_secret check` at multiple points:
1. After `queue_create()` return
2. After Contact creation
3. After SUB command
4. At message decrypt time

### 194.2 First Test - INCONSISTENT!
```
Queue creation shared_secret: 09dd794e...
Decrypt time shared_secret:   abfee2ff...  <- DIFFERENT!
```

**The shared_secret was being overwritten somewhere!**

### 194.3 Second Test - After Investigation
After careful code review, the shared_secret remained consistent:
```
I (7931) SMP_QUEUE:    shared_secret at creation: dfc18d8e...
I (8061) SMP: DEBUG shared_secret check: dfc18d8e  <- after queue_create()
I (9891) SMP: DEBUG shared_secret check: dfc18d8e  <- after contact create
I (10331) SMP: DEBUG shared_secret check: dfc18d8e <- after SUB
I (38381) SMP: DEBUG shared_secret check: dfc18d8e <- at decrypt time
```

**But decrypt still failed!**

---

## 195. The Real Bug: HSalsa20 Key Derivation

### 195.1 Comparing Working vs Broken Code

**Working Contact Decrypt (smp_crypto.c):**
```c
// Uses crypto_box_beforenm - does X25519 + HSalsa20
crypto_box_beforenm(shared, c->srv_dh_public, c->rcv_dh_secret);

// Then decrypt with derived key
crypto_box_open_easy_afternm(plain, encrypted, enc_len, nonce, shared);
```

**Broken Reply Queue (smp_queue.c):**
```c
// Uses crypto_scalarmult - ONLY raw X25519, NO HSalsa20!
crypto_scalarmult(our_queue.shared_secret,
                  our_queue.rcv_dh_private,
                  our_queue.srv_dh_public);
```

### 195.2 The Key Insight
SimpleX uses **NaCl crypto_box** which internally does:
1. X25519 DH -> raw shared secret
2. HSalsa20 key derivation -> final encryption key

`crypto_scalarmult` only does step 1!
`crypto_box_beforenm` does both steps 1 and 2!

### 195.3 Reference: Haskell Code
```haskell
-- Server.hs:2024
encrypt body = RcvMessage msgId' . EncRcvMsgBody $ 
    C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId') body

-- Crypto.hs - cbEncryptMaxLenBS uses cryptoBox
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s
    tag = Poly1305.auth rs c
```

The `cryptoBox` function uses XSalsa20-Poly1305, but when called through NaCl's `crypto_box`, the key first goes through HSalsa20 derivation.

---

## 196. The Fix

### 196.1 smp_queue.c Line ~444
**Before:**
```c
// Compute shared secret for message decryption
if (crypto_scalarmult(our_queue.shared_secret,
                      our_queue.rcv_dh_private,
                      our_queue.srv_dh_public) != 0) {
```

**After:**
```c
// Compute shared secret for message decryption
// Use crypto_box_beforenm for X25519 + HSalsa20 key derivation!
if (crypto_box_beforenm(our_queue.shared_secret,
                        our_queue.srv_dh_public,
                        our_queue.rcv_dh_private) != 0) {
```

**Note:** Parameter order changes! `crypto_box_beforenm(out, pk, sk)` vs `crypto_scalarmult(out, sk, pk)`

### 196.2 main.c Reply Queue Decrypt
**Before:** Complex manual XSalsa20 + Poly1305 implementation (50+ lines)

**After:** Simple NaCl decrypt (10 lines)
```c
if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
    uint8_t server_nonce[24];
    memset(server_nonce, 0, 24);
    memcpy(server_nonce, msg_id, msgIdLen);
    
    uint8_t *server_plain = malloc(enc_len);
    if (server_plain) {
        if (crypto_box_open_easy_afternm(server_plain, &resp[p], enc_len, 
                                          server_nonce, our_queue.shared_secret) == 0) {
            int plain_len = enc_len - crypto_box_MACBYTES;
            ESP_LOGI(TAG, "      Reply Queue decrypt SUCCESS! (%d bytes)", plain_len);
            // ... process message
        }
        free(server_plain);
    }
}
```

---

## 197. SUCCESS: Reply Queue Decryption Working!

### 197.1 The Moment of Truth
```
I (38391) SMP:
I (38391) SMP:    Decrypting REPLY QUEUE message...
I (38401) SMP:       Reply Queue decrypt SUCCESS! (16106 bytes)
I (38401) SMP:       First 32 bytes:
         3e 82 00 00 00 00 69 79 2a 97 54 20 00 04 31 2c 30 2a 30 05 06 03 2b 65 6e 03 21 00 8d 17 1a 24
```

### 197.2 Decrypted Message Structure
The decrypted bytes show:
- `3e 82` = Length prefix (16002 bytes)
- `00 00 00 00 69 79 2a 97` = Timestamp
- `54 20 00 04` = Header bytes
- `31 2c` = "1," (Version string)
- `30 2a 30 05 06 03 2b 65 6e 03 21 00` = **X25519 SPKI Header!**

**This is the exact same format as Contact Queue messages!**

---

## 198. New Problem Discovered: A_CRYPTO Error

### 198.1 SimpleX Chat Console
```
21:14:00 < error agent
```

**Error Details:**
```json
{
    "contactId": 11,
    "localDisplayName": "ESP32",
    "activeConn": {
        "connStatus": "requested",
        "pqSupport": true,
        "pqEncryption": true
    }
}
```

**Error Type:** `AGENT A_CRYPTO`

### 198.2 Self-Decrypt Test Already Failing
In our own logs:
```
I (36071) SMP_PEER: DEBUG: Testing self-decrypt of encConnInfo...
E (36081) SMP_RATCH: Header decryption failed (try with full AAD?)
E (36081) SMP_PEER: Self-decrypt FAILED!
```

### 198.3 Diagnosis
The SimpleX App **cannot decrypt** our AgentConfirmation message!
- Server accepts the message (OK response)
- But cryptographic verification fails in the app
- Our own self-decrypt test also fails

**The Double Ratchet header encryption has a bug.**

---

## 199. Technical Summary: crypto_scalarmult vs crypto_box_beforenm

### 199.1 crypto_scalarmult (libsodium)
```
Input:  scalar (private key), point (public key)
Output: raw X25519 shared secret
```

### 199.2 crypto_box_beforenm (libsodium/NaCl)
```
Input:  pk (public key), sk (secret key)  
Output: derived key = HSalsa20(raw_shared_secret, zero_nonce)
```

### 199.3 Wire Format for crypto_box
```
[16-byte Poly1305 tag][ciphertext]
```

### 199.4 Key Lesson
**Always use the same crypto primitive chain as the sender!**
- If sender uses `crypto_box_*` -> receiver must use `crypto_box_open_*`
- Raw `crypto_scalarmult` output is NOT compatible with NaCl crypto_box!

---

## 200. Updated Bug Tracker

| Bug ID | Description | Status | Session | Solution |
|--------|-------------|--------|---------|----------|
| BUG-001 to BUG-012 | All previous encoding bugs | FIXED | S1-S7 | Various fixes |
| BUG-013 | chainKdf IV order | FIXED | S8 | iv1=msg, iv2=header |
| BUG-014 | Payload AAD length prefix | FIXED | S8 | Removed prefix |
| **BUG-015** | **Reply Queue HSalsa20** | **FIXED** | **S9** | **crypto_box_beforenm** |
| **BUG-016** | **A_CRYPTO in App** | **ACTIVE** | **S9** | **Header decrypt AAD issue** |

---

## 201. Next Steps (Session 9 -> 10)

### 201.1 Priority 1: Fix A_CRYPTO Error
The app cannot decrypt our AgentConfirmation. Need to investigate:
- Header encryption AAD format
- Self-decrypt test failure
- Possible rcAD construction issue

### 201.2 Priority 2: Parse Reply Queue Messages  
Now that decrypt works, parse the agent messages:
- Same format as Contact Queue
- Handle HELLO, CON, MSG commands

### 201.3 Priority 3: Full Duplex Communication
- Send messages TO the app
- Receive messages FROM the app
- Complete chat functionality

---

## 202. Code Changes Summary (Session 9)

### 202.1 smp_queue.c
```diff
- if (crypto_scalarmult(our_queue.shared_secret,
-                       our_queue.rcv_dh_private,
-                       our_queue.srv_dh_public) != 0) {
+ if (crypto_box_beforenm(our_queue.shared_secret,
+                         our_queue.srv_dh_public,
+                         our_queue.rcv_dh_private) != 0) {
```

### 202.2 main.c - Reply Queue Decrypt
Replaced ~60 lines of manual XSalsa20-Poly1305 with:
```c
if (crypto_box_open_easy_afternm(server_plain, &resp[p], enc_len, 
                                  server_nonce, our_queue.shared_secret) == 0) {
    // SUCCESS!
}
```

---

## 203. Session 9 Timeline

| Time | Event |
|------|-------|
| Start | Debug Reply Queue Poly1305 tag mismatch |
| +30m | Added shared_secret consistency checks |
| +45m | Discovered shared_secret was consistent |
| +60m | Compared working Contact decrypt vs broken Reply Queue |
| +75m | Identified crypto_scalarmult vs crypto_box_beforenm |
| +90m | Applied fix to smp_queue.c |
| +100m | **Reply Queue decrypt SUCCESS!** |
| +110m | Discovered A_CRYPTO error in SimpleX App |
| +120m | Identified self-decrypt test failure |
| Current | Investigating header encryption AAD issue |

---

## 204. Key Learning: NaCl Crypto Layers

```
+-------------------------------------------------------------+
|                     NaCl crypto_box                         |
+-------------------------------------------------------------+
|  1. X25519 DH:       scalarmult(sk, pk) -> raw_secret       |
|  2. HSalsa20:        derive(raw_secret) -> box_key          |
|  3. XSalsa20-Poly1305: encrypt(box_key, nonce, msg)         |
+-------------------------------------------------------------+
|  crypto_scalarmult:    Only step 1                          |
|  crypto_box_beforenm:  Steps 1 + 2 (returns box_key)        |
|  crypto_box_easy:      All steps in one call                |
+-------------------------------------------------------------+
```

---

## 205. SimpleGo Version Update

```
SimpleGo v0.1.17-alpha - "Reply Queue Unlocked"
===============================================================

Session 9 Achievements:
- Reply Queue decryption WORKING!
- HSalsa20 key derivation bug fixed
- Same message format as Contact Queue confirmed
- A_CRYPTO error discovered (next priority)

Bug Fix:
- crypto_scalarmult -> crypto_box_beforenm
- Raw X25519 -> X25519 + HSalsa20 derivation
- NaCl crypto_box compatibility restored

Current Status:
- AgentConfirmation: Server accepts, App decrypt FAILS
- Reply Queue Server Decrypt: WORKING
- Header Encryption AAD: Needs investigation
- Full duplex communication: Pending

===============================================================
```

---

**DOCUMENT CREATED: 2026-01-27 Session 9 v23**  
**Reply Queue Decrypt: WORKING**  
**Next: Fix A_CRYPTO Error (Header Encryption AAD)**
