# SimpleGo Python Verification Tests

Python scripts for verifying cryptographic implementations against ESP32 code.
These tests use **exact values from ESP32 logs** to identify implementation bugs.

## Requirements

```bash
pip install pynacl cryptography
```

## Test Categories

### 1. X3DH Key Agreement Tests

| File | Description | Status |
|------|-------------|--------|
| `test_x3dh.py` | Basic X448 DH calculations | ✅ PASS |
| `test_app_x3dh.py` | X3DH from app (receiver) perspective | ✅ PASS |
| `test_new_x3dh.py` | X3DH with swapped DH order | ✅ PASS |
| `test_swapped.py` | DH order swap verification | ✅ PASS |
| `test_current.py` | Current session X3DH test | ✅ PASS |
| `test_simplex_crypto.py` | Full X3DH with byte reversal | ✅ PASS |
| `test_full_crypto.py` | Complete X3DH + KDF chain | ✅ PASS |

### 2. KDF Tests (Key Derivation Functions)

| File | Description | Status |
|------|-------------|--------|
| `test_hkdf.py` | X3DH HKDF output verification | ✅ PASS |
| `test_rootkdf.py` | Root ratchet KDF | ✅ PASS |
| `test_root_kdf.py` | Root KDF with full key computation | ✅ PASS |
| `test_chainkdf.py` | Chain ratchet KDF | ✅ PASS |

### 3. AES-GCM Header Encryption Tests

| File | Description | Status |
|------|-------------|--------|
| `test_aes_gcm.py` | AES-GCM encrypt/decrypt verification | ✅ PASS |
| `test_header_encrypt.py` | MsgHeader encryption | ✅ PASS |
| `test_header_decrypt.py` | MsgHeader decryption | ✅ PASS |
| `test_full_decrypt.py` | Full message decryption | ✅ PASS |
| `test_receiver.py` | Receiver-side ratchet analysis | ✅ PASS |

### 4. Per-Queue E2E Decryption Tests (BUG #18)

| File | Description | Status |
|------|-------------|--------|
| `test_e2e_decrypt.py` | Reply Queue E2E decrypt (Session 14) | ❌ ALL FAIL |
| `test_reply_queue_e2e.py` | Reply Queue E2E (alternative) | ❌ ALL FAIL |

## Key Findings

### What Works ✅
- X3DH key agreement matches between ESP32 and Python
- HKDF derivations match (root KDF, chain KDF)
- AES-GCM header encryption/decryption works
- Double Ratchet key derivation is correct

### What Doesn't Work ❌
- **Reply Queue per-queue E2E decryption** (BUG #18)
- All crypto methods tested (raw DH, crypto_box, MAC reorder) fail
- **Conclusion:** Problem is key/structure, NOT crypto implementation

## Usage

Run individual tests:
```bash
python test_x3dh.py
python test_aes_gcm.py
python test_e2e_decrypt.py
```

Run all tests:
```bash
for f in test_*.py; do echo "=== $f ===" && python "$f" && echo; done
```

## How to Add New Tests

1. Copy exact hex values from ESP32 log
2. Create new `test_<feature>.py` file
3. Compare Python output with ESP32 output
4. Update this README with results

## Test Data Sources

All test data comes from ESP32 monitor logs:
- Keys: `our_key1_priv`, `peer_key1`, etc.
- DH outputs: `dh1`, `dh2`, `dh3`
- KDF outputs: `hk`, `rk`, `ck`, etc.
- Encrypted data: `encrypted_header`, `header_tag`, etc.

## Session History

| Session | Tests Added | Result |
|---------|-------------|--------|
| 8-12 | X3DH, KDF, AES-GCM | ✅ All crypto verified |
| 13 | Reply Queue E2E initial | ❌ Key extraction issues |
| 14 | Reply Queue E2E complete | ❌ All methods fail |

## Conclusion

The Python verification tests confirm that:
1. **X3DH, KDF, and AES-GCM implementations are CORRECT**
2. **Reply Queue E2E problem is NOT a crypto bug**
3. **The issue is likely key identification or message structure**

See `BUG18_SESSION14_FORTSETZUNG.md` for detailed analysis.
