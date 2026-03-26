# Part 47: Session 50 - Queue Rotation Multi-Fix
**Date:** 2026-03-22 to 2026-03-26 | **Version:** v0.1.18-alpha
**Milestone:** First successful multiple Queue Rotation on ESP32-S3 with Post-Quantum Cryptography

## Overview

Session 49 had Queue Rotation working for ONE rotation. Session 50 made it work for unlimited consecutive rotations. Six fixes across 7 files. The critical root cause was a cache-invalidation timing bug: the CQ E2E peer key cache in smp_tasks.c was filled with stale data BETWEEN rotation_start() and rotation_complete() by incoming QTEST messages during rotation. A full day (~8 hours) was lost chasing the wrong problem due to an incorrect bug classification that Mausi accepted without verifying against the known fact that the first rotation worked. Four consecutive rotations verified on fresh device with post-quantum crypto.

## Starting Point

Five open priorities from Session 49 handoff: (1) Second rotation crashes - HIGH, (2) Late-arrival flow for offline contacts - HIGH, (3) RQ SUB "Non-matching frame" 30s boot delay - MEDIUM, (4) CQ E2E peer key only for first contact - LOW, (5) Refresh timer runs endlessly - LOW.

## Tag 1: Four Fixes

### Fix A: s_complete_logged Never Reset

`static bool s_complete_logged` in app_rotation_step() (smp_tasks.c line 1868) was function-local, set to true on first "ALL CONTACTS MIGRATED" and never reset. Second rotation skipped the entire live-switch block because `if (!s_complete_logged)` was false. Variable was function-local so rotation_start() in smp_rotation.c could not reset it.

Fix: file-static variable + new API smp_tasks_reset_rotation_guard() called from rotation_start(). Pattern matches smp_clear_42d(). Three files: smp_tasks.h, smp_tasks.c, smp_rotation.c.

### Fix B: Auth/DH Backup Overwrites Originals

rotation_complete() backed up auth/DH keys at every rotation. Second rotation saved Rotation 1's keys instead of the originals. Peer server NEVER changes during Queue Rotation and ALWAYS expects ORIGINAL keys.

First implementation (unconditional backup) caused ERR AUTH at second rotation. Fix: conditional backup `if (!rq->has_peer_auth)` and `if (!rq->has_peer_dh)` - save only on first rotation. New fields in reply_queue.h: peer_auth_private[64], peer_auth_public[32], has_peer_auth. Five locations in smp_peer.c updated with has_peer_auth ternary (line 932: `msg_auth_private = rq->has_peer_auth ? rq->peer_auth_private : rq->rcv_auth_private`).

### Fix C: rq->snd_id Semantics Wrong

rotation_complete() wrote `rq->snd_id = rd->rq_new_snd_id` (Reply Queue snd_id). But rotation_build_qadd_payload() reads rq->snd_id for the replacedSndQueue field. The app compares in findQ() against the snd_id from QUSE, and QUSE sends rd->new_snd_id (Main Queue snd_id). First rotation matched by coincidence, second didn't.

Fix: `rq->snd_id = rd->new_snd_id` (Main Queue snd_id).

### Fix D: CQ E2E Peer Cache Never Invalidated

`s_cq_e2e_peer_valid` stayed true after Rotation 1. Lazy-load check never ran for Rotation 2. Stale peer key from Rotation 1 remained in cache.

Fix: smp_tasks_reset_rotation_guard() also sets `s_cq_e2e_peer_valid = false`.

**Result Tag 1:** Four fixes verified. Second rotation runs through on protocol level (QADD/QKEY/KEY/QUSE/QTEST). Sending works after second rotation. Receiving fails with `CQ E2E failed (ret=-5)`.

## Tag 1 Evening to Tag 2: The Wrong Problem (Mausi Error)

**HONEST DOCUMENTATION OF MAUSI'S MISTAKE:**

Hasi reported that CQ E2E Layer 2 decrypt fails after rotation and classified it as "pre-existing bug that fails after EVERY rotation, including the first." Mausi accepted this classification WITHOUT checking it against the known fact that the first rotation worked completely (send AND receive). This was documented in the handoff and confirmed by mein Prinz.

What followed was ~8 hours of hypothesis testing against the wrong problem:

- Phase 1b key override test (comment out Phase 1b -> still ret=-5)
- Various key source tests (rq->e2e_peer_public, s_cq_e2e_peer_public, original keys)
- Hasi discovered pre-rotation messages arrive via RQ (reply_q=1), post-rotation via CQ (reply_q=0), concluded CQ pipeline "never worked"
- Queue 2 activation pursued (four stages planned)
- Haskell analysis showed app only processes Queue 1 from QADD (NonEmpty Head: `(qUri, Just addr) :| _`)
- Dispatch redirection proposed but rejected (Layer 3 happens inside handlers with wrong keys)

**The correction:** Mein Prinz intervened: "Die erste Rotation funktioniert doch die ganze Zeit!" The CQ E2E bug existed ONLY after the SECOND rotation, not after every rotation.

