---
title: "Class 1 - Known Vulnerabilities and Attack Research"
sidebar_position: 5
---

![SimpleGo Security Architecture - Hardware Class 1](../../.github/assets/github_header_security_architecture_class_1.png)

# Known Vulnerabilities and Attack Research

**Document version:** Session 44 | March 2026
**Applies to:** ESP32 family with focus on ESP32-S3 applicability
**Copyright:** 2025-2026 Sascha Daemgen, IT and More Systems, Recklinghausen
**License:** AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)

---

## Purpose and Honesty Policy

This document catalogs every published security vulnerability and research attack relevant to the ESP32 family, with an honest assessment of whether each applies to SimpleGo's ESP32-S3 implementation. Where Espressif's official advisories differ from independent research findings, both perspectives are presented. No vulnerability is downplayed, omitted, or marked "acceptable." Security researchers reviewing SimpleGo should be able to use this document as a starting point for their own assessment.

---

## Vulnerability Timeline

| Date | ID | Researcher | Target | Type | SimpleGo Impact |
|------|-----|-----------|--------|------|-----------------|
| Jul 2019 | CVE-2019-17391 | LimitedResults | ESP32 V1/V2 | Voltage FI | Not affected (different silicon) |
| Jun 2020 | CVE-2020-13629 | Raelize | ESP32 V1/V2 | EM FI | Not affected (different silicon) |
| Jun 2020 | CVE-2020-15048 | Raelize | ESP32 V1/V2 | EM FI + brute force | Not affected (different silicon) |
| Jan 2023 | IACR 2023/090 | Ledger Donjon | ESP32-V3 | Side-channel (CPA) | **Likely affected** (per AR2022-003) |
| Mar 2023 | AR2022-003 v2 | Espressif (acknowledging Donjon) | ESP32/S2/C3/S3 | Side-channel (AES) | **ESP32-S3 explicitly listed** |
| Aug 2023 | AR2023-005 | Espressif (acknowledging Delvaux/TII) | ESP32-V3 | EM FI | Unknown for S3 (untested) |
| Jan 2024 | Courk blog | Kevin Courdesses | ESP32-C3/C6 | SCA + Voltage FI | Relevant (shared AES design) |
| Aug 2024 | WOOT 2024 | Delvaux/TII/Raelize | ESP32-V3 | EM FI (PC control) | Unknown for S3 (untested) |
| Mar 2025 | CVE-2025-27840 | Tarlogic Security | ESP32 original | BT HCI commands | **Not affected** (different chip) |

---

## CVE-2019-17391: The Attack That Started It All

### What happened

Security researcher LimitedResults discovered that the ESP32 V1/V2's eFuse read-protection mechanism could be bypassed through voltage fault injection during the boot process. By injecting precisely timed voltage glitches on the VDD_RTC power rail during the approximately 500 microsecond window when eFuse values are loaded from OTP memory into shadow registers, the read-protection bits could be corrupted while the key data transferred correctly. This allowed software to read the Flash Encryption Key (BLK1) and Secure Boot Key (BLK2) that were supposed to be hardware-protected.

### Technical details

The attack targeted the eFuse controller's initialization sequence in the first-stage ROM bootloader. During normal operation, eFuse values are read from OTP into shadow registers in a single batch, with protection bits applied simultaneously. The voltage glitch corrupted the protection bit transfer while leaving the key data intact. By repeating the attack hundreds of times and statistically analyzing the partially corrupted readouts, the complete 256-bit keys could be recovered.

Equipment used: voltage glitch generator connected to VDD_RTC via a 1 Ohm shunt resistor, oscilloscope for timing, total equipment cost estimated under $1,000.

### Espressif response

Espressif acknowledged the vulnerability and released the ESP32-D0WD-V3 (ECO V3) with additional ROM checks to detect eFuse loading anomalies. The advisory stated that the V3 revision is "hardened against fault injection attacks in hardware and software." Espressif also recommended per-device unique keys (limiting blast radius) and noted that deployed V1/V2 hardware has no software mitigation.

### SimpleGo applicability: NOT AFFECTED

