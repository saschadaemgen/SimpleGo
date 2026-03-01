# Security through reduction: SimpleGo versus GrapheneOS

![SimpleDB](gfx/GRAPHENEOS_VS_SIMPLEGO.png)

## Executive Summary

SimpleGo complements GrapheneOS by minimizing attack surfaces for secure messaging on ESP32/STM32 hardware, reducing the trusted computing base from 10-20 million to 50,000 lines of code. While GrapheneOS excels in full smartphone hardening against forensics - locked devices running recent patches resist commercial extraction tools like Cellebrite - SimpleGo eliminates baseband risks through UART isolation and multi-vendor secure elements, supporting LoRa and satellite connectivity for off-grid operations. Targeting niches like intelligence operatives and high-risk activists, tiers range from €100 DIY builds to €15,000 vault-level devices with post-quantum cryptography and sub-100ns tamper response. The competitive landscape shows consistent failure in overambitious secure phones but success in focused single-purpose devices like the Coldcard hardware wallet. Investors should note: low-volume production yields high margins in this market, but independent security audits are essential for credibility. Combined, GrapheneOS and SimpleGo offer concentric security rings, addressing unmet needs in metadata-free, auditable secure communications.

---

## Table of Contents

1. [Opening: The March 2023 Disclosure](#opening-the-march-2023-disclosure)
2. [How to Compare a Smartphone and a Messaging Device Fairly](#how-to-compare-a-smartphone-and-a-messaging-device-fairly)
3. [GrapheneOS Has Earned Its Reputation Through Engineering Depth](#grapheneos-has-earned-its-reputation-through-engineering-depth)
4. [Forensic Tools Confirm What the Architecture Promises](#forensic-tools-confirm-what-the-architecture-promises)
5. [The Baseband Remains the Elephant in Every Room](#the-baseband-remains-the-elephant-in-every-room)
6. [Why the Trusted Computing Base Still Matters](#why-the-trusted-computing-base-still-matters)
7. [Hardware Security Below the Software Layer](#hardware-security-below-the-software-layer)
8. [Tamper Detection Separates Tiers From One Another](#tamper-detection-separates-tiers-from-one-another)
9. [The SimpleX Protocol Eliminates Identifiers](#the-simplex-protocol-eliminates-identifiers)
10. [Alternative Connectivity Eliminates Single Points of Failure](#alternative-connectivity-eliminates-single-points-of-failure)
11. [Post-Quantum Cryptography Runs on Microcontrollers Today](#post-quantum-cryptography-runs-on-microcontrollers-today)
12. [The Competitive Landscape Is Littered With Instructive Failures](#the-competitive-landscape-is-littered-with-instructive-failures)
13. [Three Tiers Map to Distinct Threat Environments](#three-tiers-map-to-distinct-threat-environments)
14. [Different Threat Models, Different Optimal Devices](#different-threat-models-different-optimal-devices)
15. [These Approaches Strengthen Each Other](#these-approaches-strengthen-each-other)
16. [References](#references)

---

## Opening: The March 2023 Disclosure

In March 2023, Google's Project Zero disclosed **18 zero-day vulnerabilities** in Samsung Exynos modem firmware - the same modem powering every Google Pixel phone. Four of those bugs allowed an attacker to remotely compromise a phone's baseband processor knowing nothing more than the victim's phone number. No click. No link. No user interaction. The phone simply had to be on. That disclosure crystallized a tension at the heart of mobile security that no amount of software hardening can fully resolve: **modern smartphones contain multiple independent computers, and the one handling your cellular connection runs opaque firmware that the operating system cannot inspect, audit, or fully contain.**

GrapheneOS, the most hardened Android distribution available, has built extraordinary layered safeguards on Google Pixel hardware - to the point where commercial forensic tools like Cellebrite cannot extract data from locked, updated devices. SimpleGo, a purpose-built secure messaging device running the SimpleX protocol natively on ESP32 and STM32 microcontrollers, takes a radically different approach: eliminate the attack surface entirely by stripping the device down to approximately 50,000 lines of auditable bare-metal firmware. These two approaches are not in competition. They represent complementary philosophies - one maximizing the security of a general-purpose platform, the other minimizing the trusted computing base to a single function. Understanding both illuminates where mobile security stands today and where it must go.

---

## How to Compare a Smartphone and a Messaging Device Fairly

Any comparison between GrapheneOS and SimpleGo risks false equivalence. GrapheneOS runs a full Android stack supporting thousands of applications, biometric authentication, cameras, GPS, web browsing, and every communication protocol in common use. SimpleGo is a dedicated secure messaging terminal. Comparing them on raw capability would be absurd - GrapheneOS wins immediately. Comparing them on theoretical attack surface reduction would be equally misleading in the other direction.

The honest comparison framework asks a different question: **for the specific threat model each device addresses, how well does its architecture match?** GrapheneOS targets individuals who need a fully functional smartphone hardened against sophisticated adversaries - journalists, activists, security professionals, executives. SimpleGo targets scenarios where the messaging channel itself must be architecturally isolated from the compromises inherent in general-purpose computing: high-value intelligence communication, dead-drop messaging, air-gapped operational security, and environments where device seizure and forensic extraction are expected threats.

Throughout this analysis, when GrapheneOS provides a better solution, that will be stated plainly. Where SimpleGo's architectural choices offer genuine advantages, those will be explained with specificity. Both projects face real limitations that deserve candid treatment.

---

## GrapheneOS Has Earned Its Reputation Through Engineering Depth

GrapheneOS represents the state of the art in hardened mobile operating systems, and that claim rests on concrete technical achievements rather than marketing. Its security architecture operates across every layer of the Android stack, from memory allocation to kernel hardening to application sandboxing.

**Memory safety forms the foundation.** GrapheneOS replaces Android's default scudo allocator with hardened_malloc, a custom allocator that stores metadata entirely out-of-line (preventing corruption), enforces deterministic detection of invalid frees, zeros memory on release, and implements randomized quarantine ordering using FIFO queues and randomized arrays against use-after-free exploitation. On Pixel 8 and later devices supporting ARMv9 cores with Memory Tagging Extension (introduced in the ARMv8.5-A specification), hardened_malloc assigns random memory tags per allocation slot and reserves a dedicated tag value for freed memory, providing **deterministic detection** of use-after-free and linear overflow vulnerabilities. A September 2025 analysis by the French security firm Synacktiv confirmed that hardened_malloc significantly reduces exploitation opportunities compared to stock Android's allocator, particularly when combined with GrapheneOS's broader hardening measures.

The exec-based spawning model addresses a fundamental weakness in Android's process architecture. Stock Android launches applications by forking the zygote process, meaning every app inherits an identical address space layout - effectively reducing Address Space Layout Randomization from per-process to per-boot entropy. An information leak in any one app reveals memory layout for all apps. GrapheneOS follows the fork with an execve call, creating a fresh randomized address space for each application. The ~200ms cold-start penalty is imperceptible on modern hardware and eliminates an entire class of cross-process exploitation.

Kernel-level hardening goes equally deep. GrapheneOS enables **both ShadowCallStack and Pointer Authentication Code** protection for return addresses simultaneously on ARMv9 devices, where stock Android uses only one. Branch Target Identification supplements Clang's type-based Control Flow Integrity to cover functions excluded from CFI instrumentation. The kernel uses 4-level page tables providing 48-bit address space with 33-bit ASLR entropy - compared to 39-bit address space and 24-bit ASLR entropy on stock Android. All kernel stack allocations are zeroed. All freed memory is zeroed in both the page allocator and SLUB heap allocator. Module loading requires per-build RSA-4096 signatures.

On the application layer, GrapheneOS **disables ART JIT compilation entirely**, enforcing ahead-of-time compilation that eliminates JIT spraying as an attack vector. Separately, seccomp-bpf enforcement blocks dynamic code loading specifically for Vanadium browser and WebView components, providing additional JIT control where needed. The permission model introduces controls absent from stock Android: a per-app network toggle that convincingly simulates network unavailability rather than crashing apps, a sensors toggle blocking accelerometer and gyroscope access, Storage Scopes providing granular file-level access instead of all-or-nothing storage permissions, and Contact Scopes limiting address book exposure to individually selected entries.

Additional features that deserve mention: **sandboxed Google Play** runs Play Store and Google services in a secure container without system-level privileges; **Vanadium**, a hardened Chromium-based browser, serves as the default; the built-in **Auditor** app enables remote attestation of device integrity; **multi-profile support** with instant "End session" allows compartmentalization of identities; and enhanced **VPN and location privacy toggles** give users fine-grained control over network behavior.

The USB-C port control defaults to charging-only when locked, blocking new connections at both the hardware controller and kernel level - a critical defense given that commercial forensic tools rely almost exclusively on USB-based exploit delivery. An auto-reboot timer (defaulting to 18 hours) returns locked devices from After First Unlock to the cryptographically sealed Before First Unlock state, clearing encryption keys from memory. A duress PIN triggers irreversible device wipe without reboot.

**GrapheneOS is, by a significant margin, the most secure general-purpose mobile operating system available to civilians.** This assessment is not contested by credible security researchers.

---

## Forensic Tools Confirm What the Architecture Promises

The strength of GrapheneOS's defenses is validated not only by design analysis but by adversarial testing from the forensic extraction industry itself. Multiple leaked Cellebrite compatibility matrices - authenticated by Cellebrite's own communications director and spanning April 2024 through October 2025 - consistently show that **locked GrapheneOS devices in Before First Unlock state, running patches from late 2022 onward, are inaccessible** to Cellebrite's commercial extraction tools.

This resistance requires important qualification. Commercial forensic tools primarily target the After First Unlock state, where encryption keys reside in memory. GrapheneOS defends this state through its exploit mitigations (hardened_malloc, MTE, USB hardware blocking), but the auto-reboot timer serves as a failsafe - after 18 hours locked, the device reboots to BFU state where keys are cryptographically sealed behind the Titan M2 secure element. Cellebrite cannot brute-force the Titan M2's hardware rate-limiting on Pixel 6 and later devices, which escalates delays to 24 hours per attempt after 139 failures. Devices in AFU state with developer options enabled may present additional extraction opportunities.

The practical picture: a locked, updated GrapheneOS device seized by law enforcement and handed to Cellebrite for extraction will yield nothing in BFU state. Even GrayKey (now owned by Magnet Forensics) downgraded Pixel access from full to partial extraction after GrapheneOS-reported vulnerability patches shipped in April 2024. The Serbian government's documented use of a Cellebrite exploit chain against a student activist in December 2024 relied on three USB-based kernel vulnerabilities - exactly the vector GrapheneOS blocks by default, and one of those CVEs had already been patched by GrapheneOS before the official Android security bulletin.

This must be stated clearly: these are **commercial tool capabilities, not nation-state intelligence capabilities.** Cellebrite Advanced Services (in-house lab analysis) and signals intelligence agencies may possess undisclosed techniques. The absence of evidence is not evidence of absence. But GrapheneOS represents the current gold standard for forensic resistance on any consumer device, and its developers actively report vulnerabilities exploited by forensic vendors to Google, raising the security floor for all Android users.

---

## The Baseband Remains the Elephant in Every Room

The baseband processor is where GrapheneOS's impressive architecture encounters a boundary it cannot fully cross. The Samsung Shannon modem in every Pixel phone runs millions of lines of proprietary firmware that GrapheneOS cannot inspect, modify, or replace. This firmware processes complex, attacker-controlled radio protocols - and its vulnerability track record demands attention.

Google Project Zero's March 2023 disclosure revealed four critical Internet-to-baseband remote code execution vulnerabilities in Exynos modems (CVE-2023-24033, CVE-2023-26496, CVE-2023-26497, CVE-2023-26498). These affected the SDP parser in the VoLTE stack and required only the victim's phone number to exploit - no user interaction, no click, no link. Tim Willis, head of Project Zero, described the severity as warranting "policy exception to delay disclosure."

Natalie Silvanovich, leading Project Zero's North American team, presented detailed exploitation research on the Shannon 5300 modem at REcon 2023 and OffensiveCon 2023. Her work demonstrated that some baseband vulnerabilities could be exploited **fully remotely across carriers** using nothing more than a rooted phone as the attack platform. This moved baseband exploitation from theoretical to demonstrated.

The Intellexa/Predator Triton tool made the threat operational. Documented by Amnesty International's Security Lab in October 2023, Triton uses an IMSI catcher to force target Samsung devices to downgrade from LTE to 2G, then delivers a baseband exploit via a software-defined radio acting as a 2G base station. Full Predator spyware installation reportedly completes within three minutes at distances up to hundreds of meters. This represents a proven, fieldable tactical capability against Samsung-baseband devices.

Google has responded. The Pixel 9's Exynos 5400 modem incorporates BoundsSanitizer, Integer Overflow Sanitizer, stack canaries, Control Flow Integrity, and auto-initialized stack variables - Clang sanitizer infrastructure previously deployed only in the application processor, now applied to baseband firmware. Google explicitly cited the Amnesty/Intellexa report as motivation. GrapheneOS contributes LTE-only mode (eliminating the 2G/3G/5G attack surface that Triton exploits) and a SUPL proxy preventing location-data leakage through modem connections.

These mitigations are meaningful. LTE-only mode directly defeats the 2G downgrade attack chain. Baseband sanitizers catch memory corruption bugs before they become exploitable. But the architectural limitation remains: the modem communicates with the application processor through shared memory regions partitioned by Samsung's SysMMU (IOMMU). Research by Markuze et al. at ASPLOS 2016 demonstrated two systemic IOMMU vulnerabilities - **sub-page granularity** (IOMMUs protect at 4KB page boundaries, but DMA buffers can share pages with other kernel data) and **deferred invalidation** (stale IOTLB entries create microsecond-scale windows where compromised devices retain access to unmapped memory). Follow-up work by the same group at EuroSys 2021 confirmed that DMA vulnerabilities are "a deep-rooted issue" where "it is often the kernel design that enables complex and multistage DMA attacks." De Bonfils Lavernelle et al. at IEEE TrustCom 2024 demonstrated straightforward DMA attacks bypassing correctly configured IOMMUs in embedded systems.

The assessment: GrapheneOS applies every software mitigation available and significantly raises the cost of baseband-to-AP exploitation. But **IOMMU-mediated shared-memory isolation is not equivalent to physical bus isolation**, and no amount of OS hardening can change the hardware communication architecture between the Shannon modem and the Tensor application processor.

---

## Why the Trusted Computing Base Still Matters

The contested "92 million lines of code" figure for Android's trusted computing base requires careful decomposition. The full Linux kernel source tree contains approximately **40 million lines** (Linux 6.14 rc1 reached 40,063,856 lines), but the vast majority consists of drivers and architecture-specific code never compiled for any given device. Amazon Linux's minimal kernel compiles roughly 1 million lines; a Pixel-configured kernel likely compiles 1.5 to 3 million. AOSP core framework code represents approximately 2.5 million lines, expanding to 7 million with bundled external dependencies. A rigorous analysis by security researcher derdilla in October 2023, using AOSP's repo tool, established these figures through direct measurement.

A defensible estimate of the on-device trusted computing base for a GrapheneOS Pixel - meaning code that actually executes with security-relevant privilege - falls in the range of **10 to 20 million lines of code**. This includes the compiled kernel, system services, framework code, and libraries, but excludes uncompiled kernel source, build tools, and test suites. Adding vendor-proprietary firmware (modem, GPU, Titan M2, TrustZone) increases the count by an unknown but substantial amount, since these are binary blobs without published source.

SimpleGo's claimed ~50,000 lines of bare-metal firmware represents a **200 to 400x reduction** in trusted code. This comparison is not apples-to-apples - GrapheneOS does incomparably more - but for the specific function of secure message composition, encryption, and transmission, the auditability difference is genuine. Academic studies of software defect density generally find 0.5 to 5 bugs per thousand lines (KLOC) in rigorously engineered code, though real-world rates vary widely (1-25/KLOC depending on quality practices and domain). As an **illustration of scale** rather than prediction: a 50,000-line system might harbor 25-250 issues, versus 5,000-75,000 in a 10-15 million-line system. This extrapolation is speculative and depends heavily on development practices, but it captures why the microkernel and unikernel security research communities continue pursuing code minimization.

The comparison gains additional weight from the seL4 microkernel precedent. seL4's formally verified ~10,000 lines of code represent the gold standard for trusted computing bases. SimpleGo cannot claim formal verification, but its firmware sits much closer to seL4's scale than to Android's - and that proximity makes comprehensive security audit economically feasible in a way that auditing millions of lines never will be.

---

## Hardware Security Below the Software Layer

The Titan M2 secure element in Google Pixel phones earned its Common Criteria certification through a rigorous process that took Google over three years. Certified under Protection Profile BSI-CC-PP-0084-2014 at **EAL4+ augmented with AVA_VAN.5** (the highest vulnerability assessment level, requiring resistance to advanced methodical attacks including physical penetration) **and ALC_DVS.2** (development security), the Titan M2 was evaluated by SGS Brightsight in the Netherlands and certified under the NSCIB scheme. PP0084 is the same protection profile used for banking chips, SIM cards, and national identity documents - the international gold standard for hardware security components.

SimpleGo's tiered architecture layers multiple secure elements from independent vendors, drawing on a design philosophy validated by the hardware wallet industry. The Coldcard Mk4 splits its Bitcoin seed phrase across **dual secure elements from different manufacturers** (Microchip ATECC608B/C and Maxim DS28C36B - production runs vary) plus the main MCU - reasoning that a critical flaw in either vendor's silicon cannot compromise the combined secret. The Keystone 3 Pro uses **triple secure elements** (Microchip ATECC608B, Maxim DS28S60, and Maxim MAX32520). The Trezor Safe 7 pairs an Infineon OPTIGA Trust M with the open-source TROPIC01 chip developed by Tropic Square.

SimpleGo's Tier 2 device pairs the Microchip ATECC608B (ECC P-256 hardware acceleration, ~$0.87 at distributor pricing for the MAHDA-T variant, proven in millions of IoT deployments) with the Infineon OPTIGA Trust M (CC EAL6+ certified hardware, supporting ECC curves through P-521, RSA-2048, AES-256, with an encrypted I2C "Shielded Connection" using pre-shared platform binding secrets to defeat bus probing). Tier 3 adds the NXP SE050, certified at **EAL6+ with FIPS 140-2 Level 3** (and Level 4 for physical security), supporting a broader algorithm suite including NIST P-384 and key derivation functions, with up to 50KB of user memory for certificate storage. The multi-vendor approach means that a vulnerability in any single secure element vendor's implementation - a discovered side channel, a flawed random number generator, a certification process failure - does not compromise the device's root of trust.

The Titan M2's single-vendor architecture is not a weakness in context. Google controls the full hardware-software integration, the chip underwent AVA_VAN.5 penetration testing (which includes invasive physical attacks), and the Pixel's threat model assumes the secure element is trusted. For a smartphone, this is appropriate. SimpleGo's multi-vendor approach reflects a different trust model: assume any single component might fail, and architect so that failure is not catastrophic.

---

## Tamper Detection Separates Tiers From One Another

SimpleGo's Tier 1 (€100-400) provides Secure Boot and flash encryption on the ESP32-S3 - meaningful protections against firmware modification and offline flash reading, but no physical tamper detection beyond what the SoC provides natively.

Tier 2 (€400-1,000) introduces the STM32U585 Cortex-M33 with TrustZone isolation, active PCB security mesh, and light sensors for decapping detection. The STM32's TAMP peripheral supports active tamper monitoring: output pins transmit pseudorandom patterns that input pins verify, detecting circuit breaks or shorts. Eight external tamper inputs monitor the security mesh, while internal monitors track voltage thresholds, temperature, and clock frequency. On tamper detection, the peripheral erases backup registers, SRAM2, backup SRAM, caches, and cryptographic peripheral contents. Critically, this operates on backup battery power in VBAT mode, maintaining tamper detection even when the device is powered off.

Tier 3 (€1,000-15,000) adds the Analog Devices DS3645 secure supervisor, which stores cryptographic keys in **4,096 bytes of battery-backed SRAM** with a hardwired clearing function that erases the entire array in under 100 nanoseconds upon tamper alarm. The DS3645 continuously complements SRAM cells in the background to prevent memory imprinting from oxide stress, monitors temperature rate-of-change to detect thermal attacks, and verifies crystal oscillator frequency stability. Its operating range spans -55°C to +95°C, and the chip is packaged in a CSBGA with no leads exposed at the outer edges - a deliberate choice to complicate physical probing.

The Tier 3 enclosure itself represents significant engineering. CNC-machined magnesium AZ91D alloy provides substantial electromagnetic shielding across the 30 KHz to 1.5 GHz range (exact attenuation depends on surface treatment and alloy modifications; composite formulations achieve 90+ dB). At one-third the weight of aluminum, AZ91D is the standard choice for military electronics housings. Seams use conductive elastomer gaskets conforming to MIL-DTL-83528 - silver-coated copper-filled silicone achieving 100 to 110 dB attenuation at 10 GHz - ensuring that enclosure joints do not become emanation leakage paths. Henkel's STYCAST 2850FT epoxy potting compound (thermal conductivity 1.25 W/m·K, Shore D hardness 92-96 depending on catalyst/hardener selection) encapsulates the PCB assembly, creating a physical barrier that resists chemical attack, hides circuit topology, and makes component-level probing extraordinarily difficult.

TEMPEST compliance at the Tier 3 price point warrants realistic assessment. **Full TEMPEST Level A certification (NATO SDIP-27 Level A) is not achievable at €15,000** - certified TEMPEST laptops alone cost that much, and accredited testing facilities charge substantial fees against classified specifications. **Level C (tactical, assuming adversary at 100 meters) is a more realistic target.** A properly gasketed magnesium enclosure with potted internals can approach Level C emanation control, but formal certification requires accredited testing that SimpleGo has not undergone. The document's Tier 3 description should be understood as **targeting Level C equivalence, not certified compliance**.

For reference, the L3Harris AN/PRC-163 Falcon IV military radio - NSA-certified for Top Secret communications - carries an estimated per-unit cost of $20,000 to $40,000 at scale under contracts with ceiling values in the billions. SimpleGo Tier 3 targets a fraction of that cost by constraining functionality to secure messaging rather than full tactical radio capability. At production volumes of 100 units, hardware costs (CNC enclosure, loaded PCB, potting, security mesh, assembly) fall in the **$280-670 per unit range** excluding certification, firmware development, and non-recurring engineering.

---

## The SimpleX Protocol Eliminates Identifiers

Most encrypted messaging protocols protect message content while preserving metadata: who communicates with whom, when, how often, and from where. The SimpleX Messaging Protocol takes a fundamentally different architectural approach. There are **no user identifiers whatsoever** - no phone numbers, usernames, email addresses, or even random persistent IDs. Each connection between two parties uses temporary anonymous pairwise identifiers for unidirectional message queues. The relay servers see only opaque queue IDs and cannot correlate sender with receiver, because each conversation routes through at least two different servers (one chosen by each party).

Messages are padded to a fixed 16 KB block size, directly countering the traffic analysis techniques demonstrated by Coull and Dyer in their 2014 ACM SIGCOMM Computer Communication Review (CCR) paper, which achieved over **96% accuracy** in identifying user actions and message content types on Apple iMessage purely by observing encrypted packet sizes. The 2020 NDSS paper by Bahramali et al. applied related techniques to Signal, Telegram, and WhatsApp, demonstrating that encrypted messaging metadata enables identification of channel administrators and members with high accuracy - a different attack type than Coull and Dyer's action identification, but equally concerning for metadata privacy.

Private message routing, introduced in SimpleX v5.8 and default from v6.0, adds two-hop onion routing where the forwarding relay is chosen by the sender and the destination relay by the recipient. Neither party reveals its IP address or transport session to the other. Each forwarded message uses a one-time ephemeral key, providing per-message transport anonymity rather than per-connection anonymity (as in Tor or VPN solutions).

Running this protocol natively on a microcontroller - rather than as an application within a general-purpose OS - means the message handling code is not sharing process memory with a web browser, GPS stack, camera driver, or any other application. The SimpleX client on SimpleGo operates in a context where there is literally nothing else to compromise.

Signal's post-quantum evolution deserves recognition here. **Signal deployed PQXDH (hybrid X25519 + CRYSTALS-Kyber-1024) in September 2023**, becoming the first major messaging platform to deploy hybrid post-quantum cryptography in production. Signal followed with the Sparse Post-Quantum Ratchet (SPQR) in October 2025, which provides quantum-resistant forward secrecy and post-compromise security throughout conversation lifecycles. SimpleX implemented hybrid post-quantum encryption (using sntrup761) in v5.6 (March 2024), making it default in v5.7 (April 2024). Signal was first, and its October 2025 Triple Ratchet goes further than SimpleX's current implementation. SimpleGo's advantage is not protocol novelty - it is the execution environment's reduced attack surface.

---

## Alternative Connectivity Eliminates Single Points of Failure

SimpleGo's architecture supports communication paths that bypass conventional cellular and internet infrastructure entirely. The Semtech SX1262 LoRa transceiver enables mesh networking over unlicensed sub-GHz ISM bands with practical ranges of 2 to 10 kilometers in open terrain using the Meshtastic protocol. Meshtastic provides AES-256 encryption for channel messages and X25519-based end-to-end encryption for direct messages, with multi-hop mesh routing extending effective range through intermediate nodes. Over 40,000 active Meshtastic nodes operate globally. The tradeoff is bandwidth - LoRa data rates peak at **62.5 kbps** under optimal spreading factor and bandwidth configurations (with up to 300 kbps possible in FSK mode), adequate for text messaging but not for media-rich communication.

For truly global, infrastructure-independent communication, Tier 3 supports Iridium Short Burst Data via the RockBLOCK 9603 module. Iridium's 66 cross-linked LEO satellites provide pole-to-pole coverage with message delivery in approximately 20 seconds. The constraint is message size - **340 bytes outbound, 270 bytes inbound per SBD transmission through RockBLOCK** (the Iridium SBD protocol itself supports larger messages up to 1,960/1,890 bytes, but RockBLOCK imposes tighter limits). SimpleX's text-centric, padded message format is well suited to this constraint with appropriate fragmentation. Hardware cost adds roughly $128 per module with $15/month line rental and approximately $0.10 per 50-byte credit. This creates a communication channel that functions independently of every terrestrial network, cellular tower, and internet backbone simultaneously.

WireGuard VPN connectivity on ESP32 is production-ready through the wireguard-lwip implementation - a malloc-free, fixed-RAM C implementation of the full WireGuard protocol integrated with the lwIP TCP/IP stack. Multiple ESP32 ports exist and are actively maintained, enabling SimpleGo to tunnel traffic through standard WireGuard peers when WiFi or cellular IP connectivity is available.

Constant-rate traffic padding - maintaining fixed transmission rates with chaff packets filling idle periods - represents the theoretical gold standard for defeating passive traffic analysis. Its extreme bandwidth and power costs make it impractical for continuous operation on battery-powered devices, but SimpleGo's narrow communication function makes selective constant-rate padding during active messaging sessions feasible in a way that a general-purpose smartphone could never sustain.

---

## Post-Quantum Cryptography Runs on Microcontrollers Today

NIST finalized FIPS 203 (ML-KEM), FIPS 204 (ML-DSA), and FIPS 205 (SLH-DSA) on August 13, 2024. FIPS 206 (FN-DSA, derived from FALCON) remains under development - as of late 2025, the Initial Public Draft was described as "basically written, awaiting approval" by NIST's Ray Perlner at the 6th PQC Standardization Conference, with **finalization projected for late 2026 or early 2027**. NIST selected HQC as an additional code-based KEM in March 2025, providing algorithmic diversity from the lattice-based schemes.

The critical question for SimpleGo is whether these algorithms are computationally feasible on Cortex-M class microcontrollers. The pqm4 benchmarking project provides data points. On an STM32F4 running at 168 MHz, published cycle counts for **ML-KEM-768 total approximately 2 million cycles for keygen + encapsulation + decapsulation combined**. At 168 MHz with ideal conditions, this translates to roughly 12ms; real-world performance with flash wait states adds overhead. WolfSSL's STM32 implementation reports **approximately 21ms total** (6.5ms keygen, 6.6ms encaps, 8.3ms decaps) for ML-KEM-768 - a more realistic figure for production use. Either way, this latency is acceptable for messaging applications that are not time-critical.

ML-DSA signing times are more variable. Published benchmarks show considerable range depending on implementation and rejection sampling outcomes; figures between 37ms and 63ms for ML-DSA-65 signing on STM32F4 appear in different sources, with worst-case spikes above 150ms. Verification completes in approximately 14ms. Memory requirements are manageable: speed-optimized ML-DSA implementations require 60 to 69 KB of stack, tight but viable on the STM32U5 series with 786 KB of SRAM.

In January 2025, Infineon announced that its TEGRION security controller family received the **world's first Common Criteria EAL6 certification for post-quantum cryptography** from Germany's BSI. The specific SLC27 model was associated with an October 2025 product launch. The TEGRION implements hardware-accelerated ML-KEM and ML-DSA with side-channel and fault-attack protection on a 28nm process. While not yet integrated into SimpleGo's design, this demonstrates that certified PQC-capable secure elements are entering the supply chain - a component that could appear in future Tier 2 and Tier 3 hardware.

SimpleGo's firmware-level PQC implementation on a Cortex-M33 (Tier 2) or Cortex-M33 with DSP extensions (Tier 3) can execute post-quantum key exchange and signing within real-time messaging constraints. The compute budget consumed by PQC operations is substantial relative to classical ECC, but the STM32U5 series provides sufficient headroom for a single-purpose messaging device - a luxury that general-purpose smartphones must balance against dozens of concurrent processes.

---

## The Competitive Landscape Is Littered With Instructive Failures

SimpleGo enters a space where several approaches have been tried and most have failed. Understanding why illuminates what might succeed.

**Precursor** (betrusted.io), created by Andrew "bunnie" Huang, takes the most intellectually rigorous approach: an FPGA-based device where even the CPU is compiled from source as a soft-core RISC-V, running Xous (a Rust-based microkernel OS). At approximately $512 and currently shipping via Crowd Supply, Precursor achieves evidence-based trust from logic gates to application code. Its limitation is practical: no built-in cellular connectivity (a mod-able GPIO bay accommodates an LTE module, but this is not standard), monochrome display, and a use case focused on password management and TOTP rather than messaging.

**Purism's Librem 5** ($699-$1,999) pioneered USB-isolated modem architecture in a consumer phone - the M.2 cellular module connects via USB bus with no shared RAM between modem and application processor, plus three hardware kill switches for cellular, WiFi/Bluetooth, and camera/microphone. The PinePhone includes UART as an alternative interface to its Quectel modem, though its primary connection is USB. Running PureOS (Debian-based Linux), the Librem 5 offers genuine modem isolation but ships without a dedicated secure element and struggles with software maturity and hardware performance.

**Bittium's Tough Mobile 2C** (approximately €1,650-2,000+) represents the enterprise/government approach: hardened Android 11 with MIL-STD-810G compliance and **NATO Restricted certification** from the NATO Communications and Information Agency. Its dual-boot architecture separates personal and confidential operating environments. Manufactured in Finland with auditable supply chains, the Tough Mobile targets government customers in over 50 countries. It demonstrates market viability for premium secure devices but relies on Android's full software stack.

**GSMK CryptoPhone** (~$3,500, founded by CCC members Frank Rieger and Andreas Müller-Maguhn) offers full source code access for independent audit - a rare commitment in the secure phone market. Still operational with customers in over 50 countries and approximately 11 employees, it serves a niche government/enterprise market.

Additional competitors worth noting: **Nitrokey** provides open-source HSMs for key management, relevant for comparing SimpleGo's secure element approach to affordable, auditable alternatives. **Utimaco** offers PQC-ready HSMs for enterprise, demonstrating that quantum-resistant hardware is not unique to SimpleGo. **Punkt MP02** represents minimalist privacy phones for basic calls and text, similar to SimpleGo's single-function philosophy. **Fairphone** offers modular, ethical Android phones with de-Googled options as another alternative to GrapheneOS. **Tropic Square's TROPIC01** is an open-source secure chip that could serve as a component for multi-vendor trust in future hardware designs.

The failure cases carry sharper lessons. Silent Circle's **Blackphone** expected 250,000 unit orders and sold roughly 6,000 to América Móvil, nearly bankrupting the company - "poor specifications, underperforming but overpriced hardware" per court filings from the Geeksphone lawsuit. **Boeing Black** was announced in February 2014 and quietly disappeared, proving that aerospace expertise does not transfer to consumer electronics. DarkMatter's **KATIM phone** was fatally undermined when Reuters exposed the company's role in Project Raven, a UAE surveillance operation targeting human rights activists - a company that hacks phones cannot credibly sell secure ones. Sirin Labs' **Solarin** launched at $14,000+ with a celebrity-studded party and sold approximately 700-750 units (calculated from reported $10M revenue), shipping with obsolete specifications.

The pattern is consistent: secure phone ventures fail when they try to compete with mainstream smartphones on general-purpose functionality. They succeed when they serve a clearly defined niche with a credible trust model. The Coldcard hardware wallet offers a useful parallel - a single-purpose, air-gapped device with dual secure elements and open-source firmware, priced at $178, that has established a sustainable position by doing exactly one thing (Bitcoin signing) with extraordinary rigor. SimpleGo aspires to be "Coldcard for messaging" - and the analogy is structurally sound.

---

## Three Tiers Map to Distinct Threat Environments

SimpleGo's tiered architecture acknowledges that security requirements vary by threat model, and over-engineering the low end prices out users who need meaningful security improvements at accessible cost.

**Tier 1  -  DIY (€100-400)** pairs an ESP32-S3 with an ATECC608B secure element, Secure Boot, flash encryption, and WiFi connectivity with optional LoRa or 4G modules. The 3D-printed enclosure provides no physical tamper resistance. This tier targets hobbyists, researchers, and users in environments where device seizure is unlikely but network surveillance is a concern. It provides cryptographic message security and metadata protection through the SimpleX protocol while remaining accessible enough for community adoption and self-build projects.

**Tier 2  -  Secure (€400-1,000)** steps to an STM32U585 Cortex-M33 with TrustZone hardware isolation, dual-vendor secure elements (ATECC608B + OPTIGA Trust M), active PCB security mesh, light sensors for tamper detection, and a CNC aluminum enclosure rated IP54. The jump from ESP32 to STM32U5 brings ARM TrustZone partitioning (splitting the processor into Secure and Non-Secure worlds at the hardware level), DPA-resistant cryptographic operations, and the STM32 TAMP peripheral for active monitoring. WiFi 6, 4G LTE, and LoRa connectivity options cover the primary communication paths. This tier serves journalists, activists, corporate security teams, and organizations operating in surveillance-heavy environments.

**Tier 3  -  Vault (€1,000-15,000)** targets state-level threats with triple-vendor secure elements (adding NXP SE050), the DS3645 secure supervisor with sub-100ns zeroization, W.L. Gore-style tamper respondent enclosure monitoring, CNC magnesium AZ91D construction with conductive elastomer gaskets and epoxy potting, IP67 environmental sealing, and 5G NR/LoRa/satellite connectivity. The wide price range reflects the variable cost of approaching TEMPEST Level C emanation control (more aggressive shielding and testing pushes toward the upper bound) and the manufacturing premium of small-batch production.

FIPS 140-3 mapping follows naturally from the capabilities at each tier. Tier 1's Secure Boot and encrypted flash align with **Level 1** (production-grade equipment with approved algorithms). Tier 2's tamper-evident enclosure, role-based authentication, and secure element key storage approach **Level 2-3** requirements. Tier 3's active tamper response, environmental attack detection, and sub-100ns zeroization targets **Level 3-4** - though formal FIPS certification requires accredited laboratory testing costing upward of $50,000 per module configuration with timelines exceeding two years. **These mappings should be understood as design targets, not certified claims.**

A frank acknowledgment of limitations is essential. SimpleGo is a **messaging-only device** - no web browser, no app ecosystem, no camera, no maps, no email client. Users must carry a separate phone for everything else, creating operational security challenges (two devices to manage, potential correlation between them). The learning curve for a text-only interface on a microcontroller-powered device is steep compared to a familiar smartphone experience. Most critically, **SimpleGo's security claims currently lack independent verification or third-party audit.** The GitHub repository for the project is not publicly accessible as of early 2026, and no working prototype has been independently evaluated. The specifications described in this document represent design goals and architectural intentions - validating them requires building hardware, writing firmware, subjecting both to professional security assessment, and publishing the results. Until that happens, SimpleGo's claims should be evaluated as engineering proposals rather than proven capabilities.

---

## Different Threat Models, Different Optimal Devices

The comparison between GrapheneOS and SimpleGo resolves cleanly once threat models are specified. Consider three scenarios.

A journalist working in a country with sophisticated surveillance infrastructure needs a functional smartphone for research, photography, source communication, and daily life, hardened against both remote exploitation and device seizure. **GrapheneOS on a current Pixel is the correct choice, and nothing SimpleGo offers changes that.** The journalist needs apps, a browser, a camera, and a communication platform that forensic tools cannot extract data from. GrapheneOS delivers all of this.

An intelligence operative needs to transmit short encrypted messages through a channel that cannot be attributed, cannot be forensically extracted even with physical seizure and unlimited lab time, and can function when cellular infrastructure is compromised or unavailable. **SimpleGo Tier 3 targets exactly this scenario.** The 50,000-line firmware can be audited. The triple secure elements provide redundant protection against hardware supply chain compromise. Sub-100ns zeroization destroys keys before a sophisticated adversary can interrupt the process. LoRa mesh and satellite connectivity function when every cell tower is monitored or offline. The device does one thing - but in an environment where that one thing must be done perfectly, the stripped-down architecture is not a limitation but a requirement.

A security-conscious executive needs both: a hardened daily-driver smartphone and an architecturally isolated channel for the most sensitive communications. **The complementary deployment makes sense.** GrapheneOS handles the 99% of communication that requires a full smartphone platform. SimpleGo handles the 1% where the threat model demands that no general-purpose computing stack sits between the message and the wire.

This framing - concentric rings of security rather than competing alternatives - resolves the false dilemma. GrapheneOS is the best hardened smartphone OS. SimpleGo aspires to be the best hardened messaging device. The attack surfaces they address overlap partially (encrypted messaging) but diverge fundamentally (general computation versus single-purpose isolation, IOMMU-mediated modem containment versus UART physical isolation, millions of lines of trusted code versus tens of thousands).

The baseband problem provides the clearest illustration. GrapheneOS mitigates it brilliantly with LTE-only mode, SUPL proxying, exploit mitigations, and USB blocking. SimpleGo proposes to eliminate it architecturally - using UART-only modem connectivity that provides no DMA capability, no shared memory, and a serial byte stream too simple to harbor the parsing complexity that breeds vulnerabilities. No existing consumer device uses UART-only modem isolation (the PinePhone includes UART as an alternative interface, but its primary modem connection is USB), making this a genuinely novel architectural choice if implemented. The tradeoff is bandwidth - UART is inadequate for high-speed data transfer, constraining SimpleGo to text messaging rates. For a messaging-only device, this constraint is acceptable. For a smartphone, it would be disqualifying.

---

## These Approaches Strengthen Each Other

The security landscape is not a tournament bracket where one approach must eliminate the other. GrapheneOS pushes the boundary of what hardened general-purpose computing can achieve, creating proof points that influence the entire Android ecosystem (Google adopted auto-reboot, fastboot memory zeroing, and vulnerability patches that GrapheneOS pioneered or reported). SimpleGo, if successfully realized, would push the boundary of what purpose-built secure hardware can deliver at accessible price points, borrowing design patterns from hardware wallets and military communications while targeting a market segment that Coldcard demonstrated is economically viable.

The deeper insight is architectural. Software hardening and hardware minimization are not opposing philosophies - they are **complementary layers in a mature security posture.** An organization running GrapheneOS for daily communications and SimpleGo for highest-sensitivity channels deploys redundant security measures across device categories, not just within a single device's software stack. The failure of one device's security model does not compromise the other's, because they share no code, no hardware platform, no trust assumptions, and no communication metadata.

GrapheneOS has proven that a small, dedicated team can make a general-purpose OS dramatically harder to exploit. SimpleGo proposes that an even smaller, more constrained device can make certain communication channels harder to compromise at the architectural level. The first claim is validated by years of Cellebrite matrices and security research. The second awaits the hard work of implementation, testing, and independent audit. If SimpleGo delivers on its design goals, the combination will offer a security posture that neither approach achieves alone.

---

## References

**GrapheneOS Documentation and Features**
- GrapheneOS Features: https://grapheneos.org/features
- GrapheneOS FAQ: https://grapheneos.org/faq
- GrapheneOS Usage Guide: https://grapheneos.org/usage
- Synacktiv hardened_malloc Analysis (September 2025): https://www.synacktiv.com/en/publications/exploring-grapheneos-secure-allocator-hardened-malloc
- hardened_malloc Source: https://github.com/GrapheneOS/hardened_malloc

**Titan M2 Certification**
- CC Portal Certificate (NSCIB-CC-2300073-01): https://www.commoncriteriaportal.org/files/epfiles/NSCIB-CC-2300073-01_CERT.pdf
- Google Security Blog (Pixel 7 Security): https://security.googleblog.com/2022/10/google-pixel-7-and-pixel-7-pro-next.html

**Cellebrite Leaked Documents**
- GrapheneOS Forum Discussion (July 2024): https://discuss.grapheneos.org/d/14344-cellebrite-premium-july-2024-documentation
- 404 Media (April 2024): https://www.404media.co/leaked-docs-show-what-phones-cellebrite-can-and-cant-unlock/
- 404 Media (October 2025 Teams Leak): https://www.404media.co/someone-snuck-into-a-cellebrite-microsoft-teams-call-and-leaked-phone-unlocking-details/

**Baseband Vulnerabilities**
- Google Project Zero Blog (March 2023): https://googleprojectzero.blogspot.com/2023/03/multiple-internet-to-baseband-remote-rce.html
- Natalie Silvanovich, "How to Hack Shannon Baseband," REcon 2023 / OffensiveCon 2023
- Amnesty International, "The Predator Files" (October 2023): https://www.amnesty.org/en/latest/research/2023/10/the-predator-files/
- Google Security Blog, Pixel 9 Baseband Hardening (October 2024): https://security.googleblog.com/2024/10/

**IOMMU Research**
- Markuze, Morrison, Tsafrir. "True IOMMU Protection from DMA Attacks." ASPLOS 2016, pp. 249-262. DOI: 10.1145/2872362.2872379
- De Bonfils Lavernelle et al. "DMA: A Persistent Threat to Embedded Systems Isolation." IEEE TrustCom 2024, pp. 101-108. DOI: 10.1109/TrustCom63139.2024.00041
- Markuze, Vargaftik et al. "Characterizing, exploiting, and detecting DMA code injection vulnerabilities." EuroSys 2021

**Traffic Analysis**
- Coull, Dyer. "Traffic Analysis of Encrypted Messaging Services." ACM SIGCOMM Computer Communication Review (CCR), Vol 44 No 5, 2014
- Bahramali, Soltani, Houmansadr, et al. "Practical Traffic Analysis Attacks on Secure Messaging Applications." NDSS 2020

**SimpleX Protocol**
- SimpleX Protocol Documentation: https://simplex.chat/docs/protocol
- SimpleX v5.7 PQC Announcement: https://simplex.chat/blog/

**Signal Post-Quantum Cryptography**
- Signal PQXDH Specification (September 2023): https://signal.org/docs/specifications/pqxdh/
- Signal SPQR / Triple Ratchet (October 2025): https://signal.org/blog/

**NIST Post-Quantum Standards**
- FIPS 203, 204, 205 (August 2024): https://csrc.nist.gov/projects/post-quantum-cryptography
- HQC Selection (March 2025): https://csrc.nist.gov/news/2025/additional-kem-for-post-quantum-cryptography
- pqm4 Benchmarks: https://github.com/mupq/pqm4
- WolfSSL STM32 Benchmarks: https://www.wolfssl.com/documentation/

**Hardware Security**
- Infineon TEGRION PQC Certification (January 2025): https://www.infineon.com/cms/en/about-infineon/press/
- NXP SE050 Datasheet: https://www.nxp.com/products/security-and-authentication/authentication/edgelock-se050
- Infineon OPTIGA Trust M: https://github.com/Infineon/optiga-trust-m
- Analog Devices DS3645 Datasheet: https://www.analog.com/en/products/ds3645.html
- Tropic Square TROPIC01: https://tropicsquare.com/

**Tamper Detection Research**
- Staat, Tobisch, Zenger, Paar. "Anti-Tamper Radio: System-Level Tamper Detection for Computing Systems." IEEE S&P 2022, pp. 1722-1736. DOI: 10.1109/SP46214.2022.00067

**Competitive Landscape**
- Precursor (betrusted.io): https://www.crowdsupply.com/sutajio-kosagi/precursor
- Bittium Tough Mobile: https://www.bittium.com/
- Purism Librem 5: https://puri.sm/products/librem-5/
- GSMK CryptoPhone: https://www.cryptophone.de/
- Nitrokey: https://www.nitrokey.com/
- Punkt MP02: https://www.punkt.ch/
- Fairphone: https://www.fairphone.com/

**Military and Industrial Standards**
- MIL-DTL-83528G: Conductive Elastomer Gaskets for EMI/RFI Suppression
- STYCAST 2850FT Technical Data: https://www.henkel-adhesives.com/
- W.L. Gore Tamper Respondent Technologies: https://www.gore.com/

**Android TCB Analysis**
- derdilla, AOSP LOC Analysis (October 2023, published January 2024)
- Google Trusty TEE Documentation: https://source.android.com/docs/security/features/trusty
- "The Android Platform Security Model," Mayrhofer et al., ACM

---

*This document is part of the SimpleGo project documentation. Licensed under AGPL-3.0.*

*Last updated: February 2026*
