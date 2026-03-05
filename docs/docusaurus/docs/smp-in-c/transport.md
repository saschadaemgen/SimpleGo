---
title: Transport Layer
sidebar_position: 3
---

# Transport Layer

## The Two-Router Delivery Path

Every SMP message travels a two-router path. The sending endpoint submits a packet to a first router, which forwards it to a second router, where the receiving endpoint retrieves it. Each endpoint connects to one router; the routers relay between each other.

This architecture has direct implications for encryption. There are separate NaCl encryption layers for each leg of the journey -- Layer 2 protects the sender-to-destination-router leg, Layer 3 protects the destination-router-to-recipient leg. If TLS on one leg is compromised, the other leg remains protected. Neither router ever sees the plaintext.

## TLS 1.3

All SMP communication uses TLS 1.3. In SimpleGo this is implemented with mbedTLS 4.x over the ESP-IDF LwIP network stack.

The SMP server presents a self-signed certificate. The client must accept it -- SMP uses certificate pinning at the application layer via the server public key in the connection string, not at the TLS layer. Do not configure mbedTLS to reject self-signed certificates.

## Block Framing

SMP uses a length-prefixed binary framing. Every message on the wire is:
```
[4-byte big-endian length][payload bytes]
```

The length field encodes the number of payload bytes that follow. Read exactly that many bytes. Do not assume a single TCP read returns a complete frame -- on embedded hardware with LwIP, partial reads are common. Buffer until you have the full frame.

## The 16 KB Hard Limit

:::danger Hard Limit
The SMP transport block hard limit is **16,384 bytes**. No single message or command can exceed this size. There is no chunking. There is no XFTP fallback for text messages.
:::

All stream packets are this fixed size. Uniform packet size is not arbitrary -- it prevents traffic analysis based on message length. Content padding is applied at each encryption layer to reach this fixed block size. This is a protocol requirement, not optional.

The constant `HISTORY_MAX_PAYLOAD` = 16000 leaves 384 bytes of headroom for framing and encryption overhead.

## Packet Buffering

SimpleX routers buffer packets for hours to days, enabling asynchronous delivery between endpoints online at different times. Packets are removed from the router after delivery or after a configured expiration period.

For embedded hardware this means: if the device is offline, messages accumulate on the server. When the device reconnects and resubscribes, the server delivers the buffered messages. Your receive loop must handle a burst of queued messages on reconnect.

## Connection Lifecycle

1. Establish TCP connection to SMP server (default port 5223)
2. Complete TLS 1.3 handshake
3. Send SMP protocol handshake (version negotiation)
4. Begin command/response exchange

One TCP connection carries all active queue subscriptions. There is no per-contact connection.

## Keep-Alive

The server closes idle connections. Without keep-alive, long-running subscriptions silently die.

Send a PING every 30 seconds on the main SSL connection. The server responds with PONG. If no PONG arrives within the timeout, reconnect.

:::warning Subscription Lost Without Keep-Alive
A dead connection causes the subscription to silently drop. Incoming messages queue on the server but never arrive. The device appears to work but receives nothing. This is not obvious from the device side.
:::

## mbedTLS on ESP32-S3

:::danger Hardware AES Must Be Disabled
Set `CONFIG_MBEDTLS_HARDWARE_AES=n` in sdkconfig. The ESP32-S3 hardware AES DMA conflicts with PSRAM at the silicon level. This is not configurable. Software AES is used throughout -- it is fast enough for all SMP traffic volumes.
:::
