# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| latest  | :white_check_mark: |

SimpleGo is in active pre-release development. Security updates are applied
to the latest version on the main branch only.

## Reporting a Vulnerability

If you discover a security vulnerability in SimpleGo, please report it
privately using GitHub's **Private Vulnerability Reporting** feature on
this repository. Do NOT open a public issue for security vulnerabilities.

**What to expect:**
- Acknowledgment within 72 hours
- Status update within 7 days
- Coordinated disclosure after a fix is available

**What to include:**
- Description of the vulnerability
- Steps to reproduce (if applicable)
- Affected component (firmware, protocol, hardware)
- Impact assessment

## Scope

SimpleGo is a hardware-secured encrypted messenger implementing the
SimpleX Messaging Protocol (SMP). Security-relevant areas include:
- Cryptographic implementation (Double Ratchet, NaCl, AES-256-GCM)
- Key storage and secure element integration
- SD card encrypted history
- NVS credential storage
- SMP protocol handling
- Memory safety in embedded C (ESP-IDF / FreeRTOS)

## Contact

For non-vulnerability security questions, open a regular GitHub issue.
