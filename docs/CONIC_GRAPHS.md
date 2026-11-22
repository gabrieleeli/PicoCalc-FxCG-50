# Conic Graphs — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **9**. Funzioni minime delle sezioni
5-50…5-51 della fx-CG50. Stato **Base**.

| Modello | Coefficienti |
|---|---|
| X = A(Y−K)² + H, Y = A(X−H)² + K | A ≠ 0, H, K |
| (X−H)² + (Y−K)² = R² | H, K, R > 0 |
| (X−H)²/A² + (Y−K)²/B² = 1 | A, B > 0, H, K |
| (X−H)²/A² − (Y−K)²/B² = 1, (Y−K)²/B² − (X−H)²/A² = 1 | A, B > 0, H, K |

- Elenco dei modelli (F1 SELECT o EXE), coefficienti come espressioni (stesso
  valutatore della calcolatrice), F6 **DRAW** sulla V-Window di Graph.
- F1 **TRACE**: cursore sulla curva con le coordinate; ↑/↓ cambia ramo
  dell'iperbole.
- F5 **G-SOLVE**: VERTEX, FOCUS, CENTER, RADIUS, ASYMPT (rette disegnate),
  e (eccentricità); «Math ERROR» se la voce non vale per il modello.
- F5 FILE (dall'elenco): OPEN, SAVE·AS (`.pcconic` in `PICOCALC/CONIC`).
- Strisce **Conics Graph** (apre il grafico) e **Conics Editor** di eActivity.

Mancano per la parità: forme polari e parametriche, direttrice, asse, latus
rectum, intercette, ZOOM.
