# PicoCalc FX-CG50 AI su Pimoroni Pico Plus 2 W (v1.9.0)

Questo documento descrive il supporto dual-board introdotto nella v1.9.0:
**Raspberry Pi Pico 2 W** (default) e **Pimoroni Pico Plus 2 W** (PSRAM 8 MB +
flash 16 MB). La mappatura GPIO del PicoCalc è invariata: cambia solo ciò che è
interno al modulo.

> Stato di verifica: tutto quanto descritto qui è **compilato per entrambe le
> schede e testato su host** (suite nativa, ASan/UBSan, ThreadSanitizer).
> **Nessun test è stato eseguito su hardware reale**: driver PSRAM, scritture
> flash, LCD, Wi-Fi, SD e Bluetooth vanno validati con il menu
> *System → Test diagnostici* sul dispositivo (vedi §11).

## 1. Confronto tra le schede

| | Raspberry Pi Pico 2 W | Pimoroni Pico Plus 2 W |
|---|---|---|
| MCU | RP2350**A** (QFN-60), 2× Cortex-M33 150 MHz | RP2350**B** (QFN-80), 2× Cortex-M33 150 MHz |
| SRAM | 520 KiB (512 KiB + 2×4 KiB scratch) | 520 KiB |
| PSRAM | — | 8 MiB APS6404L, QMI CS1 su **GPIO47** |
| Flash QSPI | 4 MiB | 16 MiB |
| GPIO | 30 (GP0–29) | 48 (GP0–47) |
| ADC | GP26–29 (4 canali + temperatura) | **GPIO40–47** (8 canali + temperatura) |
| VSYS (ADC) | GP29 (condiviso con CYW43 CLK) | GPIO43 |
| Wireless | CYW43439 (Wi-Fi 2.4 GHz + BT 5.2) | Raspberry Pi RM2 (CYW43439) |
| Pin CYW43 (dal board header SDK) | WL_ON 23, DATA 24, CS 25, CLK 29 | WL_ON 23, DATA 24, CS 25, CLK 29 |
| `PICO_BOARD` | `pico2_w` | `pimoroni_pico_plus2_w_rp2350` |
| `FXAI_BOARD` | `PICO2W` | `PICO_PLUS_2W` |
| UF2 | `picocalc_fxai_pico2w.uf2` | `picocalc_fxai_pico_plus_2w.uf2` |

Tutti i valori specifici del modulo provengono dagli header dell'SDK
(`pico2_w.h`, `pimoroni_pico_plus2_w_rp2350.h`, `hardware/platform_defs.h`) e
sono raccolti in `include/board_config.hpp`; `static_assert` impediscono
discordanze (es. `PICO_FLASH_SIZE_BYTES == FXAI_FLASH_SIZE`, RP2350A/B atteso,
nessun pin PicoCalc in conflitto con CYW43 o con il CS della PSRAM).

### Nota ADC (RP2350B)

Sul RP2350B l'ADC è collegato a **GPIO40–47** (`ADC_BASE_PIN = 40`,
`NUM_ADC_CHANNELS = 9`), non a GP26–29: sul Pico Plus 2 W i pin d'header
GP26/27/28 sono solo digitali. Il firmware **non usa l'ADC del modulo**: la
batteria del PicoCalc è letta dal controller STM32 della tastiera via I²C
(`REG_ID_BAT`), e l'audio PWM non è implementato. `board::config::gpioHasAdc()`
permette di verificarlo in codice. Nota: i sorgenti ClockworkPi riportano pin
audio discordanti (GP26/27 in `pwm_sound.h`, GP28/27 in `MP3Player/config.h`);
va verificato sullo schema prima di scrivere un driver audio.

### Pin PicoCalc (invariati)

| Funzione | Pin |
|---|---|
| LCD ST7365P (PIO SPI) | SCK GP10, MOSI GP11, (MISO GP12), CS GP13, DC GP14, RST GP15 |
| Tastiera/batteria (I²C1, 0x1F, 10 kHz) | SDA GP6, SCL GP7 |
| micro-SD (SPI0) | SCK GP18, MOSI GP19, MISO GP16, CS GP17, detect GP22 (attivo basso) |
| UART0 → USB-C PicoCalc | TX GP0, RX GP1 (non usata: stdio su USB CDC) |

