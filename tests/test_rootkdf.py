from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

# From X3DH:
sk = bytes.fromhex("a8a6a03914a8d5a40508a591a3fd5451c4d0f1e51c32f0f740afe50a55012bae")
# dh3 = DH(peer_key2, our_key2) - used again in ratchet init
dh3 = bytes.fromhex("d21162387b4f634f680baa01698d5b0f64589366850205eab75244daa15a049e8807a844bf3395ce275b5e25ba19f236d5dcb6769198e70f")

# Root KDF: HKDF(salt=root_key, ikm=dh_output, info="SimpleXRootRatchet", len=96)
hkdf = HKDF(
    algorithm=hashes.SHA512(),
    length=96,
    salt=sk,  # root_key as salt
    info=b"SimpleXRootRatchet",
)
output = hkdf.derive(dh3)

new_rk = output[0:32]
ck = output[32:64]
next_hk = output[64:96]

print(f"new_rk:  {new_rk.hex()}")
print(f"ck:      {ck.hex()}")
print(f"next_hk: {next_hk.hex()}")

print("\nESP32 calculated:")
print("new_rk:  4671846ce94ee5d4...")
print("ck:      0e27c51732b40db9...")
print("next_hk: 2576d24e680d53fe...")
