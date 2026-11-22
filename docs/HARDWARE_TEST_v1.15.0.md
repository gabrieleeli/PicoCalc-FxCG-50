# Collaudo su PicoCalc fisica — v1.15.0 (fondamenta comuni delle modalità)

Questa procedura verifica sul dispositivo le fondamenta della v1.15.0
([reports/SVILUPPO_v1.15.0.md](reports/SVILUPPO_v1.15.0.md)): record del
workspace v11, file tipizzati e MEMORY › FILES, TABLE sul guscio comune,
calcolo progressivo del 3D Graph sul servizio StepTask, controlli di
dimensione del firmware. **Nessuna voce è stata provata su hardware**: fino a
quando i risultati non arrivano dal dispositivo nessuna voce è dichiarata
provata. Ogni prova ha un codice: riportare l'esito (OK / KO / non eseguita),
le righe di log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-189** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.14.0](HARDWARE_TEST_v1.14.0.md))
restano valide e vanno eseguite con il firmware 1.15.0. Prove aggiuntive:
**HW-190…HW-201**, da eseguire sul **Pimoroni Pico Plus 2 W** e sul
**Raspberry Pi Pico 2 W** dove indicato.

## 0. Preparazione

1. UF2 della release `v1.15.0` (`picocalc_fxai_pico_plus_2w-v1.15.0.uf2`,
   `picocalc_fxai_pico2w-v1.15.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo per scheda con la **v1.14.0** installata e dati in tutte le
   modalità (cronologia, liste con sottonomi, Y1–Y20, tabella generata,
   EQUATION, 3D Graph, matrici) e file GRAPH sulla microSD e nella flash interna.
4. microSD FAT32 e, sul PC, una cartella `PICOCALC/EACT` con un file
   `.pceact` copiato da un'altra PicoCalc 1.15.0 (HW-196).

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-190 | Aggiornamento dalla v1.14.0 alla v1.15.0 (UF2), spegnimento e riaccensione, su **entrambe** le schede | tutti i dati del workspace restano (record v10 letto, riscritto in v11 al primo salvataggio automatico); i file GRAPH sulla microSD e nella LittleFS restano; nessun messaggio d'errore | foto di Run-Matrix, Statistics, Graph, Table, Equation, 3D; log `[flash]` |
| HW-191 | Dopo HW-190: riscrivere la v1.14.0 (ritorno a un firmware precedente) | il record v11 non è leggibile dalla v1.14.0 (problema aperto A1 per il passato): la v1.14.0 riparte vuota ma **non** cancella i dati finché non salva; reinstallando la v1.15.0 i dati tornano solo se la v1.14.0 non ha salvato | esito, log `[flash]` |
| HW-192 | Mappa della flash invariata sul Pico 2 W: SYSTEM → INFO / DIAG | partizione firmware 2 MB, LittleFS e record negli stessi indirizzi della v1.14.0; immagine ≤ 2 064 384 B | foto di INFO, righe `[DIAG]` |
| HW-193 | MEMORY → F3 FILES su entrambe le schede, archivio microSD e poi flash interna (SYSTEM → «Archivio file») | Pico 2 W: solo eActivity; Plus: eActivity, Program, Spreadsheet, Python, Geometry, Picture Plot, ognuno con 0 file | foto per scheda |
| HW-194 | Microsd estratta con MEMORY → FILES aperto, poi F3 | messaggio dell'archivio («No SD card»), nessun blocco; reinserita, l'elenco torna | foto |
| HW-195 | Copiare sul PC un file `PICOCALC/EACT/PROVA.pceact` valido (da `typed_files_test`, intestazione PCEA) e uno con un byte cambiato; MEMORY → FILES → eActivity | entrambi elencati con la dimensione; DELETE con conferma Sì/No elimina il file scelto | foto, contenuto della cartella sul PC dopo |
| HW-196 | Interruzione di alimentazione (batteria tolta) durante l'eliminazione di HW-195 e al riavvio MEMORY → FILES | il file c'è o non c'è, nessun `.tmp`/`.bak` rimasto dopo l'elenco, volume non corrotto (`fsck.fat -n` sul PC) | output di `fsck.fat -n` |
| HW-197 | TABLE: tutte le prove HW-2x della v1.8.0 (lista, SET, SET UP, tabella, DELETE con conferma, ROW, EDIT, LISTMEM, GPH-CON/GPH-PLT) | comportamento e schermate identici alla v1.14.0 (Table è passata sul guscio comune) | foto di SET, SET UP, conferma, ROW |
| HW-198 | 3D Graph: tre grafici Z= con griglia 50 (Plus) e 25 (Pico 2 W), DRAW, poi AC durante il calcolo | calcolo progressivo con la percentuale come nella v1.14.0; AC interrompe subito, grafico parziale, tastiera reattiva | tempi di calcolo (confronto con HW-173) |
| HW-199 | SYSTEM → DIAG su entrambe le schede dopo un'ora di uso | heap libero e PSRAM come nella v1.14.0 (± 2 KB per le tabelle nuove), nessun `[fault]` | righe `[DIAG]`, `[mem]` |
| HW-200 | Collaudo della build: `scripts/build.sh --clean --board pico2w` con un sorgente di una modalità della Plus aggiunto apposta a `FXAI_CORE_SOURCES` | il build **fallisce** con «excluded mode/service found in the image» (controllo della mappa del linker) | output del build |
| HW-201 | Collaudo della build: `-DFXAI_FIRMWARE_MARGIN_BYTES=200000` sul Pico 2 W | il build **fallisce** con «leaves less than … free» | output del build |

HW-200 e HW-201 si eseguono sul PC: verificano che i controlli dell'immagine
siano errori e non avvisi. **Eseguite sul PC durante lo sviluppo della
v1.15.0: OK** (output in [reports/SVILUPPO_v1.15.0.md](reports/SVILUPPO_v1.15.0.md) §8);
non sono prove su hardware.
