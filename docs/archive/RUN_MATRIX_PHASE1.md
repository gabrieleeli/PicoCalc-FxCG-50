# Run-Matrix — Fase 1 (v1.5.2-dev)

## Implementato in questa tranche

- La schermata Run-Matrix riceve lo storico effettivo da `RunMatrixSession`.
- Mostra gli ultimi due calcoli precedenti (espressione + valore) nel riquadro inferiore.
- Mantiene il risultato corrente separato e visibile nel pannello Risultato.
- Messaggi di errore e storico sono disposti nel limite verticale del pannello 320×320.
- La suite `tests/run_host_tests.sh` passa integralmente.

## Non ancora completato

Questa release non è ancora una replica completa della fx-CG50. Restano almeno:

1. Replay dello storico con frecce SU/GIÙ senza interferire con la navigazione strutturata del cursore.
2. Undo/redo, selezione, copia/incolla e modifica rapida.
3. Gestione completa SHIFT/ALPHA e cataloghi di funzioni/comandi.
4. Formati di risultato (decimale/frazione, standard/scientifico/ingegneristico), arrotondamento e precisione configurabile.
5. Modalità angolare selezionabile DEG/RAD/GRA e indicatori di stato coerenti.
6. Editor e operazioni interattive per matrici, vettori, liste, numeri complessi e Base-N.
7. Verifica differenziale dei casi di calcolo contro una fx-CG50 reale e test su PicoCalc 2 W.

## Test eseguito

```bash
cd picocalc151
bash tests/run_host_tests.sh
```

Risultato: PASS per tutti i test host inclusi nella suite. Questo non verifica il display/tastiera fisici né la build UF2.
