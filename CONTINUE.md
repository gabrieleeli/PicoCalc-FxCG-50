# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.8.0 — collaudo software di GRAPH e microSD,
completamento di TABLE**, branch **`version/v1.8.0`** (rapporto in
`docs/reports/SESSIONE_v1.8.0_COLLAUDO_TABLE.md`).

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| `main` | codice originale caricato | non modificare |
| `version/v1.5.4`, `version/v1.6.0` | release storiche | non modificare |
| `version/v1.7.0` | Sessione 1 (audit, architettura, Run-Matrix), base della v1.8.0 (`72285e9`) | non modificare |
| **`version/v1.8.0`** | GRAPH, microSD, TABLE e collaudo software | si continua qui finché l'utente non approva la release |
| `claude/loving-meitner-jifxhg` | branch di sviluppo delle sessioni v1.8.0, allineato a `version/v1.8.0` | storico |

Non creare `version/v1.9.0` finché il collaudo fisico della v1.8.0 non è
valutato e l'utente non lo chiede. Nessun tag/release senza richiesta. Mai
force-push.

## 1. Stato attuale

- **Firmware RP2350 compilabile**: `build/pico2w/picocalc_fxai.uf2` (3 012 096
  byte, ≈1470 KB di flash, RAM statica ≈286 KB, heap ≈194 KB, stack 32 KB),
  nessun warning nei sorgenti del progetto; `picotool info`: 1.8.0, `pico2_w`.
- **Test host**: 25 test CTest (24 suite, 7169 controlli + `sd_interop_test`
  con 41 controlli), 0 errori, anche con ASan/UBSan. Relay AI: 15 test.
- **Nessun test su hardware fisico**: procedura pronta in
  `docs/HARDWARE_TEST_v1.8.0.md`, diagnostica sul dispositivo in SYSTEM → F2
  DIAG (righe `[DIAG]` anche sulla seriale USB).
- GRAPH: `docs/GRAPH_PARITY.md` (135 voci: 29 TESTED ON SIMULATOR, 92 TESTED ON
  HOST, 3 IMPLEMENTED, 2 parziali, 6 NOT STARTED, 3 fuori ambito). **Parità
  completa non raggiunta.**
- TABLE: completa secondo il manuale 5-32…5-35 tranne Dual Screen
  (`docs/TABLE.md`).
- microSD: pila verificata sul PC con scheda simulata e con fsck/mtools su
  immagini FAT16/FAT32/exFAT (`docs/STORAGE.md`); bus SPI0 reale non provato.
- Screenshot GRAPH/TABLE 06, 07, 20–37 (`tests/sim/graph.txt`), Run-Matrix 12–19.

## 2. Cosa è stato fatto nell'ultima sessione

| Area | Contenuto |
|---|---|
| GRAPH | `graph_verify_test` (precisione ≤ 1 px, valori analitici); corretti ∫dx sui poli (ora Math ERROR), integrali con salti, record flash superabile con lo SKETCH PEN (limite 1200 punti, messaggio "Memoria flash piena"); tratteggio 4× più rapido |
| microSD | corretto il blocco dopo l'estrazione tra due operazioni (reinizializzazione + nuovo tentativo) e il messaggio della scheda non formattata; test di guasti, dimensioni, cartelle, scheda piena; `sd_interop_test`; `tools/sd_image_tool`; simulatore con `PICOCALC_SD_IMAGE`; SYSTEM → DIAG |
| TABLE | riscritta: lista delle relazioni, Y=/r=/Param, Variable Range/List, SET, Setup, EDIT, ROW, DELETE, LISTMEM, GPH-CON/PLT dalle righe; workspace v4; `table_test` |
| Documentazione | TABLE, HARDWARE_TEST_v1.8.0, rapporto di sessione; aggiornati GRAPH_PARITY, GRAPH_ENGINE, STORAGE, PCGRAPH_FORMAT, FEATURE_MATRIX, TESTING, ROADMAP, GRAPHICS_ENGINE, APPLICATIONS_ARCHITECTURE, ARCHITECTURE, HARDWARE, BUILD_STATUS, CHANGELOG, README |

