# Memorizzazione: flash interna e microSD (v1.8.0)

Il firmware usa due memorie con ruoli diversi.

| Memoria | Contenuto | Modulo |
|---|---|---|
| Flash del Pico 2 W (ultimi 128 KB, due slot A/B) | workspace: impostazioni, variabili, Ans, cronologia Run-Matrix, liste, funzioni e Setup di GRAPH, memorie V-Window e grafiche, SKETCH, coefficienti, conversazioni AI | `storage.hpp/.cpp`, `workspace.cpp` |
| microSD della PicoCalc | progetti GRAPH `.pcgraph`, immagini `.pcpict`, esportazioni `.bmp` | `file_store`, `fatfs_store`, `sd_card`, `sd_spi_pico.cpp` |

Il workspace in flash conserva lo stato di lavoro anche senza scheda; la
microSD contiene i file che l'utente salva, apre, rinomina o cancella.

## Workspace in flash

Due slot da 64 KB con CRC-32 e numero di sequenza: si scrive sempre lo slot più
vecchio, quindi un'interruzione durante la scrittura lascia valido l'altro
(test di interruzione in ogni punto in `storage_test`). Il record è alla
**versione 3** (v1.8.0): aggiunge le 20 funzioni di GRAPH con stile e Yt, il
Setup della modalità, le 6 memorie V-Window, le 20 memorie grafiche, gli
elementi SKETCH, l'immagine richiamata e il progetto aperto. I record v1
(v1.6.0) e v2 (v1.7.0) sono letti: le 6 funzioni di allora diventano Y1–Y6 e
la griglia booleana diventa Grid On/Off; una V-Window che non supera i controlli
della v1.8.0 viene corretta (scale positive, intervallo T) o, se non basta,
sostituita da quella iniziale, senza scartare il resto dei dati (test in
`run_matrix_test`).

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
| `Corrupt` | file system non riconosciuto o file non valido |
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

**Da collaudare su PicoCalc fisica**: inizializzazione con schede reali
(SDHC, SDXC exFAT), clock massimo affidabile, rilevamento dell'inserimento.

## Test

| Suite | Cosa prova |
|---|---|
| `storage_test` | slot A/B, CRC, interruzione di alimentazione in ogni punto, round trip del record v3 |
| `run_matrix_test` | lettura del record v1.6.0 (fixture), V-Window legacy corretta senza perdere i dati |
| `graph_file_test` | codifica/decodifica di progetti, memorie e immagini; file danneggiati, troncati, di versione futura; `writeAtomic` e `recover` con guasti iniettati in ogni operazione (scrittura parziale, rinomina, rimozione, scheda estratta) |
| `sd_fatfs_test` | protocollo SD con schede simulate SDHC, SDv2 e SDv1 (CSD v1/v2, token, busy), formattazione FAT su disco RAM, file con nomi lunghi, salvataggio atomico su FatFs, estrazione e reinserimento della scheda, scheda assente |
| `graph_app_test` | progetti dal file manager: NEW, OPEN, SAVE, SAVE AS, RENAME, DELETE, scheda assente, scheda piena, ciclo completo 12.4 |
