![SimpleGo Protocol Analysis](../../.github/assets/github_header_protocol_analysis.png)

# SimpleX Protocol Analysis - Part 8: Session 11
# Format Experiments Regression, Git Recovery, cmNonce Fix

**Document Version:** v2 (rewritten for clarity, March 2026)
**Date:** 2026-01-30
**Status:** COMPLETED -- Recovered to working state after regression
**Previous:** Part 7 - Session 10
**Next:** Part 9 - Session 12
**Project:** SimpleGo - ESP32 Native SimpleX Client
**License:** AGPL-3.0

---

## SESSION 11 SUMMARY

```
Session 11 documented a regression caused by unnecessary format
experiments, and the recovery via git reset. Starting from a working
state (Bug #17 solved with cmNonce fix, app showing "connecting"),
five format experiments broke the connection: RATCHET_VERSION 2->3,
Maybe tag ASCII->binary, version encoding byte swap, DH order swap,
and payload AAD size changes. All were wrong. Circular debugging was
recognized and stopped. Git checkout restored clean state, the cmNonce
fix (TEST4) was re-applied, and "connecting" status was restored. A
key discovery was made: Reply Queue messages have maybe_e2e = ','
(Nothing), meaning no per-queue E2E layer and direct Double Ratchet.

 5 Format experiments conducted (all wrong)
 Circular debugging pattern recognized
 Git reset + cmNonce re-apply restored working state
 Reply Queue: maybe_e2e = Nothing (direct Double Ratchet)
```

---

## Format Experiments (All Wrong)

Starting from the working state after Bug #17 (cmNonce fix, app "connecting"), five experiments were tried:

**RATCHET_VERSION 2 to 3:** Changed em_header from 123 to 124 bytes, ehBody length prefix from 1 byte to 2 bytes, emHeader length from 0x7B to 0x7C. Result: regression to "request to connect".

**Maybe tag encoding:** Changed from ASCII `'1'` (0x31) to binary `0x01`. Haskell uses ASCII: `smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)`. Nothing = '0' (0x30), Just = '1' (0x31). Result: app parse failure.

**Version encoding:** Changed from Word16 BE `00 02` to two separate bytes `02 02`, producing version 514 instead of version 2. Result: wrong version.

**DH order swap:** Swapped dh1/dh2 in X3DH. Result: wrong shared secrets.

**Payload AAD size:** Toggled between 236 and 238 bytes. Result: no improvement.

---

## Circular Debugging

The experiments produced a circular pattern: change A failed, try B, B failed, revert to A, A failed again, try C, revert, repeat. The same changes were applied and reverted multiple times. The Python crypto tests had already proven the cryptography was correct, making format experiments unnecessary.

### Recovery

```powershell
cd /mnt/c/Espressif/projects/simplex_client
git checkout -- main/
```

The only necessary change was the cmNonce fix (Bug #17, "TEST4"):

```c
// WRONG: Using msgId as nonce
memcpy(nonce, msg_id, msgIdLen);

// CORRECT: Using cmNonce from ClientMsgEnvelope at offset 60
int cm_nonce_offset = spki_offset + 44;  // [60-83]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
```

After git reset + cmNonce re-apply: TEST4 succeeded, 15904 bytes decrypted (ClientMessage), PrivHeader tag 'K' (PHConfirmation), app status "connecting".

---

## Verified Working Code State

```c
// smp_ratchet.c
#define RATCHET_VERSION         2       // Version 2
uint8_t em_header[123];                 // 123 bytes for v2
em_header[hp++] = 0x00;
em_header[hp++] = RATCHET_VERSION;      // ehVersion (Word16 BE)
em_header[hp++] = 0x58;                 // ehBody-len = 88 (1 byte for v2)
output[p++] = 0x7B;                     // emHeader len = 123

// smp_peer.c
agent_msg[amp++] = '1';                 // ASCII '1' (0x31) for Maybe Just

// smp_x448.c
output[offset++] = 0x00;                // HIGH byte
output[offset++] = params->version_min; // LOW byte = 0x02
// Result: 00 02 = Version 2
```

### ClientMsgEnvelope Structure (from cmNonce fix)

```
[0-1]    Length prefix
[12-13]  Version
[14]     Maybe tag
[15]     Maybe tag for e2ePubKey
[16-59]  X25519 SPKI (44 bytes)
[60-83]  cmNonce (24 bytes)
[84+]    cmEncBody
```

---

## Reply Queue Discovery

Reply Queue messages have `maybe_e2e = ','` (Nothing), meaning no separate e2ePubKey and no per-queue E2E layer. These messages go directly into Double Ratchet decryption.

| Queue | Maybe Tag | Meaning |
|-------|-----------|---------|
| Contact Queue | '1' (Just) | Has e2ePubKey, per-queue E2E layer present |
| Reply Queue | ',' (Nothing) | No e2ePubKey, direct Double Ratchet |

This means the next step is implementing Double Ratchet receiver-side decryption (header decrypt with `header_key_recv`, then payload decrypt with derived message key).

---

## Lessons

1. If it works, commit immediately and do not experiment with format changes.
2. Git reset is faster than manual reverting of multiple interleaved changes.
3. If crypto tests pass, the crypto is not the problem.
4. Recognize circular debugging (same changes applied repeatedly) and stop.

---

*Part 8 - Session 11: Format Experiments Regression & Recovery*
*SimpleGo Protocol Analysis*
*Original date: January 30, 2026*
*Rewritten: March 4, 2026 (v2)*
*Regression recovered, cmNonce fix preserved, Reply Queue direct Ratchet discovered*
