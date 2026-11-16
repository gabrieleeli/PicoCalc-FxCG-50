# Collaudo su PicoCalc fisica — v1.12.0

Questa procedura porta le novità della v1.12.0 da *TESTED ON HOST* /
*TESTED ON SIMULATOR* a *TESTED ON HARDWARE*. **Fino a quando i risultati non
arrivano dal dispositivo nessuna voce è dichiarata provata su hardware.**
Ogni prova ha un codice: riportare l'esito (OK / KO / non eseguita), le
righe di log richieste e, dove indicato, una foto.

La v1.12.0 contiene tutte le funzioni della v1.11.0: le prove **HW-01…HW-36**
([v1.8.0](HARDWARE_TEST_v1.8.0.md)), **HW-40…HW-82** ([v1.9.0](HARDWARE_TEST_v1.9.0.md)),
**HW-90…HW-99** ([v1.10.0](HARDWARE_TEST_v1.10.0.md)) e **HW-100…HW-115**
([v1.11.0](HARDWARE_TEST_v1.11.0.md)) restano valide e vanno eseguite con il
firmware 1.12.0. Qui sotto le prove aggiuntive (**HW-120…HW-139**), da
eseguire sul **Pimoroni Pico Plus 2 W** e, se disponibile, sul **Raspberry
Pi Pico 2 W** (limiti diversi: liste 999 / 255 elementi, 20 000 / 1 530 in
tutti i List File; forme esatte delle matrici 4 000 / 400).

## 0. Preparazione

1. UF2 della release `v1.12.0` (`picocalc_fxai_pico_plus_2w-v1.12.0.uf2` o
   `picocalc_fxai_pico2w-v1.12.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Prima dell'aggiornamento annotare i dati salvati dalla v1.11.0 (variabili,
   una riga della cronologia con ▶Simp, List 3 con il sottonome, Mat A con
   forme esatte): servono per HW-120.
3. Una microSD FAT32 o exFAT (HW-133, HW-134) e un PC per leggere i file.
4. Log USB seriale dall'accensione, come per le versioni precedenti.

## 1. Prove

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-120 | Aggiornare dalla v1.11.0 senza cancellare la memoria | dati della v1.11.0 intatti (variabili, cronologia, List 3 e sottonome, Mat A esatta), nessun avviso di memoria azzerata; dopo un riavvio i dati restano (record v8) | log, foto della cronologia |
| HW-121 | OPTN: scorrere le 21 categorie | ordine LIST, MAT/VCT, COMPLEX, CALC, STAT, CONVERT, HYPERBL, PROB, NUMERIC, ANGLE, ESYM, PICTURE, FUNCMEM, LOGIC, CAPTURE, TVM, SKETCH, … | foto |
| HW-122 | OPTN › CALC › SolveN(x²−x−6); FMin(x²−4x+9,1,4) | `{−2,3}`; `{2,5}`; tempo di risposta | foto, tempi |
| HW-123 | Modelli Σ (variabile k, 1…100, k²) e d²/dx²(x³) in x=2 | 338350; ≈12; il cursore passa tra i quattro campi di Σ con ↑↓ | foto |
| HW-124 | OPTN › STAT › DIST: NormCD(−1,1), InvNormCD("C",0.95), BinomialPD(3,0.5) | 0.6826894921; {−1.959963985,1.959963985}; lista di 4 | foto |
| HW-125 | OPTN › TVM: Cmpd_FV(10,5,−100,0,1,1) | 162.8894627 | foto |
| HW-126 | OPTN › CONVERT: 10[cm]▶[in], 100[°C]▶[°F] | 3.937007874; 212 | foto |
| HW-127 | ESYM P, ENG, ENG← su 1234; 70! | `1.234×10³`, `0.001234×10⁶`, `1234000×10⁻³`; Math ERROR | foto |
| HW-128 | SET UP Simplify Manual: 2/4+1/4, 1/2×2/3, ▶Simp, ▶Simp | 3/4, 2/6, 1/3 F=2, 1/3 | foto |
| HW-129 | SET UP Mode Bin: 0−1; Hex: VARS › A (variabile sottolineata) e Alt+A | `1111111111111111`; la variabile A distinta dalla cifra A | foto |
| HW-130 | `3→A:A×2`, `5◢6+1`, `"ABC"→Str 1`, FUNCMEM STORE f1 con x+1 e poi f1(2) | 6; `5 ◢ 7`; ABC; 3 | foto |
| HW-131 | SET UP List File 2, dati in List 1, ritorno a File 1; `List "QTY"` | i file restano separati; il sottonome funziona nei calcoli | foto |
| HW-132 | SHIFT 4 (CATALOG): scorrere e inserire un comando | elenco alfabetico, inserimento corretto | foto |
| HW-133 | SHIFT 7 (CAPTURE) in Capture 1, poi `RclCapt 1` | file `PICOCALC/CAPT/CAPT01.pcpict`; l'immagine compare in GRAPH | file, foto |
| HW-134 | `StoPict 2`, `Cls`, `RclPict 2`, `BG-Pict 2` | la schermata salvata e richiamata | foto |
| HW-135 | `Graph Y>x²−2`, `G∫dx x,0,2`, `SketchThick F-Line 0,0,2,2` | regione tratteggiata, area colorata con ∫dx=2, linea spessa | foto |
| HW-136 | `PxlOn 10,10` poi `PxlTest(10,10)` e `PxlTest(200,300)` | 1 e 0; tempo di ogni PxlTest( | foto, tempi |
| HW-137 | Statistics con List 1/2 e regressione ax+b, poi in Run-Matrix ŷ(10), VARS › STAT, `DrawStat` | stima corretta, valori di VARS STAT, punti nel grafico | foto |
| HW-138 | `"2rec_an+1"→an+1` (da VARS › RECURSION), `1→a0`, `DrawR-Con`, `DrawWeb` | punti collegati 1,3,7,15…; ragnatela | foto |
| HW-139 | Matrice 10×10 con celle 1/3 e una cella 0.25 digitata, `Mat A×3` | forme esatte conservate, 0.25 resta decimale; memoria libera in DIAG | foto, log `[mem]` |

## 2. Esiti

Riportare per ogni codice: data, scheda (Pico Plus 2 W / Pico 2 W, revisione),
esito, osservazioni. Ogni KO va riprodotto con un test host prima della
correzione; solo le voci provate passano a TESTED ON HARDWARE in
[FEATURE_MATRIX.md](FEATURE_MATRIX.md).
