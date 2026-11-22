# Sviluppo della v1.15.0 — fondamenta comuni delle modalità (M1)

Milestone **M1** del piano descritto in
[ANALISI_FONTI_v1.15.0.md](ANALISI_FONTI_v1.15.0.md): i servizi comuni che le
modalità mancanti useranno, su entrambe le schede dove servono (eActivity e le
modalità della v1.14.0) e solo sulla Pimoroni Pico Plus 2 W per il resto.
**Nessuna modalità nuova diventa Available**: l'insieme delle modalità
disponibili è quello della v1.14.0 su entrambe le schede (verificato da
`architecture_test`). **Nessuna prova su hardware**:
[HARDWARE_TEST_v1.15.0.md](../HARDWARE_TEST_v1.15.0.md) (HW-190…HW-201).

## 1. Git

| Voce | Valore |
|---|---|
| Partenza | `main` = `72d0921` (v1.14.0 + recap), `git fetch origin main` all'inizio e prima della pubblicazione: `main` non è avanzato |
| Branch di lavoro | `claude/fervent-albattani-hepkis` (commit intermedi) |
| Pubblicazione | un solo commit su `main` in fast-forward, «PicoCalc FxCG-50 v1.15.0: fondamenta comuni delle modalità…»; tag e release dal workflow (`workflow_dispatch` con «publish») |

## 2. Decisioni

