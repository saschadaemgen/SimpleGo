/**
 * SimpleGo - smp_frame_pool.h
 * Static frame pool for zero-copy inter-task communication interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef SMP_FRAME_POOL_H
#define SMP_FRAME_POOL_H

#include "smp_events.h"

// Pool configuration: 4 frames x 4KB = 16KB
#define SMP_FRAME_POOL_SIZE 4

// Initialize the frame pool (call once at startup)
void smp_frame_pool_init(void);

// Allocate a frame from the pool (returns NULL if exhausted)
smp_frame_t *smp_frame_pool_alloc(void);

// Return a frame to the pool
void smp_frame_pool_free(smp_frame_t *frame);

// Get number of available frames in pool
int smp_frame_pool_available(void);

#endif // SMP_FRAME_POOL_H
