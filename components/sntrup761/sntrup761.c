/*
 * sntrup761.c - SimpleGo API wrapper for PQClean sntrup761
 *
 * Maps the clean sntrup761_*() API to PQClean's
 * PQCLEAN_SNTRUP761_CLEAN_* functions.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#include "sntrup761.h"
#include "api.h"

int sntrup761_keypair(uint8_t *pk, uint8_t *sk) {
    return PQCLEAN_SNTRUP761_CLEAN_crypto_kem_keypair(pk, sk);
}

int sntrup761_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk) {
    return PQCLEAN_SNTRUP761_CLEAN_crypto_kem_enc(ct, ss, pk);
}

int sntrup761_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk) {
    return PQCLEAN_SNTRUP761_CLEAN_crypto_kem_dec(ss, ct, sk);
}