- **Program in C, C++ e C#** (risposta dell'utente alla domanda di M0):
  sottoinsiemi interpretati scritti da zero, nucleo F9; dettagli e conseguenze
  in [ANALISI_FONTI_v1.15.0.md §6](ANALISI_FONTI_v1.15.0.md#6-decisione-per-program-domanda-allutente).
- **Codifica per parti delle sezioni esistenti** (F2.5): spostata alla M2,
  dove le strisce di eActivity la usano; la M1 consegna il record v11 e l'API
  delle sezioni.
- **Distribution** (capitolo 18 della fx-CG50): non nel registro né nel piano;
  la decisione è chiesta all'utente (CONTINUE.md).

## 3. Cosa è stato fatto

| # | Fondamenta | File | Schede |
|---|---|---|---|
| F1 | opzioni `FXAI_MODE_*`/`FXAI_SERVICE_*` (`cmake/fxai_sources.cmake`, `include/features.hpp`), campo `boards` e risoluzione del registro (`src/app_registry.cpp`), controllo bloccante di partizione, margine e mappa del linker (`cmake/check_firmware_size.cmake`, `FXAI_FIRMWARE_MARGIN_BYTES` in `cmake/fxai_board.cmake`) | registro, CMake | entrambe |
| F2 | record **v11** con coda TLV, `RecordSection`, `SectionCodec`, `Workspace::preservedSections`, `src/workspace_sections.cpp` (vuoto nella v1.15.0) | `workspace.hpp/.cpp` | entrambe |
| F3 | `typed_files.hpp/.cpp` (tabella dei tipi, intestazione e CRC, UTF-8, `TypedStore`, ricerca), `FileDialog` unico (`app_widgets.hpp`, `widgets.cpp`), MEMORY › F3 FILES (`memory_app.cpp`), [FILE_FORMATS.md](../FILE_FORMATS.md) | | entrambe (tipi della Plus solo sulla Plus) |
| F4 | `mode_shell.hpp`, `src/apps/mode_shell.cpp`: `ScreenStack`, `FKeyBar`, `SelectList`, `ConfirmBox`, `AskBox`, `SetupItem`, `varsItems`, `shellCatalog`, `ModeShell`; **TABLE** portata sul guscio (`table_app.cpp`), `PopupMenu` spostato in `app_widgets.hpp` | | entrambe |
| F5 | `text_editor.hpp/.cpp` (righe UTF-8, a capo, ricerca, salto, selezione, clipboard `Context::textClipboard`, limiti `limits::kTextEditorBytes`/`kTextLineBytes`, garanzia forte) | | entrambe |
| F6 | `step_task.hpp/.cpp` (`Budget`, `Task`, `Runner`); **3D Graph** sul runner (`graph3d_plot.cpp`: `Graph3DApp` è un `step::Task`, 600 punti per passo come prima) | | entrambe |
| F7 | `graph2d.hpp/.cpp`: `Layout` (Full, G+G, GtoT, T+G), `PointSource` (funzione, parametrica, punti), `Plotter` (un `step::Task`), `drawAxes`, `TraceCursor`, zoom | | Plus |
| F8 | `binding.hpp/.cpp` (`BindingResolver`, celle e intervalli, `cellReferences`, `DependencyGraph` con «Circular ERROR»); nel lexer unico: `ParseOptions` con `cellReferences` e `boundNames` (`expression.hpp/.cpp`), nomi legati `rec_an1`…`rec_cn2` | | Plus (le opzioni del lexer sono nel motore comune ma usate solo dai servizi della Plus) |
| F9 | `program.hpp`, `src/program_internal.hpp`, `src/program_lang.cpp` (lessico e compilatore C/C++/C# in bytecode), `src/program_vm.cpp` (macchina iterativa, funzioni predefinite, `WorkspaceHost` con `graphcmd::execute`, `fx::calc` e A–Z) | | Plus |

## 4. Tabella dei requisiti (finale)

Tutte le voci della tabella di [ANALISI_FONTI_v1.15.0.md §7](ANALISI_FONTI_v1.15.0.md#7-requisiti-delle-fondamenta-tabella-di-parità-di-m1)
sono presenti, tranne F2.5 spostata alla M2.

| # | Requisito | Stato | Test |
|---|---|---|---|
| F1.1 | opzioni per scheda e definizioni | presente | `architecture_test` (opzioni dichiarate, servizi compilati esattamente sulle loro schede) |
| F1.2 | `boards` e testo per il Pico 2 W | presente | `architecture_test` (modalità fuori scheda: Planned, testo, nessun sorgente/collegamento) |
| F1.3 | registro ⇔ sorgenti compilati | presente | `architecture_test` con `FXAI_COMPILED_SOURCES` passato da CMake |
| F1.4 | nessun oggetto/simbolo escluso nell'immagine del Pico 2 W | presente | build del firmware: «18 excluded objects/symbols, none in the image» |
| F1.5 | funzioni della v1.14.0 identiche | presente | `architecture_test` (insieme Available = v1.14.0), `sim_compare`: 94 screenshot identici |
| F1.6 | controlli di dimensione bloccanti | presente | `check_firmware_size.cmake` (errore oltre partizione e margine; HW-200/HW-201 sul PC) |
| F2.1 | record v11 | presente | `workspace_v115_test` |
| F2.2 | sezioni sconosciute e più nuove conservate | presente | `workspace_v115_test` (anche la sezione nota più nuova che vince sulla codifica) |
| F2.3 | sezioni corrotte, troncate, duplicate, fuori ordine | presente | `workspace_v115_test`, `fuzz_workspace` |
| F2.4 | lettura v1–v10 e fixture della v1.14.0 | presente | `workspace_v115_test` (8 fixture), test esistenti |
| F2.5 | codifica per parti | **spostata alla M2** | — |
| F3.1–F3.6 | tipi, intestazione, API, microSD/LittleFS con interruzioni, dialogo, MEMORY | presente | `typed_files_test`, `fuzz_typedfile`, screenshot 105–107 |
| F4.1–F4.4 | guscio comune | presente | `mode_shell_test` |
| F4.5 | TABLE sul guscio, screenshot identici | presente | `table_test`, `mode_shell_test`, screenshot 06, 07, 20–37 identici |
| F5.1–F5.3 | editor di testo | presente | `text_editor_test`, `fuzz_texteditor` |
| F6.1–F6.3 | StepTask e 3D Graph sul runner | presente | `step_task_test`, screenshot 40–43 e 88–104 identici |
| F7.1–F7.4 | grafico 2D | presente | `graph2d_test` (incrementale = completo al pixel) |
| F8.1–F8.3 | variabili legate, celle, Circular ERROR | presente | `binding_test`, `fuzz_expr` (celle e nomi legati) |
| F9.1–F9.6 | interprete C/C++/C# | presente | `program_core_test`, `fuzz_program` |

## 5. Test, fuzzing, simulatore

| Verifica | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| CTest (`tests/run_host_tests.sh`) | **55/55**: 53 suite C++ con **14 304** controlli, `mem_pools_smoke`, `sd_interop_test` | **52/52**: 50 suite con **14 209** controlli |
| Suite nuove | `workspace_v115_test` 114, `typed_files_test` 434, `mode_shell_test` 53, `text_editor_test` 72, `step_task_test` 19, `graph2d_test` 26, `binding_test` 32, `program_core_test` 47; `architecture_test` 1 774 | `workspace_v115_test` 114, `typed_files_test` 430, `mode_shell_test` 53, `text_editor_test` 72, `step_task_test` 19; `architecture_test` 1 794 |
| ASan + UBSan (bloccante), TSan | 55/55, TSan superato | 52/52, TSan superato |
| Simulatore (`tools/sim_compare.py`) | 94 screenshot identici pixel per pixel + 3 nuovi (105–107, MEMORY › FILES) | — |
| Relay AI | 15/15 | — |

Errori trovati dai test nuovi durante lo sviluppo e corretti prima della
pubblicazione: `cout << variabile` non caricava il valore (compilatore F9);
la macchina in attesa di input non ripartiva dopo l'arrivo dell'input
(`runToEnd`); un intervallo di celle era trattato come matrice (lexer F8);
TABLE: l'enum delle azioni della barra copriva il colore `Select` del tema
(selezione nera invece che blu, trovato dal confronto degli screenshot);
`varsItems` si fermava sulle variabili statistiche senza dati.

## 6. Firmware e memoria

| Voce | Pico Plus 2 W | Pico 2 W |
|---|---|---|
| Immagine | 1 978 840 B = 47 % della partizione (+30 816 B) | **1 969 808 B** = 93 % (+30 628 B): restano **94 576 B** sotto il budget di 2 064 384 B per la v1.16.0 |
| RAM statica / heap SRAM | 309 064 B (+40) / 182 456 B | 305 140 B (+40) / 186 380 B |
| Budget | < 90 % della partizione da 4 MB (3 774 873 B) | ≤ 2 064 384 B (margine di 32 KiB) |
| Codice nuovo collegato | registro risolto, record v11, file tipizzati e MEMORY, guscio (TABLE), runner (3D) | gli stessi; `graph2d`, `binding`, `program_*` non sono compilati |
| Servizi solo Plus | compilati ma non ancora collegati nell'immagine: nessuna modalità li usa (il linker li scarta); entreranno con Recursion, Spreadsheet, Program… | assenti |

Stack (`-fstack-usage`): analizzatore dei programmi a discesa ricorsiva con al
più `MaxNesting` = 48 livelli contati (frame massimi `Compiler::postfix` 304 B,
`statement` 256 B, `assignment` 152 B, `binary` 120 B, `unary` 112 B, più
`Depth` 88 B): nel caso peggiore ≈ 19 KB, oltre «Stack ERROR»; la macchina
non è ricorsiva (frame massimo `Machine::Impl::printf` 800 B,
`Machine::step` 448 B). Editor di testo, runner, file tipizzati: nessuna
ricorsione.

## 7. Differenze e limiti

| Voce | Descrizione |
|---|---|
| Interprete (F9) | sottoinsiemi: niente puntatori (tranne `char*` come stringa), strutture, classi (tranne il contenitore dei metodi static di C#), riferimenti, template, eccezioni; interi a 64 bit per tutti i tipi interi; `find` restituisce −1 invece di `npos`; macro `#define` solo senza argomenti; `#include` ignorato. Sono scelte del sottoinsieme, da completare nella tabella di parità di Program (M10). |
| Righe di testo con espressioni Math | l'editor di testo F5 è testo semplice; le espressioni in forma naturale nelle righe di testo di eActivity si progettano nella M2 |
| Cartelle dei file | `TypedStore` lavora nella cartella del tipo; le sottocartelle (Memory Manager, cap. 11) arrivano con eActivity (elenco) e con MEMORY completo (M13) |

## 8. Cancello

| Controllo | Esito |
|---|---|
| `tests/run_host_tests.sh` (due profili) | 55/55 e 52/52 |
| `--sanitize --tsan` | 55/55 e 52/52, TSan superato su entrambi |
| Fuzzing ≥ 300 s per harness nuovo o toccato | `fuzz_workspace` 2 284 459 esecuzioni, `fuzz_expr` 683 978, `fuzz_ui` 11 912, `fuzz_typedfile` 62 249 374, `fuzz_texteditor` 2 555 841, `fuzz_program` 2 532 316: nessun crash, nessun artefatto |
| Controlli del build (HW-200/HW-201, sul PC) | margine superato → errore; oggetto escluso nella mappa → errore; servizio della Plus acceso sul Pico 2 W → errore di configurazione |
| Simulatore | 94 identici, 3 nuovi |
| Firmware | due schede senza warning, nei budget (dettagli in [BUILD_STATUS.md](../BUILD_STATUS.md)) |
| Relay | 15/15 |
| Documentazione | FEATURE_MATRIX, APPLICATIONS_ARCHITECTURE, STORAGE, FILE_FORMATS, BUILD_STATUS, CHANGELOG, ROADMAP, TESTING, CONTINUE, README, releases/v1.15.0, VERSION |
