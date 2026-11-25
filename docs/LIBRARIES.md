# Librerie dei programmi (v1.20.0)

Solo **Pimoroni Pico Plus 2 W**. Una libreria è codice sorgente C, C++ o C#
(Java dalla v1.21.0, Python dalla v1.22.0) che i programmi di
[Program](PROGRAM.md) usano con `#include <nome.h>` o `using Namespace;`.
Si installa da un **indice** su Internet (Wi-Fi + HTTPS), da una cartella
copiata dal PC o con `tools/picocalc_link.py send-library`; ogni file è
controllato con **SHA-256** e l'installazione è **atomica**. Progetto:
[reports/ANALISI_FONTI_v1.20.0.md §6](reports/ANALISI_FONTI_v1.20.0.md);
scelte: [reports/SVILUPPO_v1.20.0.md](reports/SVILUPPO_v1.20.0.md).

## 1. Sulla microSD

```
PICOCALC/LIB/<LING>/<nome>/library.json   una libreria installata (LING = C, CPP, CS)
PICOCALC/LIB/<LING>/<nome>/...            i suoi file (sorgenti, intestazioni)
PICOCALC/LIB/IMPORT/                      repository copiato dal PC (index.json + cartelle) o ricevuto da Link
PICOCALC/LIB/.cache/index.json            ultimo indice scaricato (usato senza rete)
PICOCALC/LIB/.tmp/                        scaricamenti in corso e giornale dell'installazione
PICOCALC/LIBDATA/<LING>/<nome>/           file scritti dal codice della libreria (I/O confinato)
```

Il codice di una libreria gira nello stesso interprete confinato dei
programmi: i file che apre stanno nella sua cartella `LIBDATA`, quelli del
programma nella cartella del programma.

## 2. Il manifest `library.json`

```json
{
  "name": "trasformazioni",
  "version": "1.0.0",
  "language": "c",
  "description": "Trasformazioni del piano con le matrici omogenee 3x3",
  "author": "PicoCalc FxCG-50",
  "license": "MIT",
  "minFirmware": "1.20.0",
  "dependencies": {"matrici": "^1.0.0"},
  "files": [
    {"path": "trasformazioni.c", "sha256": "…64 cifre esadecimali…", "size": 1534},
    {"path": "trasformazioni.h", "sha256": "…", "size": 612}
  ]
}
```

| Campo | Regola |
|---|---|
| `name` | `a-z 0-9 _ -`, inizia con una lettera, al più 32 caratteri, uguale alla cartella |
| `version` | semver `MAJOR.MINOR.PATCH` |
| `language` | `c`, `cpp`, `cs` (`java`, `py` nelle versioni seguenti) |
| `description`, `author`, `license` | testo (200, 80, 40 caratteri) |
| `namespace` | C#: il nome di `using` (se manca, il nome della libreria) |
| `minFirmware` | firmware minimo; più nuovo di quello installato → la libreria non si installa |
| `dependencies` | al più 16 voci nome → intervallo: `1.2.3`, `=1.2.3`, `^1.2.3` (come npm, anche `^0.x`), `~1.2.3`, `>=`, `>`, `<`, `<=`, `*` e intervalli con lo spazio (`>=1.0.0 <2.0.0`) |
| `files` | al più 64 file, percorsi relativi senza `..`, `\`, nomi nascosti o assoluti; 256 KB per file, 1 MB per libreria; ogni file con SHA-256 e dimensione |

`tools/libraries.py update` calcola `files` e l'indice; `check` controlla che
siano aggiornati (lo fa anche il test `libraries_index_test`); `manifest
CARTELLA` stampa il manifest di una libreria.

## 3. L'indice `index.json`

```json
{
  "version": 1,
  "libraries": [
    {"name": "matrici", "version": "1.0.0", "language": "c", "path": "c/matrici/1.0.0",
     "description": "…", "manifestSha256": "…", "manifestSize": 559, "minFirmware": "1.20.0", "dependencies": {}}
  ]
}
```

Una voce per versione; `path` è relativo all'indice. L'indirizzo si imposta
in **Program › SET UP › Indice LIB** (predefinito
`https://raw.githubusercontent.com/gabrieleeli/PicoCalc-FxCG-50/main/libraries/index.json`,
che funziona finché il repository è pubblico). Limiti: indice 64 KB, 512
voci, manifest 16 KB, piano di 32 librerie.

## 4. Il menu LIB

Program › pagina 3 › **LIB**. L'elenco mostra le librerie installate del
linguaggio della cartella corrente (di tutti i linguaggi da `CODE`).

