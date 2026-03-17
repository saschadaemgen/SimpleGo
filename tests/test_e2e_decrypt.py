#!/usr/bin/env python3
"""
SimpleGo Session 15 - E2E Decrypt Verification
Tests the XSalsa20-Poly1305 decryption with raw DH secret

USAGE:
1. Export full ciphertext from ESP32 (all 16022 bytes of cmEncBody)
2. Replace the truncated data below with full data
3. Run: python3 test_e2e_decrypt.py
"""

from nacl.bindings import (
    crypto_scalarmult,
    crypto_secretbox_open,
    crypto_secretbox_NONCEBYTES,
    crypto_secretbox_MACBYTES
)
from nacl.secret import SecretBox
from nacl.exceptions import CryptoError
import binascii

# =============================================================================
# TEST DATA FROM SESSION 14/15
# =============================================================================

# Our E2E private key (from ESP32)
OUR_E2E_PRIVATE = bytes.fromhex(
    "83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa"
)

# Peer's ephemeral E2E public key (from message header at offset 28)
PEER_E2E_PUBLIC = bytes.fromhex(
    "9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300"
)

# Expected DH secret (verified in Session 14)
EXPECTED_DH_SECRET = bytes.fromhex(
    "d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810"
)

# Nonce (24 bytes, from offset 60)
CM_NONCE = bytes.fromhex(
    "b21fa2bc0dbb5cb02d674dedfd65b0e6ff0fcf793791fd3b"
)

# MAC (16 bytes, from offset 84)
MAC = bytes.fromhex(
    "cc3eec548b0440cf0222466a79a00c0c"
)

# cmEncBody (MAC + Ciphertext) - TRUNCATED! Need full 16022 bytes!
# This is just the first ~300 bytes - replace with full data from ESP32
CM_ENC_BODY_TRUNCATED = bytes.fromhex(
    "cc3eec548b0440cf0222466a79a00c0c"  # MAC (16 bytes)
    "5bf22efa6d8ea4bb7af503d9564546ef"  # Ciphertext starts here
    "4ed1d15b7c0f2306e80d7825df3696fd"
    "c2a3bafe55cd04dae63cb942cb610765"
    "72234a354ca5162b2da46d8c952886a3"
    "636172b87f4fc6025fcb9a44bd150419"
    "1856bb9065da72efeb4ca2320e6f9480"
    "6280917cef8abfce1057a0fd0686cb1a"
    "d342672c795132627e9e970d0105b94c"
    "ff34dc6a74bac17ebc50e6e4f6144019"
    "02c5e6fcd7dd108aae91aa2978763ab9"
    "8b2d5dfaea5ca39c9c7f79ef8347ae6e"
    "0d4f76fddf2618e71c618e124274c955"
    "4076d75a9cb2a40df657c2b49f71d6f1"
    "ec532c729a094d58f8f45f2fa080fd1c"
    "8b67aa5140f820336be9f881a833dc60"
    "c09ce00c24ac2f2755f28f7a549b9d36"
    "f90a77195584fdb51a8edb5b3b5f3b19"
    "4dff49871d008f9992ef4a11e1364d8e"
)

# =============================================================================
# STEP 1: Verify DH Secret Computation
# =============================================================================
def test_dh_secret():
    """Verify that DH secret matches expected value"""
    print("=" * 60)
    print("STEP 1: DH Secret Verification")
    print("=" * 60)
    
    # Raw X25519 DH (no HSalsa20!)
    computed_dh = crypto_scalarmult(OUR_E2E_PRIVATE, PEER_E2E_PUBLIC)
    
    print(f"Our private:     {OUR_E2E_PRIVATE.hex()}")
    print(f"Peer public:     {PEER_E2E_PUBLIC.hex()}")
    print(f"Computed DH:     {computed_dh.hex()}")
    print(f"Expected DH:     {EXPECTED_DH_SECRET.hex()}")
    print(f"Match:           {computed_dh == EXPECTED_DH_SECRET}")
    
    assert computed_dh == EXPECTED_DH_SECRET, "DH secret mismatch!"
    print("✅ DH Secret VERIFIED!")
    return computed_dh