**Rule for the future:** Before Mausi writes any task, three questions: (1) What KNOWN works? Don't touch it. (2) What is the EXACT difference between "works" and "doesn't work"? (3) What STATE changed between the two?

## Tag 3 Morning: DIAG-Based Analysis

Instead of guessing, DIAG logging inserted showing actual values. Mausi's approach: "Don't guess, look."

Layer 3 output hex-dump revealed: pre-rotation corrId='1' (0x31) with inline SPKI key (44 bytes X25519), post-rotation corrId='0' (0x30) without inline key. This means: after rotation the app expects pre-shared keys from QADD/QKEY exchange. Phase 1b keys were correct conceptually.

Claude Code Haskell analysis (simplexmq repo) confirmed four findings: (1) e2eDhSecret uses C.dh'(rcvE2ePubDhKey, e2ePrivKey) - new keypair after rotation, (2) after QADD/QUSE the app generates new E2E keypair, dhPublicKey from QADD combined with app's new private key, (3) 0x30/0x31 encodes Maybe Nothing/Just in Haskell encoding, (4) nonce derived from msgId (C.cbNonce msgId, 24 bytes).

Result: Phase 1b keys were conceptually correct. The bug was elsewhere.

## Tag 3 Afternoon: Root Cause Found

Byte-by-byte comparison of DIAG values between first and second rotation:

**First Rotation (WORKS):**
```
e2e_priv: f0d9ef51    (Phase 1b keys from Rotation 1)
e2e_peer: 82488db3    (Peer key from QKEY Rotation 1)
DH(priv,sender): 2c6eb124   -> E2E LAYER 2 DECRYPT SUCCESS
```

**Second Rotation Phase 1b writes:**
```
DIAG our_priv: dfbc4872    (new keys from Rotation 2)
DIAG peer_pub: 8fca4b2e    (new peer key from QKEY Rotation 2)
```

**Second Rotation Decrypt USES:**
```
e2e_priv: dfbc4872    <- CORRECT (new keys)
e2e_peer: 82488db3    <- WRONG! That's Rotation 1's key!
DH(priv,sender): 932dbfa8   -> ret=-5 (wrong shared secret)
```

**Root Cause:** Cache timing in smp_tasks.c. Fix D reset s_cq_e2e_peer_valid at rotation_start(). But DURING the second rotation, CQ messages arrived (QTEST from running rotation). CQ pipeline saw valid==false, loaded OLD key (82488db3) from NVS cache, set valid=true. Phase 1b ran AFTER and wrote NEW key (8fca4b2e) to NVS and static in smp_rotation.c - but smp_tasks.c cache was already filled and never re-invalidated.

**Fix:** smp_tasks_reset_rotation_guard() called a SECOND time at the end of Phase 1b in rotation_complete(), AFTER the new peer key is written. One line in smp_rotation.c.

## Tag 3-4: Verification

Fresh device, erase-flash, complete test:
1. Contact created, bidirectional chat confirmed
2. First rotation (simplego.dev -> smp10.simplex.im): receive works, double checkmarks
3. Second rotation (smp10 -> smp8.simplex.im): receive works, double checkmarks
4. Third and fourth rotation: also successful

DIAG after second rotation shows correct peer key. E2E LAYER 2 DECRYPT SUCCESS at all rotations.

## All Fixes Summary

| Fix | Problem | Solution |
|-----|---------|----------|
| A | s_complete_logged function-local, never reset | File-static + reset API in rotation_start() |
| B | Auth/DH backup overwrites originals at 2nd rotation | Conditional: if (!has_peer_auth), first time only |
| C | rq->snd_id = RQ snd_id, should be Main Queue snd_id | rq->snd_id = rd->new_snd_id |
| D | CQ E2E peer cache never invalidated | Reset in rotation_start() |
| Phase 1b | CQ pipeline needs new E2E keys after rotation | our_queue.e2e_private = rd->new_e2e_private + peer key from QKEY |
| Cache | Phase 1b writes new key AFTER cache fill during rotation | Second reset_rotation_guard() call after Phase 1b |

## Architecture Insights

**1. Peer server never changes during Queue Rotation.** All peer-send credentials (DH keys, auth keys) must stay ORIGINAL regardless of rotation count. Backup only on first rotation (`if (!has_peer_auth)` pattern).

**2. App processes only Queue 1 from QADD.** Haskell NonEmpty Head: `(qUri, Just addr) :| _`. Tail discarded. count=2 in QADD is harmless but Queue 2 is ignored.

**3. CQ E2E uses QADD/QKEY dhPublicKey.** After rotation, app sends via Main Queue (CQ). Layer 2 uses new keys from rotation exchange. corrId='0' (Nothing) = pre-shared, no inline key. corrId='1' (Just) = inline SPKI key included.

**4. Cache invalidation must happen AFTER final key write.** Static caches can be refilled between rotation_start() and rotation_complete() by incoming messages. Invalidate TWICE: at start AND after Phase 1b.

**5. rq->snd_id semantics.** Used by rotation_build_qadd_payload() for replacedSndQueue. App's findQ() matches (SMPServer, SenderId). Must be Main Queue snd_id, not Reply Queue snd_id.