## 2. Mappa della memoria

```
0x10000000 ┌──────────────────────────┐  Flash XIP (CS0), cached
           │ firmware (partizione FW) │  Pico 2 W: 2 MiB  | Plus: 4 MiB
           ├──────────────────────────┤  FW
           │ settore diagnostico 4 KiB│  test flash/PSRAM (mai LittleFS)
           ├──────────────────────────┤  FW + 4 KiB
           │ LittleFS                 │  Pico 2 W: 1.99 MiB | Plus: 11.99 MiB
           ├──────────────────────────┤  END - 8 KiB
           │ riservato BTstack (TLV)  │  sempre riservato (anche con BT OFF)
0x10400000 / 0x11000000 └────────────┘  fine flash (4 MiB / 16 MiB)
0x11000000 ┌──────────────────────────┐  PSRAM (CS1), cached, write-back
           │ pool PSRAM_BOARD (TLSF)  │  8 MiB (solo Pico Plus 2 W)
0x11800000 └──────────────────────────┘
0x14000000   alias XIP non cached/non allocante (flash)
0x15000000   alias non cached della PSRAM (usato dal self-test)
0x18000000   spazio di manutenzione della cache XIP
0x20000000 ┌──────────────────────────┐  SRAM striped 512 KiB
           │ vettori RAM, .data, .bss │  framebuffer 200 KiB, stack app 32 KiB,
           │                          │  stack core1 8 KiB, lwIP, CYW43, ...
           ├──────────────────────────┤  __end__
           │ heap newlib (pool SRAM)  │  ~192 KiB liberi all'avvio
0x20080000 ├──────────────────────────┤  __HeapLimit
           │ SCRATCH_X 4 KiB          │  (stack core1 SDK non usato)
0x20081000 │ SCRATCH_Y 4 KiB          │  stack di boot core 0 (2 KiB)
0x20082000 └──────────────────────────┘
```

I due pool sono **banchi distinti**: non vengono mai presentati come 16 MiB
contigui. La PSRAM ha latenza e banda diverse (QSPI a 75 MHz attraverso la
cache XIP da 16 KiB condivisa con il codice in flash).

## 3. Driver PSRAM (`src/psram.cpp`)

Porting della sequenza di **SparkFun `sfe_psram.c`** (repository
`sparkfun/sparkfun-pico`, licenza MIT, © 2024 SparkFun Electronics), a sua volta
derivata dal bring-up RP2350 di CircuitPython/Raspberry Pi. Solo definizioni
dell'SDK (`hardware/structs/qmi.h`, `xip_ctrl.h`), nessun indirizzo inventato.

1. CS dal board header SDK: `PIMORONI_PICO_PLUS2_W_PSRAM_CS_PIN` (= GPIO47) →
   `GPIO_FUNC_XIP_CS1`.
2. Modalità diretta QMI (codice in SRAM, IRQ mascherati): uscita QPI `0xF5`
   (in caso di soft-reset), **Read ID `0x9F`** → MF ID, KGD (deve essere
   `0x5D`), EID (densità).
3. `0x66` Reset Enable, `0x99` Reset, `0x35` Enter Quad.
4. Timing M1 calcolato da `clk_sys`: `CLKDIV = ceil(clk_sys / 109 MHz)`
   (150 MHz → div 2 → SCK 75 MHz), `MAX_SELECT` ≤ 8 µs (tCEM, unità di 64 cicli),
   `MIN_DESELECT` ≥ 50 ns, `PAGEBREAK = 1024` (pagine da 1 KiB), `RXDELAY 1`,
   `SELECT_HOLD 3`, `COOLDOWN 1`.
5. Lettura `0xEB` quad con 6 clock di dummy, scrittura `0x38` quad.
6. `XIP_CTRL.WRITABLE_M1`.

