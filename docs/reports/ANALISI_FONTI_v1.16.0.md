# Analisi delle fonti — v1.16.0 (M2): eActivity

Scritto **prima** del codice della v1.16.0. Milestone **M2** del piano
([ANALISI_FONTI_v1.15.0.md](ANALISI_FONTI_v1.15.0.md)): eActivity su
**entrambe le schede**; alla fine della milestone il Raspberry Pi Pico 2 W è
**congelato** (le milestone successive non ne cambiano funzioni, schermate o
dimensione oltre le correzioni di errori condivise).

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 Software User's Guide* v3.60, **capitolo 10 «eActivity»** (pagine stampate 10-1…10-23, PDF 401–423), capitolo 11 (cartelle), Appendice α («No MEMO», «Memory ERROR», «Not Found») | solo descrizione del comportamento: menu, tasti, messaggi, limiti |
| 2 | Codice a `main` = `08a4e6a` (v1.15.0) | fondamenta F2–F6, Run-Matrix, editor MAT/VCT, Statistics, Graph, Table, Equation, 3D Graph |

## 2. Struttura del capitolo 10

| Pagina | Contenuto |
|---|---|
| 10-1 | elenco file all'ingresso; workspace con righe di testo, di calcolo, di stop, immagine, strisce, barra di scorrimento |
| 10-2 | menu dell'elenco file: OPEN, NEW, DELETE, SEARCH, MEMO (JUMP, EDIT, DETAIL, DELETE, DEL-ALL); 128 KB al primo uso |
| 10-3…10-4 | menu del workspace: FILE (SAVE, SAVE·AS, OPT, CAPACITY), STRIP, JUMP, DEL-LINE, INSERT (TEXT, CALC, STOP, PICTURE), 'MAT/VCT, 'LIST; riga di testo: TEXT, CHAR, A⇔a, MATH, COLOR (MARKER, CHAR), MEMO (INSERT, DELETE, Catalog, VIEW); riga di calcolo: CALC, MATH, COLOR, MEMO; striscia: FILE (… SIZE), CHAR, A⇔a |
| 10-4…10-5 | NEW (nome di 8 caratteri A–Z { } ' ~ 0–9), OPEN, DELETE («Delete eActivity?»), SEARCH (inizio del nome, «Not Found») |
| 10-6 | cursore ↑↓, SHIFT ↑/↓ pagina, JUMP TOP/BOTTOM/PageUp/PageDown |
| 10-6…10-7 | riga di testo: 255 byte, a capo automatico (le espressioni e i comandi non vanno a capo), EXE = a capo, A⇔a, AC cancella la riga visualizzata, input in Math |
| 10-7…10-8 | riga di calcolo: come Run-Matrix in Math, EXE mostra il risultato nella riga seguente, scorrimento ◀▶ senza a capo |
| 10-9 | MAT/VCT e LIST: memorie **separate per file**; LIST con la sola seconda pagina del menu di Statistics, EXIT torna; ricalcolo delle righe seguenti fino a una riga di stop |
| 10-10…10-11 | INSERT › STOP; COLOR › MARKER (magenta, verde, ciano, giallo; Clear toglie) e › CHAR (colore del testo), SET conferma, EXIT annulla; non sul risultato |
| 10-11…10-13 | MEMO: uno per riga (testo o calcolo), icona in alto a destra, INSERT con colore e finestra di 255 byte, Catalog → salto alla riga, VIEW e DETAIL per modificare, DELETE con conferma |
| 10-14…10-16 | immagini (righe immagine): inserimento dalla cartella PICT, selezione con bordo rosso, FORMAT bordo (Line Style Normal/Thick/Thin, Line Color 8 + Clear), DELETE con conferma |
| 10-16…10-21 | strisce: tabella dei 24 tipi, inserimento con titolo di 16 caratteri, EXE apre la modalità con i dati della striscia (indipendenti da quelli del menu principale), barra di stato azzurra, SHIFT ' torna, SHIFT , cambia applicazione, Notes (editor di testo lungo con JUMP, DEL-LINE, INSERT, MATH, CHAR, A⇔a), titolo modificabile, FILE › SIZE, DEL-LINE con conferma; Conics Graph senza dati apre Conics Editor |
| 10-22…10-23 | SAVE, SAVE·AS con «sovrascrivere?» Sì/No; file di circa 29 000 byte; CAPACITY; ritorno all'elenco con «Save?» Sì / No / AC |

## 3. Decisioni di progetto

### 3.1 Documento e file

- File in `PICOCALC/EACT/<NOME>.pceact` (tipo `files::FileType::EActivity`,
  intestazione PCEA con CRC, [FILE_FORMATS.md](../FILE_FORMATS.md)); microSD o
  flash interna secondo SYSTEM.
- Limite del documento aperto: `limits::kEActivityDocumentBytes` (24 KB sul
  Pico 2 W, 256 KB sulla Plus); oltre «Memory ERROR» e il documento resta
  com'era (garanzia forte). CAPACITY mostra byte usati e rimasti.
- Contenuto del file (versione 1): elenco di righe; ogni riga ha tipo (testo,
  calcolo, stop, immagine, striscia), testo, colori e marcatori a intervalli,
  memo (colore e testo); le righe di calcolo hanno espressione e risultato; le
  immagini il nome del file e il bordo; le strisce tipo, titolo e **dati**
  (sezioni del workspace della modalità); in coda le memorie Mat/Vct/List
  **del file**. Tipi di striscia sconosciuti (o non disponibili sulla scheda)
  sono conservati byte per byte.
- **Cartelle**: l'elenco mostra le sottocartelle di `PICOCALC/EACT` (create
  sul PC o da MEMORY in futuro); OPEN entra, EXIT risale (cap. 11 per creare e
  cancellare cartelle).

