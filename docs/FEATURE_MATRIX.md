# Feature matrix — v1.6.0

Stati: **NOT STARTED** · **IN PROGRESS** · **IMPLEMENTED** (codice presente, senza
test automatici dedicati) · **TESTED ON HOST** (test automatici su PC superati) ·
**TESTED ON HARDWARE** · **COMPLETE**.

Nessuna voce è ancora TESTED ON HARDWARE: la PicoCalc fisica non è stata usata.

## Piattaforma e hardware

| Funzione | Stato | Note |
|---|---|---|
| Build UF2 RP2350 (Pico SDK 2.2.0) | TESTED ON HOST | build eseguita, `picotool info` verificato |
| Driver LCD ST7365P (SPI1, init ufficiale, RGB666) | IMPLEMENTED | da collaudare; opzione RGB565 |
| Aggiornamento parziale LCD (tile 16×16) | TESTED ON HOST | `TileDiff`, test in app_test |
| DMA per il flush LCD | NOT STARTED | non conveniente: il collo di bottiglia è il bus SPI (vedi HARDWARE.md) |
| Tastiera STM32 I2C (FIFO, stati, modificatori) | IMPLEMENTED | decoder TESTED ON HOST; trasporto I2C da collaudare |
| Pressione prolungata / ripetizione | TESTED ON HOST | ripetizione generata dallo STM32, long-press nel decoder |
| Batteria e retroilluminazione | IMPLEMENTED | registri 0x05/0x0A/0x0B |
| Storage flash A/B con CRC | TESTED ON HOST | test di interruzione di alimentazione; driver Pico da collaudare |
| Scheda SD | NOT STARTED | |
| Wi-Fi CYW43 | IMPLEMENTED | da collaudare |
| HTTPS (lwIP altcp_tls + mbedTLS, verifica obbligatoria) | IMPLEMENTED | parser HTTP TESTED ON HOST |
| Gestione energetica / sospensione | NOT STARTED | |

## Interfaccia

| Funzione | Stato |
|---|---|
| Menu principale a icone (23 modalità, accesso diretto 1–9/A–N) | TESTED ON HOST |
| Barra di stato (angolo, display, complessi, batteria, Wi-Fi, SHIFT) | TESTED ON HOST |
| Tasti funzione F1–F6 stile CG50, SHIFT one-shot | TESTED ON HOST |
| Finestre di errore stile CG50 (Math/Syntax/Non-Real ERROR) | TESTED ON HOST |
| Menu popup, form, liste di risultati, griglie di celle | TESTED ON HOST |
| Font Unifont 8×16 e Spleen 6×12 (greco, simboli matematici) | TESTED ON HOST |
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
| Selezione parziale con il cursore | NOT STARTED (copia/incolla dell'intera espressione) |
| Scorrimento orizzontale dell'input | TESTED ON HOST |
| Matrici/vettori come modelli 2D modificabili | NOT STARTED (solo rendering del nodo Matrix) |

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
| Variabili A–Z, Ans, PreAns, memorizzazione con → | TESTED ON HOST |
| Costanti matematiche e fisiche | TESTED ON HOST |
| Numeri complessi nel parser (a+bi, r∠θ, arg, conj, re, im) | TESTED ON HOST |
| Formati Norm1/Norm2/Fix/Sci/Eng (10 cifre) | TESTED ON HOST |
| Risultati in frazione (S⇔D) | TESTED ON HOST |
| Base-N (letterali 0x/0b/0o, visualizzazione Hex/Bin/Oct) | TESTED ON HOST |
| Operatori logici Base-N (and/or/xor/not) nel parser | NOT STARTED (API C++ presenti) |
| Sessagesimale (dms, °, visualizzazione DMS) | TESTED ON HOST |
| Pol / Rec | TESTED ON HOST |
| Integrali (Gauss-Kronrod adattivo), derivate (Ridders), d²/dx² | TESTED ON HOST |
| Σ, Π, SOLVE nel parser | TESTED ON HOST |
| Derivate/integrali simbolici, limiti | NOT STARTED |
| Matrici: somma, prodotto, det, inversa, trasposta, traccia, RREF, sistemi (API C++) | TESTED ON HOST |
| Matrici come valori nel parser e editor MAT in Run-Matrix | NOT STARTED |
| Vettori: somma, prodotto scalare/vettoriale, norma, angolo (API C++) | TESTED ON HOST |
| Conversioni metriche (10 grandezze, prefissi SI) | TESTED ON HOST |

## Modalità

| Modalità | Stato | Note |
|---|---|---|
| Run-Matrix | TESTED ON HOST | storia, OPTN, VARS, Base-N, DMS |
| Statistics | TESTED ON HOST | liste, 1/2-Var, 9 regressioni, grafici, DIST, test Z/t, intervalli |
| Regressioni sinusoidale e logistica | NOT STARTED | |
| Test χ², F, 2-campioni, ANOVA nell'interfaccia | NOT STARTED | ANOVA e t a 2 campioni esistono come API C++ |
| Grafico a torta / a barre | NOT STARTED | |
| Graph (Y=, r=, disequazioni) | TESTED ON HOST | |
| Trace, Zoom, V-Window, Sketch, G-Solve, ∫dx | TESTED ON HOST | |
| Grafici parametrici, impliciti | NOT STARTED | |
| Table | TESTED ON HOST | |
| Equation: sistemi 2–6, polinomi 2–6, solver | TESTED ON HOST | |
| Disequazioni polinomiali 1–6 | TESTED ON HOST | |
| Sistemi di disequazioni (interfaccia) | NOT STARTED | API `solveInequalitySystem` (grado ≤ 2) TESTED ON HOST |
| System / Set Up | TESTED ON HOST | |
| Memory | TESTED ON HOST | |
| Conversion | TESTED ON HOST | |
| AI Chat (Claude / ChatGPT) | TESTED ON HOST | con trasporto simulato; rete reale da collaudare |
| Symbols (guida) | TESTED ON HOST | |
| eActivity, Spreadsheet, Dyna Graph, Recursion, Conic Graphs | NOT STARTED | indicate come non disponibili nel menu |
| Program, Financial, E-CON4, Link, Geometry, Picture Plot, 3D Graph | NOT STARTED | idem |
| Python (MicroPython) | NOT STARTED | richiede l'integrazione del runtime come libreria |

## Persistenza

| Dato | Stato |
|---|---|
| Impostazioni, variabili, Ans | TESTED ON HOST |
| Storia Run-Matrix | TESTED ON HOST |
| Liste statistiche | TESTED ON HOST |
| Funzioni e V-Window di Graph, range di Table | TESTED ON HOST |
| Coefficienti Equation | TESTED ON HOST |
| Conversazioni AI (cancellabili da AI Chat e Memory) | TESTED ON HOST |
| Programmi, file Python, documenti, matrici salvate | NOT STARTED |

## Relay AI

| Funzione | Stato |
|---|---|
| Claude (Anthropic SDK, `claude-opus-5-5`, fallback server-side sui rifiuti) | TESTED ON HOST (API simulata) |
| ChatGPT (OpenAI SDK, modello configurato dall'operatore) | TESTED ON HOST (API simulata) |
| Token per dispositivo (hash SHA-256), limiti di dimensione e frequenza | TESTED ON HOST |
| TLS integrato con verifica del certificato | TESTED ON HOST |
| Prova con chiavi reali | NOT STARTED (richiede chiavi e credito dell'utente) |