**Self-test di avvio** (`include/psram_selftest.hpp`, template portabile e
testato su host con modelli di guasto): data bus (walking 1/0), address bus
(test di M. Barr su offset potenza di due), 8192 campioni pseudo-casuali
distribuiti sull'intera area (scrittura di tutti, poi rilettura → rileva
aliasing), percorso **con cache** (scrittura cached + clean + lettura
dall'alias **non cached** `0x15000000`) e percorso inverso (scrittura non cached
+ invalidazione + lettura cached). Se il chip non risponde o il test fallisce:
finestra M1 rimessa in sola lettura, errore stampato su USB e mostrato
all'avvio, **pool PSRAM non registrato**, firmware avviato solo in SRAM, nessun
crash.

**Cambio di `clk_sys`**: `board::setSystemClockKhz()` chiama
`psram::reapplyTiming()` (ricalcolo di CLKDIV/MAX_SELECT/MIN_DESELECT) e
riallinea il divisore PIO del LCD.

## 4. Architettura multi-pool (`include/mem_pools.hpp`)

| Pool | Backend | Note |
|---|---|---|
| `Pool::SRAM` | heap newlib (`malloc`, mutex `pico_malloc`) | blocchi sovra-allineati tramite registro lock-free (nessuna lettura fuori blocco) |
| `Pool::PSRAM_BOARD` | **TLSF** (`src/tlsf.cpp`) su 8 MiB | O(1), coalescenza immediata, allineamento arbitrario |
| `Pool::PSRAM_PICOCALC` | stub | interfaccia pronta, nessun driver finché lo schema ClockworkPi non è verificato |

API: `mem::alloc(pool|policy, size, align)` (lancia `std::bad_alloc`),
`mem::tryAlloc(...)` (senza eccezioni), `mem::free(p)` (riconosce il pool
dall'indirizzo, anche l'alias non cached), `mem::stats(pool)` → totale, usato,
libero, blocco libero più grande, picco, fallimenti, allocazioni vive.
Thread-safety: `critical_section` (spin lock + IRQ mascherati) per entrambi i
core; il TLSF è verificabile con `mem::checkIntegrity()`.

**Policy**: `PREFER_PSRAM` (PSRAM → SRAM), `SRAM_ONLY`, `PSRAM_ONLY`,
`PREFER_SRAM` (SRAM mantenendo 16 KiB di riserva, poi PSRAM).

**Adattatori `std::pmr`**: `mem::resource(Pool)` e `mem::resource(Policy)`
restituiscono `std::pmr::memory_resource*`; usati per campioni dei grafici,
mesh/z-buffer 3D, buffer AI.

**`operator new` globale** (`src/mem_new.cpp`): tutti i contenitori standard
passano dai pool *senza cambiare le API pubbliche*. Richieste ≥
`FXAI_PSRAM_NEW_THRESHOLD` (512 B) → `PREFER_PSRAM`, più piccole →
`PREFER_SRAM`; un `mem::PlacementScope` attivo sul core corrente forza la
policy (es. cronologia, matrici, liste, foglio di calcolo → PSRAM). Sul
Pico 2 W la PSRAM non esiste e tutto finisce nell'heap SRAM come prima: **zero
`#ifdef` nel codice applicativo**.

### Cosa resta in SRAM e cosa va in PSRAM

| SRAM (latenza/DMA/flash-safe) | PSRAM (capacità) |
|---|---|
| stack core 0 (32 KiB) e core 1 (8 KiB), ISR, vettori | matrici, liste statistiche, foglio di calcolo |
| pbuf/pool lwIP (`MEM_LIBC_MALLOC=0`), stato CYW43 | cronologia Run-Matrix |
| codice critico `__not_in_flash_func` (flash job, HardFault, QMI) | campioni dei grafici 2D (cache) e layer del grafico |
| buffer di rimbalzo flash (4 KiB) e buffer LittleFS | mesh, vertici proiettati e z-buffer 3D |
| framebuffer principale (default, vedi §7) | snapshot delle finestre del window manager |
| nodi AST piccoli e caldi dell'editor | buffer richieste/risposte AI, dati temporanei grandi |

## 5. Limiti: profilo per scheda + limiti runtime (`include/limits.hpp`)

I limiti di capacità hanno un tetto per scheda e vengono **ridotti a runtime**
in base alla memoria realmente libera (`heavyBudget()`, `largestFree`). Sul
Pico Plus 2 W i tetti sono quelli serviti dalla PSRAM, non quelli pensati per
520 KiB.

