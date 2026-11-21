# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.14.0 — completamento totale di 3D Graph** (parità
con la fx-CG50, capitolo 16 del manuale; record del workspace v10), branch
**`main`**, tag `v1.14.0` creato dal workflow `.github/workflows/release.yml`.
Rapporti: `docs/reports/ANALISI_FONTI_v1.14.0.md` (fonti e tabella di
parità iniziale), `docs/reports/SVILUPPO_v1.14.0.md` (tabella finale, test,
memoria), guida `docs/GRAPH3D.md`; v1.13.1 (correzioni):
`docs/reports/ERRORI_v1.13.1.md`, `docs/reports/SVILUPPO_v1.13.1.md`;
v1.13.0 (Run-Matrix): `docs/reports/ANALISI_FONTI_v1.13.0.md`, `docs/reports/SVILUPPO_v1.13.0.md`;
v1.12.0 (EQUATION): `docs/reports/SVILUPPO_v1.12.0.md`, guida `docs/EQUATION.md`; v1.11.0: `docs/reports/SVILUPPO_v1.11.0.md`; v1.10.0:
`docs/reports/SVILUPPO_v1.10.0.md`; v1.9.0:
`docs/reports/CONSOLIDAMENTO_v1.9.0.md`.

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| **`main`** | **v1.14.0** (tag `v1.14.0`): v1.13.1 + 3D Graph completo (record v10) | si continua qui |
| tag `v1.13.1` (`62b3f8f`), `v1.13.0` (`a72d901`), `v1.12.0` (`51691de`), `v1.11.0` (`f6933fd`), `v1.10.0` (`dfdb08b`), `v1.9.0` (`bb2bdee`) | versioni precedenti su `main` | non spostare |
| `version/v1.5.4`, `version/v1.6.0`, `version/v1.7.0`, `version/v1.8.0` | snapshot storici, uno per versione | non modificare |

I branch di lavoro della v1.14.0 (`ccr-d10cca3d-ioq9ea`), della v1.13.1
(`claude/intelligent-galileo-xc0332`) e della v1.13.0
(`claude/run-matrix-completamento-tbi679`) hanno commit intermedi;
`main` riceve un solo commit per versione (fast-forward), quindi quei branch
possono essere eliminato dopo la pubblicazione, come i
duplicati della 1.9.0 elencati nel rapporto del consolidamento (l'ambiente
di sviluppo rifiuta l'eliminazione dei branch remoti e il push dei tag:
HTTP 403). Un solo branch per versione; mai force-push.

## 1. Stato attuale

**v1.14.0** (sopra la v1.13.1): **3D Graph** con ogni voce della tabella di
parità presente o documentata come differenza inevitabile
(`docs/reports/SVILUPPO_v1.14.0.md` §5–§6): modelli Line (EXPRESS, VECTOR,
P&V, POINTS), Plane (EXPRESS, VECTOR, POINTS), Sphere (FACTOR, EXPAND),
Cylinder, Cone con le condizioni «Invalid Setting»; Z=, Param (S, T), Rot X,
Rot Y; lista con SELECT/DELETE/TYPE/3D-GMEM/DRAW e FORMAT; V-Window con
Xgrid/Ygrid, S/T, θ/φ, INITIAL e 3D-VMEM; SET UP (Axes, Box, Label,
Plot/LineCol…); rotazione, ROTATE, ZOOM e viste, SKETCH, PICTURE, CAPTURE,
TRACE, G-Solve CROSS/INTSECT/RELATION; z-buffer condiviso in PSRAM, pittore
senza, taglio esatto al box, calcolo progressivo con AC. Nuovi moduli
`graph3d_model` (puro) e `graph3d` (disegno), controller in
`src/apps/graph3d_app.cpp` e `src/apps/graph3d_plot.cpp`. Record del
workspace **v10** (fixture nuova `workspace_v1_13_1_graph3d.inc` generata con i
sorgenti della v1.13.1). Numeri di test e firmware:
`docs/reports/SVILUPPO_v1.14.0.md` §4 e `docs/BUILD_STATUS.md`.
**Nessuna prova su hardware**: `docs/HARDWARE_TEST_v1.14.0.md` (HW-170…HW-189).

