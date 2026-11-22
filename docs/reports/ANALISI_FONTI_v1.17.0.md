# Analisi delle fonti — v1.17.0: base di tutte le modalità

Scritto **prima** del codice della v1.17.0. Questa versione sostituisce il
piano M3…M15 «una modalità completa per versione» ([CONTINUE.md](../../CONTINUE.md))
con due fasi:

1. **v1.17.0 (questa)**: ogni modalità del registro si inizializza
   correttamente ed è **usabile**: si apre, fa il suo lavoro principale, salva e
   ripristina il proprio stato, si chiude senza perdite di memoria.
2. **dalla v1.18.0**: una modalità per versione portata alla parità completa
   con il manuale fx-CG50 (prima Statistics nella v1.18.0, poi le altre).

Regole che non cambiano: manuale fx-CG50 v3.60 solo come descrizione del
comportamento (nessun codice, grafica o testo copiato); **Pico 2 W congelato
alla v1.16.0** (nessuna funzione, schermata o crescita dell'immagine salvo
correzioni di errori condivise; `architecture_test` fallisce se cambia
l'insieme delle sue modalità aperte); le modalità nuove solo sulla Pimoroni
Pico Plus 2 W; nessun «TESTED ON HARDWARE» senza prova fisica (le prove nuove
partono da **HW-220**); un solo commit su `main` in fast-forward.

## 1. Fonti

| # | Fonte | Uso |
|---|---|---|
| 1 | *fx-CG50 Software User's Guide* v3.60 (stesso PDF della v1.15.0): cap. 5-42…5-51 (Dyna Graph, Recursion, Conic Graphs), 7 (Financial), 8 (Program, solo i servizi della modalità), 9 (Spreadsheet), 13 (Link, solo come riferimento: il protocollo Casio non è usato), 14 (Geometry), 15 (Picture Plot), 17 (Python), 18 (Distribution), Appendice α (messaggi) | descrizione del comportamento minimo di ogni modalità |
| 2 | Codice a `main` = `eb2338a` (v1.16.0) | fondamenta della v1.15.0 e della v1.16.0 da riusare |
| 3 | Standard dei linguaggi C, C++, C# (come nella v1.15.0) | sottoinsiemi di Program |
| 4 | MicroPython (licenza MIT), porta `ports/embed` | solo se scelta nella domanda Q3 (§7) |

### 1.1 Punto di partenza verificato

| Controllo | Esito |
|---|---|
| `git status` | branch `claude/eager-lamport-9fht7w` = `main` (`eb2338a`), albero pulito |
| `tests/run_host_tests.sh --board PICO_PLUS_2W` | **56/56** (1 min 40 s con 4 processori) |
| Strumenti | GCC 13 host, clang 18, arm-none-eabi-gcc, Pico SDK in `/opt/sdk/pico-sdk` (tinyusb, cyw43, lwip, mbedtls, btstack), rete verso github.com |
| Immagini della v1.16.0 | Plus **2 097 548 B** (50 % di 4 MB, limite 3 774 873 B: restano **1 677 325 B**); Pico 2 W **1 944 212 B** (congelato, budget 2 064 384 B) |

## 2. Fondamenta riusate (nessuna riscrittura)

| Fondamenta (v1.15.0/v1.16.0) | Dove serve nella v1.17.0 |
|---|---|
| `ModeShell` (pila di schermate, `FKeyBar` con pagine e SHIFT, `ConfirmBox`, `AskBox`, `SetupItem`, `varsItems`, `shellCatalog`) | **ogni** modalità nuova: barra F1–F6, SET UP (SHIFT MENU) da tabella, VARS, conferme e richieste |
| `TypedFiles` + `FileDialog` | file di ogni modalità, MEMORY › FILES |
| `TextEditor` | Program (sorgenti), Python (script), barra della formula del foglio |
| `StepTask` (`Runner`, AC → «Break») | ricalcolo del foglio, animazione di Dyna, tabelle di Recursion, esecuzione di Program e Python, trasferimenti di Link, risolutore TVM/IRR |
| `Graph2D` (`Plotter`, `PointSource`, `TraceCursor`, zoom, `drawAxes`) | Dyna, Recursion (termini e ragnatela), Conic (curve e TRACE), Geometry, Picture Plot (regressione sopra lo sfondo) |
| `BindingResolver` + celle `cell_a1` nel lexer unico + `DependencyGraph` | formule del foglio (riferimenti, intervalli, «Circular ERROR»), variabili n/an/an+1 di Recursion, coefficiente di Dyna, variabili locali di `fx::calc` in Program |
| Interprete C/C++/C# (`program.hpp`, `Machine` come `step::Task`, `WorkspaceHost`) | Program (esecuzione, input, grafici tramite `graph_commands`, `fx::calc`, A–Z) |
| `calc_library` TVM (Smpl, Cmpd, Cash, Amt, Cnvt, Days) | Financial (gli stessi calcoli di OPTN › TVM in Run-Matrix: un solo motore) |
| `statistics.hpp` (`regress`, 9 tipi) | Picture Plot (regressione sui punti), Spreadsheet (Mean, Median…) |
| `pict.hpp` (`.pcpict`) | Picture Plot (sfondo) |
| Record v11 a sezioni TLV, `SectionCodec`, `preservedSections` | sezione propria di ogni modalità (`0x0100 + AppId`) |
| Parti del workspace (`encodePart`…) e sessione da striscia (`Context::beginStrip`, `stripentry`) | strisce di eActivity delle modalità nuove (Conics, Recur, Dynamic, Financial, SpreadSheet, Geometry, Picture Plot) |

## 3. Nuovo stato `AppStatus::Base`

| Stato | Significato | Menu | Registro |
|---|---|---|---|
| `Available` | parità completa con il capitolo del manuale (tabella di parità chiusa) | icona normale | `sources`, nessun `missing` |
| **`Base`** (nuovo) | aperta e usabile, parità **non** completa | si apre come una Available; **piccolo segno sull'icona** (triangolo nell'angolo in alto a destra) e «EXE: apri (base)» nella riga in basso | `sources` presenti; `missing` dice cosa manca alla parità |
| `Planned` | controller non scritto | icona grigia, schermata «non disponibile» | nessun sorgente |
| `External` | richiede hardware assente | come Planned | nessun sorgente |

