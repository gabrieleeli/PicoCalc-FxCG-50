# Feature matrix — v1.12.0

La v1.12.0 porta Run-Matrix alla parità con la fx-CG50: ogni voce della
tabella di parità è presente o è una differenza inevitabile documentata
([reports/SVILUPPO_v1.12.0.md](reports/SVILUPPO_v1.12.0.md#5-tabella-di-parità-finale)).
La v1.11.0 contiene tutte le funzioni della v1.10.0 più il completamento di
Run-Matrix: List 1–26 con i sottonomi, file CSV di liste e matrici,
schermate MatAns/ListAns, celle esatte e complesse nell'editor MAT/VCT,
modalità Line con INS, ⌟ e risultati scorrevoli, SHIFT+INS, simboli
ingegneristici e display Eng, Simp, Base-N intera e comandi grafici
([reports/SVILUPPO_v1.11.0.md](reports/SVILUPPO_v1.11.0.md)). v1.10.0:
matrici, vettori e liste nel parser, editor MAT/VCT, Math/Line, CLIP,
parentesi colorate, cursore sugli errori
([reports/SVILUPPO_v1.10.0.md](reports/SVILUPPO_v1.10.0.md)). Origine dei
componenti della v1.9.0: [reports/CONSOLIDAMENTO_v1.9.0.md](reports/CONSOLIDAMENTO_v1.9.0.md).

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

Nessuna voce è TESTED ON HARDWARE: la PicoCalc fisica non è stata usata (né
con il Pico 2 W né con il Pimoroni Pico Plus 2 W). "TESTED ON HOST" vale per
entrambi i profili di scheda: la suite gira con `FXAI_TEST_BOARD=PICO_PLUS_2W`
e `PICO2W`.
Nessuna voce è COMPLETE e il progetto **non** ha la parità con la fx-CG50.

## Piattaforma e hardware

| Funzione | Stato | Note |
|---|---|---|
| Build UF2 Pimoroni Pico Plus 2 W (RP2350B, `pimoroni_pico_plus2_w_rp2350`) | TESTED ON HOST | build eseguita senza warning nei sorgenti del progetto; controllo della dimensione nella partizione firmware |
| Build UF2 Raspberry Pi Pico 2 W (profilo alternativo) | TESTED ON HOST | build eseguita senza warning; UF2 con nomi distinti per scheda |
| Selezione della scheda (`FXAI_BOARD`), cartelle di build separate, `board_config.hpp` con `static_assert` sui pin e sulle risorse | TESTED ON HOST | `board_profile_test` |
| Driver PSRAM APS6404L (QMI CS1, GPIO47, QPI 75 MHz, self-test all'avvio) | IMPLEMENTED | self-test TESTED ON HOST con modelli di guasto (`psram_selftest_smoke`); driver da collaudare (HW-40…HW-45) |
| Avvio solo in SRAM se la PSRAM manca o fallisce il self-test | TESTED ON HOST | limiti RAM al profilo SRAM, diagnostica e messaggio (`system_diag_test`, `limits_smoke`) |
| Pool SRAM/PSRAM (TLSF), politiche, `operator new` instradato, `std::pmr` | TESTED ON HOST | `mem_pools_smoke` (anche ThreadSanitizer), `mem_new_smoke`, `oom_test` |
| "Memory ERROR" senza crash e senza allocazioni nel percorso dell'errore | TESTED ON HOST | `oom_test` con `operator new` instradato e SRAM limitata |
| Autosalvataggio dopo un "Memory ERROR": modifiche conservate e salvate appena la memoria torna | TESTED ON HOST | `oom_test` (nessun nuovo tasto: il salvataggio riparte da solo e il record contiene la modifica) |
| Limiti persistenti per scheda, limiti RAM dalla memoria presente | TESTED ON HOST | `limits_smoke` |
| Partizioni della flash (firmware, diagnostica, LittleFS, BTstack, workspace) | TESTED ON HOST | `board_profile_test` |
| Ultimo settore mai scritto (erratum RP2350-E10) | TESTED ON HOST | `board_profile_test` |
| Capacità reale della flash letta all'avvio (JEDEC ID): regioni oltre il chip disattivate con un UF2 della scheda sbagliata | IMPLEMENTED | decodifica e limiti TESTED ON HOST (`board_profile_test`, `system_diag_test`); lettura sul chip da collaudare (HW-40, HW-49) |
| Scritture flash con buffer di rimbalzo SRAM e verifica QMI M1 | IMPLEMENTED | logica TESTED ON HOST con la PSRAM simulata (`system_diag_test`); da collaudare (HW-45) |
| Stack core 0 da 32 KB con guardia MSPLIM, HardFault con rapporto al riavvio | IMPLEMENTED | da collaudare (HW-42, HW-81) |
| Core 1 (stress dell'allocatore su due core) | IMPLEMENTED | solo diagnostica; da collaudare (HW-44) |
| Driver LCD ST7365P (SPI1, init ufficiale, RGB666) | IMPLEMENTED | da collaudare; opzione RGB565 |
| Aggiornamento parziale LCD (tile 16×16) | TESTED ON HOST | `TileDiff`, test in app_test |
| DMA per il flush LCD | NOT STARTED | non conveniente: il collo di bottiglia è il bus SPI (vedi HARDWARE.md) |
| Tastiera STM32 I2C (FIFO, stati, modificatori) | IMPLEMENTED | decoder TESTED ON HOST; trasporto I2C da collaudare |
| Pressione prolungata / ripetizione | TESTED ON HOST | ripetizione generata dallo STM32, long-press nel decoder |
| Batteria e retroilluminazione | IMPLEMENTED | registri 0x05/0x0A/0x0B |
| Storage flash A/B con CRC | TESTED ON HOST | test di interruzione di alimentazione; driver Pico da collaudare |
| LittleFS sulla flash interna come archivio dei file utente | TESTED ON HOST | `internal_fs_test` (anche interruzioni di alimentazione); da collaudare (HW-47, HW-70) |
| Migrazione del workspace v1.8.0 sulla Pico Plus 2 W | TESTED ON HOST | `board_profile_test` con un record generato dai sorgenti v1.8.0 |
| microSD (SPI0 GP16–19, card detect GP22) | IMPLEMENTED | protocollo SD, recupero dopo estrazione/errori e FatFs TESTED ON HOST con scheda simulata (`sd_fatfs_test`); volumi FAT16/FAT32/exFAT verificati con fsck.fat, fsck.exfat e mtools (`sd_interop_test`); bus SPI0 reale da collaudare ([STORAGE.md](STORAGE.md), [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)) |
| Diagnostica sul dispositivo (SYSTEM → DIAG: memoria/PSRAM, allocatore su 1 e 2 core, flash↔PSRAM, LCD, microSD, flash interna, tempi GRAPH, log USB) | TESTED ON HOST | `system_diag_test`, `graph_app_test`; serve per il collaudo fisico |
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
| Font Unifont 8×16 e Spleen 6×12 (greco, simboli matematici; ⌟ disegnato dal progetto, v1.11.0) | TESTED ON HOST |
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
| Selezione parziale con il cursore (CLIP: SHIFT 8, COPY, CUT, SHIFT 9 PASTE; parentesi sempre bilanciate) | TESTED ON SIMULATOR (v1.10.0) |
| Scorrimento orizzontale dell'input | TESTED ON HOST |
| Matrici/vettori come modelli 2D modificabili (MATH › MAT/VCT, fino a 6×6) | TESTED ON SIMULATOR (v1.10.0) |
| Modalità di input/output Math e Line (SET UP › Input/Output, persistente) | TESTED ON SIMULATOR (v1.10.0) |
| Parentesi colorate per livello (blu, rosso, verde, magenta, nero) | TESTED ON SIMULATOR (v1.10.0; contrasto verificato sul PC, colori sul pannello reale non provati) |
| Cursore ed evidenziazione sulla causa di un errore | TESTED ON SIMULATOR (v1.10.0) |
| Sovrascrittura (INS) in modalità Line, SHIFT+INS (argomento di un modello) | TESTED ON SIMULATOR (v1.11.0) |
| Frazioni ⌟ in modalità Line (input e risultati) | TESTED ON SIMULATOR (v1.11.0) |
| Risultati più lunghi della riga scorrevoli (←/→ sulla riga selezionata) | TESTED ON SIMULATOR (v1.11.0) |

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
| Formati Norm1/Norm2/Fix/Sci/Eng (10 cifre; Eng con i simboli f…T dalla v1.11.0, P ed E dalla v1.12.0) | TESTED ON SIMULATOR |
| Aritmetica esatta: razionali, radicali a√b/c (≤ 2 termini), multipli di π | TESTED ON HOST |
| Valutazione esatta sullo stesso AST (trigonometria ai multipli di 30°/45°, radici, log esatti, n!, nCr…) | TESTED ON HOST |
| S⇔D con valore interno preservato; regole di visualizzazione CG50 (10 caratteri, limiti radicali e π) | TESTED ON SIMULATOR |
| Frac Result d/c ↔ ab/c (impostazione e per risultato) | TESTED ON SIMULATOR |
| Numeri periodici: vista del periodo e input periodico | TESTED ON SIMULATOR |
| Arrotondamento di visualizzazione come la calcolatrice (mantissa 15 cifre, per eccesso a metà) | TESTED ON HOST |
| Operatori relazionali e logici (= ≠ < > ≤ ≥, And, Or, Xor, Not) | TESTED ON HOST |
| Int÷, Rmdr, MOD_Exp, RndFix, RanNorm#, RanBin# | TESTED ON HOST |
| Simboli ingegneristici in input (m µ n p f k M G T, OPTN › ESYM; P ed E dalla v1.12.0) | TESTED ON SIMULATOR (v1.11.0) |
| CALC: Solve con limiti ed equazione, SolveN, FMin, FMax, Σ/Π con variabile e passo, ∫ con tolleranza, d²/dx² (anche modello), funzioni vietate negli argomenti | TESTED ON SIMULATOR (v1.12.0) |
| PROB: Ran# n, RanInt#/RanNorm#/RanBin# con n, RanList#, RanSamp#, P( Q( R( t( | TESTED ON HOST (v1.12.0) |
| NUMERIC Rnd; ANGLE ʳ ᵍ ° ′ ″; Pol/Rec in List Ans; COMPLEX ∠, ►r∠θ, ►a+bi | TESTED ON HOST (v1.12.0) |
| STAT in Run-Matrix: x̂ ŷ, 24 distribuzioni, S-Dev, Var, Mean/Median con frequenze, Min/Max di due liste | TESTED ON SIMULATOR (v1.12.0) |
| TVM (22 funzioni) e CONVERT (103 unità) | TESTED ON SIMULATOR (v1.12.0) |
| ENG / ENG←, overflow ≥ 10¹⁰⁰ | TESTED ON SIMULATOR (v1.12.0) |
| Simplify Manual con la regola del m.c.m. per tutte le operazioni tra frazioni | TESTED ON HOST (v1.12.0) |
| Base-N: Bin a 16 bit, variabili A–F in Hex, liste e matrici nella base | TESTED ON HOST (v1.12.0) |
| ▶Simp con e senza fattore (F=n), Simplify Auto/Manual | TESTED ON SIMULATOR (v1.11.0) |
| Simboli P, E e conversioni ►ENG/►ENG← | NOT STARTED |
| Base-N (letterali 0x/0b/0o, visualizzazione Hex/Bin/Oct) | TESTED ON HOST |
| Modalità Base-N intera (Mode Dec/Hex/Bin/Oct, 32 bit in complemento a due) con And, Or, Xor, Xnor, Not, Neg e prefissi d h b o | TESTED ON SIMULATOR (v1.11.0) |
| Sessagesimale (dms, °, visualizzazione DMS) | TESTED ON HOST |
| Pol / Rec | TESTED ON HOST |
| Integrali (Gauss-Kronrod adattivo), derivate (Ridders), d²/dx² | TESTED ON HOST |
| Σ, Π, SOLVE nel parser | TESTED ON HOST |
| Derivate/integrali simbolici, limiti | NOT STARTED |
| Matrici: somma, prodotto, det, inversa, trasposta, traccia, RREF, sistemi (API C++) | TESTED ON HOST |
| Matrici come valori nel parser: + − × ^, inversa, Det, Trn, Identity, Dim, Augment, Fill, Ref, Rref, Mat→List, List→Mat, Norm, elementi `Mat A[r,c]`, forme esatte, complessi | TESTED ON HOST (v1.10.0) |
| Vettori come valori nel parser: DotP, CrossP, Angle, UnitV, Norm, `Vct A[n]` | TESTED ON HOST (v1.10.0) |
| Liste come valori nel parser: aritmetica elemento per elemento, Sum, Prod, Min, Max, Mean, Median, Cuml, Percent, ΔList, SortA/D, Seq, Augment, `List 1[n]` | TESTED ON HOST (v1.10.0; List 1–26 dalla v1.11.0) |
| Forme esatte conservate nelle memorie Mat/Vct (fino a 64 elementi nella v1.11.0; dalla v1.12.0 senza limite per memoria, budget della scheda, celle decimali conservate) | TESTED ON HOST (v1.12.0) |
| Errori Dimension ERROR / Type ERROR con posizione | TESTED ON HOST (v1.10.0) |
| Vettori: somma, prodotto scalare/vettoriale, norma, angolo (API C++) | TESTED ON HOST |
| Conversioni metriche (10 grandezze, prefissi SI) | TESTED ON HOST |
| Limiti di lunghezza/annidamento e budget di stack ("Stack ERROR", "Memory ERROR") | TESTED ON HOST (budget del firmware verificato solo con `-fstack-usage`) |

## Modalità

| Modalità | Stato | Note |
|---|---|---|
| Run-Matrix | TESTED ON SIMULATOR | forme esatte, S⇔D, frazioni miste, periodo, cronologia modificabile con ricalcolo, OPTN, VARS, Base-N, DMS (dettagli in RUN_MATRIX.md) |
| Run-Matrix: cronologia modificabile sul posto con ricalcolo delle righe successive | TESTED ON SIMULATOR | |
| Run-Matrix: matrici/vettori/liste (MAT/VCT/List) e memorie MatAns/VctAns/ListAns | TESTED ON SIMULATOR | v1.10.0, [RUN_MATRIX.md](RUN_MATRIX.md#matrici-vettori-e-liste-v1100) |
| Run-Matrix: editor MAT/VCT (DIM, celle, ROW-OP, ROW, COLUMN, DELETE, DEL-ALL, M⇔V) | TESTED ON SIMULATOR | v1.10.0; dalla v1.11.0 celle con espressioni esatte e complesse, colonne scorrevoli con la cella corrente, CSV LOAD/SAVE·AS |
| Run-Matrix: schermate MatAns / VctAns / ListAns oltre 6×6 o 20 elementi | TESTED ON SIMULATOR | v1.11.0 |
| Run-Matrix: comandi grafici (Cls, ViewWindow, Graph Y=/r=/X=/(X,Y)=, Plot…, Line, F-Line, Circle, Vertical, Horizontal, Text, Pxl…, Tangent, Normal, Inverse) disegnati da GRAPH | TESTED ON SIMULATOR | v1.11.0 |
| Run-Matrix: PxlTest(, Graph Y>/Y</Y≥/Y≤ e X…, G∫dx, Sketch…, StoPict/RclPict/BG-Pict/RclCapt, DrawGraph, DrawStat, DrawDyna, DrawR, DrawWeb, DrawFTG, G-SelOn/Off; grafici dei comandi nei `.pcgraph` | TESTED ON SIMULATOR | v1.12.0 |
| Run-Matrix: istruzioni multiple `:` e `◢`, Str 1–20, function memory f1–f20, r e θ, variabili di sistema assegnabili | TESTED ON HOST | v1.12.0 |
| Run-Matrix: List "nome", List File 1–6, Sub Name, n→Dim List, Fill sulle memorie, Swap/\*Row/\*Row+/Row+ | TESTED ON HOST | v1.12.0 |
| Run-Matrix: OPTN nell'ordine della fx-CG50, VARS completo (FACTOR, STAT, GRAPH, DYNA, TABLE Result, RECURSION, EQUATION, TVM, Str), CATALOG (SHIFT 4), CAPTURE (SHIFT 7) | TESTED ON SIMULATOR | v1.12.0 |
| Editor: round-trip testo ↔ albero esatto (corpus), parentesi implicite delle frazioni, testo Line minimo | TESTED ON HOST | v1.12.0 |
| Statistics | TESTED ON SIMULATOR | List 1–26 con sottonomi (v1.11.0), SET XList/YList, CSV LOAD/SAVE·AS, 1/2-Var, 9 regressioni, grafici, DIST, test Z/t, intervalli |
| Liste: file CSV su microSD e flash interna (separatore e decimale configurabili, scrittura atomica) | TESTED ON HOST | v1.11.0, anche con interruzione di alimentazione sulla flash interna |
| Liste: List File 1–6 e riferimento per sottonome (`List "QTY"`) | NOT STARTED | |
| Regressioni sinusoidale e logistica | NOT STARTED | |
| Test χ², F, 2-campioni, ANOVA nell'interfaccia | NOT STARTED | ANOVA e t a 2 campioni esistono come API C++ |
| Grafico a torta / a barre | NOT STARTED | |
| Graph: 20 funzioni Y=, r=, Param, X=, 8 disequazioni, intervalli, sovrapposizioni, liste, funzioni composte | TESTED ON SIMULATOR | matrice completa in [GRAPH_PARITY.md](GRAPH_PARITY.md) |
| Graph: TRACE, ZOOM (13 voci), V-Window e memorie, SKETCH (12 voci), G-SOLVE (ROOT…∫dx MIXED), Modify, memorie grafiche, PICT, sfondo | TESTED ON SIMULATOR / TESTED ON HOST | voce per voce in GRAPH_PARITY.md |
| Graph: progetti `.pcgraph` su microSD (NEW/OPEN/SAVE/SAVE AS/RENAME/DELETE) | TESTED ON SIMULATOR | scheda simulata; formato in [PCGRAPH_FORMAT.md](PCGRAPH_FORMAT.md) |
| Graph: budget dei punti 40 000/400 000 con la PSRAM (6000/24 000 senza) | TESTED ON HOST | `graphics_memory_test`: 20 parametriche dense complete con la PSRAM, troncate senza |
| Graph: layer statici in PSRAM durante TRACE/puntatore | TESTED ON HOST | immagine identica al pixel a un ridisegno completo (`graphics_memory_test`) |
| Graph: Dual Screen G+G/GtoT, Graph to Table | NOT STARTED | |
| Graph: disegno dei comandi grafici di Run-Matrix (EXIT torna a Run-Matrix) | TESTED ON SIMULATOR | v1.11.0 |
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
| 3D Graph: Z1–Z3 = f(x,y), finestra 3D, griglia, rotazione, zoom, AUTO, campionamento incrementale, z-buffer in PSRAM (pittore senza PSRAM), un colore per superficie, persistenza | TESTED ON SIMULATOR | `graph3d_test` (motore e modalità, con e senza PSRAM); screenshot 40–43 (`tests/sim/graph3d.txt`); sfere/piani/rette parametrici della fx-CG50 NOT STARTED |
| Program, Financial, Link, Geometry, Picture Plot | IN PROGRESS | solo modello dati; schermata "non disponibile" |
| E-CON4 | NOT STARTED | richiede un data logger esterno: non realizzabile sulla PicoCalc (stato External) |
| Python (MicroPython) | NOT STARTED | richiede l'integrazione del runtime come libreria (modello dei file di script definito) |

## Persistenza

| Dato | Stato |
|---|---|
| Impostazioni, variabili, Ans | TESTED ON HOST |
| Storia Run-Matrix (con forma esatta, vista, frazione mista) | TESTED ON HOST |
| Lettura dei record v1.6.0 (formato versione 1) | TESTED ON HOST |
| Liste statistiche (List 1–26 e sottonomi dalla v1.11.0, record v7) | TESTED ON HOST |
| Stato di GRAPH (20 funzioni, V-Window, Setup, memorie V-Window e grafiche, SKETCH) e di TABLE (range, variabile, tipo, righe) — record v4 | TESTED ON HOST |
| Record v5 (v1.9.0): 3D Graph e archivio dei file; lettura dei record v4 della v1.8.0 | TESTED ON HOST |
| Liste di 999 elementi e cronologia di 200 righe (Pico Plus 2 W) | TESTED ON HOST |
| Lettura dei record v3 (GRAPH v1.8.0 prima di TABLE) | TESTED ON HOST |
| Record oltre la capacità della flash: messaggio "Memoria flash piena" con la dimensione | TESTED ON HOST |
| Lettura dei record v1.7.0 (versione 2) e correzione delle V-Window legacy | TESTED ON HOST |
| Progetti `.pcgraph` e immagini `.pcpict`/BMP sulla microSD, scrittura atomica e recupero | TESTED ON HOST (scheda simulata, pila completa GraphApp → FatFs → SD) |
| Progetti e immagini sulla flash interna (LittleFS), scelta in SYSTEM | TESTED ON HOST |
| Coefficienti Equation | TESTED ON HOST |
| Conversazioni AI (cancellabili da AI Chat e Memory) | TESTED ON HOST |
| Programmi, file Python, documenti | NOT STARTED |
| Matrici e vettori salvati nel workspace (record v6) | TESTED ON HOST (v1.10.0) |
| Matrici e liste come file CSV (manuale 2-48, 3-18) | TESTED ON HOST (v1.11.0) |
| Record v8 (v1.12.0): List File, Sub Name, r e θ, Str, function memory, variabili Dyna/Recursion/TVM, celle decimali, ENG/◢/stringhe della cronologia; lettura dei record v7 della v1.11.0 (fixture) | TESTED ON HOST |
| Record v7 (v1.11.0): Base-N, Simplify, CSV, List 7–26, sottonomi, forme esatte delle memorie, ▶Simp e base della cronologia, grafici dei comandi; lettura dei record v6 della v1.10.0 | TESTED ON HOST |

## Relay AI

| Funzione | Stato |
|---|---|
| Claude (Anthropic SDK, `claude-opus-5-5`, fallback server-side sui rifiuti) | TESTED ON HOST (API simulata) |
| ChatGPT (OpenAI SDK, modello configurato dall'operatore) | TESTED ON HOST (API simulata) |
| Token per dispositivo (hash SHA-256), limiti di dimensione e frequenza | TESTED ON HOST |
| TLS integrato con verifica del certificato | TESTED ON HOST |
| Prova con chiavi reali | NOT STARTED (richiede chiavi e credito dell'utente) |