| Limite (file) | Prima (fisso) | Pico 2 W | Pico Plus 2 W |
|---|---|---|---|
| Finestre registrate (`window_manager.hpp`) | 8 | 16, dinamico | 64, dinamico |
| Profondità navigazione (`window_manager.hpp`) | 8 | 16, dinamico | 64, dinamico |
| Snapshot finestre | — | 0 | fino a 8 (solo PSRAM) |
| Dirty regions (`graphics_engine.hpp`) | 16 | 16 | 32 |
| Vertici poligono pieno (`graphics_engine.cpp`) | 32 (silenzioso) | 256 | 4096 (buffer dal pool) |
| Matrici: slot / dimensione (`editors.hpp`) | 4 / 4×4 | 26 / 40×40 | 26 / 999×999 (runtime: ~510 con 8 MiB liberi, vincolo inversione 4n² double) |
| Liste statistiche (`editors.hpp`) | 6 × 256 | 26 × 999 | 26 × 1 000 000 (runtime) |
| Foglio di calcolo (`math_engine.hpp`) | 45 × 5 (array) | 999 × 26 sparso | 999 × 26 sparso |
| Cronologia Run-Matrix (`point_a.cpp`) | 30 | 30 (parità fx-CG50) | 10 000 (runtime) |
| Nodi editor naturale | illimitato (rischio OOM) | 256 (runtime) | 4096 (runtime) |
| Funzioni Y= simultanee (`graph_renderer`) | 1 | 20 | 64 |
| Campioni per colonna del grafico | 1 | 1 | 8 (min/max per colonna) |
| Cache layer del grafico | — | no | sì (PSRAM) |
| Griglia 3D / z-buffer | 18, punti | 24, algoritmo del pittore | 128, z-buffer 16 bit |
| Buffer AI richiesta / risposta | — | 8 / 16 KiB | 256 KiB / 2 MiB |
| Arena CAS (riservata) | — | 0 | 1 MiB (nessun modulo CAS esiste ancora) |

**Limiti strutturali mantenuti** (stack in SRAM, uguali sulle due schede):
profondità parser 48 livelli (`kMaxParseDepth`, prima **illimitata**),
valutazione/serializzazione AST 64, layout matematico 32, annidamento modelli
VPAM 12, sommatoria ≤ 10 001 termini (guardia di tempo CPU), indice radice
2–99, fattoriale ≤ 170, grado regressione ≤ 4. Misura dei frame ARM
(`-fstack-usage`): ≤ ~300 B per livello di ricorsione → 48 livelli ≈ 14 KiB
sullo stack applicativo da 32 KiB.

### Gestione "Memoria insufficiente"

Ogni allocazione grande ha controllo del fallimento, rollback e nessun abort:

