# Report — Sessione 1 (v1.7.0): audit, architettura delle applicazioni, Run-Matrix

Data: 2026-11-08. Branch: `version/v1.7.0`.

## 1. Audit iniziale

| Voce | Esito |
|---|---|
| Branch remoti | `main` (537d591), `version/v1.5.4` (= main), `version/v1.6.0` (1e8bfca), `claude/peaceful-euler-auyrm5` (7dc65cd = v1.6.0 + 2 commit di documentazione), `version/v1.7.0` (= main, nessun lavoro) |
| Branch di partenza | `claude/peaceful-euler-auyrm5` @ 7dc65cd, il più aggiornato; `version/v1.7.0` avanzato in fast-forward (nessun force-push, `main` intatto) |
| Working tree | pulito, nessuna modifica non committata |
| Baseline | 14 suite host superate; UF2 compilato senza warning (toolchain e SDK installati nel container) |
| Archivio v1.6.0 dell'utente | non presente tra i file caricati: confronto fatto con il branch `version/v1.6.0` |

Problemi individuati:
1. **Frazioni approssimate**: S⇔D cercava una frazione vicina al valore in virgola
   mobile (frazioni continue, denominatore ≤ 9999); nessuna forma esatta in Ans e
   nelle variabili; niente radicali, π, numeri misti, periodo.
2. Run-Matrix senza la funzione History della CG50 (modifica e ricalcolo delle righe).
3. Motore senza operatori relazionali/logici, Int÷/Rmdr, MOD_Exp, RndFix; VARS con
   le sole variabili A–Z.
4. Arrotondamento Fix/Sci sul valore binario (0.125 → 0.12 in Fix2).
5. Architettura: tabella delle app dentro `app_manager.cpp`, nessuna dichiarazione
   di servizi/dati/collegamenti, nessun test della separazione; `SymbolsApp` nel
   file del launcher.
6. `VERSION` fermo a 1.6.0; nessuna prova su hardware.

## 2. Architettura

- Applicazioni analizzate: le 23 del menu (Run-Matrix, Statistics, eActivity,
  Spreadsheet, Graph, Dyna Graph, Table, Recursion, Conic Graphs, Equation,
  Program, Financial, E-CON4, Link, Memory, System, Geometry, Picture Plot,
  3D Graph, Conversion, Python, AI Chat, Symbols).
- Fondamenta: registro (`app_registry`), modelli dati delle modalità future
  (`app_models`), test di architettura; documento `APPLICATIONS_ARCHITECTURE.md`.
- Servizi condivisi: Expression, Exact, Editor, MathRender, Format, Solvers, Stats,
  MathCore, Widgets, Network, Storage, Units, Help, RunCore, Models.

## 3. Run-Matrix

Completato: risultati esatti (frazioni, radicali, π) con regole CG50; S⇔D con
valore preservato; Frac Result e frazioni miste; periodo (vista e input);
cronologia modificabile con ricalcolo; riuso di espressioni e risultati; →A~F;
VARS (A–Z, Ans, V-WIN, TABLE); OPTN (LOGIC, RELAZIONI, RISULTATO); relazioni,
logica, Int÷, Rmdr, MOD_Exp, RndFix, RanNorm#, RanBin#; "≈" per i metodi numerici;
arrotondamento come la calcolatrice; formato di salvataggio v2 compatibile con v1.

Mancante: matrici/vettori/liste nel parser, Base-N intera con operatori bit a
bit, simboli ingegneristici, Simp manuale, cursore sull'errore, CLIP,
impostazione Input/Output Math/Line.

## 4. Test e build

| Voce | Esito |
|---|---|
| Suite host | 17, 5347 controlli, 0 errori |
| ASan + UBSan | 17 suite, 0 errori |
| Relay AI | 15 test Python superati |
| Firmware | UF2 compilato (build pulita) senza warning nei sorgenti del progetto |
| Stack | nuovi frame ricorsivi 80–288 byte su ARM (`-fstack-usage`) |
| Simulatore | screenshot 12–19 |
| Hardware | non provato |

## 5. Prossima sessione (Punto 3)

Vedi `docs/ROADMAP.md` §2: impostazione Input/Output Math/Line, round-trip
garantito testo ↔ albero con test su corpus, cursore sul punto dell'errore, CLIP,
SHIFT+INS, colori delle parentesi, regola del × prima delle frazioni, modelli
MAT/VCT.
