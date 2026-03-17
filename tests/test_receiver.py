from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# From X3DH (with swap):
sk = bytes.fromhex("5707ece6e5ca89574dedeec7f610b2d293bfca441674cd91c6d23af45282690b")  # ratchetKey
hk = bytes.fromhex("403bfafb07525b09b69c4cd97f21a766ff98a006ec092d58642d416d6afe89f9")  # sndHK

# App (receiver) uses sndHK as rcNHKr to decrypt header
# After decrypting header, app does ratchet step:
# - rcDHRr = msgDHRs from header = our_key2_pub
# - ss = DH(rcDHRr, rcDHRs) but app uses its own rcDHRs!

# The issue: App's rcDHRs is DIFFERENT from our_key2!
# App generates NEW keys when it creates the invitation!

print("=== The Problem ===")
print("App uses its own DH keys (from invitation), not ours!")
print("In initRcvRatchet, app has:")
print("  rcDHRs = app's OWN key2 private")
print("  After ratchet step: ss = DH(our_pub_key2, app_priv_key2)")
print("")
print("We compute in rootKdf:")
print("  ss = DH(app_pub_key2, our_priv_key2)")
print("")
print("By commutativity, these are EQUAL!")
print("So the chain_key SHOULD match...")
print("")
print("Let me check if we're using the correct keys for rootKdf...")

# What we pass to rootKdf:
# - ratchetKey = sk from X3DH
# - rcDHRr = peer_key2 (app's public key2)
# - rcDHRs = our_key2 (our private key2)

# What app passes to rootKdf in ratchetStep:
# - rcRK = ratchetKey from X3DH (same sk)
# - rcDHRr = msgDHRs from header = our_key2_pub
# - rcDHRs = app's key2 private

# DH outputs:
# Us: DH(peer_pub_key2, our_priv_key2) = dh3
# App: DH(our_pub_key2, app_priv_key2) = dh3 (by commutativity)

# So both use the same dh3 for rootKdf!

dh3 = bytes.fromhex("12fba3bbb83d0bbe2b540e6b46e0e1f2f85c28e6ec2c5c66f2c1aa17b0c52c3e34a46bb3e8e1d4c9e8b6f4a2e0c8d6b4a2e0c8d6b4a2e0c8")

# Wait, that's not right. Let me get the actual dh3 from the logs.
# From ESP32: dh3: 12fba3bb...

# Actually the issue might be in HOW we compute rootKdf
# Let me check the inputs...

print("Checking rootKdf inputs:")
print(f"  salt (ratchetKey): {sk[:8].hex()}...")
print(f"  dh3 should be: 12fba3bb...")
