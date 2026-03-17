from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

dh1 = bytes.fromhex("24b7b65fd6e0ea720efa85ec2706e8d75a570af96321f858e13e5191ceda07d78bce1b79df357f59d6a9a4ab3a99904e91628d176db01803")
dh2 = bytes.fromhex("6733925c564a10c2e411eed90cd85f4b570180e97a3ee99cc08ad950cf5ae61f3f79a9a56b4cfc66fd6be256f1d787e6670ce98648887cb9")
dh3 = bytes.fromhex("d21162387b4f634f680baa01698d5b0f64589366850205eab75244daa15a049e8807a844bf3395ce275b5e25ba19f236d5dcb6769198e70f")

# SWAPPED: dh2 || dh1 || dh3
dh_combined_swapped = dh2 + dh1 + dh3

hkdf = HKDF(algorithm=hashes.SHA512(), length=96, salt=b"", info=b"SimpleXX3DH")
output = hkdf.derive(dh_combined_swapped)

print("With SWAPPED order [dh2, dh1, dh3]:")
print(f"  hk:  {output[0:32].hex()}")
print(f"  nhk: {output[32:64].hex()}")
print(f"  sk:  {output[64:96].hex()}")