`architecture_test`: Available e Base hanno un controller registrato e i
sorgenti compilati; solo Available ⇔ nessun `missing`; i collegamenti vanno
verso modalità Available o Base; sul Pico 2 W l'insieme delle modalità
**aperte** (Available ∪ Base) resta quello della v1.16.0 (nessuna Base). Il
disegno del segno è compilato solo sulla Plus: l'immagine del Pico 2 W non
cambia. **Statistics resta Available** (è usabile da prima e la parità con il
capitolo 6 è il lavoro della v1.18.0: la FEATURE_MATRIX elenca le voci
mancanti).

## 4. Modalità Planned → Base (solo Plus)

Per ogni modalità: funzioni minime (dalla richiesta della v1.17.0), sezione del
record, file tipizzato, servizi del registro, costo stimato sulla Plus. I costi
in flash sono stimati confrontando moduli simili della mappa della v1.16.0
(`apps/table_app.cpp` 28 KB, `apps/equation_app.cpp` 25 KB,
`apps/statistics_app.cpp` 38 KB); saranno misurati con `tools/map_sizes.py` nel
rapporto di sviluppo. RAM: stato persistente nel workspace + memoria di lavoro
durante l'uso, con il limite del profilo SRAM (PSRAM assente) dove conta.

| Modalità | Funzioni minime | Sezione | File | Servizi | Flash | RAM (PSRAM / solo SRAM) |
|---|---|---|---|---|---|---|
| **Spreadsheet** | griglia A–Z × 999 con cursore e scorrimento; celle con numeri, testo, formule `=A1+B2*2`; riferimenti relativi e assoluti (`$A$1`); intervalli `A1:A10` con Sum, Mean, Min, Max, Median; ricalcolo automatico; «Circular ERROR»; CUT/COPY/PASTE di celle e blocchi; FILL; colonna → List n e List n → colonna | `0x0103` foglio aperto (celle, cursore, nome del file) | `.pcsheet` (`PCSH`, binario) | Expression, Stats, Binding, ModeShell, TypedFiles, TextEdit, StepTask, Storage | ~60 KB | celle sparse: 26 000 celle con la PSRAM, 3 000 solo SRAM («Memory ERROR» oltre) |
| **Dyna Graph** | Y= con un coefficiente variabile (A–Z); Start, End, Pitch; animazione a passi con velocità (Slow/Normal/Fast) e AC per fermarla; V-Window di Graph; ciclo continuo e passata singola | `0x0105` funzione, variabile, velocità, tipo (Start/End/Pitch restano in `modeVars` come VARS › DYNA) | `.pcdyna` (`PCDY`) | Expression, Graph2D, Binding, ModeShell, TypedFiles, StepTask, Storage | ~25 KB | fotogrammi ricalcolati (nessuna cache): ≤ 2 KB per fotogramma |
| **Recursion** | an+1 = f(n, an) e an+2 = f(n, an, an+1); a0 e a1; tabella per n da Start a End; grafico dei termini; ragnatela per an+1; Σan | `0x0107` tipo, formule, Σ, vista (an+1 resta anche in `modeVars.recFormula`, Start/End/a0/a1 in `modeVars`) | `.pcrecur` (`PCRC`) | Expression, Graph2D, Binding, ModeShell, TypedFiles, StepTask, Storage | ~30 KB | tabella ≤ 1 000 righe × 3 double = 24 KB |
| **Conic Graphs** | parabola, circonferenza, ellisse, iperbole in forma cartesiana da un elenco di modelli; coefficienti; disegno e TRACE; G-Solve: vertice, fuoco, centro, raggio, asintoti, eccentricità | `0x0108` modello, coefficienti | `.pcconic` (`PCCN`) | Expression, Graph2D, Solvers, ModeShell, TypedFiles, Storage | ~30 KB | < 4 KB |
| **Financial** | interesse semplice; TVM (n, I%, PV, PMT, FV, P/Y, C/Y) che risolve qualsiasi variabile, BGN/END; Cash Flow (NPV, IRR, PBP, NFV); ammortamento (BAL, INT, PRN, ΣINT, ΣPRN); conversione APR ⇔ EFF; giorni tra due date; risultato → Ans | `0x010B` impostazioni (BGN/END, 365/360), valori dell'interesse semplice, flussi di cassa, PM1/PM2, date (le variabili TVM restano in `modeVars` come VARS › TVM) | `.pcfin` (`PCFN`) | Expression, Solvers, Binding, ModeShell, TypedFiles, StepTask, Storage | ~30 KB | < 8 KB |
| **Program** | elenco con NEW, EDIT, DEL, RUN; editor con evidenziazione minima (parole chiave, commenti, stringhe, numeri); esecuzione con l'interprete della v1.15.0: output a schermo, input da tastiera, comandi grafici (disegnati da Graph), variabili A–Z e liste; AC = Break; errori con riga e colonna e cursore sull'errore | `0x010A` ultimo file, posizione del cursore | `.pcprog` (`PCPG`, testo UTF-8 con la riga del dialetto; `.c`, `.cpp`, `.cs` del PC si aprono come testo) | Expression, Interp, Binding, GraphCmd, ModeShell, TypedFiles, TextEdit, StepTask, Storage | ~35 KB di modalità + ~95 KB dell'interprete (oggi compilato ma scartato dal linker) | sorgente ≤ 256 KB; memoria del programma 200 000 celle con la PSRAM, 20 000 solo SRAM |
| **Link** | invio e ricezione del workspace o di singoli file sulla USB seriale, formato proprio a pacchetti con CRC-32; avanzamento a schermo; AC annulla; strumento per il PC `tools/picocalc_link.py` con test di andata e ritorno | `0x010D` ultimo pacchetto ricevuto, statistiche | `.pclink` (`PCLK`, pacchetto: workspace e/o file) | Storage, Files, TypedFiles, ModeShell, StepTask | ~20 KB | buffer di 4 KB + un file alla volta |
| **Geometry** | punti, segmenti, rette, circonferenze, poligoni messi con il cursore; selezione, spostamento, cancellazione; misure di lunghezza, angolo, area, coordinate; zoom e pan | `0x0110` costruzione aperta, vista | `.pcgeo` (`PCGO`) | Expression, MathCore, Graph2D, ModeShell, TypedFiles, StepTask, Storage | ~40 KB | ≤ 500 oggetti (≈ 40 KB) |
| **Picture Plot** | apre una `.pcpict` come sfondo; punti messi con il cursore e numerati; coordinate → List 1 e List 2; regressione col motore di Statistics; retta o curva sopra lo sfondo | `0x0111` sfondo, punti, regressione scelta | `.pcplot` (`PCPP`) | Expression, Stats, Graph2D, Files, ModeShell, TypedFiles, Storage | ~30 KB | ≤ 999 punti; sfondo 320 × 320 × 2 B = 200 KB (PSRAM) / mezza risoluzione solo SRAM |
| **Python** | editor degli script `.py`, RUN, REPL, output sul display; moduli `math` e disegno minimo; heap separato in PSRAM; AC = KeyboardInterrupt | `0x0114` ultimo script, cronologia della REPL | `.py` (testo) | ModeShell, TypedFiles, TextEdit, StepTask, Graph2D, Storage | dipende da Q3: MicroPython embed ≈ 250–320 KB; sottoinsieme proprio ≈ 80 KB | heap 512 KB in PSRAM; solo SRAM: 48 KB o modalità non avviabile («Memory ERROR») |

