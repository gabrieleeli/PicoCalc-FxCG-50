# Report — v1.8.0: collaudo di GRAPH e microSD, completamento di TABLE

Branch `version/v1.8.0` (punto di partenza `fd6be96`), pubblicato anche su
`claude/loving-meitner-jifxhg`. Ambito: punti 1 e 2 della roadmap. Dual
Screen, Graph to Table, Dyna Graph, Recursion, Conic Graphs e Natural V.P.A.M.
↔ lineare **non** sono stati iniziati.

> **Nessuna prova è stata eseguita sulla PicoCalc fisica.** Tutto ciò che
> segue è verificato su PC (test host, sanitizer, simulatore, strumenti FAT
> indipendenti) e con la compilazione del firmware UF2. Il collaudo fisico è
> preparato in [HARDWARE_TEST_v1.8.0.md](../HARDWARE_TEST_v1.8.0.md).

## 1. Stato iniziale della v1.8.0 (audit)

Git: `version/v1.8.0` = `claude/loving-meitner-jifxhg` = `fd6be96`, nessuna
modifica locale, `main` e `version/v1.5.4…v1.7.0` invariati. Codice e
documentazione coerenti con la v1.8.0 (VERSION 1.8.0, 22 suite / 6540
controlli superati).

| Area | Stato reale trovato |
|---|---|
| GRAPH | motore e interfaccia completi secondo GRAPH_PARITY, ma **mai verificati al pixel** né con funzioni esponenziali/logaritmiche/razionali come riferimento; integrali su poli e salti non provati |
| microSD | protocollo SD e FatFs provati solo con una scheda simulata "ideale"; nessun guasto di comunicazione simulato, nessun confronto con un'altra implementazione FAT, nessuno strumento per il collaudo sul dispositivo |
| TABLE | solo funzioni Y= di GRAPH, range (max 500 righe), colonne delle derivate, GPH-CON/PLT; **mancavano** lista delle relazioni, TYPE (r=, Param), STYLE, Variable = List, EDIT, ROW, DELETE, LISTMEM |
| Architettura | GPH-CON/PLT ricalcolava la tabella in GRAPH con regole diverse da TABLE (anche le disequazioni, solo il range); TABLE dipendeva dalla lista di GRAPH; nessun servizio di diagnostica |
| Memoria | record del workspace in flash ~63,7 KB, superabile dallo stato GRAPH (SKETCH PEN fino a ~205 KB) → autosalvataggio bloccato per sempre; riempimento delle disequazioni su tutti i pixel a ogni ridisegno |
| Dipendenze | GraphApp/TableApp → GraphCore (graph_engine, graph_solve, ora table_model) → Expression; Files → FatFs → SdCard → SpiBus (PicoSdBus sul firmware); System → Diag → Files + GraphCore |

## 2. Problemi trovati in GRAPH (tutti riprodotti con un test prima della correzione)

1. **∫dx attraverso un polo** (es. ∫₀³ (x²−1)/(x−2) dx, ∫₀² 1/(x−1) dx):
   restituiva un numero finito senza senso invece di un errore.
2. **Integrale di funzioni a salto** (int x) e con singolarità integrabili:
   ∫₀³·⁵ int x dx = 4,500001 invece di 4,5 (errore visibile con 10 cifre).
3. **Stato GRAPH oltre la capacità del record in flash** (64 tratti PEN da
   400 punti ≈ 205 KB): l'autosalvataggio falliva per sempre con un messaggio
   generico, perdendo la persistenza di tutti i dati.
4. **Prestazioni**: il tratteggio delle disequazioni scorreva tutti i 90 240
   pixel per ogni disequazione a ogni ridisegno.
5. Frame di stack della diagnostica di ~4 KB (oggetti grandi sullo stack).

Verificato senza difetti: precisione di ogni punto disegnato (≤ 1 px, massimo
0,49 px) e dei segmenti per 18 casi e per X=, r=, Param; discontinuità
(tan, 1/x, razionali, int), domini (ln, √), più grafici, cambi di V-Window,
G-Solve, derivate in radianti/gradi/gradienti.

## 3. Problemi trovati nella microSD

1. **Nessun recupero dopo un errore di comunicazione**: se la scheda veniva
   estratta e reinserita tra due operazioni (card detect mai letto aperto) la
   scheda restava "pronta" e **ogni operazione successiva falliva** fino al
   riavvio.
2. **Scheda non formattata** segnalata come "file danneggiato o non valido".
3. Nessuno strumento per collaudare la scheda sul dispositivo e nessuna
   verifica dell'interoperabilità con il PC.

