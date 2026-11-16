# Roadmap — dopo la v1.12.0

Il progetto **non** ha la parità con la Casio fx-CG50 e non va presentato come
tale: le tabelle sotto elencano ciò che manca. Fa eccezione **Run-Matrix**, che
dalla v1.12.0 ha ogni voce della tabella di parità presente o documentata come
differenza inevitabile ([reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md)). Il documento storico
`archive/CG50_PARITY_ROADMAP.md` descrive gli obiettivi delle versioni precedenti.
La v1.9.0 ha unito la v1.8.0 al supporto del Pimoroni Pico Plus 2 W
([reports/CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md)); la
v1.10.0 ha aggiunto a Run-Matrix matrici, vettori e liste, l'editor
MAT/VCT, Math/Line, CLIP, parentesi colorate e il cursore sugli errori
([reports/SVILUPPO_v1.10.0.md](reports/SVILUPPO_v1.10.0.md)); la v1.11.0
ha completato Run-Matrix: List 1–26 e sottonomi, CSV, MatAns/ListAns,
celle esatte e complesse, INS/⌟/scorrimento in Line, SHIFT+INS, simboli
ingegneristici, Simp, Base-N intera e comandi grafici
([reports/SVILUPPO_v1.11.0.md](reports/SVILUPPO_v1.11.0.md)); la v1.12.0
(`main`) ne ha completato la parità: List "nome" e List File, P/E e ENG,
Simplify Manual, Base-N completa, tutti i comandi grafici, round trip
Math/Line, CALC/PROB/STAT/CONVERT/TVM, CATALOG e CAPTURE
([reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md)). Le release
successive si creano solo quando i rispettivi incrementi sono completati e
verificati. La matrice di parità di GRAPH è in
[GRAPH_PARITY.md](GRAPH_PARITY.md).

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
| 0 | **Esito del collaudo fisico** della v1.12.0 sulla Pico Plus 2 W | procedure [HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md), [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md), [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md), [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md); correggere i KO prima del resto e aggiornare a TESTED ON HARDWARE solo le voci provate |
| 1 | Dual Screen G+G (COPY, SWAP), GtoT e T+G, Graph to Table | `graph::Viewport` accetta già un'area diversa da 320×282 |
| 2 | Dyna Graph, Recursion, Conic Graphs | sul servizio GraphCore (`sampleStep`, `Viewport`, G-Solve, `table_model` per le tabelle delle successioni); i dati (D Start/End/pitch, an+1, a0, R Start/End) sono nel workspace dalla v1.12.0 e i comandi DrawDyna/DrawR/DrawWeb li disegnano già |
| 3 | FORMAT durante lo SKETCH | piccolo completamento di GRAPH (i suffissi angolari °/ʳ/ᵍ sono fatti nella v1.12.0) |
| 4 | Completamenti di matrici e liste | **fatti nella v1.10.0 e nella v1.11.0**: matrici/vettori/liste nel parser, editor MAT/VCT, List 1–26 e sottonomi, CSV, MatAns/ListAns, celle esatte e complesse; **v1.12.0**: List File 1–6, `List "nome"` nei calcoli, RanList#/RanSamp# e tutte le funzioni di lista |
| 5 | Punto 3: Natural V.P.A.M. ↔ formato lineare | **fatto** (v1.10.0–v1.12.0), dettaglio sotto |

### Punto 3: Natural V.P.A.M. ↔ formato lineare

Stato (v1.12.0): tutte le voci sono fatte. Editor e parser condividono `ast::Node`; l'editor produce
il testo lineare canonico (`ast::serialize`) e lo reimporta strutturalmente
(`NaturalEditor::setExpression`); Run-Matrix usa solo questa pipeline. Le voci
1, 3, 4, 6 e 8 sono state fatte nella v1.10.0 (con i limiti scritti accanto):