Totale stimato sulla Plus: **≈ 0,7–0,9 MB** con MicroPython → immagine
≈ 2,8–3,0 MB, sotto il limite del 90 % (3 774 873 B). La RAM statica non
cresce: lo stato delle modalità è nel workspace (heap) e le memorie di lavoro
esistono solo mentre la modalità è aperta.

**Un file tipizzato per ogni modalità.** Le modalità con documenti
(Spreadsheet, Program, Geometry, Picture Plot, Python) salvano i loro
documenti; le modalità «a stato» (Dyna, Recursion, Conic, Financial) hanno
FILE › SAVE·AS / OPEN che scrive e legge la loro sezione del record in un
file (stessa codifica, quindi nessun formato in più da mantenere); Link usa il
proprio formato di pacchetto anche come file (`.pclink`), così un pacchetto
ricevuto si conserva e si applica dopo. Tutti compaiono in MEMORY › FILES.

**Strisce di eActivity (Plus).** Le modalità nuove con una striscia (Conics
Graph/Editor, Recur Editor, Dynamic Graph, Financial, SpreadSheet, Geometry,
Picture Plot) aggiungono la propria parte del workspace (la stessa codifica
della sezione) e la schermata d'ingresso (`stripentry`):
`eact::stripAvailable` le rende disponibili da sola. Il campo `parts` delle
strisce passa da 8 a 32 bit solo dove serve senza cambiare il Pico 2 W.

