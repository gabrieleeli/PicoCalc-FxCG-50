# CONTINUE — punto di ripresa del progetto PicoCalc FX-CG50 AI

Documento di checkpoint per riprendere lo sviluppo esattamente da qui.
Ultimo aggiornamento: **v1.9.0 — consolidamento della v1.8.0 con il supporto
del Pimoroni Pico Plus 2 W**, branch **`main`** (tag `v1.9.0` da creare: vedi
il rapporto, §16). Rapporto
completo: `docs/reports/CONSOLIDAMENTO_v1.9.0.md`.

## 0. Branch

| Branch | Contenuto | Regola |
|---|---|---|
| **`main`** | **v1.9.0**: v1.8.0 completa + Pimoroni Pico Plus 2 W (PSRAM, flash 16 MB, 3D Graph) | si continua qui |
| `version/v1.5.4`, `version/v1.6.0`, `version/v1.7.0`, `version/v1.8.0` | snapshot storici, uno per versione | non modificare |

I branch duplicati della 1.9.0 e i branch temporanei (`version/v1.9.0`,
`release/1.9.0`, `claude/laughing-mendel-jewowt`, `claude/loving-meitner-jifxhg`,
`claude/peaceful-euler-auyrm5`, `claude/pimoroni-pico-plus-2w-94lg56`,
`ccr-90a0c133-k0rf3a`) sono **interamente contenuti in `main`** (verificato con
`git merge-base --is-ancestor`) e vanno eliminati: l'ambiente di sviluppo della
sessione ha rifiutato l'eliminazione dei branch remoti (HTTP 403), quindi va
fatta dal proprietario (comandi nel rapporto, §16). Un solo branch per
versione; mai force-push; nessuna release o tag nuovi senza richiesta.

## 1. Stato attuale

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

## 2. Cosa è stato fatto nell'ultima sessione

| Area | Contenuto |
|---|---|
| Git | analisi di tutti i branch; merge della v1.8.0 nella linea 1.9.0 con l'architettura della v1.8.0; `main` avanzato solo con fast-forward; branch duplicati verificati; eliminazione dei branch e push del tag `v1.9.0` rifiutati dall'ambiente (HTTP 403), da fare |
| Hardware | profili `PICO_PLUS_2W` (predefinito) e `PICO2W`, mappa della flash per scheda, PSRAM con self-test e avvio solo SRAM in caso di guasto, pool TLSF, `operator new` instradato, limiti per scheda e per memoria presente, MSPLIM, HardFault, core 1 per la diagnostica, lettura del JEDEC ID della flash |
| Correzioni | ultimo settore (RP2350-E10), scrittura flash da buffer in PSRAM, "Memory ERROR" che allocava (terminazione), `bad_alloc` dell'autosalvataggio, modifiche perse dopo un salvataggio fallito per memoria, `Workspace` di 5,4 KB sullo stack, yaw 3D, UF2 della scheda sbagliata |
| Applicazioni | 3D Graph; cache dei layer di GRAPH in PSRAM; archivio dei file su LittleFS; SYSTEM INFO/DIAG estesi; MEMORY per il 3D |
| Test | suite per entrambi i profili, sanitizer, 6 nuove suite, fixture generata dai sorgenti della v1.8.0 |

## 3. Funzionalità ancora da completare

- Dual Screen (G+G, GtoT, T+G) e Graph to Table; comandi grafici di
  Run-Matrix/programmi; FORMAT durante lo SKETCH; suffissi angolari °/ʳ/ᵍ.
- Liste: 999 elementi sulla Plus; sul Pico 2 W restano 255 (record da 64 KB).
- Run-Matrix: Punto 3 (Natural V.P.A.M. ↔ lineare), matrici/liste nel parser,
  editor MAT (l'API supporta matrici fino a 999×999 con la PSRAM), Base-N
  intera, simboli ingegneristici.
- 3D Graph: sfere, piani e rette parametriche della fx-CG50.
- Modalità con solo modello dati: eActivity, Spreadsheet, Dyna Graph,
  Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot;
  Python; E-CON4 non realizzabile.

## 4. Prossimi passaggi esatti

1. **Collaudo fisico sulla PicoCalc con il Pimoroni Pico Plus 2 W**: HW-01…HW-36
   e HW-40…HW-82 (log USB dall'accensione, righe `[psram]`, `[flash]`, `[mem]`,
   `[DIAG]`, `[fault]`, foto). Ogni KO va riprodotto con un test e corretto;
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
# Simulatore (profilo Plus, pool PSRAM simulato): Run-Matrix, GRAPH/TABLE, 3D
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
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
