# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.16.0 — eActivity su entrambe le schede** (milestone
**M2** del piano M0…M15), branch **`main`**, tag `v1.16.0` creato dal workflow
`.github/workflows/release.yml`. Rapporti: `docs/reports/ANALISI_FONTI_v1.16.0.md`
(capitolo 10, progetto, tabella di parità E01–E56 prima del codice),
`docs/reports/SVILUPPO_v1.16.0.md` (parità finale con le prove, differenze,
test, firmware). M0/M1: `ANALISI_FONTI_v1.15.0.md`, `SVILUPPO_v1.15.0.md`.

> **Pico 2 W congelato alla v1.16.0.** Dalla v1.17.0 nessuna funzione,
> schermata o crescita dell'immagine sul Raspberry Pi Pico 2 W, salvo
> correzioni di errori condivise. Immagine finale del Pico 2 W:
> **{{PICO2W_SIZE}} byte** (budget 2 064 384 B). `architecture_test` fallisce se
> cambia l'insieme delle modalità Available del Pico 2 W.

## 0. Piano delle milestone (dal prompt di sviluppo della v1.15.0)

Ogni milestone è una versione con il cancello completo (test su entrambi i
profili anche `--sanitize --tsan`, fuzzing ≥ 300 s per harness nuovo o
toccato, simulatore con gli screenshot vecchi identici, firmware delle due
schede senza warning e nei budget, relay 15/15, documentazione) e **un solo
commit su `main`** in fast-forward («PicoCalc FxCG-50 v<ver>: <contenuto>»).

