# Test — v1.14.0 (completamento totale di 3D Graph: `graph3d_v114_test`, record v10, fuzzing del 3D; v1.13.1: un test per ogni errore di ERRORI_v1.13.1, UBSan bloccante)

## Cosa viene provato e dove

| Livello | Strumento | Stato |
|---|---|---|
| Logica portabile (motore, editor, app, grafici, tabelle, 3D, file, storage, memoria, protocollo AI) | test host C++ (CTest), **per entrambi i profili di scheda** (`FXAI_TEST_BOARD=PICO_PLUS_2W` e `PICO2W`) | eseguito: 45 suite; 13 111 controlli (Plus) / 13 105 (Pico 2 W) più ≈ 333 000 verifiche dello stress dell'allocatore (`mem_pools_smoke`) |
| Integrazione continua | GitHub Actions (`.github/workflows/release.yml`, `ubuntu-24.04`) | a ogni push: test host dei due profili, ASan/UBSan, TSan, relay, firmware dei due moduli, pacchetto |
| Architettura (registro ↔ sorgenti ↔ firmware) | `architecture_test` | eseguito |
| Stessi test con AddressSanitizer + UBSan | `--sanitize` (dalla v1.13.1 UBSan è bloccante: `-fno-sanitize-recover=undefined`) | eseguito su entrambi i profili |
| Allocatore su due thread con ThreadSanitizer | `--tsan` (`mem_pools_tsan`, cartella di build propria) | eseguito su entrambi i profili |
| Memoria esaurita con `operator new` instradato nei pool | `oom_test` (SRAM simulata limitata, senza PSRAM) | eseguito |
| PSRAM | pool PSRAM simulato sul PC (arena TLSF; 8 MB nei test di sistema e nel simulatore), self-test con modelli di guasto | eseguito; il chip APS6404L reale non è provato |
| Flash interna | modello NOR (`storage::RamFlash`: erase a 0xFF, program che azzera bit, interruzione dopo n byte) per il record A/B e per LittleFS | eseguito; la flash reale non è provata |
| Interfaccia | simulatore host + screenshot PNG | eseguito (`docs/screenshots/`: Run-Matrix 12–19, 44–51 e 52–61, EQUATION 08 e 62–76, GRAPH/TABLE 06, 07, 20–37, 3D Graph 40–43 e 88–104) |
| microSD (protocollo SD in SPI + FatFs) | scheda SD simulata nei test host | eseguito (`sd_fatfs_test`); bus SPI0 reale non provato |
| Interoperabilità FAT/exFAT | `sd_interop_test` (immagini con mkfs.fat/mkfs.exfat, fsck, mtools) | eseguito: 41 controlli |
| Relay AI | `unittest` Python con SDK ufficiali e API simulate, TLS in loopback | eseguito: 15 test |
| Compilazione firmware RP2350 | Pico SDK 2.2.0, `scripts/build.sh --board pico_plus_2w` e `--board pico2w` | eseguita, nessun warning, `picotool info` verificato ([BUILD_STATUS.md](BUILD_STATUS.md)) |
| Stack sul target | `-fstack-usage` su tutto il firmware | eseguito nella v1.13.1: frame massimo 2 016 byte (`SystemApp::enter`) su 32 KB; valutatore ricorsivo ≤ 328 byte per livello, limitato dal budget di 16 KB |
| Fuzzing (v1.13.1, v1.14.0) | libFuzzer + ASan + UBSan (`tools/fuzz`, `run_fuzz.sh`): parser e valutatori, forma esatta contro decimale, workspace con andata e ritorno (anche record v10), CSV, `.pcgraph`, EQUATION, sequenze di tasti; **`fuzz_graph3d`** (insiemi delle 3D G-Mem con geometria, sezioni e disegno, espressioni Z=/Param, finestre) | eseguito sul profilo Plus (conteggi in [reports/SVILUPPO_v1.13.1.md](reports/SVILUPPO_v1.13.1.md) e [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)) |
| Collaudo fisico | procedure [HARDWARE_TEST_v1.14.0.md](HARDWARE_TEST_v1.14.0.md), [HARDWARE_TEST_v1.13.1.md](HARDWARE_TEST_v1.13.1.md), [HARDWARE_TEST_v1.13.0.md](HARDWARE_TEST_v1.13.0.md), [HARDWARE_TEST_v1.12.0.md](HARDWARE_TEST_v1.12.0.md), [HARDWARE_TEST_v1.11.0.md](HARDWARE_TEST_v1.11.0.md), [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md), [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) e [HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md), SYSTEM → DIAG | **non eseguito: in attesa dei risultati dal dispositivo** |

Quattro livelli distinti, mai confusi nella documentazione:
**test host** (logica compilata dallo stesso sorgente del firmware ed eseguita
sul PC), **test di compilazione firmware** (build, link, dimensioni, warning,
stack), **test simulati** (simulatore host, hardware modellato: PSRAM, flash
NOR, scheda SD) e **test hardware reali** (nessuno finora). Nessuna funzione è
dichiarata *TESTED ON HARDWARE*: display, tastiera I2C, PSRAM e flash reali,
microSD reale, Wi-Fi e TLS sul modulo non sono stati provati.

