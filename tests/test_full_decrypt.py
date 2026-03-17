from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# From latest ESP32 log - the encrypted message
enc_msg = bytes.fromhex("007c00035cdea25069191b4dcbfe206389a560a5d7ff8f85d9231c748f62d18b5283b3d100583f85097598d7d4fd54b93a90c0fa73de62fce15370fb55733fcf6f5387b2ac64f75ea1ebe019119dc1c7189dc7a100d7016d7d66bbe872e8934059eea3bc07f6e77e7493d5535fb35a2375c3ef9165b8a3d760fdc50745ea")

# Header key (from X3DH with swap)
hk = bytes.fromhex("403bfafb07525b09b69c4cd97f21a766ff98a006ec092d58642d416d6afe89f9")

# rcAD (our_key1 || peer_key1)
our_key1 = bytes.fromhex("01bf1d1594061385d2009eb7d0938044934a511d074f5f1540c03cfb01d53deee8b5bca2c9dffb1d958de48903b2bc30a73e14626161edd8")
peer_key1 = bytes.fromhex("b7256cdbabfd33dd6a270504cf22bb327f758700cdf4aebab819ee010a6d18d58ce7d8860a5321ff239e628e8c50bdc3b7159722fe6ce5f0")
rcAD = our_key1 + peer_key1

print(f"rcAD len: {len(rcAD)}")  # 112

# Parse EncRatchetMessage
p = 0
em_header_len = (enc_msg[p] << 8) | enc_msg[p+1]; p += 2
print(f"emHeader len: {em_header_len}")  # Should be 124

em_header = enc_msg[p:p+em_header_len]; p += em_header_len
payload_tag = enc_msg[p:p+16]; p += 16
payload = enc_msg[p:]

print(f"emHeader: {em_header[:20].hex()}...")
print(f"payload_tag: {payload_tag.hex()}")
print(f"payload len: {len(payload)}")

# Parse emHeader (EncMessageHeader)
hp = 0
eh_version = (em_header[hp] << 8) | em_header[hp+1]; hp += 2
eh_iv = em_header[hp:hp+16]; hp += 16
eh_tag = em_header[hp:hp+16]; hp += 16
eh_body_len = (em_header[hp] << 8) | em_header[hp+1]; hp += 2
eh_body = em_header[hp:hp+eh_body_len]

print(f"\nehVersion: {eh_version}")
print(f"ehIV: {eh_iv.hex()}")
print(f"ehTag: {eh_tag.hex()}")
print(f"ehBody len: {eh_body_len}")

# Decrypt header
aesgcm = AESGCM(hk)
try:
    decrypted_header = aesgcm.decrypt(eh_iv, eh_body + eh_tag, rcAD)
    print(f"\n✅ Header decrypted: {decrypted_header[:20].hex()}...")
except Exception as e:
    print(f"\n❌ Header decrypt failed: {e}")
