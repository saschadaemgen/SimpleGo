#!/usr/bin/env python3
"""
SimpleGo Protocol Test Suite
============================
Test: X3DH Key Agreement Verification

This script verifies the X3DH calculation using exact values
from the ESP32 Desktop test log (2026-01-31).

Purpose:
    - Verify DH1, DH2, DH3 calculations match ESP32
    - Verify HKDF output (hk, nhk, rk) matches ESP32
    - Identify any discrepancies in key derivation

Requirements:
    pip install cryptography

Author: SimpleGo Project
Date: 2026-01-31 Session 14
Related Bug: BUG #18 - A_CRYPTO error
"""

import hashlib
import hmac
from typing import Tuple

try:
    from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
    from cryptography.hazmat.primitives import serialization
    X448_AVAILABLE = True
except ImportError:
    X448_AVAILABLE = False
    print("⚠️  cryptography library not available for X448")
    print("   Install with: pip install cryptography")

# ============================================================================
# TEST DATA - Extracted from ESP32 Desktop Log (2026-01-31)
# ============================================================================

# Peer (App) keys from invitation
PEER_KEY1_HEX = "7fc37c1b820fb3cbb63e352bfc5d89a6f0d8ddc3dad05628a60a39a41714eed3ac71bbc7cbe20a22acd53729a08d834042a1747650e80adf"
PEER_KEY2_HEX = "93140de54b8e8dbcd175576f96adbc8a3bbb91bd72d78af40f7d192b187710ea9b256a530ba6c0252d44d60d55f4b795f08da7d9bcee6aad"

# Our (ESP32) keys
OUR_KEY1_PUB_HEX = "08cfaffb1b52649fdef5767eb7643b33a8239a9af5bed4d84d37d784a0f03b562add9ff1e0b7a353109f88418ab4f347d1f250b97f326fd8"
OUR_KEY1_PRIV_HEX = "349301188bde5c9913979e89302ac176ccfd2f15342231477e8d8019b6faa41d9ae57d23f42472a7a575903c7890f86d374fcd17e9187fe7"
OUR_KEY2_PUB_HEX = "0f49a0ca5c194255d5039564b63a3a424e908108567c96a4d09c101b8853c33c9214984d59acb05f6d5caecb6c18d51e60ff3cee43d64a7f"
OUR_KEY2_PRIV_HEX = "881930767ffabeaf57bc17117d315175594c9620536c5189336304c6750dc11bb624fe589a9460f4f29ff794cf2544e97fac79b7701327f7"

# Expected X3DH outputs from ESP32 log
EXPECTED_DH1 = "5c7b05b55baa5b33"  # First 8 bytes
EXPECTED_DH2 = "e8905f6c5951bb27"  # First 8 bytes
EXPECTED_DH3 = "e804aec901ba1da0"  # First 8 bytes
EXPECTED_HK  = "4cb1a36d0d1ea0ab"  # header_key_send (first 8 bytes)
EXPECTED_NHK = "c048aba5399d5910"  # header_key_recv (first 8 bytes)
EXPECTED_RK  = "5e0d6314fc338d02"  # root_key (first 8 bytes)


