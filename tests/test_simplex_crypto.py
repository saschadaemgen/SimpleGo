#!/usr/bin/env python3
from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
import binascii

def hex_to_bytes(hex_str):
    return binascii.unhexlify(hex_str.replace(" ", ""))

def bytes_to_hex(b, limit=8):
    return "".join(f"{x:02x}" for x in b[:limit]) + "..."

def x448_dh(their_public_bytes, my_private_bytes):
    private_key = X448PrivateKey.from_private_bytes(my_private_bytes)
    public_key = X448PublicKey.from_public_bytes(their_public_bytes)
    return private_key.exchange(public_key)

def hkdf_sha512(salt, ikm, info, length):
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=length,
        salt=salt if len(salt) > 0 else None,
        info=info,
    )
    return hkdf.derive(ikm)

# NEW ESP32 VALUES (with byte reversal fix):
peer_key1_hex = "a55811a8125f220c7542145ad771e8b15b67a0025a4c4085e2fd641fd53d0ec8d0c0c194a4756629591acf9ed25ec0deb79252a37d026885"
peer_key2_hex = "d57295e911ba9ddc127a8cd26d202cf3201637401036259e896d67662e7cb33422a34a332b27f3ee31bf9dd10ddb5540a69c6874f2867d3c"
our_key1_pub_hex = "ba3a141fcc7e23c2f394d0671cdcb61614f6766aca54b2bbecc63e4e40a5d0bcd992075b39538e592d0b874d167ac3998b63a7dd0363df63"
our_key1_priv_hex = "288a296d4e707b93f8aab1ac8f8c46917d497e0731863369889867e67f424db6abb078a3af43d68c90ef7636bf88c3978fabcd762418cdec"
our_key2_pub_hex = "f19e3452bc7aca369da8d6233f7b7fc3fe642685759ac91b08e24752637fb0fb83a64b217b39b3ddb2acab5b1c360b086eef346836cf049a"
our_key2_priv_hex = "280b3efee97f9c81de2e3194e5f17293d5b2a9e1fd1a32bb12f53f5dc231589f753d4b65402233aa5472b1dd59d3513582ef2a56451ca8f7"

peer_key1 = hex_to_bytes(peer_key1_hex)
peer_key2 = hex_to_bytes(peer_key2_hex)
our_key1_priv = hex_to_bytes(our_key1_priv_hex)
our_key2_priv = hex_to_bytes(our_key2_priv_hex)

print("=" * 60)
print("X3DH TEST (with byte reversal fix)")
print("=" * 60)

print("\nDH Operations:")
dh1 = x448_dh(peer_key1, our_key2_priv)
print(f"  dh1: {bytes_to_hex(dh1)}")

dh2 = x448_dh(peer_key2, our_key1_priv)
print(f"  dh2: {bytes_to_hex(dh2)}")

dh3 = x448_dh(peer_key2, our_key2_priv)
print(f"  dh3: {bytes_to_hex(dh3)}")

print("\nX3DH HKDF:")
salt = b'\x00' * 64
ikm = dh1 + dh2 + dh3
kdf_output = hkdf_sha512(salt, ikm, b"SimpleXX3DH", 96)

print(f"  hk:  {bytes_to_hex(kdf_output[0:32])}")
print(f"  nhk: {bytes_to_hex(kdf_output[32:64])}")
print(f"  rk:  {bytes_to_hex(kdf_output[64:96])}")

print("\n" + "=" * 60)
print("ESP32 OUTPUT (target):")
print("  dh1: 4fcc6e86c981086a...")
print("  dh2: b798486f8ce8b4b8...")
print("  dh3: c9b425b0f2de8bb5...")
print("  hk:  3e5c67721b1dd874...")
print("  rk:  c2a6adbdd40440df...")
print("=" * 60)