The ESP32-S3 uses a completely different silicon design from the original ESP32. It has different eFuse controller hardware, different ROM bootloader code, and different protection mechanisms. The specific voltage glitching technique targeting the VDD_RTC eFuse load sequence does not transfer to the ESP32-S3. However, the lesson - that eFuse protection is not absolute - applies universally and informs SimpleGo's defense-in-depth approach.

---

## CVE-2020-13629: Electromagnetic Fault Injection Bypasses Secure Boot

### What happened

Researchers Niek Timmers and Cristofaro Mune at Raelize demonstrated that a single electromagnetic pulse during the bootloader copy from external flash to internal SRAM could corrupt the data transfer, causing the CPU to execute attacker-controlled code from SRAM instead of the legitimate bootloader. This bypassed both Secure Boot signature verification and Flash Encryption in a single glitch.

### Technical details

The attack exploited two weaknesses in the ESP32 V1/V2. First, the UART bootloader could not be fully disabled and was always accessible - even with Secure Boot and Flash Encryption enabled. Second, data loaded into SRAM via the UART bootloader persisted across warm resets (watchdog-triggered resets did not clear SRAM). The attack sequence was:

```
1. Fill SRAM with attacker code via UART bootloader load_ram command
2. Trigger a warm reset (watchdog)
3. During boot, inject EM pulse when ROM copies bootloader from flash to SRAM
4. Glitch corrupts the flash-to-SRAM copy
5. CPU executes attacker's code that persisted in SRAM from step 1
6. Attacker code has full access to decrypted flash contents
```

### Espressif response

Espressif confirmed the vulnerability (CVE-2020-13629) and noted that the ESP32-V3 revision supports permanently disabling the UART bootloader via the UART_DOWNLOAD_DIS eFuse, blocking this attack chain. For deployed V1/V2 hardware, no mitigation exists.

### SimpleGo applicability: NOT AFFECTED

The ESP32-S3 supports permanent UART download mode disabling via eFuse. In SimpleGo Mode 4 (Bunker), DIS_DOWNLOAD_MODE is burned, eliminating this attack vector entirely. Even in lower modes, the ESP32-S3's Secure Download Mode restricts UART functionality to prevent arbitrary code execution.

---

## CVE-2020-15048: Flash Encryption Bypass via Brute Force

### What happened

Building on CVE-2020-13629, Raelize demonstrated that even with Flash Encryption enabled (preventing the SRAM persistence attack), an attacker could load an arbitrary value into the CPU's Program Counter by brute-forcing a 4-byte ciphertext word leaked via the serial interface during the glitched boot.

### Technical details

The attack required brute-forcing a 2^32 search space at approximately 10 experiments per second - theoretically over 10 years. Raelize acknowledged this was impractical but demonstrated the principle that Flash Encryption does not completely prevent fault injection attacks on the ESP32 V1/V2.

### SimpleGo applicability: NOT AFFECTED

The brute force requirement makes this impractical even on the affected ESP32 V1/V2, and the ESP32-S3 has different boot ROM code that does not expose the same vulnerability pattern.

---

## IACR 2023/090: Side-Channel AES Key Extraction (THE PRIMARY THREAT)

### What happened

Researchers Karim Abdellatif, Olivier Heriveaux, and Adrian Thillard from Ledger's Donjon security lab demonstrated Correlation Power Analysis (CPA) against the ESP32-V3's hardware AES accelerator. They extracted the complete 256-bit flash encryption key by measuring the chip's power consumption during the boot-time flash decryption process. The attack was validated against a Blockstream Jade hardware cryptocurrency wallet, demonstrating real-world impact.

### Technical details

The attack targeted the AES block cipher's SubBytes operation, where the S-box lookup creates data-dependent power consumption patterns. The ESP32's hardware AES accelerator has zero side-channel countermeasures - no masking, no clock randomization, no dummy rounds, no power consumption balancing.

**Equipment:** Tektronix MSO54 oscilloscope at 3 GS/s sampling rate, custom EM probe near the chip surface, automated measurement collection software.

**Measurements required:** Approximately 60,000 traces to extract the first AES round key, approximately 300,000 traces for the full 256-bit key with high confidence.

