# Rapporto di sviluppo — v1.13.0 (completamento totale di Run-Matrix)

Base: `main` = tag `v1.11.0` = commit `f6933fd` (verificato prima di iniziare:
nessun commit successivo al tag). Prima della pubblicazione `main` era
avanzato al commit `51691de` (v1.12.0, completamento di EQUATION, sviluppato
in parallelo da un'altra sessione, record v8): `main` è stato unito nel
branch di sessione (merge, nessuna riscrittura), la versione è stata
rinumerata in **1.13.0**, il record in **v9** e i documenti in `_v1.13.0`;
tutti i test sono stati rieseguiti sul risultato (§4). Punti di contatto
risolti nel merge: VARS › EQUATION usa le matrici SimRes/SimCoef/PlyRes/PlyCoef
della v1.12.0 (il mio calcolo separato è stato tolto); le parentesi implicite
delle frazioni uniscono le due regole (frazione accostata a un operando prima
o dopo, `ast::detail::implicitFactor`); i titoli degli errori senza «ERROR»
uniscono i due elenchi. Analisi preliminare e perimetro:
[ANALISI_FONTI_v1.13.0.md](ANALISI_FONTI_v1.13.0.md). Riferimento di
comportamento: manuale software fx-CG50 (cap. 1, 2, 3, 5, 6, 7, 8) e, per
Simplify, i manuali fx-7400G e fx-9750GII della stessa famiglia; nessun
codice, ROM, font o grafica Casio.

## 1. Cosa è stato fatto

| Gruppo (§4) | Contenuto |
|---|---|
| 4.1 Liste | `List "QTY"` nei calcoli e come destinazione; **List File 1–6** (SET UP, budget comune per scheda); `n→Dim List k`; `Fill(v, List n)` modifica la lista; Min/Max di due liste; Mean/Median con frequenze; S-Dev(, Var(; SET UP **Sub Name** |
| 4.2 Simboli | ESYM **P** (10¹⁵) ed **E** (10¹⁸), anche nel display Eng; **ENG / ENG←** come viste del risultato |
| 4.3 Simplify Manual | frazioni non ridotte per tutte le operazioni (+ − sul m.c.m. dei denominatori, × ÷ sui prodotti, potenze intere), solo nel modo Real; ▶Simp su una frazione irriducibile restituisce il valore senza «F=», un divisore non valido è un errore |
| 4.4 Base-N | **Bin a 16 bit** (Dec/Oct/Hex 32 bit); variabili A–F in Hex distinte dalle cifre (`v_a`, VARS o Alt, mostrate sottolineate); liste e matrici mostrate nella base della riga; messaggi e limiti verificati per le quattro basi |
| 4.5 MAT/VCT | forme esatte **senza il limite di 64 elementi** (budget per scheda `limits::kExactStoreElements`: 400 / 4 000 forme); una cella digitata come decimale resta decimale; Fill(v, Mat A); comandi **Swap, \*Row, \*Row+, Row+** |
| 4.6 Comandi grafici | **PxlTest(**, **Graph Y> Y< Y≥ Y≤ X> X< X≥ X≤**, **G∫dx**, prefissi **SketchNormal/Thick/Broken/Dot/Thin**, **StoPict / RclPict / BG-Pict / RclCapt**, **DrawGraph, DrawStat, DrawDyna, DrawR-Con/Plt, DrawWeb, DrawFTG-Con/Plt**, **G-SelOn/Off**; disegnati dal motore e dal renderer di GRAPH; **decisione: i grafici dei comandi vanno nei progetti `.pcgraph`** (righe `c=`) |
| 4.7 Math/Line | conversione testo ↔ albero esatta nelle due direzioni (gruppi con modelli, radice con indice qualsiasi, d²/dx², Σ/Π con la variabile, nuovi token) con un corpus di test; **parentesi implicite** di una frazione accostata a un operando (`2[1/3]` → `2((1)/(3))`: `6÷2[1/3]` = 9 come sulla calcolatrice); testo Line con le parentesi minime (`2(1/3)`, `x^2`); nuovi modelli **d²/dx²** e **Σ/Π con il campo della variabile** |
| 4.8 Funzioni | CALC: **Solve con limiti** e forma `a=b`, **SolveN**, **FMin**, **FMax**, **Σ(f,k,a,b[,n])**, ∫ con tolleranza, divieto delle funzioni annidate (2-28); PROB: **Ran# n**, RanInt#/RanNorm#/RanBin# con n, **RanList#**, **RanSamp#**, **P( Q( R( t(**; NUMERIC **Rnd**; ANGLE **ʳ ᵍ**, **° ′ ″**, **Pol/Rec in List Ans**; COMPLEX **∠**, **►r∠θ / ►a+bi**; STAT **x̂ ŷ** e le 24 **distribuzioni**; **TVM** (22 funzioni); **CONVERT** (11 categorie, 103 unità); overflow |x| ≥ 10¹⁰⁰ |
| 4.9 Cronologia ed errori | istruzioni multiple **`:`** e **`◢`** (risultati intermedi in cronologia); **stringhe** e **Str 1–20**; **function memory f1–f20** (FUNCMEM STORE/RECALL/fn/SEE); variabili **r e θ**; **variabili di sistema assegnabili** (V-Window, FACTOR, TABLE, DYNA, RECURSION, TVM); VARS **FACTOR, STAT, GRAPH, DYNA, TABLE Result, RECURSION, EQUATION, TVM, Str**; **CATALOG** (SHIFT 4); **CAPTURE** (SHIFT 7) e RclCapt; messaggi dell'Appendice α (niente «Type ERROR», `Complex Number In List/Matrix`, `Can't Solve!`, `Iteration ERROR`, `Range ERROR`) |
| OPTN | riordinato come la fx-CG50: LIST, MAT/VCT, COMPLEX, CALC, STAT, CONVERT, HYPERBL, PROB, NUMERIC, ANGLE, ESYM, PICTURE, FUNCMEM, LOGIC, CAPTURE, TVM (poi SKETCH, relazioni, viste del risultato, Base-N, costanti) |
| Workspace | **record v9** in coda al v8 della v1.12.0; v1–v8 letti senza perdite; fixture generate con i sorgenti della v1.11.0 (`tests/fixtures/workspace_v1_11_0_run_matrix.inc`, v7) e della v1.12.0 (`tests/fixtures/workspace_v1_12_0.inc`, v8) |
| Font | glifi disegnati dal progetto: ◢, ʳ, ᵍ, ′, ″, ŷ, x̂, x̄, ȳ, Å (`tools/gen_fonts.py`, MANUAL_GLYPHS) |

## 2. Architettura

- **Un solo parser, un solo AST, un solo valutatore.** Tutti i comandi nuovi
  sono nodi `Call` dello stesso parser (`src/expression.cpp`); unità
  (`10[cm]▶[in]`), `∠`, `ʳ`, `ᵍ`, `° ′ ″`, stringhe `"…"`, `v_a`, `r_var`,
  `theta`, `f_1`… sono token dello stesso lexer; SolveN/FMin/FMax/Pol/Rec e le
  liste casuali sono metodi di `CollEval`; le frazioni non ridotte
  (`evaluateUnreducedFraction`) sono un secondo dominio sullo stesso AST, come
  `evaluateExact`. `:` e `◢` separano le istruzioni prima del parser e ognuna
  passa dal parser unico.
- **`calc_library`** (nuovo, motore puro): unità di CONVERT, distribuzioni,
  TVM, nomi mostrati e CATALOG; il test di architettura verifica che non usi
  workspace né parser.
- **Dati solo nel workspace** tramite interfacce: `CollectionResolver::listByName`
  e `special` (TABLE Result; VARS › EQUATION da `collection(Memory::Equation)` della v1.12.0), `ServiceResolver` (PxlTest, function
  memory), `NameResolver` per le variabili di VARS; nuova sezione dati
  `Strings` nel registro delle app.
- **PxlTest(**: l'AppManager collega `Workspace::pixelProbe` a
  `GraphApp::probePixel`, che disegna la schermata del grafico nel framebuffer
  (la «VRAM») e legge il pixel; Run-Matrix si ridisegna subito dopo.
- **`architecture_test` più severo**: nessun altro sorgente definisce lexer,
  parser o istanze del valutatore; `calc_library` puro; Run-Matrix non tocca
  `ws.listFiles`, `ws.listNames`, `ws.modeVars`, `ws.functionMemories`,
  `pixelProbe`, `selectListFile`, `statistics.hpp`, `solvers.hpp`,
  `table_model.hpp`; i comandi grafici usano il parser unico.

## 3. Memoria per scheda (`limits.hpp`)

| Limite | Pico 2 W | Pico Plus 2 W |
|---|---|---|
| Elementi di tutte le liste di tutti i List File | 1 530 | 20 000 |
| Forme esatte di tutte le memorie di matrici/vettori/risposte | 400 | 4 000 |
| Punti di DrawStat / DrawR / DrawFTG nella schermata | 1 000 | 4 000 |
| Function memory, Str | 20 × 255 caratteri | 20 × 255 caratteri |

Nuovi oggetti grandi: nessuno sullo stack (gli argomenti delle funzioni sono un
array fisso di 8; SolveN e FMin campionano senza vettori temporanei grandi).

## 4. Test eseguiti

| Prova | Esito |
|---|---|
| `tests/run_host_tests.sh` (profili Plus e Pico 2 W) | 45/45 per profilo: 43 suite C++ (12 081 controlli sulla Plus, 12 075 sul Pico 2 W, più ≈ 334 000 verifiche dello stress dell'allocatore) e l'interoperabilità FAT/exFAT |
| `--sanitize --tsan` | vedi [BUILD_STATUS.md](../BUILD_STATUS.md) |
| Relay AI | 15/15 |
| Nuove suite | `calc_v112_test` (gruppi 4.2–4.4, 4.8), `run_matrix_v112_test` (4.1, 4.5, 4.9, record v9 e migrazione v8→v9, interfaccia), `graph_cmd_v112_test` (4.6, PxlTest, `.pcgraph`), `editor_v112_test` (4.7, corpus di round-trip, glifi) |
| Simulatore | `tests/sim/run_matrix_v112.txt`, screenshot 77–87 (PNG a palette, compressione senza perdita verificata pixel per pixel) |
| Build firmware | vedi [BUILD_STATUS.md](../BUILD_STATUS.md) |

Nessuna prova su hardware: procedure in
[HARDWARE_TEST_v1.13.0.md](../HARDWARE_TEST_v1.13.0.md).

## 5. Tabella di parità finale

**P** presente, **I** differenza inevitabile documentata (§6). Le voci sono
quelle dell'[analisi](ANALISI_FONTI_v1.13.0.md#2-tabella-di-parità-perimetro-del-lavoro).

| # | Voce | v1.11.0 | v1.13.0 | Note |
|---|---|---|---|---|
| B1–B2 | aritmetica, priorità, Fix/Sci/Norm/Eng | P | P | |
| B3 | overflow ≥ 10¹⁰⁰ | D | P | ogni passaggio del valutatore |
| B4 | variabili r e θ | D | P | `r` da VARS (token `r_var`, mostrato «r»): **I** per il modo di digitarla |
| B5–B6 | →, A~F, Ans | P | P | |
| B7–B8 | `:` e `◢` | M | P | |
| B9 | function memory f1–f20 | M | P | |
| B10–B11 | ripetizione, cronologia, CLIP | P | P | |
| S1–S6 | SET UP Input/Output, Mode, Frac Result, Angle, Complex, Display | P | P | |
| S7 | Simplify | D | P | regola §6 |
| S8 | List File | M | P | |
| S9 | Sub Name | M | P | |
| S10–S11 | Sketch Line e voci di GRAPH | P | P | |
| F1–F4, F7, F9, F15, F17, F18, F21, F22 | funzioni già presenti | P | P | |
| F5 | ʳ ᵍ | M | P | |
| F6 | ° ′ ″ in input | D | P | |
| F8 | Pol/Rec in List Ans | D | P | |
| F10 | Ran# n | M | P | valori delle sequenze: **I** (generatore Casio non pubblico) |
| F11–F12 | RanInt#/RanNorm#/RanBin# con n | D | P | |
| F13–F14 | RanList#, RanSamp# | M | P | |
| F16 | P( Q( R( t( | M | P | |
| F19 | ESYM P, E | M | P | |
| F20 | ENG / ENG← | M | P | |
| F23 | Rnd | D | P | |
| C1–C2, C4 | Int÷ Rmdr Simp d/dx | P | P | |
| C3 | Solve con limiti ed equazione | D | P | |
| C5 | d²/dx² modello | D | P | |
| C6 | ∫ con tolleranza | D | P | |
| C7 | SolveN | M | P | senza limiti cerca tra ±10¹⁰ **(scelta)** |
| C8 | FMin / FMax | M | P | |
| C9 | Σ con variabile e passo, modello | D | P | |
| C10 | funzioni vietate in Solve… | D | P | Syntax ERROR |
| X1 | i Abs Arg Conjg ReP ImP | P | P | |
| X2–X3 | ►r∠θ ►a+bi, input ∠ | M | P | |
| X4 | Arg sugli elementi | D | P | |
| R1, R3 | Simplify Auto, ▶Simp | P | P | |
| R2 | Simplify Manual | D | P | |
| N1, N3 | Base-N 32 bit, prefissi, operatori | P | P | |
| N2 | Bin 16 bit | D | P | |
| N4 | variabili A–F in Hex | D | P | inserimento da VARS/Alt: **I** (manca ALPHA) |
| N5 | liste e matrici in Base-N | D | P | |
| N6 | messaggi Base-N | P | P | |
| T1, T4, T7, T8 | editor e funzioni MAT/VCT | P | P | |
| T2 | forme esatte senza 64 | D | P | budget per scheda |
| T3 | decimale digitato | D | P | |
| T5 | Fill su Mat | D | P | |
| T6 | Swap, \*Row, \*Row+, Row+ | M | P | |
| L1, L6, L9, L10 | liste già presenti | P | P | |
| L2 | List "QTY" | M | P | |
| L3 | List File | M | P | |
| L4 | n→Dim List | D | P | |
| L5 | Fill su List | D | P | |
| L7 | Min/Max di due liste | M | P | (funzionava già elemento per elemento; ora verificato) |
| L8 | Mean/Median con frequenze | M | P | |
| L11 | 999 elementi | P / I | P / I | 255 sul Pico 2 W |
| D1 | x̂ ŷ | M | P | regressione dell'ultimo calcolo di Statistics |
| D2 | distribuzioni | M | P | |
| D3 | S-Dev, Var | M | P | |
| U1 | CONVERT | M | P | integrato (sulla calcolatrice è un add-in) |
| O1 | PICTURE | M | P | |
| O2 | FUNCMEM | M | P | |
| O3 | CAPTURE / RclCapt | M | P | catture come file PICOCALC/CAPT |
| O4 | TVM | M | P | anno di 365 giorni, pagamento a fine periodo (predefiniti di Financial) |
| V1, V5 | V-WIN, GRAPH | P | P | |
| V2 | V-WIN in scrittura | M | P | Range ERROR |
| V3–V4, V6, V8–V11 | FACTOR, STAT, DYNA, RECURSION, EQUATION, TVM, Str | M | P | EQUATION: SimRes… della v1.12.0 |
| V7 | TABLE in scrittura e Result | D | P | |
| G1 | comandi della v1.11.0 | P | P | |
| G2 | PxlTest( | M | P | righe 1–282, colonne 1–320: **I** |
| G3 | disequazioni | M | P | |
| G4 | G∫dx | M | P | |
| G5 | stili Sketch… | M | P | |
| G6 | StoPict / RclPict / BG-Pict | M | P | |
| G7 | DrawStat | M | P | StatGraph1 predefinito (Scatter di XList/YList) **(scelta)** |
| G8 | DrawGraph, DrawDyna, DrawR, DrawWeb, DrawFTG | M | P | Dyna: Y1 con il coefficiente A **(scelta)**; Recursion: formula an+1 da `"…"→an+1` |
| G9 | G-SelOn/Off | M | P | |
| G10 | comandi solo dei programmi | — | — | fuori perimetro (non accettati in Run-Matrix) |
| G11 | comandi nei `.pcgraph` | D | P | decisione: sì |
| E1, E6 | modelli e INS/CLIP | P | P | |
| E2–E3 | d²/dx², Σ con variabile | M/D | P | |
| E4 | round-trip esatto | D | P | corpus di 68 testi + 13 alberi di modelli |
| E5 | × prima di una frazione, parentesi implicite | M | P | |
| E7 | CATALOG | M | P | |
| ER1, ER6 | messaggi e cursore | P | P | |
| ER2 | niente Type ERROR | D | P | Argument ERROR |
| ER3 | Complex Number In List/Matrix | D | P | |
| ER4 | Can't Solve! / Iteration ERROR | D | P | **(scelta)** sulla ripartizione tra i due |
| ER5 | Range ERROR | D | P | |

Voci rimaste «diverse»: **nessuna**. Le scelte segnate **(scelta)** sono
comportamenti che il manuale non descrive per intero; le differenze
inevitabili sono in §6.

## 6. Differenze inevitabili e scelte

| Voce | Differenza | Motivo |
|---|---|---|
| Schermo | area grafica 320×282 (PxlOn/PxlTest/Text: riga 1–282, colonna 1–320) | display PicoCalc 320×320 |
| Tastiera | SHIFT = F7, OPTN = F8, VARS = F9, SET UP = F10, CATALOG = SHIFT 4, CAPTURE = SHIFT 7, INS = SHIFT DEL | tastiera PicoCalc |
| Variabili A–F in Hex, r | inserite da VARS (o Alt+lettera), mostrate sottolineate / «r» | senza ALPHA le lettere digitate sono cifre (Hex) o la variabile R |
| Ran# n | sequenze riproducibili ma con valori diversi da quelli della calcolatrice | generatore Casio non pubblico |
| CONVERT | integrato | sulla fx-CG50 è un add-in |
| Simplify Manual | denominatore = m.c.m. per + e −, prodotti per × e ÷ | regola dei manuali fx-7400G («least common multiple») e fx-9750GII; il manuale fx-CG50 non dà esempi completi |
| SolveN senza limiti | ricerca tra −10¹⁰ e 10¹⁰ | intervallo predefinito non documentato |
| DrawStat / DrawDyna | StatGraph1 Scatter; Y1 con il coefficiente A | le modalità Statistics (impostazioni StatGraph) e Dyna Graph del progetto non hanno quei dati |
| Liste sul Pico 2 W | 255 elementi, 1 530 in tutti i file | 520 KB di SRAM, record da 59,75 KB |
| Font e grafica | glifi e grafica propri | proprietà Casio |

## 7. Problemi rimasti

- Nessuna prova su hardware PicoCalc (HW-140…HW-159 in
  [HARDWARE_TEST_v1.13.0.md](../HARDWARE_TEST_v1.13.0.md)).
- PxlTest( ridisegna la schermata di GRAPH nel framebuffer a ogni chiamata:
  in un ciclo lungo è lento (sul Cortex-M33 da misurare con DIAG).
- Gli screenshot 01–61 non sono stati rigenerati: mostrano i menu OPTN della
  v1.11.0 dove compaiono.
- Modalità Program, Dyna Graph, Recursion, Financial non realizzate: i loro
  dati usati da Run-Matrix (VARS) sono nel workspace, le schermate no.