Verificato senza difetti: pin e polarità del card detect (sorgenti ufficiali
ClockworkPi), scrittura atomica e recupero, file da 0 a 300 000 byte ai bordi
di settore/cluster, cartelle annidate, 150 file in una cartella, nomi lunghi e
rinomina di sole maiuscole, scheda piena con il file precedente intatto.

## 4. Correzioni effettuate

| Problema | Correzione | Test |
|---|---|---|
| ∫dx su un polo | poli individuati sui massimi locali di \|f\| (Brent) con un test di crescita (≥ 1/\|t−x\|): "Math ERROR: integrale divergente (polo in x=…)" | graph_verify_test, graph_app_test |
| integrali con salti/picchi | intervallo diviso ai salti (bisezione) e ai picchi integrabili, Gauss–Kronrod per tratti: ∫ int x esatto | graph_verify_test |
| record flash | limite complessivo di 1200 punti PEN; caso peggiore GRAPH < 60 KB; record troppo grande → "Memoria flash piena: N KB > M KB (…)" | graph_app_test |
| tratteggio | ciclo solo sui pixel del motivo (4× meno, immagine identica) | screenshot confrontati byte per byte |
| microSD bloccata | `SdCard::fail` marca la scheda non pronta; `FatFsFileSystem::withVolume` reinizializza, rimonta e ripete una volta; `readAll` ripete una lettura | sd_fatfs_test (8 controlli falliscono senza la correzione) |
| scheda non formattata | nuovo stato `Unformatted` con istruzioni | sd_fatfs_test, graph_app_test |
| diagnostica | SYSTEM → F2 DIAG (microSD + tempi GRAPH, log `[DIAG]` su USB), oggetti grandi nell'heap | graph_app_test, simulatore |
| TABLE (vedi §7) | fuori indice dopo un intervallo non valido; SET che non si chiudeva con valori errati; righe che marcavano il progetto come modificato | table_test |

## 5. Stato finale di GRAPH

Tutte le funzioni della matrice ([GRAPH_PARITY.md](../GRAPH_PARITY.md), 135
voci: 29 TESTED ON SIMULATOR, 92 TESTED ON HOST, 3 IMPLEMENTED, 2 parziali,
6 NOT STARTED, 3 fuori ambito) eseguono realmente l'operazione e sono
verificate su PC con valori di riferimento. Parità completa con la fx-CG50
**non** raggiunta (Dual Screen, Graph to Table, comandi grafici dei programmi,
FORMAT nello SKETCH, suffissi angolari). Nessuna voce TESTED ON HARDWARE.

## 6. Stato del sistema microSD

Codice completo e verificato sul PC lungo tutta la pila (GraphApp → file store
→ FatFs → protocollo SD SPI → scheda simulata), con guasti iniettati, e con
strumenti indipendenti: volumi FAT16, FAT32 ed exFAT scritti dal codice del
firmware risultano puliti per `fsck.fat`/`fsck.exfat`, i file letti da
`mtools` coincidono byte per byte e i file scritti dal PC (nomi lunghi) sono
letti dal firmware. Il simulatore può usare l'immagine di una scheda reale
(`PICOCALC_SD_IMAGE`). **Il bus SPI0 reale non è stato provato.**

## 7. Funzionalità TABLE implementate

Dettaglio in [TABLE.md](../TABLE.md). Lista delle relazioni di TABLE
(SELECT, DELETE, TYPE Y=/r=/Param, STYLE, SET, TABLE, editor naturale),
Setup (Variable Range/List 1–6, Derivative, Angle), righe da range o lista
(≤ 999), colonne Y, r, Xt/Yt e derivate (errore con [a,b]/[A=] e Derivative
On), celle di 6 caratteri e valore completo, EDIT (anche digitando), ROW
DELETE/INSERT/ADD, DELETE, LISTMEM, GPH-CON/GPH-PLT dalle righe effettive,
persistenza (workspace v4 con le righe, `.pcgraph` con variabile e tipo),
lettura dei record v1–v3.

## 8. Funzionalità TABLE ancora mancanti

Dual Screen T+G e Graph to Table (v1.9.0); LISTMEM limitata a 255 valori
(liste del progetto); valore delle righe INSERT/ADD = copia (comportamento
CG50 non documentato); con [A=…] senza Derivative la colonna usa il primo
valore di A.

## 9. Test eseguiti e risultati

