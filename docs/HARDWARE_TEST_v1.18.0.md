# Collaudo su PicoCalc fisica — v1.18.0 (Statistics alla parità, Distribution Base)

Questa procedura verifica sul dispositivo la v1.18.0
([reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md)): sulla **Pimoroni
Pico Plus 2 W** Statistics alla parità con il capitolo 6
([STATISTICS.md](STATISTICS.md)) e la modalità **Base** Distribution
([DISTRIBUTION.md](DISTRIBUTION.md)), con le sezioni `0x0101` e `0x0117` del
record v12. Il **Raspberry Pi Pico 2 W è congelato**: si controlla solo che sia
rimasto quello della v1.16.0. **Nessuna voce è stata provata su hardware**:
fino a quando i risultati non arrivano dal dispositivo nessuna voce è
dichiarata provata. Riportare per ogni prova l'esito (OK / KO / non eseguita),
le righe di log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-245** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.17.0](HARDWARE_TEST_v1.17.0.md))
restano valide e vanno eseguite con il firmware 1.18.0. Prove aggiuntive:
**HW-246…HW-271**, sulla Plus salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.18.0` (`picocalc_fxai_pico_plus_2w-v1.18.0.uf2`,
   `picocalc_fxai_pico2w-v1.18.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo per scheda con la **v1.17.0** installata e dati in Statistics
   (List 1–3, sottonomi, List File 2), una regressione calcolata e x̂/ŷ usati in Run-Matrix.
4. microSD FAT32; per il confronto numerico, se disponibile, una fx-CG50 reale
   con gli stessi dati (il confronto è facoltativo: i valori sono verificati
   sul PC contro scipy, `tools/stat_reference.py`).

## 1. Prove

| # | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-246 | Aggiornamento dalla v1.17.0 alla v1.18.0 sulla Plus; Statistics | liste, sottonomi, List File e SET 2Var conservati; nessun avviso di sezione danneggiata | foto, log `[flash]` |
| HW-247 | **Pico 2 W**: aggiornamento alla v1.18.0 | stesse modalità e schermate della v1.17.0 (23 icone, Statistics della v1.16.0), immagine di 1 944 228 B | foto del menu e di Statistics |
| HW-248 | Editor: tre pagine di F1–F6, SRT-A su tre liste (base al centro), SRT-D, DEL, DEL-ALL (conferma), INS, ADD, JUMP (TOP, BOTTOM, ROW) | liste riordinate insieme; conferme; cursore nei punti giusti | foto |
| HW-249 | Formula nella testa di List 3 (`List 1*2`), modifica di List 1, ciclo (`List 3+1` su List 1), modifica a mano di List 3 | ricalcolo immediato; Circular ERROR; la formula sparisce con la modifica a mano | foto |
| HW-250 | COLOR di una lista, FILE (List File 2 e ritorno), CSV SAVE·AS e LOAD su microSD | colori sul pannello leggibili; liste per file; CSV letto dal PC | foto, file CSV |
| HW-251 | GRAPH: SET (cambio di tipo, DefG), SELECT con tre grafici, GRAPH1/2/3 | voci del SET per tipo; tre grafici sovrapposti con colori distinti | foto |
| HW-252 | Scatter con frequenze e i quattro marcatori; xyLine; NPPlot | marcatori distinguibili sul pannello; punti con frequenza 0 assenti | foto |
| HW-253 | Histogram e Broken con Start/Width; MedBox con Outliers On/Off; N-Dist | classi e baffi corretti; outlier come punti | foto |
| HW-254 | Pie (Display %, Data, None; % Sto List), Bar (tre liste, Length e Horiz) | fette e barre corrette; percentuali nella lista | foto |
| HW-255 | Regressioni come tipo di grafico (X, Med, X², X³, X⁴, Log, Exp, Power, Sin, Logistic) | curva sopra i punti per ogni tipo | foto, tempo di Sin con 999 punti |
| HW-256 | Stat Wind Auto e Manual, V-WIN dal grafico, TRACE su Scatter, Histogram, MedBox | finestre giuste; coordinate in basso | foto |
| HW-257 | CALC 1-VAR e 2-VAR con le frequenze; Q1Q3 Type Std e OnData | valori come quelli del PC (`statistics_test`) | foto |
| HW-258 | REG: i 12 tipi; COPY in Y7 e grafico in Graph; DRAW; Resid List | coefficienti, r, r², MSe; la curva di Graph coincide con quella di Statistics (Sin anche in Deg) | foto |
| HW-259 | **SinReg e LogisticReg con 999 punti**: tempo e AC durante il calcolo | «Calcolo in corso» con la percentuale; AC = Break senza blocchi della tastiera | tempo, foto |
| HW-260 | TEST Z (1-S, 2-S, 1-P, 2-P) con List e Var, Save Res, DRAW | risultati e area evidenziata | foto |
| HW-261 | TEST t (1-S, 2-S con Pooled On/Off, REG), F | df di Welch con Pooled Off | foto |
| HW-262 | χ² GOF con CNTRB, χ² 2-way con Mat A osservata e Mat B attesa | contributi nella lista; Mat B in Run-Matrix | foto |
| HW-263 | ANOVA a una e due vie; grafico d'interazione; ANOVA con 999 dati e AC | tabella; linee per livello di B; Break | foto, tempo |
| HW-264 | INTR: Z e t (tutti i tipi) | Left, Right | foto |
| HW-265 | DIST: una funzione per famiglia con Var (Ans in Run-Matrix) e con List (ListAns, Save Res) | valori come sul PC; ListAns in Run-Matrix | foto |
| HW-266 | DIST: Invt con 999 aree in lista e AC; DRAW di Ncd, InvN Central, tcd, Ccd, Fcd | Break immediato; aree evidenziate | foto, tempo |
| HW-267 | VARS › STAT in Run-Matrix (X, Y, GRAPH, PTS, INPUT, TEST, INTR, DIST) dopo i calcoli e dopo un riavvio; x̂(/ŷ( con Logistic e X³ | valori dell'ultimo calcolo conservati; p̂ disegnato con il circonflesso | foto |
| HW-268 | SET UP di Statistics: tutte le voci; riavvio | impostazioni conservate (sezione `0x0101`) | foto |
| HW-269 | **Distribution**: 24ª icona (tasto O, triangolo Base), Normal C.D, DRAW, Bpd con List e Save Res, Invt con 999 aree e AC, riavvio | risultati come in Statistics › DIST; stato conservato (sezione `0x0117`) | foto |
| HW-270 | Batteria tolta durante un SinReg e durante un'ANOVA lunga; riaccensione | record v12 leggibile (slot A/B); nessuna sezione isolata | log `[flash]` |
| HW-271 | Ritorno alla v1.17.0 dopo la v1.18.0 sulla Plus | la v1.17.0 conserva le sezioni `0x0101` e `0x0117` senza leggerle (sezioni sconosciute riscritte intatte) e torna alla v1.18.0 con i dati | esito |
