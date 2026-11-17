# Rapporto di sviluppo — v1.14.0 (CONVERSION completo)

Base: `v1.13.1` (commit `62b3f8f`). Obiettivo: trasformare la modalità
CONVERSION nel convertitore più completo possibile per una calcolatrice
scientifica/ingegneristica, con **una sola fonte dati** delle unità condivisa
con Run-Matrix, senza cambiare Run-Matrix. Elenco completo di categorie,
unità, simboli, fattori e fonti: [../CONVERSION.md](../CONVERSION.md) (generato
dalla tabella da `tests/conversion_v114_test.cpp`, che fallisce se il file non
coincide). **Nessuna prova su hardware fisico**: procedura
[../HARDWARE_TEST_v1.14.0.md](../HARDWARE_TEST_v1.14.0.md).

## 1. Numeri

| Voce | v1.13.1 | v1.14.0 |
|---|---|---|
| Voci di primo livello di CONVERSION | 10 grandezze | **97**: 77 grandezze numeriche + 19 conversioni di formato + valute, in 12 gruppi |
| Unità | 40 (CONVERSION, `point_a`) + 106 (Run-Matrix, `calc_library`), due tabelle | **771** in una tabella (`units.cpp`), di cui le 106 della fx-CG50 |
| Modello di conversione | lineare + temperature | lineare 722, affine 12, inverso 12, funzione 25 |
| Unità con fattore esatto (risultato esatto) | — | 636 (razionali e multipli razionali di π) |
| Prefissi | — | 24 SI (quecto q … quetta Q, anche ronto e ronna) + 8 IEC (Ki … Yi), per le unità con il flag |
| Rappresentazioni di formato | — | 73 in 19 convertitori |

Per gruppo:

| Gruppo | Categorie | Unità | Formati |
|---|---|---|---|
| Base SI | 8 | 193 | |
| Meccanica | 19 | 218 | |
| Elettricità | 13 | 85 | |
| Termica | 9 | 52 | |
| Luce e radiazioni | 8 | 43 | |
| Chimica | 4 | 32 | |
| Dati | 2 | 34 | |
| Logaritmiche | 8 | 41 | |
| Derivate | 5 | 65 | |
| Formati | | | 18 |
| Taglie | 1 | 8 | 1 (anelli) |
| Valute | | | 16 valute con tassi dell'utente |
| **Totale** | **77** | **771** | **19** |

**Nota sul numero delle unità fx-CG50.** La richiesta parlava di 103 unità
in Run-Matrix (e così dicevano README e RUN_MATRIX.md); la tabella
`calc_library::kUnits` della v1.13.1 ne contiene però **106**
(18+9+15+11+4+5+10+5+10+14+5). Sono state mantenute tutte le 106, identiche;
la documentazione è corretta.

## 2. Architettura

- **Una sola fonte dati** — `include/units.hpp`, `src/units.cpp`: tabella
  `const` in flash di `Unit {factor, name, label, exact, cat, flags, kind|fonte, aux}`
  (24 byte per unità su RP2350: 18 504 byte per 771 unità), 77 categorie con
  gruppo, unità di riferimento, nota e parametro, 32 prefissi, 10 fonti.
  Nessuna copia in RAM (le stringhe sono letterali in `.rodata`).
