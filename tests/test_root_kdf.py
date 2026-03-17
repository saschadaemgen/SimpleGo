#!/usr/bin/env python3
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey

# Keys from ESP32 log
peer_key2_hex = "3acd9b2fd868f96b6ffa3d921e675149a9d815e8679f00a2eaef02ffa8be0fe99ab4442e8fb2ca6feb145c29cd0472b4365ed05db6723f89"
our_key2_priv_hex = "74565eeb382912155f87ce2d1fb13badc6834eaf146b23cd74a60f07cf4e1a2cfe8b1f9c1163b8c842b4a324fec82f59535e905925fd85af"

# X3DH output from ESP32
esp32_root_key_after_x3dh = "fbe155e6bd578b6a"  # first 8 bytes

# Root KDF output from ESP32
esp32_dh_out = "c0bb8d7a63b0beca"  # first 8 bytes (same as dh3!)
esp32_new_rk = "4ed52f90834491d4"
esp32_ck = "7a6771f2441af07f"
esp32_next_hk = "1b6e7b8064000b77"

print("=== Root KDF Test ===")
print()

# Compute DH for root KDF: dh(peer_key2, our_key2_priv)
peer_key2 = X448PublicKey.from_public_bytes(bytes.fromhex(peer_key2_hex))
our_key2_priv = X448PrivateKey.from_private_bytes(bytes.fromhex(our_key2_priv_hex))
dh_out = our_key2_priv.exchange(peer_key2)

print(f"DH out: {dh_out[:8].hex()}...")
print(f"ESP32:  {esp32_dh_out}...")
print(f"Match: {dh_out[:8].hex() == esp32_dh_out}")
print()

# Now we need the FULL root key from X3DH (not just first 8 bytes)
# Let's compute it properly

# First, recompute X3DH to get full root key
peer_key1_hex = "fd75add9ebb1087d7e962535e04d6af0e4f03c17f6f2a790ddc6d6430beec02e5fe8bee3c587b04b27195c5d8468dc89a3b7b699dd0c2c7f"
our_key1_priv_hex = "00202ce4240524a4cc2c500a9a2e288245765e0c4fe8bbef9ce4a1bab4dff0f989fdd6d6aac128fa4a7671a23959fe68a7dbd8d3960b66cc"

peer_key1 = X448PublicKey.from_public_bytes(bytes.fromhex(peer_key1_hex))
our_key1_priv = X448PrivateKey.from_private_bytes(bytes.fromhex(our_key1_priv_hex))

# X3DH DH calculations
dh1 = our_key2_priv.exchange(peer_key1)
dh2 = our_key1_priv.exchange(peer_key2)
dh3 = our_key2_priv.exchange(peer_key2)

dh_combined = dh1 + dh2 + dh3

# X3DH HKDF
salt_x3dh = bytes(64)
hkdf_x3dh = HKDF(algorithm=hashes.SHA512(), length=96, salt=salt_x3dh, info=b"SimpleXX3DH")
x3dh_output = hkdf_x3dh.derive(dh_combined)

hk = x3dh_output[0:32]
nhk = x3dh_output[32:64]
rk = x3dh_output[64:96]

print(f"X3DH Root Key: {rk[:8].hex()}...")
print(f"ESP32:         {esp32_root_key_after_x3dh}...")
print(f"Match: {rk[:8].hex() == esp32_root_key_after_x3dh}")
print()

# Now Root KDF with full root key
# hkdf3(rk, dh_out, "SimpleXRootRatchet")
hkdf_root = HKDF(algorithm=hashes.SHA512(), length=96, salt=rk, info=b"SimpleXRootRatchet")
root_kdf_output = hkdf_root.derive(dh_out)

new_rk = root_kdf_output[0:32]
ck = root_kdf_output[32:64]
next_hk = root_kdf_output[64:96]

print(f"New Root Key: {new_rk[:8].hex()}...")
print(f"ESP32:        {esp32_new_rk}...")
print(f"Match: {new_rk[:8].hex() == esp32_new_rk}")
print()

print(f"Chain Key: {ck[:8].hex()}...")
print(f"ESP32:     {esp32_ck}...")
print(f"Match: {ck[:8].hex() == esp32_ck}")
print()

print(f"Next Header Key: {next_hk[:8].hex()}...")
print(f"ESP32:           {esp32_next_hk}...")
print(f"Match: {next_hk[:8].hex() == esp32_next_hk}")
