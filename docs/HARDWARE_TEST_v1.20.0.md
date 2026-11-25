# Collaudo su PicoCalc fisica — v1.20.0 (Program sulla microSD e librerie)

Questa procedura verifica sul dispositivo la v1.20.0
([reports/SVILUPPO_v1.20.0.md](reports/SVILUPPO_v1.20.0.md)): sulla **Pimoroni
Pico Plus 2 W** Program con i programmi C, C++ e C# come file sorgente sulla
microSD ([PROGRAM.md](PROGRAM.md)), il nucleo dell'interprete nuovo, il DEBUG
e il gestore delle librerie ([LIBRARIES.md](LIBRARIES.md)), con la sezione
`0x010A` versione 2 del record v12. Il **Raspberry Pi Pico 2 W è congelato**:
si controlla solo che sia rimasto quello della v1.16.0. **Nessuna voce è stata
provata su hardware**: fino a quando i risultati non arrivano dal dispositivo
nessuna voce è dichiarata provata. Riportare per ogni prova l'esito (OK / KO /
non eseguita), le righe di log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-295** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.19.0](HARDWARE_TEST_v1.19.0.md))
restano valide e vanno eseguite con il firmware 1.20.0. Prove aggiuntive:
**HW-296…HW-313**, sulla Plus salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.20.0` (`picocalc_fxai_pico_plus_2w-v1.20.0.uf2`,
   `picocalc_fxai_pico2w-v1.20.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo con la **v1.19.0** installata e due programmi della
   v1.17.0: un `.pcprog` in C++ nella flash (`PICOCALC/PROG`) e un `.c` sulla
   microSD.
4. Due microSD FAT32: una normale e una **quasi piena** (meno di 100 KB
   liberi); un adattatore con l'interruttore di protezione dalla scrittura.
5. Rete Wi-Fi con accesso a Internet configurata in SYSTEM (come per AI
   Chat); sul PC il repository con `docs/examples/program` e `libraries/`.
6. Un cronometro.

## 1. Prove

| # | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-296 | Aggiornamento dalla v1.19.0 alla v1.20.0 sulla Plus; Program | menu «Programmi della v1.17.0 trovati»; *Converti in PICOCALC/CODE*: i due programmi compaiono in `CODE/CPP` e `CODE/C`, la prima riga `// pcprog:` sparisce; si aprono ed eseguono; nessun avviso di sezione danneggiata | foto, file sul PC, log `[flash]` |
| HW-297 | **Pico 2 W**: aggiornamento alla v1.20.0 | stesse modalità e schermate della v1.19.0 (Program «non disponibile»), dimensione dell'immagine come nella tabella di BUILD_STATUS | foto del menu |
| HW-298 | Program **senza microSD**; inserimento della SD a Program aperto; SD tolta mentre l'elenco è aperto | «Inserire la scheda SD», poi l'elenco; nessun blocco né `[fault]` | foto, log |
| HW-299 | Gestore dei file: NEW file, NEW progetto, NEW cartella, REN, COPY, MOVE, DEL, DEL·A (conferma), SRC, SORT; lettera per saltare a un nome | operazioni corrette; sul PC i file hanno i nomi e il contenuto attesi e si compilano con gcc/g++/dotnet | foto, elenco dei file sul PC |
| HW-300 | Editor: scrittura veloce con la tastiera della PicoCalc, evidenziazione, rientro automatico, parentesi, FIND/REPL, UNDO/REDO, Ctrl+frecce, Ctrl+C/V, Ctrl+S, JUMP | nessun tasto perso, nessun ritardo percepibile | foto, tempo di 10 righe scritte |
| HW-301 | **File di 3 MB** (sul PC: 60 000 righe) aperto nell'editor: apertura, Pagina giù fino alla fine (cambi di blocco), FIND, modifica e SAVE | apertura e salvataggio in tempi accettabili; RAM stabile | tempi, log `[mem]` prima e dopo |
| HW-302 | **SD tolta durante SAVE** (file di 3 MB) e **durante un programma che scrive un file** | messaggio chiaro; reinserita la SD, il file vecchio è intatto (nessun file a metà, nessun `.tmp` rimasto dopo il salvataggio successivo); nessun `[fault]` | foto, file sul PC |
| HW-303 | SD **quasi piena** (SAVE di un file grande, NEW progetto) e SD **protetta** dall'interruttore | «SD piena» / «SD non scrivibile…»; nessun file perso | foto |
| HW-304 | Tempi dell'interprete: ciclo di 10⁶ iterazioni in C, `fib(25)` ricorsiva in C++, 20 000 oggetti in un ciclo in C# (GC), ricorsione profonda 1 500 chiamate; AC durante ognuno | tempi; AC = Break sempre; nessun `[fault]` (MSPLIM della coroutine) | tempi, log `[mem]`, `[fault]` |
| HW-305 | **LIB via Wi-Fi reale**: SET UP › Indice LIB predefinito; LIB › INST › `trasformazioni` | piano con `matrici` come dipendenza, conferma, avanzamento, «Installazione completata»; VERIF OK; `FigureLibreria.c` (esempi) dà l'uscita di `FigureLibreria.out` | foto, tempo dell'installazione |
| HW-306 | Interruzioni dell'installazione: AC a metà; router spento a metà; SD tolta a metà; batteria tolta a metà; poi LIB | dopo ognuna c'è la versione vecchia o quella nuova completa (VERIF OK), nessun file in `LIB/.tmp` dopo il rientro in LIB | foto, contenuto di `LIB` sul PC |
| HW-307 | Senza rete: INST con l'indice in `.cache`; LOCAL con `libraries/` copiata in `PICOCALC/LIB/IMPORT`; `tools/picocalc_link.py send-library --port … libraries/cpp/frazioni/1.0.0` poi LOCAL | installazioni riuscite con gli stessi controlli | foto, output di `picocalc_link.py` |
| HW-308 | UPD di una libreria all'ultima versione; versione più vecchia (indice di prova) con la conferma «ATTENZIONE»; DEL di `matrici` con `trasformazioni` installata | avviso sulle dipendenti; downgrade solo dopo la conferma | foto |
| HW-309 | DEBUG di `Trasformazioni.cpp`: BRK su due righe, STEP dentro `prodotto`, OUT, OVER, CONT, VARS con espansione di una `Matrice`, AC in pausa | posizione mostrata = riga che sta per essere eseguita; valori corretti; AC chiude il programma | foto |
| HW-310 | PASS: password su un file, EDIT con la password sbagliata e giusta, RUN senza domanda, password vuota per toglierla; REN del file protetto | «password errata»; la password segue il file | foto |
| HW-311 | Esempi di `docs/examples/program`: `Equazioni.cpp` (input, Ctrl+D; poi X e Y in Run-Matrix), `Trasformazioni.cpp` (F6 GRAPH), `StatisticheCSV.cs` (List 1 e 2 in Statistics), `Duello.cs` | uscite come nei `.out`; disegno e liste corretti | foto |
| HW-312 | C# con le collezioni: `List<int>` di 10 000 elementi ordinata, `Dictionary` di 2 000 parole, `StringBuilder` di 50 KB | tempi; nessuna «Memory ERROR» | tempi, log `[mem]` |
| HW-313 | Ritorno alla v1.19.0 dopo la v1.20.0 sulla Plus, poi di nuovo alla v1.20.0 | la v1.19.0 isola la sezione `0x010A` versione 2 («Data ERROR» di Program) senza perdere il resto; i programmi restano sulla SD in `PICOCALC/CODE`; tornando alla v1.20.0 tutto si rilegge | esito |