## 5. Record v12 (solo Plus)

| Voce | Decisione |
|---|---|
| Versione | **12 sulla Plus**; il Pico 2 W (congelato) continua a scrivere la **11** e il suo codice non cambia |
| Contenuto | v11 + (a) **firma** dopo la versione: scheda e versione del firmware che ha scritto il record (Link e il ritorno a una versione precedente sanno da dove viene); (b) **CRC-32 per sezione** nella coda TLV: una sezione di modalità danneggiata o non decodificabile viene **isolata** (la modalità riparte dai valori iniziali con un avviso) invece di rendere illeggibile tutto il record |
| Sezioni nuove | `0x0103` Spreadsheet, `0x0105` Dyna, `0x0107` Recursion, `0x0108` Conic, `0x010A` Program, `0x010B` Financial, `0x010D` Link, `0x0110` Geometry, `0x0111` Picture Plot, `0x0114` Python |
| Migrazione dalla v11 | un record v11 (fixture prodotta dal codice della v1.16.0) si legge intero; le modalità nuove partono dai dati che la v11 aveva già: variabili TVM, Dyna Start/End/Pitch, formula an+1 e Start/End/a0/a1 di Recursion (`modeVars`, VARS di Run-Matrix); il primo salvataggio scrive la v12 |
| Test | `workspace_v117_test`: v11 → v12 con le fixture, v12 → v12 senza perdite, sezione danneggiata isolata, sezioni di una versione più nuova conservate; `fuzz_workspace` aggiornato |
| Limite noto | un firmware ≤ v1.16.0 non legge un record v12 (stesso problema A1 della v1.13.1 per i ritorni indietro): documentato in STORAGE.md; il Pico 2 W non è interessato |

