# Collaudo su PicoCalc fisica — v1.10.0

Questa procedura porta le novità della v1.10.0 da *TESTED ON HOST* /
*TESTED ON SIMULATOR* a *TESTED ON HARDWARE*. **Fino a quando i risultati non
arrivano dal dispositivo nessuna voce è dichiarata provata su hardware.**
Ogni prova ha un codice: riportare l'esito (OK / KO / non eseguita), le
righe di log richieste e, dove indicato, una foto.

La v1.10.0 contiene tutte le funzioni della v1.9.0: le prove **HW-01…HW-36**
([HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)) e **HW-40…HW-82**
([HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md)) restano valide e vanno
eseguite con il firmware 1.10.0. Qui sotto le prove aggiuntive
(**HW-90…HW-99**), da eseguire sul **Pimoroni Pico Plus 2 W** e, se
disponibile, sul **Raspberry Pi Pico 2 W** (i limiti delle matrici sono
diversi: lato 999 / 40, 10 000 / 400 elementi per matrice, 20 000 / 1 200 in
tutto).

## 0. Preparazione

1. UF2 della release `v1.10.0` (`picocalc_fxai_pico_plus_2w-v1.10.0.uf2` o
   `picocalc_fxai_pico2w-v1.10.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Prima dell'aggiornamento annotare i dati salvati dalla versione precedente
   (variabili A–Z, una riga della cronologia, List 1): servono per HW-90.
3. Log USB seriale dall'accensione, come per la v1.9.0.

## 1. Prove

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-90 | Aggiornare da una v1.9.0 (o v1.8.0) con dati salvati e riaccendere | variabili, cronologia, liste e grafici presenti; SYSTEM › INFO mostra 1.10.0; barra di stato con `Math` | foto |
| HW-91 | Run-Matrix: `SHIFT`(F7) poi `3` | si apre l'editor MAT/VCT con `Mat A … Mat Z, Mat Ans` e `None` | foto |
| HW-92 | F3 DIM, 2 EXE 3 EXE, poi 1 EXE 2 EXE … 6 EXE; EXIT, EXIT | le celle si riempiono verso destra e poi nella riga sotto; nel calcolo `Trn(Mat A)` dà la matrice 3×2 disegnata a griglia | foto |
| HW-93 | `[[1,2][3,4]]^-1` con il modello MATH › MAT/VCT › 2×2 e con il testo digitato; poi `Det(Mat A)` con una matrice 3×3 | `[[-2,1][3/2,-1/2]]`; Det corretto; misurare il tempo di risposta di `Identity(20)^-1` (Pico 2 W) e `Identity(100)^-1` (Plus) | foto + tempo |
| HW-94 | `{1,2,3}→List 1`, poi STATISTICS | la lista 1 contiene 1, 2, 3; in Run-Matrix `Mean(List 1)` = 2 | foto |
| HW-95 | Digitare `((1+2)(3-4)+(5` | parentesi blu, rosso … rosso, blu, rosso leggibili sul pannello (anche con la retroilluminazione bassa); dopo EXE nella cronologia sono nere | foto |
| HW-96 | `SHIFT 8`, ← ← ←, F1 COPY; `SHIFT 9` | zona evidenziata visibile; incolla nel punto del cursore; la barra di stato mostra `CLIP` durante la selezione | foto |
| HW-97 | `14÷0×2.3` EXE, EXIT, ←, `1`, EXE | finestra "Math ERROR"; dopo EXIT `14÷0` evidenziato e cursore dopo lo 0; risultato 3.22 | foto |
| HW-98 | SYSTEM (SET UP): Input/Output → Line, EXIT; `1/3+1/6`, `[[1,2][3,4]]^-1`; spegnere e riaccendere | input e risultati su una riga; dopo la riaccensione la modalità resta Line; tornare a Math | foto |
| HW-99 | Riempire le memorie: `Identity(n)→Mat A`, `→Mat B`… fino al messaggio "Memory ERROR: memoria di matrici e vettori piena"; attendere l'autosalvataggio; spegnere e riaccendere | nessun blocco; dopo la riaccensione le matrici ci sono; MEMORY › "Matrici e vettori" mostra il conteggio e cancella tutto | log + foto |

## 2. Rapporto da inviare

```
Mainboard: v2.0   Modulo: Pimoroni Pico Plus 2 W / Raspberry Pi Pico 2 W   Firmware (SYSTEM > INFO): 1.10.0
HW-01..HW-36, HW-40..HW-82: esiti...
HW-90: OK/KO  note...
...
Tempi HW-93: ...
Log USB seriale: allegato
Foto: elenco
```

Ogni KO viene analizzato e corretto con un test che lo riproduca; le voci OK
passano a TESTED ON HARDWARE in [FEATURE_MATRIX.md](FEATURE_MATRIX.md) con data,
modulo e revisione della scheda.
