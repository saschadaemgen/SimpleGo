---
title: Idempotency and Lost Responses
sidebar_position: 8
---

# Idempotency and Lost Responses

> "Whatever you do for networking, make sure to handle lost responses -- that was the biggest learning. For us it was a large source of bugs."
>
> -- Evgeny Poberezkin

This is the most important operational lesson in the entire SMP implementation. It sits at the boundary between the networking layer and the protocol layer -- which is exactly why it is easy to miss.

## The Problem

Consider this sequence:

1. Client sends `NEW` (generates a key pair locally)
2. Server processes the request and sends `OK` with the queue ID
3. The response is lost in transit (wifi dropout, TCP retransmit failure, buffer overflow)
4. Client times out and retries

The naive answer: generate a new key pair and send `NEW` again. This is wrong.

The correct answer: **use the same key pair as the first attempt**.

## Why the Same Key

The server may have already processed your first `NEW`. If you retry with a new key pair, you now have a new queue with different keys -- the old queue is orphaned, and your crypto state is inconsistent with the server state.

By retrying with the same key, the operation is idempotent: if the server created the queue on the first attempt, the retry either gets `OK` again or a detectable error. Either way your local crypto state matches the server state.

## The Golden Rule
```
Generate key -> Persist to flash -> THEN send -> If response lost -> Retry with SAME key
```

**Never generate the key after a failed send. Always generate and persist before sending.**

This applies to every command that modifies server state:
- `NEW` -- persist the key pair before sending
- `KEY` -- persist the sender key before sending

## Repeat Message Delivery

The same principle applies to `ACK`. If your `ACK` is lost, the server re-delivers the message on the next `SUB`. Your application layer must be idempotent for message receipt -- receiving the same message twice must not cause duplicate display or duplicate state changes.

Store the last acknowledged message ID per queue. If a received message ID matches the stored ID it is a re-delivery -- discard silently.

## Implementation Pattern
```c
// WRONG -- key generated after failed send
smp_result_t result = smp_send_new(conn, &response);
if (result == SMP_TIMEOUT) {
    // generate new key and retry -- THIS IS A BUG
}

// CORRECT -- key generated and persisted before send
smp_generate_key(&key_pair);
nvs_write_key("pending_new_key", &key_pair);  // persist first
smp_result_t result = smp_send_new(conn, &key_pair, &response);
if (result == SMP_TIMEOUT) {
    nvs_read_key("pending_new_key", &key_pair);  // load persisted key
    result = smp_send_new(conn, &key_pair, &response);  // retry with same key
}
// on success: clear the pending key from NVS
nvs_delete_key("pending_new_key");
```