### 3.2 Strisce

Una striscia conserva **le parti del workspace** della sua modalità
(codifiche per parti, F2.5, in `workspace.cpp`): Graph (Y1–Y20, V-Window,
Setup di Graph, SKETCH), Table, Lists (List 1–26 e sottonomi), Equation,
Matrices, Graph3D, History (Run-Matrix). Aprendo la striscia eActivity salva
le parti correnti nella propria sezione del record (`0x0102`), carica quelle
della striscia e apre la modalità con una **sessione da striscia** nel
`Context` (barra di stato azzurra, schermata d'ingresso della striscia);
SHIFT ' (o MENU) torna all'eActivity, che rimette i dati aggiornati nella
striscia e ripristina quelli salvati. Se l'alimentazione manca durante la
sessione, all'avvio successivo la sezione `0x0102` ripristina i dati del menu
principale (le modifiche alla striscia non salvate si perdono, come un
documento non salvato).

| Striscia (manuale) | Modalità e schermata | Parti | Pico 2 W | Plus |
|---|---|---|---|---|
| RUN | Run-Matrix (Math) | History, Graph (i comandi grafici scrivono V-Window e SKETCH; aggiunto durante lo sviluppo) | ✔ | ✔ |
| Graph | Graph, grafico | Graph | ✔ | ✔ |
| Graph Editor | Graph, lista | Graph | ✔ | ✔ |
| Table Editor | Table, lista | Graph, Table | ✔ | ✔ |
| Conics Graph / Conics Editor | Conic Graphs | — | non disponibile | dalla M7 |
| Stat Graph | Statistics, grafico | Lists | ✔ | ✔ |
| List Editor | Statistics, liste | Lists | ✔ | ✔ |
| Solver | Equation, SOLVER | Equation | ✔ | ✔ |
| Recur Editor | Recursion | — | non disponibile | dalla M5 |
| Notes | editor di testo dell'eActivity | testo | ✔ | ✔ |
| Matrix Editor / Vector Editor | Run-Matrix, editor MAT/VCT | Matrices | ✔ | ✔ |
| Simul Equation / Poly Equation | Equation, SIML / POLY | Equation | ✔ | ✔ |
| Dynamic Graph | Dyna Graph | — | non disponibile | dalla M6 |
| Financial | Financial | — | non disponibile | dalla M8 |
| SpreadSheet | Spreadsheet | — | non disponibile | dalla M9 |
| E-CON Top / E-CON Result | E-CON4 | — | non disponibile (External) | non disponibile (External) |
| 3D Graph / 3D Graph Editor | 3D Graph, grafico / lista | Graph3D | ✔ | ✔ |
| Geometry | Geometry | — | non disponibile | dalla M12 |
| Picture Plot | Picture Plot | — | non disponibile | dalla M11 |

Una striscia «non disponibile» si può inserire solo dove la modalità esiste;
quelle già presenti in un documento (creato su un'altra scheda o versione)
restano intatte e si mostrano con «non disponibile».

### 3.3 Righe di testo con espressioni

Le righe di testo sono testo con **espressioni Math** incorporate: MATH (e i
tasti dei modelli) inserisce un'espressione che si scrive con lo stesso
editor naturale di Run-Matrix **nella riga**, al suo posto, e si mostra in
forma naturale; ← → entrano ed escono dall'espressione. Le espressioni non
vanno a capo (10-7).

### 3.4 Memorie per file

All'apertura di un documento le memorie Mat A–Z, Vct A–Z e List 1–26
correnti vengono salvate nella sezione `0x0102` e sostituite da quelle del
file (vuote in un file nuovo); alla chiusura tornano quelle di prima.
A–Z, Ans e il resto del workspace sono condivisi con le altre modalità (il
manuale separa solo matrici, vettori e liste).

## 4. Tabella di parità (prima del codice)

Ogni voce avrà almeno un test (`eactivity_test`, sui due profili).

| # | Voce | Pagina | Piano |
|---|---|---|---|
| E01 | elenco file all'ingresso, stato vuoto | 10-1 | presente |
| E02 | OPEN (F1 o EXE) di file e cartelle, EXIT risale | 10-2, 10-5 | presente |
| E03 | NEW: nome di 8 caratteri A–Z { } ' ~ 0–9, workspace vuoto | 10-4 | presente |
| E04 | DELETE con «Delete eActivity?» Sì/No | 10-5 | presente |
| E05 | SEARCH dall'inizio del nome, «Not Found» | 10-5 | presente |
| E06 | MEMO dall'elenco: lista dei memo del file, JUMP, EDIT, DETAIL, DELETE, DEL-ALL; «No MEMO» | 10-2, 10-11 | presente |
| E07 | errore all'apertura (file oltre il limite o corrotto) senza perdita | 10-5 | presente («Memory ERROR», «Data ERROR» dell'Appendice α) |
| E08 | righe di testo, calcolo, stop, immagine, striscia; barra di scorrimento | 10-1 | presente |
| E09 | ↑↓, SHIFT ↑/↓ (pagina), JUMP TOP/BOTTOM/PageUp/PageDown | 10-6 | presente |
| E10 | riga di testo: 255 byte, a capo automatico sulle parole, EXE = a capo | 10-7 | presente |
| E11 | A⇔a (maiuscole/minuscole) | 10-7 | presente |
| E12 | AC cancella la riga visualizzata | 10-7 | presente |
| E13 | espressioni Math nelle righe di testo (MATH, modelli) senza a capo | 10-7 | presente (§3.3) |
| E14 | CHAR: simboli matematici, speciali e lettere di altre lingue | 10-3 | presente |
| E15 | F3 TEXT ⇔ CALC sulla riga corrente | 10-3, 10-6 | presente |
| E16 | riga di calcolo come Run-Matrix in Math: EXE mostra il risultato | 10-7 | presente (nucleo `evaluateEntry`) |
| E17 | forme esatte e S⇔D del risultato come Run-Matrix, input/output sempre Math | 10-7 | presente (S⇔D da OPTN: la PicoCalc non ha il tasto S⇔D) |
| E18 | scorrimento ◀▶ delle righe di calcolo lunghe | 10-7 | presente |
| E19 | ricalcolo delle righe seguenti fino alla riga di stop | 10-9 | presente (a passi con StepTask, AC → Break) |
| E20 | INSERT › STOP | 10-10 | presente |
| E21 | INSERT › TEXT/CALC/PICTURE sopra la riga corrente | 10-3 | presente |
| E22 | DEL-LINE con conferma (calcolo e risultato insieme) | 10-3, 10-21 | presente |
| E23 | 'MAT/VCT: editor MAT/VCT con memorie del file | 10-9 | presente |
| E24 | 'LIST: editor delle liste con memorie del file, EXIT torna | 10-9 | presente |
| E25 | memorie Mat/Vct/List separate per file | 10-9 | presente (§3.4) |
| E26 | COLOR › MARKER: magenta, verde, ciano, giallo, Clear, SET/EXIT, su più righe | 10-10 | presente |
| E27 | COLOR › CHAR: colore del testo, SET/EXIT | 10-11 | presente |
| E28 | niente colore sul risultato di un calcolo | 10-10 | presente |
| E29 | MEMO › INSERT con colore e finestra di 255 byte; uno per riga; icona | 10-11, 10-12 | presente |
| E30 | MEMO › Catalog: elenco e salto alla riga | 10-12 | presente |
| E31 | MEMO › VIEW e DETAIL: modifica | 10-13 | presente |
| E32 | MEMO › DELETE con conferma | 10-13 | presente |
| E33 | immagini: INSERT › PICTURE dalla cartella PICT | 10-15 | presente (`.pcpict` di PICT e CAPT; differenza D1) |
| E34 | selezione dell'immagine (bordo rosso / evidenziato) | 10-15 | presente |
| E35 | FORMAT del bordo: Line Style, Line Color, Clear | 10-16 | presente |
| E36 | DELETE dell'immagine con conferma | 10-16 | presente |
| E37 | STRIP: elenco dei 24 tipi, inserimento sopra la riga | 10-17, 10-18 | presente (disponibilità per scheda §3.2) |
| E38 | titolo della striscia di 16 caratteri, modificabile | 10-18, 10-20 | presente |
| E39 | EXE sulla striscia apre la modalità con i dati della striscia | 10-18, 10-20 | presente |
| E40 | dati della striscia indipendenti da quelli del menu principale | 10-19 | presente (§3.2) |
| E41 | barra di stato azzurra durante la striscia | 10-20 | presente |
| E42 | SHIFT ' alterna eActivity e modalità della striscia | 10-21 | presente |
| E43 | SHIFT , apre un'altra applicazione dalla striscia | 10-21 | presente (elenco delle strisce disponibili) |
| E44 | FILE › SIZE della striscia | 10-21 | presente |
| E45 | Notes: editor di testo lungo con JUMP, DEL-LINE, INSERT, MATH, CHAR, A⇔a | 10-19, 10-20 | presente (editor F5) |
| E46 | Conics Graph senza dati apre Conics Editor | 10-20 | non applicabile sul Pico 2 W; sulla Plus con la M7 |
| E47 | striscia di una modalità non disponibile: conservata, «non disponibile» | (progetto) | presente |
| E48 | FILE › SAVE; SAVE·AS con «sovrascrivere?» Sì/No e ritorno al nome | 10-22 | presente |
| E49 | FILE › OPT (ottimizzazione della memoria) | 10-3 | presente (recupero dei salvataggi interrotti, differenza D2) |
| E50 | FILE › CAPACITY: byte usati e rimasti | 10-23 | presente |
| E51 | EXIT dal workspace: richiesta di salvataggio Sì / No / AC | 10-23 | presente («Salvare il file?»: F1/EXE Sì, F6/EXIT No, AC torna) |
| E52 | limite del file (fx-CG50 ≈ 29 000 byte) | 10-23 | presente: 24 KB (Pico 2 W) / 256 KB (Plus), «Memory ERROR» senza perdita |
| E53 | 128 KB richiesti al primo uso | 10-2 | differenza D3 |

Voci aggiunte dall'Appendice α (messaggi di eActivity e delle cartelle),
lette durante il progetto del controller e prima di scriverne il codice:

| # | Voce | Pagina | Piano |
|---|---|---|---|
| E54 | «Only one memo allowed per line.»: MEMO › INSERT su una riga con memo e unione di due righe con memo | α-6 | presente |
| E55 | «Image wrong size for insertion.»: immagine di dimensioni non ammesse | α-6, 10-14 | presente (lato oltre 320 punti, lo schermo della PicoCalc) |
| E56 | cartelle annidate fino a tre livelli; il quarto si vede ma non si apre | α-7, 11-13 | presente («Sub-folders cannot be displayed») |

## 5. Differenze previste

| # | Voce | Motivo |
|---|---|---|
| D1 | immagini `.g3p` della fx-CG50 | la PicoCalc usa il formato del progetto `.pcpict` (immagini di Graph in `PICOCALC/PICT` e catture SHIFT 7 in `PICOCALC/CAPT`); il BMP è solo esportato, non letto; schermo 320×320 invece di 384×216: le immagini sono mostrate alla larghezza della finestra |
| D2 | OPT | la fx-CG50 compatta la memoria di archiviazione; su FAT/LittleFS non serve: OPT completa o annulla i salvataggi interrotti (`files::recover`) e mostra lo spazio libero |
| D3 | «128 KB al primo uso» | requisito della memoria della fx-CG50; qui il limite è quello del documento aperto per scheda |
| D4 | file `.g1e/.g2e/.g3e` | formato proprietario Casio non letto; i documenti sono `.pceact` |
| D6 | tasti fisici S⇔D, ' e , della fx-CG50 | sulla PicoCalc S⇔D è in OPTN della riga di calcolo; SHIFT ' e SHIFT , sono F7 seguito dai tasti ' e , della tastiera |
| D7 | menu delle lettere (CHAR) | i simboli e le lettere di altre lingue sono quelli che il font della PicoCalc sa disegnare, in quattro pagine (MATH, SYMBOL, GREEK, LANG) |
| D5 | E-CON Top / E-CON Result | E-CON4 è External (nessun data logger) |
