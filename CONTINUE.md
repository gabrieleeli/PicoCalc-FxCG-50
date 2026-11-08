# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.7.0 — Sessione 1** (audit, architettura delle
applicazioni, Run-Matrix), branch **`version/v1.7.0`**.

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| `main` | codice originale caricato | non modificare |
| `version/v1.5.4`, `version/v1.6.0` | release storiche | non modificare |
| `claude/peaceful-euler-auyrm5` | v1.6.0 + 2 commit di documentazione (base della v1.7.0) | storico |
| **`version/v1.7.0`** | lavoro in corso della v1.7.0 | si continua qui finché l'utente non approva la release |

Non creare `version/v1.8.0` o successive finché i rispettivi incrementi non sono
completati e verificati. Mai force-push.

## 1. Stato attuale

- **Firmware RP2350 compilabile**: `build/pico2w/picocalc_fxai.uf2` (≈2,3 MB UF2,
  ≈1130 KB di immagine flash, heap ≈197 KB, stack 32 KB), nessun warning nei
  sorgenti del progetto. Pico SDK 2.2.0, board `pico2_w`, arm-none-eabi-gcc 13.2.
- **Test host**: 17 suite CTest, 5347 controlli, 0 errori, anche con ASan/UBSan
  (dettaglio in `docs/TESTING.md`). Relay AI: 15 test Python superati.
- **Nessun test su hardware fisico**.
- Simulatore: screenshot 12–19 di Run-Matrix in `docs/screenshots/`
  (script `tests/sim/run_matrix.txt`).

## 2. Cosa è stato fatto nella Sessione 1

| Area | Contenuto |
|---|---|
| Architettura | registro delle 23 modalità (`app_registry.hpp/.cpp`), modelli dati delle modalità future (`app_models.hpp/.cpp`), `docs/APPLICATIONS_ARCHITECTURE.md`, `architecture_test` (registro ↔ sorgenti ↔ firmware, separazione, stratificazione, cicli) |
| Motore | aritmetica esatta (`exact.hpp/.cpp`) e valutatore esatto sullo stesso AST; relazioni, logica, Int÷, Rmdr, MOD_Exp, RndFix, RanNorm#, RanBin#; frazioni miste; decimali periodici; variabili condivise via `NameResolver`; arrotondamento di visualizzazione come la calcolatrice |
| Run-Matrix | risultati esatti (frazioni, √, π) con le regole CG50; S⇔D che preserva il valore; Frac Result d/c ↔ ab/c; periodo; cronologia modificabile sul posto con ricalcolo; riuso di espressione/risultato; →A~F; VARS per categorie; OPTN esteso; "≈" per i metodi numerici |
| Persistenza | formato v2 del workspace (valori esatti), lettura dei record v1.6.0 |
| Documentazione | APPLICATIONS_ARCHITECTURE, RUN_MATRIX, MATH_ENGINE, FEATURE_MATRIX (stato "TESTED ON SIMULATOR"), TESTING, CHANGELOG, ROADMAP, BUILD_STATUS, ARCHITECTURE |

## 3. Funzionalità ancora da completare

- Run-Matrix: matrici/vettori/liste nel parser, modalità Base-N intera con
  operatori bit a bit, simboli ingegneristici, Simp manuale, cursore sul punto
  dell'errore, selezione parziale (CLIP).
- Modalità con solo architettura e modello dati: eActivity, Spreadsheet, Dyna
  Graph, Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture
  Plot, 3D Graph; Python richiede MicroPython; E-CON4 non realizzabile.
- Statistics: regressioni sinusoidale e logistica, test χ²/F/2 campioni/ANOVA
  nell'interfaccia, grafici a torta e a barre. Graph: parametrici.
- Collaudo hardware. Dettaglio in `docs/ROADMAP.md` e `docs/FEATURE_MATRIX.md`.

## 4. Prossimi passaggi esatti

1. **Sessione successiva — Punto 3: Natural V.P.A.M. ↔ formato lineare**
   (elenco completo in `docs/ROADMAP.md` §2): impostazione Input/Output
   Math/Line, round-trip garantito testo ↔ albero con test su corpus, cursore
   sul punto dell'errore, CLIP, SHIFT+INS, colori delle parentesi, regola del ×
   prima delle frazioni, modelli MAT/VCT.
2. Collaudo hardware (procedura in `docs/HARDWARE.md`).
3. Poi: matrici nel parser (MAT/VCT in Run-Matrix), Recursion, Conic Graphs,
   Financial (modelli pronti in `app_models.hpp`).

## 5. Problemi noti / da verificare

- Nessuna verifica su PicoCalc: display, tastiera, flash, Wi-Fi, TLS; tempi della
  valutazione esatta e del ricalcolo della cronologia sul Cortex-M33.
- Senza RTC, mbedTLS non verifica le date di validità dei certificati.
- Ritardo di 16 ms della tastiera I2C ripreso dal driver ufficiale, da ottimizzare.
- La fattorizzazione per i radicali (√ di razionali) usa divisioni di prova fino
  a 10⁵: su numeri primi grandi può richiedere fino a ~0,1 s sul target (stima,
  da misurare).

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
# Simulatore: screenshot di Run-Matrix
build/host/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
```

```bash
# Relay AI: test
cd relay && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python -m unittest discover -s tests
```

```bash
# Stack sul target di un sorgente (flag dal build del firmware)
cd build/pico2w && FL=CMakeFiles/picocalc_fxai.dir/flags.make
eval arm-none-eabi-g++ $(grep ^CXX_DEFINES $FL | cut -d= -f2-) $(grep ^CXX_INCLUDES $FL | cut -d= -f2-) $(grep ^CXX_FLAGS $FL | cut -d= -f2-) -fstack-usage -c ../../src/expression.cpp -o /tmp/expression.o
```
