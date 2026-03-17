from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

# Chain key from root KDF:
ck = bytes.fromhex("0e27c51732b40db9a17a39656eb001411473dc1ba19248198d5abafee87ed1da")

# Chain KDF: HKDF(salt="", ikm=chain_key, info="SimpleXChainRatchet", len=96)
hkdf = HKDF(
    algorithm=hashes.SHA512(),
    length=96,
    salt=b"",  # empty salt
    info=b"SimpleXChainRatchet",
)
output = hkdf.derive(ck)

new_ck = output[0:32]
mk = output[32:64]
ivs = output[64:96]

msg_iv = ivs[0:16]
header_iv = ivs[16:32]

print(f"new_ck:    {new_ck.hex()}")
print(f"mk:        {mk.hex()}")
print(f"msg_iv:    {msg_iv.hex()}")
print(f"header_iv: {header_iv.hex()}")

print("\nESP32 calculated:")
print("chain_key_in:  0e27c51732b40db9...")
print("message_key:   0698f360fd353919...")
print("msg_iv:        a3250c880a93e9e0...")
print("header_iv:     3b5d95bcf4fda583...")