**v1.13.1** (sopra la v1.13.0): versione di correzione. Revisione completa
(test, ASan/UBSan bloccante, TSan, cppcheck, GCC con warning extra,
clang-tidy, `-fstack-usage`, fuzzing con `tools/fuzz`, simulatore con
confronto pixel per pixel, manuale) e 18 errori corretti (E01–E18 di
`docs/reports/ERRORI_v1.13.1.md`), ognuno con un gruppo di controlli in
`tests/fixes_v1131_test.cpp`. Formato del record invariato (v9). Problemi
aperti A1–A6 nello stesso documento. Numeri di test e firmware:
`docs/reports/SVILUPPO_v1.13.1.md` §3 e `docs/BUILD_STATUS.md`.
**Nessuna prova su hardware**: `docs/HARDWARE_TEST_v1.13.1.md` (HW-160…HW-169).

**v1.13.0** (sopra la v1.12.0): Run-Matrix con ogni voce della tabella di
parità presente (`docs/reports/SVILUPPO_v1.13.0.md` §5) o documentata come
differenza inevitabile (§6): List "nome", List File 1–6 e tutte le funzioni di
lista; ESYM P/E, ENG/ENG←; Simplify Manual con le regole del manuale; Base-N
con A–F in Hex, liste e matrici e i bit per base; editor MAT/VCT senza il
limite di 64 elementi; tutti i comandi grafici (anche nei `.pcgraph`); round
trip Math/Line con un corpus ampio; CALC, PROB, distribuzioni, CONVERT, TVM,
angoli, complessi, numeri casuali, variabili statistiche; stringhe e function
memory; `:` e ◢; CATALOG e CAPTURE; libreria pura `calc_library`; record del
workspace **v9** (fixture della v1.11.0 e della v1.12.0). Test host: 45 per profilo (12 081
controlli sulla Plus, 12 075 sul Pico 2 W), anche con ASan/UBSan e TSan; relay
15/15; firmware senza warning (immagine di 1 841 608 byte sulla Plus,
1 832 772 byte sul Pico 2 W; RAM statica 307 296 / 303 372 byte).
**Nessuna prova su hardware**: `docs/HARDWARE_TEST_v1.13.0.md` (HW-140…HW-159).


**v1.12.0** (sopra la v1.11.0): EQUATION con le schermate e i tasti del
manuale fx-CG50 (4-1…4-5), coefficienti come espressioni esatte e complesse
(editor `MathField`), SIML 2–6 con Gauss-Jordan esatto e formula delle
infinite soluzioni, POLY 2–6 con radici esatte, ordine del manuale, ×k e
Complex Mode, SOLVER su qualsiasi variabile con Lower/Upper e i messaggi del
manuale, VARS › SimRes/SimCoef/PlyRes/PlyCoef in Run-Matrix, SET UP che torna
alla modalità, correzione della frazione seguita da un fattore implicito
nell'editor Math; record del workspace **v8**. Numeri di test e firmware:
`docs/reports/SVILUPPO_v1.12.0.md` §6–7 e `docs/BUILD_STATUS.md`.
**Nessuna prova su hardware**: `docs/HARDWARE_TEST_v1.12.0.md` (HW-120…HW-134).

v1.11.0: List 1–26 e sottonomi, CSV, MatAns/ListAns, celle esatte e
complesse, Line con INS e ⌟, simboli ingegneristici, Simp, Base-N, comandi
grafici, record v7.

v1.10.0: matrici, vettori e liste come valori del parser, editor MAT/VCT,
Math/Line, CLIP, parentesi colorate, cursore sugli errori, record v6.

Stato della v1.9.0 (invariato dove non indicato):