**6. SRAM budget.** 2 simultaneous TLS connections safe (~1,500 bytes each). Third causes sdmmc DMA errors. Lazy TLS pattern (open for NEW/KEY, close immediately) keeps budget.

## Haskell Reference Knowledge (from Claude Code Analysis)

Documented for SimpleGo and GoChat reference:

**X3DH:** Four DH operations (not three like Signal): DH1-DH4 using Bob/Alice Key1/Key2 pairs. Plus optional fifth secret: sntrup761 KEM (post-quantum). HKDF-SHA512, Salt = 32 zero bytes, Info = "SimpleXX3DH", Output = 96 bytes (RK + HK + NHK).

**Ratchet Encrypt:** AES-256-GCM. Message key via HKDF-SHA256 (Info = "SimpleXMK"). Chain key via HKDF-SHA256 (Info = "SimpleXCK"). Nonce: 12 bytes counter-based. Header: 2346 bytes fixed (56B X448 + 4B pn + 4B ns + padding).

**Zstd:** Level 3 (default), always active, including first request.

## GoChat Support

GoChat (github.com/saschadaemgen/GoChat) discovered during session. Technical questions answered: X448 mandatory (OID 1.3.101.111), e2eEncryption_ byte layout, X3DH 4-DH schema, HKDF-SHA512 parameters, AES-256-GCM counter-nonce, 2346-byte fixed header, Zstd level 3 always active. Layer-by-layer reference dump offered for Session 51.

## Files Changed (7 files)

| File | Changes |
|------|---------|
| smp_rotation.c | Fix A call, Fix C, Phase 1b, cache invalidation (second reset), QADD count=2 |
| smp_rotation.h | New fields: rq_peer_sender_key, rq_peer_e2e_public |
| smp_tasks.c | Fix A (file-static + reset), Fix D, CQ E2E pipeline |
| smp_tasks.h | smp_tasks_reset_rotation_guard() declaration |
| reply_queue.h | Fix B: peer_auth_private[64], peer_auth_public[32], has_peer_auth |
| smp_peer.c | Fix B: 5 locations with has_peer_auth ternary |
| smp_e2e.c | DIAG logging (added and removed) |

## Open Items for Session 51

| Priority | Task |
|----------|------|
| HIGH | Late-arrival flow (offline contacts during rotation) |
| MEDIUM | Refresh timer runs endlessly after DONE |
| MEDIUM | Legacy RQ SUB FAILED after rotation |
| MEDIUM | sdmmc DMA errors during tight SRAM rotation windows |
| LOW | smp.simplego.dev fingerprint update |
| LOW | wifi:m f null warnings at tight SRAM |
| LOW | Error handling for rotation (timeouts, retries) |
| INFO | GoChat reference dump (layer-by-layer confirmation) |

## Lessons Learned

**L271 (CRITICAL):** Never accept a bug classification from Hasi without verifying against known facts. "Fails after every rotation" contradicted the known fact that first rotation worked. One full day (~8 hours) lost. Rule: (1) What KNOWN works? (2) EXACT difference? (3) What STATE changed?

**L272 (CRITICAL):** Cache invalidation at rotation_start() is insufficient. Incoming messages during rotation (QTEST) can refill the cache with stale data. Must invalidate AGAIN after final key write in rotation_complete(). The cache timing window between start and complete is the danger zone.

**L273 (HIGH):** Peer-send credentials must stay ORIGINAL across all rotations. Backup only on first rotation (has_peer_auth/has_peer_dh guard). Unconditional backup causes ERR AUTH at second rotation because Rotation 1's keys overwrite originals.

**L274 (HIGH):** DIAG-based analysis beats hypothesis testing. "Don't guess, look." Hex-dump of actual keys going into decrypt, byte-by-byte comparison between working (first rotation) and failing (second rotation), immediately reveals the stale value (82488db3 appearing where 8fca4b2e should be).

**L275 (HIGH):** rq->snd_id must be Main Queue snd_id (rd->new_snd_id), not Reply Queue snd_id (rd->rq_new_snd_id). findQ() in Haskell matches (SMPServer, SenderId) tuples. First rotation matches by coincidence.

**L276:** App processes only Queue 1 from QADD (Haskell NonEmpty Head pattern). count=2 is harmless but irrelevant. Don't spend time on Queue 2 activation.

**L277:** corrId encoding: '0' (0x30) = Maybe Nothing = pre-shared key expected (after rotation). '1' (0x31) = Maybe Just = inline SPKI key included (before rotation). After QADD/QKEY exchange, app switches to pre-shared mode.

---

*Part 47 - Session 50: Queue Rotation Multi-Fix*
*SimpleGo Protocol Analysis*
*Date: March 22-26, 2026*
*First multiple Queue Rotation on ESP32-S3 with Post-Quantum Crypto*
*4 consecutive rotations verified on fresh device*
*6 fixes, 7 files, cache-invalidation timing as root cause*
*1 day lost on Mausi error (wrong bug classification)*
*Bugs: 81 total (rotation issues resolved)*
*Lessons: 277 total (7 new: L271-L277)*
