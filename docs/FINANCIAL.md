# Financial — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **C**. Funzioni minime del capitolo 7
della fx-CG50. I calcoli sono quelli di OPTN › TVM di Run-Matrix (un solo
motore, `calc_library`). Stato **Base**.

| Pagina | Dati | Calcoli |
|---|---|---|
| Simple Interest | n (giorni, anno di 365), I%, PV | SI, SFV |
| Compound Interest | n, I%, PV, PMT, FV, P/Y, C/Y (= VARS › TVM) | F1–F5 risolve n, I%, PV, PMT o FV |
| Cash Flow | I%, Csh (lista `{…}` o `List n`) | NPV, IRR, PBP, NFV |
| Amortization | PM1, PM2 e le variabili TVM | BAL, INT, PRN, ΣINT, ΣPRN |
| Interest Conversion | n, I% | ▶EFF, ▶APR |
| Day Calculation | m1 d1 y1, m2 d2 y2 | PRD (giorni) |

- Dal menu: ↑/↓ ed EXE o i tasti 1–6. **SHIFT MENU** (SET UP) alterna
  **Payment BGN / END**; con BGN la prima rata dell'ammortamento è tutto
  capitale.
- **F6 →Ans** copia l'ultimo risultato in Ans.
- F5 FILE (dal menu): OPEN, SAVE·AS (`.pcfin` in `PICOCALC/FIN`, con le
  variabili TVM).

Mancano per la parità: Cost/Sell/Margin, Depreciation, Bond, grafici dei
flussi, Date Mode 360.
