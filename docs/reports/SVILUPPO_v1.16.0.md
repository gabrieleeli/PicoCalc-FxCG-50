# Sviluppo della v1.16.0 — eActivity (M2) e congelamento del Pico 2 W

Milestone **M2** del piano ([ANALISI_FONTI_v1.15.0.md](ANALISI_FONTI_v1.15.0.md)):
eActivity alla parità con il capitolo 10 del manuale fx-CG50 su **entrambe le
schede**. Analisi e tabella di parità scritte prima del codice:
[ANALISI_FONTI_v1.16.0.md](ANALISI_FONTI_v1.16.0.md). Con questa versione il
**Raspberry Pi Pico 2 W è congelato**: dalla v1.17.0 le modalità nuove vanno
solo sulla Pimoroni Pico Plus 2 W. **Nessuna prova su hardware**:
[HARDWARE_TEST_v1.16.0.md](../HARDWARE_TEST_v1.16.0.md) (HW-202…HW-219).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `08a4e6a` (v1.15.0); `git fetch origin main` all'inizio e prima della pubblicazione |
| Branch di lavoro | `claude/fervent-albattani-hepkis` (commit intermedi) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.16.0: eActivity…»; tag e release dal workflow (`workflow_dispatch` con «publish») |

## 2. Cosa è stato fatto

