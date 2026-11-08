# Roadmap — dopo la v1.7.0

Il progetto **non** ha la parità con la Casio fx-CG50 e non va presentato come
tale: le tabelle sotto elencano ciò che manca. Il documento storico
`archive/CG50_PARITY_ROADMAP.md` descrive gli obiettivi delle versioni precedenti.
Le release successive (v1.8.0, v1.9.0, v2.0.0) si creano solo quando i rispettivi
incrementi sono completati e verificati.

## 1. Collaudo su hardware (priorità massima, richiede la PicoCalc)

Nessuna funzione è stata provata sulla PicoCalc. Procedura in
[HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo): display
(RGB666/RGB565, orientamento), tastiera (codici, ripetizione, tempi I2C), flash
(salvataggio, blackout), batteria, Wi-Fi/TLS/relay, stabilità. Per Run-Matrix:
tempi di risposta della valutazione esatta (fattorizzazione di radicandi grandi)
e del ricalcolo della cronologia (40 righe).

## 2. Prossima sessione — Punto 3: Natural V.P.A.M. ↔ formato lineare

Stato attuale (v1.7.0): editor e parser condividono `ast::Node`; l'editor produce
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
| Graph | grafici parametrici, X=f(y); servizio di plotting condiviso per Dyna/Recursion/Conic |
| Equation | interfaccia per i sistemi di disequazioni; risultati esatti (servizio Exact) |
| Recursion, Conic Graphs, Dyna Graph | modelli dati pronti (`app_models.hpp`); manca controller + UI |
| Financial | TVM (modello pronto), flussi di cassa, ammortamento |
| Spreadsheet, eActivity, Program | modelli pronti; richiedono un'area file (SD o flash) e, per Program, l'interprete |
| Geometry, Picture Plot, 3D Graph | modelli pronti; 3D usa `sample3DSurface()` esistente |
| Link | trasferimento del record del workspace via USB seriale (formato proprio) |
| Python | MicroPython come libreria con heap separato |
| E-CON4 | non realizzabile (data logger esterno); al più import CSV nelle liste |

## 5. Piattaforma

- Scheda SD (FAT) per programmi, documenti e backup del workspace.
- Gestione energetica: spegnimento del display, sospensione, risveglio da tastiera.
- DMA e clock SPI più alto per il display, se il collaudo lo consente.
- Animazioni di transizione tra le schermate (primitiva già presente).

## 6. AI

- Prova con chiavi reali e misura dei tempi di risposta dal Pico.
- Eventuale streaming della risposta (oggi la risposta arriva intera).
