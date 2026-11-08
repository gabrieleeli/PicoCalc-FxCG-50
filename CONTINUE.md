# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.8.0 — Sessione GRAPH** (motore dei grafici, modalità
GRAPH, progetti e microSD), branch **`version/v1.8.0`**.

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| `main` | codice originale caricato | non modificare |
| `version/v1.5.4`, `version/v1.6.0` | release storiche | non modificare |
| `version/v1.7.0` | Sessione 1 (audit, architettura, Run-Matrix), base della v1.8.0 (`72285e9`) | non modificare |
| **`version/v1.8.0`** | lavoro della v1.8.0 | si continua qui finché l'utente non approva la release |
| `claude/loving-meitner-jifxhg` | branch di sviluppo della sessione GRAPH, allineato a `version/v1.8.0` | storico |

Non creare `version/v1.9.0` o successive finché i rispettivi incrementi non sono
completati e verificati. Nessun tag/release v1.8.0 senza richiesta. Mai
force-push.

## 1. Stato attuale

- **Firmware RP2350 compilabile**: `build/pico2w/picocalc_fxai.uf2` (2 874 880
  byte, ≈1403 KB di immagine flash, RAM statica ≈286 KB, heap ≈194 KB, stack
  32 KB), nessun warning nei sorgenti del progetto; `picotool info`: versione
  1.8.0, `pico2_w`, SDK 2.2.0.
- **Test host**: 22 suite CTest, 6540 controlli, 0 errori, anche con ASan/UBSan
  (dettaglio in `docs/TESTING.md`). Relay AI: 15 test Python superati.
- **Nessun test su hardware fisico** (né display né microSD).
- GRAPH: matrice di parità voce per voce in `docs/GRAPH_PARITY.md`
  (132 voci: 26 TESTED ON SIMULATOR, 87 TESTED ON HOST, 3 IMPLEMENTED,
  3 parziali, 10 NOT STARTED, 3 fuori ambito). **Parità completa non
  raggiunta.**
- Simulatore: screenshot GRAPH/TABLE 06, 07, 20–34 (`tests/sim/graph.txt`) e
  Run-Matrix 12–19 (`tests/sim/run_matrix.txt`) in `docs/screenshots/`.

## 2. Cosa è stato fatto nella Sessione GRAPH

| Area | Contenuto |
|---|---|
| Motore | `graph_model`, `graph_engine` (viewport, clipping, compilazione della lista, riferimenti tra funzioni, campionamento adattivo incrementale, ZOOM), `graph_solve` (Brent, Ridders, Gauss–Kronrod) — `docs/GRAPH_ENGINE.md` |
| Modalità GRAPH | riscritta in `graph_app.cpp`, `graph_plot.cpp`, `graph_files.cpp`: 20 funzioni e 12 tipi, V-Window, Setup proprio, TRACE, ZOOM, SKETCH, G-SOLVE, Modify, memorie, PICT, sfondo, progetti |
| TABLE | sullo stesso motore: dY/dX, GPH-CON/GPH-PLT |
| File | `file_store` (interfaccia, scrittura atomica, recupero, scheda in RAM), `graph_file` (`.pcgraph`, `PCGMEM`), `pict` (`.pcpict`, BMP) — `docs/PCGRAPH_FORMAT.md` |
| microSD | `sd_card` (protocollo SD in SPI), `fatfs_store` + FatFs R0.16 (`third_party/fatfs`), `sd_spi_pico.cpp` (SPI0 GP16–19, GP22) — `docs/STORAGE.md` |
| Persistenza | workspace versione 3; versioni 1 e 2 lette, V-Window legacy corrette |
| Test | `graph_engine_test`, `graph_solve_test`, `graph_file_test`, `graph_app_test`, `sd_fatfs_test` |
| Documentazione | GRAPH_PARITY, GRAPH_ENGINE, STORAGE, PCGRAPH_FORMAT (nuovi); FEATURE_MATRIX, APPLICATIONS_ARCHITECTURE, ARCHITECTURE, GRAPHICS_ENGINE, MATH_ENGINE, TESTING, HARDWARE, BUILD_STATUS, ROADMAP, CHANGELOG, README; rapporto `docs/reports/SESSIONE_GRAPH_v1.8.0.md` |

## 3. Funzionalità ancora da completare

- GRAPH (dettaglio in `docs/GRAPH_PARITY.md`, sezione "Mancanze"): Dual Screen
  G+G/GtoT e Graph to Table, comandi grafici di Run-Matrix/programmi, FORMAT
  durante lo SKETCH, suffissi angolari °/ʳ/ᵍ, immagini a due livelli.
- TABLE: r= e parametriche, Variable = LIST, editing delle righe, LISTMEM,
  lista delle relazioni propria.
- Run-Matrix: Punto 3 (Natural V.P.A.M. ↔ formato lineare), matrici/vettori/
  liste nel parser, Base-N intera, simboli ingegneristici, Simp manuale.
- Modalità con solo architettura e modello dati: eActivity, Spreadsheet, Dyna
  Graph, Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture
  Plot, 3D Graph; Python richiede MicroPython; E-CON4 non realizzabile.
- Collaudo hardware. Dettaglio in `docs/ROADMAP.md` e `docs/FEATURE_MATRIX.md`.

## 4. Prossimi passaggi esatti (v1.9.0, proposta in `docs/ROADMAP.md` §2)

1. Collaudo su PicoCalc (procedura in `docs/HARDWARE.md`, punti 6–7 per GRAPH e
   microSD); aggiornare GRAPH_PARITY/FEATURE_MATRIX a TESTED ON HARDWARE solo
   dopo la prova, con data e revisione della scheda.
2. TABLE completa (r=, parametriche, LIST, editing, LISTMEM).
3. Dual Screen G+G/GtoT e Graph to Table (`graph::Viewport` accetta già
   un'area diversa da 320×282).
4. Dyna Graph, Recursion, Conic Graphs sul servizio GraphCore.
5. Punto 3: Natural V.P.A.M. ↔ formato lineare.

## 5. Problemi noti / da verificare

- Nessuna verifica su PicoCalc: display, tastiera, flash, microSD, Wi-Fi, TLS;
  tempi di disegno di GRAPH e della valutazione esatta sul Cortex-M33.
- microSD: pin SPI0 presi dai sorgenti ufficiali ClockworkPi ma non provati;
  accesso a settore singolo (niente CMD18/CMD25); il firmware non formatta
  schede (`f_mkfs` solo nell'host).
- Memoria GRAPH: punti delle curve ≤ 96 KB in totale; due immagini RLE molto
  complesse (≤ 96 KB ciascuna) insieme a 20 grafici densi possono esaurire
  l'heap: il disegno si ferma con "Memory ERROR" (non verificato su hardware).
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
# Relay AI: test
cd relay && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python -m unittest discover -s tests
```

```bash
# Stack sul target di un sorgente (flag dal build del firmware)
cd build/pico2w && FL=CMakeFiles/picocalc_fxai.dir/flags.make
eval arm-none-eabi-g++ $(grep ^CXX_DEFINES $FL | cut -d= -f2-) $(grep ^CXX_INCLUDES $FL | cut -d= -f2-) $(grep ^CXX_FLAGS $FL | cut -d= -f2-) -fstack-usage -c ../../src/graph_engine.cpp -o /tmp/graph_engine.o
```