**Time:** Approximately 2 hours for trace collection, plus analysis time.

The researchers performed two separate analyses. First, a direct CPA attack on the AES accelerator using known plaintext inputs, which succeeded with approximately 60,000 traces. Second, a firmware decryption attack targeting the XTS-AES decryption of flash contents during boot, which required approximately 300,000 traces because the attacker does not control the plaintext (it is the encrypted firmware).

### Espressif response

Espressif published Security Advisory AR2022-003 (Version 2.0), acknowledging the vulnerability. The critical statement: "This side channel attack may be applicable for Espressif SoCs including ESP32, ESP32-S2, ESP32-C3 and **ESP32-S3.**" Espressif stated that hardware countermeasures would be incorporated in future chip generations but offered no fix for existing silicon.

Espressif's mitigations for deployed hardware were limited to general recommendations: use unique per-device keys (limits blast radius to single device), combine Flash Encryption with Secure Boot (prevents firmware tampering even if encryption key is extracted), and consider the physical access requirement in the threat model.

### SimpleGo applicability: LIKELY AFFECTED

The ESP32-S3 is explicitly listed in Espressif's advisory as likely vulnerable. No independent researcher has published a CPA attack specifically against the ESP32-S3, but the hardware AES accelerator shares the same unprotected design across the ESP32 family. The conservative and honest assessment is that the attack applies.

**Impact on SimpleGo's HMAC NVS encryption:** The published attack targets the AES-XTS engine used for flash encryption and NVS encryption. However, the HMAC key derivation uses the SHA-256-based HMAC peripheral, not the AES accelerator. CPA techniques optimized for AES S-box operations do not directly transfer to SHA-256's different mathematical structure (mix of additions, rotations, and XOR operations). The HMAC peripheral has not been specifically targeted in published research.

This does NOT mean the HMAC key is safe from side-channel analysis - it means the attack would require new research specific to SHA-256 side-channels on ESP32 hardware, rather than applying the published AES methodology directly. The difficulty is higher, the research does not yet exist, but it cannot be ruled out.

### What this means for SimpleGo Class 1

In the worst case, an attacker with physical access and CPA equipment can extract the flash encryption key (if flash encryption is enabled in Mode 3/4) in approximately 2 hours. Whether they can also extract the HMAC key is unknown but must be assumed possible with additional research effort.

The defense is layered: even with extracted keys, the attacker only compromises one specific device. The SimpleX Protocol's forward secrecy means that past messages cannot be decrypted even with current keys. And the four encryption layers mean that transport-level key extraction still leaves three additional encryption envelopes intact for in-transit messages.

---

## USENIX WOOT 2024: Program Counter Control on ESP32-V3

### What happened

Jeroen Delvaux, working with the Technology Innovation Institute (Abu Dhabi) and Raelize, achieved CPU Program Counter control on the ESP32-V3 using a single electromagnetic fault injection pulse. This redirected code execution from the signature verification routine to the ROM's UART Download Mode, giving the attacker access to decrypted flash contents.

### Technical details

The attack used the SPI flash chip-enable signal as a timing reference to precisely target the moment after encrypted firmware was loaded into SRAM but before the RSA signature was verified. A single EM pulse (using a Riscure EM-FI Transient Probe with 4mm tip) caused the CPU to skip the verification branch and jump to the Download Mode entry point.

**Equipment:** Riscure EM-FI Transient Probe, Teledyne LeCroy WavePro 804HD oscilloscope, precision XYZ positioning stage. Professional-grade equipment totaling approximately $30,000-50,000.

Unlike the Ledger Donjon CPA attack which requires hundreds of thousands of measurements, this attack requires only a single well-timed pulse once the parameters are optimized. Parameter optimization requires experimentation, but once found, the attack is repeatable.

### Espressif response

Espressif published advisory AR2023-005 acknowledging the attack on ESP32-V3.0 and V3.1. The advisory confirmed that the ESP32's ROM-implemented UART Download Mode entry point is not hardened against fault injection, and the ROM code's redundant checks can be bypassed with appropriately timed electromagnetic disruption.

### SimpleGo applicability: UNKNOWN

