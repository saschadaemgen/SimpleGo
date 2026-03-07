---
slug: /protocol-analysis
sidebar_position: 5
title: Protocol Analysis
---

# Protocol Analysis

The complete development log of the world's first native C implementation of the SimpleX Messaging Protocol. 43 sessions documenting every breakthrough, every bug, and every byte-level discovery from the reverse-engineering process.

This is not sanitized documentation -- it is the raw engineering journal of building an SMP client from scratch on an ESP32-S3 microcontroller, verified byte-for-byte against the official Haskell reference implementation.

## Why This Matters

SimpleGo is the first external implementation of the SimpleX Messaging Protocol. The official codebase is written in Haskell. Everything documented here was reverse-engineered from source code analysis, wire captures, and trial-and-error on bare-metal hardware. This knowledge exists nowhere else.

## Quick Navigation

- [Index](/protocol-analysis/session-index) -- complete session overview
- [Status](/protocol-analysis/STATUS) -- current project status
- [Quick Reference](/protocol-analysis/QUICK_REFERENCE) -- essential protocol details at a glance
- [Bug Tracker](/protocol-analysis/BUG_TRACKER) -- all discovered bugs and their resolution

## Sessions

The protocol analysis spans 43 sessions from initial research through production-ready alpha firmware. Each session documents specific technical challenges, discoveries, and solutions encountered while implementing SMP in C.

Key milestones include the TLS 1.3 handshake (Sessions 1-4), SMP queue operations (Sessions 5-9), the cryptographic layer breakthroughs (Sessions 10-20), Double Ratchet implementation (Sessions 21-30), and production hardening (Sessions 31-43).
