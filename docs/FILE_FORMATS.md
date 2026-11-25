# Formati dei file delle modalità — v1.17.0

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
| Program | `PICOCALC/PROG` | `.pcprog` (v1.17.0), `.cpp`, `.c`, `.cs` | testo UTF-8 | 1–8 caratteri `A–Z a–z 0–9 _ -` | 256 KB | Pico Plus 2 W; **dalla v1.20.0 solo letti per la migrazione**: i programmi sono sorgenti standard in `PICOCALC/CODE/<LING>` ([STORAGE.md](STORAGE.md#programmi-e-librerie-sulla-microsd-v1200)) |
| Spreadsheet | `PICOCALC/SHEET` | `.pcsheet` (v1.17.0; `.sheet` nella tabella della v1.16.0, mai scritto) | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |
| Python | `PICOCALC/PY` | `.py` | testo UTF-8 | come Program | 256 KB | Pico Plus 2 W |
| Geometry | `PICOCALC/GEOM` | `.pcgeo` | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |
| Picture Plot | `PICOCALC/PPLOT` | `.pcplot` | binario con intestazione | come Program | 1 MB | Pico Plus 2 W |
| Dyna Graph (v1.17.0) | `PICOCALC/DYNA` | `.pcdyna` | binario con intestazione | come Program | 64 KB | Pico Plus 2 W |
| Recursion (v1.17.0) | `PICOCALC/RECUR` | `.pcrecur` | binario con intestazione | come Program | 64 KB | Pico Plus 2 W |
| Conic Graphs (v1.17.0) | `PICOCALC/CONIC` | `.pcconic` | binario con intestazione | come Program | 64 KB | Pico Plus 2 W |
| Financial (v1.17.0) | `PICOCALC/FIN` | `.pcfin` | binario con intestazione | come Program | 64 KB | Pico Plus 2 W |
| Link (v1.17.0) | `PICOCALC/LINK` | `.pclink` | binario con intestazione | come Program | 8 MB | Pico Plus 2 W |

Un tipo esiste su una scheda solo se c'è anche la sua modalità
(`FXAI_MODE_<NOME>`, [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md)):
sul Pico 2 W solo eActivity (la tabella del Pico 2 W è rimasta quella della
v1.16.0). MEMORY → F3 FILES mostra i file per tipo con il
numero e lo spazio usato, e li elimina con conferma.

## Intestazione dei file binari

16 byte, little endian, seguiti dai dati:

| Offset | Campo | Valore |
|---|---|---|
| 0 | `u32` magia | `PCEA` eActivity, `PCSH` Spreadsheet, `PCGO` Geometry, `PCPP` Picture Plot; v1.17.0: `PCDY` Dyna Graph, `PCRC` Recursion, `PCCN` Conic Graphs, `PCFN` Financial, `PCLK` Link |
| 4 | `u16` versione del formato | 1 nella v1.15.0 |
| 6 | `u16` flag | 0 |
| 8 | `u32` lunghezza dei dati | = dimensione del file − 16 |
| 12 | `u32` CRC-32 dei dati | `storage::crc32` (polinomio 0xEDB88320) |

Lettura (`files::decodeTypedFile`): magia sbagliata, flag non nulli, lunghezza
diversa dal file o CRC errato → `Corrupt` («file corrotto»); versione più
nuova di quella del firmware → `NotSupported`; oltre il limite della scheda →
`TooLarge`. Il contenuto dei dati è definito dalla modalità (eActivity dalla
v1.16.0, le modalità Base dalla v1.17.0: sotto).

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
| `str` testo | testo (≤ 255 byte, senza caratteri di controllo: l'a capo separa le righe), espressione (testo lineare canonico del parser), nome del file dell'immagine, titolo della striscia (≤ 16 byte); vuoto per lo stop |
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

## Dati delle modalità Base (v1.17.0, solo Pimoroni Pico Plus 2 W)

Dopo l'intestazione di 16 byte. Codifica comune `include/mode_codec.hpp`:
interi little endian, `f64` = double IEEE 754 in 8 byte little endian, `str` =
`u16` lunghezza + byte UTF-8, `blob` = `u32` lunghezza + byte. Ogni lettura
rifiuta i byte in più, i valori fuori dai limiti e i numeri non finiti
(«Data ERROR: file … danneggiato», il file resta com'è). La **sezione** di una
modalità è la stessa codifica che il record v12 usa per `0x0100 + AppId`
([STORAGE.md](STORAGE.md)).

| Tipo | Dati |
|---|---|
| `.pcsheet` (`PCSH`) | `u8` versione 1, `u32` numero di celle (≤ 26 × 999), per ogni cella in ordine crescente di chiave `u16` chiave (= riga × 32 + colonna, colonna 0 = A, riga 1…999) e `str` testo digitato (≤ 255 byte, non vuoto: numero, testo o formula che inizia con `=`). I valori non si salvano: si ricalcolano all'apertura |
| `.pcdyna` (`PCDY`) | `blob` sezione di Dyna Graph (`u8` versione 1, `str` funzione, `u8` coefficiente A–Z, `u8` Cont/Stop, `u8` velocità 0–2) e `f64` Start, End, Pitch (le variabili di VARS › DYNA) |
| `.pcrecur` (`PCRC`) | `blob` sezione di Recursion (`u8` versione 1, `u8` tipo an+1/an+2, `str` formula an+2, `u8` Σan), `str` formula an+1 (la variabile di modo della v1.13.0) e `f64` Start, End, a0, a1 |
| `.pcconic` (`PCCN`) | la sezione di Conic Graphs: `u8` versione 1, `u8` modello (0–5), `f64` A, B, H, K, R |
| `.pcfin` (`PCFN`) | `blob` sezione di Financial (`u8` versione 1, `u8` BGN/END, `f64` n, I%, PV di SMPL, I% di CASH, PM1, PM2, n e I% di CNVT, `f64` sei campi delle date, `u16` flussi di cassa ≤ 999 e `f64` per flusso) e `f64` n, I%, PV, PMT, FV, P/Y, C/Y (le variabili di VARS › TVM) |
| `.pcgeo` (`PCGO`) | **v1.19.0: `u8` versione 2** (sotto). La **versione 1** della v1.17.0 si legge ancora e si migra: `u16` oggetti (≤ 500), per ognuno `u8` tipo (0 punto, 1 segmento, 2 retta, 3 circonferenza, 4 poligono), `u8` punti, `f64` x, y per punto; poi `f64` centro x, centro y e scala della vista; ogni coordinata diventa un punto libero e le figure restano legate ai loro punti |
| `.pcplot` (`PCPP`) | `u8` versione 1, `str` file dello sfondo (`.pcpict` di `PICOCALC/PICT`), `f64` Xmin, Xmax, Ymin, Ymax, `u8` regressione + 1 (0 = nessuna, 1–9), `u16` punti (≤ 999), `f64` x, y per punto |
| `.pclink` (`PCLK`) | un **pacchetto** di Link (sotto) |

### Costruzioni di Geometry (`.pcgeo` versione 2, v1.19.0)

Stessa codifica della sezione `0x0110` del record, senza il nome del file
aperto. Interi little endian, `f64` IEEE 754, `str` = `u16` lunghezza + byte.

| Campo | Contenuto |
|---|---|
| `u8` | versione **2** |
| `u16` oggetti (≤ 1 000 con la PSRAM, 250 senza) | per ognuno, **in ordine di id crescente**: `u16` id; `u8` tipo (0 punto, 1 segmento, 2 retta, 3 semiretta, 4 vettore, 5 circonferenza, 6 arco, 7 poligono, 8 funzione, 9 testo, 10 misura incollata, 11 espressione, 12 trasformazione); `u8` definizione (0 libero, 1 su un oggetto, 2 punto medio, 3 intersezione, 4 immagine, 5 vertice del rettangolo, 6 del quadrato, 7 apice dell'isoscele, 8 vertice dell'n-gono, 9 fine dell'arco, 10 per due punti, 11 asse, 12 perpendicolare, 13 parallela, 14 bisettrice, 15 tangente, 16 immagine di una curva, 17 circonferenza per centro e punto, 18 arco, 19 semicirconferenza, 20 arco per tre punti, 21 poligono, 22 f(x), 23 testo, 24 misura, 25 espressione, 26 riflessione, 27 traslazione, 28 traslazione col vettore, 29 rotazione, 30 dilatazione, 31 simmetria); `u8` flag e `u8` flag2 (ramo, verso, n e k, tipo di misura, segno d'angolo); `u8` genitori (≤ 50) e `u16` id di ognuno (**minori** dell'id dell'oggetto); `u8` scalari (≤ 4) e `f64` di ognuno; `str` etichetta (≤ 14); `str` testo (≤ 128: testo, f(x), espressione); stile `u8` colore della linea, tratto, area (255 = Clear), Paint Style, colore del testo; `u8` bit (1 nascosto, 2 etichetta visibile); `u16` figura; `u16` ordine di disegno (con segno); `u8` Number Format (255 = quello delle opzioni); `u16` numero `@n` |
| `u8` blocchi (≤ 64) | `u8` tipo di misura, `u8` operandi (1–3) e `u16` id; `f64` valore 1, valore 2 (angoli in radianti), tipo di tangenza (0 esterna, 1 interna) |
| `u8` animazioni (≤ 16) | `u16` punto, `u16` percorso, `u8` Steps (2–100), `f64` t0, t1, `u16` Times |
| `u16` punti con la traccia | `u16` id crescenti |
| `u8` colonne della tabella (≤ 26) | misura come nei blocchi, `u8` componente (0 x, 1 y), `str` nome (≤ 14) |
| `u16` righe (≤ 100) | `f64` per cella, riga per riga (`1e300` = cella indefinita) |
| opzioni | `u8` Number Format (0–9 Fix, 10–19 Sci1–10, 20 Norm), `u8` angolo (0 Deg, 1 Rad), `u8` unità di lunghezza (0–8), `u8` griglia (0 Off, 1 On, 2 Line), `f64` passo (0,01–1000), `u8` assi (0 Off, 1 On, 2 Scale) |
| vista | `f64` centro x, centro y, scala (pixel per unità) |
| sfondo | `str` file `.pcpict` di `PICOCALC/PICT` (≤ 64), `u8` Fade (0–100) |
| contatori | `u16` prossimo id, prossima figura, prossima etichetta, prossimo `@n` |

La lettura ha la **garanzia forte**: valori finiti, id crescenti, genitori
esistenti con id minore e del tipo giusto per la definizione, limiti rispettati;
altrimenti il file è rifiutato («Data ERROR») e lo stato non cambia. La
geometria si ricalcola dopo la lettura (non è salvata).

### Pacchetti di Link (`.pclink`)

`u8` versione 1, `u16` voci (≤ 256), per ogni voce `u8` tipo (0 workspace,
1 file), `str` percorso (vuoto per il workspace; per un file
`PICOCALC/<cartella>/<nome>`, ≤ 96 byte, solo `A–Z a–z 0–9 / . _ - { } ' ~`,
senza `..` né `//`) e `blob` contenuto (il record del workspace come lo scrive
`serializeWorkspace`, o il file com'è). Al più un workspace per pacchetto;
pacchetto ≤ 8 MB. APPLY scrive i file con `files::writeAtomic` e sostituisce il
workspace solo dopo una conferma e solo se il record si legge.

### Trame di Link sulla USB seriale

Il trasferimento porta un pacchetto diviso in trame (little endian):

| Campo | Contenuto |
|---|---|
| `u8 u8` sincronismo | `0x7E` `'P'` |
| `u8` tipo | 2 BEGIN, 3 DATA, 4 END, 5 ACK, 6 NAK, 7 ABORT |
| `u16` seq | 0 per BEGIN, 1…n per DATA, n+1 per END; ACK/NAK ripetono il seq confermato |
| `u16` lunghezza | ≤ 1024 |
| dati | BEGIN: `u32` dimensione del pacchetto, `u32` CRC-32 del pacchetto, `u8` lunghezza e nome (≤ 64 byte); DATA: un pezzo del pacchetto; ABORT: il motivo in testo |
| `u32` CRC-32 | di tipo, seq, lunghezza e dati |

Stop-and-wait: ogni trama aspetta il suo ACK per 1 s, al più 5 volte, poi
«Time Out»; la ricezione ferma per 15 s dà «Time Out»; END è confermato solo
se il CRC dell'intero pacchetto torna, altrimenti NAK. AC manda ABORT
(«Break»). I byte che non formano una trama valida (righe di log sulla stessa
porta, rumore) vengono scartati e il ricevitore si risincronizza sul prossimo
`0x7E 'P'`. Lo stesso formato è scritto in Python in `tools/picocalc_link.py`
([LINK.md](LINK.md)).

## Statistics e Distribution (v1.18.0, solo Pimoroni Pico Plus 2 W)

Nessun tipo di file nuovo: lo stato di Statistics (StatGraph, SET, SET UP,
colori e formule delle liste, risultati) è nella sezione `0x0101` del record
v12 e quello di Distribution nella sezione `0x0117`
([STORAGE.md](STORAGE.md)); le liste continuano a uscire e a entrare in CSV
(`PICOCALC/CSV`) e i risultati vanno nelle liste con Save Res.

## File di testo

I sorgenti di Program (C, C++, C#) e gli script Python non hanno intestazione
binaria, così si aprono e si modificano anche sul PC. Nei file `.pcprog`
(v1.17.0) la prima riga dice il dialetto: `// pcprog: C`, `// pcprog: C++` o
`// pcprog: C#` (un commento valido nei tre linguaggi; tolto all'apertura e
rimesso al salvataggio; una riga diversa resta nel testo e vale il C++); nei
`.c`, `.cpp` e `.cs` il dialetto viene dall'estensione. Devono essere UTF-8 valido (niente forme troppo lunghe né surrogati) senza
caratteri NUL; `\r\n` dei file scritti su Windows è accettato dall'editor.

## Ricerca

SEARCH (manuale 10-5) confronta il testo con l'**inizio** del nome, senza
distinguere le maiuscole: «IT» trova ITXX, ITABC, IT123, non XXIT
(`files::searchPrefix`).
