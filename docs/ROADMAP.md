# Roadmap — dopo la v1.20.0

**Piano dalla v1.17.0** ([reports/ANALISI_FONTI_v1.17.0.md](reports/ANALISI_FONTI_v1.17.0.md)):
la v1.17.0 è la **base di tutte le modalità** (ogni modalità del registro si
apre, fa il suo lavoro principale, salva e rimette a posto lo stato, esce senza
perdite; dieci modalità nuove nello stato **Base** sulla Pimoroni Pico Plus
2 W). Da qui **una modalità per versione arriva alla parità completa** con la
fx-CG50 e passa da Base (o da Available incompleta) ad Available, con la sua
tabella di parità scritta prima del codice e il cancello completo (test su
entrambi i profili anche con i sanitizer, fuzzing, simulatore, firmware nei
budget, relay, documentazione) e un solo commit su `main`.

| Versione | Modalità alla parità completa | Note | Schede |
|---|---|---|---|
| v1.15.0 | fondamenta comuni (fatta) | ModeShell, file tipizzati, record v11, StepTask, TextEdit; Graph2D, Binding, Interp solo Plus | entrambe |
| v1.16.0 | **eActivity** (cap. 10, fatta); il **Pico 2 W è congelato** | | entrambe |
| v1.17.0 | **base di tutte le modalità** (fatta) | stato Base, `apps_init_test`, record v12, dieci modalità Base | Plus (inizializzazione: entrambe) |
| v1.18.0 | **Statistics** (cap. 6, fatta) | parità completa sulla Plus: editor a tre pagine con formule e colori, StatGraph1–3 con 20 tipi, 12 regressioni con residui, TEST (Z, t, χ², F, ANOVA), INTR, DIST con le 24 funzioni, VARS › STAT; **Distribution** (cap. 18) nel registro come modalità Base | Plus |
| v1.19.0 | **Geometry** (cap. 14, fatta) | parità completa sulla Plus: Draw/Draw Spec, Construct e Transform legati ai genitori (grafo delle dipendenze, ricalcolo incrementale), casella di misura con blocchi e risolutore a passi, testo, etichette, FORMAT, Undo/Redo, View, animazioni con traccia e tabella, `.pcgeo` v2; Geometry **Available** | Plus |
| v1.20.0 | **Program** (cap. 8 come modalità, fatta: C, C++, C#) | programmi come sorgenti standard sulla microSD (`PICOCALC/CODE`), gestore dei file (NEW, RENAME, COPY, MOVE, DEL-ALL, SEARCH, password), editor con file grandi a blocchi, DEBUG, nucleo dell'interprete nuovo confrontato con gcc/g++/Mono, libreria `fx` completa, gestore delle librerie (indice HTTPS, SHA-256, dipendenze, installazione atomica); Program resta **Base** | Plus |
| **v1.21.0** | **Program: Java** | sottoinsieme di Java 17 sul nucleo della v1.20.0 (classi, interfacce, generici, eccezioni, `java.util`, `Scanner`), librerie Java (`LIB/JAVA`); tabella del capitolo 8 completa per i quattro linguaggi → Program **Available** | Plus |
| v1.22.0 | Python (cap. 17) | parità del capitolo 17 con gli script sulla microSD (`PICOCALC/CODE/PY`), menu delle funzioni, CATALOG, moduli `random`, `turtle`, `matplotl`, `import` dalle librerie (`LIB/PY`) | Plus |
| v1.23.0 | Graph e Table | Dual Screen G+G/GtoT/T+G, Graph to Table, FORMAT nello SKETCH | Plus |
| v1.24.0 | Recursion (5-45) | bn, cn, an = f(n), grafico di fase, convergenza | Plus |
| v1.25.0 | Dyna Graph (5-42) | BUILT-IN, Locus, Dynamic Graph Memory | Plus |
| v1.26.0 | Conic Graphs (5-50) | forme polari e parametriche, direttrice, asse, latus rectum, ZOOM | Plus |
| v1.27.0 | Financial (cap. 7) | Cost/Sell/Margin, Depreciation, Bond, grafici, 360 giorni | Plus |
| v1.28.0 | Spreadsheet (cap. 9) | SEQ, SORT, CellIf/CellProd, formattazione condizionale, GRAPH e CALC dalle celle | Plus |
| v1.29.0 | Picture Plot (cap. 15) | fotogrammi, AXTRANS, Plot List, Fade I/O | Plus |
| v1.30.0 | Memory, System e Link (cap. 11–13) | Link: più file, immagini dello schermo | Plus |
| v1.31.0 | Distribution (cap. 18) | dalla Base della v1.18.0: schermo diviso dal vivo, limiti sul grafico, tabella, strisce | Plus |
| v1.32.0 | chiusura: revisione completa e recap | | entrambe |

La v1.18.0 (Statistics), la v1.19.0 (Geometry) e la v1.20.0 (Program in C,
C++ e C#) sono fatte
([reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md),
[reports/SVILUPPO_v1.19.0.md](reports/SVILUPPO_v1.19.0.md),
[reports/SVILUPPO_v1.20.0.md](reports/SVILUPPO_v1.20.0.md)); la prossima è
**Java in Program** (v1.21.0), seguita da Python (v1.22.0); dalla v1.20.0 le
versioni successive sono spostate di uno (Graph e Table v1.23.0 … chiusura
v1.32.0);
l'ordine delle versioni successive è una proposta da confermare all'inizio di ognuna (cosa manca a ogni modalità Base è
nel campo `missing` del registro e in [FEATURE_MATRIX.md](FEATURE_MATRIX.md)).
E-CON4 resta External.

Il **Raspberry Pi Pico 2 W è congelato alla v1.16.0**: le versioni successive
aggiungono funzioni solo alla Pimoroni Pico Plus 2 W; sul Pico 2 W entrano
soltanto correzioni di errori condivise (controllo in `architecture_test`, che
fallisce se cambia l'insieme delle modalità aperte del Pico 2 W). Le prove su
hardware continuano la numerazione: la v1.20.0 arriva a HW-313.

La v1.14.0 completa il **3D Graph** (capitolo 16 del manuale fx-CG50: modelli
Line, Plane, Sphere, Cylinder, Cone, grafici Z=, Param, Rot X/Y, V-Window con
θ e φ, Setup, FORMAT, 3D G-Mem e V-Mem, rotazione e viste, TRACE, G-Solve
CROSS/INTSECT/RELATION; [GRAPH3D.md](GRAPH3D.md),
[reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)). La v1.13.1 corregge gli errori trovati nella revisione completa della v1.13.0
([reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md)); restano aperti i
punti A1–A6 di quel documento (A2: precisione di HypergeoPD con argomenti
tutti grandi; A3: risultati sotto 1E−99 in Run-Matrix da verificare su una
fx-CG50; A5: collaudo fisico; A6: SOLVER di EQUATION con SolveN/FMin/FMax
nell'equazione, Time Out solo dopo un calcolo lungo).

Il progetto **non** ha la parità con la Casio fx-CG50 e non va presentato come
tale: le tabelle sotto elencano ciò che manca. Fanno eccezione **Run-Matrix**
(dalla v1.13.0, [reports/SVILUPPO_v1.13.0.md](reports/SVILUPPO_v1.13.0.md)),
**Statistics** (dalla v1.18.0 sulla Plus, [STATISTICS.md](STATISTICS.md), [reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md)),
**Geometry** (dalla v1.19.0 sulla Plus, [GEOMETRY.md](GEOMETRY.md), [reports/SVILUPPO_v1.19.0.md](reports/SVILUPPO_v1.19.0.md)),
**EQUATION** (dalla v1.12.0, [reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md))
**3D Graph** (dalla v1.14.0, [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md))
ed **eActivity** (dalla v1.16.0, [reports/SVILUPPO_v1.16.0.md](reports/SVILUPPO_v1.16.0.md)),
con ogni voce della tabella di parità presente o documentata come differenza
inevitabile. Nove delle dieci modalità della v1.17.0 (tutte tranne Geometry) e Distribution (v1.18.0) sono **Base**: si usano, ma la
parità non c'è ancora ([reports/SVILUPPO_v1.17.0.md](reports/SVILUPPO_v1.17.0.md)). Il documento storico
`archive/CG50_PARITY_ROADMAP.md` descrive gli obiettivi delle versioni precedenti.
La v1.9.0 ha unito la v1.8.0 al supporto del Pimoroni Pico Plus 2 W
([reports/CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md)); la
v1.10.0 ha aggiunto a Run-Matrix matrici, vettori e liste, l'editor
MAT/VCT, Math/Line, CLIP, parentesi colorate e il cursore sugli errori
([reports/SVILUPPO_v1.10.0.md](reports/SVILUPPO_v1.10.0.md)); la v1.11.0 ha
completato Run-Matrix: List 1–26 e sottonomi, CSV, MatAns/ListAns,
celle esatte e complesse, INS/⌟/scorrimento in Line, SHIFT+INS, simboli
ingegneristici, Simp, Base-N intera e comandi grafici
([reports/SVILUPPO_v1.11.0.md](reports/SVILUPPO_v1.11.0.md)); la v1.12.0
ha completato EQUATION (SIML, POLY, SOLVER, VARS › EQUATION;
[EQUATION.md](EQUATION.md), [reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md)); la v1.13.0
(`main`) ha completato la parità di Run-Matrix: List "nome" e List File, P/E e ENG,
Simplify Manual, Base-N completa, tutti i comandi grafici, round trip
Math/Line, CALC/PROB/STAT/CONVERT/TVM, CATALOG e CAPTURE
([reports/SVILUPPO_v1.13.0.md](reports/SVILUPPO_v1.13.0.md)). Le release
successive si creano solo quando i rispettivi incrementi sono completati e
verificati. La matrice di parità di GRAPH è in
[GRAPH_PARITY.md](GRAPH_PARITY.md).

## 0. Recap e fondamenta delle applicazioni

Il recap della v1.14.0 con la parità modalità per modalità è in
[reports/RECAP_v1.14.0.md](reports/RECAP_v1.14.0.md). Prima di realizzare le
modalità mancanti, la v1.15.0 mette le basi comuni (budget per scheda, record
v11 a sezioni estensibili, file tipizzati, guscio delle modalità, grafico 2D con
Dual Screen, espressioni con variabili legate, lavoro a passi interrompibile,
editor multiriga, nucleo dell'interprete, studio di MicroPython); seguono le
modalità una per versione, dalla più piccola e più riusata alla più grande.

## 1. Collaudo su hardware (priorità massima, richiede la PicoCalc)

Nessuna funzione è stata provata sulla PicoCalc, né con il Pimoroni Pico Plus 2 W
né con il Pico 2 W. Procedure in [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md)
(PSRAM: rilevamento, self-test, frequenza, convivenza con la cache XIP e le
scritture flash; partizioni della flash da 16 MB; JEDEC ID; 3D; prestazioni con
i limiti della PSRAM; guasti), [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)
e [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo): display
(RGB666/RGB565, orientamento), tastiera (codici, ripetizione, tempi I2C), flash
(salvataggio, blackout), batteria, Wi-Fi/TLS/relay, stabilità. Per Run-Matrix:
tempi di risposta della valutazione esatta (fattorizzazione di radicandi grandi)
e del ricalcolo della cronologia (40 righe sul Pico 2 W, 200 sulla Plus). Per GRAPH: tempi del disegno
incrementale (600 valutazioni per ciclo), reattività della tastiera durante il
disegno, colori delle curve e dei riempimenti sul pannello, memoria libera con
20 grafici e immagini di sfondo. Per la microSD (SPI0, GP16–19, GP22):
inizializzazione di schede SDHC e SDXC exFAT reali, clock massimo affidabile,
estrazione durante il salvataggio, progetti scritti dal PC.

## 2. Prossima sessione (proposta)

Il collaudo software di GRAPH e della microSD e il completamento di TABLE
sono stati fatti nella v1.8.0 (rapporto
[reports/SESSIONE_v1.8.0_COLLAUDO_TABLE.md](reports/SESSIONE_v1.8.0_COLLAUDO_TABLE.md));
la v1.9.0 ha aggiunto il supporto della Pico Plus 2 W, il 3D Graph e le liste da
999 elementi sulla Plus; la v1.10.0 le matrici, i vettori e le liste in
Run-Matrix con l'editor MAT/VCT e quattro voci del Punto 3. Restano, in ordine:

| # | Voce | Note |
|---|---|---|
| 0 | **Esito del collaudo fisico** della v1.13.1 sulla Pico Plus 2 W | procedure [HARDWARE_TEST_v1.13.1.md](HARDWARE_TEST_v1.13.1.md), [HARDWARE_TEST_v1.13.0.md](HARDWARE_TEST_v1.13.0.md), [HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md), [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md), [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md), [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md); correggere i KO prima del resto e aggiornare a TESTED ON HARDWARE solo le voci provate |
| 1 | Dual Screen G+G (COPY, SWAP), GtoT e T+G, Graph to Table | `graph::Viewport` accetta già un'area diversa da 320×282 |
| 2 | Dyna Graph, Recursion, Conic Graphs | sul servizio GraphCore (`sampleStep`, `Viewport`, G-Solve, `table_model` per le tabelle delle successioni); i dati (D Start/End/pitch, an+1, a0, R Start/End) sono nel workspace dalla v1.13.0 e i comandi DrawDyna/DrawR/DrawWeb li disegnano già |
| 3 | FORMAT durante lo SKETCH | piccolo completamento di GRAPH (i suffissi angolari °/ʳ/ᵍ sono fatti nella v1.13.0) |
| 4 | Completamenti di matrici e liste | **fatti nella v1.10.0 e nella v1.11.0**: matrici/vettori/liste nel parser, editor MAT/VCT, List 1–26 e sottonomi, CSV, MatAns/ListAns, celle esatte e complesse; **v1.13.0**: List File 1–6, `List "nome"` nei calcoli, RanList#/RanSamp# e tutte le funzioni di lista |
| 5 | Punto 3: Natural V.P.A.M. ↔ formato lineare | **fatto** (v1.10.0–v1.13.0), dettaglio sotto |

### Punto 3: Natural V.P.A.M. ↔ formato lineare

Stato (v1.13.0): tutte le voci sono fatte. Editor e parser condividono `ast::Node`; l'editor produce
il testo lineare canonico (`ast::serialize`) e lo reimporta strutturalmente
(`NaturalEditor::setExpression`); Run-Matrix usa solo questa pipeline. Le voci
1, 3, 4, 6 e 8 sono state fatte nella v1.10.0 (con i limiti scritti accanto):

| # | Voce | Note |
|---|---|---|
| 1 | Impostazione **Input/Output Math/Line** (SET UP) | **fatto (v1.10.0)**; INS, ⌟ e risultati scorrevoli nella v1.11.0 |
| 2 | Round-trip garantito testo ↔ albero | **fatto (v1.13.0)**: `Group` per le parentesi che contengono modelli, `root(n,x)` con qualsiasi indice, modello d²/dx², Σ/Π con variabile e passo; corpus `serialize(import(t)) == t` in `editor_v112_test` |
| 3 | Cursore sul punto dell'errore dopo EXIT | **fatto (v1.10.0)**: `ErrorLocation`, `ast::SerializeMap`, `NaturalEditor::markError` |
| 4 | Selezione parziale CLIP (copia/taglia di un intervallo) | **fatto (v1.10.0)**, con le parentesi sempre bilanciate |
| 5 | SHIFT+INS: usare un'espressione esistente come argomento di un modello | **fatto (v1.11.0)** |
| 6 | Colori delle parentesi per livello (blu, rosso, verde, magenta, nero) | **fatto (v1.10.0)**: `NaturalDrawOp::color`, `math::Palette` |
| 7 | Regola del segno × obbligatorio prima di una frazione e parentesi implicite (`2 1/3` → `2(1/3)` nel lineare) | **fatto (v1.13.0)**: `ast::impliedFraction` |
| 8 | Modelli MAT/VCT modificabili nell'editor | **fatto (v1.10.0)** (fino a 6×6) |
| 9 | Visualizzazione in Graph/Table/Equation delle espressioni con gli stessi modelli | **fatto**: `math_renderer` condiviso (Graph/Table v1.10.0, Equation v1.12.0) |

Non anticipato in questa sessione per non interferire: nessuna modifica al
formato canonico del testo lineare esistente (salvo l'aggiunta di `mixed(…)` e
del suffisso periodico `_cifre`, già coperti da test di round-trip).

## 3. Motore matematico

| Funzione | Note |
|---|---|
| ~~Matrici, vettori e liste come valori del parser; MAT/VCT/List in Run-Matrix~~ | **fatto nella v1.10.0** (List 1–26 dalla v1.11.0) |
| ~~Simboli ingegneristici, Simp, Base-N intera, comandi grafici di Run-Matrix~~ | **fatti nella v1.11.0**, completati nella v1.13.0 |
| ~~Base-N completa (A–F in Hex, liste e matrici, bit per base)~~ | **fatto nella v1.13.0** |
| ~~Simboli P/E, ENG→/←ENG~~ | **fatto nella v1.13.0** |
| ~~Simplify Manual~~ | **fatto nella v1.13.0** (regole del manuale) |
| ~~RanList#, RanSamp#, Ran# con sequenze~~ | **fatto nella v1.13.0** |
| ~~SolveN, FMin/FMax, distribuzioni, CONVERT, TVM~~ | **fatto nella v1.13.0** (`calc_library`) |
| Derivate/integrali simbolici, limiti | fuori dall'ambito: la fx-CG50 non ha un CAS |

## 4. Modalità

Stato e progetto di ogni modalità in [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md).

| Modalità | Contenuto da realizzare |
|---|---|
| Statistics | **parità completata nella v1.18.0** sulla Plus ([STATISTICS.md](STATISTICS.md)); il Pico 2 W congelato resta alla v1.16.0. Differenze inevitabili: frequenze intere, Sin/Logistic con un metodo iterativo proprio, ANOVA a due vie bilanciata |
| Distribution | **Base dalla v1.18.0** (solo Plus, [DISTRIBUTION.md](DISTRIBUTION.md)): mancano lo schermo diviso dal vivo, i limiti sul grafico, la tabella, le strisce |
| Graph | Dual Screen G+G/GtoT, Graph to Table, FORMAT nello SKETCH (i comandi grafici di Run-Matrix sono completi dalla v1.13.0) (dettaglio in [GRAPH_PARITY.md](GRAPH_PARITY.md)) |
| Table | Dual Screen T+G (LISTMEM con 999 elementi già sulla Plus; 255 sul Pico 2 W) |
| Equation | **parità completata nella v1.12.0** ([EQUATION.md](EQUATION.md)); restano come estensioni: interfaccia per i sistemi di disequazioni; forme esatte di quozienti complessi (`1/(1+i)`) e delle radici cubiche |
| Recursion, Conic Graphs, Dyna Graph | **Base dalla v1.17.0** (solo Plus): mancano bn/cn, an = f(n), fase (Recursion); BUILT-IN, Locus, memoria (Dyna); forme polari/parametriche, direttrice, asse, latus rectum, ZOOM (Conic) |
| Financial | **Base dalla v1.17.0** (SMPL, CMPD, CASH, AMT, CNVT, DAYS): mancano Cost/Sell/Margin, Depreciation, Bond, grafici, 360 giorni |
| Spreadsheet, Program | **Base dalla v1.17.0**: mancano SEQ, SORT, CellIf/CellProd, formattazione condizionale, GRAPH/CALC dalle celle (Spreadsheet); Program: dalla v1.20.0 gestore dei file, DEBUG, SEARCH, RENAME, password, libreria `fx` completa e librerie in C, C++ e C#; manca Java (v1.21.0), poi Available. eActivity: parità dalla v1.16.0 |
| Geometry | **parità completata nella v1.19.0** sulla Plus ([GEOMETRY.md](GEOMETRY.md)); differenze inevitabili D1–D9 di [reports/ANALISI_FONTI_v1.19.0.md](reports/ANALISI_FONTI_v1.19.0.md) (grafica propria, tasti, casella anche con OPTN, Clr Constraint non tocca le costruzioni, sfondi `.pcpict`, STORE › STAT al posto di S-SHT) |
| Picture Plot | **Base dalla v1.17.0**: mancano fotogrammi, AXTRANS, Plot List, Fade I/O |
| 3D Graph | **parità completata nella v1.14.0** ([GRAPH3D.md](GRAPH3D.md)); differenze inevitabili: griglia ≤ 25 e pittore senza PSRAM, tasti della PicoCalc; collaudo su hardware (HW-170…HW-189) |
| Link | **Base dalla v1.17.0** (workspace e file in pacchetti `.pclink` su USB seriale, `tools/picocalc_link.py`): mancano la selezione di più file e le immagini dello schermo; il protocollo Casio tra calcolatrici non è supportato |
| Python | **Base dalla v1.17.0** (MicroPython v1.24.1, heap separato, casioplot): mancano il menu delle funzioni, il CATALOG, il debug e i moduli `random`, `turtle`, `matplotl` (v1.22.0, con gli script in `PICOCALC/CODE/PY` sulla microSD) |
| E-CON4 | non realizzabile (data logger esterno); al più import CSV nelle liste |

## 5. Piattaforma

- microSD: driver SPI + FatFs presenti dalla v1.8.0 (progetti GRAPH, immagini),
  recupero dopo estrazione verificato sul PC, diagnostica SYSTEM → DIAG;
  da collaudare su hardware; da estendere a programmi, documenti e backup del
  workspace; accesso multiblocco (CMD18/CMD25) per file grandi.
- PSRAM: collaudo del timing (oggi valori SparkFun per l'APS6404L) e, se stabile,
  frequenza più alta; `PSRAM_PICOCALC` (PSRAM della mainboard) è solo
  un'interfaccia.
- Gestione energetica: spegnimento del display, sospensione, risveglio da tastiera.
- DMA e clock SPI più alto per il display, se il collaudo lo consente.
- Animazioni di transizione tra le schermate (primitiva già presente).

## 6. AI

- Prova con chiavi reali e misura dei tempi di risposta dal Pico.
- Eventuale streaming della risposta (oggi la risposta arriva intera).
