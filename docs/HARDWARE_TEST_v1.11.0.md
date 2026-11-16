# Collaudo su PicoCalc fisica — v1.11.0

Questa procedura porta le novità della v1.11.0 da *TESTED ON HOST* /
*TESTED ON SIMULATOR* a *TESTED ON HARDWARE*. **Fino a quando i risultati non
arrivano dal dispositivo nessuna voce è dichiarata provata su hardware.**
Ogni prova ha un codice: riportare l'esito (OK / KO / non eseguita), le
righe di log richieste e, dove indicato, una foto.

La v1.11.0 contiene tutte le funzioni della v1.10.0: le prove **HW-01…HW-36**
([HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md)), **HW-40…HW-82**
([HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md)) e **HW-90…HW-99**
([HARDWARE_TEST_v1.10.0.md](HARDWARE_TEST_v1.10.0.md)) restano valide e vanno
eseguite con il firmware 1.11.0. Qui sotto le prove aggiuntive
(**HW-100…HW-115**), da eseguire sul **Pimoroni Pico Plus 2 W** e, se
disponibile, sul **Raspberry Pi Pico 2 W** (limiti diversi: liste 999 / 255
elementi e 20 000 / 1 530 in tutte le liste; matrici come nella v1.10.0).

## 0. Preparazione

1. UF2 della release `v1.11.0` (`picocalc_fxai_pico_plus_2w-v1.11.0.uf2` o
   `picocalc_fxai_pico2w-v1.11.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Prima dell'aggiornamento annotare i dati salvati dalla v1.10.0 (variabili,
   una riga della cronologia, una matrice, List 1): servono per HW-100.
3. Una microSD FAT32 o exFAT (per HW-104…HW-106) e un PC per leggere i file.
4. Log USB seriale dall'accensione, come per le versioni precedenti.

## 1. Prove

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-100 | Aggiornare da una v1.10.0 con dati salvati (matrici comprese) e riaccendere | variabili, cronologia, matrici, liste, grafici presenti; SYSTEM › INFO mostra 1.11.0; SET UP con le nuove voci Mode, Simplify, CSV Separator, CSV Decimal in fondo | foto |
| HW-101 | STATISTICS: → fino a List 26, digitare 3 valori; ↑ fino alla riga SUB, scrivere `QTY` EXE | List 26 con i valori e il sottonome `QTY` in blu; EXE scende alla prima cella; ◀ indica le colonne nascoste a sinistra | foto |
| HW-102 | Run-Matrix: `Seq(X,X,1,999,1)→List 20` (Plus) o `…255…` (Pico 2 W); poi riempire altre liste fino a "Memory ERROR: memoria delle liste piena" | nessun blocco; dopo la riaccensione le liste ci sono; MEMORY › "Liste 1-26 e sottonomi" mostra il conteggio e il massimo | log + foto |
| HW-103 | STATISTICS: CALC › SET, XList = List 26; CALC › 1-Var | titolo "1-Variable (List26)" e risultati di List 26 | foto |
| HW-104 | SYSTEM › Archivio file = microSD. STATISTICS: OPTN › CSV SAVE·AS, nome `PROVA`; leggere `PICOCALC/CSV/PROVA.csv` sul PC | file con una colonna per lista, separatore `,`, fine riga CRLF; nessun sottonome nel file | file |
| HW-105 | Sul PC: modificare il file (aggiungere una riga), rimetterlo sulla microSD; OPTN › CSV LOAD da List 3 | le colonne del file vanno in List 3, 4…; un file con un testo non numerico dà "Format ERROR" e non cambia nulla | foto |
| HW-106 | Estrarre la microSD durante un CSV SAVE·AS di molte liste; reinserirla; CSV LOAD dello stesso nome | file vecchio o nuovo, mai un misto; nessun `.tmp` / `.bak` dopo il LOAD | foto + log |
| HW-107 | SYSTEM › Archivio file = Flash interna; editor MAT/VCT: Mat A 3×3, F4 CSV › SAVE·AS; Mat B, F4 › LOAD `MATA`; spegnere durante un salvataggio successivo | Mat B uguale a Mat A; dopo la riaccensione il file è leggibile (vecchio o nuovo) | foto |
| HW-108 | Run-Matrix: `Identity(8)×2` EXE | si apre la schermata MatAns 8×8; ← → ↑ ↓ scorrono, ◀ ▶ e `r c` corretti; EXIT torna ai calcoli; ↑ sulla riga e F4 MatAns la riaprono | foto |
| HW-109 | Editor MAT/VCT: DIM 2×6; celle `1/3`, `2sqrt(3)`, `pi/4`, …, → fino alla colonna 6; poi `Mat A×6` | celle con le forme esatte (`1/3`, `2√3`, `π/4`); colonne scorrevoli; risultato esatto | foto |
| HW-110 | SET UP › Input/Output = Line. `1` F1 `3` `+1` F1 `6` EXE; `1234`, ←…, SHIFT DEL, `9` | `1⌟3+1⌟6` = `1⌟2`; `OVR` nella barra di stato, il 9 sostituisce l'1; il glifo ⌟ leggibile sul pannello | foto |
| HW-111 | Line: `Seq(X^3,X,1,18,1)` EXE; ↑ sulla riga, → → → | il risultato scorre, ◀ ▶ visibili | foto |
| HW-112 | Math: `1+(2+3)+4`, cursore prima di `(`, SHIFT DEL, F2 (√) | `1+√(2+3)+4`, cursore a forma di `[` prima di F2 | foto |
| HW-113 | SET UP › Display = Eng; OPTN › ESYM: `999k+25k`, `9÷10`; SET UP › Display = Norm1; `15÷60` OPTN › NUMERIC › ▶Simp EXE, poi ▶Simp EXE | `1.024M`, `900m`; `5/20  F=3`, `1/4  F=5`; il simbolo µ leggibile | foto |
| HW-114 | SET UP › Mode = Hex: `FF+1`, `0-1`, `FF00` Xnor `0FF0`, `Neg(7F)` con ▶Bin; `1.5` | `100`, `FFFFFFFF`, `FFFF0F0F`, 32 cifre binarie; `1.5` → Syntax ERROR; la barra di stato mostra `Hex`; tornare a Comp | foto |
| HW-115 | Run-Matrix: `ViewWindow -6.3,6.3,1,-3.1,3.1,1`, `Graph Y=x^2-2`, `Circle 0,0,2`, `F-Line -6,-3,6,3`, `Text 20,10,"PicoCalc"` (EXIT dopo ognuno) | ogni comando apre il grafico con il disegno; EXIT torna a Run-Matrix con `Done`; GRAPH aperto dal menu mostra gli stessi elementi; `Cls` li cancella; misurare il tempo tra EXE e il grafico completo | foto + tempo |

## 2. Esito

Per ogni prova: esito, modulo (Pico Plus 2 W / Pico 2 W), revisione della
scheda, data. Ogni KO va riprodotto con un test host e corretto prima di
dichiarare la voce *TESTED ON HARDWARE* in
[FEATURE_MATRIX.md](FEATURE_MATRIX.md).
