# Collaudo su PicoCalc fisica — v1.12.0

Questa procedura porta le novità della v1.12.0 (completamento di EQUATION) da
*TESTED ON HOST* / *TESTED ON SIMULATOR* a *TESTED ON HARDWARE*. **Fino a
quando i risultati non arrivano dal dispositivo nessuna voce è dichiarata
provata su hardware.** Ogni prova ha un codice: riportare l'esito (OK / KO /
non eseguita), le righe di log richieste e, dove indicato, una foto.

La v1.12.0 contiene tutte le funzioni della v1.11.0: le prove **HW-01…HW-36**
([v1.8.0](HARDWARE_TEST_v1.8.0.md)), **HW-40…HW-82** ([v1.9.0](HARDWARE_TEST_v1.9.0.md)),
**HW-90…HW-99** ([v1.10.0](HARDWARE_TEST_v1.10.0.md)) e **HW-100…HW-115**
([v1.11.0](HARDWARE_TEST_v1.11.0.md)) restano valide e vanno eseguite con il
firmware 1.12.0. Qui sotto le prove aggiuntive (**HW-120…HW-134**), da
eseguire sul **Pimoroni Pico Plus 2 W** e, se disponibile, sul **Raspberry Pi
Pico 2 W** (stessi limiti di EQUATION sulle due schede; il record del Pico 2 W
è di 59,75 KB).

## 0. Preparazione

1. UF2 della release `v1.12.0` (`picocalc_fxai_pico_plus_2w-v1.12.0.uf2` o
   `picocalc_fxai_pico2w-v1.12.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Prima dell'aggiornamento, con la v1.11.0: EQUATION › SIML 3 incognite con
   l'esempio del manuale (4, 1, −2, −1 / 1, 6, 3, 1 / −5, 4, 1, −7), POLY grado
   3 con 1, −2, −1, 2 e SOLVER con l'equazione `x^3=8` e valore iniziale 1;
   annotare il valore di X. Servono per HW-120.
3. Log USB seriale dall'accensione, come per le versioni precedenti.

## 1. Prove

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-120 | Aggiornare dalla v1.11.0 senza cancellare i dati; EQUATION › SIML (3) e POLY (3) | coefficienti della v1.11.0 al loro posto; SOLVE dà X=1, Y=−1, Z=2 e 2, 1, −1; nel SOLVER l'equazione `x^3-2x=…`/`x^3=8` e X col valore iniziale salvato | foto + log `[storage]` |
| HW-121 | Menu EQUATION | "Equation / Select Type / F1:Simultaneous / F2:Polynomial / F3:Solver / F4:Inequality estensione", tasti SIMUL POLY SOLVER INEQ | foto |
| HW-122 | SIML 3: digitare l'esempio del manuale con EXE dopo ogni valore; ↑ su una cella, `99`, EXIT | il cursore segue a1→b1→c1→d1→a2…; EXIT rimette il valore; intestazione `aₙX+bₙY+cₙZ=dₙ` con le n in basso | foto |
| HW-123 | F1 SOLVE; ↓; F5 (S⇔D); F1 REPEAT | X=1, Y=−1, Z=2 in colonna tra parentesi, valore in basso a destra; REPEAT conserva i coefficienti | foto |
| HW-124 | SIML 2: `1`, `2`, `1` F1 `3` (frazione), `3`, `-1`, `2`; SOLVE | X = 13/21 e Y = −1/7 come frazioni sovrapposte in basso; F5 dà i decimali | foto |
| HW-125 | SIML 2: `1 1 1 / 1 1 2`; SOLVE | "No Solution" | foto |
| HW-126 | SET UP › Complessi a+bi, EXIT (deve tornare a EQUATION); SIML 3: `2 0 0 -i / 0 1 2+i 1.4-1.8i / 0 2 4+2i 2.8-3.6i` (F5 = i durante l'immissione) | "Infinitely Many Solutions", `X=−½i`, `Y=1.4−1.8i−(2+i)Z`, `Z=Z` | foto |
| HW-127 | POLY 3: `1 3 3 1`; poi `1 2 3 2` in Real, a+bi, r∠θ | `X1=−1 ×3` (×3 magenta); Real: solo −1; a+bi: −1, (−1±√7 i)/2 in basso; r∠θ: `1∠π` in basso | foto |
| HW-128 | POLY 2 in a+bi: `1 3 5` | celle `−1.5+1.658312395i`, in basso `(−3+√11 i)/2` | foto |
| HW-129 | POLY 2: `0 1 1`; POLY 2: `1E99 1E99 1E-99` | Ma ERROR (a = 0); Underflow | foto |
| HW-130 | SOLVER: `H=VT-` F1 `1` ↓ `2` → `GT` SHIFT F2 EXE; H=14, V=0, T=2, G=9.8; ↑ fino a V; F6 | `V=16.8`, `Lft=14`, `Rgt=14`; REPEAT: V=16.8 nella tabella; F1 RECALL rimette V=0; misurare il tempo di SOLVE | foto + tempo |
| HW-131 | SOLVER: Lower=20, Upper=30, SOLVE su V; poi Eq `x^2=-1`; poi Eq `3=4` | Can't Solve!; Time Out; No Variable (titoli della finestra di errore) | foto |
| HW-132 | Run-Matrix: VARS › EQUATION › SimRes EXE, `SimCoef`, `PlyRes*2`, `SimRes+SimCoef` | matrici delle soluzioni/coefficienti, risultato in MatAns; l'ultimo dà Ma ERROR | foto |
| HW-133 | Run-Matrix in Math: modello frazione ½ seguito da `X` con X=4 EXE; `GT` x² con G=3, T=2 | 2 (non 0.125); 12 (non 36) | foto |
| HW-134 | Spegnere e riaccendere; EQUATION | coefficienti (anche 1/3 e i complessi), equazione del Solver, Lower/Upper e soluzioni (VARS › SimRes) restano | foto + log `[storage]` |

## 2. Esito

Per ogni prova: esito, modulo (Pico Plus 2 W / Pico 2 W), revisione della
scheda, data. Ogni KO va riprodotto con un test host e corretto prima di
dichiarare la voce *TESTED ON HARDWARE* in [FEATURE_MATRIX.md](FEATURE_MATRIX.md).
