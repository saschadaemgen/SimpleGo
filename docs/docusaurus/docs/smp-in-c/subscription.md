---
title: Subscription and Keep-Alive
sidebar_position: 9
---

# Subscription and Keep-Alive

> "Subscription can only exist in one socket though. Do you keep them alive? Do you use keep-alive?"
>
> -- Evgeny Poberezkin

## One Socket Per Subscription

A queue subscription exists on exactly one TCP socket. If you subscribe to the same queue from a second socket, the server sends `END` to the first socket and moves the subscription to the second.

There is no multi-socket subscription. This is a protocol constraint, not a server limitation.

## NEW Creates Subscribed

`NEW` creates the queue already subscribed. Calling `SUB` after `NEW` is a no-op -- except that it re-delivers the last message if it was not acknowledged. You do not need to call `SUB` after `NEW`.

## Keep-Alive is Essential

Without keep-alive, the server considers an idle connection dead and drops the subscription. The client sees no error. Incoming messages queue on the server but never arrive. The device appears functional but receives nothing.
```c
#define SMP_KEEPALIVE_INTERVAL_S  30
#define SMP_KEEPALIVE_TIMEOUT_S   10

// Send PING every 30 seconds on the main SSL connection
// Server responds with PONG
// If no PONG within 10 seconds: reconnect
```

:::warning Silent Failure
A subscription lost due to missing keep-alive produces no error on the client side. The only symptom is that messages stop arriving. Always implement PING/PONG before considering the receive path working.
:::

## Session Validation on Reconnect

When the client reconnects after a timeout or network interruption:

1. Client establishes a new TLS connection
2. Client resubscribes to all active queues on the new socket
3. The server sends `END` to the old (now dead) socket

**The client must ignore `END` frames that arrive on the old socket after the new connection is established.** An `END` on a stale socket is expected cleanup -- not a protocol error.

> "reconnection must result in END to the old connection, so you must do the same session validation as haskell code does - it's quite possible that client reconnects on timeout, the old socket then receives END that must be ignored because the client already has the new connection."
>
> -- Evgeny Poberezkin

In C: maintain a session ID per connection. Discard any `END` frame whose session ID does not match the current active session.

## Concurrency

> "concurrency is hard."
>
> -- Evgeny Poberezkin

In a multi-task FreeRTOS architecture, subscription management is the most likely source of hard-to-reproduce bugs. The symptoms -- missing messages, spurious `END` frames, subscriptions that silently die -- are identical to hardware or network problems.

Ensure that subscription state is protected by a mutex and that only one task ever holds an active subscription to any given queue. Never subscribe to the same queue from two different FreeRTOS tasks.