| Milestone | Versione | Contenuto | Stato |
|---|---|---|---|
| M0 | — | analisi e cancello sulla v1.14.0 | **fatta** (`ANALISI_FONTI_v1.15.0.md`) |
| M1 | v1.15.0 | fondamenta comuni F1–F9 | **fatta** |
| M2 | v1.16.0 | eActivity su entrambe le schede; Pico 2 W congelato | **fatta** |
| **M3** | **v1.17.0** | **Statistics completa (cap. 6), solo Plus** | **PROSSIMA** |
| M4…M15 | v1.18.0…v1.29.0 | solo Pimoroni Pico Plus 2 W: Dual Screen, Recursion, Dyna, Conic, Financial, Spreadsheet, Program (C/C++/C#), Picture Plot, Geometry, Memory/System/Link, Python, chiusura | da fare |

Regole fisse: manuale fx-CG50 v3.60 solo come descrizione del comportamento;
`ANALISI_FONTI_v<ver>.md` con la tabella di parità **prima** del codice; un test
per voce; una modalità è Available solo con la tabella completa; nessun
«TESTED ON HARDWARE» senza prova fisica; HARDWARE_TEST con numerazione
continua (ultima usata: **HW-219**). Budget del Pico 2 W: immagine ≤ 2 064 384 B
(controllo bloccante), documento eActivity aperto ≤ 24 KB
(`limits::kEActivityDocumentBytes`), record ≤ 59,75 KB. Plus: < 90 % della
partizione da 4 MB (controllo bloccante).

### Decisioni prese

- **Program** (risposta dell'utente in M0: «usa il c, c++ e c#»): sottoinsiemi
  interpretati di C, C++ e C# (nucleo F9 pronto, `include/program.hpp`); la
  modalità arriva nella M10. Se l'interpretazione non è quella voluta va
  chiarito con l'utente prima della M10.
- **Distribution** (cap. 18 della fx-CG50, OS 3.40+): non è nel registro né nel
  piano; **chiedere all'utente** se aggiungerla (probabilmente con la M3).
- F2.5 (codifica per parti delle sezioni esistenti del workspace): fatta nella
  M2 (`encodeParts`/`decodeParts`/`resetPart`).
- Espressioni Math nelle righe di testo di eActivity: testo lineare tra
  U+F8F0 e U+F8F1 (U+E000–E002 sono simboli statistici del font).
- Pico 2 W: `-Os` per eActivity e per i sorgenti fuori dai percorsi di calcolo
  e disegno (`FXAI_PICO2W_SIZE_SOURCES`); la Plus resta a `-O2`.

## 1. Stato attuale

**v1.16.0** (sopra la v1.15.0): **eActivity Available su entrambe le schede**
alla parità con il capitolo 10 (ogni voce E01–E56 presente o differenza
inevitabile D1–D7): elenco dei file e cartelle, righe di testo con espressioni
Math, colori, marcatori, memo, righe di calcolo con il nucleo di Run-Matrix e
ricalcolo fino alla riga di stop (StepTask, AC = Break), immagini `.pcpict`,
strisce con dati propri (sessione da striscia del `Context`, barra azzurra,
SHIFT ' e SHIFT ,), Notes, memorie Mat/Vct/List per documento, limite del
documento 24 KB / 256 KB. Record v11 con la sezione `0x0102`. Guida
`docs/EACTIVITY.md`; numeri in `docs/reports/SVILUPPO_v1.16.0.md` e
`docs/BUILD_STATUS.md`. **Nessuna prova su hardware**:
`docs/HARDWARE_TEST_v1.16.0.md` (HW-202…HW-219).

## 2. Prossima sessione: M3 = v1.17.0 Statistics completa (solo Plus)

1. `git fetch origin main`; branch di sessione da `main`.
2. **Chiedere all'utente** se aggiungere **Distribution** (cap. 18, OS 3.40+)
   e, prima della M10, confermare Program in C/C++/C#.
3. `docs/reports/ANALISI_FONTI_v1.17.0.md` con la tabella di parità del
   capitolo 6 (Statistics): editor delle liste completo (pagine dei menu,
   SORT, DEL, DEL-ALL, INS, ADD, sottonomi, CSV), tutti i grafici (Scatter,
   xyLine, NPP, istogramma, box plot, normale, barre, torta, regressioni con
   tutti i tipi incluse sinusoidale e logistica), CALC 1/2-Var e regressioni
   con i residui, TEST (Z, t, χ², F, 2-campioni, ANOVA a una e due vie),
   INTR, DIST complete; poi codice **solo sulla Plus** (`boards::PicoPlus2W`
   per le funzioni nuove: il Pico 2 W resta quello della v1.16.0).
4. Ogni funzione nuova di Statistics sulla Plus deve lasciare identici gli
   screenshot e il comportamento del Pico 2 W (profilo host PICO2W) e la sua
   immagine (≤ {{PICO2W_SIZE}} B, nessuna crescita salvo correzioni).
5. Strisce di eActivity delle modalità nuove: aggiungere le parti del
   workspace e la schermata d'ingresso (`stripentry`), poi
   `eact::stripAvailable` le rende disponibili da sola (solo sulla Plus).

## 3. Cosa è stato fatto nelle ultime sessioni (M0, M1, M2)

| Area | Contenuto |
|---|---|
| Git | partenza da `main` = `72d0921`; commit intermedi su `claude/fervent-albattani-hepkis`; un solo commit su `main` in fast-forward; tag e release dal workflow |
| M0 | manuale v3.60 scaricato e capitoli verificati; cancello sulla v1.14.0 (47/47 per profilo anche con i sanitizer, relay 15/15, 94 screenshot identici, firmware 1 948 024 / 1 939 180 B senza warning); `tools/map_sizes.py`, `tools/sim_compare.py`; fixture `workspace_v1_14_0.inc`; domanda su Program |
| M2 | eActivity: `include/eactivity.hpp`, `src/eactivity.cpp`, `src/apps/eactivity_app.cpp`; parti del workspace e sezione `0x0102`; sessione da striscia; schermate d'ingresso in Run-Matrix, Graph, Statistics, Equation, 3D Graph; `eactivity_test` (E01–E56), `fuzz_eactivity`, screenshot 108–127; `-Os` sul Pico 2 W; congelamento in `architecture_test` |
| M1 | F1–F9 (sopra); test `workspace_v115_test`, `typed_files_test`, `mode_shell_test`, `text_editor_test`, `step_task_test`, `graph2d_test`, `binding_test`, `program_core_test`; fuzzing `fuzz_typedfile`, `fuzz_texteditor`, `fuzz_program`, `fuzz_workspace` e `fuzz_expr` aggiornati; screenshot 105–107 |

## 4. Collaudo fisico

Resta prioritario appena la PicoCalc è disponibile: HW-01…HW-219 (log USB
dall'accensione, righe `[psram]`, `[flash]`, `[mem]`, `[DIAG]`, `[fault]`,
foto). Ogni KO va riprodotto con un test e corretto; solo le voci provate
passano a TESTED ON HARDWARE (data, modulo, revisione della scheda).

## 5. Problemi noti / da verificare

- Problemi aperti della revisione v1.13.1 (A1–A6 di `docs/reports/ERRORI_v1.13.1.md`):
  record illeggibile dopo il ritorno a un firmware precedente, precisione di
  HypergeoPD con argomenti tutti grandi, risultati sotto 1E−99 in Run-Matrix
  da verificare su una fx-CG50, screenshot storici 01–05 e 09–11 non
  rigenerabili, nessun collaudo su hardware, SOLVER di EQUATION con SolveN/FMin/FMax
  nell'equazione (Time Out solo dopo un calcolo lungo).
- Nessuna verifica su PicoCalc: display, tastiera, flash, PSRAM (timing:
  valori SparkFun per l'APS6404L), microSD (SPI0 a 12,5 MHz, card detect GP22),
  Wi-Fi, TLS; tempi di GRAPH, TABLE e 3D sul Cortex-M33 (li misura SYSTEM →
  DIAG).
- Le scritture flash mascherano gli IRQ per un settore (≈ 45 ms tipici) o per
  1 KB di programmazione.
- microSD: accesso a settore singolo (niente CMD18/CMD25); il firmware non
  formatta schede; `f_getfree` su FAT32 con FSInfo non valido può essere lento.
- Sul Pico 2 W il record del workspace (59,75 KB) può ancora essere superato da
  molte conversazioni AI lunghe: l'autosalvataggio lo segnala ("Memoria flash
  piena"); sulla Plus il record è di 512 KB.
- Senza RTC, mbedTLS non verifica le date dei certificati e FatFs usa la data
  fissa 1/1/2026 (LittleFS non registra date).
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
# Test host: entrambi i profili (build/host-pico_plus_2w, build/host-pico2w)
tests/run_host_tests.sh
tests/run_host_tests.sh --sanitize --tsan        # ASan/UBSan + ThreadSanitizer
tests/run_host_tests.sh --board PICO2W           # un solo profilo
```

```bash
# Firmware UF2 (una cartella per scheda) e pacchetto di release (dist/)
scripts/build.sh --board pico_plus_2w
scripts/build.sh --board pico2w
scripts/make_release.sh
```

```bash
# Simulatore: tutti gli script con il confronto pixel per pixel (v1.15.0)
python3 tools/sim_compare.py build/host-pico_plus_2w/picocalc_sim /tmp/shots
python3 tools/sim_compare.py --compress /tmp/shots/NNN_nome.png docs/screenshots/NNN_nome.png   # screenshot nuovi
# Simulatore (profilo Plus, pool PSRAM simulato): Run-Matrix, MAT/VCT e novità 1.10.0 e 1.11.0, EQUATION 1.12.0, GRAPH/TABLE, 3D
# (il simulatore scrive PNG non compressi: comprimerli senza perdita prima di copiarli in docs/screenshots)
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v110.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v111.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v112.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/equation.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d_v114.txt   # 3D Graph v1.14.0 (88-104)
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/foundations_v115.txt # MEMORY › FILES (105-107)
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/eactivity.txt        # eActivity v1.16.0 (108-127)
```

```bash
# Fuzzing leggero (v1.13.1): clang con libFuzzer (Ubuntu: libclang-rt-18-dev), ASan + UBSan
CC=clang CXX=clang++ cmake -S tools/fuzz -B build/fuzz -G Ninja -DFXAI_TEST_BOARD=PICO_PLUS_2W
cmake --build build/fuzz
tools/fuzz/run_fuzz.sh build/fuzz 300       # secondi per harness (12 harness dalla v1.16.0); crash in build/fuzz/artifacts
FUZZ_ONLY="fuzz_workspace fuzz_program" tools/fuzz/run_fuzz.sh build/fuzz 300   # solo gli harness nuovi o toccati
```

```bash
# microSD: immagine reale nel simulatore, strumento e controllo con fsck
sudo apt-get install -y dosfstools mtools exfatprogs
truncate -s 64M sd.img && mkfs.fat -F 32 sd.img
PICOCALC_SD_IMAGE=sd.img build/host-pico_plus_2w/picocalc_sim /tmp/shots -
build/host-pico_plus_2w/sd_image_tool sd.img ls PICOCALC/GRAPH
fsck.fat -n sd.img
```

```bash
# Relay AI: test
cd relay && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python -m unittest discover -s tests
```

```bash
# Dimensioni per modulo dalla mappa del linker (v1.15.0)
python3 tools/map_sizes.py build/pico2w/picocalc_fxai_pico2w.elf.map --group
# Stack sul target: il firmware è compilato con -fstack-usage (file .su)
find build/pico_plus_2w/CMakeFiles/picocalc_fxai.dir/src -name '*.su' | xargs cat | sort -t$'\t' -k2 -n -r | head
```