No researcher has published EM-FI results against the ESP32-S3 specifically. The ESP32-S3 uses Xtensa LX7 cores (different from the V3's Xtensa LX6) and has different ROM code. However, the fundamental vulnerability pattern - EM disruption of CPU execution flow during security-critical boot code - applies to any processor without hardware fault injection countermeasures. The ESP32-P4's Power Glitch Detector would provide some mitigation for this class of attack.

---

## Courk 2024: ESP32-C3 and C6 Countermeasures Proven Ineffective

### What happened

Independent researcher Kevin Courdesses reproduced the Ledger Donjon CPA attack on the ESP32-C3 and ESP32-C6. More significantly, he demonstrated that the ESP32-C6's DPA countermeasures (anti-attack pseudo-rounds and clock randomization) were ineffective. He then combined side-channel key extraction with voltage fault injection to bypass Secure Boot on both chips.

### Technical details

The ESP32-C6 was specifically designed with anti-DPA features: pseudo-rounds that insert random dummy AES operations, and clock randomization that varies the AES engine's timing. Courdesses found that the masking had minimal impact on CPA effectiveness, and the clock randomization could be defeated by inferring the crypto clock behavior from the power trace itself. The ESP32-C3, which has no DPA countermeasures, was broken with the same straightforward CPA approach as the ESP32-V3.

For the Secure Boot bypass, Courdesses exploited buffer overflow vulnerabilities in the boot ROM's image verification code, which the redundant-check countermeasures (the `check_condCOUNTER.XXX()` functions visible in the published ROM ELF files) failed to prevent.

### Espressif response

Espressif published advisory AR2023-007 acknowledging the findings. The advisory stated that "no software or hardware fix is available" for affected chips and that hardware countermeasures would be improved in future generations.

### SimpleGo applicability: HIGHLY RELEVANT

Although the ESP32-S3 is not the C3 or C6, this research has critical implications. First, it proves that Espressif's first generation of DPA countermeasures (as shipped in the C6) is ineffective - which means the ESP32-P4's similar anti-DPA pseudo-rounds (controlled by XTS_DPA_PSEUDO_LEVEL) should be treated as unproven until independently validated. Second, it demonstrates that the AES accelerator vulnerability is consistent across the ESP32 family, supporting the assumption that the ESP32-S3 is also vulnerable. Third, it shows that Secure Boot ROM code has implementation bugs that can be exploited via fault injection, suggesting that ROM code quality is a systemic concern across Espressif's chip lineup.

---

## CVE-2025-27840: Undocumented Bluetooth HCI Commands

### What happened

Tarlogic Security disclosed 29 undocumented vendor-specific HCI (Host Controller Interface) commands in the ESP32's Bluetooth firmware at RootedCON Madrid in March 2025. These commands include capabilities for reading and writing RAM, reading and writing flash memory, and spoofing Bluetooth MAC addresses. Initial media coverage described these as a "backdoor," but subsequent analysis clarified they are debugging and manufacturing test commands that were not removed from production firmware.

### Technical details

The commands are accessible only through the HCI interface, which requires either physical UART/USB access or prior firmware compromise to reach. They are not remotely exploitable over Bluetooth without first establishing an authenticated HCI connection. The commands affect only the original ESP32 chip's Bluetooth controller firmware, which runs on a separate ULP (Ultra Low Power) processor from the main application.

### Espressif response

Espressif published a clarification blog post stating these are standard debugging features present in many Bluetooth implementations, not intentional backdoors. They committed to removing them from future firmware releases and providing configuration options to disable them. The CVE was assigned a CVSS score of 6.8 (Medium).

### SimpleGo applicability: NOT AFFECTED

SimpleGo uses the ESP32-S3, which has a different Bluetooth controller from the original ESP32. More importantly, SimpleGo does not use Bluetooth at all - the Bluetooth radio is not initialized in the firmware. The WiFi stack is the only wireless interface active. Even if similar undocumented commands existed in the ESP32-S3's Bluetooth firmware (not demonstrated), they would be inaccessible because the Bluetooth subsystem is never started.

---

## Attack Applicability Matrix

| Attack | Requires Physical Access | Requires Lab Equipment | Time to Execute | Affects ESP32-S3 | Affects HMAC NVS | Affects Flash Encryption |
|--------|------------------------|----------------------|-----------------|-------------------|-------------------|--------------------------|
| Voltage FI (CVE-2019-17391) | Yes | ~$1,000 | Hours | No | N/A | N/A |
| EMFI (CVE-2020-13629) | Yes | ~$5,000 | Hours | No | N/A | N/A |
| CPA AES key (IACR 2023/090) | Yes | ~$2,000-30,000 | 2+ hours | **Likely** | Indirectly | **Yes** |
| EMFI PC control (WOOT 2024) | Yes | ~$30,000-50,000 | Minutes (once tuned) | Unknown | Unknown | Yes (bypasses boot) |
| CPA + FI combo (Courk 2024) | Yes | ~$2,000-5,000 | Hours to days | Relevant | Relevant | Yes |
| BT HCI (CVE-2025-27840) | Physical or firmware | None | Minutes | No | No | No |

---

## Recommendations for SimpleGo

Based on the complete vulnerability landscape:

1. **HMAC NVS encryption is the correct choice for key protection.** The HMAC peripheral uses SHA-256 (not AES), which has not been targeted by published CPA research on ESP32. This provides a meaningful security advantage over flash-encryption-based NVS.

2. **Secure Boot is essential for Mode 3/4.** Without it, the derived XTS keys in RAM are accessible to malicious firmware. With it, only signed firmware runs, and the key derivation is trusted.

3. **Per-device unique keys are mandatory.** Every SimpleGo device must generate its own HMAC key at first boot. No shared keys, no master keys, no fleet-wide secrets. This limits any successful attack to a single compromised device.

4. **Do not claim resistance to state-level physical attacks.** The published research clearly demonstrates that the ESP32 AES accelerator can be broken with $2,000 equipment in hours. The HMAC peripheral may resist longer, but claiming it is unbreakable would be dishonest. Hardware Class 2 and 3 with dedicated secure elements are the answer for advanced physical attack resistance.

5. **The ESP32-P4 is an improvement but not a guarantee.** Its anti-DPA pseudo-rounds and Power Glitch Detector are promising but unproven. The ESP32-C6's similar countermeasures were broken. Treat P4 security features as raising the bar, not as absolute protection.

6. **Monitor ongoing research.** The ESP32 security research community is active. New attacks may be published that affect the ESP32-S3 directly. This document should be updated when new research is published.

---

## References

| ID | Source | Description |
|----|--------|-------------|
| CVE-2019-17391 | limitedresults.com + HackMag | Voltage glitch eFuse key extraction, ESP32 V1/V2 |
| CVE-2020-13629 | raelize.com | EM fault injection Secure Boot bypass, ESP32 V1/V2 |
| CVE-2020-15048 | raelize.com | Flash Encryption bypass via brute force, ESP32 V1/V2 |
| IACR 2023/090 | eprint.iacr.org | Ledger Donjon CPA attack on ESP32-V3 AES, 300K traces |
| AR2022-003 V2 | espressif.com/advisory | Espressif advisory listing ESP32-S3 as affected |
| AR2023-005 | espressif.com/advisory | Espressif advisory for ESP32-V3 EMFI |
| WOOT 2024 | usenix.org | Delvaux et al., PC control via single EM pulse on V3 |
| Courk 2024 | courk.cc | ESP32-C3/C6 SCA + FI, DPA countermeasures ineffective |
| AR2023-007 | espressif.com/advisory | Espressif advisory for C3/C6 findings |
| CVE-2025-27840 | Tarlogic/BleepingComputer | Undocumented BT HCI commands in ESP32 original |
| IACR 2019/401 | eprint.iacr.org | Side-channel assessment of open-source hardware wallets |
| Ledger Donjon Jade blog | ledger.com/blog | Evil-maid attack demonstration on Jade wallet |

---

*SimpleGo - IT and More Systems, Recklinghausen*
*First native C implementation of the SimpleX Messaging Protocol*
*AGPL-3.0 (Software) | CERN-OHL-W-2.0 (Hardware)*
