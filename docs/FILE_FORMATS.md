# Formati dei file delle modalità — v1.16.0

I file dell'utente stanno sulla microSD (FatFs) o nella flash interna
(LittleFS), scelta in SYSTEM → «Archivio file». Dalla v1.15.0 ogni modalità
che salva documenti usa la tabella dei **file tipizzati**
(`include/typed_files.hpp`, `src/typed_files.cpp`) e l'API comune
`files::TypedStore`: elenco, apri, salva, salva come (con conferma di
sovrascrittura), rinomina, elimina. Ogni scrittura passa da
`files::writeAtomic` (file temporaneo verificato con CRC-32, poi sostituzione;
dopo un'interruzione resta il file vecchio o il nuovo, mai un misto). Gli altri
formati (progetti `.pcgraph`, immagini `.pcpict`, CSV) sono descritti in
[PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md) e [STORAGE.md](STORAGE.md).

## Tabella dei tipi

| Tipo | Cartella | Estensioni | Formato | Nomi | Dati per file | Schede |
|---|---|---|---|---|---|---|
| eActivity | `PICOCALC/EACT` | `.pceact` | binario con intestazione | 1–8 caratteri `A–Z 0–9 { } ' ~` (manuale 10-5; le minuscole digitate diventano maiuscole) | 24 KB sul Pico 2 W, 256 KB sulla Plus | entrambe |
| Program | `PICOCALC/PROG` | `.cpp`, `.c`, `.cs` | testo UTF-8 | 1–8 caratteri `A–Z a–z 0–9 _ -` | 256 KB | Pico Plus 2 W |
| Spreadsheet | `PICOCALC/SHEET` | `.sheet` | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |
| Python | `PICOCALC/PY` | `.py` | testo UTF-8 | come Program | 256 KB | Pico Plus 2 W |
| Geometry | `PICOCALC/GEOM` | `.pcgeo` | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |
| Picture Plot | `PICOCALC/PPLOT` | `.pcplot` | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |

Un tipo esiste su una scheda solo se c'è anche la sua modalità
(`FXAI_MODE_<NOME>`, [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md)):
sul Pico 2 W solo eActivity. MEMORY → F3 FILES mostra i file per tipo con il
numero e lo spazio usato, e li elimina con conferma.

## Intestazione dei file binari

16 byte, little endian, seguiti dai dati:

| Offset | Campo | Valore |
|---|---|---|
| 0 | `u32` magia | `PCEA` eActivity, `PCSH` Spreadsheet, `PCGO` Geometry, `PCPP` Picture Plot |
| 4 | `u16` versione del formato | 1 nella v1.15.0 |
| 6 | `u16` flag | 0 |
| 8 | `u32` lunghezza dei dati | = dimensione del file − 16 |
| 12 | `u32` CRC-32 dei dati | `storage::crc32` (polinomio 0xEDB88320) |

Lettura (`files::decodeTypedFile`): magia sbagliata, flag non nulli, lunghezza
diversa dal file o CRC errato → `Corrupt` («file corrotto»); versione più
nuova di quella del firmware → `NotSupported`; oltre il limite della scheda →
`TooLarge`. Il contenuto dei dati è definito dalla modalità (eActivity dalla
v1.16.0, sotto; le altre nelle loro versioni).

## Documenti eActivity (`.pceact`, dati versione 1, v1.16.0)

Dati dopo l'intestazione `PCEA` (nucleo `include/eactivity.hpp`,
`src/eactivity.cpp`; interi little endian; `str` = `u16` lunghezza + byte
UTF-8; `blob` = `u32` lunghezza + byte):

| Campo | Contenuto |
|---|---|
| `u16` versione | 1 (una versione più nuova non si apre: «Data ERROR») |
| `u16` righe | 0…2000 |
| righe | per ogni riga i campi sotto |
| `blob` memorie | memorie Mat A–Z, Vct A–Z, List 1–26 **del documento** (manuale 10-9): codifica a parti del workspace (`encodeParts`, parti Matrices e Lists); vuoto = memorie vuote |

Ogni riga:

| Campo | Contenuto |
|---|---|
| `u8` tipo | 0 testo, 1 calcolo, 2 stop, 3 immagine, 4 striscia |
| `str` testo | testo (≤ 255 byte), espressione (testo lineare canonico del parser), nome del file dell'immagine, titolo della striscia (≤ 16 byte); vuoto per lo stop |
| attributi (testo e calcolo) | `u16` sequenze, ciascuna `u8` attributo + `u16` lunghezza in byte del testo; attributo = colore del testo (bit 0–3: 0 nero, 1 blu, 2 rosso, 3 magenta, 4 verde, 5 ciano, 6 giallo) + marcatore (bit 4–7: 0 nessuno, 1 magenta, 2 verde, 3 ciano, 4 giallo) |
| `u8` memo | 0/1; se 1: `u8` colore (0–6) e `str` testo (≤ 255 byte). Solo su righe di testo e di calcolo (10-11) |
| calcolo | `blob` risultato: una voce di cronologia di Run-Matrix (`encodeHistoryEntries`), vuoto se non calcolata |
| immagine | `u8` stile del bordo (0 Normal, 1 Thick, 2 Thin), `u8` colore del bordo (1–8 Black…White, `0xFF` Clear) |
| striscia | `u8` tipo (0 RUN … 23 Picture Plot, tabella del manuale 10-17; i tipi sconosciuti si conservano), `blob` dati: parti del workspace della modalità (`encodeParts`) o testo UTF-8 di Notes (righe separate da `\n`) |

Le **espressioni Math nelle righe di testo** sono testo lineare canonico tra
U+F8F0 e U+F8F1 (caratteri di uso privato che il font e il parser non usano;
U+E000–E002 sono già i simboli statistici x̂ x̄ ȳ). Il file intero deve stare
nel limite del documento aperto della scheda (`limits::kEActivityDocumentBytes`:
24 KB sul Raspberry Pi Pico 2 W, 256 KB sulla Pimoroni Pico Plus 2 W); oltre,
l'apertura dà «Memory ERROR» e il file resta com'è. Le immagini **non** sono
nel documento: la riga contiene il nome di un file `.pcpict` delle cartelle
`PICOCALC/PICT` o `PICOCALC/CAPT`.

Parti del workspace (`encodeParts`): `u8` numero di parti, poi per ogni parte
`u8` id (1 History, 2 Graph, 3 Table, 4 Lists, 5 Equation, 6 Matrices,
7 Graph3D) e `blob` dati nella codifica della stessa sezione del record del
workspace ([STORAGE.md](STORAGE.md)). La decodifica è tutto-o-niente.

## File di testo

I sorgenti di Program (C, C++, C#; il dialetto viene dall'estensione) e gli
script Python non hanno intestazione, così si aprono e si modificano anche sul
PC. Devono essere UTF-8 valido (niente forme troppo lunghe né surrogati) senza
caratteri NUL; `\r\n` dei file scritti su Windows è accettato dall'editor.

## Ricerca

SEARCH (manuale 10-5) confronta il testo con l'**inizio** del nome, senza
distinguere le maiuscole: «IT» trova ITXX, ITABC, IT123, non XXIT
(`files::searchPrefix`).
