# Security Architecture

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

## Threat Model

| Adversary | Capabilities | Target Tier |
|-----------|--------------|-------------|
| **Casual** | Physical access, no tools | Tier 1 |
| **Skilled** | Lab equipment, software exploits | Tier 2 |
| **Professional** | Advanced lab, custom tools | Tier 2-3 |
| **State-Level** | Unlimited resources, supply chain | Tier 3 |

### Attack Vectors

- **Network**: MITM, server impersonation, traffic analysis
- **Physical**: Device theft, probing, voltage glitching, cold boot
- **Software**: Firmware bugs, malicious updates, debug interfaces
- **Supply Chain**: Backdoored components, counterfeits

---

## Security Layers

### Layer 1: Software Protections (All Tiers)

**Secure Boot Chain:**
```
ROM Bootloader (Immutable)
    │ Verify signature (RSA-3072 / ECDSA)
    ▼
First-Stage Loader
    │ Verify signature
    ▼
Application Firmware
    │ Verify with Secure Element
    ▼
Runtime Integrity Checks
```

**Flash Encryption:**
- ESP32-S3: AES-256-XTS (key in eFuse)
- STM32U5: OTFDEC (on-the-fly decryption)

### Layer 2: Hardware Root of Trust (Tier 1+)

**Key Hierarchy:**
```
IDENTITY KEY (Slot 0) ← Never leaves SE
    ├── TLS Key (Slot 1)
    ├── X3DH Key (Slot 2)
    └── Signing Key (Slot 3)
            │
            ▼
    RATCHET KEYS (Ephemeral) ← Derived via ECDH in SE
```

**Operations in Secure Element:**
| Operation | Data Exported |
|-----------|---------------|
| Key Generation | Public key only |
| ECDH Key Agreement | Shared secret |
| TLS Client Auth | Signature only |
| Message Signing | Signature only |

### Layer 3: Tamper Detection (Tier 2+)

**Sensors:**
- Light sensor → Enclosure opening
- PCB tamper mesh → Drilling/cutting
- Case switch → Physical tampering

**DS3645 Tamper Supervisor (Tier 3):**
- 8 external tamper inputs
- Voltage monitoring (glitch detection)
- Temperature (rate-of-change)
- Clock frequency monitoring
- Sub-microsecond zeroization output

### Layer 4: Physical Protection (Tier 3)

**Potting:**
- Aluminum-filled epoxy
- Drilling creates conductive debris → short circuits
- Chemical resistant to common solvents

**Package Selection:**
- BGA preferred (hidden solder balls)
- QFN acceptable (leads under package)
- Avoid LQFP (exposed leads, easy probing)

---

## Cryptographic Architecture

| Function | Algorithm | Key Size |
|----------|-----------|----------|
| Key Agreement | X25519 | 256-bit |
| Signing | Ed25519 | 256-bit |
| Symmetric | ChaCha20-Poly1305 | 256-bit |
| Key Derivation | HKDF-SHA512 | 512-bit |
| SE Operations | ECDSA-P256 | 256-bit |

### Double Ratchet with SE

```
Receive peer's DH key
    │
    │ ECDH(my_priv, peer_pub) ──► [SECURE ELEMENT]
    │                                    │
    │◄─────── shared_secret ─────────────┘
    │
HKDF(root_key, shared_secret)
    │
    ▼
new_root_key, chain_key
    │
    ▼
message_key = KDF(chain_key)
    │
    ▼
Encrypt/Decrypt with ChaCha20-Poly1305
```

---

## Emergency Procedures

### Zeroization Triggers

**Automatic:**
1. Tamper mesh break
2. Light sensor activation
3. Temperature out of range
4. Voltage anomaly
5. Multiple failed PIN attempts

**Manual:**
1. "Brick Me" PIN
2. User-initiated secure wipe
3. Dead man's switch timeout

### Zeroization Sequence

```
T+0.0µs   Tamper event detected
T+0.1µs   DS3645 asserts ZERO output
T+0.2µs   ├── ATECC608B erased
T+0.3µs   ├── OPTIGA Trust M reset
T+0.4µs   ├── SE050 keys destroyed
T+0.5µs   └── Battery-backed SRAM cleared
T+1.0µs   All key material destroyed

Supercapacitor ensures completion even if power cut.
```

### Duress Mode (Tier 3)

```
Normal PIN:  123456  →  Normal operation
Duress PIN:  654321  →  Duress mode
    - Display decoy contacts/messages
    - Silent alert to trusted contact
    - Real data remains encrypted
```

---

## Security Comparison

| Aspect | Smartphone | SimpleGo Tier 2 | SimpleGo Tier 3 |
|--------|------------|-----------------|-----------------|
| OS Attack Surface | Millions LOC | <50k LOC | <50k LOC |
| Key Storage | Software/TEE | Hardware SE | Triple SE |
| Tamper Detection | None | Basic | Advanced |
| Physical Security | None | Moderate | High |
| Baseband Risk | High | None | Isolated |
