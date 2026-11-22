# Formati dei file delle modalità — v1.15.0

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
v1.16.0, le altre nelle loro versioni).

## File di testo

I sorgenti di Program (C, C++, C#; il dialetto viene dall'estensione) e gli
script Python non hanno intestazione, così si aprono e si modificano anche sul
PC. Devono essere UTF-8 valido (niente forme troppo lunghe né surrogati) senza
caratteri NUL; `\r\n` dei file scritti su Windows è accettato dall'editor.

## Ricerca

SEARCH (manuale 10-5) confronta il testo con l'**inizio** del nome, senza
distinguere le maiuscole: «IT» trova ITXX, ITABC, IT123, non XXIT
(`files::searchPrefix`).