## 6. Fase 1: casi di inizializzazione

`tests/apps_init_test.cpp` prende **dal registro** ogni modalità Available o
Base della build (12 sul Pico 2 W, 12 + 10 sulla Plus) e per ognuna prova
tutti i casi sotto, sul profilo di scheda della build (la suite gira sui due
profili). Per ogni modalità il test conosce solo una **sequenza tipica** e una
**sonda di stato** (un dato che la sequenza cambia e che deve sopravvivere):

| Modalità | Sequenza tipica | Sonda di stato |
|---|---|---|
| Run-Matrix | `1+2` EXE, ↑, EXIT | riga della cronologia `1+2` = 3 |
| Statistics | List 1 ← 1, 2, 3; CALC 1-VAR; EXIT | List 1 |
| eActivity | NEW «INIT», riga di calcolo `2+3`, SAVE | file `INIT.pceact`, ultimo file |
| Graph | Y1 = `x²`, DRAW, EXIT | Y1 |
| Table | Y1 = `x`, TABLE, EXIT | Start di TABLE e righe |
| Equation | POLY grado 2, coefficienti, SOLVE | coefficienti di POLY |
| Memory | F1/F3, ↓, EXIT | nessun dato cambiato (la sonda verifica che il workspace resti identico) |
| System | SET UP › Angle = Deg | `settings.angle` |
| 3D Graph | Z1 = `x+y`, DRAW, AC, EXIT | Z1 |
| Conversion | 1 km → m, EXE | Ans = 1000 |
| AI Chat | testo senza rete, EXE | nessuna conversazione persa, messaggio di rete non disponibile |
| Symbols | pagine ←/→, EXIT | nessun dato cambiato |
| Spreadsheet (Plus) | A1 = 2, B1 = `=A1*3`, ricalcolo | B1 = 6 |
| Dyna Graph (Plus) | funzione, Start/End/Pitch, DYNA, AC | funzione e variabile |
| Recursion (Plus) | an+1 = `2an+1`, a0 = 1, TABLE | formula e tabella |
| Conic Graphs (Plus) | modello circonferenza, coefficienti, DRAW, G-Solve | modello e coefficienti |
| Financial (Plus) | CMPD, n, I%, PV, SOLVE FV | variabili TVM |
| Program (Plus) | NEW «P», sorgente, RUN | file `P.pcprog`, output |
| Link (Plus) | invio del workspace su una porta simulata | statistiche dell'ultimo trasferimento |
| Geometry (Plus) | due punti e un segmento, misura | oggetti |
| Picture Plot (Plus) | tre punti, regressione | punti |
| Python (Plus) | REPL `1+2`, script con `print` | ultimo script |

Casi (ognuno su ogni modalità):

| # | Caso | Come |
|---|---|---|
| C1 | avvio a freddo, workspace vuoto | record cancellato, `Firmware::boot` |
| C2 | avvio dopo RESET di System | SYSTEM › RESET (tutti i dati) e riavvio |
| C3 | record v11 valido | fixture `workspace_v1_16_0.inc` (codice della v1.16.0) |
| C4 | record v11 corrotto o troncato | la fixture con byte cambiati, tagliata a metà, CRC del record sbagliato: si parte vuoti senza crash; sulla Plus anche una sezione v12 danneggiata (isolata) |
| C5 | profilo solo SRAM | `mem::testing::simulatePsram(0)` e limiti del profilo SRAM; anche self-test della PSRAM fallito |
| C6 | memoria quasi esaurita | SRAM simulata piena: un «Memory ERROR» nella finestra, nessun crash, la modalità resta usabile quando la memoria torna |
| C7 | microSD assente e presente | `board::host::setFileSystem(nullptr)` e la scheda RAM / immagine FAT |