## 3. Funzionalità ancora da completare

- Dual Screen (G+G, GtoT, T+G) e Graph to Table; comandi grafici di
  Run-Matrix/programmi; FORMAT durante lo SKETCH; suffissi angolari °/ʳ/ᵍ.
- Liste di 999 elementi (oggi 255: limita LISTMEM).
- Run-Matrix: Punto 3 (Natural V.P.A.M. ↔ lineare), matrici/liste nel parser,
  Base-N intera, simboli ingegneristici.
- Modalità con solo modello dati: eActivity, Spreadsheet, Dyna Graph,
  Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot,
  3D Graph; Python; E-CON4 non realizzabile.

## 4. Prossimi passaggi esatti

1. **Ricevere ed esaminare i risultati del collaudo fisico**
   (`docs/HARDWARE_TEST_v1.8.0.md`: esiti HW-01…HW-36, log `[DIAG]`, foto);
   correggere ogni KO con un test che lo riproduca; aggiornare a TESTED ON
   HARDWARE solo le voci provate, con data e revisione della scheda.
2. Poi la v1.9.0 secondo `docs/ROADMAP.md` §2.

## 5. Problemi noti / da verificare

- Nessuna verifica su PicoCalc: display, tastiera, flash, microSD (SPI0 a
  12,5 MHz, card detect GP22), Wi-Fi, TLS; tempi di GRAPH e TABLE sul
  Cortex-M33 (li misura SYSTEM → DIAG).
- microSD: accesso a settore singolo (niente CMD18/CMD25); il firmware non
  formatta schede; `f_getfree` su FAT32 con FSInfo non valido può essere lento
  (scansione della FAT).
- Il record del workspace (~63,7 KB) può ancora essere superato da molte
  conversazioni AI lunghe: l'autosalvataggio lo segnala ("Memoria flash
  piena"), ma la soluzione definitiva (spostare la chat o i dati grandi sulla
  microSD) è da fare.
- Senza RTC, mbedTLS non verifica le date dei certificati e FatFs usa la data
  fissa 1/1/2026.
- Ritardo di 16 ms della tastiera I2C ripreso dal driver ufficiale, da ottimizzare.

## 6. Comandi per riprendere

```bash
# Toolchain (Ubuntu 24.04)
sudo apt-get install -y gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib cmake ninja-build
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git /opt/sdk/pico-sdk
git -C /opt/sdk/pico-sdk submodule update --init --depth 1 lib/tinyusb lib/cyw43-driver lib/lwip lib/mbedtls
export PICO_SDK_PATH=/opt/sdk/pico-sdk
```

```bash
# Test host (con e senza sanitizer)
tests/run_host_tests.sh
tests/run_host_tests.sh --sanitize
```

```bash
# Firmware UF2 e pacchetto di release (dist/)
scripts/build_pico2w.sh
scripts/make_release.sh
```

```bash
# Simulatore: screenshot di Run-Matrix e di GRAPH/TABLE
build/host/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host/picocalc_sim docs/screenshots tests/sim/graph.txt
```

```bash
# microSD: immagine reale nel simulatore, strumento e controllo con fsck
sudo apt-get install -y dosfstools mtools exfatprogs
truncate -s 64M sd.img && mkfs.fat -F 32 sd.img
PICOCALC_SD_IMAGE=sd.img build/host/picocalc_sim /tmp/shots -
build/host/sd_image_tool sd.img ls PICOCALC/GRAPH
fsck.fat -n sd.img
```

```bash
# Relay AI: test
cd relay && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python -m unittest discover -s tests
```

```bash
# Stack sul target di un sorgente (flag dal build del firmware)
cd build/pico2w && FL=CMakeFiles/picocalc_fxai.dir/flags.make
eval arm-none-eabi-g++ $(grep ^CXX_DEFINES $FL | cut -d= -f2-) $(grep ^CXX_INCLUDES $FL | cut -d= -f2-) $(grep ^CXX_FLAGS $FL | cut -d= -f2-) -fstack-usage -c ../../src/graph_engine.cpp -o /tmp/graph_engine.o
```
