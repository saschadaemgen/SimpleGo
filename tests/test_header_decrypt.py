from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# From latest ESP32 log
header_key = bytes.fromhex("1b97e59e49cedc12de2f379e9af451b247f76c0e0986e562a93074630bad342a")
header_iv = bytes.fromhex("7bed814eac55da7c85d22272d286eec2")
header_tag = bytes.fromhex("8e527c0955e58950054aa99abf2e95e4")
encrypted_header = bytes.fromhex("47cf7109fc48a5a6dbe7fb6ad90ebc9ecfdb195a81b60e1ba3736c10f6c86ac507bc611b9e75498a825f8c3e7eb489abdbaebc8a4215828355a36a7b570093ea653ad2f111b7b920d5388aefb9ccdf945c84f88dbfff7982")

# rcAD from log
rcAD = bytes.fromhex("5c046cb6a4cb42aceeb4ae04f51d5faa5d5410bc444c22c5892c0f81aa25f2c1324085d30653e0313fc13b0a46f641acbab99d251cffefc8bca4d5ea89feaf6bbaccd224f93f6d70291cd7bdd885b98d0ef43d163364e8fdf47f6336c3d270b5e4d2292b392b320f9b8bdf5744d28195")

print(f"rcAD length: {len(rcAD)}")  # Should be 112
print(f"encrypted_header length: {len(encrypted_header)}")  # Should be 88

# AES-GCM decrypt
aesgcm = AESGCM(header_key)
try:
    decrypted = aesgcm.decrypt(header_iv, encrypted_header + header_tag, rcAD)
    print(f"\n✅ Header decrypted successfully!")
    print(f"Decrypted header: {decrypted[:20].hex()}...")
except Exception as e:
    print(f"\n❌ Header decrypt FAILED: {e}")

# Now test what the APP would use for rcAD
# App receives our_key1 in the AgentConfirmation E2E params
# App uses: sender_key1 || receiver_key1 = our_key1 || peer_key1
our_key1 = bytes.fromhex("5c046cb6a4cb42aceeb4ae04f51d5faa5d5410bc444c22c5892c0f81aa25f2c1324085d30653e0313fc13b0a46f641acbab99d251cffefc8")
peer_key1 = bytes.fromhex("bca4d5ea89feaf6bbaccd224f93f6d70291cd7bdd885b98d0ef43d163364e8fdf47f6336c3d270b5e4d2292b392b320f9b8bdf5744d28195")

app_rcAD = our_key1 + peer_key1
print(f"\nOur rcAD:  {rcAD[:16].hex()}...")
print(f"App rcAD:  {app_rcAD[:16].hex()}...")
print(f"rcAD match: {rcAD == app_rcAD}")