* `operator new` lancia `std::bad_alloc`; `PICO_MALLOC_PANIC=0` evita il
  `panic()` dell'SDK; il loop principale intercetta l'eccezione e mostra
  *Memoria insufficiente* (messaggio e popup in buffer pre-riservati: segnalare
  l'OOM non alloca).
* Run-Matrix: la voce di cronologia è costruita prima di toccare lo stato; se
  il vettore non cresce si scartano le voci più vecchie; `Ans` si aggiorna solo
  dopo. Editor naturale: riserva dei contenitori prima di ogni modifica
  strutturale. Matrici/liste/foglio: strong exception guarantee.
* Grafico 2D: senza memoria per la cache si torna alla valutazione diretta;
  3D: rinuncia allo z-buffer, poi dimezza la griglia, poi errore.
* Window manager: registrazione/apertura restituiscono `false`, mai eccezioni.

### Guardia di stack (entrambi i core)

`PICO_USE_STACK_GUARDS=1` (MSPLIM del Cortex-M33) + stack applicativo core 0 da
32 KiB in SRAM (`runOnAppStack`, MSPLIM impostato prima di spostare SP) e stack
core 1 da 8 KiB con guardia installata dall'SDK. Gli stack sono "dipinti"
(canary 0xA5A5A5A5) per la misura del picco. L'handler HardFault (in SRAM)
azzera MSPLIM, passa a uno stack d'emergenza per core, registra il guasto nei
registri scratch del watchdog e riavvia; l'avvio successivo lo segnala
("stack overflow (MSPLIM)" o "hard fault").

## 6. Flash 16 MB, LittleFS e bus QSPI condiviso

Partizioni calcolate da `PICO_FLASH_SIZE_BYTES` (`cmake/fxai_board.cmake`):
firmware 2 MiB (Pico 2 W) / 4 MiB (Plus), settore diagnostico, LittleFS v2.11.3
(vendorizzato in `third_party/littlefs`), 8 KiB finali riservati a BTstack.
Al primo avvio il file system viene formattato; `/boot_count` conta gli avvii.

**Vincolo del bus condiviso**: flash e PSRAM stanno sulla stessa QMI. Durante
erase/program l'XIP è disattivato per entrambi i chip, quindi:

* ogni scrittura passa da `flash_safe_execute()` con un *flash safety helper*
  proprio: il core 1 viene fermato (multicore lockout) solo se è in esecuzione;
  le scritture si fanno solo dal core 0; IRQ mascherati sul core chiamante;
* i dati da programmare sono prima copiati in un **buffer SRAM da 4 KiB**
  (una sorgente in PSRAM o flash non sarebbe leggibile durante l'operazione);
* nessun DMA o ISR tocca flash/PSRAM durante l'operazione (il flush LCD è
  sincrono via CPU, gli IRQ sono mascherati);
* l'SDK 2.2.0 esegue `xip_cache_clean_all()` prima del flush della cache (le
  righe sporche della PSRAM vengono scritte) e salva/ripristina M1
  `timing/rcmd/rfmt` (FLASH_DEVINFO senza CS1). **Verifica aggiuntiva**: dentro
  la zona sicura `psram::verifyConfigurationFromSafeZone()` confronta tutti i
  registri M1 (`timing, rfmt, rcmd, wfmt, wcmd`) e `WRITABLE_M1` con i valori
  attesi e li ripristina se alterati (contatore "ripristini" nella diagnostica);
* **test dedicato**: *System → Test → PSRAM dopo scrittura flash* riempie
  64 KiB di PSRAM (più della cache XIP), cancella e programma il settore
  diagnostico, verifica la flash e il contenuto PSRAM via alias cached e non
  cached, e che la PSRAM sia ancora scrivibile.

Opzioni di build: `FXAI_OPT_LEVEL=Os|O2` (default Os sul Pico 2 W, O2 sulla
Plus), `-ffunction-sections -fdata-sections --gc-sections`. `FXAI_ENABLE_LTO`
(default OFF): con LTO sull'intero target la build **fallisce** (il meccanismo
`-Wl,--wrap` di `pico_stdio`/`pico_float` lascia `__wrap_printf`/`__wrap_pow`
irrisolti), perciò l'opzione applica LTO ai soli sorgenti dell'applicazione:
compila (guadagno trascurabile: Plus 504 648 B contro 505 364 B) ma non è
validata su hardware, quindi resta disattivata.
Controllo dimensione post-build: warning oltre il 90% e oltre la partizione
firmware, errore oltre la flash fisica.

## 7. Grafica

* `gfx::Canvas` riceve sempre un buffer esterno; nessuna riscrittura del motore.
* **Framebuffer principale in SRAM (default)**, `FXAI_FRAMEBUFFER_IN_PSRAM=ON`
  per spostarlo (con fallback automatico in SRAM se la PSRAM non è disponibile).
  Analisi: `present()` è limitato dal link PIO-SPI a 12,5 Mbit/s → un frame
  completo da 204 800 byte richiede ≥ 131 ms in entrambi i casi; leggere il
  framebuffer dalla PSRAM (≈ 20 MB/s effettivi con miss da 8 byte a 75 MHz)
  costa ≈ 10 ms che si sovrappongono all'attesa della FIFO PIO, quindi la
  differenza attesa su `present()` è < 10%. Il disegno invece (fill, linee,
  testo) su PSRAM passa da write-allocate/write-back della cache XIP da 16 KiB,
  condivisa con il codice eseguito dalla flash: un framebuffer da 200 KiB la
  "sporca" continuamente e rallenta anche l'esecuzione. Per questo il default
  resta SRAM; sulla Plus la PSRAM ospita back buffer/layer, campioni, mesh e
  z-buffer. **Numeri reali**: *System → Test → Framebuffer: SRAM vs PSRAM*
  misura fill e `present()` da entrambe le memorie (non ancora eseguito su
  hardware).
