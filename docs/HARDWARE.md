# Hardware — PicoCalc (mainboard v2.0) + Pimoroni Pico Plus 2 W / Raspberry Pi Pico 2 W

Fonti: repository ufficiale [clockworkpi/PicoCalc](https://github.com/clockworkpi/PicoCalc)
(`Code/picocalc_helloworld/lcdspi`, `Code/picocalc_helloworld/i2ckbd`,
`Code/picocalc_keyboard`, `ST7365P_SPEC_V1.0.pdf`, `clockwork_Mainboard_V2.0_Schematic.pdf`).
**Tutti i valori sono ripresi da queste fonti; nessuno è ancora stato verificato su
un dispositivo fisico.** I pin della PicoCalc sono gli stessi con entrambi i
moduli (raccolti in `include/board_config.hpp`, verificati da `board_profile_test`);
memoria, flash e PSRAM della Plus sono descritte in [PICO_PLUS_2W.md](PICO_PLUS_2W.md).

## Microcontrollore

| | |
|---|---|
| MCU | Pico Plus 2 W: RP2350B (QFN-80, 48 GPIO), flash QSPI 16 MB, **PSRAM APS6404L 8 MB** su QMI CS1 (GPIO47); Pico 2 W: RP2350A (QFN-60), flash 4 MB, niente PSRAM. Entrambi Cortex-M33, 520 KB SRAM |
| Clock | 150 MHz (default SDK); PSRAM a 75 MHz (divisore 2) |
| Wi-Fi | CYW43439 tramite `pico_cyw43_arch_lwip_poll`, stessi pin GP23/24/25/29 sui due moduli |
| Core 1 | usato solo dalla diagnostica (stress dell'allocatore su due core), con lo stack in SCRATCH_X |

## Display

| | |
|---|---|
| Controller | ST7365P (compatibile ILI9488), 320×320 |
| Bus | **SPI1 hardware**: SCK GP10, MOSI GP11, MISO GP12, CS GP13, DC GP14, RST GP15 |
| Clock SPI | 25 MHz (come il driver ufficiale; configurabile con `FXAI_LCD_SPI_HZ`) |
| Formato | `COLMOD 0x66` (RGB666, 3 byte/pixel) come il driver ufficiale; `-DFXAI_LCD_RGB565=ON` usa `0x55` (supportato dal datasheet, metà dati) |
| Orientamento | `MADCTL 0x48` (MX + BGR), `INVON` |
| Init | gamma positiva/negativa, power control, VCOM, B0/B1/B4/B6/B7/E9/F7 come `pico_lcd_init()` ufficiale |

Il framebuffer è RGB565 in RAM; durante l'invio ogni riga è convertita in RGB666.
Solo i tile 16×16 modificati vengono trasmessi (`gfx::TileDiff`), raggruppati in
rettangoli. **DMA non usato**: il collo di bottiglia è il bus SPI (un frame
completo RGB666 ≈ 98 ms a 25 MHz) e la conversione di una riga richiede molto meno
del suo invio, quindi la DMA libererebbe solo CPU inutilizzata; resta un'ottimizzazione
possibile dopo il collaudo, insieme a un clock SPI più alto.

## Tastiera

| | |
|---|---|
| Controller | STM32 (firmware ClockworkPi) su **I2C1**: SDA GP6, SCL GP7, 10 kHz, indirizzo 0x1F |
| Lettura | scrittura del registro + STOP, attesa 16 ms (`FXAI_KBD_REPLY_DELAY_MS`), lettura di 2 byte; macchina a stati non bloccante |
| FIFO (0x09) | `[stato, tasto]`: 1 premuto, 2 tenuto, 3 rilasciato |
| Batteria (0x0B) | `[reg, percentuale \| 0x80 se in carica]`; 0 = nessuna batteria |
| Retroilluminazione | LCD 0x05, tastiera 0x0A (scrittura con bit 7) |
| Configurazione STM32 | `CFG_USE_MODS \| CFG_REPORT_MODS`: Shift/Caps applicati dallo STM32, modificatori riportati come eventi |

Lo STM32 ripete i caratteri e le frecce come PRESSED ogni 100 ms dopo 300 ms; per
gli altri tasti invia HOLD, che il firmware trasforma in un solo evento "pressione
lunga" (EXIT tenuto = menu principale).

Codici dei tasti (da `keyboard.h` ufficiale): F1–F5 `0x81–0x85`, Shift+F1–F5
`0x86–0x89, 0x90`, Esc `0xB1`, frecce `0xB4–0xB7`, Enter `0x0A`, Backspace `0x08`,
Tab `0x09`, Del `0xD4`, Shift+Del `0xD5`, Shift+Esc `0xD0`, Shift+Enter `0xD1`,
Shift+Tab `0xD2`, modificatori `0xA1–0xA5`, Caps `0xC1`.

## microSD

| | |
|---|---|
| Bus | **SPI0 hardware**: SCK GP18, MOSI GP19, MISO GP16 (pull-up), CS GP17 |
| Card detect | GP22, basso = scheda inserita (pull-up interno) |
| Clock | 400 kHz durante l'inizializzazione, poi 12,5 MHz |
| Protocollo | SD in modalità SPI (CMD0/8/ACMD41/58/9/16/17/24/13), schede SDSC, SDHC, SDXC |
| File system | FatFs R0.16: FAT12/16/32 ed exFAT, nomi lunghi |
| Fonte dei pin | sorgenti ufficiali ClockworkPi: `Code/MP3Player/config.h`, `Code/pico_multi_booter/boot.c` |

Driver in `src/platform/pico/sd_spi_pico.cpp` (bus) sopra il protocollo
portabile `src/sd_card.cpp` e `src/fatfs_store.cpp`, provati sul PC con una
scheda simulata. Cartelle usate: `PICOCALC/GRAPH` e `PICOCALC/PICT`
([STORAGE.md](STORAGE.md)). **Da collaudare** sulla PicoCalc: procedura completa
in [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) e diagnostica SYSTEM → F2 DIAG.

## Flash

- Mappa per scheda (firmware, settore diagnostico, LittleFS, BTstack, workspace
  A/B): [PICO_PLUS_2W.md](PICO_PLUS_2W.md#5-flash-partizioni-e-protezioni) e
  [STORAGE.md](STORAGE.md). Sul Pico 2 W il workspace resta negli ultimi 128 KB
  come nella v1.8.0; sulla Plus occupa 1 MB prima dell'ultimo settore.
- L'ultimo settore non viene mai scritto (erratum RP2350-E10).
- Scrittura con `flash_safe_execute()` (interrupt disabilitati, core 1 fermo se
  attivo), dati copiati prima in un buffer di rimbalzo in SRAM, verifica della
  configurazione PSRAM dopo ogni operazione.
- Le regioni non si sovrappongono mai all'immagine del firmware e vengono
  limitate alla capacità reale del chip (JEDEC ID letto all'avvio).

## Differenze tra le schede che riguardano la v1.12.0

EQUATION (v1.12.0) ha gli stessi limiti sulle due schede, come la fx-CG50: 6
incognite, grado 6, equazione del Solver di 255 caratteri, al più 4 000
valutazioni per un SOLVE (`limits.hpp`). I coefficienti con la forma esatta
(≈ 7,6 KB) stanno sull'heap (PSRAM sulla Plus); il blocco v8 del record
occupa da ≈ 1,2 a ≈ 4 KB (59,75 KB disponibili sul Pico 2 W, 511,75 KB sulla
Plus). Nessun pin, driver, partizione o script del linker cambiato. Collaudo:
[HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md) (HW-120…HW-134).

## Differenze tra le schede che riguardano la v1.11.0

Anche le novità della v1.11.0 (List 1–26, CSV, MatAns/ListAns, celle esatte,
Line con INS e ⌟, simboli ingegneristici, Simp, Base-N, comandi grafici) sono
**solo software**: pin, driver, periferiche, mappa della flash e script del
linker restano quelli della v1.9.0/v1.10.0 (verificati di nuovo sugli header
del Pico SDK 2.2.0 e con gli `static_assert` di `board_config.hpp`). I CSV
usano le stesse due memorie dei progetti GRAPH: la microSD su SPI0 e LittleFS
nella partizione interna (≈ 11 MB sulla Plus, ≈ 1,9 MB sul Pico 2 W).

Le differenze sono nei limiti (`include/limits.hpp`):

| Voce | Pico 2 W | Pico Plus 2 W |
|---|---|---|
| Elementi per lista | 255 | 999 (come la fx-CG50) |
| Elementi in tutte le 26 liste | 1 530 (l'occupazione massima delle 6 liste della v1.10.0: ≈ 12 KB nel record da 59,75 KB) | 20 000 (≈ 156 KB nel record da 511,75 KB; liste in PSRAM) |
| CSV letto al massimo | ≈ 41 KB | ≈ 484 KB |
| Forme esatte delle memorie | fino a 64 elementi per memoria (≈ 56 byte per elemento) | uguale |

Immagine del firmware v1.11.0: +80 KB circa rispetto alla v1.10.0 su
entrambe le schede (1 594 956 byte = 38 % della partizione da 4 MB sulla
Plus, 1 586 168 byte = 76 % della partizione da 2 MB sul Pico 2 W); RAM
statica +924 byte. Valori esatti in [BUILD_STATUS.md](BUILD_STATUS.md).

## Differenze tra le schede che riguardano la v1.10.0

Le novità della v1.10.0 (matrici, vettori e liste, editor MAT/VCT, Math/Line,
CLIP, parentesi colorate, cursore sugli errori) sono **solo software**: nessun
pin, driver o periferica nuovi, stessa mappa della flash e stessi script del
linker (`memmap_picocalc.ld`) della v1.9.0. Verificati sugli header del Pico
SDK 2.2.0 (`pico2_w.h`, `pimoroni_pico_plus2_w_rp2350.h`) e da `board_config.hpp`
con `static_assert`:

| Voce | Raspberry Pi Pico 2 W | Pimoroni Pico Plus 2 W |
|---|---|---|
| `PICO_BOARD` / `FXAI_BOARD` | `pico2_w` / `PICO2W` | `pimoroni_pico_plus2_w_rp2350` / `PICO_PLUS_2W` |
| MCU, GPIO | RP2350A, 30 GPIO | RP2350B, 48 GPIO |
| SRAM / PSRAM | 520 KB / — | 520 KB / 8 MB (CS GPIO47) |
| Flash, partizione firmware | 4 MB, 2 MB | 16 MB, 4 MB |
| SPI (LCD SPI1, microSD SPI0), I²C (tastiera I2C1), Wi-Fi (CYW43439 GP23/24/25/29) | identici | identici (Wi-Fi sul modulo RM2) |
| Connettore Qw/ST (I²C) | — | presente, non usato dal firmware |

La memoria è **diversa**, quindi lo sono i limiti delle matrici
(`include/limits.hpp`) e lo spazio che occupano:

| Matrici e vettori | Pico 2 W | Pico Plus 2 W |
|---|---|---|
| Lato massimo | 40 | 999 (come la fx-CG50) |
| Elementi per matrice o vettore | 400 | 10 000 |
| Elementi di tutte le memorie (Mat/Vct A–Z, MatAns, VctAns) | 1 200 | 20 000 |
| Dove stanno in RAM | heap SRAM (≈ 190 KB liberi) | PSRAM (≥ 512 byte → pool PSRAM) |
| Spazio nel record in flash | ≤ 9,4 KB (reali) / 18,8 KB (complessi) su 59,75 KB | ≤ 156 KB / 312 KB su 511,75 KB |
| RAM di lavoro di un calcolo | 16 byte per elemento per matrice temporanea (es. inversa di 20×20: ≈ 19 KB) | idem (inversa di 100×100: ≈ 480 KB in PSRAM) |

Immagine del firmware v1.10.0: +129 KB circa rispetto alla v1.9.0 su entrambe
le schede (1 515 272 byte = 36 % della partizione da 4 MB sulla Plus,
1 506 516 byte = 71 % della partizione da 2 MB sul Pico 2 W); RAM statica
+2 188 byte. Valori esatti in [BUILD_STATUS.md](BUILD_STATUS.md).

## Collaudo consigliato sul dispositivo

1. Build con `-DFXAI_DISPLAY_CALIBRATION_SCREEN=ON`: barre R/G/B/W/Y/M/C, quadrato
   rosso in alto a sinistra, blu in basso a destra. Se i colori sono scambiati o
   invertiti, annotare il risultato e provare `-DFXAI_LCD_RGB565=ON`.
2. Log su USB seriale (115200): messaggi di avvio, stato del workspace, heap libero.
3. Verificare ogni tasto nel menu e in Run-Matrix; SHIFT, Ctrl+Z, Esc tenuto.
4. Modificare un'impostazione, attendere 3 s, togliere alimentazione, riaccendere.
5. Configurare Wi-Fi e relay in SYSTEM e provare AI Chat.
6. GRAPH: disegnare 20 funzioni (anche parametriche con Tpitch piccolo) e
   misurare il tempo di disegno; verificare che TRACE ed EXIT rispondano
   durante il disegno; controllare i colori di curve e riempimenti.
7. PSRAM, flash, 3D e prestazioni della Plus: seguire
   [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) (HW-40…HW-82); novità
   della v1.10.0 (MAT/VCT, parentesi colorate sul pannello, CLIP, cursore
   sugli errori, Math/Line): [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md) (HW-90…HW-99);
   novità della v1.11.0 (List 1–26, CSV su microSD e flash interna, MatAns,
   INS e ⌟, Eng, Simp, Base-N, comandi grafici):
   [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md) (HW-100…HW-115).
8. microSD e TABLE: seguire [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)
   (diagnostica SYSTEM → DIAG con log `[DIAG]` sulla seriale USB). In sintesi:
   FILE > SAVE AS, estrarre e reinserire la scheda,
   OPEN; PICTURE STORE/RECALL; estrarre la scheda durante un salvataggio e
   verificare che resti il file precedente; leggere i file `.pcgraph` dal PC.