# =============================================================================
# STEP 2: Test Decryption with PyNaCl SecretBox
# =============================================================================
def test_secretbox_decrypt(dh_secret: bytes, full_ciphertext: bytes = None):
    """
    Test decryption using PyNaCl's SecretBox
    
    PyNaCl SecretBox.decrypt expects: nonce + ciphertext
    Where ciphertext = [MAC 16][encrypted data]
    
    OR with detached mode: ciphertext + mac separately
    """
    print("\n" + "=" * 60)
    print("STEP 2: SecretBox Decryption Test")
    print("=" * 60)
    
    ciphertext_data = full_ciphertext if full_ciphertext else CM_ENC_BODY_TRUNCATED
    is_truncated = full_ciphertext is None
    
    print(f"Using {'TRUNCATED' if is_truncated else 'FULL'} ciphertext")
    print(f"Total cmEncBody length: {len(ciphertext_data)} bytes")
    print(f"Expected length: 16022 bytes")
    
    if is_truncated:
        print("\n⚠️  WARNING: Ciphertext is truncated!")
        print("    MAC verification will FAIL because MAC is computed over FULL ciphertext")
        print("    Export full 16022 bytes from ESP32 to test properly\n")
    
    # Extract MAC and ciphertext
    mac = ciphertext_data[:16]
    ciphertext = ciphertext_data[16:]
    
    print(f"MAC (first 16 bytes):  {mac.hex()}")
    print(f"Ciphertext length:     {len(ciphertext)} bytes")
    print(f"Nonce:                 {CM_NONCE.hex()}")
    
    # Test 1: Standard SecretBox (expects [nonce][mac][ciphertext])
    print("\n--- Test 1: Standard SecretBox format ---")
    try:
        box = SecretBox(dh_secret)
        # SecretBox.decrypt expects: nonce (24) + mac (16) + ciphertext
        combined = CM_NONCE + mac + ciphertext
        plaintext = box.decrypt(combined)
        print(f"✅ DECRYPT SUCCESS!")
        print(f"Plaintext length: {len(plaintext)} bytes")
        print(f"First 32 bytes:   {plaintext[:32].hex()}")
        return plaintext
    except CryptoError as e:
        print(f"❌ FAILED: {e}")
    
    # Test 2: Reordered format [ciphertext][mac] (some implementations)
    print("\n--- Test 2: Reordered format [cipher][mac] ---")
    try:
        box = SecretBox(dh_secret)
        reordered = CM_NONCE + ciphertext + mac
        plaintext = box.decrypt(reordered)
        print(f"✅ DECRYPT SUCCESS with reordered format!")
        return plaintext
    except CryptoError as e:
        print(f"❌ FAILED: {e}")
    
    # Test 3: Using raw bindings
    print("\n--- Test 3: Raw crypto_secretbox_open binding ---")
    try:
        # crypto_secretbox_open expects: [mac 16][ciphertext]
        combined_ct = mac + ciphertext
        plaintext = crypto_secretbox_open(combined_ct, CM_NONCE, dh_secret)
        print(f"✅ DECRYPT SUCCESS with raw binding!")
        return plaintext
    except Exception as e:
        print(f"❌ FAILED: {e}")
    
    return None

# =============================================================================
# STEP 3: Manual XSalsa20-Poly1305 Implementation Test
# =============================================================================
def test_manual_xsalsa20_poly1305(dh_secret: bytes):
    """
    Test with manual XSalsa20 and Poly1305 to match Haskell's implementation exactly
    
    Haskell's cryptoBox does:
    1. (rs, c) = xSalsa20 secret nonce plaintext
    2. tag = Poly1305.auth rs c
    3. return tag <> c  (i.e., [MAC][Ciphertext])
    
    Haskell's xSalsa20 does:
    1. Split nonce into [iv0 8 bytes][iv1 16 bytes]
    2. state0 = XSalsa.initialize 20 secret (zero16 ++ iv0)
    3. state1 = XSalsa.derive state0 iv1
    4. (rs, state2) = XSalsa.generate state1 32  -- Poly1305 subkey
    5. (ciphertext, _) = XSalsa.combine state2 plaintext
    """
    print("\n" + "=" * 60)
    print("STEP 3: Manual XSalsa20-Poly1305 Analysis")
    print("=" * 60)
    
    # This requires nacl.bindings for low-level access
    # or we use pycryptodome for XSalsa20
    
    try:
        from Crypto.Cipher import Salsa20
        print("Using PyCryptodome for Salsa20 analysis")
        
        # XSalsa20 nonce structure
        iv0 = CM_NONCE[:8]   # First 8 bytes
        iv1 = CM_NONCE[8:]   # Remaining 16 bytes
        
        print(f"Nonce split:")
        print(f"  iv0 (8 bytes):  {iv0.hex()}")
        print(f"  iv1 (16 bytes): {iv1.hex()}")
        
        # Note: PyCryptodome's Salsa20 uses 8-byte nonce
        # XSalsa20 requires HSalsa20 derivation which is more complex
        print("\n⚠️  Full XSalsa20 implementation requires HSalsa20 core")
        print("    This is handled by libsodium internally")
        
    except ImportError:
        print("PyCryptodome not available for detailed analysis")
    
    return None

