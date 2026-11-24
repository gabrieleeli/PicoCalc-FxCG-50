# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.19.0 — Geometry alla parità completa (capitolo 14)
sulla Pimoroni Pico Plus 2 W**, branch **`main`** (tag `v1.19.0` da creare con
il workflow `.github/workflows/release.yml`, azione «publish»).
Rapporti: `docs/reports/ANALISI_FONTI_v1.19.0.md` (tabella di parità del
capitolo 14 voce per voce, progetto del nucleo con il grafo delle dipendenze,
il risolutore dei blocchi e le tolleranze dichiarate, formato `.pcgeo` v2,
domande Q1–Q18 e risposte prima del codice), `docs/reports/SVILUPPO_v1.19.0.md`
(tabella finale, codice, errori trovati, test, firmware, cancello). Versioni
precedenti: `ANALISI_FONTI_v1.15.0…v1.18.0.md`, `SVILUPPO_v1.15.0…v1.18.0.md`.

> **Pico 2 W congelato alla v1.16.0.** Nessuna funzione, schermata o crescita
> dell'immagine sul Raspberry Pi Pico 2 W, salvo correzioni di errori
> condivise. Nella v1.18.0 l'immagine del Pico 2 W è **identica** a quella
> della v1.17.0 salvo i 4 byte della stringa della versione (1 944 228 B); nella
> v1.19.0 è ancora di 1 944 228 B, uguale alla v1.18.0 salvo la versione e la data.
> `architecture_test` fallisce se cambia l'insieme delle modalità aperte del
> Pico 2 W (23 modalità) o se una modalità Base vi compare. Le modalità nuove
> sono solo per la Pimoroni Pico Plus 2 W. Budget del Pico 2 W: immagine ≤ 2 064 384 B.

## 0. Piano (dalla v1.17.0; ordine aggiornato nella v1.18.0)

La v1.17.0 è la **base**: ogni modalità del registro si apre, fa il suo lavoro
principale, salva e rimette a posto lo stato ed esce senza perdite
(`apps_init_test`). Da qui **una modalità per versione arriva alla parità
completa** con la fx-CG50 e passa ad **Available** (stato riservato alla
parità completa); le modalità ancora incomplete sono **Base** (si aprono, con
il triangolo ambra sull'icona). Ogni versione ha il cancello completo (test su
entrambi i profili anche `--sanitize --tsan`, fuzzing ≥ 300 s per harness
nuovo o toccato, simulatore con gli screenshot vecchi identici, firmware delle
due schede senza warning e nei budget, relay 15/15, documentazione) e **un solo
commit su `main`** in fast-forward («PicoCalc FxCG-50 v<ver>: <contenuto>»).

| Versione | Contenuto | Stato |
|---|---|---|
| v1.15.0 | fondamenta comuni F1–F9 | **fatta** |
| v1.16.0 | eActivity su entrambe le schede; Pico 2 W congelato | **fatta** |
| v1.17.0 | base di tutte le modalità: stato Base, `apps_init_test`, record v12, dieci modalità Base sulla Plus | **fatta** |
| v1.18.0 | **Statistics alla parità completa (cap. 6)**, solo Plus; **Distribution** (cap. 18) come modalità Base | **fatta** |
| v1.19.0 | **Geometry alla parità completa (cap. 14)**, solo Plus | **fatta** |
| **v1.20.0** | **Program** (cap. 8 come modalità), solo Plus | **PROSSIMA** |
| v1.21.0 | Python (cap. 17) | proposta |
| v1.22.0… | Graph/Table (Dual Screen), Recursion, Dyna Graph, Conic Graphs, Financial, Spreadsheet, Picture Plot, Memory/System/Link, Distribution (v1.30.0), chiusura (v1.31.0) | proposta in `docs/ROADMAP.md`, da confermare a ogni versione |

Regole fisse: manuale fx-CG50 v3.60 solo come descrizione del comportamento
(niente codice, grafica o testi copiati da Casio); `ANALISI_FONTI_v<ver>.md`
con la tabella di parità **prima** del codice; un test per voce; una modalità è
Available solo con la tabella completa; nessun «TESTED ON HARDWARE» senza prova
fisica; HARDWARE_TEST con numerazione continua (ultima usata: **HW-295**).
Plus: immagine < 90 % della partizione da 4 MB (controllo bloccante).

### Decisioni prese

- **Program**: sottoinsiemi interpretati di C, C++ e C# (confermato
  nella v1.17.0); file `.pcprog` con `// pcprog: <dialetto>` nella prima riga,
  `.c/.cpp/.cs` apribili.