| Tasto | Azione |
|---|---|
| F1 **INFO** | manifest: versione, linguaggio, descrizione, autore, licenza, firmware minimo, file e dimensione, cartella |
| F2 **INST** | sceglie una libreria dall'indice (scaricato con il Wi-Fi di SYSTEM, poi tenuto in `.cache` per l'uso senza rete), mostra il **piano** con le dipendenze e chiede conferma |
| F3 **UPD** | aggiorna la libreria scelta all'ultima versione compatibile; una versione **più vecchia** si installa solo dopo la conferma «ATTENZIONE versione più vecchia!» |
| F4 **DEL** | rimuove dopo la conferma, avvisando se altre librerie installate ne dipendono |
| F5 **VERIF** | ricontrolla dimensione e SHA-256 di ogni file |
| pagina 2: F1 **LOCAL** | usa il repository copiato in `PICOCALC/LIB/IMPORT` (stesso procedimento, senza rete) |
| pagina 2: F2 **INDEX** | scarica di nuovo l'indice |

Durante lo scaricamento e l'installazione la barra mostra l'avanzamento e
**AC annulla** in ogni momento (la libreria resta com'era).

## 5. Le tre fonti

1. **Wi-Fi + HTTPS** (mbedTLS del Pico SDK, certificati delle CA pubbliche in
   `certs/library_ca.pem`, nome dell'host verificato, solo `https`, solo la
   risposta 200 senza redirect, `Content-Length` o `chunked`, al più 180 s
   per file e 30 s senza dati): l'indice, poi ogni manifest
   (impronta confrontata con l'indice), poi ogni file in streaming verso la SD
   con SHA-256 e conteggio dei byte mentre arriva (un byte oltre la
   dimensione dichiarata ferma subito lo scaricamento).
2. **Cartella copiata dal PC**: la cartella `libraries/` del repository (o un
   repository proprio con `index.json`) copiata in `PICOCALC/LIB/IMPORT`;
   LIB › LOCAL la installa con gli stessi controlli.
3. **Dal PC con Link**: `tools/picocalc_link.py send-library --port
   /dev/ttyACM0 CARTELLA_LIBRERIA` controlla il manifest e invia la libreria
   in `PICOCALC/LIB/IMPORT`; poi LIB › LOCAL.

## 6. Installazione atomica e sicurezza

- Il **piano** è calcolato dall'indice: per ogni dipendenza la versione più
  alta che soddisfa tutti gli intervalli e il firmware; una libreria già
  installata e compatibile resta. Errori chiari per dipendenza mancante,
  conflitto di intervalli e **ciclo** («ciclo di dipendenze: a → b → a»).
- Ogni libreria del piano si scarica e si verifica in `LIB/.tmp`; solo
  quando **tutte** sono complete un **giornale** scritto in modo atomico
  guida lo scambio delle cartelle. All'ingresso in LIB il recupero completa o
  annulla un'installazione interrotta (AC, rete persa, SD tolta, batteria):
  c'è sempre la versione vecchia o quella nuova, mai una libreria a metà, e
  nessun file resta aperto.
- L'installazione **non esegue codice**: copia file e controlla impronte.
  Percorsi assoluti, `..`, nomi non validi, file troppo grandi, manifest o
  indici malformati sono rifiutati con un messaggio (parser JSON proprio con
  limiti di profondità e dimensione, provato dal fuzzer `fuzz_libjson`).
- A ogni RUN i file delle librerie usate si ricontrollano con SHA-256: un file
  cambiato dopo l'installazione dà «file modificato dopo l'installazione
  (LIB > VERIFY)».

## 7. Le librerie del repository

| Linguaggio | Librerie (1.0.0) | Uso |
|---|---|---|
| C | `matrici`, `statistica`, `trasformazioni` (dipende da `matrici`) | `#include <matrici.h>` |
| C++ | `frazioni`, `geometria2d`, `polinomi` | `#include <frazioni.hpp>` |
| C# | `finanza`, `matrici`, `statistica` | `using PicoCalc.Matrici;` |

Le prova `libraries_test`: ogni libreria si installa dal repository vero
(impronte e dipendenze dell'indice), passa VERIFY e si usa da un programma
con l'uscita di gcc, g++ e Mono; l'esempio
[FigureLibreria.c](examples/program/c/FigureLibreria.c) usa
`trasformazioni`. `lib_manager_test` prova semver, piani, cicli, firmware,
giornale e interruzioni a ogni passo; `https_lib_test` un server HTTPS sul PC
(successo, impronta sbagliata, connessione chiusa a metà, dipendenze mancanti,
ciclo).

## 8. Scrivere una libreria

1. Una cartella `libraries/<ling>/<nome>/<versione>/` con i sorgenti e un
   `library.json` con `name`, `version`, `language`, `description`, `author`,
   `license`, `minFirmware`, `dependencies` (anche `{}`).
2. `tools/libraries.py update libraries` calcola le impronte e l'indice.
3. Una versione pubblicata non cambia più: per una correzione si crea una
   cartella con una versione nuova (`libraries.py check` segnala i file
   cambiati senza aggiornare manifest e indice).
