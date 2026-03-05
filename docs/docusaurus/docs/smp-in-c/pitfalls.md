---
title: Pitfalls
sidebar_position: 10
---

# Implementation Pitfalls

Everything that will silently break a C implementation of SMP. Each entry here was discovered during SimpleGo development.

## Crypto Pitfalls

**Wrong curve for E2E ratchet.** The Double Ratchet uses X448, not X25519. Using X25519 produces key agreement that appears to work locally but fails to decrypt messages from the Haskell reference implementation. No error -- just garbage plaintext.

**Hardware AES enabled.** `CONFIG_MBEDTLS_HARDWARE_AES=n` must be explicitly set. The default is enabled. With PSRAM, hardware AES DMA produces memory corruption that manifests as intermittent crypto failures, not a clean crash.

**cbNonce off by one.** The 24-byte nonce for per-queue NaCl is constructed from the message ID. Getting the byte layout wrong produces silent decryption failure -- no error, just garbage plaintext.

**Not persisting keys before sending.** See [Idempotency](./idempotency). Generating a new key after a failed send instead of reusing the persisted key creates irrecoverable crypto state divergence between client and server.

**Stale NVS ratchet state after branch switch.** Always run `idf.py erase-flash` when switching branches or changing crypto state layout. Old ratchet state in NVS silently corrupts the Double Ratchet on the next boot.

**E2E key not in PHConfirmation.** When implementing the handshake, the E2E key for the Reply Queue is inside the `PHConfirmation` message payload -- not transmitted separately. Looking for it elsewhere is the bug.

## Networking Pitfalls

**No keep-alive.** The subscription silently dies on idle connections. See [Subscription](./subscription). This is the most common reason a device appears to work but never receives messages.

**Not handling partial reads.** LwIP on ESP32 does not guarantee that a single read returns a complete SMP frame. Always buffer until the full length-prefixed frame is available before parsing.

**Subscribing from multiple sockets.** The second subscription sends `END` to the first socket. Symptoms look like the subscription randomly drops for no reason.

**Not ignoring stale END frames.** After reconnection, `END` arrives on the old socket. If not ignored, it triggers unnecessary resubscription that can create a reconnection loop.

**Calling SUB after NEW.** Not technically wrong, but SUB after NEW re-delivers the last unacknowledged message unexpectedly. Can cause duplicate message processing if not handled.

## PSRAM and NVS Pitfalls

**NVS writes from PSRAM task stack.** Tasks with PSRAM-allocated stacks cannot write to NVS flash. The write silently fails or crashes. NVS writes must happen on the main task with internal SRAM stack.

**SPI2 mutex held during crypto.** The SPI2 bus is shared between display and SD card. If you hold the LVGL mutex while performing crypto operations, mutex hold time reaches 500ms or more. This causes display stuttering and watchdog resets. Complete all crypto operations before acquiring the mutex.

## WiFi Pitfalls

**Wrong WPA3 auth threshold.** Use `WIFI_AUTH_WPA2_PSK` as the auth threshold -- not `WIFI_AUTH_WPA_WPA2_PSK`. The latter causes WPA3 SAE authentication failures on networks that support WPA3.

## Protocol Pitfalls

**Wrong encryption layer order.** Layers must be applied in the correct order on send and unwrapped in reverse on receive. Double Ratchet (Layer 1) is innermost, TLS (Layer 4) is outermost.

**Not padding to 16KB.** Content padding to 16,384 bytes fixed block size is required at each encryption layer. Sending unpadded messages causes decryption failure on the server side.

**HISTORY_MAX_TEXT changed without approval.** `HISTORY_MAX_TEXT` = 4096 controls the SD storage and receive path. Changing it without explicit approval causes permanent data loss on existing SD card history. Never change this constant without understanding the full write path.
