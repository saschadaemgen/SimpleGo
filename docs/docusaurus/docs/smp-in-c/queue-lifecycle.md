---
title: Queue Lifecycle
sidebar_position: 4
---

# Queue Lifecycle

A SimpleX queue is a unidirectional message channel between two parties. Every contact requires two queues: one for each direction. Queues have no persistent identity -- they are ephemeral by design.

## Commands Overview

| Command | Direction | Purpose |
|---------|-----------|---------|
| `NEW` | Client -> Server | Create a new queue |
| `KEY` | Client -> Server | Set the sender public key on the queue |
| `SUB` | Client -> Server | Subscribe to receive messages |
| `SEND` | Client -> Server | Send a message to a queue |
| `ACK` | Client -> Server | Acknowledge receipt of a message |
| `MSG` | Server -> Client | Deliver a message to a subscriber |
| `OK` | Server -> Client | Command acknowledged |
| `ERR` | Server -> Client | Command failed |
| `END` | Server -> Client | Subscription ended |

## NEW -- Create a Queue

`NEW` creates a queue on the server. The server returns a queue ID and connection parameters.

:::warning NEW Creates Subscribed
`NEW` creates the queue already subscribed by default. A subsequent `SUB` command is a no-op -- except that it re-delivers the last unacknowledged message if one exists. You do not need to call `SUB` after `NEW`.
:::

The queue is created with two key pairs: the recipient key pair and the sender key pair. Both are generated locally before sending `NEW`. See [Encryption](./encryption) for key generation details.

**Persist the key pair before sending `NEW`.** If the response is lost, you must retry with the same key. See [Idempotency](./idempotency).

## KEY -- Set Sender Key

`KEY` associates a sender public key with a queue. Called by the sender after receiving the queue connection parameters out-of-band via the invitation link.

## SUB -- Subscribe

`SUB` subscribes to message delivery on a queue. After a successful `SUB`, the server pushes `MSG` frames to this connection as messages arrive.

**One socket per queue subscription.** If you subscribe from a second socket, the first socket receives `END`. There is no multi-socket subscription for a single queue.

## SEND -- Send a Message

`SEND` delivers an encrypted message to a queue. The message must be encrypted before sending. The server stores the encrypted blob and delivers it to the subscriber.

## ACK -- Acknowledge

`ACK` tells the server the message was received and processed. The server deletes the message after acknowledgment. If you do not `ACK`, the server re-delivers on the next `SUB`.

## MSG -- Receive a Message

`MSG` frames are pushed by the server to the subscribed connection without solicitation. They arrive whenever a sender delivers a message, interleaved with responses to commands you sent. Your receive loop must handle `MSG` frames at any time.

## The State Machine
```
[Not Created]
     | NEW
     v
[Created + Subscribed]
     | KEY (sender side)
     v
[Ready]
     |
     v (MSG arrives)
[Message Pending]
     | ACK
     v
[Ready]
```

## Delivery Receipts

After the recipient ACKs a message, a delivery receipt (`MsgDelivered`) is sent back through the reply queue. The sender displays a double checkmark. This requires both queues to be fully established and operational.
