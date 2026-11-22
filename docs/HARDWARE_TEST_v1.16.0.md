# Collaudo su PicoCalc fisica — v1.16.0 (eActivity)

Questa procedura verifica sul dispositivo eActivity della v1.16.0
([reports/SVILUPPO_v1.16.0.md](reports/SVILUPPO_v1.16.0.md)) su **entrambe le
schede** e la dimensione finale dell'immagine del Raspberry Pi Pico 2 W, che
da questa versione è congelato. **Nessuna voce è stata provata su hardware**:
fino a quando i risultati non arrivano dal dispositivo nessuna voce è
dichiarata provata. Riportare per ogni prova l'esito (OK / KO / non eseguita),
le righe di log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-201** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.15.0](HARDWARE_TEST_v1.15.0.md))
restano valide e vanno eseguite con il firmware 1.16.0. Prove aggiuntive:
**HW-202…HW-219**, su **entrambe le schede** salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.16.0` (`picocalc_fxai_pico_plus_2w-v1.16.0.uf2`,
   `picocalc_fxai_pico2w-v1.16.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo per scheda con la **v1.15.0** installata e dati in Run-Matrix,
   Statistics (List 1–3), Graph (Y1–Y3), Equation, matrici (Mat A, Mat B).
4. microSD FAT32; sul PC, in `PICOCALC/PICT`, almeno un file `.pcpict` salvato
   da Graph (STO › PICT) e uno da CAPTURE (SHIFT 7) in `PICOCALC/CAPT`.

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-202 | Aggiornamento dalla v1.15.0 alla v1.16.0, spegnimento e riaccensione | tutti i dati restano; MENU › 3 apre eActivity («Nessun file eActivity») su **entrambe** le schede | foto del menu e di eActivity per scheda |
| HW-203 | NEW «FISICA», una riga di testo lunga con parole, EXE a capo, A⇔a, CHAR (≤, π, à), AC su una riga visiva | testo a capo sulle parole, AC cancella solo la riga visualizzata | foto |
| HW-204 | MATH › frazione nella riga di testo, ← → dentro e fuori dall'espressione | frazione in forma naturale nel testo, non va a capo | foto |
| HW-205 | Righe di calcolo: `120/8`, `1/3+1/6`, `√8`, OPTN › S⇔D | 15, 1/2, 2√2; S⇔D come in Run-Matrix | foto |
| HW-206 | 60 righe di calcolo `A+1→A` con una riga di stop a metà; modifica della prima e EXE; poi AC durante il ricalcolo | ricalcolo fino alla riga di stop con l'avanzamento; AC → «Break» subito, tastiera pronta | tempo del ricalcolo per scheda |
| HW-207 | COLOR › MARKER e › CHAR su una riga e su più righe; MEMO › INSERT, Catalog, VIEW, DETAIL, DELETE | colori e marcatori come impostati; icona del memo in alto a destra | foto |
| HW-208 | INSERT › PICTURE di un `.pcpict` di PICT e di uno di CAPT; SHIFT ↑ FORMAT (Thick, Red, Clear); DELETE | immagini alla larghezza dello schermo, bordo rosso se selezionate | foto; tempo di disegno della prima volta (lettura dalla microSD) |
| HW-209 | STRIP › Graph «Grafico», EXE, EXIT, Y1 = 3X, DRAW, SHIFT ' | barra di stato azzurra nella striscia; tornando, Y1 del menu principale (MENU › 5) è quello di prima | foto della barra azzurra e di Graph dal menu principale |
| HW-210 | Le altre strisce: RUN, Table Editor, Stat Graph, List Editor, Solver, Matrix/Vector Editor, Simul/Poly Equation, 3D Graph; SHIFT , (cambio di applicazione); FILE › SIZE | ognuna apre la schermata giusta con i dati propri e torna con SHIFT ' o MENU | elenco degli esiti |
| HW-211 | Notes: testo lungo con MATH, INSERT, DEL-LINE, JUMP | testo conservato nella striscia dopo SAVE e riapertura | foto |
| HW-212 | 'MAT/VCT e 'LIST nel documento; MENU › Run-Matrix › MAT; ritorno a eActivity | Mat/List del documento diverse da quelle del menu principale; tornano con il file | foto |
| HW-213 | SAVE, SAVE·AS su un nome esistente (No, poi Sì), OPT, CAPACITY, EXIT con «Salvare il file?» (AC, No, Sì) | come il manuale 10-22, 10-23; il file sul PC ha l'intestazione `PCEA` | `xxd -l 16 FISICA.pceact` |
| HW-214 | Batteria tolta (a) mentre una striscia Graph è aperta, (b) durante SAVE | (a) al riavvio Y1 del menu principale è quello di prima della striscia; (b) il file è quello vecchio o il nuovo, nessun `.tmp` dopo l'elenco | foto, `fsck.fat -n` |
| HW-215 | Pico 2 W: documento vicino a 24 KB (molte righe), poi altro testo | «Memory ERROR», il documento resta com'era; CAPACITY mostra il limite di 24 KB | foto |
| HW-216 | Plus: documento oltre 24 KB (fino a 256 KB) con strisce di liste lunghe | si apre e si salva; tempi di apertura e di salvataggio | tempi |
| HW-217 | Pico 2 W: SYSTEM → INFO e DIAG | immagine ≤ 2 064 384 B (vedi [BUILD_STATUS.md](BUILD_STATUS.md)); mappa della flash invariata; heap libero come nella v1.15.0 ± 4 KB | foto, righe `[DIAG]` |
| HW-218 | Cartelle create sul PC: `PICOCALC/EACT/SCUOLA/A/B/C` con file | si entra fino a B; C si vede ma «Sub-folders cannot be displayed» | foto |
| HW-219 | File `.pceact` copiato dal Pico 2 W alla Plus e viceversa (anche con una striscia Spreadsheet creata in futuro) | si apre sull'altra scheda; strisce non disponibili mostrate «non disponibile» e conservate | foto |
