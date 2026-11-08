# Roadmap — dopo la v1.6.0

Il progetto **non** ha la parità con la Casio fx-CG50 e non va presentato come
tale: la tabella sotto elenca ciò che manca, in ordine di priorità.
Il documento storico `archive/CG50_PARITY_ROADMAP.md` descrive gli obiettivi delle
versioni precedenti.

## 1. Collaudo su hardware (priorità massima)

Nessuna funzione è stata provata sulla PicoCalc. Procedura in
[HARDWARE.md](HARDWARE.md#collaudo-consigliato-sul-dispositivo):

1. display (colori RGB666/RGB565, orientamento, area visibile, velocità);
2. tastiera (codici, tenuta/ripetizione, modificatori, tempi I2C);
3. flash (salvataggio, riavvio, blackout durante la scrittura);
4. batteria e retroilluminazione;
5. Wi-Fi, DNS, handshake TLS, consumo di RAM di mbedTLS, chat reale tramite relay;
6. stabilità prolungata e consumi.

Ogni risultato va riportato in [FEATURE_MATRIX.md](FEATURE_MATRIX.md).

## 2. Motore matematico

| Funzione | Note |
|---|---|
| Matrici e vettori come valori del parser, editor MAT/VCT in Run-Matrix | le API C++ esistono e sono testate |
| Operatori logici Base-N (and, or, xor, not) nel parser | API C++ presenti |
| Selezione parziale nell'editor | oggi copia/incolla dell'intera espressione |
| Matrici come modelli 2D modificabili | solo rendering |
| Derivate/integrali simbolici, limiti | |

## 3. Modalità

| Modalità | Contenuto da realizzare |
|---|---|
| Statistics | regressioni sinusoidale e logistica; test χ², F, 2 campioni, ANOVA nell'interfaccia; grafici a torta e a barre |
| Graph | grafici parametrici e impliciti |
| Equation | interfaccia per i sistemi di disequazioni |
| Recursion, Conic Graphs, Dyna Graph | nuove modalità basate sul motore esistente |
| Spreadsheet, eActivity | richiedono un formato di file e più memoria |
| Program | interprete del linguaggio di programma (sintassi propria, non il codice Casio) |
| Python | integrazione di MicroPython come libreria (heap separato, I/O sul display) |
| Financial, E-CON4, Link, Geometry, Picture Plot, 3D Graph | da progettare |

## 4. Piattaforma

- Scheda SD (FAT) per programmi, documenti e backup del workspace.
- Gestione energetica: spegnimento del display, sospensione, risveglio da tastiera.
- DMA e clock SPI più alto per il display, se il collaudo lo consente.
- Animazioni di transizione tra le schermate (primitiva già presente).

## 5. AI

- Prova con chiavi reali e misura dei tempi di risposta dal Pico.
- Eventuale streaming della risposta (oggi la risposta arriva intera).
