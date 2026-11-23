# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.18.0 — Graph e Table alla parità completa**,
branch di sessione `claude/graph-table-parity-v118-1is1v6`, un solo commit
«PicoCalc FxCG-50 v1.18.0: …». Rapporti: `docs/reports/ANALISI_FONTI_v1.18.0.md`
(audit sul manuale v3.60, 114 voci, errori E1–E6, progetto del Dual Screen,
decisioni D1–D4 con le risposte), `docs/reports/SVILUPPO_v1.18.0.md` (parità
finale, differenze, test, firmware, cancello). Versioni precedenti:
`ANALISI_FONTI_v1.17.0.md`, `SVILUPPO_v1.17.0.md` e prima.

> **Pico 2 W congelato** (alla v1.16.0 per le modalità, alla v1.17.0 per i
> file). Nessuna funzione, schermata o crescita dell'immagine sul Raspberry Pi
> Pico 2 W, salvo correzioni di errori condivise. **Unica eccezione: Graph e
> Table nella v1.18.0** (parità completa con lo stesso codice della Plus, nel
> servizio GraphCore). `architecture_test` fallisce se cambia l'insieme delle
> modalità aperte del Pico 2 W, se una modalità Base vi compare, o se cambia un
> file del manifesto `tests/fixtures/pico2w_frozen_v1_17_0.txt` (impronte dei
> file del Pico 2 W alla v1.17.0) fuori dall'elenco esplicito
> `kGraphTableV118`; un sorgente nuovo del Pico 2 W deve stare in quell'elenco.
> Una correzione condivisa futura aggiorna la riga del file nel manifesto
> (`tools/freeze_manifest.py`) con il motivo nel rapporto. Budget del Pico 2 W:
> immagine ≤ **2 015 232 B** (2 064 384 B meno 48 KB riservati alle
> correzioni; margine di 81 920 B, controllo bloccante); v1.18.0: 1 972 608 B.

## 0. Piano (dalla v1.17.0)

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
| v1.18.0 | **Graph e Table alla parità completa** su entrambe le schede (Dual Screen G+G/GtoT/T+G, FORMAT nello SKETCH, immagini a due livelli, cartelle, LISTMEM a 999, E1–E6); eccezione al congelamento del Pico 2 W solo per Graph/Table | **fatta** |
| **v1.19.0** | **Statistics alla parità completa (cap. 6)**, solo Plus; **Distribution** (cap. 18) entra come modalità Base (spostate dalla v1.18.0) | **PROSSIMA** |
| v1.20.0… | una modalità per versione: Recursion, Dyna Graph, Conic Graphs, Financial, Spreadsheet, Program, Picture Plot, Geometry, Memory/System/Link, Python, Distribution, chiusura | proposta in `docs/ROADMAP.md`, da confermare a ogni versione |

Regole fisse: manuale fx-CG50 v3.60 solo come descrizione del comportamento
(niente codice, grafica o testi copiati da Casio); `ANALISI_FONTI_v<ver>.md`
con la tabella di parità **prima** del codice; un test per voce; una modalità è
Available solo con la tabella completa; nessun «TESTED ON HARDWARE» senza prova
fisica; HARDWARE_TEST con numerazione continua (ultima usata: **HW-265**).
Plus: immagine < 90 % della partizione da 4 MB (controllo bloccante); Pico 2 W:
immagine ≤ 2 015 232 B (controllo bloccante). Screenshot esistenti identici
(ultimo usato: **160**).

### Decisioni prese

- **Program**: sottoinsiemi interpretati di C, C++ e C# (confermato
  nella v1.17.0); file `.pcprog` con `// pcprog: <dialetto>` nella prima riga,
  `.c/.cpp/.cs` apribili.
- **Python**: MicroPython vero (porta `embed` v1.24.1, MIT) in
  `third_party/micropython`, solo Plus; coroutine con pila propria (ucontext
  sul PC, cambio di contesto Cortex-M33 con MSPLIM sul dispositivo), heap
  separato 512 KB PSRAM / 48 KB SRAM, AC → KeyboardInterrupt.
- **Distribution**: entra nel registro nella **v1.19.0** come Base, insieme alla
  parità di Statistics (risposta Q1 della v1.17.0, spostata dalla v1.18.0).
- **Dual Screen** (v1.18.0, D1–D4): principale sopra e sotto-schermo/tabella
  sotto (320×140); in G+G il principale è sempre attivo e lo ZOOM scrive la
  V-Window del sotto-schermo; immagini `.pcpict` v2 a due livelli; liste da 999
  elementi anche sul Pico 2 W con il totale invariato (1 530).
- **Eccezione al congelamento** del Pico 2 W per Graph/Table della v1.18.0:
  codice nuovo nel servizio GraphCore (`graph_view.cpp`, `graph_dual.cpp`),
  niente codice per scheda; `-Os` sul Pico 2 W per `graph_dual.cpp`.