def hkdf_sha512(salt: bytes, ikm: bytes, info: bytes, length: int) -> bytes:
    """HKDF-SHA512 implementation."""
    # Extract
    if not salt:
        salt = b'\x00' * 64  # SHA-512 block size
    prk = hmac.new(salt, ikm, hashlib.sha512).digest()
    
    # Expand
    t = b""
    okm = b""
    for i in range((length + 63) // 64):
        t = hmac.new(prk, t + info + bytes([i + 1]), hashlib.sha512).digest()
        okm += t
    
    return okm[:length]


def hkdf3(salt: bytes, ikm: bytes, info: bytes) -> Tuple[bytes, bytes, bytes]:
    """
    HKDF producing 3 x 32-byte outputs (96 bytes total).
    Matches Haskell hkdf3 function.
    """
    out = hkdf_sha512(salt, ikm, info, 96)
    return out[0:32], out[32:64], out[64:96]


def x448_dh(private_key_bytes: bytes, public_key_bytes: bytes) -> bytes:
    """Perform X448 Diffie-Hellman."""
    if not X448_AVAILABLE:
        return b'\x00' * 56  # Dummy
    
    # Load private key
    private_key = X448PrivateKey.from_private_bytes(private_key_bytes)
    
    # Load public key
    public_key = X448PublicKey.from_public_bytes(public_key_bytes)
    
    # Perform DH
    shared = private_key.exchange(public_key)
    return shared


def verify_x3dh():
    """Verify X3DH calculation matches ESP32."""
    print("=" * 70)
    print("X3DH VERIFICATION TEST")
    print("=" * 70)
    print()
    
    # Parse keys
    peer_key1 = bytes.fromhex(PEER_KEY1_HEX)
    peer_key2 = bytes.fromhex(PEER_KEY2_HEX)
    our_key1_priv = bytes.fromhex(OUR_KEY1_PRIV_HEX)
    our_key2_priv = bytes.fromhex(OUR_KEY2_PRIV_HEX)
    our_key1_pub = bytes.fromhex(OUR_KEY1_PUB_HEX)
    
    print(f"Peer Key1 (App): {peer_key1[:8].hex()}...")
    print(f"Peer Key2 (App): {peer_key2[:8].hex()}...")
    print(f"Our Key1 Pub:    {our_key1_pub[:8].hex()}...")
    print(f"Our Key2 Priv:   {our_key2_priv[:8].hex()}...")
    print()
    
    if not X448_AVAILABLE:
        print("❌ Cannot perform X448 DH - cryptography library not available")
        print("   Testing HKDF only with expected DH values...")
        # Use dummy values for HKDF test
        dh1 = bytes.fromhex(EXPECTED_DH1 + "00" * 48)  # Pad to 56 bytes
        dh2 = bytes.fromhex(EXPECTED_DH2 + "00" * 48)
        dh3 = bytes.fromhex(EXPECTED_DH3 + "00" * 48)
    else:
        # Calculate DH values
        print("📊 Calculating DH values...")
        print()
        
        # DH1 = DH(peer_key1, our_key2_priv)
        print("DH1 = DH(peer_key1, our_key2.priv)")
        dh1 = x448_dh(our_key2_priv, peer_key1)
        dh1_match = dh1[:8].hex() == EXPECTED_DH1
        print(f"   Calculated: {dh1[:8].hex()}...")
        print(f"   Expected:   {EXPECTED_DH1}...")
        print(f"   {'✅ MATCH!' if dh1_match else '❌ MISMATCH!'}")
        print()
        
        # DH2 = DH(peer_key2, our_key1_priv)
        print("DH2 = DH(peer_key2, our_key1.priv)")
        dh2 = x448_dh(our_key1_priv, peer_key2)
        dh2_match = dh2[:8].hex() == EXPECTED_DH2
        print(f"   Calculated: {dh2[:8].hex()}...")
        print(f"   Expected:   {EXPECTED_DH2}...")
        print(f"   {'✅ MATCH!' if dh2_match else '❌ MISMATCH!'}")
        print()
        
        # DH3 = DH(peer_key2, our_key2_priv)
        print("DH3 = DH(peer_key2, our_key2.priv)")
        dh3 = x448_dh(our_key2_priv, peer_key2)
        dh3_match = dh3[:8].hex() == EXPECTED_DH3
        print(f"   Calculated: {dh3[:8].hex()}...")
        print(f"   Expected:   {EXPECTED_DH3}...")
        print(f"   {'✅ MATCH!' if dh3_match else '❌ MISMATCH!'}")
        print()
    
    # Combine DH outputs
    dh_combined = dh1 + dh2 + dh3
    print(f"DH Combined: {len(dh_combined)} bytes")
    print(f"   First 16: {dh_combined[:16].hex()}")
    print()
    
    # X3DH HKDF
    print("📊 X3DH HKDF...")
    salt = b'\x00' * 64  # 64 zero bytes
    info = b"SimpleXX3DH"
    
    hk, nhk, sk = hkdf3(salt, dh_combined, info)
    
    print(f"Salt: {salt[:8].hex()}... (64 zero bytes)")
    print(f"Info: {info.decode()}")
    print()
    
    hk_match = hk[:8].hex() == EXPECTED_HK
    nhk_match = nhk[:8].hex() == EXPECTED_NHK
    rk_match = sk[:8].hex() == EXPECTED_RK
    
    print(f"HK (header_key_send):")
    print(f"   Calculated: {hk[:8].hex()}...")
    print(f"   Expected:   {EXPECTED_HK}...")
    print(f"   {'✅ MATCH!' if hk_match else '❌ MISMATCH!'}")
    print()
    
    print(f"NHK (header_key_recv):")
    print(f"   Calculated: {nhk[:8].hex()}...")
    print(f"   Expected:   {EXPECTED_NHK}...")
    print(f"   {'✅ MATCH!' if nhk_match else '❌ MISMATCH!'}")
    print()
    
    print(f"RK (root_key):")
    print(f"   Calculated: {sk[:8].hex()}...")
    print(f"   Expected:   {EXPECTED_RK}...")
    print(f"   {'✅ MATCH!' if rk_match else '❌ MISMATCH!'}")
    print()
    
    # Summary
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    
    if X448_AVAILABLE:
        all_match = dh1_match and dh2_match and dh3_match and hk_match and nhk_match and rk_match
        print(f"DH1:  {'✅' if dh1_match else '❌'}")
        print(f"DH2:  {'✅' if dh2_match else '❌'}")
        print(f"DH3:  {'✅' if dh3_match else '❌'}")
    else:
        all_match = hk_match and nhk_match and rk_match
        print("DH1-3: ⚠️  (not tested - need cryptography library)")
    
    print(f"HK:   {'✅' if hk_match else '❌'}")
    print(f"NHK:  {'✅' if nhk_match else '❌'}")
    print(f"RK:   {'✅' if rk_match else '❌'}")
    print()
    
    if all_match:
        print("🎉 ALL TESTS PASSED - X3DH is correct!")
        print()
        print("If A_CRYPTO still occurs, the problem is AFTER X3DH:")
        print("   - rootKdf in initSndRatchet")
        print("   - chainKdf for message encryption")
        print("   - AAD construction for AES-GCM")
    else:
        print("❌ SOME TESTS FAILED - Check X3DH implementation!")
    
    # Print full keys for debugging
    print()
    print("=" * 70)
    print("FULL CALCULATED KEYS (for debugging)")
    print("=" * 70)
    print(f"HK (full):  {hk.hex()}")
    print(f"NHK (full): {nhk.hex()}")
    print(f"RK (full):  {sk.hex()}")


def verify_rcad():
    """Verify rcAD (Associated Data) construction."""
    print()
    print("=" * 70)
    print("rcAD VERIFICATION")
    print("=" * 70)
    print()
    
    our_key1_pub = bytes.fromhex(OUR_KEY1_PUB_HEX)
    peer_key1 = bytes.fromhex(PEER_KEY1_HEX)
    
    # ESP32 constructs: our_key1 || peer_key1
    rcAD = our_key1_pub + peer_key1
    
    print(f"rcAD = our_key1_pub || peer_key1")
    print(f"Length: {len(rcAD)} bytes (should be 112)")
    print(f"First 16 bytes:  {rcAD[:16].hex()}")
    print(f"Bytes 56-72:     {rcAD[56:72].hex()}")
    print()
    
    # Expected from ESP32 log
    expected_rcAD_start = "08cfaffb1b52649fdef5767eb7643b33"
    expected_rcAD_mid = "7fc37c1b820fb3cbb63e352bfc5d89a6"
    
    start_match = rcAD[:16].hex() == expected_rcAD_start
    mid_match = rcAD[56:72].hex() == expected_rcAD_mid
    
    print(f"Start (our_key1): {'✅' if start_match else '❌'}")
    print(f"Mid (peer_key1):  {'✅' if mid_match else '❌'}")
    
    # Full rcAD hex for comparison
    print()
    print("Full rcAD (for comparison with ESP32 log):")
    print(f"   {rcAD.hex()}")


if __name__ == "__main__":
    verify_x3dh()
    verify_rcad()