- **Run-Matrix identico.** `calc_library` è un involucro: `unitCategories()`
  dà gli 11 nomi del menu, `unitsOf(k)` le unità con il flag `FxCG50` nello
  stesso ordine (le nuove sono intercalate nella tabella ma non cambiano
  l'ordine relativo delle fx-CG50), `convertUnit` usa la stessa formula
  `(v + oa)·fa/fb − ob`. Il test confronta con la tabella della v1.13.1
  copiata nel test: nomi, categorie, ordine, fattori e offset uguali, e
  **8 666 conversioni** (ogni coppia di ogni categoria × 7 valori, anche −0) uguali bit per bit.
- **Sintassi `[unità]` estesa senza cambiare il passato.** Le unità nuove (e
  i prefissi: `[nPa]`, `[TiB]`) sono riconosciute solo se seguite da `▶[` o
  dopo `▶`; due nomi fx-CG50 restano limitati alle loro 11 categorie, così
  `1[eV]▶[K]` dà ancora Argument ERROR. Le unità con parametro (pixel, moli ↔
  massa) restano in CONVERSION.
- **`pointa::convert`** è un wrapper della stessa tabella (mappa dell'enum
  sui simboli); l'unica differenza numerica è il cavallo vapore, che ora usa
  il valore della fx-CG50 `745.69987158227` (prima `745.6998715822702`,
  differenza relativa 3E−15).
- **Modello generico** (`units::toSI/fromSI/convert`): lineare, affine,
  inverso (`L/100 km`, `min/km`, lunghezza d'onda, `[OH⁻]`) e 9 funzioni
  (`k·10^(v/10)`, `k·10^(v/20)`, `10^(k·v)`, `e^(k·v)`, `2^(k·v)`, AWG,
  sezione → diametro, magnitudo sismica, pOH) con l'inversa, domini
  controllati (Math ERROR per log di valori ≤ 0 e divisioni per zero,
  Argument ERROR fuori scala, risultati non finiti → Math ERROR).
- **Esatti**: per le unità lineari e affini con fattore esatto (testo
  `"1200/3937"`, `"pi/180"`, oppure il decimale scritto con ≤ 15 cifre) il
  risultato è calcolato con `exact.hpp` (razionali a 64 bit con controllo
  dell'overflow, multipli di π): `1/3 m → 100/3 cm`, `180 deg → π rad`,
  `100 °F → 340/9 °C`, `1 kg → 89875517873681764 J`.
- **Formati** — `include/unit_formats.hpp`, `src/unit_formats.cpp`: interfaccia
  testo → testo separata, con validazione (Syntax ERROR per testo malformato,
  Argument ERROR fuori intervallo). Riusa `toBase/fromBase` (Base-N), DMS,
  `cnvtEFF/cnvtAPR`, `daysPrd` (Days_Prd, usato quando le due date sono nel suo
  intervallo 1902–2097), `exact::parseDecimal` e `decimalExpansion`
  (periodi), `exactFraction` (S⇔D).
- **Dipendenze**: Conversion dichiara ora anche **Exact** (forma esatta del
  risultato); `units.hpp` e `unit_formats.hpp` sono gli header del servizio
  Units (al posto di `point_a.hpp`) e sono nell'elenco dei moduli del motore
  di `architecture_test` (nessun header di interfaccia). Documentato in
  [../APPLICATIONS_ARCHITECTURE.md](../APPLICATIONS_ARCHITECTURE.md).
- **Workspace v10**: in coda al record le preferenze di CONVERSION (16 tassi,
  DPI, massa molare, 8 conversioni recenti, 16 preferiti). I record v1–v9 si
  leggono senza perdite: fixture `tests/fixtures/workspace_v1_13_1.inc`
  scritto con i sorgenti della v1.13.1, riletto, salvato come v10 e riletto.
- **Glifi**: ø (°Rø), ħ, ‰, ⇄ (SWAP) aggiunti ai due font come glifi del
  progetto (`tools/gen_fonts.py`, MANUAL_GLYPHS).

## 3. Interfaccia

Elenco raggruppato (Preferiti, Recenti, poi i 12 gruppi) con ricerca: le
lettere scritte filtrano per nome della grandezza, simbolo o nome di
un'unità, nome di una rappresentazione o codice di valuta (`poll` → 13 voci).
Schermata di conversione: Valore (espressione con A–Z e Ans), Da, A, e il
parametro quando serve (DPI, massa molare, base, bit, capitalizzazioni).
◀▶ cambia l'unità, EXE o una lettera sul campo apre l'elenco filtrato (il
simbolo identico in cima: `eV` prima di `meV`). F1 CONVERT, F2 SWAP⇄, F3 ALL
(il valore in tutte le unità della categoria, errori per riga), F4 PREFIX
(TASSI per le valute), F5 COPY (negli appunti; Ctrl+V in ogni editor), F6
EXIT. Il risultato usa il formato del SET UP e va in Ans (anche in forma
esatta); sotto: forma esatta, costanti e formula della categoria, fonte dei
fattori, "APPROSSIMATO" per taglie e magnitudo; anteprima del colore.
Screenshot 88–96 in [../screenshots](../screenshots) (script
`tests/sim/conversion.txt`).

## 4. Costanti e fonti

SI 2019 esatte (c, h, e, k, N_A, g₀); NIST SP 811 (2008) App. B per le unità
anglosassoni (yard e libbra del 1959, galloni, Btu IT, caloria IT,
US survey foot); **CODATA 2018** per le costanti misurate (u, mₑ, mₚ, a₀,
Eₕ, R∞hc, unità di Planck), la stessa base della fx-CG50 per u; IAU 2012 B2
(au), IAU 2015 B2/B3 (parsec, valori nominali di Terra e Sole); ISO 2533
(mach a 15 °C); ISO 9920 (clo); IEA (tep). Ogni fattore non esatto ha la
fonte in un commento di `units.cpp` e nella colonna *Fonte* di CONVERSION.md.
Le 8 unità fx-CG50 con fattori troncati dalla calcolatrice (s-yr, t-yr,
slug, lbf/in², ft·lbf, hp, ft·lbf/s, Btu/min) restano con il valore della
fx-CG50 e sono marcate con la fonte "valore della fx-CG50"; cal15 e kcal15
hanno il valore arrotondato di NIST SP 811. Le nuove unità equivalenti (psi,
lbf·ft, CV…) hanno il valore esatto.

## 5. Test

| Test | Plus | Pico 2 W | Contenuto |
|---|---|---|---|
| `conversion_v114_test` **nuovo** | 5 632 | 5 632 | tabella (77 categorie, 771 unità, simboli unici per categoria, decimali esatti ≤ 15 cifre, testi esatti = fattori entro 4E−16, prefissi); Run-Matrix identico alla v1.13.1 (106 unità, ordine, 8 666 conversioni bit per bit, sintassi estesa); per **ogni** categoria un valore di riferimento NIST/BIPM/CODATA, il round trip A→B→A di ogni coppia di unità (oltre 40 000, entro 1E−12) e l'errore per categorie incompatibili; casi limite delle non lineari (0, negativi, inversi, domini); esatti; tutti i 19 formati con input non validi; record v10 e compatibilità v9; generazione di CONVERSION.md |
| `apps_flow_test` (esteso) | 93 | 93 | apertura di CONVERSION, ricerca e cambio di categoria, scelta delle unità per lettere, SWAP, ALL, PREFIX, valore con A e Ans, COPY, Syntax/Argument ERROR senza crash, romani, valute con TASSI, preferiti e recenti salvati |
| `architecture_test` | | | units e unit_formats tra i moduli del motore; Conversion con Exact |
| Test esistenti | | | aggiornati solo per la versione del record (9 → 10) e la coda v10 nel test della tabella |

Totale: **17 962** controlli sulla Plus e **17 956** sul Pico 2 W
(47 test CTest per profilo), tutti superati, anche con AddressSanitizer +
UBSan bloccante (`tests/run_host_tests.sh --sanitize`, entrambi i profili).

**Simulatore**: lo script `tests/sim/run_matrix_v112.txt` rigenera gli
screenshot 77–87 di Run-Matrix (compreso `81_convert.png`, il menu CONVERT)
**identici pixel per pixel** a quelli pubblicati con la v1.13.0;
`tests/sim/conversion.txt` produce i nuovi 88–96.

**Fuzzing** (`tools/fuzz/fuzz_conversion.cpp`, libFuzzer + ASan + UBSan):
convertitore, rappresentazioni, parametro e testo scelti dal fuzzer, più la
ricerca dei simboli con prefisso e `convertUnitNames`; 42 132 esecuzioni in
120 s dal corpus di 23 semi (cresciuto a 694 input): nessun crash, nessun
timeout.

## 6. Flash e RAM

Build Release con `scripts/build.sh --board <scheda>` (Pico SDK 2.2.0,
`gcc-arm-none-eabi` 13.2.1), nessun warning del compilatore (resta l'avviso
di CMake dell'SDK su picotool). `check_firmware_size.cmake` passa su entrambe
le schede senza avvisi.

| | Pimoroni Pico Plus 2 W | Raspberry Pi Pico 2 W |
|---|---|---|
| Immagine v1.13.1 | 1 848 768 B (44 % di 4 MB) | 1 839 940 B (87 % di 2 MB) |
| v1.14.0 tutto a `-O2` (misura intermedia) | — | 1 976 552 B (**94 %**: oltre la soglia del 90 %) |
| v1.14.0 con CONVERSION a `-Os` (misura intermedia) | — | 1 936 864 B (92 %) |
| **Immagine v1.14.0** | **1 787 788 B (42 %)**, −60 980 B | **1 780 056 B (84 %)**, −59 884 B |
| RAM statica (.data/.bss) | 309 608 B (+264 B) | 305 684 B (+264 B) |

Costo dei moduli nuovi (Pico 2 W, `-Os`, text + rodata degli oggetti):
`units.cpp` 49 660 B (di cui la tabella `kUnits` 18 504 B, categorie 1 540 B,
prefissi 768 B, il resto stringhe e codice), `unit_formats.cpp` 41 207 B,
`conversion_app.cpp` 19 547 B: circa **110 KB di flash**, **0 B di RAM per
le tabelle** (i +264 B statici sono lo stato dell'oggetto `ConversionApp`
nel firmware). In esecuzione CONVERSION alloca solo stringhe brevi e, per
ALL, al più 47 righe.

Per restare sotto il 90 % della partizione da 2 MB del Pico 2 W, i sorgenti
che non sono sul percorso critico del calcolo né del disegno dei grafici sono
ora compilati con `-Os` (elenco `FXAI_SIZE_OPTIMIZED_SOURCES` in
`CMakeLists.txt`: CONVERSION, serializzazione del workspace e dei file, CSV,
diagnostica, protocollo AI, schermate di menu, impostazioni, editor di dati,
Statistics, Table, Equation). Il valutatore, il motore dei grafici, il
disegno, le statistiche e i solutori restano a `-O2`. Effetto non misurato
sul dispositivo: prova HW-190.

## 7. Cosa non è stato implementato e perché

- **Aggiornamento dei tassi delle valute via Wi-Fi**: non fatto. Servirebbe un
  endpoint del relay con una fonte di tassi a licenza libera e la gestione
  degli errori di rete; la funzione resta indipendente dalla rete con i tassi
  inseriti dall'utente e salvati (16 codici fissi, nessun codice
  personalizzato). In [../ROADMAP.md](../ROADMAP.md).
- **Vinculum con la sopralineatura** e **periodo con la sopralineatura**: il
  font non ha la combinazione U+0305; si usano le notazioni equivalenti
  `(IV)CCXXXIV` (= 4234) e `0.1(6)`.
- **Taglie**: solo scarpe da adulto (EU, UK, US uomo/donna, Mondopoint, JP,
  piede in cm/in) con formule convenzionali e anelli EU/ISO, US, UK (A–Z con
  ½); niente taglie per bambini, anelli giapponesi o oltre Z (tabelle non
  lineari diverse per paese). Marcate "approssimato".
- **Unità agrarie italiane storiche**: 4 (pertica milanese, tornatura
  bolognese, moggio napoletano, giornata piemontese) con valori convenzionali;
  i valori variavano per città e periodo.
- **Risultato esatto** solo per unità lineari e affini con fattore esatto
  rappresentabile come razionale a 64 bit (×π): non per le unità inverse e
  non lineari, per i parametri (DPI, massa molare) e per fattori come eV o u
  (denominatori oltre 10¹⁸) — lì si mostra il decimale. In Run-Matrix
  CONVERT resta senza forma esatta, come nella v1.13.1.
- **Unità con parametro e valute in Run-Matrix**: escluse per scelta (il
  parametro e i tassi sono impostazioni di CONVERSION).
- **Velocità del suono**: un valore (aria secca a 20 °C, 343,21 m/s) e mach
  ISA a 15 °C; nessun calcolo in funzione di temperatura o gas.
- **Collaudo sul dispositivo**: non eseguito (procedura HW-170…HW-190).