- **Record v12 solo sulla Plus** (firma e CRC per sezione, sezione danneggiata
  isolata); il Pico 2 W scrive ancora la v11. Limite noto: un firmware ≤ v1.16.0
  non legge un record v12 (`docs/STORAGE.md`).
- Link: protocollo proprio a trame con CRC-32 su USB CDC
  (`Platform::serialPort`), pacchetti `.pclink`, `tools/picocalc_link.py`.
- Espressioni Math nelle righe di testo di eActivity: testo lineare tra
  U+F8F0 e U+F8F1 (U+E000–E002 sono simboli statistici del font).
- Pico 2 W: `-Os` per eActivity e per i sorgenti fuori dai percorsi di calcolo
  e disegno (`FXAI_PICO2W_SIZE_SOURCES`); la Plus resta a `-O2`.

## 1. Stato attuale

**v1.18.0** (sopra la v1.17.0): Graph e Table hanno la **parità completa** con
la fx-CG50 su entrambe le schede (`docs/GRAPH_PARITY.md`, 114 voci; differenze
DI-1…DI-9). Codice nuovo: `src/apps/graph_view.cpp` (riquadri, cornice, curve,
tabella), `src/apps/graph_dual.cpp` (Dual Screen), curve per riquadro in
`graph_plot.cpp`, T+G in `table_app.cpp`, sezioni `0x0104`/`0x0106`, parte Dual
delle strisce, `.pcpict` v2, cartelle delle immagini. Test nuovi
`graph_v118_test`, `table_v118_test`, `workspace_v118_test`, `fuzz_dual`;
screenshot 148–160. **Nessuna prova su hardware**:
`docs/HARDWARE_TEST_v1.18.0.md` (HW-246…HW-265).

**v1.17.0** (sopra la v1.16.0): tutte le modalità del registro si aprono tranne
E-CON4 (External). Sulla **Pimoroni Pico Plus 2 W** dieci modalità **Base**:
Spreadsheet, Dyna Graph, Recursion, Conic Graphs, Program, Financial, Link,
Geometry, Picture Plot, Python; ognuna su `ModeShell` con il suo nucleo puro
(servizio ModeCore), la sua sezione `0x0100 + AppId` del record v12, il suo
tipo di file in MEMORY › FILES, le operazioni lunghe in `step::Task` con AC; le
strisce di eActivity delle modalità della Plus sono disponibili. Cosa manca a
ogni modalità per la parità: campo `missing` del registro
(`src/app_registry.cpp`) e `docs/FEATURE_MATRIX.md`. Guide:
`docs/SPREADSHEET.md`, `DYNA_GRAPH.md`, `RECURSION.md`, `CONIC_GRAPHS.md`,
`FINANCIAL.md`, `PROGRAM.md`, `LINK.md`, `GEOMETRY.md`, `PICTURE_PLOT.md`,
`PYTHON.md`. Numeri in `docs/reports/SVILUPPO_v1.17.0.md` e
`docs/BUILD_STATUS.md`. **Nessuna prova su hardware**:
`docs/HARDWARE_TEST_v1.17.0.md` (HW-220…HW-245).

## 2. Prossima sessione: v1.19.0 = Statistics alla parità completa (solo Plus)

1. `git fetch origin main`; branch di sessione da `main`.
2. `docs/reports/ANALISI_FONTI_v1.19.0.md` con la tabella di parità del
   capitolo 6 (Statistics) **prima del codice**: editor delle liste completo
   (pagine dei menu, SORT, DEL, DEL-ALL, INS, ADD, sottonomi, CSV), tutti i
   grafici (Scatter, xyLine, NPP, istogramma, box plot, normale, barre, torta,
   regressioni con tutti i tipi incluse sinusoidale e logistica), CALC 1/2-Var
   e regressioni con i residui, TEST (Z, t, χ², F, 2 campioni, ANOVA a una e
   due vie), INTR, DIST complete; più il progetto di **Distribution**
   (cap. 18) come modalità Base (registro, sezione, tipo di file se serve).
