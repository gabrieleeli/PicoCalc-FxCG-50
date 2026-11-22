# eActivity — guida (v1.16.0)

eActivity (MENU › **3**) è un quaderno: un documento è un elenco di righe di
testo, righe di calcolo, righe di stop, immagini e **strisce** che aprono le
altre modalità con dati propri. Comportamento ripreso dal capitolo 10 del
manuale fx-CG50 (solo come descrizione); su **entrambe le schede**. Tabella di
parità: [reports/SVILUPPO_v1.16.0.md](reports/SVILUPPO_v1.16.0.md); formato
dei file: [FILE_FORMATS.md](FILE_FORMATS.md).

## Tasti della PicoCalc

| fx-CG50 | PicoCalc |
|---|---|
| F1…F6 | F1…F5, F6 = Shift+F1 (barre a più pagine: **▶** passa alla pagina seguente) |
| SHIFT | F7 (Shift+F2), una volta |
| EXE / EXIT / AC | Enter / Esc / Break (Shift+Esc) |
| → (memorizzazione) | Insert (Shift+Enter) |
| S⇔D | OPTN (F8) su una riga di calcolo con risultato: S⇔D, a b/c ⇔ d/c |
| SHIFT ' / SHIFT , | F7 e poi il tasto ' oppure , |
| SHIFT ↑ / SHIFT ↓ | F7 e poi ↑ / ↓ (pagina; su un'immagine SHIFT ↑ è FORMAT) |

## Elenco dei file

`PICOCALC/EACT` sulla microSD o nella flash interna (SYSTEM › «Archivio
file»). Cartelle fino a tre livelli (create sul PC): OPEN o EXE entra, EXIT
risale. **F1 OPEN**, **F2 NEW** (nome di 1–8 caratteri A–Z 0–9 { } ' ~; il file
nasce subito), **F3 DELETE** («Delete eActivity?»), **F4 SEARCH** (inizio del
nome; «Not Found»), **F5 MEMO** (memo del file: JUMP, EDIT, DETAIL, DELETE,
DEL-ALL; «No MEMO»).

## Righe

- **Testo**: fino a 255 byte, a capo sulle parole; EXE va a capo (nuova riga),
  DEL all'inizio unisce alla riga sopra, AC cancella la riga visualizzata,
  A⇔a inverte maiuscole e minuscole, **CHAR** apre simboli matematici, speciali,
  greci e lettere di altre lingue. **MATH** inserisce un'espressione in forma
  naturale (frazioni, radici, potenze…) nel testo: si scrive al suo posto, ← e
  → entrano ed escono, EXE la chiude; le espressioni non vanno a capo.
- **Calcolo**: come Run-Matrix in input Math; EXE mostra il risultato sotto
  (forme esatte) e passa alla riga seguente (in fondo ne crea una nuova); le
  righe di calcolo seguenti vengono **ricalcolate** fino a una riga di **stop**,
  con l'avanzamento in basso; AC interrompe («Break»).
- **F3** sulla riga: TEXT (testo → calcolo) / CALC (calcolo → testo).
- **Stop**, **immagine** (`.pcpict` di `PICOCALC/PICT` o `PICOCALC/CAPT`,
  larga al più 320 punti: «Image wrong size for insertion.»), **striscia**.

Barre (pagine ▶): testo `FILE STRIP TEXT CHAR A⇔a` ▶ `JUMP DEL-LINE INSERT
MATH COLOR` ▶ `'MAT/VCT 'LIST MEMO`; calcolo `FILE STRIP CALC MATH INSERT` ▶
`JUMP DEL-LINE 'MAT/VCT 'LIST COLOR` ▶ `MEMO`; immagine `FILE STRIP` ▶ `JUMP
DELETE INSERT`; striscia `FILE(…SIZE) STRIP CHAR A⇔a` ▶ `JUMP DEL-LINE
INSERT`. FILE: SAVE, SAVE·AS (con «Sovrascrivere?»), OPT, CAPACITY; INSERT:
TEXT, CALC, STOP, PICTURE (sopra la riga); COLOR: MARKER (magenta, verde,
ciano, giallo, Clear) e CHAR (colore del testo) — si scelgono, si muove il
cursore anche su più righe e F1 **SET** applica, EXIT annulla; MEMO: INSERT
(colore e finestra di 255 byte; uno per riga), DELETE, Catalog (salto alla
riga), VIEW (DETAIL per il testo lungo).

## Strisce

**STRIP** mostra i 24 tipi del manuale; quelli di modalità che questa scheda
non ha sono «non disponibile» (Conics, Recur, Dynamic Graph, Financial,
SpreadSheet, Geometry, Picture Plot arriveranno solo sulla Pimoroni Pico Plus
2 W; E-CON è External). La striscia va sopra la riga; si scrive il titolo (16
caratteri, EXE), poi EXE apre la modalità con i **dati della striscia**,
separati da quelli del menu principale, e la barra di stato **azzurra**.
SHIFT ' (o MENU) torna al documento e salva i dati nella striscia; SHIFT ,
apre un'altra applicazione in una striscia nuova. **Notes** è un editor di
testo lungo (JUMP, DEL-LINE, INSERT, MATH, CHAR, A⇔a; EXIT o SHIFT ' torna).

| Striscia | Si apre |
|---|---|
| RUN | Run-Matrix in Math |
| Graph / Graph Editor | Graph: grafico / lista delle funzioni |
| Table Editor | Table |
| Stat Graph / List Editor | Statistics: grafico (Scatter) / liste |
| Solver, Simul Equation, Poly Equation | Equation |
| Matrix Editor / Vector Editor | editor MAT/VCT |
| 3D Graph / 3D Graph Editor | 3D Graph: grafico / lista |

## Memorie e limiti

Mat A–Z, Vct A–Z e List 1–26 sono **del documento** ('MAT/VCT e 'LIST le
mostrano; EXIT torna): aprendo un documento quelle del menu principale vengono
messe da parte e tornano alla chiusura. EXIT dal documento chiede «Salvare il
file?» (F1/EXE Sì, F6/EXIT No, AC torna al documento). Il documento aperto può
arrivare a **24 KB** sul Raspberry Pi Pico 2 W e **256 KB** sulla Pimoroni
Pico Plus 2 W (CAPACITY); oltre, «Memory ERROR» e il documento resta com'era.
Se l'alimentazione manca con un documento o una striscia aperti, al riavvio
i dati del menu principale sono al loro posto; le modifiche non salvate del
documento si perdono.
