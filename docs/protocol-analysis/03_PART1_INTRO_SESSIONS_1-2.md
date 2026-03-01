![SimpleGo](../gfx/sg_multi_agent_ft_header.png)


# SimpleX Protocol Analysis - A_VERSION Error Debugging

## 🚀 WE ARE THE FIRST! 🚀
**First native ESP32 implementation of the SimpleX Messaging Protocol!**

---

## ⚠️ DOCUMENT RULES ⚠️

**THIS DOCUMENT IS NEVER SHORTENED, ONLY EXTENDED!**

1. **No deletions** - Even incorrect theories remain, but are marked as ❌ DISPROVEN
2. **Every change is logged** - Maintain changelog at the end
3. **Document code changes** - Every fix is recorded here
4. **For chat continuity** - Take this document when starting new chats
5. **Capture everything** - Theories, assumptions, discoveries, EVERYTHING

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [A_VERSION Error - Locations](#1-a_version-error---locations-in-haskell-source)
3. [The Three Critical Version Checks](#2-the-three-critical-version-checks-for-agentconfirmation)
4. [Message Structure Analysis](#3-message-structure-analysis)
5. [Version Ranges](#4-version-ranges-supported)
6. [Encoding Rules](#5-encoding-rules-critical-discoveries)
7. [ESP32 Implementation](#6-current-esp32-implementation---byte-analysis)
8. [Next Steps](#7-next-steps)
9. [Changelog](#8-changelog)
10. [Open Questions](#9-open-questions)
11. [Code Architecture Overview](#10-code-architecture-overview)
12. [Theories & Assumptions](#11-theories--assumptions-to-test)
13. [ESP32 Code Reference](#12-esp32-code-reference)
14. [Current Error Status](#13-current-error-status)
15. [A_MESSAGE Error Analysis](#14-a_message-error-analysis) ← NEW!
16. [SMPQueueInfo Encoding](#15-smpqueueinfo-encoding) ← NEW!
17. [X3DH Key Agreement Analysis](#17-x3dh-key-agreement-analysis-2026-01-23) ← NEW!
18. [Updated Error Status 2026-01-23](#18-updated-error-status-2026-01-23) ← NEW!
19. [New Theories 2026-01-23](#19-new-theories-2026-01-23) ← NEW!
20. [Double Ratchet Format Analysis](#20-double-ratchet-format-analysis) ← NEW!
21. [Extended Changelog](#21-extended-changelog) ← NEW!
22. [Next Steps 2026-01-23](#22-next-steps-2026-01-23) ← NEW!
23. [Open Questions 2026-01-23](#23-open-questions-2026-01-23) ← NEW!
24. [CRITICAL BUGS FOUND Session 2](#24-critical-bugs-found-2026-01-23-session-2) ← NEW!
25. [Updated Error Status Session 2](#25-updated-error-status-2026-01-23-session-2) ← NEW!
26. [Code Fixes Session 2](#26-code-fixes-session-2) ← NEW!
27. [Extended Changelog Session 2](#27-extended-changelog-session-2) ← NEW!
28. [Open Questions Session 2](#28-open-questions-2026-01-23-session-2) ← NEW!
29. [Port Encoding Bug](#29-port-encoding-bug-2026-01-23-session-2) ← NEW!
30. [Payload AAD Analysis](#30-payload-aad-analysis-2026-01-23-session-2) ← NEW!
31. [queueMode Bug](#31-queuemode-bug-2026-01-23-session-2) ← NEW!
32. [Current Status Session 2](#32-current-status-2026-01-23-session-2) ← NEW!
33. [Updated Bug Status Final](#33-updated-bug-status-2026-01-23-session-2-final) ← NEW!
34. [Extended Changelog Final](#34-extended-changelog-2026-01-23-session-2-final) ← NEW!
35. [Open Questions Final](#35-open-questions-2026-01-23-session-2-final) ← NEW!
36. [Session 3 Overview](#36-session-3-overview-2026-01-23) ← SESSION 3!
37. [Bug 1: ClientMessage Padding](#37-bug-1-clientmessage-padding-was-completely-missing-2026-01-23-session-3) ← SESSION 3!
38. [Bug 2: Buffer Sizes Stack Overflow](#38-bug-2-buffer-sizes-too-small---stack-overflow-2026-01-23-session-3) ← SESSION 3!
39. [Bug 3: Payload AAD 235 bytes](#39-bug-3-payload-aad-wrong---112-instead-of-235-bytes-2026-01-23-session-3) ← SESSION 3!
40. [Bug 4: Ratchet Padding MAIN PROBLEM](#40-bug-4--ratchet-internal-padding-missing---main-problem--2026-01-23-session-3) ← SESSION 3! 🔥
41. [Bug Status Session 3](#41-updated-bug-status-2026-01-23-session-3) ← SESSION 3!
42. [Changelog Session 3](#42-extended-changelog-2026-01-23-session-3) ← SESSION 3!
43. [Open Questions Session 3](#43-open-questions-2026-01-23-session-3) ← SESSION 3!
44. [Next Steps Session 3](#44-next-steps-2026-01-23-session-3) ← SESSION 3!
45. [Session 3 Continuation Overview](#45-session-3-continuation-overview-2026-01-24) ← 2026-01-24! 🎉
46. [Bug 4 Update: Ratchet Padding FIXED](#46-bug-4-update-ratchet-padding---fixed-2026-01-24) ← 2026-01-24!
47. [Bugs 5-8: Buffer Overflow Cascade](#47-bugs-5-8-buffer-overflow-cascade-2026-01-24) ← 2026-01-24!
48. [Current Connection Status](#48-current-connection-status-2026-01-24) ← 2026-01-24!
49. [Log Evidence for Progress](#49-log-evidence-for-progress-2026-01-24) ← 2026-01-24!
50. [Why 15KB for 50 Bytes? - Privacy](#50-why-15kb-for-50-bytes---padding-for-privacy-2026-01-24) ← 2026-01-24!
51. [Bug Status 2026-01-24](#51-updated-bug-status-2026-01-24) ← 2026-01-24!
52. [Changelog 2026-01-24](#52-extended-changelog-2026-01-24) ← 2026-01-24!
53. [Open Questions 2026-01-24](#53-open-questions-2026-01-24) ← 2026-01-24!
54. [Next Steps 2026-01-24](#54-next-steps-2026-01-24) ← 2026-01-24!
55. [SimpleGo Version Update](#55-simplego-version-update-2026-01-24) ← 2026-01-24!
56. [Padding Sizes Double Ratchet](#56-padding-sizes-for-double-ratchet---verified-2026-01-24) ← UPDATE 2! 🔥
57. [HELLO Message Format](#57-hello-message-format---complete-analysis-2026-01-24) ← UPDATE 2!
58. [Status: 2x A_MESSAGE](#58-current-status-2x-a_message-error-2026-01-24) ← UPDATE 2!
59. [Open Hypotheses](#59-open-hypotheses-2026-01-24) ← UPDATE 2!
60. [Debugging Strategy](#60-debugging-strategy-2026-01-24) ← UPDATE 2!
61. [Bug Status Update 2](#61-updated-bug-status-2026-01-24-update-2) ← UPDATE 2!
62. [Changelog Update 2](#62-extended-changelog-2026-01-24-update-2) ← UPDATE 2!
63. [Open Questions Update 2](#63-open-questions-2026-01-24-update-2) ← UPDATE 2!
64. [HELLO Format Verified](#64-hello-message-format---verified-2026-01-24-session-4) ← SESSION 4!
65. [EncRatchetMessage Format](#65-encratchetmessage-format---verified-2026-01-24-session-4) ← SESSION 4!
66. [KDF Output Order Bug](#66--kdf-output-order-bug---found--fixed-2026-01-24-session-4) ← SESSION 4! 🔥
67. [rcAD Format Verified](#67-rcad-associated-data-format---verified-2026-01-24-session-4) ← SESSION 4!
68. [SPKI vs Raw Keys Hypothesis](#68--new-hypothesis-spki-vs-raw-keys-in-rcad-2026-01-24-session-4) ← SESSION 4! 🔥
69. [Bug Status Session 4](#69-updated-bug-status-2026-01-24-session-4) ← SESSION 4!
70. [Changelog Session 4](#70-extended-changelog-2026-01-24-session-4) ← SESSION 4!
71. [Open Questions Session 4](#71-open-questions-2026-01-24-session-4) ← SESSION 4!
72. [SimpleGo v0.1.22](#72-simplego-version-update-2026-01-24-session-4) ← SESSION 4!
73. [Session 4 Part 2 Overview](#73-session-4-part-2-overview-2026-01-24) ← S4T2! 🔥
74. [Length Encoding Rules](#74-length-encoding-rules---complete-analysis-2026-01-24) ← S4T2!
75. [Bug: MsgHeader Length](#75--bug-msgheader-length-encoding-2026-01-24) ← S4T2! 🔥
76. [Bug: ehBody Length](#76--bug-ehbody-length-encoding-2026-01-24) ← S4T2! 🔥
77. [Corrected Wire Format](#77-corrected-wire-format-2026-01-24) ← S4T2!
78. [Encode Type Analysis](#78-encode-type-analysis-2026-01-24) ← S4T2!
79. [Bug Status S4T2](#79-updated-bug-status-2026-01-24-s4t2) ← S4T2!
80. [Changelog S4T2](#80-extended-changelog-2026-01-24-s4t2) ← S4T2!
81. [SimpleGo v0.1.23](#81-simplego-version-update-2026-01-24-s4t2) ← S4T2!
82. [Session 4 Finale Overview](#82-session-4-finale-overview-2026-01-24) ← FINALE! 🎉
83. [Status: Still 2x A_MESSAGE](#83-status-still-2x-a_message-2026-01-24-finale) ← FINALE!
84. [Fix 1: E2E Key Length Word16](#84-fix-1-e2e-key-length---word16-2026-01-24-finale) ← FINALE!
85. [Fix 2: emHeader Length 124](#85-fix-2-emheader-length---124-bytes-2026-01-24-finale) ← FINALE!
86. [Fix 3: HELLO prevMsgHash Word16](#86-fix-3-hello-prevmsghash-length---word16-2026-01-24-finale) ← FINALE!
87. [Fix 4: MsgHeader DH Key Word16](#87-fix-4-msgheader-dh-key-length---word16-2026-01-24-finale) ← FINALE!
88. [Fix 7: Payload AAD 236](#88-fix-7-payload-aad-length---236-bytes-2026-01-24-finale) ← FINALE!
89. [Fix 8: ChainKDF IV Order](#89--fix-8-chainkdf-iv-order---critical-2026-01-24-finale) ← FINALE! 🔥
90. [Complete Wire Format](#90-fully-verified-wire-format-2026-01-24-finale) ← FINALE!
91. [HKDF Complete](#91-hkdf-key-derivation---fully-documented-2026-01-24-finale) ← FINALE!
92. [AAD Complete](#92-aad-associated-data---fully-documented-2026-01-24-finale) ← FINALE!
93. [Status A_MESSAGE Remains](#93-current-status---a_message-remains-2026-01-24-finale) ← FINALE!
94. [Next Debug Steps](#94-next-debug-steps-2026-01-24-finale) ← FINALE!
95. [Bug Status Finale](#95-updated-bug-status-2026-01-24-finale) ← FINALE!
96. [Changelog Finale](#96-extended-changelog-2026-01-24-finale) ← FINALE!
97. [Open Questions Finale](#97-open-questions-2026-01-24-finale) ← FINALE!
98. [SimpleGo v0.1.24](#98-simplego-version-update-2026-01-24-finale) ← FINALE!
99. [🎉 BREAKTHROUGH: wolfSSL X448](#99--the-bug-was-found-wolfssl-x448-byte-order-2026-01-24) ← BREAKTHROUGH! 🎉
100. [Fix 9: X448 Byte Order](#100-fix-9-wolfssl-x448-byte-order---the-critical-fix-2026-01-24) ← BREAKTHROUGH! 🎉
101. [Complete Bug List (9 Bugs)](#101-complete-bug-list-2026-01-24-final) ← BREAKTHROUGH!
102. [SimpleX Encoding Convention](#102-insight-simplex-encoding-convention-2026-01-24) ← BREAKTHROUGH!
103. [Lessons Learned](#103-debug-methodology---lessons-learned-2026-01-24) ← BREAKTHROUGH!
104. [Status after BREAKTHROUGH](#104-current-status-after-breakthrough-2026-01-24) ← BREAKTHROUGH!
105. [Changelog BREAKTHROUGH](#105-extended-changelog-2026-01-24-breakthrough) ← BREAKTHROUGH!
106. [SimpleGo v0.1.25](#106-simplego-version-update-2026-01-24-breakthrough) ← BREAKTHROUGH! 🎉
107. [Session 5 Overview](#107-session-5-overview---cryptography-solved-2026-01-24) ← SESSION 5! 🎉
108. [Crypto Verification Python vs ESP32](#108-crypto-verification---python-vs-esp32-2026-01-24) ← SESSION 5!
109. [Error Remains Despite Correct Crypto](#109--error-remains-despite-correct-cryptography-2026-01-24) ← SESSION 5!
110. [New Hypothesis: E2E Version/KEM](#110--new-hypothesis-e2e-version--kem-mismatch-2026-01-24) ← SESSION 5! 🔥
111. [Verified Wire Format](#111-verified-wire-format-2026-01-24-session-5) ← SESSION 5!
112. [Python Test Script](#112-python-test-script-for-future-comparisons-2026-01-24) ← SESSION 5!
113. [Next Debug Steps](#113-next-debug-steps-2026-01-24-session-5) ← SESSION 5!
114. [Bug Status Session 5](#114-updated-bug-status-2026-01-24-session-5) ← SESSION 5!
115. [Changelog Session 5](#115-extended-changelog-2026-01-24-session-5) ← SESSION 5!
116. [Open Questions Session 5](#116-open-questions-2026-01-24-session-5) ← SESSION 5!
117. [SimpleGo v0.1.26](#117-simplego-version-update-2026-01-24-session-5) ← SESSION 5!
118. [Lessons Learned Update](#118-lessons-learned-update-2026-01-24-session-5) ← SESSION 5!
119. [Session 6 Overview](#119-session-6-overview---handshake-flow-analysis-2026-01-24) ← SESSION 6!
120. [Contact Address Handshake Flow](#120-contact-address-handshake-flow-qc-2026-01-24) ← SESSION 6!
121. [Wire Format Detail View](#121-verified-wire-format---detail-view-2026-01-24-session-6) ← SESSION 6!
122. [App Console Logs Analysis](#122-app-console-logs-analysis-2026-01-24-session-6) ← SESSION 6!
123. [Open Hypotheses](#123-open-hypotheses-2026-01-24-session-6) ← SESSION 6! 🔥
124. [Crypto Verification Summary](#124-crypto-verification-summary-2026-01-24-session-6) ← SESSION 6!
125. [Next Debug Steps](#125-next-debug-steps-2026-01-24-session-6) ← SESSION 6!
126. [Bug Status Session 6](#126-updated-bug-status-2026-01-24-session-6) ← SESSION 6!
127. [Changelog Session 6](#127-extended-changelog-2026-01-24-session-6) ← SESSION 6!
128. [Open Questions Session 6](#128-open-questions-2026-01-24-session-6) ← SESSION 6!
129. [SimpleGo v0.1.26 (S6)](#129-simplego-version-update-2026-01-24-session-6) ← SESSION 6!
130. [Lessons Learned Session 6](#130-lessons-learned-2026-01-24-session-6) ← SESSION 6!
131. [Bug 10: Port Encoding](#131--bug-10-smpqueueinfo-port-encoding-2026-01-24-s6) ← S6 CONT! 🔥
132. [Haskell Source Analysis](#132-haskell-source-analysis---wire-format-verified-2026-01-24-s6) ← S6 CONT!
133. [Version-dependent Encoding](#133-version-dependent-encoding-2026-01-24-s6) ← S6 CONT!
134. [SMPQueueInfo Encoding](#134-smpqueueinfo-encoding-for-clientversion4-2026-01-24-s6) ← S6 CONT!
135. [Raw AgentConnInfoReply](#135-raw-agentconninforeply-analysis-2026-01-24-s6) ← S6 CONT!
136. [Open Questions S6 Cont](#136-open-questions-2026-01-24-s6-continuation) ← S6 CONT!
137. [Bug Status S6 Cont](#137-updated-bug-status-2026-01-24-s6-continuation) ← S6 CONT!
138. [Changelog S6 Cont](#138-extended-changelog-2026-01-24-s6-continuation) ← S6 CONT!
139. [Next Steps S6 Cont](#139-next-steps-2026-01-24-s6-continuation) ← S6 CONT!
140. [SimpleGo v0.1.27](#140-simplego-version-update-2026-01-24-s6-continuation) ← S6 CONT!
141. [Session 6 FINALE Overview](#141-session-6-finale-overview-2026-01-24) ← S6 FINALE!
142. [Bug 11: smpQueues Count](#142-bug-11-smpqueues-list-length-2026-01-24-s6-finale) ← S6 FINALE! 🔥
143. [Bug 12: queueMode Nothing](#143-bug-12-queuemode-for-nothing-2026-01-24-s6-finale) ← S6 FINALE! 🔥
144. [Verified Raw Output](#144-verified-raw-output-2026-01-24-s6-finale) ← S6 FINALE!
145. [Current Crypto Values](#145-current-crypto-values-2026-01-24-s6-finale) ← S6 FINALE!
146. [Complete Bug List (12)](#146-complete-bug-list-12-bugs---sessions-1-6) ← S6 FINALE!
147. [Remaining Hypotheses](#147-remaining-hypotheses-2026-01-24-s6-finale) ← S6 FINALE!
148. [Next Debug Steps](#148-next-debug-steps-2026-01-24-s6-finale) ← S6 FINALE!
149. [Bug Status S6 Finale](#149-updated-bug-status-2026-01-24-s6-finale) ← S6 FINALE!
150. [Changelog S6 Finale](#150-extended-changelog-2026-01-24-s6-finale) ← S6 FINALE!
151. [SimpleGo v0.1.28](#151-simplego-version-update-2026-01-24-s6-finale) ← S6 FINALE!
152. [Lessons Learned Overall](#152-lessons-learned---overall-overview-sessions-1-6) ← S6 FINALE!
153. [Session 7 Overview](#153-session-7-overview---aes-gcm-16-byte-iv-analysis-2026-01-24) ← SESSION 7! 🎉
154. [AES-GCM Verification](#154--aes-gcm-verification---python-vs-mbedtls-2026-01-24) ← SESSION 7! 🎉
155. [Overall Status S7](#155-overall-status-after-session-7-2026-01-24) ← SESSION 7!
156. [New Hypothesis: MsgHeader Parsing](#156--new-focus-hypothesis-msgheader-parsing-2026-01-24) ← SESSION 7! 🔥
157. [Python AES-GCM Script](#157-python-test-script-for-aes-gcm-2026-01-24-s7) ← SESSION 7!
158. [Bug Status S7](#158-updated-bug-status-2026-01-24-session-7) ← SESSION 7!
159. [Changelog S7](#159-extended-changelog-2026-01-24-session-7) ← SESSION 7!
160. [SimpleGo v0.1.29](#160-simplego-version-update-2026-01-24-session-7) ← SESSION 7!
161. [Open Questions S7](#161-open-questions-2026-01-24-session-7) ← SESSION 7!
162. [Lessons Learned S7](#162-lessons-learned-2026-01-24-session-7) ← SESSION 7!
163. [1-byte Length Hypothesis](#163-hypothesis-1-byte-vs-2-byte-length-prefix-2026-01-24-s7f) ← S7 CONT!
164. [Analysis: Hypothesis Wrong](#164-analysis-why-was-the-hypothesis-wrong-2026-01-24-s7f) ← S7 CONT!
165. [Code Rollback](#165-code-rollback-performed-2026-01-24-s7f) ← S7 CONT!
166. [Exclusion List](#166-updated-exclusion-list-2026-01-24-s7f) ← S7 CONT!
167. [New Hypothesis: X3DH Asymmetry](#167--new-focus-hypothesis-x3dh-parameter-asymmetry-2026-01-24-s7f) ← S7 CONT! 🔥
168. [Bug Status S7F](#168-updated-bug-status-2026-01-24-s7f) ← S7 CONT!
169. [Changelog S7F](#169-extended-changelog-2026-01-24-s7f) ← S7 CONT!
170. [SimpleGo Status S7F](#170-simplego-version-update-2026-01-24-s7f) ← S7 CONT!
171. [🏆 FIRST native SMP Implementation!](#171--historic-significance-first-native-smp-implementation-2026-01-24) ← S7 DEEP! 🏆
172. [A_MESSAGE vs A_CRYPTO](#172-a_message-vs-a_crypto---error-analysis-2026-01-24) ← S7 DEEP!
173. [🔥 Tail Encoding Hypothesis](#173--new-hypothesis-tail-encoding-without-length-prefix-2026-01-24) ← S7 DEEP! 🔥
174. [Flexible 0xFF Length Encoding](#174-flexible-length-encoding-0xff-flag-2026-01-24) ← S7 DEEP!
175. [Three Length Prefix Strategies](#175-three-length-prefix-strategies-2026-01-24) ← S7 DEEP!
176. [🔥 Potential Bug Identified](#176--potential-bug-identified-2026-01-24) ← S7 DEEP! 🔥
177. [Bug Status S7 Deep](#177-updated-bug-status-2026-01-24-s7-deep-research) ← S7 DEEP!
178. [Changelog S7 Deep](#178-extended-changelog-2026-01-24-s7-deep-research) ← S7 DEEP!
179. [Session 7 Summary](#179-session-7-overall-summary-2026-01-24) ← S7 DEEP!
180. [SimpleGo v0.1.29 S7 Deep](#180-simplego-version-update-2026-01-24-s7-deep-research) ← S7 DEEP!

---

## 🌳 SimpleX Protocol Stack - Tree Structure

```
SimpleX Messaging Protocol
│
├── 📡 Transport Layer (TLS 1.3)
│   └── ALPN: "smp/1"
│
├── 📦 SMP Protocol Layer
│   ├── Commands: NEW, KEY, SUB, SEND, ACK, OFF, DEL
│   ├── Server Responses: IDS, MSG, OK, ERR, END
│   └── ClientMsgEnvelope (for SEND)
│       ├── PubHeader
│       │   ├── phVersion (Word16 BE) ← SMP Client Version 1-4
│       │   └── phE2ePubDhKey (Maybe X25519)
│       ├── cmNonce (24 bytes)
│       └── cmEncBody (encrypted)
│
├── 🔐 Encryption Layer (crypto_box)
│   └── ClientMessage (after decryption)
│       ├── PrivHeader
│       │   ├── PHConfirmation = 'K' + Ed25519 SPKI (NO length prefix!)
│       │   └── PHEmpty = '_'
│       └── Body (AgentMsgEnvelope)
│
├── 🤝 Agent Protocol Layer
│   ├── AgentMsgEnvelope Types:
│   │   ├── AgentConfirmation (Tag 'C') ← WE ARE HERE!
│   │   ├── AgentInvitation
│   │   ├── AgentMsgEnvelope (Tag 'M')
│   │   └── AgentRatchetKey
│   │
│   └── AgentConfirmation Structure:
│       ├── agentVersion (Word16 BE) ← Agent Version 2-7
│       ├── 'C' Tag
│       ├── e2eEncryption_ (Maybe E2ERatchetParams)
│       │   └── E2ERatchetParams
│       │       ├── e2eVersion (Word16 BE) ← E2E Version 2-3
│       │       ├── e2ePubKey1 (X448 SPKI)
│       │       ├── e2ePubKey2 (X448 SPKI)
│       │       └── e2eKEM (optional, v3+)
│       └── encConnInfo (Ratchet-encrypted)
│           └── After decryption: AgentMessage
│               ├── 'I' = AgentConnInfo (initiating party)
│               └── 'D' = AgentConnInfoReply (joining party) ← US!
│
├── 🔄 Double Ratchet Layer
│   ├── X3DH Key Agreement
│   ├── Header Encryption
│   └── Message Encryption (ChaCha20-Poly1305)
│
└── 💬 Chat Protocol Layer (JSON)
    └── {"event":"x.info","params":{...}}
```

---

## Overview
This document systematically tracks the code analysis of the SimpleX Messaging Protocol, specifically for debugging the A_VERSION error when sending AgentConfirmation messages from ESP32.

**Project:** SimpleGo - Native SimpleX Implementation for ESP32  
**Problem:** ~~Server accepts messages (OK), but SimpleX App shows "A_VERSION" Error (2x)~~  
**Current Status:** A_VERSION FIXED! New error: A_MESSAGE (Parsing Error)  
**Date:** 2026-01-22

---

## 1. A_VERSION Error - Locations in Haskell Source

### 1.1 All Locations Where A_VERSION is Thrown

**Source:** `grep -rn "A_VERSION" ~/simplexmq/src/ --include="*.hs"`

| File | Line | Context |
|------|------|---------|
| Agent.hs | 756 | Version check |
| Agent.hs | 1042 | Version check |
| Agent.hs | 1046 | Version check |
| Agent.hs | 1099 | Version check |
| Agent.hs | 1185 | Version check |
| **Agent.hs** | **2707** | **phVer (SMP Client Version) check** |
| Agent.hs | 2893 | (commented out) |
| **Agent.hs** | **2908** | **agentVersion compatible check** |
| **Agent.hs** | **2913** | **e2eVersion check** |
| Agent.hs | 3057 | Version check |
| Agent.hs | 3064 | qInfo compatible check |
| Agent.hs | 3139 | e2eVersion check |
| Agent.hs | 3229 | Version check |
| Protocol.hs | 1911 | Error Definition |

### 1.2 Error Definition (Protocol.hs:1911)

```haskell
  | -- | prohibited SMP/agent message
    A_PROHIBITED {prohibitedErr :: String}
  | -- | incompatible version of SMP client, agent or encryption protocols
    A_VERSION
  | -- | failed signature, hash or senderId verification
    A_LINK {linkErr :: String}
```

**Insight:** A_VERSION is thrown for **three different** version incompatibilities:
1. SMP Client Protocol Version
2. Agent Protocol Version
3. E2E Encryption Version

---

## 2. The Three Critical Version Checks (for AgentConfirmation)

### 2.1 Line 2707 - SMP Client Version (phVer) - FIRST CHECK

**Source:** `sed -n '2700,2720p' ~/simplexmq/src/Simplex/Messaging/Agent.hs`

```haskell
processClientMsg srvTs msgFlags msgBody = do
  clientMsg@SMP.ClientMsgEnvelope {cmHeader = SMP.PubHeader phVer e2ePubKey_} <-
    parseMessage msgBody
  clientVRange <- asks $ smpClientVRange . config
  unless (phVer `isCompatible` clientVRange || phVer <= agreedClientVerion) . throwE $ AGENT A_VERSION
  case (e2eDhSecret, e2ePubKey_) of
    (Nothing, Just e2ePubKey) -> do
      let e2eDh = C.dh' e2ePubKey e2ePrivKey
      decryptClientMessage e2eDh clientMsg >>= \case
        (SMP.PHConfirmation senderKey, AgentConfirmation {e2eEncryption_, encConnInfo, agentVersion}) ->
          smpConfirmation srvMsgId conn (Just senderKey) e2ePubKey e2eEncryption_ encConnInfo phVer agentVersion >> ack
```

**🔍 ANALYSIS:**
- `phVer` is extracted from the `PubHeader` of the `ClientMsgEnvelope`
- Checked: `phVer `isCompatible` smpClientVRange`
- This is the **SMP Client Protocol Version** in the OUTER unencrypted header!

**📦 Where does phVer come from:**
```
ClientMsgEnvelope
  └── cmHeader :: PubHeader
        └── phVersion :: VersionSMPC  ← THIS IS BEING CHECKED!
        └── phE2ePubDhKey :: Maybe PublicKeyX25519
```

**⚠️ IMPORTANT:** This version must be in the **unencrypted** part of the message, BEFORE the nonce!

---

### 2.2 Line 2908 - COMBINED Check (Agent + SMP Client Version) - SECOND CHECK

**Source:** `sed -n '2900,2920p' ~/simplexmq/src/Simplex/Messaging/Agent.hs`

```haskell
smpConfirmation srvMsgId conn' senderKey e2ePubKey e2eEncryption encConnInfo phVer agentVersion = do
  logServer "<--" c srv rId $ "MSG <CONF>:" <> logSecret' srvMsgId
  AgentConfig {smpClientVRange, smpAgentVRange, e2eEncryptVRange} <- asks config
  let ConnData {pqSupport} = toConnData conn'
      -- checking agreed versions to continue connection in case of client/agent version downgrades
      compatible =
        (agentVersion `isCompatible` smpAgentVRange || agentVersion <= agreedAgentVersion)
          && (phVer `isCompatible` smpClientVRange || phVer <= agreedClientVerion)
  unless compatible $ throwE $ AGENT A_VERSION
```

**🔍 ANALYSIS:**
- This is a **COMBINED** check!
- Checks BOTH: `agentVersion` AND `phVer`
- `agentVersion` comes from the decrypted AgentConfirmation
- `phVer` was already extracted above from the PubHeader

**📦 What is being checked:**
1. `agentVersion `isCompatible` smpAgentVRange` - Agent Protocol Version (2-7)
2. `phVer `isCompatible` smpClientVRange` - SMP Client Version

**💡 INSIGHT:** If we get 2x A_VERSION, it could be that:
- Once at line 2707 (first phVer check)
- Once at line 2908 (combined check)

---

### 2.3 Line 2913 - E2E Encryption Version - THIRD CHECK

```haskell
case status of
  New -> case (conn', e2eEncryption) of
    -- party initiating connection
    (RcvConnection _ _, Just (CR.AE2ERatchetParams _ e2eSndParams@(CR.E2ERatchetParams e2eVersion _ _ _))) -> do
      unless (e2eVersion `isCompatible` e2eEncryptVRange) (throwE $ AGENT A_VERSION)
      (pk1, rcDHRs, pKem) <- withStore c (`getRatchetX3dhKeys` connId)
      rcParams <- liftError cryptoError $ CR.pqX3dhRcv pk1 rcDHRs pKem e2eSndParams
```

**🔍 ANALYSIS:**
- `e2eVersion` is extracted from `E2ERatchetParams`
- Checked: `e2eVersion `isCompatible` e2eEncryptVRange`
- This is the **E2E Encryption Version** (2-3)

**📦 Where does e2eVersion come from:**
```
AgentConfirmation
  └── e2eEncryption_ :: Maybe (SndE2ERatchetParams 'X448)
        └── E2ERatchetParams
              └── e2eVersion :: VersionE2E  ← THIS IS BEING CHECKED!
              └── e2ePubKey1
              └── e2ePubKey2
              └── e2eKEM (optional, only v3+)
```

---

### 2.4 🎯 SUMMARY: The Three Version Checks

| Check | Line | What | Where | Valid Range |
|-------|------|------|-------|-------------|
| 1 | 2707 | phVer (SMP Client) | PubHeader (unencrypted) | smpClientVRange |
| 2 | 2908 | agentVersion + phVer | AgentConfirmation + PubHeader | smpAgentVRange (2-7) |
| 3 | 2913 | e2eVersion | E2ERatchetParams | e2eEncryptVRange (2-3) |

**🔥 MAIN SUSPECT:** Check 1 (Line 2707) - The `phVer` in PubHeader!

The ESP32 might not be sending an SMP Client Version in the PubHeader, or it's in the wrong place!

---

## 3. Message Structure Analysis

### 3.1 Outer Layer: ClientMsgEnvelope

**File:** Protocol.hs  
**Encoding:**

```haskell
data ClientMsgEnvelope = ClientMsgEnvelope
  { cmHeader :: PubHeader,
    cmNonce :: C.CbNonce,      -- 24 bytes
    cmEncBody :: ByteString
  }

data PubHeader = PubHeader
  { phVersion :: VersionSMPC,           -- Word16 BE
    phE2ePubDhKey :: Maybe C.PublicKeyX25519  -- Maybe tag + SPKI
  }

smpEncode (PubHeader v k) = smpEncode (v, k)
smpEncode ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody} =
    smpEncode (cmHeader, cmNonce, Tail cmEncBody)
```

**Wire Format:**
```
[2B Version BE][Maybe-Tag][44B X25519 SPKI (if '1')][24B Nonce][Encrypted Body]
```

### 3.2 Encrypted Layer: ClientMessage (PrivHeader + Body)

**File:** Protocol.hs

```haskell
data ClientMessage = ClientMessage PrivHeader ByteString

data PrivHeader
  = PHConfirmation C.APublicAuthKey  -- 'K' + Ed25519 SPKI
  | PHEmpty                           -- '_'

smpEncode = \case
  PHConfirmation k -> "K" <> smpEncode k
  PHEmpty -> "_"
```

**Wire Format (after decryption):**
```
['K'][44B Ed25519 SPKI Auth Key][AgentMsgEnvelope...]
```

### 3.3 Inner Layer: AgentConfirmation

**File:** Agent/Protocol.hs

```haskell
data AgentMessage (p :: AParty)
  = AgentConnInfo ConnInfo
  | AgentConnInfoReply (NonEmpty SMPQueueInfo) ConnInfo
  | ...

data AgentMsgEnvelope
  = AgentConfirmation
      { agentVersion :: VersionSMPA,              -- Word16 BE
        e2eEncryption_ :: Maybe (SndE2ERatchetParams 'C.X448),
        encConnInfo :: ByteString
      }
  | ...

smpEncode AgentConfirmation {agentVersion, e2eEncryption_, encConnInfo} =
  smpEncode (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)
```

**Wire Format:**
```
[2B Agent Version BE]['C'][Maybe-Tag '1'][E2E Params][encConnInfo...]
```

### 3.4 E2E Ratchet Parameters

**File:** Crypto/Ratchet.hs

```haskell
data E2ERatchetParams (a :: Algorithm) = E2ERatchetParams
  { e2eVersion :: VersionE2E,
    e2ePubKey1 :: PublicKey a,
    e2ePubKey2 :: PublicKey a,
    e2eKEM :: Maybe KEMPublicKey
  }

-- Encoding for v2 (without PQ):
smpEncode (E2ERatchetParams v k1 k2 kem_)
  | v >= pqRatchetE2EEncryptVersion = smpEncode (v, k1, k2, kem_)
  | otherwise = smpEncode (v, k1, k2)  -- v2: no KEM
```

**Wire Format (v2, without PQ):**
```
[2B E2E Version BE][1B len][68B X448 SPKI Key1][1B len][68B X448 SPKI Key2]
```

---

## 4. Version Ranges (Supported)

### 4.1 SMP Agent Versions (smpAgentVRange)

**File:** Agent/Protocol.hs:286-308

| Version | Name | Feature |
|---------|------|---------|
| 2 | duplexHandshakeSMPAgentVersion | Duplex handshake |
| 3 | ratchetSyncSMPAgentVersion | Ratchet sync |
| 4 | deliveryRcptsSMPAgentVersion | Delivery receipts |
| 5 | pqdrSMPAgentVersion | Post-quantum double ratchet |
| 6 | sndAuthKeySMPAgentVersion | Secure reply queues |
| 7 | ratchetOnConfSMPAgentVersion | Ratchet on confirmation (current) |

**Supported Range:** 2-7

### 4.2 E2E Encryption Versions (e2eEncryptVRange)

**File:** Crypto/Ratchet.hs:145-155

| Version | Name | Feature |
|---------|------|---------|
| 2 | kdfX3DHE2EEncryptVersion | KDF X3DH |
| 3 | pqRatchetE2EEncryptVersion | Post-Quantum Ratchet (current) |

**Supported Range:** 2-3

### 4.3 SMP Client Versions (smpClientVRange)

**File:** Protocol.hs:277-293

```haskell
initialSMPClientVersion :: VersionSMPC
initialSMPClientVersion = VersionSMPC 1

srvHostnamesSMPClientVersion :: VersionSMPC
srvHostnamesSMPClientVersion = VersionSMPC 2

sndAuthKeySMPClientVersion :: VersionSMPC
sndAuthKeySMPClientVersion = VersionSMPC 3

shortLinksSMPClientVersion :: VersionSMPC
shortLinksSMPClientVersion = VersionSMPC 4

currentSMPClientVersion :: VersionSMPC
currentSMPClientVersion = VersionSMPC 4

supportedSMPClientVRange :: VersionRangeSMPC
supportedSMPClientVRange = mkVersionRange initialSMPClientVersion currentSMPClientVersion
```

| Version | Name | Feature |
|---------|------|---------|
| 1 | initialSMPClientVersion | Initial version |
| 2 | srvHostnamesSMPClientVersion | Server hostnames |
| 3 | sndAuthKeySMPClientVersion | Send auth key |
| 4 | shortLinksSMPClientVersion | Short links (current) |

**Supported Range:** 1-4

**🔥 CRITICAL FOR ESP32:** We must send version 4 (or 1-4) in the PubHeader!

---

### 4.4 Summary of All Versions

| Protocol Layer | Version Range | Current | ESP32 sends | Status |
|----------------|---------------|---------|-------------|--------|
| SMP Client (phVer) | 1-4 | 4 | 4 (0x00 0x04) | ✅ |
| SMP Agent (agentVersion) | 2-7 | 7 | 7 (0x00 0x07) | ✅ |
| E2E Encryption (e2eVersion) | 2-3 | 3 | 2 (0x00 0x02) | ✅ |

---

## 5. Encoding Rules (Critical Discoveries)

### 5.1 Maybe Type Encoding

**File:** Encoding.hs:114-115

```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
```

**IMPORTANT:** Uses ASCII characters, NOT binary!
- `Nothing` = ASCII '0' (0x30)
- `Just x` = ASCII '1' (0x31) + encoded value

### 5.2 ByteString Length Encoding

**File:** Encoding.hs:100-104

```haskell
instance Encoding ByteString where
  smpEncode s = B.cons (lenEncode $ B.length s) s

lenEncode :: Int -> Char
lenEncode = w2c . fromIntegral  -- Single byte!
```

**IMPORTANT:** 1-byte length prefix for normal ByteStrings

### 5.3 Large Type Encoding

**File:** Encoding.hs:136

```haskell
instance Encoding Large where
  smpEncode (Large s) = smpEncode @Word16 (fromIntegral $ B.length s) <> s
```

**IMPORTANT:** 2-byte (Word16 BE) length prefix for Large wrapper

---

## 6. Current ESP32 Implementation - Byte Analysis

### 6.1 What We Send

```
Client Message: 1873 bytes
SEND body: 1907 bytes

AgentConfirmation Header: 00 07 43 31 00 02 44 30 42 30 05 06 03 2b 65 6f ...
                          ^^^^^ ^^ ^^ ^^^^^ ^^ ^^^^^^^^^^^^^^^^^^^^^
                          v7    C  '1' v2   68 X448 SPKI OID...
```

### 6.2 🔥 THE CORRECT ClientMsgEnvelope Format (per Haskell)

```haskell
instance Encoding PubHeader where
  smpEncode (PubHeader v k) = smpEncode (v, k)  -- (Version, Maybe Key)

instance Encoding ClientMsgEnvelope where
  smpEncode ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody} =
    smpEncode (cmHeader, cmNonce, Tail cmEncBody)
```

**Correct Wire Format:**
```
┌──────────────────────────────────────────────────────────────────────┐
│                     ClientMsgEnvelope                               │
├──────────────┬─────────┬────────────────┬───────────┬───────────────┤
│ phVersion    │ Maybe   │ X25519 SPKI    │ Nonce     │ Encrypted     │
│ (2B BE)      │ Tag '1' │ (44 bytes)     │ (24B)     │ Body          │
├──────────────┼─────────┼────────────────┼───────────┼───────────────┤
│ 00 04        │ 31      │ 30 2a 30 05... │ [random]  │ [ciphertext]  │
│ SMP v4       │ = Just  │ X25519 Key     │           │               │
└──────────────┴─────────┴────────────────┴───────────┴───────────────┘
```

---

## 7. Next Steps

### 7.1 ✅ DONE: phVersion in PubHeader 

The SMP Client Version is correctly in ClientMsgEnvelope!

### 7.2 ✅ DONE: Nonce Encoding

Nonce is correctly sent as 24 bytes without length prefix.

### 7.3 🔥 CURRENT: Fix SMPQueueInfo clientVersion!

**The Problem:** `clientVersion = 14` in SMPQueueInfo is WRONG!

**Valid:** 1-4 (supportedSMPClientVRange)

**The Fix:**
```c
// BEFORE (WRONG):
buf[p++] = 0x00;
buf[p++] = 0x0E;  // 14 - TOO HIGH!

// AFTER (CORRECT):
buf[p++] = 0x00;
buf[p++] = 0x04;  // 4 - currently supported!
```

---

## 8. Changelog

| Date | Change |
|------|--------|
| 2026-01-22 | Document created |
| 2026-01-22 | A_VERSION locations identified (14 locations in Agent.hs) |
| 2026-01-22 | Message structure documented |
| 2026-01-22 | Version ranges documented (SMP Client 1-4, Agent 2-7, E2E 2-3) |
| 2026-01-22 | Encoding rules documented (Maybe='0'/'1', ByteString=1B len) |
| 2026-01-22 | Lines 2700-2720 and 2900-2920 analyzed |
| 2026-01-22 | SMP Client Version in ESP32 code CONFIRMED (it's there!) |
| 2026-01-22 | CbNonce encoding analyzed |
| 2026-01-22 | Tree structure and table of contents added |
| 2026-01-22 | A_VERSION FIXED - all version checks pass! |
| 2026-01-22 | CryptoInvalidMsgError FIXED - 2-byte padding prefix |
| 2026-01-22 | PrivHeader length prefix REMOVED |
| 2026-01-22 | A_MESSAGE error analysis started |
| 2026-01-22 | **SMPQueueInfo clientVersion=14 identified as BUG!** |
| 2026-01-22 | **SMPQueueInfo clientVersion fixed from 14 to 4** |
| 2026-01-22 | **MsgHeader X448 Key format BUG found!** ESP32 was sending 0x01+raw instead of SPKI |
| 2026-01-22 | **MsgHeader Fix:** X448 Key now as ByteString with SPKI (68 bytes) |

---

## 9. Open Questions

1. ✅ ~~Is the ESP32 sending the SMP Client Version (phVersion) correctly?~~ **YES, it's there!** (0x00 0x04)
2. ✅ ~~How is the nonce encoded in ClientMsgEnvelope?~~ **Directly 24 bytes, no length prefix**
3. ✅ ~~Is the ClientMsgEnvelope format exactly correct?~~ **YES**
4. ✅ ~~Which versions does the current SimpleX App accept?~~ **SMP 1-4, Agent 2-7, E2E 2-3**
5. ✅ ~~Is the PrivHeader ('K' + Ed25519 Key) encoded correctly?~~ **YES, without length prefix**
6. ❓ Is the SMPServer encoding in SMPQueueInfo correct?
7. 🔥 **clientVersion in SMPQueueInfo is 14 instead of 1-4!**

---

## 10. Code Architecture Overview

### 10.1 Haskell Source Files - Where is What?

```
~/simplexmq/src/Simplex/Messaging/
│
├── Protocol.hs          ← SMP Protocol Layer
│   ├── ClientMsgEnvelope (Line 1067)
│   ├── PubHeader (Line 1074)
│   ├── PrivHeader (Line 1093)
│   ├── ClientMessage (Line 1091)
│   ├── SMPQueueInfo (Encoding!)
│   └── VersionSMPC Definitions (Line 277-293)
│
├── Encoding.hs          ← Encoding Rules
│   ├── Maybe Encoding (Line 114-115) - '0'/'1' ASCII!
│   ├── ByteString Encoding (Line 100-104) - 1 byte length
│   ├── Large Encoding (Line 136) - 2 byte length
│   └── Tuple Encoding
│
├── Crypto.hs            ← Cryptographic Functions
│   ├── CbNonce (Line 1350) - 24 byte nonce
│   ├── cbEncrypt (Line 1268)
│   ├── crypto_box functions
│   └── Key types (X25519, Ed25519, X448)
│
├── Crypto/Ratchet.hs    ← Double Ratchet
│   ├── E2ERatchetParams (Line ~239)
│   ├── e2eEncryptVRange (Line 145-155)
│   ├── EncRatchetMessage (Line 773)
│   └── rcDecrypt (Line 997)
│
├── Agent.hs             ← Agent Protocol Logic
│   ├── A_VERSION checks (Line 756, 1042, 1046, 1099, 1185, 2707, 2908, 2913, 3057, 3064, 3139, 3229)
│   ├── A_MESSAGE checks (Line 2780, 2813, 2897)
│   ├── processClientMsg (Line 2704)
│   ├── smpConfirmation (Line 2900)
│   └── Message handling
│
└── Agent/Protocol.hs    ← Agent Protocol Types
    ├── AgentMsgEnvelope (AgentConfirmation, etc.)
    ├── AgentMessage (AgentConnInfo, AgentConnInfoReply)
    ├── SMPQueueInfo
    ├── smpAgentVRange (Line 286-308)
    ├── A_MESSAGE Error Definition
    └── Message type encodings
```

### 10.2 CbNonce (Crypto Box Nonce) Details

**File:** Crypto.hs:1350-1381

```haskell
newtype CbNonce = CryptoBoxNonce {unCbNonce :: ByteString}
  deriving (Eq, Show)

randomCbNonce :: TVar ChaChaDRG -> STM CbNonce
randomCbNonce = fmap CryptoBoxNonce . randomBytes 24
```

**Insight:** 
- CbNonce is simply a 24-byte ByteString
- No special encoding - used directly
- Generated with `randomBytes 24`

---

## 11. Theories & Assumptions (to Test)

### Theory 1: ❌ phVersion missing
**Status:** DISPROVEN  
**Evidence:** ESP32 code shows `client_msg[cmp++] = 0x00; client_msg[cmp++] = 0x04;`

### Theory 2: ✅ FIXED - Nonce is now SEPARATE
**Status:** IMPLEMENTED  
**Fix:** Nonce is now sent separately before the ciphertext

### Theory 3: ❌ DISPROVEN - Length Prefix for PrivHeader Keys
**Status:** DISPROVEN  
**Evidence:** Crypto.hs shows that APublicAuthKey is encoded WITHOUT length prefix!

### Theory 4: ❌ DISPROVEN - CryptoInvalidMsgError
**Status:** DISPROVEN / FIXED  
**Fix:** 2-byte padding prefix added for crypto_box

### Theory 5: ❓ E2E Params Encoding
**Status:** PROBABLY OK  
**Assumption:** E2ERatchetParams appear to be correct

### Theory 6: ✅ CONFIRMED - crypto_box Padding missing
**Status:** FIXED!  
**Date:** 2026-01-22

**The Problem:**
SimpleX uses a special padding format for crypto_box:
```
Encrypted = crypto_box([2B payload_len BE][payload][optional random padding])
```

**The Fix (smp_peer.c):**
```c
// Build PADDED plaintext with 2-byte length prefix
uint8_t padded[2048];
int padded_len = 0;

// 2-Byte Length Prefix (Big Endian)
uint16_t payload_len = pp;
padded[padded_len++] = (payload_len >> 8) & 0xFF;  // High byte
padded[padded_len++] = payload_len & 0xFF;         // Low byte

// Copy actual plaintext
memcpy(&padded[padded_len], plaintext, pp);
padded_len += pp;

// crypto_box with PADDED instead of plaintext!
crypto_box_easy(&encrypted[24], padded, padded_len, nonce, ...);
```

**Result:** `CryptoInvalidMsgError` is GONE! ✅

### Theory 7: ✅ CONFIRMED - PrivHeader without Length Prefix
**Status:** FIXED!  
**Date:** 2026-01-22

**The Problem:**
ESP32 was sending a length prefix after 'K', but SimpleX expects the SPKI directly!

**Haskell Code (Crypto.hs):**
```haskell
instance Encoding APublicAuthKey where
  smpEncode (APublicAuthKey a k) = aKeyTag a <> BA.convert (encodePubKey k)
  -- NO smpEncode on the result = NO length prefix!
```

**The Fix:** Length prefix after 'K' removed.
```c
plaintext[pp++] = 'K';
// NO length prefix here!
memcpy(&plaintext[pp], ED25519_SPKI_HEADER, 12);
pp += 12;
memcpy(&plaintext[pp], our_queue.rcv_auth_public, 32);
pp += 32;
```

**Result:** A_VERSION is GONE! ✅

### Theory 8: ✅ CONFIRMED - EncMessageHeader IV/AuthTag without Length Prefix
**Status:** FIXED!  
**Date:** 2026-01-22

**Haskell Code (Crypto.hs):**
```haskell
instance Encoding IV where
  smpEncode = unIV  -- NO length prefix! Directly 16 bytes!

instance Encoding AuthTag where
  smpEncode = BA.convert . unAuthTag  -- NO length prefix! Directly 16 bytes!
```

**The Fix (smp_ratchet.c):**
- IV and AuthTag are now written DIRECTLY, without length prefix
- em_header is now 123 bytes (not 125)

### Theory 9: ✅ FIXED - SMPQueueInfo clientVersion wrong!
**Status:** FIXED!  
**Date:** 2026-01-22

**The Problem:**
```c
// clientVersion = 14 (2 bytes BE) - WRONG!
buf[p++] = 0x00;
buf[p++] = 0x0E;  // 14 is outside of 1-4!
```

**Haskell (Protocol.hs):**
```haskell
supportedSMPClientVRange :: VersionRangeSMPC
supportedSMPClientVRange = mkVersionRange initialSMPClientVersion currentSMPClientVersion
-- = mkVersionRange 1 4
```

**The Fix:**
```c
// clientVersion = 4 (2 bytes BE) - CORRECT!
buf[p++] = 0x00;
buf[p++] = 0x04;
```

**Result:** Fixed, but A_MESSAGE remains! ❌

### Theory 10: 🔥 CURRENT - MsgHeader X448 Key Format wrong!
**Status:** TO FIX!  
**Date:** 2026-01-22

**The Problem:**
ESP32 was sending the X448 Key in MsgHeader with a simple tag byte instead of SPKI format!

**Haskell (Crypto.hs):**
```haskell
encodePubKey = toPubKey $ encodeASNObj . publicToX509
-- = SPKI Format (ASN.1 X.509 SubjectPublicKeyInfo)
```

**ESP32 was sending (WRONG):**
```c
header[p++] = X448_KEY_TAG;  // 0x01 - WRONG!
memcpy(&header[p], dh_public, 56);  // Raw bytes
// = 57 bytes for Key
```

**SimpleX expects (CORRECT):**
```
[1B len=68][12B SPKI Header][56B raw key]
// = 69 bytes for Key (with length prefix)
```

**X448 SPKI Header (12 bytes):**
```
30 42 30 05 06 03 2b 65 6f 03 39 00
```
- `30 42` = SEQUENCE, 66 bytes
- `30 05 06 03 2b 65 6f` = AlgorithmIdentifier (OID 1.3.101.111 = X448)
- `03 39 00` = BIT STRING, 57 bytes (0 unused bits)

**The Fix (smp_ratchet.c, build_msg_header):**
```c
static void build_msg_header(uint8_t *header, const uint8_t *dh_public,
                             uint32_t pn, uint32_t ns) {
    memset(header, 0, MSG_HEADER_PADDED_LEN);
    
    int p = 0;
    
    // msgMaxVersion = 2
    header[p++] = 0x00;
    header[p++] = RATCHET_VERSION;
    
    // msgDHRs as ByteString with length prefix (SPKI format)
    header[p++] = 68;  // Length prefix (68 bytes SPKI)

    // X448 SPKI Header (12 bytes)
    static const uint8_t X448_SPKI_HEADER[] = {
        0x30, 0x42, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6f, 0x03, 0x39, 0x00
    };
    memcpy(&header[p], X448_SPKI_HEADER, 12);
    p += 12;

    // Raw X448 key (56 bytes)
    memcpy(&header[p], dh_public, 56);
    p += 56;
    
    // msgPN (4 bytes BE)
    header[p++] = (pn >> 24) & 0xFF;
    header[p++] = (pn >> 16) & 0xFF;
    header[p++] = (pn >> 8) & 0xFF;
    header[p++] = pn & 0xFF;
    
    // msgNs (4 bytes BE)
    header[p++] = (ns >> 24) & 0xFF;
    header[p++] = (ns >> 16) & 0xFF;
    header[p++] = (ns >> 8) & 0xFF;
    header[p++] = ns & 0xFF;
    
    // Padding (remaining bytes already zeroed by memset)
    // Total used: 2 + 1 + 68 + 4 + 4 = 79 bytes
    // Padding: 88 - 79 = 9 zero bytes
}
```

**The Calculation:**
- Before: 2 + 1 + 56 + 4 + 4 = 67 bytes (WRONG!)
- After: 2 + 1 + 68 + 4 + 4 = 79 bytes (CORRECT!)

---

## 12. ESP32 Code Reference

### 12.1 Relevant Files

```
C:\Espressif\projects\simplex_client\main\
│
├── main.c              ← Main program, WiFi, Message Loop
├── smp_peer.c          ← AgentConfirmation, HELLO
├── smp_queue.c         ← Queue Creation (NEW, KEY, SUB), SMPQueueInfo Encoding (BUG!)
├── smp_handshake.c     ← SKEY, Handshake completion
├── smp_ratchet.c       ← Double Ratchet Encryption
├── smp_x448.c          ← X448 DH, E2E Params
├── smp_crypto.c        ← crypto_box, Signatures
├── smp_parser.c        ← Message Parsing
├── smp_contacts.c      ← Contact Management
├── smp_network.c       ← TLS Connection
├── smp_utils.c         ← Base64, Encoding helpers
└── smp_globals.c       ← Global state, SPKI headers
```

### 12.2 Current ClientMsgEnvelope Code (smp_peer.c)

```c
// ========== Wrap in SMP ClientMsgEnvelope ==========
uint8_t client_msg[2000];
int cmp = 0;

// PubHeader: SMP Client Version (Word16 BE) - use version 4
client_msg[cmp++] = 0x00;
client_msg[cmp++] = 0x04;  // SMP client version 4  ✅

// Maybe tag: '1' = Just (we have a DH key)
client_msg[cmp++] = '1';   // ✅

// X25519 DH public key SPKI (so peer can decrypt) - NO length prefix!
memcpy(&client_msg[cmp], X25519_SPKI_HEADER, 12);
cmp += 12;
memcpy(&client_msg[cmp], contact->rcv_dh_public, 32);
cmp += 32;  // Total SPKI: 44 bytes ✅

// Nonce (24 bytes)
memcpy(&client_msg[cmp], nonce, 24);
cmp += 24;

// Encrypted body (ciphertext + MAC, WITHOUT nonce!)
memcpy(&client_msg[cmp], &encrypted[24], encrypted_len - 24);
cmp += encrypted_len - 24;
```

---

## 13. Current Error Status

| Error | Status | Date | Note |
|-------|--------|------|------|
| A_VERSION (2x) | ✅ FIXED | 2026-01-22 | All three version checks pass |
| CryptoInvalidMsgError | ✅ FIXED | 2026-01-22 | 2-byte padding prefix added |
| SMPQueueInfo v14 | ✅ FIXED | 2026-01-22 | clientVersion changed from 14 to 4 |
| A_MESSAGE (2x) | 🔥 CURRENT | 2026-01-22 | MsgHeader X448 Key Format wrong! |

**Next Fix:** Send X448 Key in MsgHeader as SPKI format (68 bytes instead of 57 bytes)!

---

## 14. A_MESSAGE Error Analysis (NEW!)

### 14.1 What is A_MESSAGE?

**Definition (Agent/Protocol.hs:1907):**
```haskell
data SMPAgentError
  = -- | client or agent message that failed to parse
    A_MESSAGE
```

**A_MESSAGE = "client or agent message that failed to parse"**

### 14.2 Where is A_MESSAGE Thrown?

**Source:** `grep -rn "A_MESSAGE" ~/simplexmq/src/Simplex/Messaging/Agent.hs`

| Line | Context |
|------|---------|
| 2780 | `liftEither (parse smpP (AGENT A_MESSAGE) agentMsgBody)` |
| 2813 | `liftEither (parse smpP (SEAgentError $ AGENT A_MESSAGE) agentMsgBody)` |
| 2897 | `parseMessage = liftEither . parse smpP (AGENT A_MESSAGE)` |

### 14.3 What is Being Parsed?

**Line 2780-2790:**
```haskell
liftEither (parse smpP (AGENT A_MESSAGE) agentMsgBody) >>= \case
  AgentMessage _ (A_MSG body) -> do
    logServer "<--" c srv rId $ "MSG <MSG>:" <> logSecret' srvMsgId
    notify $ MSG msgMeta msgFlags body
    pure ACKPending
  _ -> ack
```

**Line 2813:**
```haskell
liftEither (parse smpP (SEAgentError $ AGENT A_MESSAGE) agentMsgBody) >>= \case
  agentMsg@(AgentMessage APrivHeader {sndMsgId, prevMsgHash} aMessage) -> do
    -- ...
```

**Line 2897:**
```haskell
parseMessage :: Encoding a => ByteString -> AM a
parseMessage = liftEither . parse smpP (AGENT A_MESSAGE)
```

### 14.4 AgentMessage Encoding

```haskell
instance Encoding AgentMessage where
  smpEncode = \case
    AgentConnInfo cInfo -> smpEncode ('I', Tail cInfo)
    AgentConnInfoReply smpQueues cInfo -> smpEncode ('D', smpQueues, Tail cInfo)
    AgentRatchetInfo info -> smpEncode ('R', Tail info)
    AgentMessage hdr aMsg -> smpEncode ('M', hdr, aMsg)
  smpP =
    smpP >>= \case
      'I' -> AgentConnInfo . unTail <$> smpP
      'D' -> AgentConnInfoReply <$> smpP <*> (unTail <$> smpP)
      'R' -> AgentRatchetInfo . unTail <$> smpP
      'M' -> AgentMessage <$> smpP <*> smpP
      _ -> fail "bad AgentMessage"
```

**For 'D' (AgentConnInfoReply):**
1. Parse `smpQueues` (NonEmpty SMPQueueInfo)
2. Parse `connInfo` (as Tail)

**🔥 If SMPQueueInfo parsing fails → A_MESSAGE!**

---

## 15. SMPQueueInfo Encoding (NEW!)

### 15.1 Haskell Definition

```haskell
data SMPQueueInfo = SMPQueueInfo {clientVersion :: VersionSMPC, queueAddress :: SMPQueueAddress}

instance Encoding SMPQueueInfo where
  smpEncode (SMPQueueInfo clientVersion SMPQueueAddress {smpServer, senderId, dhPublicKey, queueMode})
    | clientVersion >= shortLinksSMPClientVersion = addrEnc <> maybe "" smpEncode queueMode
    | clientVersion >= sndAuthKeySMPClientVersion && sndSecure = addrEnc <> smpEncode sndSecure
    | clientVersion > initialSMPClientVersion = addrEnc
    | otherwise = smpEncode clientVersion <> legacyEncodeServer smpServer <> smpEncode (senderId, dhPublicKey)
    where
      addrEnc = smpEncode (clientVersion, smpServer, senderId, dhPublicKey)
      sndSecure = senderCanSecure queueMode
```

### 15.2 Version-dependent Encoding

| clientVersion | Encoding |
|---------------|----------|
| 1 | `clientVersion + legacyEncodeServer + senderId + dhPublicKey` |
| 2-3 | `clientVersion + smpServer + senderId + dhPublicKey` |
| 4+ | `clientVersion + smpServer + senderId + dhPublicKey + [queueMode]` |

### 15.3 Current ESP32 Code (smp_queue.c)

```c
int queue_encode_info(uint8_t *buf, int max_len) {
    int p = 0;
    
    // clientVersion = 14 (2 bytes BE) - 🔥 WRONG!
    buf[p++] = 0x00;
    buf[p++] = 0x0E;  // 14 is NOT in 1-4!
    
    // smpServer encoding...
    // senderId...
    // dhPublicKey...
}
```

### 15.4 🔥 THE BUG: clientVersion = 14

**The Problem:**
- ESP32 sends `clientVersion = 14`
- SimpleX expects `clientVersion = 1-4`
- Version 14 is far outside the supported range!

**The Fix:**
```c
// BEFORE (WRONG):
buf[p++] = 0x00;
buf[p++] = 0x0E;  // 14

// AFTER (CORRECT):
buf[p++] = 0x00;
buf[p++] = 0x04;  // 4 (current version)
```

### 15.5 SMPServer Encoding (legacyEncodeServer)

```haskell
legacyEncodeServer :: ProtocolServer p -> ByteString
legacyEncodeServer ProtocolServer {host, port, keyHash} =
  smpEncode (L.head host, port, keyHash)
```

**Format for v2+:**
```
smpEncode (clientVersion, smpServer, senderId, dhPublicKey)
```

**SMPServer for v2+:**
```
[host_count][host1_len][host1]...[port_len][port][keyhash_len][keyhash]
```

### 15.6 Current ESP32 SMPServer Encoding

```c
// smpServer encoding:
buf[p++] = 0x01;  // 1 host
buf[p++] = (uint8_t)host_len;
memcpy(&buf[p], our_queue.server_host, host_len);
p += host_len;

// Port as string
buf[p++] = (uint8_t)port_len;
memcpy(&buf[p], port_str, port_len);
p += port_len;

// KeyHash (32 bytes)
buf[p++] = 32;
memcpy(&buf[p], our_queue.server_key_hash, 32);
p += 32;
```

**❓ Question:** Is this format correct for v4? Do we need to append `queueMode`?

---

## 16. Next Steps (Priority)

1. ✅ **DONE:** Change clientVersion from 14 to 4 in `smp_queue.c`
2. 🔥 **NOW:** Send MsgHeader X448 Key as SPKI format in `smp_ratchet.c`
3. ❓ **AFTER:** Test if A_MESSAGE disappears

---

**DOCUMENT UPDATED: 2026-01-22 - MsgHeader X448 SPKI Format Bug found!**

---

## 17. X3DH Key Agreement Analysis (2026-01-23)

### 17.1 🔥 CRITICAL BUG FOUND: DH3 = DH2!

**Date:** 2026-01-23

**The Problem in old code (smp_ratchet.c):**
```c
bool ratchet_x3dh_sender(const uint8_t *peer_identity_key,
                         const uint8_t *peer_signed_prekey,
                         const x448_keypair_t *our_ephemeral) {
    // ...
    // DH1 = DH(our_ephemeral, peer_identity_key)
    x448_dh(peer_identity_key, our_ephemeral->private_key, dh1);
    
    // DH2 = DH(our_ephemeral, peer_signed_prekey)
    x448_dh(peer_signed_prekey, our_ephemeral->private_key, dh2);
    
    // DH3 = DH(our_ephemeral, peer_signed_prekey) [same as DH2 for simplicity]
    memcpy(dh3, dh2, 56);  // ← COMPLETELY WRONG! "for simplicity" was a placeholder!
```

**Log output showed:**
```
DH1: 35b009ad...
DH2: 64798a0e...
DH3: 64798a0e...  ← IDENTICAL TO DH2! WRONG!
```

### 17.2 Haskell Reference (Ratchet.hs)

**pqX3dhSnd** (us as Sender/Joiner):
```haskell
pqX3dhSnd spk1 spk2 spKem_ (E2ERatchetParams v rk1 rk2 rKem_) = do
  let initParams = pqX3dh (publicKey spk1, rk1) (dh' rk1 spk2) (dh' rk2 spk1) (dh' rk2 spk2)
--                                               ^^^^^^^^^^^    ^^^^^^^^^^^    ^^^^^^^^^^^
--                                                  DH1            DH2            DH3
```

**pqX3dhRcv** (App as Receiver/Initiator):
```haskell
pqX3dhRcv rpk1 rpk2 rpKem_ (E2ERatchetParams v sk1 sk2 sKem_) = do
  let initParams = pqX3dh (sk1, publicKey rpk1) (dh' sk2 rpk1) (dh' sk1 rpk2) (dh' sk2 rpk2)
```

### 17.3 Key Assignment

| Haskell | ESP32 Variable | Description |
|---------|----------------|-------------|
| spk1 | our_key1 | Our first keypair |
| spk2 | our_key2 | Our second keypair |
| rk1 | peer_key1 | Peer's first public key (from invitation) |
| rk2 | peer_key2 | Peer's second public key (from invitation) |

### 17.4 Correct DH Operations (as Sender)

| DH | Haskell | ESP32 Code (CORRECT) |
|----|---------|----------------------|
| DH1 | `dh' rk1 spk2` | `x448_dh(peer_key1, our_key2->private_key, dh1)` |
| DH2 | `dh' rk2 spk1` | `x448_dh(peer_key2, our_key1->private_key, dh2)` |
| DH3 | `dh' rk2 spk2` | `x448_dh(peer_key2, our_key2->private_key, dh3)` |

### 17.5 The Fix

**File 1: `main/include/smp_ratchet.h` (Lines 61-63)**

BEFORE:
```c
bool ratchet_x3dh_sender(const uint8_t *peer_identity_key,
                         const uint8_t *peer_signed_prekey,
                         const x448_keypair_t *our_ephemeral);
```

AFTER:
```c
bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2);
```

**File 2: `main/smp_ratchet.c` (Lines 160-183)**

AFTER:
```c
bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "🔐 X3DH Key Agreement (sender)...");

    uint8_t dh1[56], dh2[56], dh3[56];

    // DH1 = DH(peer_key1, our_private2) - Haskell: dh' rk1 spk2
    if (!x448_dh(peer_key1, our_key2->private_key, dh1)) {
        ESP_LOGE(TAG, "   ❌ DH1 failed");
        return false;
    }
    ESP_LOGI(TAG, "   DH1: %02x%02x%02x%02x...", dh1[0], dh1[1], dh1[2], dh1[3]);

    // DH2 = DH(peer_key2, our_private1) - Haskell: dh' rk2 spk1
    if (!x448_dh(peer_key2, our_key1->private_key, dh2)) {
        ESP_LOGE(TAG, "   ❌ DH2 failed");
        return false;
    }
    ESP_LOGI(TAG, "   DH2: %02x%02x%02x%02x...", dh2[0], dh2[1], dh2[2], dh2[3]);

    // DH3 = DH(peer_key2, our_private2) - Haskell: dh' rk2 spk2
    if (!x448_dh(peer_key2, our_key2->private_key, dh3)) {
        ESP_LOGE(TAG, "   ❌ DH3 failed");
        return false;
    }
    ESP_LOGI(TAG, "   DH3: %02x%02x%02x%02x...", dh3[0], dh3[1], dh3[2], dh3[3]);
    
    // Rest stays the same...
}
```

**File 3: `main/smp_peer.c` (Line 306)**

BEFORE:
```c
if (!ratchet_x3dh_sender(pending_peer.e2e_key1, pending_peer.e2e_key2, &e2e_params->key1)) {
```

AFTER:
```c
if (!ratchet_x3dh_sender(pending_peer.e2e_key1, pending_peer.e2e_key2, &e2e_params->key1, &e2e_params->key2)) {
```

### 17.6 Status after Fix

✅ **X3DH DH3 Bug FIXED** (2026-01-23)
❌ **A_MESSAGE Error remains** - Problem is elsewhere!

---

## 18. Updated Error Status (2026-01-23)

| Error | Status | Date | Note |
|-------|--------|------|------|
| A_VERSION (2x) | ✅ FIXED | 2026-01-22 | All three version checks pass |
| CryptoInvalidMsgError | ✅ FIXED | 2026-01-22 | 2-byte padding prefix added |
| clientVersion=14 | ✅ FIXED | 2026-01-22 | Now 4 |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-23 | Parsing Error - Double Ratchet Format? |

---

## 19. New Theories (2026-01-23)

### Theory 10: ✅ FIXED - X3DH DH3=DH2 Bug
**Status:** IMPLEMENTED  
**Date:** 2026-01-23  
**Result:** A_MESSAGE remains - so this was NOT the main problem!

### Theory 11: ❓ OPEN - pqX3dh KDF Function
**Status:** TO CHECK  
**Question:** How does pqX3dh combine the DH outputs? Are we using the same KDF info string?

**ESP32 Code:**
```c
hkdf_sha256(NULL, 0, dh_combined, 168,
            (const uint8_t *)"SimpleXX3DH", 11,
            ratchet_state.root_key, 32);
```

**To check:** Is "SimpleXX3DH" the correct info string?

### Theory 12: ❓ OPEN - E2E Keys Order in E2ERatchetParams
**Status:** TO CHECK  
**Question:** Are we sending key1.public as e2ePubKey1 and key2.public as e2ePubKey2?

### Theory 13: ❓ OPEN - Ratchet Initialization after X3DH
**Status:** TO CHECK  
**Question:** How are Header/Chain Keys derived from the Root Key?

### Theory 14: ❓ OPEN - MsgHeader msgDHRs Key
**Status:** TO CHECK  
**Question:** Which DH public key is used in MsgHeader?

---

## 20. Double Ratchet Format Analysis

### 20.1 EncRatchetMessage (from Haskell)

```haskell
encodeEncRatchetMessage :: VersionE2E -> EncRatchetMessage -> ByteString
encodeEncRatchetMessage v EncRatchetMessage {emHeader, emBody, emAuthTag} =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)

encodeLarge v s
  | v >= pqRatchetE2EEncryptVersion = smpEncode $ Large s  -- v3+: 2-byte length
  | otherwise = smpEncode s                                 -- v2: 1-byte length
```

**Wire Format for v2:**
```
[1B emHeader-len][emHeader...][16B emAuthTag][emBody... (Tail, no prefix)]
```

### 20.2 EncMessageHeader (from Haskell)

```haskell
instance Encoding EncMessageHeader where
  smpEncode EncMessageHeader {ehVersion, ehIV, ehAuthTag, ehBody} =
    smpEncode (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody
```

**Wire Format for v2:**
```
[2B ehVersion][16B ehIV][16B ehAuthTag][1B ehBody-len][88B ehBody]
= 2 + 16 + 16 + 1 + 88 = 123 bytes
```

**IMPORTANT:** IV and AuthTag have NO length prefix!

### 20.3 MsgHeader (from Haskell)

```haskell
encodeMsgHeader :: AlgorithmI a => VersionE2E -> MsgHeader a -> ByteString
encodeMsgHeader v MsgHeader {msgMaxVersion, msgDHRs, msgKEM, msgPN, msgNs}
  | v >= pqRatchetE2EEncryptVersion = smpEncode (msgMaxVersion, msgDHRs, msgKEM, msgPN, msgNs)
  | otherwise = smpEncode (msgMaxVersion, msgDHRs, msgPN, msgNs)  -- v2: no KEM
```

**Wire Format for v2:**
```
[2B msgMaxVersion][X448 SPKI as ByteString][4B msgPN BE][4B msgNs BE]
```

**X448 SPKI as ByteString:**
```
[1B len=68][68B X448 SPKI]
```

---

## 21. Extended Changelog

| Date | Change |
|------|--------|
| 2026-01-22 | Document created |
| 2026-01-22 | A_VERSION locations identified |
| 2026-01-22 | Message structure documented |
| 2026-01-22 | Version ranges documented |
| 2026-01-22 | Encoding rules documented |
| 2026-01-22 | A_VERSION FIXED |
| 2026-01-22 | CryptoInvalidMsgError FIXED |
| 2026-01-22 | PrivHeader length prefix REMOVED |
| 2026-01-22 | SMPQueueInfo clientVersion=14 identified |
| 2026-01-22 | clientVersion 14→4 FIXED |
| 2026-01-22 | IV/AuthTag length prefixes REMOVED |
| 2026-01-23 | **X3DH analysis from Haskell source** |
| 2026-01-23 | **X3DH DH3=DH2 Bug FOUND!** |
| 2026-01-23 | **ratchet_x3dh_sender: 4 parameters instead of 3** |
| 2026-01-23 | **X3DH Bug FIXED - A_MESSAGE remains** |
| 2026-01-23 | Double Ratchet format analyzed |
| 2026-01-23 | Documentation v4 created |

---

## 22. Next Steps (2026-01-23)

1. **Analyze pqX3dh** - What exactly happens after the 3 DHs?
   ```bash
   grep -B5 -A40 "pqX3dh " ~/simplexmq/src/Simplex/Messaging/Crypto/Ratchet.hs | head -60
   ```

2. **Check KDF info string** - Are we using the same string as Haskell?

3. **Analyze initSndRatchet** - How are Header/Chain Keys derived?
   ```bash
   grep -B5 -A30 "initSndRatchet" ~/simplexmq/src/Simplex/Messaging/Crypto/Ratchet.hs
   ```

4. **Check E2E Keys order** - key1 vs key2 in E2ERatchetParams

---

## 23. Open Questions (2026-01-23)

1. ✅ ~~X3DH DH3 correct?~~ **FIXED, but problem remains!**
2. ❓ What is the correct KDF info string for X3DH?
3. ❓ How are Header/Chain Keys derived from Root Key?
4. ❓ Which DH Key goes in the MsgHeader?
5. ❓ Is the E2E Key order correct (key1 vs key2)?
6. ❓ Is "SimpleXX3DH" the correct info string?
7. ❓ Is "SimpleXRatchet" the correct info string for kdf_root?
8. ❓ Is "SimpleXChain" the correct info string for kdf_chain?

---

## 24. CRITICAL BUGS FOUND 2026-01-23 Session 2

### 24.1 🔥 BUG: X3DH Salt was NULL instead of 64 zero bytes

**Haskell Code (Ratchet.hs):**
```haskell
pqX3dh (sk1, rk1) dh1 dh2 dh3 kemAccepted =
  RatchetInitParams {assocData, ratchetKey = RatchetKey sk, sndHK = Key hk, rcvNextHK = Key nhk, kemAccepted}
  where
    ...
    (hk, nhk, sk) =
      let salt = B.replicate 64 '\0'   -- ← 64 NULL BYTES!
       in hkdf3 salt dhs "SimpleXX3DH"
```

**ESP32 Code (BEFORE - WRONG):**
```c
hkdf_sha256(NULL, 0, dh_combined, 168,  // ← NULL pointer instead of 64 null bytes!
            (const uint8_t *)"SimpleXX3DH", 11,
            ratchet_state.root_key, 32);
```

**ESP32 Code (AFTER - CORRECT):**
```c
uint8_t salt[64] = {0};  // 64 null bytes like Haskell!
hkdf_sha512(salt, 64, dh_combined, 168,
            (const uint8_t *)"SimpleXX3DH", 11,
            kdf_output, 96);
```

---

### 24.2 🔥 BUG: X3DH Output was 32 bytes instead of 96 bytes

**Haskell:**
```haskell
(hk, nhk, sk) = hkdf3 salt dhs "SimpleXX3DH"
-- hkdf3 returns 96 bytes: 32 + 32 + 32
```

**ESP32 (BEFORE - WRONG):**
```c
hkdf_sha256(..., ratchet_state.root_key, 32);  // Only 32 bytes!
```

**ESP32 (AFTER - CORRECT):**
```c
uint8_t kdf_output[96];  // hk + nhk + sk
hkdf_sha512(..., kdf_output, 96);

// Split output:
memcpy(ratchet_state.header_key_send, kdf_output, 32);       // hk = sndHK
memcpy(ratchet_state.header_key_recv, &kdf_output[32], 32);  // nhk = rcvNextHK
memcpy(ratchet_state.root_key, &kdf_output[64], 32);         // sk = ratchetKey
```

---

### 24.3 🔥 BUG: HKDF used SHA256 instead of SHA512

**Haskell (Crypto.hs):**
```haskell
hkdf :: ByteArrayAccess secret => ByteString -> secret -> ByteString -> Int -> ByteString
hkdf salt ikm info n =
  let prk = H.extract salt ikm :: H.PRK SHA512   -- ← SHA512!
   in H.expand prk info n
```

**ESP32 (BEFORE - WRONG):**
```c
static int hkdf_sha256(...) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), ...);
}
```

**ESP32 (AFTER - CORRECT):**
```c
static int hkdf_sha512(...) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), ...);
}
```

---

### 24.4 🔥 BUG: KDF info strings were wrong

| Function | ESP32 (WRONG) | Haskell (CORRECT) |
|----------|---------------|-------------------|
| kdf_root | `"SimpleXRatchet"` (14 bytes) | `"SimpleXRootRatchet"` (18 bytes) |
| kdf_chain | `"SimpleXChain"` (12 bytes) | `"SimpleXChainRatchet"` (19 bytes) |

---

### 24.5 🔥 BUG: kdf_chain returned 64 bytes instead of 96 bytes

**Haskell (Ratchet.hs):**
```haskell
chainKdf :: RatchetKey -> (RatchetKey, Key, IV, IV)
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)
```

**Output:** 96 bytes = 32 (ck') + 32 (mk) + 16 (iv1) + 16 (iv2)

**ESP32 (BEFORE - WRONG):**
```c
static void kdf_chain(const uint8_t *chain_key,
                      uint8_t *message_key, uint8_t *next_chain_key) {
    uint8_t kdf_output[64];  // ONLY 64 bytes!
    // IVs were generated RANDOM!
    esp_fill_random(header_iv, GCM_IV_LEN);
    esp_fill_random(msg_iv, GCM_IV_LEN);
```

**ESP32 (AFTER - CORRECT):**
```c
static void kdf_chain(const uint8_t *chain_key,
                      uint8_t *next_chain_key, uint8_t *message_key,
                      uint8_t *msg_iv, uint8_t *header_iv) {
    uint8_t kdf_output[96];  // 32 + 32 + 16 + 16

    hkdf_sha512(NULL, 0, chain_key, 32,
                (const uint8_t *)"SimpleXChainRatchet", 19,
                kdf_output, 96);

    memcpy(next_chain_key, kdf_output, 32);       // ck'
    memcpy(message_key, kdf_output + 32, 32);     // mk
    memcpy(msg_iv, kdf_output + 64, 16);          // iv1 (message)
    memcpy(header_iv, kdf_output + 80, 16);       // iv2 (header/ehIV)
}
```

---

### 24.6 🔥 BUG: ratchet_init_sender overwrote the X3DH Key

**The Problem:**
```c
bool ratchet_init_sender(const uint8_t *peer_dh_public, const x448_keypair_t *our_key2) {
    // Use our key2 from X3DH (NOT a new key!)
    memcpy(&ratchet_state.dh_self, our_key2, sizeof(x448_keypair_t));  // ← CORRECT
    
    // Generate our sending DH keypair
    if (!x448_generate_keypair(&ratchet_state.dh_self)) {  // ← OVERWRITES!
```

**The Fix:** Remove the lines with `x448_generate_keypair`!

---

### 24.7 🔥 BUG: AssocData (AAD) missing in AES-GCM

**Haskell uses `rcAD` as AAD for header encryption:**
```haskell
(ehAuthTag, ehBody) <- encryptAEAD rcHKs ehIV ... rcAD (msgHeader ...)
```

**AssocData Definition:**
```haskell
assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1
-- = our_key1_public (56 bytes) || peer_key1 (56 bytes) = 112 bytes
```

**ESP32 (BEFORE - WRONG):**
```c
aes_gcm_encrypt(ratchet_state.header_key_send,
                header_iv, GCM_IV_LEN,
                NULL, 0,  // ← No AAD!
```

**ESP32 (AFTER - CORRECT):**
```c
// Add to ratchet_state_t:
uint8_t assoc_data[112];

// Set during X3DH:
memcpy(ratchet_state.assoc_data, our_key1->public_key, 56);
memcpy(ratchet_state.assoc_data + 56, peer_key1, 56);

// Use during encryption:
aes_gcm_encrypt(ratchet_state.header_key_send,
                header_iv, GCM_IV_LEN,
                ratchet_state.assoc_data, 112,  // ← AAD!
```

---

## 25. Updated Error Status (2026-01-23 Session 2)

| Error | Status | Date | Note |
|-------|--------|------|------|
| A_VERSION (2x) | ✅ FIXED | 2026-01-22 | All three version checks pass |
| CryptoInvalidMsgError | ✅ FIXED | 2026-01-22 | 2-byte padding prefix added |
| clientVersion=14 | ✅ FIXED | 2026-01-22 | Now 4 |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root info string wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain info string wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF instead of random |
| ratchet_init_sender key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without length prefix |
| **AssocData (AAD) missing** | 🔥 CURRENT | 2026-01-23 | Implement 112 bytes AAD |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-23 | Parsing Error |

---

## 26. Code Fixes Session 2

### 26.1 Fix: hkdf_sha512 Function

**File:** `main/smp_ratchet.c`

```c
static int hkdf_sha512(const uint8_t *salt, size_t salt_len,
                       const uint8_t *ikm, size_t ikm_len,
                       const uint8_t *info, size_t info_len,
                       uint8_t *okm, size_t okm_len) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
                        salt, salt_len, ikm, ikm_len, info, info_len,
                        okm, okm_len);
}
```

### 26.2 Fix: kdf_root with correct info string

```c
static void kdf_root(const uint8_t *root_key, const uint8_t *dh_out,
                     uint8_t *header_key, uint8_t *chain_key, uint8_t *next_root_key) {
    uint8_t kdf_output[96];

    hkdf_sha512(root_key, 32, dh_out, 56,
                (const uint8_t *)"SimpleXRootRatchet", 18,
                kdf_output, 96);

    memcpy(header_key, kdf_output, 32);
    memcpy(chain_key, kdf_output + 32, 32);
    memcpy(next_root_key, kdf_output + 64, 32);
}
```

### 26.3 Fix: kdf_chain with IVs

```c
static void kdf_chain(const uint8_t *chain_key,
                      uint8_t *next_chain_key, uint8_t *message_key,
                      uint8_t *msg_iv, uint8_t *header_iv) {
    uint8_t kdf_output[96];

    hkdf_sha512(NULL, 0, chain_key, 32,
                (const uint8_t *)"SimpleXChainRatchet", 19,
                kdf_output, 96);

    memcpy(next_chain_key, kdf_output, 32);
    memcpy(message_key, kdf_output + 32, 32);
    memcpy(msg_iv, kdf_output + 64, 16);
    memcpy(header_iv, kdf_output + 80, 16);
}
```

### 26.4 Fix: ratchet_x3dh_sender with assocData

```c
bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2) {
    // ... DH calculations ...

    uint8_t salt[64] = {0};
    uint8_t kdf_output[96];
    
    hkdf_sha512(salt, 64, dh_combined, 168,
                (const uint8_t *)"SimpleXX3DH", 11,
                kdf_output, 96);
    
    memcpy(ratchet_state.header_key_send, kdf_output, 32);
    memcpy(ratchet_state.header_key_recv, &kdf_output[32], 32);
    memcpy(ratchet_state.root_key, &kdf_output[64], 32);

    // Set AssocData!
    memcpy(ratchet_state.assoc_data, our_key1->public_key, 56);
    memcpy(ratchet_state.assoc_data + 56, peer_key1, 56);

    return true;
}
```

### 26.5 Fix: ratchet_init_sender without key generation

```c
bool ratchet_init_sender(const uint8_t *peer_dh_public, const x448_keypair_t *our_key2) {
    ESP_LOGI(TAG, "🔄 Initializing send ratchet...");

    // Use our key2 from X3DH (NOT a new key!)
    memcpy(&ratchet_state.dh_self, our_key2, sizeof(x448_keypair_t));
    
    // Store peer's DH public key (peer's key1)
    memcpy(ratchet_state.dh_peer, peer_dh_public, 56);
    
    // Perform DH
    uint8_t dh_out[56];
    if (!x448_dh(peer_dh_public, ratchet_state.dh_self.private_key, dh_out)) {
        ESP_LOGE(TAG, "   ❌ DH failed");
        return false;
    }
    
    // Derive chain key (header_key_send stays from X3DH!)
    uint8_t next_header_key[32];
    kdf_root(ratchet_state.root_key, dh_out,
             next_header_key,
             ratchet_state.chain_key_send,
             ratchet_state.root_key);
    
    ratchet_state.msg_num_send = 0;
    ratchet_state.prev_chain_len = 0;
    ratchet_state.initialized = true;
    
    return true;
}
```

### 26.6 Fix: ratchet_state_t with assoc_data

**File:** `main/include/smp_ratchet.h`

```c
typedef struct {
    uint8_t root_key[RATCHET_KEY_LEN];
    uint8_t header_key_send[RATCHET_KEY_LEN];
    uint8_t header_key_recv[RATCHET_KEY_LEN];
    uint8_t chain_key_send[RATCHET_KEY_LEN];
    uint8_t chain_key_recv[RATCHET_KEY_LEN];
    x448_keypair_t dh_self;
    uint8_t dh_peer[X448_PUBLIC_KEY_LEN];
    uint32_t msg_num_send;
    uint32_t msg_num_recv;
    uint32_t prev_chain_len;
    bool initialized;
    
    // NEW: Associated Data for AEAD
    uint8_t assoc_data[112];  // our_key1_public || peer_key1
} ratchet_state_t;
```

### 26.7 Fix: ratchet_encrypt with IVs from KDF and AAD

```c
int ratchet_encrypt(...) {
    // Derive message key AND IVs from chain
    uint8_t message_key[32];
    uint8_t next_chain_key[32];
    uint8_t msg_iv[16];
    uint8_t header_iv[16];
    kdf_chain(ratchet_state.chain_key_send, next_chain_key, message_key, msg_iv, header_iv);
    memcpy(ratchet_state.chain_key_send, next_chain_key, 32);

    // ... build msg_header ...

    // Encrypt header WITH AAD!
    if (aes_gcm_encrypt(ratchet_state.header_key_send,
                        header_iv, GCM_IV_LEN,
                        ratchet_state.assoc_data, 112,  // ← AAD!
                        msg_header, MSG_HEADER_PADDED_LEN,
                        encrypted_header, header_tag) != 0) {
```

---

## 27. Extended Changelog Session 2

| Date | Change |
|------|--------|
| 2026-01-23 S2 | **X3DH Salt Bug found**: NULL → 64 null bytes |
| 2026-01-23 S2 | **X3DH Output Bug found**: 32 → 96 bytes (hk+nhk+sk) |
| 2026-01-23 S2 | **HKDF Hash Bug found**: SHA256 → SHA512 |
| 2026-01-23 S2 | **kdf_root info string fixed**: "SimpleXRootRatchet" |
| 2026-01-23 S2 | **kdf_chain info string fixed**: "SimpleXChainRatchet" |
| 2026-01-23 S2 | **kdf_chain output fixed**: 96 bytes with IVs |
| 2026-01-23 S2 | **IVs from KDF instead of random** |
| 2026-01-23 S2 | **ratchet_init_sender fixed**: No new key |
| 2026-01-23 S2 | **emHeader format fixed**: 123 bytes (no len-prefix for IV/Tag) |
| 2026-01-23 S2 | **AssocData (AAD) Bug found**: 112 bytes missing |
| 2026-01-23 S2 | **ratchet_state_t extended**: assoc_data[112] |
| 2026-01-23 S2 | **AES-GCM calls with AAD** |
| 2026-01-23 S2 | Documentation v5 created |

---

## 28. Open Questions (2026-01-23 Session 2)

1. ✅ ~~X3DH Salt correct?~~ **FIXED: 64 null bytes**
2. ✅ ~~HKDF hash algorithm?~~ **FIXED: SHA512**
3. ✅ ~~KDF info strings correct?~~ **FIXED: SimpleXRootRatchet, SimpleXChainRatchet**
4. ✅ ~~kdf_chain output?~~ **FIXED: 96 bytes with IVs**
5. ✅ ~~ratchet_init_sender key?~~ **FIXED: use our_key2**
6. ✅ ~~emHeader format?~~ **FIXED: 123 bytes**
7. 🔥 **AssocData (AAD) correctly implemented?** - TO TEST
8. ❓ Is AAD also used in payload encryption?
9. ❓ Are all X448 Keys in SPKI format correct?

---

## 29. Port Encoding Bug (2026-01-23 Session 2)

### 29.1 🔥 BUG: Port had length prefix instead of space

**Problem identified:** In the SMP message structure, the port was incorrectly encoded with a length prefix, although Haskell expects a space as separator.

**ESP32 sent (WRONG):**
```
04 35 32 32 33   = Length(4) + "5223"
```

**Haskell expects (CORRECT):**
```
20 35 32 32 33   = Space (0x20) + "5223"
```

### 29.2 Detailed Hex Dump with Context

**ESP32 sent (WRONG) - with Host Suffix:**
```
63 6f 6d 04 35 32 32 33   = "com" + Length(4) + "5223"
```

**Haskell expects (CORRECT) - with Host Suffix:**
```
63 6f 6d 20 35 32 32 33   = "com" + Space(0x20) + "5223"
```

### 29.3 Root Cause Analysis

The SMP protocol uses **no** length-prefixed strings at this point, but space-separated values. This is a remnant from the original text-based protocol definition.

**Affected Structure:**
```
SMPQueueInfo Format:
  ├── qVersion (Word16 BE)
  ├── smpClientVersion (Word16 BE)  
  ├── ' ' (Space separator)
  ├── queueId (ShortByteString with length prefix)
  ├── smpKey (PublicKeyX25519 SPKI)
  ├── smpDhParams (DhPublicKey)
  └── hostPort (Space-separated!)
      ├── host (String)
      ├── ' ' (Space)       ← HERE WAS THE BUG!
      └── port (String, NO length prefix!)
```

### 29.4 Fix

**File:** `main/smp_queue.c`, Line 489

**Before (WRONG):**
```c
// Port with length prefix
buf[p++] = (uint8_t)port_len;  // WRONG: Length-Prefix
memcpy(&buf[p], port_str, strlen(port_str));
p += strlen(port_str);
```

**After (CORRECT):**
```c
// Space + Port as string (no length prefix!)
buf[p++] = ' ';  // 0x20 Space separator
memcpy(&buf[p], port_str, strlen(port_str));
p += strlen(port_str);
```

### 29.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ 2026-01-23 |
| Root cause analyzed | ✅ Space vs Length-Prefix |
| Fix implemented | ✅ Line 489 in smp_queue.c |
| Hex dump verified | ✅ `63 6f 6d 20 35 32 32 33` |
| Status | ✅ **FIXED** |

---

## 30. Payload AAD Analysis (2026-01-23 Session 2)

### 30.1 Haskell rcEncryptMsg Code Analysis

During examination of the Double Ratchet protocol, an interesting discrepancy in AAD (Additional Authenticated Data) usage was discovered.

**Source:** SimpleX Haskell Ratchet Implementation - `rcEncryptMsg`

```haskell
(emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
--                                                     ^^^^^^^^^^^^^^^^^^^^^^^^
--                                                     AAD = rcAD + emHeader!
```

**Header encryption (for comparison):**
```haskell
encryptAEAD rcHKs ehIV ... rcAD (msgHeader ...)
--                         ^^^^
--                         AAD = rcAD (112 bytes)
--                         = our_key1_public || peer_key1 (56 + 56)
```

### 30.2 Grok's Objection (Critical Consideration)

**The Chicken-Egg Problem:**

During decrypt, the recipient only knows `emHeader` AFTER header decryption. How can `emHeader` then be part of the AAD for payload decryption?

**Grok's Recommendation:** Use only `rcAD` (112 bytes) as AAD for both encryptions.

### 30.3 The Two Encryption Phases (Detailed)

**Phase 1: Header Encryption**
```
Input:
  - Key: rcHKs (Header Key Send, 32 bytes)
  - IV: ehIV (Header IV, 12/16 bytes)
  - AAD: rcAD (112 bytes) = our_key1_public || peer_key1
  - Plaintext: msgHeader (80 bytes padded)

Output:
  - emHeader (123 bytes) = IV + encrypted_header + AuthTag
```

**Phase 2: Payload Encryption**
```
Input:
  - Key: mk (Message Key, 32 bytes)
  - IV: msg_iv (12/16 bytes)
  - AAD: rcAD + emHeader (112 + 123 = 235 bytes)  ← According to Haskell code!
  - Plaintext: msg (padded)

Output:
  - Encrypted payload + AuthTag
```

### 30.4 Test with Only rcAD (112 bytes)

**Implementation in ESP32:**
```c
// Payload encryption - only rcAD as AAD (Grok's recommendation)
if (aes_gcm_encrypt(message_key,
                    msg_iv, GCM_IV_LEN,
                    ratchet_state.assoc_data, 112,  // only rcAD, NOT 235!
                    plaintext, pt_len,
                    encrypted_body, body_tag) != 0) {
    ESP_LOGE(TAG, "   ❌ Body encryption failed");
    return -1;
}
```

**Test Result:** A_MESSAGE remains - so this is **not** the cause of the problem!

### 30.5 Cryptographic Analysis

**Why 235 bytes AAD makes cryptographic sense:**
1. The recipient has all data BEFORE payload decryption:
   - `rcAD` is known from the X3DH exchange
   - `emHeader` is the received **encrypted** blob (not decrypted!)
2. It cryptographically binds the payload to the header
3. Any manipulation of the header would cause payload validation to fail

**But:** Since test with 112 bytes also produces A_MESSAGE, the problem is elsewhere!

### 30.6 Implications for ESP32 Implementation

**Currently implemented (after test):**
```c
// Header encryption with AAD
aes_gcm_encrypt(..., ratchet_state.assoc_data, 112, ...);  // only rcAD ✅

// Payload encryption with AAD
aes_gcm_encrypt(..., ratchet_state.assoc_data, 112, ...);  // only rcAD (tested)
```

**Alternative (if needed):**
```c
// Payload encryption with extended AAD
uint8_t extended_aad[235];  // rcAD (112) + emHeader (123)
memcpy(extended_aad, ratchet_state.assoc_data, 112);
memcpy(extended_aad + 112, encrypted_header, 123);

aes_gcm_encrypt(..., extended_aad, 235, ...);  // rcAD + emHeader
```

### 30.7 Status and Findings

| Item | Status |
|------|--------|
| Haskell code analyzed | ✅ |
| AAD structure understood | ✅ |
| Grok's objection documented | ✅ |
| Test with 112 bytes AAD | ✅ **Implemented** |
| A_MESSAGE fixed? | ❌ **NO** - Problem is elsewhere! |

### 30.8 Conclusion

The AAD (112 vs 235 bytes) is **not** the cause of the A_MESSAGE error. The bug must be elsewhere:
- MsgHeader format?
- Padding?
- Other structure?

---

## 31. queueMode Bug (2026-01-23 Session 2)

### 31.1 🔥 BUG: queueMode was missing for clientVersion >= 4

**Discovery:** For SMP Client Version 4+, the parser expects an additional `queueMode` field after `dhPublicKey`.

**Haskell Source (Protocol.hs):**
```haskell
smpEncode (SMPQueueInfo clientVersion ...)
  | clientVersion >= shortLinksSMPClientVersion = addrEnc <> maybe "" smpEncode queueMode
  --                 ^^^^^^^^^^^^^^^^^^^^^^^^^
  --                 shortLinksSMPClientVersion = 4
```

**Meaning:** For version 4+, the parser expects:
```
... dhPublicKey + Maybe queueMode + connInfo
```

### 31.2 Byte Analysis

**ESP32 sent (WRONG - 224 bytes):**
```
... dhPublicKey ... 7b 22 76   ← JSON directly after dhPublicKey
                    ^^^^^^^^
                    "{\"v  (Start of AgentConnInfoReply JSON)
```

**Haskell expects (CORRECT - 225 bytes):**
```
... dhPublicKey ... 30 7b 22 76   ← '0' (Nothing) + JSON
                    ^^
                    '0' = 0x30 = ASCII '0' = Maybe Nothing encoding
```

### 31.3 Haskell Maybe Encoding

**Important detail:** In SimpleX, `Maybe` is encoded as ASCII:
- `'0'` (0x30) = `Nothing`
- `'1'` (0x31) = `Just` (followed by value)

```haskell
-- Encoding for Maybe a:
instance Encoding a => Encoding (Maybe a) where
  smpEncode = \case
    Nothing -> "0"   -- ASCII '0' = 0x30
    Just a  -> "1" <> smpEncode a
```

### 31.4 Fix

**File:** `main/smp_queue.c`, after Line 507

**Before (WRONG):**
```c
memcpy(&buf[p], our_queue.rcv_dh_public, 32);
p += 32;

// Directly continue with encConnInfo...
ESP_LOGI(TAG, "   Encoded SMPQueueInfo: %d bytes", p);
```

**After (CORRECT):**
```c
memcpy(&buf[p], our_queue.rcv_dh_public, 32);
p += 32;

// queueMode for clientVersion >= 4: '0' = Nothing
buf[p++] = '0';  // ASCII '0' = 0x30 = Maybe Nothing

ESP_LOGI(TAG, "   Encoded SMPQueueInfo: %d bytes", p);
```

---


## 36. Session 3 Overview (2026-01-23)

### 36.1 Session 3 Goals

After Sessions 1-2 fixed many cryptographic bugs (X3DH, HKDF, KDF info strings, etc.), Session 3 focuses on remaining format issues.

---


## 41. Updated Bug Status (2026-01-23 Session 3)

| Bug | Status | Date | Solution |
|-----|--------|------|----------|
| A_VERSION Error (2x) | ✅ FIXED | 2026-01-22 | Version ranges corrected |
| PrivHeader Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| IV/AuthTag Length-Prefix | ✅ FIXED | 2026-01-22 | Removed |
| X3DH DH3=DH2 Bug | ✅ FIXED | 2026-01-23 | All 3 DHs now different |
| X3DH Salt NULL instead of 64 bytes | ✅ FIXED | 2026-01-23 | `uint8_t salt[64] = {0}` |
| X3DH Output 32 instead of 96 bytes | ✅ FIXED | 2026-01-23 | 96 bytes: hk+nhk+sk |
| HKDF SHA256 instead of SHA512 | ✅ FIXED | 2026-01-23 | `MBEDTLS_MD_SHA512` |
| kdf_root info string wrong | ✅ FIXED | 2026-01-23 | `"SimpleXRootRatchet"` |
| kdf_chain info string wrong | ✅ FIXED | 2026-01-23 | `"SimpleXChainRatchet"` |
| kdf_chain output 64 instead of 96 | ✅ FIXED | 2026-01-23 | IVs from KDF instead of random |
| ratchet_init_sender key overwritten | ✅ FIXED | 2026-01-23 | generate_keypair removed |
| emHeader 125 instead of 123 bytes | ✅ FIXED | 2026-01-23 | IV/Tag without length prefix |
| Port Length-Prefix instead of Space | ✅ FIXED | 2026-01-23 | `buf[p++] = ' '` |
| queueMode missing for v4+ | ✅ FIXED | 2026-01-23 | `buf[p++] = '0'` |
| **ClientMessage Padding missing** | ✅ FIXED | 2026-01-23 S3 | 15904 bytes with '#' |
| **Buffer too small (Stack Overflow)** | ✅ FIXED | 2026-01-23 S3 | malloc() instead of Stack |
| **Payload AAD 112 instead of 235 bytes** | ✅ FIXED | 2026-01-23 S3 | `payload_aad[235]` |
| **🔥 Ratchet Padding missing (14832)** | 🔥 OPEN | 2026-01-23 S3 | Fix in ratchet_encrypt() |
| **A_MESSAGE (2x)** | 🔥 CURRENT | 2026-01-23 | Cause: Ratchet Padding! |

---