3. Codice delle funzioni nuove **solo sulla Plus** (`#if FXAI_BOARD_PICO_PLUS_2W`
   con il codice della v1.16.0 nell'`#else`, come nella v1.17.0): Statistics
   resta Available sul Pico 2 W con le funzioni della v1.16.0 e i file del
   manifesto del congelamento invariati (l'eccezione `kGraphTableV118` vale
   solo per la v1.18.0; immagine ≤ 2 015 232 B).
4. Statistics sulla Plus resta Available (lo è dalla v1.8.0) ma deve avere la
   tabella di parità completa alla fine della v1.19.0; `architecture_test`
   controlla che le modalità della v1.16.0 restino Available sulla Plus.
5. `apps_init_test` copre da solo Distribution appena è nel registro come
   Base; aggiungere i suoi casi a `base_modes_test`/`base_flow_test`.

## 3. Cosa è stato fatto nelle ultime sessioni (v1.15.0, v1.16.0, v1.17.0)

| Area | Contenuto |
|---|---|
| v1.18.0 | fase 0 `ANALISI_FONTI_v1.18.0.md` (audit sul manuale v3.60, risposte D1–D4); `graph_view.cpp`, `graph_dual.cpp`, Dual Screen G+G/GtoT/T+G, voci piccole ed errori E1–E6, persistenza, `.pcpict` v2, cartelle, liste da 999; test `graph_v118_test`, `table_v118_test`, `workspace_v118_test`, manifesto del congelamento in `architecture_test`; `fuzz_dual`; screenshot 148–160 |
| Git (v1.17.0) | partenza da `main` = `eb2338a` (v1.16.0); commit intermedi su `claude/eager-lamport-9fht7w`; un solo commit su `main` in fast-forward; tag e release dal workflow |
| v1.17.0 fase 0 | `ANALISI_FONTI_v1.17.0.md`; risposte: Distribution nella v1.18.0, Program C/C++/C#, MicroPython vero, record v12 solo Plus |
| v1.17.0 fase 1 | `tests/apps_init_test.cpp` (casi C1–C7, controlli K1–K6, due profili, `mem_new` per le perdite); errore trovato e corretto: Conversion non segnava Ans da salvare |
| v1.17.0 fase 2 | `AppStatus::Base`, segno nel launcher; record v12 (`src/workspace.cpp`, `src/workspace_sections.cpp`); nuclei `src/{spreadsheet,dyna,recursion,conic,financial,program_mode,link_protocol,geometry,picture_plot,python_port}.cpp` e controller `src/apps/*_app.cpp`; `mode_codec.hpp`; `TypedFileMenu`; MicroPython in `third_party/micropython`; `tools/picocalc_link.py`; test `base_modes_test`, `base_flow_test`, `workspace_v117_test`, `link_tool_test`; fuzzer `fuzz_sheet`, `fuzz_link`, `fuzz_modes`, `fuzz_python`; screenshot 128–147 |
| v1.16.0 | eActivity: `include/eactivity.hpp`, `src/eactivity.cpp`, `src/apps/eactivity_app.cpp`; parti del workspace e sezione `0x0102`; sessione da striscia; `eactivity_test` (E01–E56), `fuzz_eactivity`, screenshot 108–127; `-Os` sul Pico 2 W; congelamento in `architecture_test` |
| v1.15.0 | F1–F9; test `workspace_v115_test`, `typed_files_test`, `mode_shell_test`, `text_editor_test`, `step_task_test`, `graph2d_test`, `binding_test`, `program_core_test`; fuzzing `fuzz_typedfile`, `fuzz_texteditor`, `fuzz_program`; screenshot 105–107 |

## 4. Collaudo fisico

Resta prioritario appena la PicoCalc è disponibile: HW-01…HW-265 (log USB
dall'accensione, righe `[psram]`, `[flash]`, `[mem]`, `[DIAG]`, `[fault]`,
foto). Per la v1.17.0 in particolare: la coroutine di Python sul Cortex-M33
(cambio di contesto, MSPLIM, KeyboardInterrupt, RecursionError senza
`[fault]`), Link sulla USB CDC con `tools/picocalc_link.py`, l'aggiornamento
del record v11 → v12 e il ritorno alla v1.16.0. Ogni KO va riprodotto con un
test e corretto; solo le voci provate passano a TESTED ON HARDWARE (data,
modulo, revisione della scheda).

## 5. Problemi noti / da verificare

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
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph_v118.txt       # Graph e Table v1.18.0 (148-160)
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
# Fuzzing leggero (v1.13.1): clang con libFuzzer (Ubuntu: libclang-rt-18-dev), ASan + UBSan
CC=clang CXX=clang++ cmake -S tools/fuzz -B build/fuzz -G Ninja -DFXAI_TEST_BOARD=PICO_PLUS_2W
cmake --build build/fuzz
tools/fuzz/run_fuzz.sh build/fuzz 300       # secondi per harness (17 harness dalla v1.18.0, con fuzz_dual); crash in build/fuzz/artifacts
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

```bash
# Congelamento del Pico 2 W (v1.18.0): rigenerare il manifesto dopo una correzione
# condivisa approvata (scriverne il motivo nel rapporto della versione)
python3 tools/freeze_manifest.py . build/host-pico2w/build.ninja <commit> tests/fixtures/pico2w_frozen_v1_17_0.txt
```