- **Python**: MicroPython vero (porta `embed` v1.24.1, MIT) in
  `third_party/micropython`, solo Plus; coroutine con pila propria (ucontext
  sul PC, cambio di contesto Cortex-M33 con MSPLIM sul dispositivo), heap
  separato 512 KB PSRAM / 48 KB SRAM, AC → KeyboardInterrupt.
- **Distribution**: Base dalla v1.18.0 (MENU › O, 24ª icona solo sulla Plus,
  `AppId::Distribution` in coda all'enum solo sulla Plus), sezione `0x0117`,
  nessun tipo di file; nucleo `dist_core` condiviso con il DIST di Statistics.
- **Statistics (Plus)**: sezione `0x0101` (StatGraph, SET, SET UP, colori e
  formule delle liste, risultati di VARS › STAT); DefG = ripristino dello
  StatGraph; quartili OnData; Sin in radianti; ANOVA a due vie bilanciata;
  valori verificati con `tools/stat_reference.py` (scipy, numpy, statsmodels).
  parità di Statistics (risposta Q1 della v1.17.0).
- **Record v12 solo sulla Plus** (firma e CRC per sezione, sezione danneggiata
  isolata); il Pico 2 W scrive ancora la v11. Limite noto: un firmware ≤ v1.16.0
  non legge un record v12 (`docs/STORAGE.md`).
- Link: protocollo proprio a trame con CRC-32 su USB CDC
  (`Platform::serialPort`), pacchetti `.pclink`, `tools/picocalc_link.py`.
- Espressioni Math nelle righe di testo di eActivity: testo lineare tra
  U+F8F0 e U+F8F1 (U+E000–E002 sono simboli statistici del font).
- Pico 2 W: `-Os` per eActivity e per i sorgenti fuori dai percorsi di calcolo
  e disegno (`FXAI_PICO2W_SIZE_SOURCES`); la Plus resta a `-O2`.
- **Geometry (Plus, v1.19.0)**: nucleo puro in cinque file (`src/geometry.cpp`
  grafo e ricalcolo, `geometry_build.cpp`, `geometry_measure.cpp` misure e
  risolutore, `geometry_anim.cpp`, `geometry_file.cpp`) con l'interno in
  `src/geometry_internal.hpp`; controller `src/apps/geometry_app.cpp` e
  `geometry_view.cpp` con lo stato in `src/apps/geometry_ui.hpp`. Oggetti con id
  crescenti = ordine topologico; costruzioni e immagini sono **definizioni**
  (restano legate); i blocchi sono equazioni risolte con Gauss-Newton a norma
  minima (`SolveTask`, step::Task con AC); Clr Constraint toglie blocchi e
  vincoli di Draw Spec, non le costruzioni (D5). Casella di misura con OPTN
  (richiesta) e con EXIT a riposo (fx-CG50); il menu Option della fx-CG50 è
  nelle schede Option e Properties di F2. Undo a un livello che si alterna.
  Sezione `0x0110` e `.pcgeo` **versione 2** (la 1 della v1.17.0 si migra).
  Limiti: 1 000 oggetti con la PSRAM, 250 senza.

## 1. Stato attuale

**v1.19.0** (sopra la v1.18.0): sulla **Pimoroni Pico Plus 2 W** Geometry è
alla **parità completa** con il capitolo 14 (Draw e Draw Spec, Construct e
Transform legati ai genitori, casella di misura con blocchi, testo, etichette,
espressioni, FORMAT, Undo/Redo, View, animazioni con traccia e tabella verso
List e Statistics, `.pcgeo` v2; guida `docs/GEOMETRY.md`) ed è **Available**.
Modalità con la parità: Run-Matrix, EQUATION, 3D Graph, eActivity, Statistics
(Plus), Geometry (Plus). Modalità Base (solo Plus): Spreadsheet, Dyna Graph,
Recursion, Conic Graphs, Program, Financial, Link, Picture Plot, Python,
Distribution; cosa manca è nel campo `missing` del registro
(`src/app_registry.cpp`) e in `docs/FEATURE_MATRIX.md`. Numeri in
`docs/reports/SVILUPPO_v1.19.0.md` e `docs/BUILD_STATUS.md`. **Nessuna prova su
hardware**: `docs/HARDWARE_TEST_v1.19.0.md` (HW-272…HW-295).

## 2. Prossima sessione: v1.20.0 = Program alla parità completa (solo Plus)

1. `git fetch origin main`; branch di sessione da `main`.
2. `docs/reports/ANALISI_FONTI_v1.20.0.md` con la tabella di parità del
   capitolo 8 come modalità (elenco, EDIT, NEW, DEL, DEL-ALL, SEARCH, RENAME,
   password, DEBUG, comandi di I/O e grafici, libreria `fx`) **prima del codice**,
   nei dialetti C, C++ e C# già decisi; una voce = un test.
3. Codice solo sulla Plus: nucleo `program_mode.hpp/.cpp` e interprete F9
   (`program.hpp`, `src/program_*.cpp`), controller `src/apps/program_app.cpp`,
   sezione `0x010A`, file `.pcprog`; esecuzione e debug in `step::Task` con AC.
4. Alla fine Program passa da Base ad Available; `architecture_test` aggiornato.
5. Poi v1.21.0 Python (ROADMAP).

## 3. Cosa è stato fatto nelle ultime sessioni (v1.15.0, v1.16.0, v1.17.0)

| Area | Contenuto |
|---|---|
| Git (v1.19.0) | partenza da `main` = `a7c861b` (v1.18.0); commit intermedi su `ccr-6804dfc4-cwp8zw`; un solo commit su `main` in fast-forward |
| v1.19.0 | `ANALISI_FONTI_v1.19.0.md` (tabella del cap. 14, Q1–Q18, tolleranze); nucleo puro `src/geometry*.cpp` (grafo, ricalcolo incrementale, intersezioni, costruzioni, trasformazioni, misure, `SolveTask`, `AnimationTask`, `.pcgeo` v2 e migrazione v1 con la fixture `tests/fixtures/geometry_v1_17_0.inc`); controller `geometry_app.cpp`/`geometry_view.cpp`; test `geometry_test` (una voce un caso, proprietà G01–G14), `geometry_flow_test` (tasti veri); `fuzz_geometry` (formato e sequenze di comandi; un errore trovato e corretto); screenshot 175–202 (144 e 172 rigenerati) |
| Git (v1.18.0) | partenza da `main` = `aa2923c` (v1.17.0); commit intermedi su `claude/eager-wozniak-bg3tqx`; un solo commit su `main` in fast-forward |
| v1.18.0 | `ANALISI_FONTI_v1.18.0.md` (tabella del cap. 6, Q1–Q16); nucleo `stat_engine` e `dist_core` contro scipy/numpy/statsmodels (`tools/stat_reference.py`, `tests/fixtures/stat_reference.inc`); controller `statistics_plus_app.cpp`, `stat_plot.cpp`; Distribution (`distribution.cpp`, `distribution_app.cpp`); VARS › STAT e x̂/ŷ in Run-Matrix; test `statistics_test` esteso, `statistics_flow_test` (una voce un caso), casi di Distribution in `apps_init_test`, `base_modes_test`, `base_flow_test`; `fuzz_stats` (tre errori trovati e corretti); screenshot 148–174 |
| Git (v1.17.0) | partenza da `main` = `eb2338a` (v1.16.0); commit intermedi su `claude/eager-lamport-9fht7w`; un solo commit su `main` in fast-forward; tag e release dal workflow |
| v1.17.0 fase 0 | `ANALISI_FONTI_v1.17.0.md`; risposte: Distribution nella v1.18.0, Program C/C++/C#, MicroPython vero, record v12 solo Plus |
| v1.17.0 fase 1 | `tests/apps_init_test.cpp` (casi C1–C7, controlli K1–K6, due profili, `mem_new` per le perdite); errore trovato e corretto: Conversion non segnava Ans da salvare |
| v1.17.0 fase 2 | `AppStatus::Base`, segno nel launcher; record v12 (`src/workspace.cpp`, `src/workspace_sections.cpp`); nuclei `src/{spreadsheet,dyna,recursion,conic,financial,program_mode,link_protocol,geometry,picture_plot,python_port}.cpp` e controller `src/apps/*_app.cpp`; `mode_codec.hpp`; `TypedFileMenu`; MicroPython in `third_party/micropython`; `tools/picocalc_link.py`; test `base_modes_test`, `base_flow_test`, `workspace_v117_test`, `link_tool_test`; fuzzer `fuzz_sheet`, `fuzz_link`, `fuzz_modes`, `fuzz_python`; screenshot 128–147 |
| v1.16.0 | eActivity: `include/eactivity.hpp`, `src/eactivity.cpp`, `src/apps/eactivity_app.cpp`; parti del workspace e sezione `0x0102`; sessione da striscia; `eactivity_test` (E01–E56), `fuzz_eactivity`, screenshot 108–127; `-Os` sul Pico 2 W; congelamento in `architecture_test` |
| v1.15.0 | F1–F9; test `workspace_v115_test`, `typed_files_test`, `mode_shell_test`, `text_editor_test`, `step_task_test`, `graph2d_test`, `binding_test`, `program_core_test`; fuzzing `fuzz_typedfile`, `fuzz_texteditor`, `fuzz_program`; screenshot 105–107 |

## 4. Collaudo fisico

Resta prioritario appena la PicoCalc è disponibile: HW-01…HW-295 (log USB
dall'accensione, righe `[psram]`, `[flash]`, `[mem]`, `[DIAG]`, `[fault]`,
foto). Per la v1.17.0 in particolare: la coroutine di Python sul Cortex-M33
(cambio di contesto, MSPLIM, KeyboardInterrupt, RecursionError senza
`[fault]`), Link sulla USB CDC con `tools/picocalc_link.py`, l'aggiornamento
del record v11 → v12 e il ritorno alla v1.16.0. Ogni KO va riprodotto con un
test e corretto; solo le voci provate passano a TESTED ON HARDWARE (data,
modulo, revisione della scheda).

## 5. Problemi noti / da verificare

- v1.19.0: tempi del risolutore dei blocchi e del ridisegno con 1 000 oggetti
  sul Cortex-M33 (HW-284, HW-293); fluidità delle animazioni, che vanno a un
  fotogramma per ciclo del firmware senza cadenza a tempo (HW-291); leggibilità
  dei tratti Thin/Dot e delle aree Lighter sul pannello (HW-287); il ritorno
  alla v1.18.0 isola la sezione `0x0110` v2 (salvare prima la costruzione in un
  file, `docs/STORAGE.md`).

- v1.18.0: tempi di SinReg/LogisticReg e dell'ANOVA con 999 dati sul Cortex-M33
  (sul PC pochi decimi di secondo; HW-259, HW-263); colori dei marcatori e
  delle aree sul pannello reale; Sin e Logistic con un metodo iterativo proprio
  (su dati mal condizionati la calcolatrice può dare un altro minimo locale);
  la striscia «Stat Graph» di eActivity porta le liste e non gli StatGraph.

- v1.17.0: la pila della coroutine di Python (24 KB, margine 4 KB) e il cambio
  di contesto Cortex-M33 sono provati solo sul PC (ucontext con ASan);
  Link solo con `tests/link_peer` sul PC (nessuna USB CDC reale);
  un firmware ≤ v1.16.0 non legge il record v12 della Plus.
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
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/base_v117.txt        # modalità Base v1.17.0 (128-147)
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/statistics_v118.txt  # Statistics e Distribution v1.18.0 (148-174)
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/geometry_v119.txt    # Geometry v1.19.0 (175-202)
```

```bash
# Riferimenti numerici di Statistics e Distribution (v1.18.0): scipy, numpy, statsmodels
pip install scipy numpy statsmodels pandas
python3 tools/stat_reference.py --check      # fixture identica ai riferimenti rigenerati
```

```bash
# Link con il PC (v1.17.0): prove di andata e ritorno e uso con la PicoCalc
python3 tools/test_picocalc_link.py build/host-pico_plus_2w/link_peer
python3 tools/picocalc_link.py receive --port /dev/ttyACM0 -o ws.pclink
python3 tools/picocalc_link.py send --port /dev/ttyACM0 HI.py CONTI.pcsheet
python3 tools/picocalc_link.py list ws.pclink
```

```bash
# MicroPython (v1.17.0): rigenerare third_party/micropython/micropython_embed
# dopo ogni modifica di mpconfigport.h o dei moduli (genhdr dipende dalla configurazione)
git clone --depth 1 --branch v1.24.1 https://github.com/micropython/micropython.git /tmp/micropython
make -C third_party/micropython MICROPYTHON_TOP=/tmp/micropython
```

```bash
# Fuzzing leggero (v1.13.1): clang con libFuzzer (Ubuntu: apt-get install libclang-rt-18-dev), ASan + UBSan
CC=clang CXX=clang++ cmake -S tools/fuzz -B build/fuzz -G Ninja -DFXAI_TEST_BOARD=PICO_PLUS_2W
cmake --build build/fuzz
tools/fuzz/run_fuzz.sh build/fuzz 300       # secondi per harness (18 harness dalla v1.19.0); crash in build/fuzz/artifacts
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
