# Distribution — guida breve (v1.18.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **O** (24ª icona). Funzioni minime del
capitolo 18 della fx-CG50 (il manuale descrive il comportamento; nessun codice
o testo copiato). Stato **Base**: usabile, parità non completa (vedi in fondo).
Il calcolo è quello di **Statistics › DIST** (nucleo condiviso `dist_core`, che
usa le stesse funzioni di OPTN › STAT › DIST di Run-Matrix).

## Uso

1. Elenco delle **24 funzioni** (Npd, Ncd, InvN, tpd, tcd, Invt, Cpd, Ccd,
   InvC, Fpd, Fcd, InvF, Bpd, Bcd, InvB, Ppd, Pcd, InvP, Gpd, Gcd, InvG, Hpd,
   Hcd, InvH): ↑ ↓ e EXE (o F1 SELECT).
2. Modulo: **Data** Variable o List (← →), Tail per Inverse Normal, i
   parametri della distribuzione, **Save Res** (None o List n).
3. **F1 CALC**: con Data Variable il risultato va in **Ans** (p, o x per le
   inverse); con Data List un risultato per elemento in **ListAns** e nella
   lista di Save Res. Le liste e le inverse girano a passi: **AC** = Break.
4. **F6 DRAW** (continue: Normale, t, χ², F): curva con l'area o il punto.
5. EXIT torna al modulo e all'elenco; i parametri di ogni funzione, la scelta
   e l'ultimo risultato restano nella sezione **`0x0117`** del record v12.

Nessun tipo di file proprio: lo stato è piccolo e i risultati vanno nelle liste.

## Manca alla parità (campo `missing` del registro)

Schermo diviso grafico/parametri con l'aggiornamento dal vivo, spostamento dei
limiti con il cursore sul grafico, tabella dei valori della distribuzione,
strisce di eActivity.
