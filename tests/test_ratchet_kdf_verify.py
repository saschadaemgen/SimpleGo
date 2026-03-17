#!/usr/bin/env python3
"""
SimpleGo Protocol Test Suite - Session 14
==========================================
Test: rootKdf and chainKdf Verification

This script verifies the ratchet KDF calculations using exact values
from the ESP32 Desktop test log (2026-01-31).

Purpose:
    - Verify rootKdf output (new_rk, ck, next_hk) matches ESP32
    - Verify chainKdf output (message_key, msg_iv, header_iv) matches ESP32
    - Identify any discrepancies in key derivation

Requirements:
    pip install cryptography pynacl

Author: SimpleGo Project
Date: 2026-01-31 Session 14.x
Related Bug: BUG #18 - A_CRYPTO error
"""

import hashlib
import hmac
from typing import Tuple

try:
    from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
    X448_AVAILABLE = True
except ImportError:
    X448_AVAILABLE = False
    print("⚠️  cryptography library not available for X448")

# ============================================================================
# TEST DATA - Extracted from ESP32 Desktop Log (2026-01-31)
# ============================================================================

# X3DH Results (VERIFIED CORRECT by test_x3dh_verify.py)
X3DH_HK = "4cb1a36d0d1ea0abe02b2c2a34f1d376d5990bf42cfd0bd140d36ce4c081e006"
X3DH_NHK = "c048aba5399d5910bd7c55d51afdd87b4857e7a56c93517ac17353a5a740da95"
X3DH_RK = "5e0d6314fc338d02bdf39efa39bfeb4e655c7cc6e1f5b910c09c2c699ce8f007"

# initSndRatchet: rootKdf inputs
ROOT_KEY_IN = X3DH_RK  # From X3DH
PEER_KEY2_HEX = "93140de54b8e8dbcd175576f96adbc8a3bbb91bd72d78af40f7d192b187710ea9b256a530ba6c0252d44d60d55f4b795f08da7d9bcee6aad"
OUR_KEY2_PRIV_HEX = "881930767ffabeaf57bc17117d315175594c9620536c5189336304c6750dc11bb624fe589a9460f4f29ff794cf2544e97fac79b7701327f7"

# Expected rootKdf outputs from ESP32 log
EXPECTED_DH_OUT = "e804aec901ba1da0"  # First 8 bytes (same as X3DH dh3!)
EXPECTED_NEW_RK = "73bbc67bc99c60d6"  # First 8 bytes
EXPECTED_CK = "5799548bbd19ee5b"       # First 8 bytes (chain_key_send)
EXPECTED_NEXT_HK = "076d99b6dd0696b4"  # First 8 bytes

# chainKdf inputs (from ESP32 log - msg 0)
CHAIN_KEY_IN = "5799548bbd19ee5b"  # First 8 bytes only in log, need full!

# Expected chainKdf outputs from ESP32 log (msg 0)
EXPECTED_MESSAGE_KEY = "19965cfb79362395"  # First 8 bytes
EXPECTED_MSG_IV = "b9acd432a69fb634"       # First 8 bytes (16 byte IV)
EXPECTED_HEADER_IV = "3f65025e8fb0d578"    # First 8 bytes (16 byte IV)

# Header encryption test data (from ESP32 log)
HEADER_KEY = X3DH_HK  # header_key_send = hk from X3DH
HEADER_IV_FULL = "3f65025e8fb0d5781c1ad8bbcc148451"  # 16 bytes
HEADER_TAG = "8c80b6b28f2f29942446699dc3ad8a16"
ENCRYPTED_HEADER = "ab424e78a759af2cced2c98a62cdc22a6dc540194994f221d844b131d2d4d31ca9dc4a7c6a640812c1a6d08dca742b0d6ab78721e751378b956e6f6eb26f1772fe195debee5590f9d9c0d8fbd9fa7b79d70e6fa53c7800a6"

# rcAD (Associated Data)
RCAD_HEX = "08cfaffb1b52649fdef5767eb7643b33a8239a9af5bed4d84d37d784a0f03b562add9ff1e0b7a353109f88418ab4f347d1f250b97f326fd87fc37c1b820fb3cbb63e352bfc5d89a6f0d8ddc3dad05628a60a39a41714eed3ac71bbc7cbe20a22acd53729a08d834042a1747650e80adf"

# MsgHeader plaintext
MSG_HEADER_PLAIN = "004f0002443042300506032b656f0339000f49a0ca5c194255d5039564b63a3a424e908108567c96a4d09c101b8853c33c9214984d59acb05f6d5caecb6c18d51e60ff3cee43d64a7f000000000000000023232323232323"


def hkdf_sha512(salt: bytes, ikm: bytes, info: bytes, length: int) -> bytes:
    """HKDF-SHA512 implementation."""
    # Extract
    if not salt or len(salt) == 0:
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
    """HKDF producing 3 x 32-byte outputs (96 bytes total)."""
    out = hkdf_sha512(salt, ikm, info, 96)
    return out[0:32], out[32:64], out[64:96]


