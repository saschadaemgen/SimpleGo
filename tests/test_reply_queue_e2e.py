#!/usr/bin/env python3
"""
================================================================================
SimpleGo - Reply Queue E2E Decrypt Verification
================================================================================

BUG #18 - Session 14: Python verification test for per-queue E2E decryption

PURPOSE:
    Test if the crypto implementation is correct by using exact values from
    the ESP32 log. This helps identify whether the problem is:
    - Crypto implementation (if Python works but ESP32 doesn't)
    - Key/Structure issue (if Python also fails)

REQUIREMENTS:
    pip install pynacl

USAGE:
    python test_reply_queue_e2e.py

    Before running, update the hex values below with your actual ESP32 log data:
    - our_e2e_private_hex: Your queue's E2E private key
    - peer_public_hex: The key extracted from message [28-59]
    - nonce_hex: The nonce from message [60-83]
    - ciphertext_hex: The encrypted body from message [84+]

RESULTS INTERPRETATION:
    - If ALL tests FAIL → Problem is key or structure, NOT crypto
    - If ONE test WORKS → ESP32 implementation needs to match that method

================================================================================
"""

from nacl.bindings import crypto_scalarmult
from nacl.secret import SecretBox
from nacl.public import PrivateKey, PublicKey, Box

# ============================================================================
# TEST DATA - REPLACE WITH YOUR ESP32 LOG VALUES!
# ============================================================================

# Your queue's E2E private key (from ESP32 log)
# Look for: "our_queue.e2e_private" or generate from keypair
our_e2e_private_hex = "30cdb61c34466225586d40a7a3a6b9010dc2ec691bd2eb5779a456491d956ff1"

# Peer's E2E public key extracted from message at [28-59]
# This is the raw X25519 key (32 bytes) after the SPKI header
peer_public_hex = "8731c502883d3924672bf3f527b43d71545647bfd007622423f908fed9884f3e"

# Nonce extracted from message at [60-83] (24 bytes)
nonce_hex = "de81a0cc1da348bf426b2a4985ea236329d0e24d4038ed00"

# Ciphertext from message at [84+]
# Format: [MAC 16 bytes][Ciphertext] (Haskell format)
# Note: This is truncated for testing, full message is ~16022 bytes
ciphertext_hex = (
    "7c14495d3459f85fbc22840ae4bc7db9"  # MAC (16 bytes)
    "2f9498886c56a8476faa5dbfa9de60c7"  # Ciphertext start
    "eedf682aedf15d7328a11d4bd71947b0"
    "76357d8d1a377d11884586daa1a21f8e"
    "5ad420c42d327e278d2c2ab99b71a9a5"
    "5afa705cca74588753fd08c122de836e"
    "9358400f8934a7c4efbed2736b515f55"
    "209ad09bc3e8df2e8e20fb5932b8d5d6"
    "bbc9e0c5823eba5069b4bc44d9eedde3"
    "2a35b40394b5881e941e5d7af9a0f7ff"
    "c16c5ce5c295e5f4f9a7e38f78cedf1b"
    "7eb5b8fcd38225c5d47d518cbe463318"
    "870292a2a633dbd6"
)

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def print_header(title):
    """Print a section header"""
    print()
    print("=" * 70)
    print(title)
    print("=" * 70)

def print_result(success, message):
    """Print test result"""
    if success:
        print(f"\n   ✅ SUCCESS: {message}")
    else:
        print(f"\n   ❌ FAILED: {message}")

# ============================================================================
# MAIN TEST
# ============================================================================

