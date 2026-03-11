/*
 * sntrup761.h - SimpleGo public API for sntrup761 post-quantum KEM
 *
 * Wraps PQClean's PQCLEAN_SNTRUP761_CLEAN_* functions with a clean
 * interface for the SimpleGo codebase.
 *
 * Key sizes:
 *   Public Key:    1158 bytes
 *   Secret Key:    1763 bytes
 *   Ciphertext:    1039 bytes
 *   Shared Secret:   32 bytes
 *
 * WARNING: Key generation requires ~60-70 KB stack space.
 *          Must run on the dedicated crypto task, never on app/UI tasks.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#ifndef SNTRUP761_H
#define SNTRUP761_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Key sizes ---- */

#define SNTRUP761_PUBLICKEYBYTES   1158
#define SNTRUP761_SECRETKEYBYTES   1763
#define SNTRUP761_CIPHERTEXTBYTES  1039
#define SNTRUP761_BYTES            32    /* shared secret */

/* Total bytes per contact in ratchet state: PK + SK + peer PK */
#define SNTRUP761_STATE_BYTES      (SNTRUP761_PUBLICKEYBYTES + \
                                    SNTRUP761_SECRETKEYBYTES + \
                                    SNTRUP761_PUBLICKEYBYTES)

/* ---- KEM operations ---- */

/**
 * Generate a sntrup761 keypair.
 *
 * WARNING: Uses ~60-70 KB stack. Must run on crypto task.
 *
 * @param pk  Output: public key (1158 bytes)
 * @param sk  Output: secret key (1763 bytes)
 * @return    0 on success
 */
int sntrup761_keypair(uint8_t *pk, uint8_t *sk);

/**
 * Encapsulate: generate shared secret and ciphertext from public key.
 *
 * @param ct  Output: ciphertext (1039 bytes) - send to key owner
 * @param ss  Output: shared secret (32 bytes)
 * @param pk  Input:  recipient's public key (1158 bytes)
 * @return    0 on success
 */
int sntrup761_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);

/**
 * Decapsulate: recover shared secret from ciphertext using secret key.
 *
 * @param ss  Output: shared secret (32 bytes)
 * @param ct  Input:  ciphertext (1039 bytes)
 * @param sk  Input:  own secret key (1763 bytes)
 * @return    0 on success
 */
int sntrup761_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#ifdef __cplusplus
}
#endif

#endif /* SNTRUP761_H */
