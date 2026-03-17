#!/usr/bin/env python3
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import binascii

# Test data from ESP32 log (msg 0)
header_key = bytes.fromhex("26f7f3304a1c6b085e2ce7c0ff077cd6087a1cc79ca05693272e66c361fc7984")
header_iv = bytes.fromhex("1efbb1ac6d756a88f0f5983ce15937fc")
rcAD = bytes.fromhex("5a2a28d801470ad58b35ce93538453dff4e8cf7d2724accf4eb81504d605e6ab0312b795606aba93d45dc2315838fa456efa1226e252f211fd75add9ebb1087d7e962535e04d6af0e4f03c17f6f2a790ddc6d6430beec02e5fe8bee3c587b04b27195c5d8468dc89a3b7b699dd0c2c7f")
msg_header_plain = bytes.fromhex("004f0002443042300506032b656f033900052e80bed1643ec5793cb52122127c6a44bcab10227587c23f10f0ab5b0ade4427c8dc1ffee26784b6333384dd7f0a88837745c5e15b6524000000000000000023232323232323")
header_tag = bytes.fromhex("f9b3ab07239dd17fd527525655a8f852")
encrypted_header = bytes.fromhex("2aa5391a8ff694d2dc8b9ad5fa43edae27df716e222aa7ddd5947c3457f26bbdbe069f612a38de3ebd0d24b6c8cd182cf8ed0e71e44afd47c9104e25da4b436c4abb7f88328df0cfa6fd95ffe35f056598b300579054f307")

print("=== AES-GCM Header Encryption Test ===")
print(f"Key length: {len(header_key)} bytes")
print(f"IV length: {len(header_iv)} bytes")
print(f"AAD (rcAD) length: {len(rcAD)} bytes")
print(f"Plaintext length: {len(msg_header_plain)} bytes")
print(f"Expected ciphertext length: {len(encrypted_header)} bytes")
print(f"Expected tag length: {len(header_tag)} bytes")
print()

# Try to encrypt
aesgcm = AESGCM(header_key)
ciphertext_with_tag = aesgcm.encrypt(header_iv, msg_header_plain, rcAD)
ciphertext = ciphertext_with_tag[:-16]
tag = ciphertext_with_tag[-16:]

print(f"Computed ciphertext: {ciphertext.hex()}")
print(f"Expected ciphertext: {encrypted_header.hex()}")
print(f"Ciphertext match: {ciphertext == encrypted_header}")
print()
print(f"Computed tag: {tag.hex()}")
print(f"Expected tag: {header_tag.hex()}")
print(f"Tag match: {tag == header_tag}")
print()

# Try to decrypt with ESP32's encrypted data
try:
    decrypted = aesgcm.decrypt(header_iv, encrypted_header + header_tag, rcAD)
    print(f"✅ Decryption SUCCESS!")
    print(f"Decrypted: {decrypted.hex()}")
    print(f"Matches plaintext: {decrypted == msg_header_plain}")
except Exception as e:
    print(f"❌ Decryption FAILED: {e}")
