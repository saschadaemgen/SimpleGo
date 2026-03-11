/*
 * sntrup761_test.h - Standalone verification test for sntrup761
 *
 * Call sntrup761_run_test() to verify the component works correctly.
 * Creates its own 80 KB task internally - safe to call from app_main.
 *
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * License: AGPL-3.0
 */

#ifndef SNTRUP761_TEST_H
#define SNTRUP761_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run sntrup761 KEM verification test.
 *
 * Creates a dedicated 80 KB task, runs keygen + encap + decap,
 * verifies shared secrets match, logs timing, then cleans up.
 *
 * Safe to call from any task - manages its own stack.
 *
 * @return 0 on PASS, >0 on specific failure, <0 on infrastructure error
 */
int sntrup761_run_test(void);

#ifdef __cplusplus
}
#endif

#endif /* SNTRUP761_TEST_H */
