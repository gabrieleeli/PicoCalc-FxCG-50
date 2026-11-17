# Collaudo su PicoCalc fisica — v1.14.0 (CONVERSION completo)

Questa procedura verifica sul dispositivo la nuova modalità CONVERSION
([CONVERSION.md](CONVERSION.md), [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)).
**Fino a quando i risultati non arrivano dal dispositivo nessuna voce è
dichiarata provata su hardware.** Ogni prova ha un codice: riportare l'esito
(OK / KO / non eseguita), le righe di log richieste e, dove indicato, una foto
o il tempo.

La v1.14.0 contiene tutte le funzioni della v1.13.1: le prove **HW-01…HW-169**
delle procedure precedenti ([v1.13.1](HARDWARE_TEST_v1.13.1.md) e i documenti
da lì collegati) restano valide e vanno eseguite con il firmware 1.14.0, in
particolare quelle di Run-Matrix su CONVERT (HW-140…HW-159). Qui sotto le prove
aggiuntive (**HW-170…HW-190**), da eseguire sul **Pimoroni Pico Plus 2 W** e
sul **Raspberry Pi Pico 2 W** (che ha la partizione firmware più piccola e
nessuna PSRAM).

## 0. Preparazione

1. UF2 della v1.14.0 (`picocalc_fxai_pico_plus_2w.uf2` o `picocalc_fxai_pico2w.uf2`)
   compilati con `scripts/build.sh --board …` o scaricati dalla release e
   controllati con `sha256sum -c SHA256SUMS`.
2. Prima dell'aggiornamento: un workspace della v1.13.1 con dati (variabili,
   cronologia di Run-Matrix con una conversione `10[cm]▶[in]`, List 1, Str 1).
3. Log USB dall'accensione (righe `[mem]`, `[flash]`, `[fault]`).

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-170 | Aggiornamento dalla v1.13.1 (record v9), spegnimento e riaccensione due volte | tutti i dati restano (record v9 letto e riscritto come v10); nessun "Memory ERROR" | foto, log `[flash]` |
| HW-171 | Menu › K (CONVERSION): scorrere l'elenco fino a "Valute" con ↓ e PageDown | 12 gruppi, 97 voci, scorrimento fluido, titoli dei gruppi visibili | tempo per arrivare in fondo |
| HW-172 | Scrivere `poll` nell'elenco | 13 voci (Lunghezza, Area, Volume… le grandezze con unità in pollici); DEL toglie una lettera, EXIT azzera la ricerca | foto |
| HW-173 | Lunghezza: Valore `1/3`, Da m, A cm, EXE | `33.33333333 cm`, `esatto: 100/3`; in Run-Matrix `Ans` = 33.33333333 e in forma esatta 100/3 | foto |
| HW-174 | F3 ALL nella stessa schermata | 47 righe (fm…m.p.), scorrimento con ↑↓; EXIT torna alla conversione | foto |
| HW-175 | F4 PREFIX su Da = m, scegliere `G` (giga); poi su A = B in Dati digitali scegliere `Ki` | Da diventa `Gm`; `KiB` | foto |
| HW-176 | F2 SWAP dopo una conversione °C → °F di 100 | 100 °F → 37.77777778 °C, esatto 340/9 | foto |
| HW-177 | F5 COPY, poi Run-Matrix e Ctrl+V | l'espressione del risultato viene incollata | foto |
| HW-178 | Valore `abc+` e EXE | Syntax ERROR, nessun riavvio; qualsiasi tasto chiude il riquadro | foto |
| HW-179 | Fotone: 532 nm → eV; Massa ↔ energia: 1 u → MeV; kT: 1 eV → K | 2.330530046; 931.4941024; 11604.51812 | foto |
| HW-180 | Potenza: 30 dBm → W; 0 W → dBm | 1; Math ERROR | foto |
| HW-181 | Calibro AWG: 10 AWG → mm e → mm²; 61 AWG | 2.588186728 mm, 5.261154955 mm²; Argument ERROR | foto |
| HW-182 | Formati › Colori: `#E8601E` → HSV e RGB565 | `19.6°, 87.1%, 91%`, `0xEB03`; il riquadro dell'anteprima ha il colore arancione del pulsante Run-Matrix del menu | foto del colore |
| HW-183 | Formati › Numeri romani: `4000` → romano (1-3999); `2026` | Argument ERROR; `MMXXVI` | foto |
| HW-184 | Formati › Date: `2026-11-17` → F3 ALL | JD 2461361.5, MJD 61361, 2026-321, 2026-W47-2, Unix 1794873600, martedì | foto |
| HW-185 | Formati › IEEE-754: `0.1` → double | `3FB999999999999A  s=0 e=1019 m=999999999999A` | foto |
| HW-186 | Valute: EUR → USD senza tasso; F4 TASSI, USD = 1.08; 100 EUR → USD; spegnere e riaccendere | Argument ERROR; 108; dopo il riavvio il tasso è ancora 1.08 | foto |
| HW-187 | F2 PREF su una voce dell'elenco; spegnere e riaccendere | la voce compare in "Preferiti" (con `*`) anche dopo il riavvio; le ultime conversioni in "Recenti" | foto |
| HW-188 | Run-Matrix: `10[nm]▶[eV]`, `2[kWb]▶[Mx]`, `1[eV]▶[K]` | 123.984198; 2E+11; Argument ERROR (come la v1.13.1: due unità fx-CG50) | foto |
| HW-189 | SYSTEM › INFO / DIAG dopo HW-170…HW-188 sul **Pico 2 W** | nessun errore di memoria; heap libero simile alla v1.13.1 (la tabella delle unità è in flash, non in RAM) | log `[mem]` |
| HW-190 | Tempi delle schermate compilate con `-Os` dalla v1.14.0 (Statistics › CALC 1-Var su 999 valori, TABLE con 255 righe, salvataggio del workspace dopo una modifica, apertura di un progetto `.pcgraph`, SYSTEM › DIAG) confrontati con la v1.13.1 sulla stessa scheda | differenza non percepibile (il calcolo resta nel motore a `-O2`) | tempi delle due versioni |
