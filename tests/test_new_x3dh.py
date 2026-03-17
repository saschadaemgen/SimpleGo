from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

# NEW keys from latest log
peer_key1 = bytes.fromhex("b7256cdbabfd33dd6a270504cf22bb327f758700cdf4aebab819ee010a6d18d58ce7d8860a5321ff239e628e8c50bdc3b7159722fe6ce5f0")
peer_key2 = bytes.fromhex("6a9716ce8eb8650b60d9e1190b4b84b2e04c6f580b2b48cb095d33d6be0d47288fb1b5ee5a74eb83be874f0bbda3a8f4256577d822655768")
our_key1_priv = bytes.fromhex("2048139e5e7d62219d88a3678596c20ff929e8ce9f2ea4b2404b14d2c2dbe53fd271f2d6a337d5b9b9ee8b095b3d134aa224059077f886a6")
our_key2_priv = bytes.fromhex("649c0fd71dd45f979086fdf1a3a464780ed2bcb879defefff3f4587b878608e4d90231dd4b79ace75516229ba2cc9f6507bf0ab0e03abfe0")

# Create key objects
priv1 = X448PrivateKey.from_private_bytes(our_key1_priv)
priv2 = X448PrivateKey.from_private_bytes(our_key2_priv)
pub1 = X448PublicKey.from_public_bytes(peer_key1)
pub2 = X448PublicKey.from_public_bytes(peer_key2)

# DH calculations (as we compute them)
dh1 = priv2.exchange(pub1)  # peer1 + our2
dh2 = priv1.exchange(pub2)  # peer2 + our1  
dh3 = priv2.exchange(pub2)  # peer2 + our2

print(f"dh1: {dh1[:8].hex()}...")
print(f"dh2: {dh2[:8].hex()}...")
print(f"dh3: {dh3[:8].hex()}...")

# ESP32 log shows:
print("\nESP32 calculated:")
print("dh1: b864b741...")
print("dh2: db9ec34b...")
print("dh3: 12fba3bb...")

# Now with SWAPPED order [dh2, dh1, dh3]
dh_combined_swapped = dh2 + dh1 + dh3

hkdf = HKDF(algorithm=hashes.SHA512(), length=96, salt=b"", info=b"SimpleXX3DH")
output = hkdf.derive(dh_combined_swapped)

print(f"\nWith SWAPPED [dh2, dh1, dh3]:")
print(f"hk:  {output[0:32].hex()}")
print(f"nhk: {output[32:64].hex()}")
print(f"sk:  {output[64:96].hex()}")

print("\nESP32 calculated:")
print("hk:  403bfafb07525b09...")
print("rk:  5707ece6e5ca8957...")