* Grafico 2D: più funzioni simultanee (Y1…Yn con colori), cache dei campioni
  (nessuna rivalutazione durante il trace), sovracampionamento min/max sulla
  Plus, layer del grafico in PSRAM (trace = copia del layer + cursore).
* 3D (`FXAI_ENABLE_3D`): superficie z=f(x,y) con mesh e buffer in
  `PREFER_PSRAM`; z-buffer 16 bit e griglia fino a 128 sulla Plus, profilo
  ridotto automatico sul Pico 2 W (pittore, griglia 24); rotazione con le
  frecce, zoom F1/F2. Tile "3D Graph" del launcher.
* Compatibilità display PicoCalc invariata (stessa sequenza ST7365P; corretti
  solo gli errori che impedivano la compilazione del driver PIO).

## 8. Feature flag

| Flag | Pico 2 W | Pico Plus 2 W | Effetto |
|---|---|---|---|
| `FXAI_ENABLE_3D` | ON (profilo ridotto) | ON | grafico 3D |
| `FXAI_ENABLE_CAS` | OFF | ON | riserva budget/arena CAS (nessun modulo CAS esiste ancora) |
| `FXAI_ENABLE_BT` | OFF | OFF | BTstack LE (`pico_btstack_ble` + `_cyw43`); mai necessario per compilare |
| `FXAI_ENABLE_SD` | ON | ON | probe SD su SPI0 (CMD0/8/ACMD41/58/9, capacità) |
| `FXAI_FRAMEBUFFER_IN_PSRAM` | n/d | OFF | framebuffer in PSRAM |
| `FXAI_ENABLE_LTO` | OFF | OFF | LTO sperimentale (solo sorgenti app) |
| `FXAI_OPT_LEVEL` | Os | O2 | ottimizzazione |

Definizioni generate: `FXAI_BOARD_PICO2W` / `FXAI_BOARD_PICO_PLUS_2W`,
`FXAI_HAS_PSRAM`, `FXAI_FLASH_SIZE`, `FXAI_VERSION`, partizioni flash.

## 9. Wi-Fi, AI, Bluetooth

* Pin CYW43 dal board file dell'SDK (identici sulle due schede).
* `lwipopts.h` aggiunto (mancava: la build non era mai riuscita);
  `wifiConnect()` ora attiva la modalità STA prima di connettersi (prima la
  connessione falliva sempre). Nessuna API key nel firmware.
* Client AI: resta il placeholder del relay HTTPS (nessuna richiesta reale né
  gestione di API key esisteva nel codice); aggiunti `AiBuffer` e capacità
  per scheda (PSRAM sulla Plus).
* Bluetooth: opzionale, compilato e linkato solo con `FXAI_ENABLE_BT=ON`
  (verificato che compila su entrambe le schede).

## 10. Build e flashing

```sh
export PICO_SDK_PATH=/percorso/pico-sdk   # SDK >= 2.1 (testato 2.2.0)
./scripts/build.sh --board pico_plus_2w    # build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2
./scripts/build.sh --board pico2w          # build/pico2w/picocalc_fxai_pico2w.uf2
./scripts/build_pico2w.sh                  # alias compatibile
# opzioni: --opt O2|Os  --fb-psram  --bt  --lto  --clean  -DVAR=VALORE
```

Flashing: tenere premuto **BOOTSEL** (sulla Pico Plus 2 W è il pulsante BOOT)
collegando l'USB-C, poi copiare l'UF2 sul drive `RP2350`, oppure
`picotool load -x <file>.uf2`. **Gli UF2 non sono intercambiabili**: un
firmware Pico 2 W su una Plus non vede la PSRAM né i 12 MiB di LittleFS; un
firmware Plus su un Pico 2 W fallisce l'avvio della PSRAM (gestito: solo SRAM)
ma assume 16 MiB di flash. Una directory di build per scheda (CMake rifiuta il
cambio di scheda nella stessa directory).

