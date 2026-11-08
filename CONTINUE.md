# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: v1.6.0, branch `claude/peaceful-euler-auyrm5`.

## 1. Stato attuale

- **Firmware RP2350 compilabile**: `build/pico2w/picocalc_fxai.uf2` (≈2,0 MB UF2,
  ≈1019 KB di immagine flash, 200 KB di heap, stack 32 KB). Pico SDK 2.2.0, board `pico2_w`.
  La v1.5.4 di partenza **non compilava** (vedi `docs/BUILD_STATUS.md`).
- **Test host**: 14 suite CTest, 4495 controlli, 0 errori; puliti anche con
  AddressSanitizer + UBSan. Relay AI: 15 test Python, tutti superati.
- **Nessun test su hardware fisico**: display, tastiera, flash, Wi-Fi e TLS sono
  implementati seguendo i driver ufficiali ClockworkPi e il datasheet ST7365P, ma
  non sono stati verificati su una PicoCalc reale.
- **Push GitHub bloccato** da permessi (403: la GitHub App di Claude non ha accesso
  al repository). I commit sono locali nel branch indicato; vedi §6.

## 2. Funzionalità completate (host-tested)

| Area | Contenuto |
|---|---|
| Build | CMake firmware + CMake host (lib core unica, CTest), linker script con stack 32 KB |
| HAL PicoCalc | LCD ST7365P su SPI1 25 MHz con init ufficiale (RGB666, INVON, MADCTL 0x48); tastiera STM32 I2C non bloccante; batteria/retroilluminazione; flash A/B ultimi 128 KB |
| Motore matematico | parser→AST→valutatore unico, reale e complesso, priorità Casio, integrali Gauss-Kronrod, derivate Ridders, SOLVE, Σ/Π, Base-N, DMS; limiti di token/annidamento e budget di stack (Stack ERROR invece del crash) |
| Editor V.P.A.M. | frazioni, radici, potenze, indici, |x|, log_a b, e^x, 10^x, ∫, d/dx, Σ, Π; undo/redo; copia/incolla; cursore allineato al renderer |
| Grafica | font Unifont 8×16 + Spleen 6×12, layout matematico 2D, aggiornamento LCD a tile (solo le zone cambiate) |
| App | Main Menu, Run-Matrix, Statistics, Graph, Table, Equation (sistemi, polinomi, solver, disequazioni), System/Set Up, Memory, Conversion, AI Chat, Symbols |
| AI | relay Python (Anthropic SDK `claude-opus-5-5` + OpenAI SDK), client Wi-Fi/TLS sul Pico con verifica certificato obbligatoria |
| Persistenza | workspace serializzato (impostazioni, variabili, storia, liste, funzioni, equazioni, chat) con CRC e doppio slot |

## 3. Funzionalità ancora da completare

Modalità mostrate come "non disponibili" nel menu: eActivity, Spreadsheet,
Dyna Graph, Recursion, Conic Graphs, Program, Financial, E-CON4, Link, Geometry,
Picture Plot, 3D Graph, Python (MicroPython non integrato).
Altro: matrici/vettori come valori nel parser (oggi solo API C++), regressioni
sinusoidale e logistica, test χ²/F/2-campioni nell'interfaccia, scheda SD,
collaudo su hardware. Dettaglio in `docs/FEATURE_MATRIX.md` e `docs/ROADMAP.md`.

## 4. Ultima operazione eseguita

1. Correzione dello stack del valutatore (commit "Bound parser/evaluator/editor
   stack use"): sul target ARM ogni livello di `evalNode` occupava ~1,3 KB, quindi
   una somma di ~25 termini avrebbe superato lo stack da 32 KB. Ora i frame
   ricorsivi sono di 32–128 byte, le catene sono iterative e ci sono limiti
   espliciti (dettagli in `docs/MATH_ENGINE.md`).
2. Documentazione completa in `docs/` (indice in `docs/README.md`; i documenti
   storici sono conservati e marcati come tali), `relay/README.md`.
3. `scripts/make_release.sh`: zip dei sorgenti (git archive) + UF2 + SHA256SUMS.
4. UF2 ricompilato, test host (normali e ASan/UBSan) e test del relay rieseguiti.
5. Push ritentato: ancora 403 (vedi §6).

## 5. Prossimi passaggi esatti

1. Dare accesso al repository (vedi §6) e poi
   `git push -u origin claude/peaceful-euler-auyrm5`.
2. Pacchetto di release: `scripts/build_pico2w.sh && scripts/make_release.sh`.
3. **Collaudo hardware** (priorità massima):
   - flash dell'UF2 con build `-DFXAI_DISPLAY_CALIBRATION_SCREEN=ON` per verificare
     colori/orientamento; se i colori sono errati provare `-DFXAI_LCD_RGB565=ON`
     o verificare il bit BGR di MADCTL;
   - verificare tutti i tasti (log su USB seriale), batteria, salvataggio dopo riavvio;
   - avviare il relay e provare AI Chat su Wi-Fi reale.
4. Prossime funzionalità consigliate: matrici nel parser (Run-Matrix MAT), Recursion,
   Conic Graphs, Financial (TVM), 3D Graph, poi integrazione MicroPython.

## 6. Problemi noti / da verificare

- Push verso GitHub rifiutato (403): il proprietario deve concedere l'accesso alla
  GitHub App di Claude o ricollegare GitHub (claude.ai → Settings → Connectors).
- Senza RTC, mbedTLS non verifica le date di validità dei certificati (catena e
  hostname sì).
- Ritardo di 16 ms tra scrittura e lettura I2C della tastiera ripreso dal driver
  ufficiale: valore conservativo, da ottimizzare sull'hardware reale.

## 7. Comandi per riprendere

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
# Simulatore con screenshot (vedi intestazione di src/platform/host/simulator.cpp)
build/host/picocalc_sim /tmp/shots script.txt
```

```bash
# Relay AI: test
cd relay && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python -m unittest discover -s tests
```