| # | Voce | Note |
|---|---|---|
| 1 | Impostazione **Input/Output Math/Line** (SET UP) | **fatto (v1.10.0)**; INS, ⌟ e risultati scorrevoli nella v1.11.0 |
| 2 | Round-trip garantito testo ↔ albero | **fatto (v1.12.0)**: `Group` per le parentesi che contengono modelli, `root(n,x)` con qualsiasi indice, modello d²/dx², Σ/Π con variabile e passo; corpus `serialize(import(t)) == t` in `editor_v112_test` |
| 3 | Cursore sul punto dell'errore dopo EXIT | **fatto (v1.10.0)**: `ErrorLocation`, `ast::SerializeMap`, `NaturalEditor::markError` |
| 4 | Selezione parziale CLIP (copia/taglia di un intervallo) | **fatto (v1.10.0)**, con le parentesi sempre bilanciate |
| 5 | SHIFT+INS: usare un'espressione esistente come argomento di un modello | **fatto (v1.11.0)** |
| 6 | Colori delle parentesi per livello (blu, rosso, verde, magenta, nero) | **fatto (v1.10.0)**: `NaturalDrawOp::color`, `math::Palette` |
| 7 | Regola del segno × obbligatorio prima di una frazione e parentesi implicite (`2 1/3` → `2(1/3)` nel lineare) | **fatto (v1.12.0)**: `ast::impliedFraction` |
| 8 | Modelli MAT/VCT modificabili nell'editor | **fatto (v1.10.0)** (fino a 6×6) |
| 9 | Visualizzazione in Graph/Table/Equation delle espressioni con gli stessi modelli | **fatto (v1.10.0)**: `math_renderer` condiviso |

Non anticipato in questa sessione per non interferire: nessuna modifica al
formato canonico del testo lineare esistente (salvo l'aggiunta di `mixed(…)` e
del suffisso periodico `_cifre`, già coperti da test di round-trip).

## 3. Motore matematico

| Funzione | Note |
|---|---|
| ~~Matrici, vettori e liste come valori del parser; MAT/VCT/List in Run-Matrix~~ | **fatto nella v1.10.0** (List 1–26 dalla v1.11.0) |
| ~~Simboli ingegneristici, Simp, Base-N intera, comandi grafici di Run-Matrix~~ | **fatti nella v1.11.0**, completati nella v1.12.0 |
| ~~Base-N completa (A–F in Hex, liste e matrici, bit per base)~~ | **fatto nella v1.12.0** |
| ~~Simboli P/E, ENG→/←ENG~~ | **fatto nella v1.12.0** |
| ~~Simplify Manual~~ | **fatto nella v1.12.0** (regole del manuale) |
| ~~RanList#, RanSamp#, Ran# con sequenze~~ | **fatto nella v1.12.0** |
| ~~SolveN, FMin/FMax, distribuzioni, CONVERT, TVM~~ | **fatto nella v1.12.0** (`calc_library`) |
| Derivate/integrali simbolici, limiti | fuori dall'ambito: la fx-CG50 non ha un CAS |

## 4. Modalità

Stato e progetto di ogni modalità in [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md).

| Modalità | Contenuto da realizzare |
|---|---|
| Statistics | regressioni sinusoidale e logistica; test χ², F, 2 campioni, ANOVA nell'interfaccia; grafici a torta e a barre |
| Graph | Dual Screen G+G/GtoT, Graph to Table, FORMAT nello SKETCH (i comandi grafici di Run-Matrix sono completi dalla v1.12.0) (dettaglio in [GRAPH_PARITY.md](GRAPH_PARITY.md)) |
| Table | Dual Screen T+G (LISTMEM con 999 elementi già sulla Plus; 255 sul Pico 2 W) |
| Equation | interfaccia per i sistemi di disequazioni; risultati esatti (servizio Exact) |
| Recursion, Conic Graphs, Dyna Graph | modelli dati pronti (`app_models.hpp`) e servizio GraphCore per il disegno; mancano controller e UI |
| Financial | schermate di TVM, flussi di cassa e ammortamento (le funzioni di calcolo sono in `calc_library` dalla v1.12.0 e Run-Matrix le usa) |
| Spreadsheet, eActivity, Program | modelli pronti; l'area file su microSD esiste (servizio Files, v1.8.0); Program richiede l'interprete |
| Geometry, Picture Plot | modelli pronti |
| 3D Graph | disponibile dalla v1.9.0 per z = f(x, y); mancano sfere, piani, rette e superfici parametriche della fx-CG50, TRACE sulla superficie |
| Link | trasferimento del record del workspace via USB seriale (formato proprio) |
| Python | MicroPython come libreria con heap separato |
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