| Parte | File |
|---|---|
| Nucleo puro: documento, righe (testo, calcolo, stop, immagine, striscia), attributi a intervalli, memo, testo con espressioni Math (U+F8F0…U+F8F1), unità del testo, a capo sulle parole, 24 tipi di striscia con disponibilità per scheda, codifica `.pceact` con garanzia forte | `include/eactivity.hpp`, `src/eactivity.cpp` |
| Controller sottile: elenco dei file e cartelle, workspace, tutte le barre F1–F6 del capitolo (tabelle del guscio comune con pagine e sottomenu), modali, disegno | `src/apps/eactivity_app.cpp`, `EActivityApp` in `include/apps.hpp` |
| Codifica per parti del workspace (F2.5, spostata dalla M1): `part::History…Graph3D`, `encodePart`/`decodePart`/`resetPart`, `encodeParts`/`decodeParts` (tutto o niente), voci di cronologia | `include/workspace.hpp`, `src/workspace.cpp` |
| Sezione `0x0102` del record: cartella, ultimo file, dati del menu principale durante un documento (memorie) o una striscia (parti), rimessi a posto alla lettura del record | `src/workspace_sections.cpp`, `EActivityState` |
| Sessione da striscia: `Context::beginStrip/endStrip/strip/inStrip`, `stripentry`, barra di stato azzurra, MENU / SHIFT ' / SHIFT , / `home()` tornano all'eActivity | `include/app.hpp`, `src/app_manager.cpp`, `include/ui_kit.hpp`, `src/ui_kit.cpp` |
| Schermate d'ingresso delle strisce nelle modalità (senza conoscere eActivity) | Run-Matrix (RUN in Math, Matrix/Vector Editor, 'MAT/VCT), Graph (grafico), Statistics (Stat Graph, 'LIST con i soli tasti di modifica), Equation (Solver, SIML, POLY), 3D Graph (grafico) |
| Registro: eActivity **Available** su entrambe le schede; servizio `EActivityCore`; dati Settings, Memory, Matrices, Lists, EActivity; collegamenti alle sei modalità delle strisce | `src/app_registry.cpp`, `include/app_registry.hpp`, `src/firmware.cpp` |
| Messaggi della fx-CG50 come titoli: «Not Found», «No MEMO», «Invalid Name», «Break» | `src/app_manager.cpp` |
| Barra F1–F6: «▶» per la pagina seguente (il font non ha «▷») | `src/apps/mode_shell.cpp` |
| Pico 2 W: sorgenti di eActivity e quelli fuori dai percorsi di calcolo e disegno compilati con `-Os` | `CMakeLists.txt`, `FXAI_PICO2W_SIZE_SOURCES` in `cmake/fxai_sources.cmake` |
| Congelamento: `architecture_test` fallisce se l'insieme delle modalità Available del Pico 2 W cambia | `tests/architecture_test.cpp` |

## 3. Tabella di parità (finale)

Ogni voce ha almeno una prova in `tests/eactivity_test.cpp` (stesso file sui
due profili; i numeri tra parentesi sono le sezioni del test).

| # | Voce | Stato | Prova |
|---|---|---|---|
| E01 | elenco file all'ingresso, stato vuoto | presente | elenco vuoto e disegno; screenshot 108 |
| E02 | OPEN di file e cartelle, EXIT risale | presente | cartelle SCUOLA/A/B, OPEN, EXIT fino alla radice |
| E03 | NEW: nome di 8 caratteri A–Z { } ' ~ 0–9 | presente | minuscole convertite, `_` rifiutato, file creato subito; screenshot 109, 110 |
| E04 | DELETE con «Delete eActivity?» | presente | No lascia il file, Sì lo elimina |
| E05 | SEARCH dall'inizio del nome, «Not Found» | presente | «lu» trova LUNGO, «ZZ» dà Not Found |
| E06 | MEMO dall'elenco: JUMP, EDIT, DETAIL, DELETE, DEL-ALL, «No MEMO» | presente | modifiche scritte nel file; JUMP apre alla riga; screenshot 127 |
| E07 | errore all'apertura senza perdita | presente | file danneggiato → «Data ERROR», file oltre il limite → «Memory ERROR», si resta sull'elenco |
| E08 | cinque tipi di riga, barra di scorrimento | presente | codifica senza perdite; disegno di ogni tipo |
| E09 | ↑↓, SHIFT ↑/↓, JUMP TOP/BOTTOM/PageUp/PageDown | presente | posizioni dopo ogni tasto |
| E10 | riga di testo 255 byte, a capo sulle parole, EXE a capo | presente | 300 caratteri → 255; a capo e unione con DEL; `eact::wrap` |
| E11 | A⇔a | presente | «aB» → «Ab» con A⇔a attivo |
| E12 | AC cancella la riga visualizzata | presente | ultima e prima riga visiva di una riga lunga |
| E13 | espressioni Math nel testo, senza a capo | presente | MATH › frazione, valore 1/2, ← → dentro e fuori; screenshot 111 |
| E14 | CHAR | presente | pagine MATH e LANG; screenshot 120 |
| E15 | TEXT ⇔ CALC | presente | testo → calcolo e calcolo → testo con l'espressione Math |
| E16 | riga di calcolo come Run-Matrix | presente | 2+3 = 5, nuova riga di calcolo in fondo, «Syntax ERROR» con la riga intatta; screenshot 112 |
| E17 | forme esatte, S⇔D | presente | 1/3 ⇔ decimale da OPTN |
| E18 | scorrimento delle righe di calcolo lunghe | presente | somma di 30 termini, disegno e risultato |
| E19 | ricalcolo fino alla riga di stop, AC = Break | presente | riga dopo lo stop non ricalcolata; 80 righe interrotte da AC con «Break» |
| E20 | INSERT › STOP | presente | |
| E21 | INSERT › TEXT/CALC/PICTURE sopra la riga | presente | |
| E22 | DEL-LINE con conferma | presente | No e Sì, anche per la riga di stop |
| E23 | 'MAT/VCT | presente | editor MAT/VCT con le memorie del file, EXIT torna |
| E24 | 'LIST | presente | editor delle liste senza GRAPH/CALC/TEST/DIST, EXIT torna |
| E25 | memorie separate per file | presente | Mat A e List 1 del documento non visibili dal menu principale, salvate e ricaricate con il file |
| E26 | COLOR › MARKER, SET/EXIT, più righe | presente | screenshot 113 |
| E27 | COLOR › CHAR | presente | colore e marcatore indipendenti |
| E28 | niente colore sul risultato | presente | attributi solo sull'espressione |
| E29 | MEMO › INSERT, finestra di 255 byte, icona | presente | screenshot 114 |
| E30 | MEMO › Catalog e salto | presente | cursore al primo carattere della riga; screenshot 124 |
| E31 | MEMO › VIEW e DETAIL | presente | |
| E32 | MEMO › DELETE con conferma | presente | |
| E33 | INSERT › PICTURE | presente | `.pcpict` di PICT e CAPT; screenshot 122 |
| E34 | immagine selezionata con bordo rosso | presente | pixel del bordo; screenshot 123 |
| E35 | FORMAT: Line Style, Line Color, Clear | presente | SHIFT ↑ |
| E36 | DELETE dell'immagine | presente | |
| E37 | STRIP: 24 tipi, inserimento sopra la riga | presente | screenshot 115 |
| E38 | titolo di 16 caratteri, modificabile | presente | il primo carattere sostituisce, EXIT annulla, → modifica; screenshot 116 |
| E39 | EXE apre la modalità con i dati della striscia | presente | Graph vuoto, poi i dati della striscia; undici tipi aperti e chiusi |
| E40 | dati indipendenti dal menu principale | presente | Y1 del menu principale intatto; record con i dati rimessi a posto |
| E41 | barra di stato azzurra | presente | `StatusInfo::strip`; screenshot 117 |
| E42 | SHIFT ' alterna | presente | dalla modalità e dal documento |
| E43 | SHIFT , cambia applicazione | presente | nuova striscia Solver aperta |
| E44 | FILE › SIZE | presente | |
| E45 | Notes | presente | INSERT, MATH, JUMP, DEL-LINE, testo nella striscia; screenshot 119 |
| E46 | Conics Graph senza dati | non applicabile | Conic Graphs non c'è ancora (M7, solo Plus); striscia «non disponibile» |
| E47 | striscia di una modalità non disponibile | presente | tipo sconosciuto e Picture Plot conservati byte per byte dopo SAVE |
| E48 | SAVE, SAVE·AS con sovrascrittura | presente | No torna al nome, Sì sostituisce |
| E49 | OPT | presente (D2) | |
| E50 | CAPACITY | presente | screenshot 121 |
| E51 | «Salvare il file?» Sì / No / AC | presente | AC resta, Sì salva, No lascia il file; screenshot 125 |
| E52 | limite del file | presente | 24 KB / 256 KB; «Memory ERROR» digitando oltre, documento entro il limite |
| E53 | 128 KB al primo uso | differenza D3 | limite del documento per scheda |
| E54 | «Only one memo allowed per line.» | presente | INSERT su riga con memo, DEL tra due righe con memo |
| E55 | «Image wrong size for insertion.» | presente | immagine 400 × 10 |
| E56 | cartelle fino a tre livelli | presente | il quarto livello: «Sub-folders cannot be displayed» |

## 4. Differenze inevitabili

| # | Voce | Motivo |
|---|---|---|
| D1 | immagini `.g3p` | la PicoCalc usa `.pcpict` (Graph, CAPTURE); schermo 320 × 320: immagini alla larghezza della finestra |
| D2 | OPT | su FAT/LittleFS non c'è da compattare: completa o annulla i salvataggi interrotti e mostra lo spazio libero |
| D3 | 128 KB al primo uso | limite del documento aperto per scheda (24 KB / 256 KB) |
| D4 | `.g1e/.g2e/.g3e` | formato proprietario non letto |
| D5 | E-CON Top / E-CON Result | E-CON4 è External |
| D6 | tasti S⇔D, ' e , | S⇔D in OPTN della riga di calcolo; SHIFT ' e SHIFT , = F7 e poi ' o , |
| D7 | CHAR | i caratteri che il font sa disegnare, in quattro pagine |

Strisce che arrivano più avanti, solo sulla Pimoroni Pico Plus 2 W: Conics
Graph/Editor (M7), Recur Editor (M5), Dynamic Graph (M6), Financial (M8),
SpreadSheet (M9), Picture Plot (M11), Geometry (M12). Fino ad allora sono
«non disponibili»; nei documenti si conservano intatte.

## 5. Test, fuzzing, simulatore

{{TESTS}}

Errori trovati durante lo sviluppo e corretti prima della pubblicazione:
le espressioni nel testo usavano U+E000/U+E001, che sono i simboli statistici
x̂ e x̄ del font e del CATALOG (spostate a U+F8F0/U+F8F1); la barra F1–F6
disegnava «?» al posto di «▷» (glifo assente, ora «▶»); la richiesta
«Salvare il file?» mostrava «F6/EXIT: No» ma EXIT tornava al documento (ora
EXIT = No, AC torna, come la fx-CG50 con 6 e AC).

## 6. Firmware e memoria

{{FIRMWARE}}

Stack: nessuna ricorsione nuova. L'a capo, il ricalcolo e la codifica sono
iterativi; le espressioni nel testo usano l'editor naturale e il disegno 2D
già limitati (12 livelli di modelli, 400 nodi). {{STACK}}

## 7. Cancello

{{GATE}}