| Verifica | Risultato |
|---|---|
| `tests/run_host_tests.sh` | 25 test CTest superati: 24 suite C++ con 7169 controlli + `sd_interop_test` (41 controlli) |
| `tests/run_host_tests.sh --sanitize` (ASan + UBSan) | 25 superati |
| Nuove suite | `graph_verify_test` 182, `table_test` 171, `sd_interop_test` 41; ampliate `sd_fatfs_test` 84 → 302, `graph_app_test` 249 → 301 |
| Relay (`python -m unittest discover -s tests`) | 15 test superati (non interessato dalle modifiche, rieseguito per controllo) |
| Simulatore | screenshot 06, 07, 20–37 rigenerati e controllati; diagnostica su immagine FAT32 reale |

## 10. Build UF2

`scripts/build_pico2w.sh`: compilazione riuscita senza warning nei sorgenti
del progetto; `build/pico2w/picocalc_fxai.uf2` di 3 012 096 byte; `picotool
info`: "PicoCalc FX-CG50 AI", versione 1.8.0, RP2350, `pico2_w`, SDK 2.2.0;
immagine flash ≈1470 KB su 4096 KB (+67 KB rispetto a `fd6be96`).

## 11. RAM e prestazioni misurate

- RAM statica ≈ 286 KB (di cui 200 KB framebuffer), heap ≈ 194 KB, stack 32 KB.
- Frame di stack massimi dei moduli toccati (`-fstack-usage`, flag del
  firmware): diagnostica 808 / 672 B (prima 4040 / 3464 B), `writeAtomic`
  640 B, `graph::integrate` 616 B, `TableApp::drawList` 424 B, nessuna nuova
  ricorsione.
- Memoria di GRAPH: punti delle curve ≤ 96 KB, PEN ≤ 9,6 KB nel record; TABLE
  ≤ 8 KB di righe, celle calcolate solo per le righe visibili.
- Tempi sul Cortex-M33: **non misurati** (servono le righe `[DIAG]` del
  dispositivo: tempo di montaggio, velocità di lettura/scrittura, valutazioni
  al secondo).

## 12. Test che richiedono la PicoCalc fisica

Tutti quelli di [HARDWARE_TEST_v1.8.0.md](../HARDWARE_TEST_v1.8.0.md):
diagnostica (HW-01…06), resa e tempi di GRAPH (HW-10…17), microSD reale con
schede FAT32/exFAT, estrazione, interoperabilità con il PC (HW-20…28), TABLE
sul dispositivo (HW-30…36).

## 13. File modificati e nuovi

Nuovi: `include/table_model.hpp`, `src/table_model.cpp`,
`include/diagnostics.hpp`, `src/diagnostics.cpp`,
`src/platform/host/file_disk.{hpp,cpp}`, `tools/sd_image_tool.cpp`,
`tests/graph_verify_test.cpp`, `tests/table_test.cpp`, `tests/sim_sd_card.hpp`,
`tests/sd_interop_test.sh`, `docs/TABLE.md`, `docs/HARDWARE_TEST_v1.8.0.md`,
questo rapporto, screenshot 35–37.
Modificati: `src/apps/table_app.cpp` (riscritto), `src/apps/graph_plot.cpp`,
`src/apps/graph_files.cpp`, `src/apps/system_app.cpp`, `src/graph_solve.cpp`,
`src/graph_model.cpp`, `src/graph_file.cpp`, `src/workspace.cpp`,
`src/firmware.cpp`, `src/sd_card.cpp`, `src/fatfs_store.cpp`,
`src/file_store.cpp`, `src/app_registry.cpp`,
`src/platform/host/simulator.cpp`, header corrispondenti, test esistenti,
`tests/sim/graph.txt`, CMake e documentazione.

## 14–15. Commit e push

| Commit | Contenuto |
|---|---|
| `75fd63c` | verifica (1): test di riferimento di GRAPH e difetti trovati |
| `7cd5dad` | verifica (2): recupero e interoperabilità della microSD, diagnostica sul dispositivo |
| `43b1b8c` | completamento di TABLE |
| ultimo | documentazione, procedura di collaudo, frame della diagnostica |

Push normale su `version/v1.8.0` e `claude/loving-meitner-jifxhg`, verificato
con `git ls-remote`; nessun force-push, nessuna PR, nessun tag o release,
nessun branch v1.9.0.

## 16. Prossimi interventi consigliati

1. Eseguire [HARDWARE_TEST_v1.8.0.md](../HARDWARE_TEST_v1.8.0.md) e inviare log
   `[DIAG]`, foto ed esiti: ogni KO va corretto prima della v1.9.0.
2. Poi la v1.9.0 secondo [ROADMAP.md](../ROADMAP.md): Dual Screen e Graph to
   Table, Dyna Graph/Recursion/Conic Graphs sul servizio GraphCore, piccoli
   completamenti di GRAPH, Natural V.P.A.M. ↔ lineare.
