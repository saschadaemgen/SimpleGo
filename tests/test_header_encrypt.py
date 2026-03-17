from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# Keys
header_key = bytes.fromhex("ba8f59f0dd47d4691faea2ef43dac8eda716e32bc846d12190cf77b0e22038b9")
header_iv = bytes.fromhex("3b5d95bcf4fda5830322ba4fc1431c4c")

# rcAD (our_key1 || peer_key1)
our_key1 = bytes.fromhex("c8c668c294ca48043df9cf382dff5f04a420a6cf3c82694cba8df92d53a832b35e1317a58a3b381aaad893fbd7c260f9e104f2cc32e13a98")
peer_key1 = bytes.fromhex("357b42c3d42d240b8ee4fd322d4d1fd5a39e74caad4f373249ea24cd51ea8b1c521c653a0c949de5922c98d1dbfa3990f97bcfc175902ccf")
rcAD = our_key1 + peer_key1
print(f"rcAD len: {len(rcAD)}")  # Should be 112

# MsgHeader plaintext from ESP32 logs
msg_header_plain = bytes.fromhex("004f0003443042300506032b656f033900d9c6f650c5455d7069c7212c8e09000e64a53673499dadae095194e228a8eaee71f31060ee6738f66b1f20e7e55d6aaf94cf9d332d580ae7000000000000000023232323232323")
print(f"MsgHeader len: {len(msg_header_plain)}")  # Should be 88

# AES-GCM encrypt
aesgcm = AESGCM(header_key)
ciphertext_with_tag = aesgcm.encrypt(header_iv, msg_header_plain, rcAD)

ciphertext = ciphertext_with_tag[:-16]
tag = ciphertext_with_tag[-16:]

print(f"ciphertext: {ciphertext.hex()}")
print(f"tag:        {tag.hex()}")

print("\nESP32 calculated:")
print("encrypted_header: aedbc417bca455458411fffe63e88c6648145f2be7488b924e355fdcf655e1695905b77ac79f42706f3eabf219a3d5fa64261619c489aa363c5df22598115e6ca856c4d0e5a51e5a4dcdb34f5e73ff5518b358bf6081d1f1")
print("header_tag:       adf26cc0659deae89806959cf65bfb3d")
