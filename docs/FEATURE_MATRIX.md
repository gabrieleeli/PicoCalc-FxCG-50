# Feature matrix — v1.8.0 (dopo il collaudo software di GRAPH, microSD e TABLE)

Stati (in italiano tra parentesi):

| Stato | Significato |
|---|---|
| **NOT STARTED** (mancante) | nessun codice |
| **IN PROGRESS** (in sviluppo) | codice parziale o solo modello dati |
| **IMPLEMENTED** (implementata) | codice presente, senza test automatici dedicati |
| **TESTED ON HOST** (testata su host) | test automatici su PC superati (stesso sorgente del firmware) |
| **TESTED ON SIMULATOR** (testata su simulatore) | in più, flusso verificato nel simulatore host con screenshot (`docs/screenshots/`) |
| **TESTED ON HARDWARE** (testata su hardware) | provata sulla PicoCalc fisica |
| **COMPLETE** | testata su hardware e con la funzionalità CG50 corrispondente completa |

Nessuna voce è TESTED ON HARDWARE: la PicoCalc fisica non è stata usata.
Nessuna voce è COMPLETE e il progetto **non** ha la parità con la fx-CG50.

## Piattaforma e hardware

| Funzione | Stato | Note |
|---|---|---|
| Build UF2 RP2350 (Pico SDK 2.2.0) | TESTED ON HOST | build eseguita, `picotool info` verificato |
| Driver LCD ST7365P (SPI1, init ufficiale, RGB666) | IMPLEMENTED | da collaudare; opzione RGB565 |
| Aggiornamento parziale LCD (tile 16×16) | TESTED ON HOST | `TileDiff`, test in app_test |
| DMA per il flush LCD | NOT STARTED | non conveniente: il collo di bottiglia è il bus SPI (vedi HARDWARE.md) |
| Tastiera STM32 I2C (FIFO, stati, modificatori) | IMPLEMENTED | decoder TESTED ON HOST; trasporto I2C da collaudare |
| Pressione prolungata / ripetizione | TESTED ON HOST | ripetizione generata dallo STM32, long-press nel decoder |
| Batteria e retroilluminazione | IMPLEMENTED | registri 0x05/0x0A/0x0B |
| Storage flash A/B con CRC | TESTED ON HOST | test di interruzione di alimentazione; driver Pico da collaudare |
| microSD (SPI0 GP16–19, card detect GP22) | IMPLEMENTED | protocollo SD, recupero dopo estrazione/errori e FatFs TESTED ON HOST con scheda simulata (`sd_fatfs_test`); volumi FAT16/FAT32/exFAT verificati con fsck.fat, fsck.exfat e mtools (`sd_interop_test`); bus SPI0 reale da collaudare ([STORAGE.md](STORAGE.md), [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)) |
| Diagnostica sul dispositivo (SYSTEM → DIAG: microSD, tempi GRAPH, log USB) | TESTED ON HOST | prova eseguita nel simulatore e nei test; serve per il collaudo fisico |
| Wi-Fi CYW43 | IMPLEMENTED | da collaudare |
| HTTPS (lwIP altcp_tls + mbedTLS, verifica obbligatoria) | IMPLEMENTED | parser HTTP TESTED ON HOST |
| Gestione energetica / sospensione | NOT STARTED | |

## Interfaccia

| Funzione | Stato |
|---|---|
| Menu principale a icone (23 modalità, accesso diretto 1–9/A–N) | TESTED ON SIMULATOR (accesso diretto verificato da architecture_test) |
| Registro delle modalità (stato, servizi, dati, collegamenti) e test di architettura | TESTED ON HOST |
| Barra di stato (angolo, display, complessi, Frac Result, batteria, Wi-Fi, SHIFT) | TESTED ON SIMULATOR |
| Tasti funzione F1–F6 stile CG50, SHIFT one-shot | TESTED ON HOST |
| Finestre di errore stile CG50 (Math/Syntax/Non-Real ERROR) | TESTED ON HOST |
| Menu popup, form, liste di risultati, griglie di celle | TESTED ON HOST |
| Font Unifont 8×16 e Spleen 6×12 (greco, simboli matematici) | TESTED ON HOST |
| Animazioni di transizione | IN PROGRESS (primitiva `ui::Animation`, non usata nelle schermate) |
| Simulatore host con screenshot PNG | TESTED ON HOST |

## Editor Natural V.P.A.M. e renderer 2D

