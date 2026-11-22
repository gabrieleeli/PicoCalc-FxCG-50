# Collaudo su PicoCalc fisica — v1.17.0 (base di tutte le modalità)

Questa procedura verifica sul dispositivo la v1.17.0
([reports/SVILUPPO_v1.17.0.md](reports/SVILUPPO_v1.17.0.md)): l'inizializzazione
di tutte le modalità su **entrambe le schede** e, sulla **Pimoroni Pico Plus
2 W**, le dieci modalità **Base** (Spreadsheet, Dyna Graph, Recursion, Conic
Graphs, Financial, Program, Link, Geometry, Picture Plot, Python) e il record
**v12**. Il **Raspberry Pi Pico 2 W è congelato**: si controlla solo che sia
rimasto quello della v1.16.0. **Nessuna voce è stata provata su hardware**:
fino a quando i risultati non arrivano dal dispositivo nessuna voce è
dichiarata provata. Riportare per ogni prova l'esito (OK / KO / non eseguita),
le righe di log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-219** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.16.0](HARDWARE_TEST_v1.16.0.md))
restano valide e vanno eseguite con il firmware 1.17.0. Prove aggiuntive:
**HW-220…HW-245**, sulla Plus salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.17.0` (`picocalc_fxai_pico_plus_2w-v1.17.0.uf2`,
   `picocalc_fxai_pico2w-v1.17.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo per scheda con la **v1.16.0** installata e dati in Run-Matrix,
   Statistics, Graph, Equation, eActivity e le variabili VARS › DYNA,
   RECURSION (formula an+1 con `"…"→an+1`) e TVM.