def root_kdf(root_key: bytes, dh_out: bytes) -> Tuple[bytes, bytes, bytes]:
    """
    rootKdf from Haskell Ratchet.hs line 1159-1166
    
    Salt = root_key
    IKM = dh_out (+ optional KEM secret, not used here)
    Info = "SimpleXRootRatchet"
    
    Returns: (new_root_key, chain_key, next_header_key)
    """
    new_rk, ck, nhk = hkdf3(root_key, dh_out, b"SimpleXRootRatchet")
    return new_rk, ck, nhk


def chain_kdf(chain_key: bytes) -> Tuple[bytes, bytes, bytes, bytes]:
    """
    chainKdf from Haskell Ratchet.hs line 1168-1172
    
    Salt = "" (empty)
    IKM = chain_key
    Info = "SimpleXChainRatchet"
    
    Returns: (next_chain_key, message_key, iv1, iv2)
    where iv1 = msg_iv (16 bytes), iv2 = header_iv (16 bytes)
    """
    out = hkdf_sha512(b"", chain_key, b"SimpleXChainRatchet", 96)
    next_ck = out[0:32]
    mk = out[32:64]
    iv1 = out[64:80]  # 16 bytes for message
    iv2 = out[80:96]  # 16 bytes for header
    return next_ck, mk, iv1, iv2


def x448_dh(private_key_bytes: bytes, public_key_bytes: bytes) -> bytes:
    """Perform X448 Diffie-Hellman."""
    if not X448_AVAILABLE:
        return b'\x00' * 56
    private_key = X448PrivateKey.from_private_bytes(private_key_bytes)
    public_key = X448PublicKey.from_public_bytes(public_key_bytes)
    return private_key.exchange(public_key)


def verify_root_kdf():
    """Verify rootKdf calculation matches ESP32."""
    print("=" * 70)
    print("rootKdf VERIFICATION TEST (initSndRatchet)")
    print("=" * 70)
    print()
    
    root_key = bytes.fromhex(ROOT_KEY_IN)
    peer_key2 = bytes.fromhex(PEER_KEY2_HEX)
    our_key2_priv = bytes.fromhex(OUR_KEY2_PRIV_HEX)
    
    print(f"Inputs:")
    print(f"  root_key (from X3DH): {ROOT_KEY_IN[:16]}...")
    print(f"  peer_key2: {PEER_KEY2_HEX[:16]}...")
    print(f"  our_key2_priv: {OUR_KEY2_PRIV_HEX[:16]}...")
    print()
    
    # Calculate DH for rootKdf
    if X448_AVAILABLE:
        dh_out = x448_dh(our_key2_priv, peer_key2)
        dh_match = dh_out[:8].hex() == EXPECTED_DH_OUT
        print(f"DH(peer_key2, our_key2.priv):")
        print(f"   Calculated: {dh_out[:8].hex()}...")
        print(f"   Expected:   {EXPECTED_DH_OUT}...")
        print(f"   {'✅ MATCH!' if dh_match else '❌ MISMATCH!'}")
        print()
        
        # Calculate rootKdf
        new_rk, ck, next_hk = root_kdf(root_key, dh_out)
        
        new_rk_match = new_rk[:8].hex() == EXPECTED_NEW_RK
        ck_match = ck[:8].hex() == EXPECTED_CK
        next_hk_match = next_hk[:8].hex() == EXPECTED_NEXT_HK
        
        print(f"rootKdf(root_key, dh_out):")
        print()
        print(f"  new_root_key:")
        print(f"     Calculated: {new_rk[:8].hex()}...")
        print(f"     Expected:   {EXPECTED_NEW_RK}...")
        print(f"     {'✅ MATCH!' if new_rk_match else '❌ MISMATCH!'}")
        print()
        print(f"  chain_key (ck):")
        print(f"     Calculated: {ck[:8].hex()}...")
        print(f"     Expected:   {EXPECTED_CK}...")
        print(f"     {'✅ MATCH!' if ck_match else '❌ MISMATCH!'}")
        print()
        print(f"  next_header_key:")
        print(f"     Calculated: {next_hk[:8].hex()}...")
        print(f"     Expected:   {EXPECTED_NEXT_HK}...")
        print(f"     {'✅ MATCH!' if next_hk_match else '❌ MISMATCH!'}")
        print()
        
        # Print full values for debugging
        print("=" * 70)
        print("FULL CALCULATED VALUES (for debugging)")
        print("=" * 70)
        print(f"dh_out (full):     {dh_out.hex()}")
        print(f"new_rk (full):     {new_rk.hex()}")
        print(f"chain_key (full):  {ck.hex()}")
        print(f"next_hk (full):    {next_hk.hex()}")
        
        return dh_match and new_rk_match and ck_match and next_hk_match, ck
    else:
        print("❌ Cannot test - X448 not available")
        return False, None