## Comandi

```bash
tests/run_host_tests.sh                     # entrambi i profili: build/host-pico_plus_2w, build/host-pico2w
tests/run_host_tests.sh --sanitize --tsan   # ASan + UBSan (build/host-<scheda>-asan) e ThreadSanitizer (-tsan)
tests/run_host_tests.sh --board PICO2W      # un solo profilo
ctest --test-dir build/host-pico_plus_2w --output-on-failure -R graph3d   # una sola suite

cd relay && python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

I test usano macro proprie (`tests/test_util.hpp`: `CHECK`, `CHECK_NEAR`,
`CHECK_THROWS_AS`, …) che restano attive anche con `NDEBUG` e riportano ogni
errore, non solo il primo.

## Suite host

| Suite | Pico Plus 2 W | Pico 2 W | Contenuto |
|---|---|---|---|
| expression_test (v1.10.0: 154) | 155 | 155 | priorità Casio, funzioni, complessi, formati e arrotondamento come la calcolatrice, limiti di lunghezza/annidamento, budget di stack |
| exact_test | 152 | 152 | razionali (overflow, confronto esatto), radicali, π, letterali decimali e periodici, sviluppo periodico, limiti di visualizzazione CG50, valutatore esatto (trigonometria esatta, radici, log, n!, nCr, relazioni, Σ/Π, memoria esatta, catene lunghe, budget di stack), coerenza esatto/virgola mobile |
| fixes_v1131_test **nuova v1.13.1** | 142 | 142 | un gruppo per ogni errore di [ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md), che fallisce con i sorgenti della v1.13.0: record v1–v8 non rileggibili dopo il salvataggio (fixture della v1.11.0 modificata), HypergeoCD/InvHypergeoCD con x, n, M grandi, inverse discrete in tempo lineare (anche contro la definizione), forme esatte complesse con Abs/Conjg/ReP, nCr/nPr fino a 1E10, CSV senza allocazioni prima dei controlli (byte misurati con un `operator new` contato), salvataggio atomico dopo due interruzioni, quantili t, 1-Variable con frequenze, Ran# n, Σ/Π con estremi enormi, distribuzione geometrica con p piccolo, InvPoissonCD oltre 1E6, HypergeoPD con argomenti grandi, nomi di VARS › V-WIN e TABLE |
| calc_v112_test **nuova v1.13.0** | 207 | 207 | CALC con i valori del manuale (d/dx 52, d²/dx² 26, ∫ 404/3, Σ con variabile e passo, Solve con limiti ed equazione, SolveN anche con radice doppia e poli, FMin/FMax, funzioni vietate), PROB (sequenze di Ran#, RanInt#/RanNorm#/RanBin# con n, RanList#, RanSamp# con e senza ripetizione, P Q R t), Rnd, ʳ ᵍ ° ′ ″, Pol/Rec in List Ans, ∠ e ►r∠θ/►a+bi, 24 distribuzioni con valori di riferimento, S-Dev/Var/Mean/Median con frequenze, x̂ ŷ e VARS STAT, 22 funzioni TVM, CONVERT (anche su liste, grandezze diverse), P ed E, ENG/ENG←, overflow, Simplify Manual (m.c.m., prodotti, ▶Simp senza F=, divisore non valido, modo complesso), Base-N (Bin 16 bit, Hex con v_a, liste e matrici nella base, Oct), messaggi dell'Appendice α |
| run_matrix_v112_test **nuova v1.13.0** | 152 | 152 | `:` e `◢` (posizione degli errori, ricalcolo), stringhe e Str, function memory (X, argomento, ricorsione), r e θ, variabili di sistema (V-Window con Range ERROR, Xfct, TblStart, D Start, R End, TVM), formula an+1, List "QTY", List File 1–6 e budget comune, n→Dim List, Fill su liste e matrici, Swap/\*Row/\*Row+/Row+, forme esatte oltre 64 elementi e celle decimali, VARS EQUATION e TABLE Result, migrazioni v7→v9 e v8→v9 dai fixture della v1.11.0 e della v1.12.0, record v9 con tutti i campi nuovi e record troncati, interfaccia (OPTN nell'ordine della fx-CG50, ENG, CONVERT, VARS STAT, Hex con v_a, FUNCMEM, CATALOG, CAPTURE e RclCapt) |
| graph_cmd_v112_test **nuova v1.13.0** | 56 | 56 | le otto disequazioni, G∫dx, prefissi di stile (anche da soli), StoPict/RclPict/BG-Pict/RclCapt, G-SelOn/Off, DrawGraph, DrawStat, DrawR-Con/Plt, DrawWeb, DrawFTG-Con/Plt, DrawDyna (anche il limite di 10 grafici, tutto o niente), limite dei punti per scheda, `.pcgraph` con i grafici dei comandi (codifica, decodifica, identità), PxlTest( sul firmware (punto acceso, assi, fuori schermo, errore), StoPict eseguito da GRAPH |
| editor_v112_test **nuova v1.13.0** | 488 | 488 | corpus di 68 testi canonici (testo → albero → testo e stesso disegno, Math e Line), 13 alberi costruiti con i modelli (gruppo con frazione, radice con indice n+1, d²/dx², Σ e Π con variabile, matrice, frazione mista, potenze, |x|, logₐb, ∫), parentesi implicite (`6÷2[1/3]` = 9, con × = 1), testo Line minimo e stesso valore, Math ↔ Line della riga di input, nomi mostrati, sottolineatura di v_a, glifi nuovi nei due font, voci del CATALOG |
| equation_v112_test **nuova v1.12.0** | 244 | 244 | EQUATION, un gruppo per punto della richiesta: coefficienti (frazioni, radicali, π, A–Z, Ans, complessi esatti come polinomi in i, Non-Real/Type/Syntax, EDIT, conversione senza perdita dei double), SIML (esempio del manuale 4-2, frazioni, radicali e π, Hilbert 6×6 esatta e decimale, 2–6 incognite, 1E50, No Solution, Infinitely Many Solutions con l'esempio complesso del manuale, r∠θ, Ma ERROR/Underflow, Fix/Sci/Eng, S⇔D, ab/c), POLY (esempi 4-3, 4-4, 1-29, ordine, ×k, radicali, biquadratiche, quartiche pari, grado 6, decimali, irriducibili, radice doppia irrazionale, radici vicine, coefficienti complessi, 10⁹, a = 0, Underflow), SOLVER (esempio 4-5 per V e T, x = X, senza "=", Lower/Upper, Can't Solve!, Time Out, No Variable, Syntax ERROR, polo, Retry), schermate e tasti sul firmware (menu, EXE/EXIT, EDIT, REPEAT, CLEAR, frazione F1, S⇔D, errori con l'immissione aperta, Line, Complex Mode dal SET UP con ritorno, Solver con RECALL, Lower/Upper, DELETE), VARS › EQUATION (errori, SimRes esatto, MatAns, SimRes+SimCoef, Rref, PlyRes, PlyCoef, Trn), record v8 e migrazione v7→v8 dal fixture della v1.11.0, correzioni dell'editor (½X, GT²), estensioni |
| lists_csv_test **nuova v1.11.0** | 878 | 878 | List 1–26 nei calcoli (anche `List12`, `→List 20[1]`), sottonomi (limiti, caratteri, record), budget di tutte le liste per scheda (tutto o niente, 20 liste da 999 sulla Plus, 6 da 255 sul Pico 2 W), GRAPH con List 12, Statistics (26 colonne, riga SUB, SET XList, 1-Var), CSV: codifica e decodifica (separatori, decimale `,`, CR/LF/CRLF, intestazione, celle vuote, testo non numerico, tabulatore, file vuoto, oltre i limiti, complessi), liste e matrici dal file, file su microSD e LittleFS, file mancante e nomi non validi, interruzione di alimentazione durante il salvataggio sulla flash interna (vecchio o nuovo, mai misto), CSV dalle applicazioni (OPTN, F4) sulle due memorie, Format ERROR senza modifiche, scheda assente; editor MAT/VCT con celle esatte, complesse ed espressioni, scorrimento delle colonne; forme esatte nei calcoli (`Mat A×6`, `Det`, `→Mat A[2,2]`) e nel record v7 (anche forme incoerenti rifiutate); migrazione v6→v7 dal fixture della v1.10.0 (valori predefiniti, calcoli, round trip, troncamenti) |
| calc_v111_test **nuova v1.11.0** | 133 | 133 | simboli ingegneristici (valori, priorità, errori, forma esatta decimale, riscrittura), display Eng con i simboli (f…T, oltre ×10ⁿ) e in Run-Matrix (`999k+25k`, `9/10`, S⇔D), frazioni `⌟` (valori, errori, output Line anche negli elementi), ▶Simp con e senza fattore (Ans, errori, segno, S⇔D), Simplify Manual (modello, `⌟`, ricalcolo, persistenza), Base-N: letterali Hex/Bin/Oct/Dec, complemento a due, limiti a 32 bit, prefissi, divisione intera, operatori bit a bit anche senza spazi (`FFxor0F`), Not/Neg, funzioni escluse, testo dei risultati, Run-Matrix con basi diverse nella cronologia, viste ▶Dec…▶Oct, ricalcolo e record v7 |
| run_matrix_v111_test **nuova v1.11.0** | 106 | 106 | INS (sovrascrittura in Line, anche di `⌟`; nessuna in Math), SHIFT+INS (gruppo, numero, funzione, frazione, annullamento), tasti sul firmware (SHIFT DEL, F1 → `⌟`, scorrimento del risultato con ←/→, SHIFT+INS + √), MatAns/ListAns (apertura automatica, navigazione, sola lettura, F4 dalla cronologia, vista naturale senza schermata), comandi grafici (ognuno, variabili negli argomenti, Line, pixel, Tangent/Normal, Graph dei 4 tipi, disegno nel motore, errori tutto o niente con posizione, limite di 64 elementi, Cls, ricalcolo senza riesecuzione, riga modificata rieseguita, persistenza), schermata di GRAPH e ritorno con EXIT, menu SKETCH, riga di comando modificata che apre GRAPH, SET UP (Mode, Simplify, CSV con la combinazione rifiutata) |
| matrix_test (v1.10.0: 673) | 1 019 | 1 019 | parsing di matrici, vettori, liste, riferimenti e indici; posizioni dei token e dei nodi; matrici (+ − × ^, inversa, Det, Trn, Identity, Dim, Augment, Fill, Ref, Rref, Mat→List, List→Mat, Abs/Int), vettori (DotP, CrossP, Norm, UnitV, Angle), liste (elemento per elemento, Sum, Prod, Mean, Median, Min, Max, Cuml, Percent, ΔList, SortA/D, Seq, Augment), complessi; errori di dimensione e di tipo con la posizione; annidamento profondo e Stack ERROR; forme esatte; nucleo di Run-Matrix (→Mat/Vct/List, elementi, Dim, MatAns/VctAns/ListAns, risultati oltre 6×6, viste, modifica della cronologia); limiti per scheda; record v6 (round trip, troncamenti) e lettura dei record v1.9.0 e v1.8.0 |
| editor_v110_test (v1.10.0: 161) | 164 | 164 | modelli MAT/VCT (inserimento, navigazione a griglia, importazione, disegno), Math/Line (modelli lineari, conversione, incolla), CLIP (selezione, copia/taglia/incolla, parentesi bilanciate, nome di funzione, intero modello), parentesi colorate per livello e contrasto ≥ 4,5:1, cursore sulla causa degli errori (anche dentro i modelli), 600 sequenze casuali di modifica; sul firmware: errori e cursore, editor MAT/VCT (DIM, celle, ROW-OP, ROW, M⇔V, vettori), CLIP con SHIFT 8/9, Line da SET UP, riavvio con matrici e impostazione conservate |
| run_matrix_test | 182 | 182 | esempi frazione ⇔ decimale, frazioni miste, periodo, input decimale, irrazionali mai convertiti, radicali e π (esempi del manuale), Ans/A–Z esatti, →A~F, variabili condivise, Int÷/Rmdr/MOD_Exp/logica, modifica della cronologia con ricalcolo, alberi 2D, persistenza e lettura dei record v1.6.0 (anche con V-Window non più valida), dati corrotti, flussi completi da tastiera attraverso il `Firmware` |
| architecture_test (v1.11.0: 686; v1.12.0: 764) | 1 379 | 1 379 | Run-Matrix (v1.13.0): nessun altro lexer, parser o valutatore, `calc_library` puro, Run-Matrix solo tramite le interfacce del workspace (anche List File, Str, function memory, PxlTest(); EQUATION (v1.12.0): servizio EquationCore solo per Equation, nessuna lettura propria dei numeri né percorsi numerici separati, uso obbligato del motore `eq::` e di `MathField`, motore che valuta con il valutatore condiviso, Run-Matrix solo tramite `CollectionResolver::Memory::Equation`, `equation.hpp`/`equation.cpp` nel livello del motore; registro coerente, sezioni dei membri e dei metodi del workspace (`dataSectionOfMember`), comandi grafici solo tramite `graphcmd::execute` e disegnati da GRAPH, moduli `value`/`csv_io`/`graph_commands` nel livello del motore, controller registrati = modalità disponibili, schermate "non disponibile", include/sezioni del workspace/collegamenti dichiarati (anche GraphCore, Files, dato Graph), separazione di Run-Matrix, stratificazione motore/UI, nessun ciclo di include, modelli dati delle modalità future; 3D Graph e servizi Graph3D/Hardware |
| editor_test | 180 | 180 | modelli 2D (anche frazione mista, periodo, radice con indice), navigazione, undo/redo, copia/incolla, layout e cursore, limiti |
| math_engine_test (v1.8.0: 108) | 108 | 109 | matrici, vettori, basi, sessagesimale, frazioni, calcolo numerico; capacità della cronologia di sessione per scheda |
| storage_test (v1.11.0: 809) | 1 020 | 1 020 | serializzazione del workspace (formato v5 con lo stato di GRAPH, TABLE e 3D Graph; lettura dei formati precedenti), slot A/B, CRC, interruzione di alimentazione in ogni punto della scrittura |
| app_test | 62 | 62 | AppManager, tasti globali, errori, barra di stato, TileDiff, rendering delle schermate |
| apps_flow_test (v1.11.0: 56) | 62 | 62 | percorsi completi: Statistics → Graph, Graph ↔ Table, Equation (schermate della v1.12.0), System, Memory, Conversion |
| solvers_test | 60 | 60 | polinomi (Aberth), disequazioni, solver numerico |
| statistics_test | 63 | 63 | 1/2-Var, regressioni, distribuzioni, test e intervalli (valori di riferimento) |
| ai_test | 138 | 138 | JSON, richiesta HTTP, parser della risposta (chunked, errori), app AI Chat con trasporto simulato |
| graph_engine_test | 351 | 351 | viewport e clipping, testi delle funzioni (intervallo, sovrapposizione), compilazione della lista, riferimenti tra funzioni (≤ 5 livelli), grafici da lista e incollati, campionamento (poli, salti, bordi di dominio, Plot), disequazioni, polari e parametriche, limiti di punti, ZOOM e preset della V-Window |
| graph_solve_test | 51 | 51 | Brent, radici (anche di tangenza), poli scartati, estremi, intersezioni, Y/X-CAL, integrali e aree contro valori analitici |
| graph_file_test | 345 | 345 | `.pcgraph` (round trip, CRC, versioni, ogni campo non valido), `PCGMEM`, `.pcpict` e BMP, file store su RAM con guasti iniettati, scrittura atomica e recupero |
| graph_app_test (v1.8.0: 301) | 303 | 303 | GRAPH dalla tastiera: lista e editor, tipi e CONVERT, disegno incrementale e TRACE, ZOOM, V-Window e memorie, Setup, BUILT-IN, Draw Type, Simul Graph, budget di memoria, G-SOLVE completo (anche ∫dx attraverso un polo), SKETCH e limite PEN, Modify, memorie grafiche, PICT e sfondo, TABLE, incolla, progetti e ciclo 12.4 (anche sull'intera pila microSD simulata), scheda assente/piena/non formattata, record oltre la capacità, SYSTEM > DIAG; budget dei punti dal profilo RAM, record oltre la capacità anche con lo slot da 512 KB della Plus |
| graph_verify_test | 182 | 182 | precisione al pixel di punti e segmenti (18 casi Y=, X=, r=, Param), discontinuità e domini, G-Solve, derivate e integrali contro valori analitici, poli, salti e singolarità integrabili, gradi/gradienti |
| table_test | 171 | 171 | TABLE: righe (intervalli positivi, negativi, misti, limiti), celle di 6 caratteri, valori contro formule chiuse, derivate, r= e Param, flussi da tastiera (SET, TABLE, aggiornamento, scorrimento, EDIT, ROW, LISTMEM, GPH-CON, DELETE, Variable List, Setup), 999 righe, persistenza v4 e v3, `.pcgraph`; LISTMEM con le liste da 999 (Plus) o 255 elementi |
| sd_fatfs_test | 302 | 302 | protocollo SD su schede simulate SDHC/SDv2/SDv1, FatFs, nomi lunghi, salvataggio atomico, estrazione e reinserimento; scheda non formattata, spenta e riaccesa, errori di lettura, scritture rifiutate, dimensioni ai bordi di settore/cluster, cartelle annidate, 150 file, rinomina, scheda piena |
| sd_interop_test (script) | 41 | 41 | FAT16/FAT32/exFAT scritti dal codice del firmware, verificati con fsck.fat/fsck.exfat/mtools e viceversa |
| graphics_smoke | 57 | 57 | primitive, clipping, blending, conversione RGB666 |
| font_smoke | 2 556 | 2 556 | copertura dei glifi usati, UTF-8, misure |
| animation_smoke | 6 | 6 | primitiva di animazione |
| cheatsheet_smoke | 365 | 365 | contenuti e navigazione di Symbols (17 pagine) |
| vpam_ast_smoke | 13 | 13 | albero condiviso editor/parser |
| board_profile_test **nuova** | 78 | 74 | profilo della scheda e mappa della flash (partizioni, ultimo settore E10, posizione della v1.8.0), pin della PicoCalc invariati, capacità della flash dal JEDEC ID e regioni oltre il chip (UF2 della scheda sbagliata), limiti persistenti, record v5, migrazione di un record generato dai sorgenti della v1.8.0 (`tests/fixtures/workspace_v1_8_0.inc`) |
| limits_smoke **nuova** | 35 | 35 | limiti persistenti, RAM e strutturali per scheda; profilo scelto dai pool presenti; budget per i dati pesanti |
| mem_pools_smoke **nuova** | ≈ 333 000 | ≈ 334 000 | TLSF (base, allineamento, stress, esaurimento e frammentazione), `PlacementScope`, registro dei pool per Plus e Pico 2 W, stress su due thread (anche con ThreadSanitizer). Il numero di verifiche varia a ogni esecuzione (contesa tra i thread) |
| mem_new_smoke **nuova** | 13 | 13 | instradamento di `operator new` (soglia 512 byte, politiche, allineamenti) |
| psram_selftest_smoke **nuova** | 10 | 10 | self-test della PSRAM con modelli di guasto (bus dati, pattern, percorso con cache) |
| oom_test (v1.9.0: 18) | 27 | 27 | "Memory ERROR" con `operator new` instradato nei pool e SRAM limitata (condizioni del Pico 2 W): Run-Matrix, autosalvataggio rimandato e ripreso da solo, GRAPH, 3D (griglia ridotta o errore), editor; stato intatto e nessuna perdita di memoria; v1.10.0: avvio con un record più grande della memoria libera (nessuna terminazione, salvataggi sospesi, dati in flash intatti) |
| graphics_memory_test **nuova** | 23 | 23 | poligoni pieni oltre i 32 vertici, copie di layer e invalidate, regioni sporche per scheda, budget PICT, 20 parametriche dense disegnate per intero con la PSRAM, cache dei layer di GRAPH identica al pixel |
| internal_fs_test **nuova** | 197 | 197 | LittleFS su flash NOR simulata: contratto `files::FileSystem` come FatFs, partizione piena, interruzione di alimentazione durante `writeAtomic`, rimontaggio senza riformattare, progetti GRAPH nell'archivio interno |
| graph3d_test (v1.9.0: 111; portata alla v1.14.0) | 88 | 88 | motore 3D (campionamento incrementale, buchi, z-buffer e pittore, due superfici con lo stesso test di profondità, taglio al box, rotazione θ/φ, colori) e modalità (lista, input Z=, V-Window, calcolo nei tick, rotazione e zoom senza ricalcolo, ORIGINAL, persistenza, profili con e senza PSRAM) |
| graph3d_v114_test **nuova v1.14.0** | 541 | 541 | un gruppo per voce della tabella di parità del 3D Graph: V-Window (Out of Domain, Invalid Setting, normalizzazione di θ/φ), modelli e loro «Invalid Setting», conversioni dei moduli, codifica delle G-Mem, INTSECT/RELATION con gli esempi del manuale, sezioni CROSS, taglio al box (nessun pixel fuori dall'involucro del box), z-buffer condiviso con le rette, proiezione e viste, colori FORMAT, coordinate enormi (fuzzing), memoria esaurita, record v10 e migrazione delle fixture v1.6.0…v1.13.1, tutta la modalità dalla tastiera su entrambi i profili |
| system_diag_test **nuova** | 15 | 12 | SYSTEM → DIAG completa (MEM, PSR, ALC, FLS, LCD, SD, INT, GPH) con la memoria della scheda; Plus con PSRAM fallita; pagina INFO; "Archivio file"; riga del chip della flash; kernel di stress dell'allocatore |
| **Totale** | **13 111** + ≈ 333 000 dello stress + 41 dello script | **13 105** + ≈ 333 000 + 41 | 0 errori, con e senza sanitizer (ASan + UBSan bloccante); `mem_pools_tsan` con ThreadSanitizer |

`run_matrix_test` usa `tests/fixtures/workspace_v1_6_0.inc`, un record salvato
dai sorgenti della v1.6.0 (commit 7dc65cd), per verificare la compatibilità.
`board_profile_test` usa `tests/fixtures/workspace_v1_8_0.inc`, un record
generato con i sorgenti della v1.8.0, per la migrazione al formato v5.
`matrix_test` usa anche `tests/fixtures/workspace_v1_9_0.inc`, un record v5
generato con i sorgenti della v1.9.0 (commit `bb2bdee`), per la lettura nel
formato v6. `lists_csv_test` usa `tests/fixtures/workspace_v1_10_0.inc`, un
record v6 generato con i sorgenti della v1.10.0 (tag `v1.10.0`, commit
`dfdb08b`) prima di ogni modifica della v1.11.0, per la migrazione al v7.
`run_matrix_v112_test` usa `tests/fixtures/workspace_v1_11_0_run_matrix.inc`,
un record v7 generato con i sorgenti della v1.11.0 (tag `v1.11.0`, commit
`f6933fd`) prima di ogni modifica della v1.13.0 (Run-Matrix: Simplify, CSV,
sottonomi, Hex, comandi grafici), e `tests/fixtures/workspace_v1_12_0.inc`, un
record v8 generato con i sorgenti della v1.12.0 (commit `51691de` di `main`,
con i dati di EQUATION), per le migrazioni v7→v9 e v8→v9.
`graph3d_v114_test` legge tutte le fixture (v1.6.0, v1.8.0, v1.9.0, v1.10.0,
v1.11.0, v1.12.0) e `tests/fixtures/workspace_v1_13_1_graph3d.inc`, un record
v9 generato con i sorgenti della v1.13.1 (tag `v1.13.1`, commit `62b3f8f`)
prima di ogni modifica della v1.14.0, con funzioni 3D, finestra, vista e
griglia non predefinite: migrazione v1…v9 → v10 e conservazione
dell'orientazione della vista.

v1.14.0: 47 test per profilo (45 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 13 111 controlli sulla Plus e 13 105 sul Pico 2 W;
si aggiunge `graph3d_v114_test` (541 controlli); `architecture_test` cresce di 59
controlli (gruppo del 3D Graph), `oom_test` di 3 (modelli sotto esaurimento),
`storage_test` (944 → 1 020) con la lunghezza del record v10 (le prove
d'interruzione dipendono dalla dimensione del record), `graph3d_test` passa da
111 a 88 controlli nella nuova API (le 30 verifiche delle famiglie di colore
per altezza, che non esistono più, sono sostituite dai colori FORMAT verificati in
`graph3d_v114_test`; AUTO è in OPTN › Z-AUTO). Test esistenti adattati solo
ai cambiamenti voluti della v1.14.0: record alla versione 10 (controlli del
byte di versione in `matrix_test`, `lists_csv_test`, `equation_v112_test`,
`run_matrix_v112_test`, `board_profile_test`; la «versione futura» diventa
11; in `table_test` la lunghezza della coda del record), modello del 3D
(`Graph3DState`: lista vuota al primo avvio, griglia in punti 2–50) in
`board_profile_test`, `lists_csv_test`, `matrix_test`, `limits_smoke`,
`architecture_test` e `oom_test`; `graph3d_test` portata alla nuova API con le
stesse verifiche (le espressioni dell'editor naturale sono nella forma
canonica `(x)^(2)`). Nessuna verifica è stata tolta senza sostituirla.

v1.13.1: 46 test per profilo (44 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 12 223 controlli sulla Plus e 12 217 sul Pico 2 W;
rispetto alla v1.13.0 (12 081 / 12 075) si aggiunge `fixes_v1131_test` (142
controlli, un gruppo per errore). Nessun test esistente è stato modificato; la
build con sanitizer è più severa (UBSan bloccante). La tabella sopra riporta i
conteggi misurati nella v1.13.1 (nella v1.13.0 l'intestazione e la riga
«Totale» erano rimaste ai numeri delle versioni precedenti).

v1.13.0: 45 test per profilo (43 suite C++, lo stress dell'allocatore e
l'interoperabilità FAT/exFAT): 12 081 controlli sulla Plus e 12 075 sul Pico 2 W
più ≈ 334 000 verifiche dello stress dell'allocatore; rispetto alla v1.12.0
(10 316 / 10 310) si aggiungono le quattro suite nuove (903 controlli) e
`architecture_test` (1 320) cresce per le regole nuove (parser
unico, `calc_library` puro, separazione di Run-Matrix ampliata). Test
esistenti adattati solo a cambiamenti voluti, con il motivo accanto: Pol/Rec
danno una lista, Bin a 16 bit, Simplify Manual con il m.c.m. e ▶Simp senza
«F=1», Eng con P ed E, record alla versione 9 (anche nei controlli di versione di `equation_v112_test`), modello Σ con la variabile,
OPTN nell'ordine della fx-CG50 (posizioni delle voci nei test dell'interfaccia).
Nessuna verifica è stata tolta.

`equation_v112_test` usa `tests/fixtures/workspace_v1_11_0.inc`, un record v7
generato con i sorgenti della v1.11.0 (tag `v1.11.0`, commit `f6933fd`) prima
di ogni modifica della v1.12.0, per la migrazione al v8.

v1.12.0: rispetto alla v1.11.0 (9 757 / 9 751 controlli) si aggiunge
`equation_v112_test` (244); `architecture_test` cresce (+78) per il servizio
EquationCore, i sorgenti nuovi e le regole di EQUATION, `storage_test` (+60)
perché il record è più lungo, `apps_flow_test` (+6) per le schermate nuove.
Test esistenti adattati solo a cambiamenti voluti: `apps_flow_test` (schermate,
ordine decrescente delle radici, modo Real, F5 REL, Solver in Math),
`storage_test` (coefficienti `eq::Number`), `table_test` (costruzione di un
record v3 con il blocco di EQUATION delle versioni 1–7), `board_profile_test`,
`matrix_test` e `lists_csv_test` (record alla versione 8). Nessuna verifica è
stata tolta.

v1.11.0: rispetto alla v1.10.0 (8 587 / 8 581 controlli) si aggiungono le tre
suite nuove (960 controlli); `architecture_test` cresce perché il registro ha
un servizio, due sorgenti e regole in più, `storage_test`, `matrix_test` ed
`expression_test` perché il record è più lungo e il formato Eng ha un caso
in più. Test esistenti adattati solo a cambiamenti voluti: `board_profile_test`,
`table_test` e `matrix_test` (record alla versione 7, List 7–26 ora valide),
`editor_v110_test` (in Line il tasto frazione scrive `⌟`), `expression_test`
(Eng con i simboli). Nessuna verifica è stata tolta.

v1.10.0: rispetto alla v1.9.0 (7 705 / 7 699 controlli) si aggiungono le due
suite nuove (834 controlli) e 6 controlli in `oom_test`; `architecture_test`
cresce perché il registro ha un file e una sezione dati in più, `storage_test`
perché il record è più lungo (più tagli provati). Due test esistenti sono stati adattati a cambiamenti
voluti: `board_profile_test` e `table_test` (il record ora è alla versione 6),
`system_diag_test` e `apps_flow_test` (il primo campo del SET UP è
Input/Output, come sulla fx-CG50). Nessuna verifica è stata tolta.

Conteggi: la v1.8.0 rieseguita come riferimento dà 7 179 controlli (la sua
documentazione diceva 7169 perché il conteggio di `architecture_test` era fermo
a 529, il test ne esegue 539). Nessuna suite della v1.8.0 è stata rimossa o
ridotta. I test della prima 1.9.0 (`flash_fs_smoke`, `graphics_memory_smoke`,
`oom_smoke`, `system_app_smoke`) usavano l'API della base v1.5.4 e non erano
più compilati: i loro controlli sono in `internal_fs_test`,
`board_profile_test`, `graphics_memory_test`, `graph3d_test`, `oom_test`,
`expression_test` (limiti strutturali) e `system_diag_test`.

## Simulatore

```bash
build/host-pico_plus_2w/picocalc_sim <cartella-output> <script|->
```

Con il profilo Pico Plus 2 W il simulatore crea il pool PSRAM da 8 MB (come il
firmware dopo il self-test), quindi GRAPH, PICT e 3D usano i limiti della Plus;
con `build/host-pico2w` quelli del Pico 2 W.

Comandi dello script: `type <testo>`, `key <NOME>` (EXE, EXIT, MENU, DEL, AC, STO,
F1–F10, SHIFT, OPTN, VARS, SETUP, frecce, HOME, END, PGUP, PGDN), `hold <NOME>`,
`ctrl <lettera>`, `wait <ms>`, `shot <nome>` (PNG 640×640, scala 2×).

Gli script degli screenshot sono `tests/sim/run_matrix.txt` (Run-Matrix 12–19),
`tests/sim/run_matrix_v110.txt` (editor MAT/VCT, matrici, parentesi colorate,
CLIP, cursore sull'errore, modalità Line: 44–51), `tests/sim/run_matrix_v111.txt`
(List 1–26 e sottonomi, CSV, MatAns, celle esatte, Line con ⌟ e INS,
risultato scorrevole, Eng, Simp, Base-N, comandi grafici: 52–61),
`tests/sim/graph.txt` (GRAPH e TABLE 06, 07, 20–37), `tests/sim/graph3d.txt`
(3D Graph 40–43, rigenerati nella v1.14.0), `tests/sim/graph3d_v114.txt` (3D Graph
v1.14.0: 88–104) e `tests/sim/run_matrix_v112.txt` (SolveN e FMin, OPTN,
CATALOG, distribuzioni, CONVERT, modelli d²/dx² e Σ, istruzioni multiple,
VARS STAT, Graph Y>, G∫dx, ENG: 77–87). Il simulatore scrive PNG non compressi: prima di copiarli in
`docs/screenshots` vanno compressi senza perdita (nella v1.11.0: palette PNG
con `compress_level` 9 tramite Pillow, con la verifica che i pixel restino
identici):

```bash
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v110.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v111.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/run_matrix_v112.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d.txt
build/host-pico_plus_2w/picocalc_sim /tmp/shots tests/sim/graph3d_v114.txt
```

Il simulatore usa una scheda microSD in RAM (16 MB) oppure, con
`PICOCALC_SD_IMAGE=<immagine>`, un'immagine FAT/exFAT reale; `wait <ms>` serve
anche a far scadere i messaggi temporanei (1,6 s) prima di uno screenshot.

```bash
truncate -s 64M sd.img && mkfs.fat -F 32 sd.img      # oppure: dd da una scheda reale
PICOCALC_SD_IMAGE=sd.img build/host-pico_plus_2w/picocalc_sim /tmp/shots -
build/host-pico_plus_2w/sd_image_tool sd.img ls PICOCALC/GRAPH
fsck.fat -n sd.img
```

## Verifiche del firmware senza hardware

- `picotool info -a build/<scheda>/picocalc_fxai_<scheda>.uf2`: nome, versione
  (`1.10.0 (pico_plus_2w)` / `1.10.0 (pico2w)`), board, SDK.
- Simboli del linker: stack 32 KB (`__StackBottom` 0x20078000 – `__StackTop`
  0x20080000), fine dell'immagine flash prima dell'area dati.
- `-fstack-usage` su tutto il firmware (file `.su` nella cartella di build) per
  controllare i frame, anche ricorsivi
  (vedi [MATH_ENGINE.md](MATH_ENGINE.md#limiti-protezione-dello-stack-da-32-kb-dellrp2350)).
- Controllo della dimensione dell'immagine nella partizione firmware a ogni
  build (errore se la supera).

## Collaudo su hardware (da fare)

Procedure in [HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md) (HW-90…HW-99),
[HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md) (HW-40…HW-82) e
[HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) (HW-01…HW-36, da ripetere con
la 1.9.0); vedi anche [HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo).
Quando una voce sarà provata sulla PicoCalc, aggiornare
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) a *TESTED ON HARDWARE* indicando data,
revisione della scheda e risultato.