4. microSD FAT32 con `PICOCALC/PICT/PICT01.pcpict` (salvato da Graph); un PC
   Linux, macOS o Windows con Python 3 e `tools/picocalc_link.py`.

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-220 | Aggiornamento dalla v1.16.0 alla v1.17.0 su **entrambe** le schede, spegnimento e riaccensione | tutti i dati restano; sulla Plus le icone di Spreadsheet, Dyna Graph, Recursion, Conic Graphs, Program, Financial, Link, Geometry, Picture Plot, Python sono colorate con il **triangolo ambra** e «EXE: apri (base)»; sul Pico 2 W il menu è identico alla v1.16.0 | foto dei due menu |
| HW-221 | Pico 2 W: SYSTEM → INFO e DIAG; ogni modalità della v1.16.0 aperta e chiusa | immagine ≤ 2 064 384 B (1 944 228 B, [BUILD_STATUS.md](BUILD_STATUS.md)); heap libero come la v1.16.0 ± 1 KB; le modalità nuove dicono «Non disponibile sul Raspberry Pi Pico 2 W…» | foto, righe `[DIAG]` |
| HW-222 | Plus: dopo HW-220 Financial › Compound Interest, Recursion, Dyna Graph | n, I%, PV… di VARS › TVM, la formula an+1 e D Start/End/Pitch della v1.16.0 sono lì (migrazione v11 → v12) | foto |
| HW-223 | Plus: ogni modalità aperta da MENU con il tasto diretto, una sequenza tipica, MENU; poi SYSTEM → RESET e di nuovo | nessun blocco né finestra d'errore inattesa; dopo RESET ogni modalità parte dai valori iniziali | elenco degli esiti |
| HW-224 | Plus senza microSD (estratta) e con la scheda reinserita: Spreadsheet SAVE·AS, Program NEW, Python NEW, Link RECEIVE | senza scheda: una finestra d'errore sulla microSD («scheda microSD assente» o «archivio dei file non disponibile (microSD assente)»), nessun blocco, i dati nel workspace restano; con la scheda tutto funziona | foto |
| HW-225 | Solo se una Plus parte senza PSRAM (self-test fallito, avviso all'avvio; non c'è un modo di forzarlo dal firmware: il profilo solo SRAM è provato sul PC dal caso C5 di `apps_init_test`): Spreadsheet oltre 3 000 celle, Python SHELL | Spreadsheet: «Memory ERROR» oltre 3 000 celle; Python: heap 48 KB in SRAM, `MemoryError` gestito | foto, righe `[mem]`, oppure «non eseguibile» |
| HW-226 | Spreadsheet: A1..A20 numeri, B1 `=A1*$A$1`, COPY di B1 e PASTE fino a B20 (EDIT › SELECT), C1 `=Sum(A1:A20)`, poi A1 modificato | ricalcolo automatico; C1 aggiornato; un ciclo dà «Circ» e Circular ERROR nella riga in basso | foto, tempo del ricalcolo |
| HW-227 | Spreadsheet: 500 formule a catena, poi AC durante il ricalcolo | avanzamento in %; AC → «Break» subito, tastiera pronta | tempo di ricalcolo completo |
| HW-228 | Spreadsheet: LIST › COL→L (List 1), poi Statistics › 1-Var; FILE › SAVE·AS «CONTI», copia sul PC | List 1 con la colonna; `CONTI.pcsheet` con l'intestazione `PCSH` | `xxd -l 16 CONTI.pcsheet` |
| HW-229 | Dyna Graph: `A(X-1)^2-2`, A da 1 a 5, DYNA; FASTER/SLOWER; AC; Dynamic Type Stop | animazione fluida, AC ferma subito; Stop si ferma all'ultimo fotogramma | fotogrammi al secondo per velocità |
| HW-230 | Recursion: an+1 = 2an+1, a0 = 1, End 50, TABLE con Σan, G-PLT, WEB; an+2 = an+1 + an (Fibonacci) | valori come `base_modes_test`; tabella a passi; ragnatela con y = x | foto |
| HW-231 | Conic Graphs: circonferenza R = 3, ellisse A = 5 B = 3, iperbole; TRACE e G-SOLVE (FOCUS, ASYMPT, e) | fuochi (±4, 0) dell'ellisse; asintoti disegnati | foto |
| HW-232 | Financial: n = 360, I% = 6, PV = 100000, P/Y = C/Y = 12, PMT (END, poi BGN con SHIFT MENU); Cash Flow; ammortamento PM1 = 1 PM2 = 12; F6 →Ans | PMT = −599,55 (END), −596,57 (BGN); →Ans e poi Run-Matrix Ans uguale | foto |
| HW-233 | Program: NEW (C++), RUN; un programma con `std::cin`, uno con `fx::plot`/`fx::line` (F6 GRAPH), uno con un errore, uno con `while(true){}` e AC | output e input sul display; grafico in Graph; errore con riga e cursore; «Break» con AC | foto |
| HW-234 | Program: un `.c` e un `.cs` copiati dal PC in `PICOCALC/PROG`; EDIT e RUN; il `.pcprog` sul PC | si aprono con il dialetto dell'estensione; il `.pcprog` inizia con `// pcprog: C++` | foto |
| HW-235 | **Link → PC**: Link › SEND › ALL con `python3 tools/picocalc_link.py receive --port /dev/ttyACM0 -o ws.pclink` | trasferimento completo; `list ws.pclink` mostra il workspace; tempo | tempo, `list` |
| HW-236 | **PC → Link**: Link › RECEIVE, poi `picocalc_link.py send --port … hi.py conti.pcsheet` | pacchetto in `PICOCALC/LINK`; APPLY scrive i file; Python li vede | foto |
| HW-237 | Link: cavo USB staccato a metà trasferimento; AC durante l'attesa; log `[DIAG]` attivi durante il trasferimento | «Time Out» dopo 15 s, nessun blocco; AC annulla; i log non rompono il trasferimento (risincronizzazione) | esiti |
| HW-238 | Link: invio di un workspace dalla Plus ad un'altra Plus tramite il PC (receive, poi send del `.pclink`) | APPLY chiede conferma e sostituisce i dati; record v12 con la firma della scheda | foto |
| HW-239 | Geometry: triangolo con POLYGON, MEASURE, MOVE, ZOOM±, PAN, SAVE·AS e OPEN | area, perimetro e angoli corretti; file `.pcgeo` | foto |
| HW-240 | Picture Plot: FILE › PICT con `PICT01.pcpict`, 6 punti, LIST, REG ax+b, SAVE·AS | sfondo, punti numerati, retta sopra lo sfondo; List 1 e 2 in Statistics | foto |
| HW-241 | **Python**: SHELL `2**100`, `import math`, un blocco `for` (riga vuota per eseguire); script con `input()`; `while True: pass` e AC | risultati corretti; KeyboardInterrupt immediato con AC; la tastiera resta pronta durante gli script | foto, tempo di `sum(range(100000))` |
| HW-242 | Python: ricorsione infinita `def f(n): return f(n+1)`; script che riempie l'heap; uscita dalla modalità e SYSTEM → DIAG | RuntimeError e MemoryError senza crash (pila della coroutine con MSPLIM, nessun `[fault]`); heap libero tornato come prima di entrare | righe `[DIAG]`, `[fault]` assenti |
| HW-243 | Python: casioplot (seno su 300 punti, `show_screen()`), EXIT | grafico a schermo; EXIT torna alla SHELL | foto |
| HW-244 | Batteria tolta durante una modifica del foglio e durante la ricezione di Link; riaccensione | il record v12 si legge (slot A/B); al più l'ultima modifica persa; nessun `.tmp` dopo l'elenco | foto, `fsck.fat -n` |
| HW-245 | Ritorno alla v1.16.0 dopo la v1.17.0 sulla Plus (UF2 vecchio) | il record v12 non si legge con la v1.16.0 (limite noto, [STORAGE.md](STORAGE.md)): annotare il comportamento e non salvare; tornando alla v1.17.0 i dati riappaiono se il firmware vecchio non ha salvato | esito |