Dimensioni (SDK 2.2.0, arm-none-eabi-gcc 13.2.1, Release):

| Build | Immagine flash | % partizione | SRAM statica |
|---|---|---|---|
| Pico 2 W (default, -Os) | 456 716 B (446 KiB) | 21% di 2 MiB | 328 448 B (320 KiB) |
| Pico Plus 2 W (default, -O2) | 505 364 B (493 KiB) | 12% di 4 MiB | 329 612 B (321 KiB) |
| Plus + framebuffer in PSRAM | 504 688 B | 12% | 124 708 B (121 KiB) |
| Pico 2 W + BT | 516 592 B | 24% | 331 960 B |
| Plus + BT | 575 568 B | 13% | 333 236 B |
| Pico 2 W -O2, 3D/SD OFF | 498 608 B | 23% | 328 720 B |
| Pico 2 W LTO (sorgenti app) | 453 752 B | 21% | 329 200 B |
| Plus LTO (sorgenti app) | 504 648 B | 12% | 330 352 B |

`arm-none-eabi-size` (default): Pico 2 W text 460 812 / bss 314 496;
Pico Plus 2 W text 509 460 / bss 314 812 (il `.data` è incluso in `text`
come immagine di caricamento).

SRAM statica = framebuffer 200 KiB + stack app 32 KiB + stack core1 8 KiB +
lwIP/CYW43/USB ≈ 50 KiB + resto; l'heap disponibile all'avvio è ≈ 192 KiB
(≈ 391 KiB con il framebuffer in PSRAM).

## 11. Test

| Tipo | Stato |
|---|---|
| Host (`./tests/run_host_tests.sh`, profili PICO2W e PICO_PLUS_2W) | **superati**: 19 suite, anche con ASan/UBSan; TSan sull'allocatore |
| CTest (`cmake -S desktop -B build-desktop && ctest`) | **superati** 19/19; emulatore SDL2 compilato |
| Compilazione firmware Pico 2 W / Pico Plus 2 W | **superata** (zero warning); varianti BT, FB in PSRAM, -O2, LTO-app compilate |
| Hardware reale (PSRAM, flash, LCD, tastiera, Wi-Fi, SD, BT) | **non eseguiti** in questo ambiente |

Test host nuovi: `mem_pools_smoke` (stress 200 000 operazioni con verifica dei
pattern, frammentazione, allineamenti fino a 64 KiB, esaurimento controllato,
pmr, policy, scope, due thread), `mem_new_smoke`, `psram_selftest_smoke`
(guasti simulati: bit dati bloccato, linee d'indirizzo, write-back perso,
cache stantia), `limits_smoke`, `oom_smoke` (rollback e zero leak dopo ogni
OOM, guardie di ricorsione), `flash_fs_smoke` (NOR emulata con regole RP2350),
`graphics_memory_smoke`, `system_app_smoke`.

Diagnostica on-device (**System → Test diagnostici**, risultati anche su USB):
ID/configurazione PSRAM, pattern PSRAM 256 KiB, stress allocatore, stress
allocatore su due core, PSRAM dopo scrittura flash, framebuffer SRAM vs PSRAM,
Wi-Fi (scansione reale: verifica radio e firmware CYW43), LittleFS, SD.
**Da eseguire sul dispositivo** prima di considerare validato il supporto.

## 12. Limitazioni note

* Nessuna verifica su hardware: i timing PSRAM (109 MHz max, RXDELAY 1) sono
  quelli verificati da SparkFun su APS6404L, non ancora sul Pico Plus 2 W.
* `PSRAM_PICOCALC` è solo un'interfaccia: lo schema della PSRAM sulla
  mainboard ClockworkPi non è stato verificato.
* Nessun modulo CAS: `FXAI_ENABLE_CAS` riserva solo budget.
* SD: solo identificazione e capacità (nessun file system su SD).
* Le scritture flash mascherano gli IRQ per la durata di un erase (tipicamente
  45 ms, fino a ~400 ms): USB e Wi-Fi si fermano per quel tempo.
* LTO sull'intero target non è supportato dall'SDK (vedi §6).
* Gli editor matrici/liste esistono come modelli; le loro schermate restano
  non raggiungibili dal launcher (come nella v1.5).
