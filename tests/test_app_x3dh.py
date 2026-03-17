from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

# Our keys (sender)
our_key1_pub = bytes.fromhex("c8c668c294ca48043df9cf382dff5f04a420a6cf3c82694cba8df92d53a832b35e1317a58a3b381aaad893fbd7c260f9e104f2cc32e13a98")
our_key2_pub = bytes.fromhex("d9c6f650c5455d7069c7212c8e09000e64a53673499dadae095194e228a8eaee71f31060ee6738f66b1f20e7e55d6aaf94cf9d332d580ae7")

# App's keys (receiver) - these are the peer_keys from our perspective
app_key1_pub = bytes.fromhex("357b42c3d42d240b8ee4fd322d4d1fd5a39e74caad4f373249ea24cd51ea8b1c521c653a0c949de5922c98d1dbfa3990f97bcfc175902ccf")
app_key2_pub = bytes.fromhex("1f3adde48598fac9234ccf6146f7052b208bf1d698a868ea8d26277b89b9a4829b5089c6e2ccbbbea0629695358f1ff3aa9dbe2cb46bbbc4")

# pqX3dhSnd (us): dh1=DH(r1,s2), dh2=DH(r2,s1), dh3=DH(r2,s2)
# pqX3dhRcv (app): dh1=DH(s1,r2), dh2=DH(s2,r1), dh3=DH(s2,r2)

# By DH commutativity: DH(A_pub, B_priv) = DH(B_pub, A_priv)
# Our dh1 = DH(app_key1, our_priv2) = DH(our_pub2, app_priv1) 
# App's dh1 = DH(our_key1, app_priv2)
# Our dh2 = DH(app_key2, our_priv1) = DH(our_pub1, app_priv2) = App's dh1!

# So: Our dh1 = App's dh2, Our dh2 = App's dh1, Our dh3 = App's dh3

our_dh1 = bytes.fromhex("24b7b65fd6e0ea720efa85ec2706e8d75a570af96321f858e13e5191ceda07d78bce1b79df357f59d6a9a4ab3a99904e91628d176db01803")
our_dh2 = bytes.fromhex("6733925c564a10c2e411eed90cd85f4b570180e97a3ee99cc08ad950cf5ae61f3f79a9a56b4cfc66fd6be256f1d787e6670ce98648887cb9")
our_dh3 = bytes.fromhex("d21162387b4f634f680baa01698d5b0f64589366850205eab75244daa15a049e8807a844bf3395ce275b5e25ba19f236d5dcb6769198e70f")

# App's DH combined: app_dh1 || app_dh2 || app_dh3 = our_dh2 || our_dh1 || our_dh3
app_dh_combined = our_dh2 + our_dh1 + our_dh3

# Our DH combined
our_dh_combined = our_dh1 + our_dh2 + our_dh3

print("Our DH combined (first 32 bytes):")
print(f"  {our_dh_combined[:32].hex()}")
print("\nApp's DH combined (first 32 bytes):")
print(f"  {app_dh_combined[:32].hex()}")

# HKDF for App
hkdf = HKDF(algorithm=hashes.SHA512(), length=96, salt=b"", info=b"SimpleXX3DH")
app_output = hkdf.derive(app_dh_combined)

app_hk = app_output[0:32]
app_nhk = app_output[32:64]
app_sk = app_output[64:96]

print(f"\nApp would compute:")
print(f"  hk:  {app_hk.hex()}")
print(f"  nhk: {app_nhk.hex()}")
print(f"  sk:  {app_sk.hex()}")

print(f"\nWe computed:")
print(f"  hk:  ba8f59f0dd47d4691faea2ef43dac8eda716e32bc846d12190cf77b0e22038b9")