| Funzione | Stato |
|---|---|
| Frazioni, radici quadrate e n-esime, potenze, indici, parentesi | TESTED ON HOST |
| \|x\|, logₐb, eˣ, 10ˣ, x², x⁻¹ | TESTED ON HOST |
| ∫ dx, d/dx, Σ, Π come modelli 2D | TESTED ON HOST |
| Navigazione dentro/fuori dai modelli, su/giù tra campi | TESTED ON HOST |
| Parole atomiche (sin, π, θ, Ans) | TESTED ON HOST |
| Undo / redo, copia / incolla (Ctrl+Z/Y/C/V) | TESTED ON HOST |
| Frazione mista `□ □/□`, radice con indice modificabile, periodo (linea sopra le cifre) | TESTED ON SIMULATOR |
| Selezione parziale con il cursore | NOT STARTED (copia/incolla dell'intera espressione) |
| Scorrimento orizzontale dell'input | TESTED ON HOST |
| Matrici/vettori come modelli 2D modificabili | NOT STARTED (solo rendering del nodo Matrix) |

## Motore matematico

| Funzione | Stato |
|---|---|
| Aritmetica, priorità Casio (moltiplicazione implicita prioritaria) | TESTED ON HOST |
| Funzioni prefisse senza parentesi (sin 2x) | TESTED ON HOST |
| Trigonometriche, inverse, iperboliche e inverse; Deg/Rad/Gra | TESTED ON HOST |
| Valori esatti a multipli di 90°/100 gon | TESTED ON HOST |
| ln, log, log₂, logₐb, exp, radici, potenze razionali di negativi | TESTED ON HOST |
| n!, nCr, nPr (senza overflow), gcd, lcm, mod, int, intg, frac, round | TESTED ON HOST |
| rand(), randint() | TESTED ON HOST |
| Variabili A–Z, Ans, PreAns, memorizzazione con →, anche →A~F | TESTED ON HOST |
| Forma esatta conservata in Ans/PreAns/A–Z | TESTED ON HOST |
| Variabili di altre modalità via workspace (VARS: V-WIN, TABLE) | TESTED ON SIMULATOR |
| Costanti matematiche e fisiche | TESTED ON HOST |
| Numeri complessi nel parser (a+bi, r∠θ, arg, conj, re, im) | TESTED ON HOST |
| Formati Norm1/Norm2/Fix/Sci/Eng (10 cifre) | TESTED ON HOST |
| Aritmetica esatta: razionali, radicali a√b/c (≤ 2 termini), multipli di π | TESTED ON HOST |
| Valutazione esatta sullo stesso AST (trigonometria ai multipli di 30°/45°, radici, log esatti, n!, nCr…) | TESTED ON HOST |
| S⇔D con valore interno preservato; regole di visualizzazione CG50 (10 caratteri, limiti radicali e π) | TESTED ON SIMULATOR |
| Frac Result d/c ↔ ab/c (impostazione e per risultato) | TESTED ON SIMULATOR |
| Numeri periodici: vista del periodo e input periodico | TESTED ON SIMULATOR |
| Arrotondamento di visualizzazione come la calcolatrice (mantissa 15 cifre, per eccesso a metà) | TESTED ON HOST |
| Operatori relazionali e logici (= ≠ < > ≤ ≥, And, Or, Xor, Not) | TESTED ON HOST |
| Int÷, Rmdr, MOD_Exp, RndFix, RanNorm#, RanBin# | TESTED ON HOST |
| Simboli ingegneristici (k, M, m, µ…), Simp manuale | NOT STARTED |
| Base-N (letterali 0x/0b/0o, visualizzazione Hex/Bin/Oct) | TESTED ON HOST |
| Modalità Base-N intera (Mode Hex/Bin/Oct) con operatori bit a bit | NOT STARTED (viste Hex/Bin/Oct dei risultati presenti) |
| Sessagesimale (dms, °, visualizzazione DMS) | TESTED ON HOST |
| Pol / Rec | TESTED ON HOST |
| Integrali (Gauss-Kronrod adattivo), derivate (Ridders), d²/dx² | TESTED ON HOST |
| Σ, Π, SOLVE nel parser | TESTED ON HOST |
| Derivate/integrali simbolici, limiti | NOT STARTED |
| Matrici: somma, prodotto, det, inversa, trasposta, traccia, RREF, sistemi (API C++) | TESTED ON HOST |
| Matrici come valori nel parser e editor MAT in Run-Matrix | NOT STARTED |
| Vettori: somma, prodotto scalare/vettoriale, norma, angolo (API C++) | TESTED ON HOST |
| Conversioni metriche (10 grandezze, prefissi SI) | TESTED ON HOST |
| Limiti di lunghezza/annidamento e budget di stack ("Stack ERROR", "Memory ERROR") | TESTED ON HOST (budget del firmware verificato solo con `-fstack-usage`) |

## Modalità

| Modalità | Stato | Note |
|---|---|---|
| Run-Matrix | TESTED ON SIMULATOR | forme esatte, S⇔D, frazioni miste, periodo, cronologia modificabile con ricalcolo, OPTN, VARS, Base-N, DMS (dettagli in RUN_MATRIX.md) |
| Run-Matrix: cronologia modificabile sul posto con ricalcolo delle righe successive | TESTED ON SIMULATOR | |
| Run-Matrix: matrici/vettori/liste (MAT/VCT/List) | NOT STARTED | API C++ esistenti |
| Statistics | TESTED ON HOST | liste, 1/2-Var, 9 regressioni, grafici, DIST, test Z/t, intervalli |
| Regressioni sinusoidale e logistica | NOT STARTED | |
| Test χ², F, 2-campioni, ANOVA nell'interfaccia | NOT STARTED | ANOVA e t a 2 campioni esistono come API C++ |
| Grafico a torta / a barre | NOT STARTED | |
| Graph: 20 funzioni Y=, r=, Param, X=, 8 disequazioni, intervalli, sovrapposizioni, liste, funzioni composte | TESTED ON SIMULATOR | matrice completa in [GRAPH_PARITY.md](GRAPH_PARITY.md) |
| Graph: TRACE, ZOOM (13 voci), V-Window e memorie, SKETCH (12 voci), G-SOLVE (ROOT…∫dx MIXED), Modify, memorie grafiche, PICT, sfondo | TESTED ON SIMULATOR / TESTED ON HOST | voce per voce in GRAPH_PARITY.md |
| Graph: progetti `.pcgraph` su microSD (NEW/OPEN/SAVE/SAVE AS/RENAME/DELETE) | TESTED ON SIMULATOR | scheda simulata; formato in [PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md) |
| Graph: Dual Screen G+G/GtoT, Graph to Table, comandi grafici di Run-Matrix | NOT STARTED | |
| Grafici impliciti (f(x,y)=0) | NOT STARTED | non presenti nella modalità Graph della CG50 (Conic Graphs) |
| Table: lista delle relazioni, Y=/r=/Param, SET, Variable Range/List, derivate, celle di 6 caratteri | TESTED ON SIMULATOR | [TABLE.md](TABLE.md), screenshot 32, 35–37 |
| Table: EDIT, ROW DELETE/INSERT/ADD, DELETE, LISTMEM, GPH-CON/GPH-PLT dalle righe | TESTED ON HOST | `table_test` |
| Table: Dual Screen T+G | NOT STARTED | sessione v1.9.0 |
| Equation: sistemi 2–6, polinomi 2–6, solver | TESTED ON HOST | |
| Disequazioni polinomiali 1–6 | TESTED ON HOST | |
| Sistemi di disequazioni (interfaccia) | NOT STARTED | API `solveInequalitySystem` (grado ≤ 2) TESTED ON HOST |
| System / Set Up | TESTED ON HOST | |
| Memory | TESTED ON HOST | |
| Conversion | TESTED ON HOST | |
| AI Chat (Claude / ChatGPT) | TESTED ON HOST | con trasporto simulato; rete reale da collaudare |
| Symbols (guida) | TESTED ON HOST | |
| eActivity, Spreadsheet, Dyna Graph, Recursion, Conic Graphs | IN PROGRESS | solo architettura e modello dati (`app_models.hpp`, testato); nessuna interfaccia; schermata "non disponibile" |
| Program, Financial, Link, Geometry, Picture Plot, 3D Graph | IN PROGRESS | idem |
| E-CON4 | NOT STARTED | richiede un data logger esterno: non realizzabile sulla PicoCalc (stato External) |
| Python (MicroPython) | NOT STARTED | richiede l'integrazione del runtime come libreria (modello dei file di script definito) |

## Persistenza

| Dato | Stato |
|---|---|
| Impostazioni, variabili, Ans | TESTED ON HOST |
| Storia Run-Matrix (con forma esatta, vista, frazione mista) | TESTED ON HOST |
| Lettura dei record v1.6.0 (formato versione 1) | TESTED ON HOST |
| Liste statistiche | TESTED ON HOST |
| Stato di GRAPH (20 funzioni, V-Window, Setup, memorie V-Window e grafiche, SKETCH) e di TABLE (range, variabile, tipo, righe) — record v4 | TESTED ON HOST |
| Lettura dei record v3 (GRAPH v1.8.0 prima di TABLE) | TESTED ON HOST |
| Record oltre la capacità della flash: messaggio "Memoria flash piena" con la dimensione | TESTED ON HOST |
| Lettura dei record v1.7.0 (versione 2) e correzione delle V-Window legacy | TESTED ON HOST |
| Progetti `.pcgraph` e immagini `.pcpict`/BMP sulla microSD, scrittura atomica e recupero | TESTED ON HOST (scheda simulata, pila completa GraphApp → FatFs → SD) |
| Coefficienti Equation | TESTED ON HOST |
| Conversazioni AI (cancellabili da AI Chat e Memory) | TESTED ON HOST |
| Programmi, file Python, documenti, matrici salvate | NOT STARTED |

## Relay AI

| Funzione | Stato |
|---|---|
| Claude (Anthropic SDK, `claude-opus-5-5`, fallback server-side sui rifiuti) | TESTED ON HOST (API simulata) |
| ChatGPT (OpenAI SDK, modello configurato dall'operatore) | TESTED ON HOST (API simulata) |
| Token per dispositivo (hash SHA-256), limiti di dimensione e frequenza | TESTED ON HOST |
| TLS integrato con verifica del certificato | TESTED ON HOST |
| Prova con chiavi reali | NOT STARTED (richiede chiavi e credito dell'utente) |
