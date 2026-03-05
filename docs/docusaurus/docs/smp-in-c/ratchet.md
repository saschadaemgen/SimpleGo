---
title: Double Ratchet in C
sidebar_position: 7
---

# Double Ratchet in C

## State Storage

The Double Ratchet state for all contacts is stored as a static array in PSRAM:
```c
ratchet_state_t ratchet_states[128]; // ~68 KB in PSRAM
```

All 128 contact states are resident in PSRAM at all times. Zero-latency contact switching -- no NVS swap, no SD card read.

## Persistence

Ratchet state must survive power cycles. After every state advance (every sent or received message), the updated state is written to NVS flash.

:::danger PSRAM Task Stack Constraint
Tasks with PSRAM-allocated stacks cannot write to NVS flash. This is a silicon-level SPI Flash cache conflict on ESP32-S3 -- not a bug, not configurable.

Any code path that advances the ratchet state and writes to NVS must execute on the main task with an internal SRAM stack.
:::

## State Advance

The Double Ratchet advances in two dimensions:

**Sending:** Each sent message advances the sending chain. The chain key is updated, a new message key is derived, the old message key is used once and discarded.

**Receiving:** Each received message may trigger a DH ratchet step if the sender used a new DH key, followed by a receiving chain advance.

Every incoming `MSG` and every outgoing `SEND` must:
1. Acquire the ratchet state for the correct contact
2. Perform the advance
3. Write the new state to NVS flash
4. Release the state

## Branch Switching

When switching firmware branches or after any crypto state change, erase flash before rebuilding:
```powershell
idf.py erase-flash
idf.py build flash monitor -p COM6
```

Stale NVS ratchet state from a previous branch causes silent decryption failures that are difficult to diagnose. Always erase when in doubt.

## Key Reference (Haskell)

| Function | File | Lines |
|----------|------|-------|
| Queue creation, both key pairs | `Agent/Client.hs` | 1449-1486 |
| Step 1 decryption, server layer | `Agent/Client.hs` | 1931-1935 |
| Step 3 decryption, per-queue E2E | `Agent.hs` | 3261-3272 |
| Subscription machinery | `Agent.hs` | every line |
| Client subscription | `Agent/Client.hs` | every line |