# =============================================================================
# STEP 4: Offset Verification
# =============================================================================
def verify_offsets():
    """
    Verify that all offsets in the message structure are correct
    """
    print("\n" + "=" * 60)
    print("STEP 4: Offset Verification")
    print("=" * 60)
    
    # From hex dump:
    # 0000: 3e 82 00 00 00 00 69 7e 97 10 54 20 00 04 31 2c
    # 0016: 30 2a 30 05 06 03 2b 65 6e 03 21 00 91 40 e1 0e
    # ...
    
    print("Message structure after server-level decrypt:")
    print("  [0-1]   Length prefix:     0x3e82 = 16002")
    print("  [2-13]  Header/padding")
    print("  [14]    Maybe tag:         '1' (0x31) = Just (has key)")
    print("  [15]    SPKI length:       44 (0x2c)")
    print("  [16-27] SPKI header:       30 2a 30 05 06 03 2b 65 6e 03 21 00")
    print("  [28-59] e2ePubKey:         32 bytes X25519 raw key")
    print("  [60-83] cmNonce:           24 bytes")
    print("  [84-99] MAC:               16 bytes")
    print("  [100+]  Ciphertext:        16006 bytes")
    print()
    print("Verification:")
    print(f"  e2ePubKey expected at [28]: {PEER_E2E_PUBLIC[:4].hex()}...")
    print(f"  cmNonce expected at [60]:   {CM_NONCE[:4].hex()}...")
    print(f"  MAC expected at [84]:       {MAC[:4].hex()}...")
    print()
    print("  Total cmEncBody = MAC(16) + Ciphertext(16006) = 16022 bytes ✓")
    print("  plain_len = 16106, offset 84, so cmEncBody = 16106-84 = 16022 ✓")

# =============================================================================
# MAIN
# =============================================================================
def main():
    print("\n" + "=" * 60)
    print("SimpleGo Session 15 - E2E Decrypt Verification")
    print("=" * 60)
    
    # Step 1: Verify DH secret
    dh_secret = test_dh_secret()
    
    # Step 2: Test decryption
    result = test_secretbox_decrypt(dh_secret)
    
    # Step 3: Manual analysis
    test_manual_xsalsa20_poly1305(dh_secret)
    
    # Step 4: Verify offsets
    verify_offsets()
    
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    
    if result:
        print("✅ Decryption SUCCESSFUL!")
    else:
        print("❌ Decryption FAILED")
        print()
        print("NEXT STEPS:")
        print("1. Export FULL cmEncBody (16022 bytes) from ESP32")
        print("   Add to main.c:")
        print('   for(int i=0; i<cm_enc_len && i<16022; i++) {')
        print('       printf("%02x", server_plain[cm_enc_offset + i]);')
        print('       if((i+1) % 64 == 0) printf("\\n");')
        print('   }')
        print()
        print("2. Replace CM_ENC_BODY_TRUNCATED with full data")
        print("3. Run this script again")
        print()
        print("4. If still fails, check:")
        print("   - Is cm_enc_offset actually 84 or 86 (length prefix)?")
        print("   - Is server_plain starting from byte 0 or byte 2?")

if __name__ == "__main__":
    main()