Controlli per ogni caso: (K1) si apre da MENU (tasto di accesso diretto) con
il proprio controller; (K2) disegna la schermata iniziale **e** la barra F1–F6
(pixel della barra diversi dallo sfondo); (K3) accetta la sequenza tipica senza
finestre d'errore inattese; (K4) esce con MENU e, per le modalità con una
striscia, con SHIFT ' e MENU dalla striscia di eActivity; (K5) gli allocatori
SRAM e PSRAM tornano al livello di partenza (nessuna perdita) dopo l'uscita;
(K6) la sonda è uguale dopo uscita e rientro e dopo un riavvio simulato
(salvataggio, nuovo `Firmware` sulla stessa flash). Ogni errore trovato è
corretto con il suo test (in `apps_init_test` o nella suite della modalità) e
descritto nel rapporto di sviluppo.

## 7. Domande aperte (prima del codice)

| # | Domanda | Proposta |
|---|---|---|
| Q1 | Aggiungere la modalità **Distribution** (cap. 18 della fx-CG50)? Cambierebbe il menu (24 icone, un tasto nuovo dopo N) | aggiungerla come Base **insieme a Statistics nella v1.18.0**, che completa lo stesso motore DIST |
| Q2 | Confermi **Program** con sottoinsiemi interpretati di C, C++ e C#? | sì: è la decisione della v1.15.0 e il nucleo esiste |
| Q3 | **Python**: MicroPython vero (porta embed) o un sottoinsieme interpretato scritto da zero? | MicroPython vero: linguaggio completo e licenza MIT, sta nel budget della Plus (§4); heap in PSRAM, AC → `KeyboardInterrupt` |
| Q4 | **Record v12** solo sulla Plus (§5) o restare alla v11 con le sole sezioni nuove? | v12 solo sulla Plus come richiesto, con l'isolamento delle sezioni danneggiate; il Pico 2 W resta alla v11 |

Le risposte vanno nel §8 prima di scrivere il codice.

## 8. Risposte (22/11/2026, prima del codice)

| # | Risposta dell'utente | Conseguenza |
|---|---|---|
| Q1 | «Sì, nella v1.18.0» | Distribution **non** entra nel registro della v1.17.0 (menu invariato, 23 icone); viene aggiunta come Base insieme alla parità di Statistics nella v1.18.0 (CONTINUE.md) |
| Q2 | «Confermo C, C++ e C#» | Program usa il nucleo F9 con i tre dialetti; `.pcprog` con la riga del dialetto, `.c`/`.cpp`/`.cs` apribili |
| Q3 | «MicroPython vero» | porta `embed` di MicroPython (MIT) in `third_party/micropython`, compilata solo sulla Plus; heap separato in PSRAM, AC → `KeyboardInterrupt`; se non sta nei budget la modalità resta Planned e la ragione è documentata |
| Q4 | «v12 solo sulla Plus» | §5: la Plus scrive la v12 (firma e CRC per sezione), il Pico 2 W resta alla v11; migrazione v11 → v12 con test |

**Nota a consuntivo (fine della v1.17.0).** La stima di §4 per MicroPython
(250–320 KB) era prudente: con la configurazione scelta (`CORE_FEATURES`, double,
interi grandi MPZ, `math`, `sys`, senza compilatore nativo né file system della
VM) Python e MicroPython occupano **135 631 B** di flash nella mappa della Plus
e 9 B di SRAM statica; l'heap (512 KB in PSRAM, 48 KB in SRAM) e la pila della
coroutine (24 KB) si allocano solo mentre la modalità è aperta. Misure di tutte
le modalità: [SVILUPPO_v1.17.0.md](SVILUPPO_v1.17.0.md#6-firmware-e-memoria).