def verify_chain_kdf(chain_key: bytes = None):
    """Verify chainKdf calculation matches ESP32."""
    print()
    print("=" * 70)
    print("chainKdf VERIFICATION TEST (msg 0)")
    print("=" * 70)
    print()
    
    if chain_key is None:
        print("⚠️  No chain_key provided, skipping chainKdf test")
        return False
    
    print(f"Input chain_key: {chain_key[:8].hex()}...")
    print()
    
    # Calculate chainKdf
    next_ck, mk, iv1, iv2 = chain_kdf(chain_key)
    
    mk_match = mk[:8].hex() == EXPECTED_MESSAGE_KEY
    iv1_match = iv1[:8].hex() == EXPECTED_MSG_IV
    iv2_match = iv2[:8].hex() == EXPECTED_HEADER_IV
    
    print(f"chainKdf(chain_key):")
    print()
    print(f"  message_key:")
    print(f"     Calculated: {mk[:8].hex()}...")
    print(f"     Expected:   {EXPECTED_MESSAGE_KEY}...")
    print(f"     {'✅ MATCH!' if mk_match else '❌ MISMATCH!'}")
    print()
    print(f"  msg_iv (iv1):")
    print(f"     Calculated: {iv1[:8].hex()}...")
    print(f"     Expected:   {EXPECTED_MSG_IV}...")
    print(f"     {'✅ MATCH!' if iv1_match else '❌ MISMATCH!'}")
    print()
    print(f"  header_iv (iv2):")
    print(f"     Calculated: {iv2[:8].hex()}...")
    print(f"     Expected:   {EXPECTED_HEADER_IV}...")
    print(f"     {'✅ MATCH!' if iv2_match else '❌ MISMATCH!'}")
    print()
    
    # Print full values
    print("=" * 70)
    print("FULL CALCULATED VALUES")
    print("=" * 70)
    print(f"next_chain_key: {next_ck.hex()}")
    print(f"message_key:    {mk.hex()}")
    print(f"msg_iv:         {iv1.hex()}")
    print(f"header_iv:      {iv2.hex()}")
    
    return mk_match and iv1_match and iv2_match


def verify_header_encryption():
    """Verify AES-GCM header encryption."""
    print()
    print("=" * 70)
    print("AES-GCM HEADER ENCRYPTION TEST")
    print("=" * 70)
    print()
    
    try:
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    except ImportError:
        print("❌ cryptography not available for AES-GCM test")
        return False
    
    header_key = bytes.fromhex(HEADER_KEY)
    header_iv = bytes.fromhex(HEADER_IV_FULL)
    rcad = bytes.fromhex(RCAD_HEX)
    plaintext = bytes.fromhex(MSG_HEADER_PLAIN)
    expected_tag = bytes.fromhex(HEADER_TAG)
    expected_ct = bytes.fromhex(ENCRYPTED_HEADER)
    
    print(f"Inputs:")
    print(f"  header_key: {HEADER_KEY[:16]}...")
    print(f"  header_iv:  {HEADER_IV_FULL}")
    print(f"  rcAD len:   {len(rcad)} bytes")
    print(f"  plaintext:  {len(plaintext)} bytes (MsgHeader)")
    print()
    
    # Encrypt
    aesgcm = AESGCM(header_key)
    ciphertext_with_tag = aesgcm.encrypt(header_iv, plaintext, rcad)
    
    # AES-GCM appends tag at end
    ciphertext = ciphertext_with_tag[:-16]
    tag = ciphertext_with_tag[-16:]
    
    ct_match = ciphertext.hex() == ENCRYPTED_HEADER
    tag_match = tag.hex() == HEADER_TAG
    
    print(f"Results:")
    print(f"  Ciphertext match: {'✅' if ct_match else '❌'}")
    print(f"     Calculated: {ciphertext[:16].hex()}...")
    print(f"     Expected:   {ENCRYPTED_HEADER[:32]}...")
    print()
    print(f"  Tag match: {'✅' if tag_match else '❌'}")
    print(f"     Calculated: {tag.hex()}")
    print(f"     Expected:   {HEADER_TAG}")
    
    return ct_match and tag_match


def main():
    print("=" * 70)
    print("SimpleGo Session 14.x - Ratchet KDF Verification")
    print("=" * 70)
    print()
    
    # Test 1: rootKdf
    root_ok, chain_key = verify_root_kdf()
    
    # Test 2: chainKdf (only if rootKdf passed and we have chain_key)
    chain_ok = False
    if chain_key:
        chain_ok = verify_chain_kdf(chain_key)
    
    # Test 3: AES-GCM header encryption
    aes_ok = verify_header_encryption()
    
    # Summary
    print()
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"rootKdf:            {'✅ PASS' if root_ok else '❌ FAIL'}")
    print(f"chainKdf:           {'✅ PASS' if chain_ok else '❌ FAIL'}")
    print(f"AES-GCM Header:     {'✅ PASS' if aes_ok else '❌ FAIL'}")
    print()
    
    if root_ok and chain_ok and aes_ok:
        print("🎉 ALL KDF TESTS PASSED!")
        print()
        print("If A_CRYPTO still occurs, the problem is in:")
        print("   - Payload AAD construction (rcAD + emHeader)")
        print("   - Wire format encoding")
        print("   - Key assignment (send vs recv)")
    else:
        print("❌ SOME TESTS FAILED - Check implementation!")


if __name__ == "__main__":
    main()
