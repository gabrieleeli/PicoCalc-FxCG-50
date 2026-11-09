# Roadmap — dopo la v1.9.0

Il progetto **non** ha la parità con la Casio fx-CG50 e non va presentato come
tale: le tabelle sotto elencano ciò che manca. Il documento storico
`archive/CG50_PARITY_ROADMAP.md` descrive gli obiettivi delle versioni precedenti.
La v1.9.0 (`main`) ha unito la v1.8.0 al supporto del Pimoroni Pico Plus 2 W
([reports/CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md)). Le release
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
999 elementi sulla Plus. Restano, in ordine:

| # | Voce | Note |
|---|---|---|
| 0 | **Esito del collaudo fisico** della v1.9.0 sulla Pico Plus 2 W | procedure [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md); correggere i KO prima del resto e aggiornare a TESTED ON HARDWARE solo le voci provate |
| 1 | Dual Screen G+G (COPY, SWAP), GtoT e T+G, Graph to Table | `graph::Viewport` accetta già un'area diversa da 320×282 |
| 2 | Dyna Graph, Recursion, Conic Graphs | sul servizio GraphCore (`sampleStep`, `Viewport`, G-Solve, `table_model` per le tabelle delle successioni) |
| 3 | FORMAT durante lo SKETCH, suffissi angolari °/ʳ/ᵍ | piccoli completamenti di GRAPH |
| 4 | Editor MAT di Run-Matrix e matrici grandi | l'API supporta matrici fino a 999×999 con la PSRAM; le liste da 999 elementi esistono già sulla Plus (sul Pico 2 W 255, record da 64 KB) |
| 5 | Punto 3: Natural V.P.A.M. ↔ formato lineare | descritto sotto |

### Punto 3: Natural V.P.A.M. ↔ formato lineare

Stato attuale (v1.8.0): editor e parser condividono `ast::Node`; l'editor produce
il testo lineare canonico (`ast::serialize`) e lo reimporta strutturalmente
(`NaturalEditor::setExpression`); Run-Matrix usa solo questa pipeline. Da fare:

| # | Voce | Note |
|---|---|---|
| 1 | Impostazione **Input/Output Math/Line** (SET UP) | input lineare con inserimento/sovrascrittura (INS) e risultati lineari, come la CG50 |
| 2 | Round-trip garantito testo ↔ albero | oggi `(x)` torna come token e non come nodo `Group`, `root(n,x)` con indice non letterale torna come lettere, d²/dx² non ha modello; servono test su un corpus: `serialize(import(t)) == t` e stesso disegno |
| 3 | Cursore sul punto dell'errore dopo EXIT | mappa di posizioni tra testo serializzato e albero (offset del token che ha causato l'errore) |
| 4 | Selezione parziale CLIP (copia/taglia di un intervallo) | limitata ai movimenti del cursore come sulla CG50 |
| 5 | SHIFT+INS: usare un'espressione esistente come argomento di un modello | manuale 1-20 |
| 6 | Colori delle parentesi per livello (blu, rosso, verde, magenta, nero) | richiede il colore nelle `NaturalDrawOp` |
| 7 | Regola del segno × obbligatorio prima di una frazione e parentesi implicite (`2 1/3` → `2(1/3)` nel lineare) | manuale 1-23 e 2-7 |
| 8 | Modelli MAT/VCT modificabili nell'editor | prerequisito delle matrici nel parser (§3) |
| 9 | Visualizzazione in Graph/Table/Equation delle espressioni con gli stessi modelli | riuso di `math_renderer` |

Non anticipato in questa sessione per non interferire: nessuna modifica al
formato canonico del testo lineare esistente (salvo l'aggiunta di `mixed(…)` e
del suffisso periodico `_cifre`, già coperti da test di round-trip).

## 3. Motore matematico

| Funzione | Note |
|---|---|
| Matrici, vettori e liste come valori del parser; MAT/VCT/List in Run-Matrix | le API C++ esistono e sono testate |
| Modalità Base-N intera (Mode Dec/Hex/Bin/Oct) con and/or/xor/xnor/Not/Neg bit a bit | oggi solo viste Hex/Bin/Oct e letterali 0x/0b/0o |
| Simboli ingegneristici (k, M, m, µ, …) e ENG→/←ENG | |
| Simplify Manual e `Simp` | oggi semplificazione automatica |
| RanList#, RanSamp#, Ran# con sequenze | richiedono le liste come valori |
| Derivate/integrali simbolici, limiti | |
| SolveN, FMin/FMax | |

## 4. Modalità

Stato e progetto di ogni modalità in [APPLICATIONS_ARCHITECTURE.md](APPLICATIONS_ARCHITECTURE.md).

| Modalità | Contenuto da realizzare |
|---|---|
| Statistics | regressioni sinusoidale e logistica; test χ², F, 2 campioni, ANOVA nell'interfaccia; grafici a torta e a barre |
| Graph | Dual Screen G+G/GtoT, Graph to Table, comandi grafici di Run-Matrix/programmi, FORMAT nello SKETCH (dettaglio in [GRAPH_PARITY.md](GRAPH_PARITY.md)) |
| Table | Dual Screen T+G (LISTMEM con 999 elementi già sulla Plus; 255 sul Pico 2 W) |
| Equation | interfaccia per i sistemi di disequazioni; risultati esatti (servizio Exact) |
| Recursion, Conic Graphs, Dyna Graph | modelli dati pronti (`app_models.hpp`) e servizio GraphCore per il disegno; mancano controller e UI |
| Financial | TVM (modello pronto), flussi di cassa, ammortamento |
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