- **Firmware** (Pico SDK 2.2.0, `-O2`, nessun warning), una cartella e un UF2
  per scheda:
  - Pimoroni Pico Plus 2 W: `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2`
    (immagine 1 386 216 byte su una partizione firmware di 4 MB, RAM statica
    299 444 byte, heap SRAM 192 076 byte, PSRAM 8 MB come pool separato);
  - Raspberry Pi Pico 2 W: `build/pico2w/picocalc_fxai_pico2w.uf2` (1 377 516
    byte su 2 MB, RAM statica 295 520 byte, heap 196 000 byte).
- **Test host**: 35 test CTest per profilo di scheda (7 705 / 7 699 controlli
  nelle suite C++, più lo stress dell'allocatore e `sd_interop_test`), 0
  errori, anche con ASan/UBSan e ThreadSanitizer. Relay AI: 15 test.
- **Nessun test su hardware fisico**, né con la Plus né con il Pico 2 W:
  procedure `docs/HARDWARE_TEST_v1.8.0.md` (HW-01…HW-36, da ripetere con la
  1.9.0) e `docs/HARDWARE_TEST_v1.9.0.md` (HW-40…HW-82); diagnostica in
  SYSTEM → F2 DIAG (righe `[DIAG]` anche sulla seriale USB).
- GRAPH: `docs/GRAPH_PARITY.md`; **parità completa con la fx-CG50 non
  raggiunta**. TABLE completa tranne Dual Screen (`docs/TABLE.md`).
- 3D Graph: completo dalla v1.14.0 (screenshot 40–43 e 88–104, `docs/GRAPH3D.md`).
- Memoria e flash della Plus: `docs/PICO_PLUS_2W.md`; formato del record:
  versione 10 (`docs/STORAGE.md`).

## 2. Cosa è stato fatto nell'ultima sessione (v1.14.0)

| Area | Contenuto |
|---|---|
| Git | partenza da `main` = `62b3f8f` (tag v1.13.1), nessun avanzamento di `main` durante il lavoro; commit intermedi sul branch `ccr-d10cca3d-ioq9ea`; un solo commit su `main` in fast-forward; tag e release dal workflow |
| Fonti | manuale software fx-CG50 v3.60 (PDF Casio), capitolo 16 letto con le schermate; analisi e tabella di parità in `ANALISI_FONTI_v1.14.0.md` prima del codice |
| Modello | `include/graph3d_model.hpp`, `src/graph3d_model.cpp`: tipi, moduli, colori, V-Window, Setup, validazione, geometria, INTSECT/RELATION, codifica |
| Disegno | `include/graph3d.hpp`, `src/graph3d.cpp` riscritti: reticoli di ogni tipo, taglio al box, proiezione θ/φ, z-buffer/pittore, sezioni |
| Modalità | `src/apps/graph3d_app.cpp`, `src/apps/graph3d_plot.cpp`: tutte le schermate del capitolo 16 |
| Workspace | record v10, conversione v5–v9; MEMORY con 3D G-Mem e 3D V-Win |
| Test | `graph3d_v114_test` (nuova), `graph3d_test` portata, `oom_test`, `architecture_test`, `limits_smoke`; fuzzing `fuzz_graph3d` (un errore trovato e corretto); simulatore 40–43 e 88–104 |

### Sessione precedente (v1.13.1)

| Area | Contenuto |
|---|---|
| Git | partenza da `main` = `51691de` (v1.12.0, l'ultima release era la v1.11.0); durante la ricerca `main` è avanzato alla v1.13.0 (`a72d901`, poi pubblicata): branch portato in avanti e ricerca ripetuta sulla v1.13.0; un solo commit su `main` in fast-forward; tag e release dal workflow |
| Correzioni | E01–E18 di `docs/reports/ERRORI_v1.13.1.md`, ognuno con un gruppo di controlli in `tests/fixes_v1131_test.cpp`; `tools/fuzz` (7 harness) |

### Sessione precedente (v1.13.0)

| Area | Contenuto |
|---|---|
| Git | partenza dal tag `v1.11.0` (`f6933fd`); prima della pubblicazione `main` era avanzato alla v1.12.0 di EQUATION (`51691de`): merge di `main` nel branch, versione rinumerata in 1.13.0 e record in v9, tutti i test rieseguiti; un solo commit su `main` in fast-forward; tag e release dal workflow |
| Motore | un solo parser, AST e valutatore: nuovi nodi per stringhe, unità, ∠, DMS, ʳ/ᵍ, `List "nome"`, function memory; `calc_library` (CONVERT, distribuzioni, PROB, TVM, nomi visualizzati, CATALOG) |
| Run-Matrix | `:` e ◢, stringhe Str 1–20, f1–f20, ENG/ENG←, Simplify Manual, Base-N completa, editor senza limite, menu OPTN/VARS dell'ordine fx-CG50, CATALOG, CAPTURE, messaggi d'errore della calcolatrice |
| GRAPH | disequazioni, G∫dx, Pict, Draw…, G-SelOn/Off, stili Sketch, PxlTest(; righe `c=` nei `.pcgraph` |
| Workspace | record v9 (List File, stringhe, function memory, variabili di modo, decimali delle celle); fixture generate con i sorgenti della v1.11.0 e della v1.13.0 |
| Architettura | sezione dati `Strings`, `ServiceResolver`, `architecture_test` più severo (1 225 controlli) |
| Test | `calc_v112_test`, `run_matrix_v112_test`, `graph_cmd_v112_test`, `editor_v112_test`; screenshot 77–87 compressi senza perdita |

## 3. Funzionalità ancora da completare

- Run-Matrix, EQUATION e 3D Graph: nessuna voce mancante (differenze inevitabili
  nel §6 di `docs/reports/SVILUPPO_v1.13.0.md` e di `docs/reports/SVILUPPO_v1.14.0.md`).
- Dual Screen (G+G, GtoT, T+G) e Graph to Table; FORMAT durante lo SKETCH.
- Modalità con solo modello dati: eActivity, Spreadsheet, Dyna Graph,
  Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot
  (i dati che Run-Matrix legge da VARS sono già nel workspace); Python;
  E-CON4 non realizzabile.

## 4. Prossimi passaggi esatti

0. **Recap e prompt delle fondamenta** (dopo la v1.14.0): stato e parità con la
   fx-CG50 in `docs/reports/RECAP_v1.14.0.md`; la prossima sessione di sviluppo
   (v1.15.0, fondamenta comuni per tutte le modalità mancanti) parte dal prompt
   in `docs/PROMPT_BASI_APPLICAZIONI.md` (§1), le modalità successive dai prompt
   del §2 dello stesso file. Il collaudo fisico del punto 1 resta prioritario
   appena la PicoCalc è disponibile.
1. **Collaudo fisico sulla PicoCalc con il Pimoroni Pico Plus 2 W**: HW-01…HW-36,
   HW-40…HW-82, HW-90…HW-99, HW-100…HW-115, HW-120…HW-134, HW-140…HW-159, HW-160…HW-169 e HW-170…HW-189 (log USB dall'accensione, righe `[psram]`,
   `[flash]`, `[mem]`, `[DIAG]`, `[fault]`, foto). Ogni KO va riprodotto con un test e corretto;
   solo le voci provate passano a TESTED ON HARDWARE (data, modulo, revisione
   della scheda).
2. Se disponibile, le stesse prove sul Pico 2 W (HW-48, HW-64, HW-73).
3. Poi le funzioni del §3 secondo `docs/ROADMAP.md`.

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
```

```bash
# Fuzzing leggero (v1.13.1): clang con libFuzzer (Ubuntu: libclang-rt-18-dev), ASan + UBSan
CC=clang CXX=clang++ cmake -S tools/fuzz -B build/fuzz -G Ninja -DFXAI_TEST_BOARD=PICO_PLUS_2W
cmake --build build/fuzz
tools/fuzz/run_fuzz.sh build/fuzz 300       # secondi per harness (8 harness, v1.14.0: fuzz_graph3d); crash in build/fuzz/artifacts
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
# Stack sul target: il firmware è compilato con -fstack-usage (file .su)
find build/pico_plus_2w/CMakeFiles/picocalc_fxai.dir/src -name '*.su' | xargs cat | sort -t$'\t' -k2 -n -r | head
```
