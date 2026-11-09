# Memorizzazione: flash interna e microSD (v1.9.0)

Il firmware usa tre memorie con ruoli diversi.

| Memoria | Contenuto | Modulo |
|---|---|---|
| Flash interna, **record del workspace** (due slot A/B) | impostazioni, variabili, Ans, cronologia Run-Matrix, liste, funzioni e Setup di GRAPH, memorie V-Window e grafiche, SKETCH, TABLE, coefficienti, conversazioni AI, **3D Graph**, archivio scelto per i file | `storage.hpp/.cpp`, `workspace.cpp` |
| Flash interna, **LittleFS** (v1.9.0) | file utente interni: progetti GRAPH `.pcgraph`, immagini `.pcpict`, `.bmp` | `flash_fs.hpp/.cpp`, `third_party/littlefs` |
| microSD della PicoCalc | progetti GRAPH `.pcgraph`, immagini `.pcpict`, esportazioni `.bmp` | `file_store`, `fatfs_store`, `sd_card`, `sd_spi_pico.cpp` |

Il workspace in flash conserva lo stato di lavoro anche senza scheda. I file
dell'utente vanno sulla microSD oppure, scegliendo SYSTEM → "Archivio file" =
Flash interna, su LittleFS: le due memorie hanno la stessa interfaccia
`files::FileSystem` e lo stesso salvataggio atomico, quindi GRAPH le usa allo
stesso modo.

## Mappa della flash (v1.9.0)

Partizioni calcolate da `cmake/fxai_board.cmake` e verificate da
`board_config.hpp` (`static_assert`) e da `board_profile_test`:

| Area | Pimoroni Pico Plus 2 W (16 MB) | Raspberry Pi Pico 2 W (4 MB) |
|---|---|---|
| Firmware | 0x000000–0x3FFFFF (4 MB) | 0x000000–0x1FFFFF (2 MB) |
| Settore diagnostico | 0x400000 (4 KB) | 0x200000 (4 KB) |
| LittleFS | 0x401000–0xEFCFFF (≈ 11,0 MB) | 0x201000–0x3DDFFF (≈ 1,9 MB) |
| BTstack (riservato) | 0xEFD000 (8 KB) | 0x3DE000 (8 KB) |
| Record del workspace | 0xEFF000–0xFFEFFF (1 MB, 2 × 512 KB) | 0x3E0000–0x3FFFFF (128 KB, 2 × 64 KB: **posizione della v1.8.0**) |
| Ultimo settore | 0xFFF000: mai scritto | 0x3FF000: mai scritto dai nuovi salvataggi |

**Ultimo settore (erratum RP2350-E10).** Durante l'aggiornamento via UF2 il
workaround del bootrom per l'erratum E10 sovrascrive l'ultimo blocco della
flash (pico-sdk PR #2412; l'SDK sposta per lo stesso motivo i dati BTstack).
Nella v1.8.0 lo slot B del workspace finiva nell'ultimo settore: un record più
grande di ~60 KB poteva perdere la sua coda all'aggiornamento successivo (lo
slot A avrebbe salvato il salvataggio precedente). Ora:
- sulla Plus il record finisce 4 KB prima della fine della flash;
- sul Pico 2 W il record resta dove lo metteva la v1.8.0 (così l'aggiornamento
  conserva i dati) ma `FlashDevice::reservedTail()` vale 4 KB: i salvataggi
  sono limitati a 59,75 KB per slot e non scrivono mai l'ultimo settore, mentre
  i record già presenti, anche più lunghi, vengono ancora letti;
- la regione della flash rifiuta comunque erase/program in quell'area.

**Migrazione dalla v1.8.0 (Pico Plus 2 W).** Se il nuovo archivio non contiene
record e all'indirizzo usato dalla v1.8.0 (0x3E0000, dentro la partizione
firmware della Plus ma oltre l'immagine) ce n'è uno valido, il workspace viene
importato una volta, in sola lettura, e salvato nel nuovo archivio al primo
autosalvataggio (toast "Dati della v1.8.0 importati"). Un record nuovo non
viene mai sostituito da quello vecchio (`board_profile_test`).

