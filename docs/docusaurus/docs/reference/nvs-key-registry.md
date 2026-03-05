---
title: NVS Key Registry
sidebar_position: 4
---

# NVS Key Registry

All NVS (Non-Volatile Storage) keys used by SimpleGo firmware. NVS keys are limited to 15 characters by ESP-IDF. Keys are organized by namespace.

:::warning Full Erase Required
When adding or changing NVS key names or value formats, always run `idf.py erase-flash` before the next build. Stale NVS data from a previous layout causes silent failures or crashes on boot.
:::

## Namespace: `simplego`

| Key | Type | Purpose |
|-----|------|---------|
| `ratchet_N` | Blob | Double Ratchet state for contact N (0-127) |
| `contact_N` | Blob | Contact metadata for contact N |
| `contact_cnt` | U32 | Number of active contacts |
| `master_key` | Blob | AES-256 master key for SD card encryption |
| `wifi_ssid_N` | String | WiFi SSID for saved network N |
| `wifi_pass_N` | String | WiFi password for saved network N |
| `wifi_cnt` | U32 | Number of saved WiFi networks |
| `pending_new` | Blob | Persisted key pair for idempotent NEW retry |
| `device_name` | String | Display name shown in SimpleX Chat |

## Namespace: `smp`

| Key | Type | Purpose |
|-----|------|---------|
| `smp_srv` | String | SMP server hostname |
| `smp_port` | U16 | SMP server port |
| `smp_pubkey` | Blob | SMP server public key for certificate pinning |

:::note Under Construction
This registry is maintained manually. If you discover NVS keys in the source code not listed here, please add them via a pull request to keep this document complete.
:::

## PSRAM Constraint Reminder

:::danger
NVS writes must happen on the main task with internal SRAM stack. Tasks with PSRAM-allocated stacks cannot write to NVS flash. See [Pitfalls](../smp-in-c/pitfalls) for details.
:::
