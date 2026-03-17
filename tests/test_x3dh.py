from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey

# Keys
peer_key1 = bytes.fromhex("357b42c3d42d240b8ee4fd322d4d1fd5a39e74caad4f373249ea24cd51ea8b1c521c653a0c949de5922c98d1dbfa3990f97bcfc175902ccf")
peer_key2 = bytes.fromhex("1f3adde48598fac9234ccf6146f7052b208bf1d698a868ea8d26277b89b9a4829b5089c6e2ccbbbea0629695358f1ff3aa9dbe2cb46bbbc4")
our_key2_priv = bytes.fromhex("4c83b84737334aa7b226a7933e254c75a970ea03f5766bce051f6ee38304dfe6e03648f3dbb6ef18505c7a93cc2d2a4dd03901753aea6d92")
our_key1_priv = bytes.fromhex("acc7fbaf335826a99844890775b40f029fc3433f9a3f73fd73fa6a18c93c6c3940f8edaca4f8d8820ae845d3b07e866ba9d021647b4c49ea")

# Create key objects
priv1 = X448PrivateKey.from_private_bytes(our_key1_priv)
priv2 = X448PrivateKey.from_private_bytes(our_key2_priv)
pub1 = X448PublicKey.from_public_bytes(peer_key1)
pub2 = X448PublicKey.from_public_bytes(peer_key2)

# DH calculations
dh1 = priv2.exchange(pub1)
dh2 = priv1.exchange(pub2)
dh3 = priv2.exchange(pub2)

print(f"dh1: {dh1.hex()}")
print(f"dh2: {dh2.hex()}")
print(f"dh3: {dh3.hex()}")

# ESP32 said:
print("\nESP32 calculated:")
print("dh1: 24b7b65fd6e0ea72...")
print("dh2: 6733925c564a10c2...")
print("dh3: d21162387b4f634f...")