def main():
    print_header("SimpleGo - Reply Queue E2E Decrypt Verification")
    print("\nBUG #18 - Session 14: Testing with ESP32 log values")
    
    # Convert hex to bytes
    our_private = bytes.fromhex(our_e2e_private_hex)
    peer_public = bytes.fromhex(peer_public_hex)
    nonce = bytes.fromhex(nonce_hex)
    ciphertext = bytes.fromhex(ciphertext_hex)
    
    print_header("Input Data")
    print(f"   our_private:  {our_private.hex()[:16]}... ({len(our_private)} bytes)")
    print(f"   peer_public:  {peer_public.hex()[:16]}... ({len(peer_public)} bytes)")
    print(f"   nonce:        {nonce.hex()} ({len(nonce)} bytes)")
    print(f"   ciphertext:   {len(ciphertext)} bytes")
    print(f"   MAC (first 16): {ciphertext[:16].hex()}")
    
    results = []
    
    # ========================================================================
    # TEST 1: Raw DH + SecretBox with MAC reorder (Haskell style)
    # ========================================================================
    # Haskell uses raw DH secret directly for XSalsa20 (no HSalsa20)
    # Haskell format: [MAC 16][Ciphertext]
    # libsodium format: [Ciphertext][MAC 16]
    
    print_header("TEST 1: Raw DH + SecretBox (MAC reorder)")
    print("   Method: crypto_scalarmult + SecretBox")
    print("   Reorder: [MAC][Cipher] → [Cipher][MAC]")
    
    try:
        # Raw DH (no HSalsa20 derivation)
        dh_secret = crypto_scalarmult(our_private, peer_public)
        print(f"   DH Secret: {dh_secret.hex()[:16]}...")
        
        # Reorder: Haskell=[MAC][Cipher] → libsodium=[Cipher][MAC]
        mac = ciphertext[:16]
        cipher_only = ciphertext[16:]
        reordered = cipher_only + mac
        
        # Decrypt with SecretBox (XSalsa20-Poly1305)
        box = SecretBox(dh_secret)
        plaintext = box.decrypt(reordered, nonce)
        
        print_result(True, f"Decrypted {len(plaintext)} bytes")
        print(f"   Plaintext hex: {plaintext[:32].hex()}")
        print(f"   Plaintext ascii: {plaintext[:50]}")
        results.append(("TEST 1", True))
        
    except Exception as e:
        print_result(False, str(e))
        results.append(("TEST 1", False))
    
    # ========================================================================
    # TEST 2: Raw DH + SecretBox without MAC reorder
    # ========================================================================
    # In case Haskell actually uses [Ciphertext][MAC] format
    
    print_header("TEST 2: Raw DH + SecretBox (no reorder)")
    print("   Method: crypto_scalarmult + SecretBox")
    print("   No reorder - direct decrypt")
    
    try:
        dh_secret = crypto_scalarmult(our_private, peer_public)
        box = SecretBox(dh_secret)
        plaintext = box.decrypt(ciphertext, nonce)
        
        print_result(True, f"Decrypted {len(plaintext)} bytes")
        print(f"   Plaintext hex: {plaintext[:32].hex()}")
        results.append(("TEST 2", True))
        
    except Exception as e:
        print_result(False, str(e))
        results.append(("TEST 2", False))
    
    # ========================================================================
    # TEST 3: crypto_box with MAC reorder (with HSalsa20)
    # ========================================================================
    # Standard NaCl crypto_box: DH → HSalsa20 → XSalsa20-Poly1305
    
    print_header("TEST 3: crypto_box (MAC reorder)")
    print("   Method: Box (includes HSalsa20 key derivation)")
    print("   Reorder: [MAC][Cipher] → [Cipher][MAC]")
    
    try:
        priv_key = PrivateKey(our_private)
        pub_key = PublicKey(peer_public)
        box = Box(priv_key, pub_key)
        
        mac = ciphertext[:16]
        cipher_only = ciphertext[16:]
        reordered = cipher_only + mac
        
        plaintext = box.decrypt(reordered, nonce)
        
        print_result(True, f"Decrypted {len(plaintext)} bytes")
        print(f"   Plaintext hex: {plaintext[:32].hex()}")
        results.append(("TEST 3", True))
        
    except Exception as e:
        print_result(False, str(e))
        results.append(("TEST 3", False))
    
    # ========================================================================
    # TEST 4: crypto_box without MAC reorder
    # ========================================================================
    
    print_header("TEST 4: crypto_box (no reorder)")
    print("   Method: Box (includes HSalsa20 key derivation)")
    print("   No reorder - direct decrypt")
    
    try:
        priv_key = PrivateKey(our_private)
        pub_key = PublicKey(peer_public)
        box = Box(priv_key, pub_key)
        
        plaintext = box.decrypt(ciphertext, nonce)
        
        print_result(True, f"Decrypted {len(plaintext)} bytes")
        print(f"   Plaintext hex: {plaintext[:32].hex()}")
        results.append(("TEST 4", True))
        
    except Exception as e:
        print_result(False, str(e))
        results.append(("TEST 4", False))
    
    # ========================================================================
    # SUMMARY
    # ========================================================================
    
    print_header("SUMMARY")
    
    for test_name, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"   {test_name}: {status}")
    
    print()
    
    all_failed = all(not success for _, success in results)
    any_passed = any(success for _, success in results)
    
    if all_failed:
        print("   ⚠️  ALL TESTS FAILED!")
        print()
        print("   This means the problem is NOT the crypto implementation.")
        print("   Possible causes:")
        print("      1. Key at [28-59] is NOT the e2ePubKey (maybe corrId?)")
        print("      2. Nonce position is wrong")
        print("      3. Ciphertext offset is wrong")
        print("      4. e2e_private doesn't match the sent e2e_public")
        print("      5. This is NOT the first message (needs stored secret)")
    elif any_passed:
        print("   🎉 AT LEAST ONE TEST PASSED!")
        print()
        print("   The passing test shows the correct method.")
        print("   Update ESP32 implementation to match.")
    
    print()

if __name__ == "__main__":
    main()
