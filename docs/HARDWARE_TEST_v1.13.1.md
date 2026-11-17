# Collaudo su PicoCalc fisica — v1.13.1 (versione di correzione)

Questa procedura verifica sul dispositivo le correzioni della v1.13.1
([reports/ERRORI_v1.13.1.md](reports/ERRORI_v1.13.1.md)). **Fino a quando i
risultati non arrivano dal dispositivo nessuna voce è dichiarata provata su
hardware.** Ogni prova ha un codice: riportare l'esito (OK / KO / non
eseguita), le righe di log richieste e, dove indicato, una foto o il tempo.

La v1.13.1 contiene tutte le funzioni della v1.13.0, senza funzioni nuove: le
prove **HW-01…HW-36** ([v1.8.0](HARDWARE_TEST_v1.8.0.md)), **HW-40…HW-82**
([v1.9.0](HARDWARE_TEST_v1.9.0.md)), **HW-90…HW-99** ([v1.10.0](HARDWARE_TEST_v1.10.0.md)),
**HW-100…HW-115** ([v1.11.0](HARDWARE_TEST_v1.11.0.md)), **HW-120…HW-134**
([v1.12.0](HARDWARE_TEST_v1.12.0.md)) e **HW-140…HW-159**
([v1.13.0](HARDWARE_TEST_v1.13.0.md)) restano valide e vanno eseguite con il
firmware 1.13.1. Qui sotto le prove aggiuntive (**HW-160…HW-169**), da
eseguire sul **Pimoroni Pico Plus 2 W** e, se disponibile, sul **Raspberry Pi
Pico 2 W**.

## 0. Preparazione

1. UF2 della release `v1.13.1` (`picocalc_fxai_pico_plus_2w-v1.13.1.uf2` o
   `picocalc_fxai_pico2w-v1.13.1.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Per HW-166 una microSD FAT32 con la cartella `PICOCALC/CSV`.

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-160 | Aggiornamento dalla v1.13.0 con dati salvati (variabili, cronologia, List 1, EQUATION), spegnimento e riaccensione due volte | tutti i dati restano; nessun "Memory ERROR" all'avvio | foto, log `[flash]` |
| HW-161 | Run-Matrix: `HypergeoCD(1E8,5,5,10)`, poi `HypergeoCD(1E9,1E9,1E9,2E9)` | 1 subito; il secondo dà Argument ERROR subito (prima: minuti o nessuna risposta) | tempi |
| HW-162 | `InvBinomialCD(0.5,20000,0.5)`, `InvPoissonCD(0.5,20000)`, `InvPoissonCD(0.5,1E9)` | 10000, 20000 in pochi secondi al più; il terzo Argument ERROR (prima: 1000000) | tempi |
| HW-163 | `InvGeoCD(0.5,1E-20)`, `GeoCD(1E20,1E-20)` | 6.931471806E+19 subito (prima: nessuna risposta); 0.6321205588 | tempi, foto |
| HW-164 | `nCr(1000000,2)`, `nPr(200000,2)`, `nCr(1E10,1)`, `Σ(X,X,1E19,1E19)` | 4.999995E+11; 3.99998E+10; Argument ERROR; 1E+19 subito (prima: Argument ERROR e, per Σ, nessuna risposta) | foto |
| HW-165 | EQUATION › SIML 2: coefficiente `1E10+Abs(i)` in a1 | valore 10000000001 (prima: 1E+10+i) | foto |
| HW-166 | Statistics › FILE › LOAD di un CSV con 40 000 virgole su una riga e di un CSV di 20 000 righe (sul Pico 2 W) | Dimension ERROR, memoria libera invariata in DIAG (prima: Memory ERROR) | foto, log `[mem]` |
| HW-167 | Salvataggio CSV (SAVE·AS) interrotto togliendo la microSD due volte di seguito, poi LOAD | il file contiene i dati vecchi o quelli nuovi, mai un file troncato | foto |
| HW-168 | Run-Matrix: VARS › V-WIN e VARS › TABLE | voci Xmin, Xmax…, Tptch, TblStart, TblEnd, TblPitch (prima: xmin, tblstart) | foto |
| HW-169 | `tInterval` (Statistics › INTR › t) con List 1 = {1, 2} e C-Level 0.99999; `InvTCD(0.5,1)` | Left −31829,4…, Right 31832,4…; 0 | foto |

## 2. Esiti

Riportare per ogni codice: data, scheda (Pico Plus 2 W / Pico 2 W, revisione),
esito, osservazioni. Ogni KO va riprodotto con un test host prima della
correzione; solo le voci provate passano a TESTED ON HARDWARE in
[FEATURE_MATRIX.md](FEATURE_MATRIX.md).
