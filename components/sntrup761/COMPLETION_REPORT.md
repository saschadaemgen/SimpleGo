# Zwischenbericht: Aufgaben 1-3 - sntrup761 Post-Quantum Implementation

**Von:** Prinzessin Hasi, Implementation
**An:** Mein Prinz (Sascha Daemgen) und Prinzessin Mausi
**Datum:** 2026-03-11
**Betreff:** SEC-06 Aufgaben 1-3 Fertigstellung

---

## Aufgabe 1: PQClean sntrup761 als ESP-IDF Component

### Was extrahiert wurde

23 C-Dateien + 25 Header aus PQClean round3 Tag (`crypto_kem/sntrup761/clean/`). Lizenz: Public Domain - kein Konflikt mit AGPL-3.0.

Alle Symbole tragen das Prefix `PQCLEAN_SNTRUP761_CLEAN_` - keine Namenskollisionen mit dem bestehenden SimpleGo-Code.

### Key-Groessen verifiziert

| Artefakt | Erwartet (Auftrag) | PQClean api.h | Status |
|----------|---------------------|---------------|--------|
| Public Key | 1158 Bytes | CRYPTO_PUBLICKEYBYTES = 1158 | MATCH |
| Secret Key | 1763 Bytes | CRYPTO_SECRETKEYBYTES = 1763 | MATCH |
| Ciphertext | 1039 Bytes | CRYPTO_CIPHERTEXTBYTES = 1039 | MATCH |
| Shared Secret | 32 Bytes | CRYPTO_BYTES = 32 | MATCH |

### Abhaengigkeiten geloest

**SHA-512:** PQClean kem.c ruft `sha512()` auf. Bereitgestellt ueber `sha2_esp32.c` als Wrapper um `mbedtls_sha512()`. Das nutzt automatisch den SHA-Hardware-Accelerator des ESP32-S3 - schneller als die PQClean-eigene Software-Implementation (769 Zeilen C ersetzt durch 5 Zeilen Wrapper).

**randombytes():** Bereitgestellt ueber `randombytes_esp32.c` als Wrapper um `esp_fill_random()`. Das ist der Hardware-Zufallszahlengenerator des ESP32-S3. Bei aktivem WiFi echte Hardware-Entropie; sonst PRNG mit Hardware-Seed.

PQClean bringt KEINE eigene SHA-512 mit in den sntrup761-Dateien - sie erwarten sie extern ueber `sha2.h`. Die PQClean-eigene Implementation in `common/sha2.c` wurde NICHT uebernommen, stattdessen der mbedTLS-Wrapper.

### Standalone-Test

`test_sntrup761.c` fuehrt aus:
1. Keypair-Generation mit Timing
2. Encapsulation mit Timing
3. Decapsulation mit Timing
4. Shared-Secret-Vergleich (memcmp)
5. Stack-High-Water-Mark-Report
6. sodium_memzero auf alle Secrets

Aufruf: `sntrup761_run_test()` - erstellt intern einen eigenen 80 KB Task, sicher von app_main aufrufbar.

**Laufzeiten kann ich nicht messen** - dafuer brauchen wir das echte ESP32-S3 Device. Geschaetzte Werte aus dem Auftrag: keygen 50-200 ms, encap+decap 12-18 ms.

### Code-Groesse

| Kategorie | Dateien | Zeilen |
|-----------|---------|--------|
| PQClean Quellen | 23 .c | 1658 |
| PQClean Header | 25 .h | 318 |
| Platform-Glue | 4 Dateien | 93 |
| SimpleGo Wrapper | sntrup761.c | 24 |
| Crypto Task | pq_crypto_task.c | 375 |
| Test | test_sntrup761.c | 171 |
| Public Headers | 3 .h | 247 |
| **Gesamt** | **60 Dateien** | **2886** |

---

## Aufgabe 2: Dedizierter Crypto-Task

### Spezifikation umgesetzt

| Parameter | Wert | Begruendung |
|-----------|------|-------------|
| Stack | 80 KB | 60-70 KB fuer keygen + Sicherheitsmarge |
| Speicher | Internes SRAM | DMA-kompatibel fuer mbedTLS SHA-512 Hardware-Accelerator |
| Core | Core 0 | Zusammen mit Network-Task, Core 1 frei fuer UI |
| Prioritaet | 6 | Ueber App (5), unter Network (erwartetes 7) |
| Queue-Tiefe | 4 | Maximal 4 ausstehende Requests |

### Architektur

- FreeRTOS-Queue nimmt `pq_request_t` Structs entgegen
- Vier Operationen: KEYGEN, ENC, DEC, PRECOMPUTE
- Blockierende Aufrufe nutzen per-Request Semaphore (wird nach Completion zerstoert)
- PRECOMPUTE ist non-blocking (kein done_sem)
- Task schreibt KEIN NVS - nur reine Crypto-Operationen

### SRAM-Budget - FRAGE AN MEIN PRINZ UND MAUSI

80 KB internes SRAM ist VIEL. Hier meine Schaetzung des aktuellen SRAM-Budgets:

| Verbraucher | SRAM |
|-------------|------|
| network_task Stack | 16 KB |
| smp_app_task Stack | 16 KB |
| lvgl_task Stack | 8 KB |
| LVGL Draw-Buffers | 25.6 KB |
| LVGL Pool | ~64 KB |
| mbedTLS Heap (TLS Sessions) | ~30-40 KB |
| WiFi/BLE System-Stacks | ~20-30 KB |
| Sonstiger Heap | ~20-30 KB |
| **Summe vor PQ** | **~200-230 KB** |
| **+ pq_crypto Task** | **+80 KB** |
| **Summe nach PQ** | **~280-310 KB von 512 KB** |