**Scritture sicure con la PSRAM.** Flash e PSRAM condividono il bus QMI: ogni
erase/program passa da `flash_safe_execute()`, i dati vengono copiati in un
buffer di rimbalzo in SRAM da 1 KB (il workspace serializzato, grande, sta in
PSRAM sulla Plus e non sarebbe leggibile con l'XIP fermo), si cancella un
settore alla volta e la configurazione M1 della PSRAM viene verificata prima
di riprendere l'XIP ([PICO_PLUS_2W.md](PICO_PLUS_2W.md#5-flash-partizioni-e-protezioni)).

**Capacità reale del chip.** Prima di creare le regioni `board::init()` legge
il JEDEC ID della flash: se il chip è più piccolo della mappa della build (UF2
della Plus copiato su un Pico 2 W) le regioni oltre il chip vengono
disattivate, perché gli indirizzi oltre i 4 MB ricadrebbero sul firmware e sui
dati del Pico 2 W. Il workspace non viene salvato ("Salvataggio non
disponibile"), l'avvio indica l'UF2 da installare e i dati già presenti
restano intatti. Un ID non riconosciuto lascia la mappa della build.

## Workspace in flash

Due slot con CRC-32 e numero di sequenza: si scrive sempre lo slot più
vecchio, quindi un'interruzione durante la scrittura lascia valido l'altro
(test di interruzione in ogni punto in `storage_test`). Il record è alla
**versione 5** (v1.9.0): aggiunge in coda l'archivio scelto per i file e lo
stato del 3D Graph (Z1–Z3, selezione, finestra 3D, griglia, vista). La
versione 3 (GRAPH v1.8.0) aggiunge le 20 funzioni di GRAPH con stile e Yt, il
Setup della modalità, le 6 memorie V-Window, le 20 memorie grafiche, gli
elementi SKETCH, l'immagine richiamata e il progetto aperto; la versione 4
(TABLE v1.8.0) la variabile, il tipo e le righe della tabella (≤ 999). I record
v1 (v1.6.0), v2 (v1.7.0), v3 e v4 (v1.8.0) sono letti: un record v4 generato
dai sorgenti della v1.8.0 è nei test (`tests/fixtures/workspace_v1_8_0.inc`);
le 6 funzioni della v1.6.0 diventano Y1–Y6 e la griglia booleana diventa Grid
On/Off; una V-Window che non supera i controlli viene corretta o sostituita da
quella iniziale, senza scartare il resto dei dati.

| Limite | Pico 2 W | Pico Plus 2 W |
|---|---|---|
| Capacità di un salvataggio | 59,75 KB | 511,75 KB |
| Cronologia Run-Matrix | 40 righe | 200 righe |
| Liste statistiche | 6 × 255 | 6 × 999 (fx-CG50) |
| Conversazione AI | 30 messaggi | 30 messaggi |

I tratti PEN dello SKETCH hanno un limite complessivo di 1200 punti; con 20
funzioni, 20 memorie grafiche piene e 64 elementi SKETCH lo stato GRAPH resta
sotto 60 KB (`graph_app_test`). Se il record supera comunque la capacità,
l'autosalvataggio mostra "Memoria flash piena: N KB > M KB (cancella memorie
grafiche, SKETCH, chat)". Se manca la memoria per serializzarlo compare
"Memory ERROR: salvataggio non riuscito" (prima della v1.9.0 l'eccezione
usciva dal ciclo principale).

## LittleFS sulla flash interna (v1.9.0)

`fx::files::LittleFsFileSystem` implementa `files::FileSystem` su LittleFS
v2.11.3 (blocchi da 4 KB, programmazione a pagine da 256 byte, cache 512 byte,
`block_cycles` 500 per il livellamento dell'usura):

- montato solo alla prima operazione (nessuna scrittura a ogni avvio);
  partizione vuota o estranea → formattata (non contiene altro);
- nomi confrontati senza distinguere maiuscole/minuscole ma conservati, come su
  FAT; `rename` rifiuta una destinazione esistente e `remove` una cartella non
  vuota, come il backend FatFs, così `writeAtomic()`/`recover()` funzionano
  uguali sulle due memorie;
- `internal_fs_test`: contratto completo, partizione piena, persistenza dopo lo
  smontaggio e **interruzione di alimentazione** in decine di punti di un
  salvataggio atomico: dopo il riavvio il file è sempre quello vecchio o quello
  nuovo, mai un misto, e non restano `.tmp`/`.bak`;
- DIAG la prova come la microSD (righe `INT`).

## Pila della microSD

```
GraphApp (graph_files.cpp)          progetti, PICT, sfondo, file manager
  └─ files::FileSystem               interfaccia: list, stat, openRead, openWrite,
     │                               remove, rename, makeDirs, freeSpace, status
     ├─ files::RamFileSystem         host: scheda in RAM con iniezione di guasti
     └─ files::FatFsFileSystem       FatFs R0.16 (third_party/fatfs)
          └─ sd::BlockDevice         settori da 512 byte
               ├─ sd::RamDisk        host: disco in RAM
               └─ sd::SdCard         protocollo SD in modalità SPI
                    └─ sd::SpiBus    trasporto
                         ├─ PicoSdBus   firmware: SPI0 del RP2350
                         └─ SimCard     host (sd_fatfs_test): scheda simulata
```

Ogni livello ha un'implementazione host, quindi il percorso completo
GraphApp → FatFs → protocollo SD è eseguito nei test senza hardware. Solo
`PicoSdBus` (poche righe sulle API `hardware_spi`/`hardware_gpio` del Pico SDK)
non è provato.

### Interfaccia `files::FileSystem`

Modellata sui servizi di FatFs, con esiti espliciti (`files::Status`):

| Esito | Significato / messaggio |
|---|---|
| `NoCard` | scheda assente o estratta ("Storage ERROR: scheda microSD assente") |
| `NotFound`, `Exists` | file o cartella mancante / già presente |
| `Full` | volume o directory pieni, anche a metà scrittura |
| `IoError` | errore di lettura/scrittura del dispositivo |
| `Corrupt` | file non valido |
| `Unformatted` | nessun volume FAT/exFAT sulla scheda ("scheda non formattata o file system non supportato: formattarla in FAT32 o exFAT dal computer") |
| `InvalidName`, `TooLarge`, `NotSupported` | nome non valido, file oltre i limiti, operazione non disponibile |

Operazioni di più alto livello (`file_store.cpp`): `readAll` con limite di
dimensione, `writeAtomic` (scrittura su `.tmp`, verifica del CRC, `.bak`,
rinomina) e `recover` per completare o annullare un salvataggio interrotto
(dettagli in [PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md#scrittura-atomica-e-recupero)).

### FatFs

FatFs R0.16 di ChaN, senza modifiche (provenienza e licenza in
`third_party/fatfs/README.md`), configurato in `third_party/fatfs/ffconf.h`:

| Opzione | Valore | Motivo |
|---|---|---|
| `FF_USE_LFN` | 1 (buffer statico) | estensioni lunghe (`.pcgraph`, `.pcpict`) |
| `FF_CODE_PAGE` | 437 | nomi ASCII |
| `FF_FS_EXFAT` | 1 | le microSD da 64 GB e oltre (SDXC) sono vendute in exFAT |
| `FF_LBA64` | 1 | settori a 64 bit e partizioni GPT (ammesso solo insieme a exFAT); le SD fino a 2 TB non ne hanno bisogno |
| `FF_FS_TINY` | 1 | un solo buffer di settore condiviso dai file (RAM) |
| `FF_FS_NORTC` | 1 | la PicoCalc non ha un orologio: data fissa 1/1/2026 |
| `FF_USE_MKFS` | solo host | i test formattano un disco in RAM; il firmware non formatta schede |

Le funzioni di I/O (`disk_status`, `disk_initialize`, `disk_read`,
`disk_write`, `disk_ioctl`) sono in `src/fatfs_store.cpp` e usano il
`BlockDevice` montato. Ogni operazione controlla la presenza della scheda: una
scheda estratta e reinserita viene reinizializzata e rimontata.

**Recupero dagli errori** (corretto nel collaudo della v1.8.0): se la scheda
viene estratta e reinserita *tra* due operazioni, il contatto del card detect
può non essere mai letto aperto; la scheda ha perso alimentazione e non
risponde più in modalità SPI. Prima la scheda restava "pronta" e ogni
operazione successiva falliva. Ora:

1. ogni comando senza risposta o con token di errore marca la scheda come non
   pronta (`SdCard::fail` → `reset`);
2. `FatFsFileSystem` esegue ogni operazione con `withVolume`: dopo un errore
   di I/O con la scheda non più pronta la reinizializza (CMD0…), rimonta il
   volume e ripete l'operazione **una volta**;
3. `files::readAll` ripete una volta la lettura completa dopo un errore di
   lettura transitorio (leggere non ha effetti collaterali).

Le scritture non vengono ripetute automaticamente: il salvataggio atomico
lascia intatto il file precedente e l'errore viene mostrato.

### Protocollo SD (SPI)

`sd::SdCard` segue la *SD Physical Layer Simplified Specification*:

1. ≥ 74 clock con CS alto a 400 kHz;
2. CMD0 (stato idle), CMD8 (tensione, distingue SD v2 da v1);
3. ACMD41 ripetuto fino a 1 s (con HCS per le v2), CMD58 per il bit CCS
   (indirizzamento a blocchi: SDHC/SDXC);
4. CMD16 (blocchi da 512 byte) per le schede a byte, CMD9 per la capacità
   (CSD v1 e v2);
5. clock a 12,5 MHz;
6. lettura CMD17 con attesa del token di dati, scrittura CMD24 con token,
   risposta di accettazione, attesa di fine programmazione e CMD13 (stato).

Timeout: 500 ms per le attese di pronto e di dati, 1 s per l'inizializzazione.
Schede MMC e accesso multiblocco (CMD18/CMD25) non sono implementati: ogni
settore è un comando separato (più lento ma sufficiente per file di qualche KB).

### PicoCalc: bus SPI0

Pin presi dai sorgenti ufficiali ClockworkPi (`Code/MP3Player/config.h`,
`Code/pico_multi_booter/boot.c`):

| Segnale | GPIO |
|---|---|
| SCK | GP18 |
| MOSI | GP19 |
| MISO | GP16 (pull-up) |
| CS | GP17 |
| Card detect | GP22 (basso = scheda inserita, pull-up interno) |

Pin, polarità del card detect e pull-up verificati sui sorgenti ufficiali
(`pico_multi_booter/boot.c`: `!gpio_get(SD_DET_PIN)` con pull-up); il loro
`sd_boot` usa ~15,6 MHz, il firmware 12,5 MHz.

**Da collaudare su PicoCalc fisica** (procedura in
[HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)): inizializzazione con
schede reali (SDHC, SDXC exFAT), velocità, rilevamento dell'inserimento,
estrazione durante un salvataggio.

## Diagnostica sul dispositivo (SYSTEM → F2 DIAG)

`src/diagnostics.cpp` esegue sul dispositivo, senza toccare i dati
dell'utente: stato e descrizione della scheda (tipo SDSC/SDHC, capacità, FAT12/
16/32 o exFAT, cluster), tempo di inizializzazione e montaggio, spazio libero,
scrittura atomica di 64 KB con verifica CRC e velocità, lettura e confronto
byte per byte, salvataggio e rilettura di un progetto `.pcgraph`, rimozione
della cartella di prova `PICOCALC/DIAG`; poi i tempi di GRAPH. Ogni riga è
mostrata sullo schermo e stampata sulla seriale USB come `[DIAG] OK|FAIL ...`.

## Immagini disco sul PC

- `board::host::FileDisk` (`src/platform/host/file_disk.*`): un `BlockDevice`
  su un file immagine.
- Simulatore: `PICOCALC_SD_IMAGE=scheda.img build/host/picocalc_sim ...` usa
  un'immagine FAT/exFAT (anche la copia di una scheda reale, es. con `dd`)
  tramite lo stesso FatFs del firmware.
- `build/host/sd_image_tool <immagine> put|get|ls|rm|mkdir|project|check|recover ...`
  scrive e legge file con il codice del firmware (`tools/sd_image_tool.cpp`);
  `sd_image_tool - sample FILE.pcgraph` crea un progetto di esempio.

## Test

| Suite | Cosa prova |
|---|---|
| `storage_test` | slot A/B, CRC, interruzione di alimentazione in ogni punto, round trip del record v4 |
| `run_matrix_test` | lettura del record v1.6.0 (fixture), V-Window legacy corretta senza perdere i dati |
| `graph_file_test` | codifica/decodifica di progetti, memorie e immagini; file danneggiati, troncati, di versione futura; `writeAtomic` e `recover` con guasti iniettati in ogni operazione (scrittura parziale, rinomina, rimozione, scheda estratta) |
| `sd_fatfs_test` | protocollo SD con schede simulate SDHC, SDv2 e SDv1 (CSD v1/v2, token, busy), formattazione FAT, nomi lunghi, salvataggio atomico, estrazione e reinserimento, scheda assente; scheda non formattata, scheda spenta e riaccesa tra due operazioni, errori di lettura permanenti e transitori, scritture rifiutate; dimensioni 0…300 000 byte ai bordi di settore e cluster, cartelle annidate, 150 file in una cartella, rinomina di sole maiuscole, nomi non validi, scheda piena con il file precedente intatto (302 controlli) |
| `graph_app_test` | progetti dal file manager: NEW, OPEN, SAVE, SAVE AS, RENAME, DELETE, scheda assente, scheda piena, ciclo completo 12.4 su `RamFileSystem` e sull'intera pila GraphApp → FatFs → protocollo SD → scheda simulata (con riavvio del firmware e scheda spenta/riaccesa), scheda non formattata, SYSTEM > DIAG |
| `sd_interop_test` | il codice del firmware scrive immagini FAT16, FAT32 ed exFAT create da `mkfs.fat`/`mkfs.exfat`; `fsck.fat`/`fsck.exfat` le trovano pulite; `mtools` rilegge i file byte per byte e scrive file (anche con nomi lunghi) che il firmware legge (41 controlli; saltato se gli strumenti non sono installati) |
