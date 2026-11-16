# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.12.0 — completamento di EQUATION** (SIML, POLY e
SOLVER come il capitolo 4 del manuale fx-CG50, coefficienti esatti e
complessi, Infinitely Many Solutions con la formula, ×k, Complex Mode,
Lower/Upper, VARS › EQUATION, record v8), branch **`main`**, tag `v1.12.0`
creato dal workflow `.github/workflows/release.yml`. Rapporti:
`docs/reports/ANALISI_FONTI_v1.12.0.md` (tabella di parità),
`docs/reports/SVILUPPO_v1.12.0.md`; guida `docs/EQUATION.md`; v1.11.0:
`docs/reports/SVILUPPO_v1.11.0.md`.

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| **`main`** | **v1.12.0** (tag `v1.12.0`): v1.11.0 + completamento di EQUATION | si continua qui |
| tag `v1.11.0` (`f6933fd`), `v1.10.0` (`dfdb08b`), `v1.9.0` (`bb2bdee`) | versioni precedenti su `main` | non spostare |
| `version/v1.5.4`, `version/v1.6.0`, `version/v1.7.0`, `version/v1.8.0` | snapshot storici, uno per versione | non modificare |

Il branch di lavoro della v1.12.0 (`ccr-bbf29b2b-0ipbnx`) ha commit
intermedi; `main` riceve un solo commit con tutta la v1.12.0 (fast-forward),
quindi quel branch può essere eliminato dopo la pubblicazione, come i
duplicati della 1.9.0 elencati nel rapporto del consolidamento (l'ambiente
di sviluppo rifiuta l'eliminazione dei branch remoti e il push dei tag:
HTTP 403). Un solo branch per versione; mai force-push.

## 1. Stato attuale

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
- 3D Graph: superfici z = f(x, y) Z1–Z3 (screenshot 40–43).
- Memoria e flash della Plus: `docs/PICO_PLUS_2W.md`; formato del record:
  versione 5 (`docs/STORAGE.md`).

## 2. Cosa è stato fatto nell'ultima sessione (v1.12.0)

| Area | Contenuto |
|---|---|
| Git | partenza verificata dal tag `v1.11.0` (`f6933fd` = `origin/main`); sviluppo sul branch di sessione; `main` ricontrollato prima della pubblicazione; un solo commit su `main` in fast-forward; tag e release dal workflow |
| Fonti | manuale ufficiale fx-CG50 v3.60 (PDF) letto anche come immagini; tabella di parità in `ANALISI_FONTI_v1.12.0.md` |
| Motore | `equation.hpp/.cpp`: `eq::Number`/`ExactComplex`, `solveSimultaneous`, `solvePolynomial`, `solveFor`, visualizzazione; `SimRes`… nel lexer e in `CollectionResolver` |
| Interfaccia | `EquationApp` riscritta, widget `MathField`, VARS › EQUATION in Run-Matrix, SET UP con ritorno |
| Workspace | record v8; fixture della v1.11.0 per la migrazione |
| Test | `equation_v112_test`, `architecture_test` più severo; screenshot 08 e 62–76 |

## 3. Funzionalità ancora da completare

- Dual Screen (G+G, GtoT, T+G) e Graph to Table; FORMAT durante lo SKETCH;
  suffissi angolari °/ʳ/ᵍ.
- Run-Matrix (dopo la v1.11.0): List File 1–6 e `List "nome"` nei calcoli;
  comandi grafici dei programmi non realizzati (`PxlTest(`, `DrawStat`,
  `StoPict`/`RclPict`, stili, disequazioni, `G∫dx`); simboli P/E e ►ENG;
  regola completa di Simplify Manual; variabili A–F in Hex.
- 3D Graph: sfere, piani e rette parametriche della fx-CG50.
- Modalità con solo modello dati: eActivity, Spreadsheet, Dyna Graph,
  Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot;
  Python; E-CON4 non realizzabile.

## 4. Prossimi passaggi esatti

1. **Collaudo fisico sulla PicoCalc con il Pimoroni Pico Plus 2 W**: HW-01…HW-36,
   HW-40…HW-82, HW-90…HW-99, HW-100…HW-115 e HW-120…HW-134 (log USB dall'accensione, righe `[psram]`,
   `[flash]`, `[mem]`, `[DIAG]`, `[fault]`, foto). Ogni KO va riprodotto con un test e corretto;
   solo le voci provate passano a TESTED ON HARDWARE (data, modulo, revisione
   della scheda).
2. Se disponibile, le stesse prove sul Pico 2 W (HW-48, HW-64, HW-73).
3. Poi le funzioni del §3 secondo `docs/ROADMAP.md`.

## 5. Problemi noti / da verificare

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
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/equation.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d.txt
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
