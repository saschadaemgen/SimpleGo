from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# Latest keys from log (OHNE Swap)
peer_key1 = bytes.fromhex("bca4d5ea89feaf6bbaccd224f93f6d70291cd7bdd885b98d0ef43d163364e8fdf47f6336c3d270b5e4d2292b392b320f9b8bdf5744d28195")
peer_key2 = bytes.fromhex("b7da171882daa8520e0cabb86d68062d5443e66c80d86bc4b37ec707b5001da89c5fc5e0fc166a97cd45cc3f82937da35b46c1c2dff91c93")
our_key1_priv = bytes.fromhex("006c40fd2b293df83010df17ebf2af06b001294f454aa9bc6e5f16b26ea325c2071260f37a1319879edfa1bde0a8a1e1233ee7b9dcbd8fc7")
our_key2_priv = bytes.fromhex("d826a3c1706e05f52b338837448751b905b19f549307c938d02b646748f5ac9ab166b00c76cb4f78aaa82e7b2472d600435748da1ca4f791")

priv1 = X448PrivateKey.from_private_bytes(our_key1_priv)
priv2 = X448PrivateKey.from_private_bytes(our_key2_priv)
pub1 = X448PublicKey.from_public_bytes(peer_key1)
pub2 = X448PublicKey.from_public_bytes(peer_key2)

dh1 = priv2.exchange(pub1)
dh2 = priv1.exchange(pub2)
dh3 = priv2.exchange(pub2)

print("=== DH Values ===")
print(f"dh1: {dh1[:8].hex()}...")
print(f"dh2: {dh2[:8].hex()}...")
print(f"dh3: {dh3[:8].hex()}...")
print(f"\nESP32 log shows:")
print(f"dh1: ddd9ee8c...")
print(f"dh2: b273e4c4...")
print(f"dh3: 85b343be...")

# ORIGINAL order [dh1, dh2, dh3]
dh_combined = dh1 + dh2 + dh3
hkdf = HKDF(algorithm=hashes.SHA512(), length=96, salt=b"", info=b"SimpleXX3DH")
output = hkdf.derive(dh_combined)
hk = output[0:32]
nhk = output[32:64]
sk = output[64:96]

print(f"\n=== X3DH Output (ORIGINAL order) ===")
print(f"hk:  {hk.hex()}")
print(f"nhk: {nhk.hex()}")
print(f"sk:  {sk.hex()}")
print(f"\nESP32 log shows:")
print(f"hk:  1b97e59e49cedc12...")
print(f"rk:  a122a2c7f460e5c9...")
