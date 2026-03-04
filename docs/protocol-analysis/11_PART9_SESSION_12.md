![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 9: Session 12
# Separate E2E Keypair Discovery & Reply Queue phE2ePubDhKey = Nothing

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-30
**Status:** COMPLETED -- E2E keypair implemented, decrypt still fails
**Previous:** Part 8 - Session 11
**Next:** Part 10 - Session 13
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 12 SUMMARY

```
Session 12 discovered through Haskell source analysis that SimpleX
uses two separate X25519 keypairs per queue: one for server-level DH
(NEW command) and one for E2E-level DH (peer encryption). The separate
e2e_public/e2e_private keypair was implemented and sent in
SMPQueueInfo. However, the Reply Queue message has phE2ePubDhKey =
Nothing, meaning the app sends no E2E public key in the header.
The app pre-computes e2eDhSecret during connection setup and never
transmits its e2ePubKey in subsequent messages. The key must come
from an earlier message (the app's AgentConfirmation).

 Haskell two-keypair architecture discovered
 Separate e2e keypair implemented in our_queue_t
 SMPQueueInfo corrected to send e2e_public
 Key finding: phE2ePubDhKey = Nothing on Reply Queue
```

---

## Two Separate X25519 Keypairs

Haskell source analysis (Agent/Client.hs:1357-1361) revealed that each queue uses two independent X25519 keypairs:

```haskell
newRcvQueue c nm userId connId srv vRange cMode enableNtfs subMode = do
  e2eKeys <- atomically . C.generateKeyPair =<< asks random  -- SEPARATE E2E KEYPAIR
  newRcvQueue_ c nm userId connId srv vRange qrd enableNtfs subMode Nothing e2eKeys
```

| Keypair | Purpose | Used in |
|---------|---------|---------|
| dhKey / privDhKey | Server-level DH (NEW command) | rcvDhSecret (server auth) |
| e2eDhKey / e2ePrivKey | E2E-level DH (peer encryption) | SMPQueueAddress (peer crypto) |

The e2e_public key is placed in SMPQueueAddress (Store.hs:123-125) and sent to the peer via the invitation link.

---

## Implementation Changes

### Structure Extension (smp_queue.h)

```c
typedef struct {
    uint8_t rcv_dh_public[32];    // server-level DH
    uint8_t rcv_dh_private[32];
    uint8_t e2e_public[32];       // E2E-level DH (NEW)
    uint8_t e2e_private[32];      // E2E-level DH (NEW)
    uint8_t shared_secret[32];    // server-level shared secret
    // ...
} our_queue_t;
```

### E2E Keypair Generation (smp_queue.c:203)

```c
crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);  // NEW
```

### SMPQueueInfo Correction (smp_queue.c:548)

Changed from `rcv_dh_public` to `e2e_public` in the encoded SMPQueueInfo, so the invitation link contains the correct E2E key.

---

## The phE2ePubDhKey = Nothing Problem

After implementing the fix, the Reply Queue message still shows:

```
[14] = '1' (0x31) -- maybe_corrId = Just (has corrId SPKI)
[15] = ',' (0x2C) -- maybe_e2e = Nothing
```

The app sends no E2E public key in the message header.

### Why the App Does Not Send Its Key

From `newSndQueue` (Agent.hs:3365-3379):

```haskell
newSndQueue userId connId (Compatible (SMPQueueInfo ... {dhPublicKey = rcvE2ePubDhKey})) = do
  (e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g
  let sq = SndQueue
        { e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey  -- pre-computed
        , e2ePubKey = Just e2ePubKey                      -- stored locally
        }
```

The app receives our `e2e_public` from SMPQueueInfo, generates its own E2E keypair, immediately pre-computes `e2eDhSecret = DH(our_e2e_pub, app_e2e_priv)`, and stores `e2ePubKey = Just app_e2e_public` locally. The first message (sendConfirmation) includes this key. Subsequent messages (sendAgentMessage) use `Nothing`, relying on the pre-computed secret.

### The Asymmetry

| Side | Has | Can Compute |
|------|-----|-------------|
| App | our_e2e_public + app_e2e_private | DH(our_pub, app_priv) |
| ESP32 | our_e2e_private + ??? | Need app_e2e_public |

The app's `e2ePubKey` was sent in an earlier message (the app's first message on our queue, which is the AgentConfirmation). We need to have received and processed that message to extract the key.

---

## Hypotheses for Key Source

**Hypothesis A:** Protocol version difference. Newer versions might handle E2E differently.

**Hypothesis B:** Queue mode (QMMessaging vs QMContact) affects E2E behavior.

**Hypothesis C:** The E2E key is derived from X3DH key agreement rather than separately generated. The `dh=` key from the invitation (`00f61e58...`) is for Contact Queue, not Reply Queue.

**Hypothesis D:** The key was sent in the app's AgentConfirmation, which arrives on our Contact Queue as a second message that we do not currently receive.

---

*Part 9 - Session 12: Separate E2E Keypair & phE2ePubDhKey = Nothing*
*SimpleGo Protocol Analysis*
*Original date: January 30, 2026*
*Rewritten: March 4, 2026 (v2)*
*Two-keypair architecture, e2e implemented, missing app key identified*