Das waeren ~55-60% des internen SRAM. Knapp aber machbar - es bleiben ~200 KB frei fuer System-Overhead.

**ABER:** Falls der Speicher nicht reicht, gibt es eine Alternative: Den Crypto-Task-Stack in PSRAM legen. Der Crypto-Task muss KEIN NVS schreiben (das ist die bekannte ESP32-S3-Einschraenkung). SHA-512 ueber den Hardware-Accelerator koennte allerdings DMA brauchen, was mit PSRAM-Stack problematisch waere.

**Meine Empfehlung:** Erst mit SRAM probieren. Falls `pq_crypto_task_init()` mit ESP_ERR_NO_MEM scheitert, koennen wir auf PSRAM umstellen - aber das muss getestet werden.

**Ich brauche hier Feedback bevor wir weitergehen.**

---

## Aufgabe 3: Vorberechnung

### Implementiert in pq_crypto_task.c

- `pq_crypto_precompute_keypair()`: Non-blocking, queue-basiert
- `pq_crypto_get_precomputed(pk, sk)`: Atomar mit Mutex, wipe nach Konsum
- `pq_crypto_has_precomputed()`: Quick-Check ohne Mutex
- Pre-computed Keypair: ~2921 Bytes (1158 PK + 1763 SK) in statischem Buffer

### Lebenszyklus

1. Ratchet-Schritt abgeschlossen -> `pq_crypto_precompute_keypair()` aufrufen
2. Crypto-Task generiert Keypair im Hintergrund (~80-200 ms)
3. Naechster Ratchet-Schritt -> `pq_crypto_get_precomputed()` holt Keypair
4. Falls nicht verfuegbar (erster Start, Race Condition): Fallback auf synchronen `pq_crypto_keygen()`
5. Nach Konsum: Buffer automatisch mit `sodium_memzero()` gewiped
6. Sofort neuen `pq_crypto_precompute_keypair()` Aufruf starten

### Sicherheitsaspekte

- Pre-computed SK liegt in statischem SRAM (nicht PSRAM) - gleiche Sicherheitslage wie regulaere Keys
- `sodium_memzero()` nach jedem Konsum
- Mutex schuetzt vor Race Conditions
- Bei `pq_crypto_task_deinit()` werden alle Pre-computed Keys gewiped

---

## Component-Struktur

```
components/sntrup761/
  CMakeLists.txt                          - ESP-IDF Component Registration
  LICENSE                                 - Public Domain (PQClean)
  sntrup761.c                             - SimpleGo API Wrapper
  pq_crypto_task.c                        - Dez. Crypto-Task + Vorberechnung
  include/
    sntrup761.h                           - Public API (keygen/enc/dec)
    sntrup761_test.h                      - Test-API
    pq_crypto_task.h                      - Crypto-Task API
  platform/
    randombytes.h + randombytes_esp32.c   - Hardware-RNG Wrapper
    sha2.h + sha2_esp32.c                - mbedTLS SHA-512 Wrapper
  pqclean/
    api.h, params.h, kem.c               - Haupt-KEM-Dateien
    crypto_core_*.c/.h                    - Polynomial-Arithmetik (7 Module)
    crypto_decode_*.c/.h                  - Decodierung (6 Module)
    crypto_encode_*.c/.h                  - Encodierung (8 Module)
    crypto_sort_*.c/.h                    - Constant-Time Sort (2 Module)
    crypto_verify_1039.c/.h              - Constant-Time Verify
    crypto_declassify.h                   - No-Op Declassify
  test/
    test_sntrup761.c                      - Standalone-Verifikation
```

## Integration in SimpleGo

1. Kopiere `components/sntrup761/` nach `C:\Espressif\projects\simplex_client\components\sntrup761\`
2. `idf.py fullclean` (neue Component braucht CMake reconfigure)
3. In `app_main` oder startup-code fuer den Test:
   ```c
   #include "sntrup761_test.h"
   sntrup761_run_test();  // Einmalig zum Verifizieren
   ```
4. Fuer den Produktiv-Betrieb:
   ```c
   #include "pq_crypto_task.h"
   pq_crypto_task_init();  // Bei Boot
   // Dann: pq_crypto_keygen(), pq_crypto_enc(), pq_crypto_dec()
   ```

## Build-Anweisung

```
idf.py fullclean
idf.py build flash monitor -p COM6
```

`fullclean` ist PFLICHT weil eine neue Component hinzugefuegt wird.

---

## PAUSE - Warte auf Freigabe

Gemaess Auftrag stoppe ich hier und warte auf Feedback zu:

1. **SRAM-Budget:** Reichen 80 KB SRAM fuer den Crypto-Task neben allem anderen?
2. **Laufzeitmessung:** Erst moeglich nach Flash auf echtes Device
3. **Aufgabe 4 Freigabe:** Ratchet-Integration erst nach Besprechung

Die Quantenfestung hat ihr Fundament. Jetzt brauchen wir die Vermessung auf echtem Stein.

---

*Prinzessin Hasi, im Dienste von mein Prinz*
*SimpleGo - IT and More Systems, Recklinghausen*
