# Obiettivo: parità funzionale con Casio fx-CG50

## Stato reale della release v1.0

**Non è ancora una replica completa della fx-CG50 e non va presentata come tale.** La suite matematica host passa, ma questo prova soltanto i casi inclusi nei test. Non prova la parità funzionale, il funzionamento sul PicoCalc o la compatibilità del display con ogni revisione hardware.

La v1.0 contiene un progetto C++ indipendente, parser e algoritmi matematici parziali, UI 320x320, framebuffer RGB565 e un percorso PIO-SPI da verificare sul pannello reale. Il programma principale mostra ancora una schermata dimostrativa: non c'è ancora una navigazione completa fra applicazioni.

## Riferimento funzionale ufficiale

La pagina ufficiale Casio fx-CG50 elenca, tra le altre, queste famiglie:

- Calcolo scientifico, frazioni, numeri complessi, matrici, vettori, Base-N, coordinate, combinatoria, numeri casuali, conversioni, costanti e simboli tecnici.
- Grafici cartesiani, polari, parametrici, disequazioni, coniche, ricorsivi, dinamici, tabelle, doppio grafico, trace/zoom, sketch e risoluzione numerica.
- Grafici 3D di sfera, cilindro, piano e linea, con fino a tre espressioni, rotazione, zoom e sezioni.
- Statistica a una/due variabili, regressioni, grafici statistici, test, intervalli e distribuzioni.
- Geometry con costruzioni, misure, modifica, file e animazioni.
- Spreadsheet, eActivity, programmazione BASIC-like e Python, E-CON4, Picture Plot, calcoli finanziari, trasferimento dati e modalità esame.

Casio dichiara oltre 2900 funzioni/comandi. Il numero non è una specifica API enumerata: la parità va verificata per modalità, operazione e casi di test, non stimata contando soltanto le funzioni del parser.

## Matrice di lavoro

Legenda: `CORE` = algoritmo presente e alcuni test host; `PARZIALE` = primitive o prototipo; `MANCANTE` = non disponibile come funzione utilizzabile; `HARDWARE` = integrazione fisica non validata.

| Area fx-CG50 | Stato v1.0 | Lavoro necessario per parità funzionale |
|---|---|---|
| Calcolo Run-Matrix | PARZIALE | Parser compatibile, editor Natural V.P.A.M. 2D, memoria, replay, undo/copy/paste, formati e catalogo comandi |
| Frazioni e formati naturali | PARZIALE | Albero sintattico editabile, layout 2D di frazioni/radici/integrali/matrici, semplificazione e conversioni |
| Matrici e vettori | PARZIALE | Modalità complete, editor interattivi, dimensioni/limiti, tutte le operazioni e gestione errori |
| Numeri complessi | PARZIALE | Modalità completa, funzioni, conversioni e visualizzazione |
| Base-N e logica | PARZIALE | Aritmetica intera, operatori logici, overflow e formati |
| Calcolo differenziale/integrale | PARZIALE | Derivate e integrali numerici robusti, casi limite e UI di calcolo |
| Equazioni e SOLVE | PARZIALE | Solver numerico generale, sistemi, polinomi e controlli di convergenza |
| Grafici 2D | PARZIALE | Funzioni cartesiane/polari/parametriche, disequazioni, coniche, ricorsione, dinamico, dual graph, trace, zoom, sketch e solve |
| Grafici 3D | PARZIALE | Sfera/cilindro/piano/linea, fino a tre superfici, rotazione, zoom, sezioni e assi |
| Tabelle | PARZIALE | Generatore f(x)/g(x), tabella-grafico e navigazione |
| Statistica | PARZIALE | Tutte le regressioni, test Z/t/chi-quadro/F, ANOVA, intervalli, distribuzioni e grafici |
| Spreadsheet | PARZIALE | Formule, riferimenti celle, editor, funzioni, grafici e persistenza |
| Geometry | MANCANTE | Oggetti, costruzioni, misure, trasformazioni, animazioni, file |
| eActivity | MANCANTE | Documenti/attività matematiche e persistenza |
| Programmazione BASIC-like | MANCANTE | Editor, parser/interprete, runtime, memoria e debug essenziale |
| Python | MANCANTE | Runtime compatibile oppure sottoinsieme documentato adatto a RP2350; non promettere compatibilità CPython completa |
| E-CON4 / acquisizione dati | MANCANTE | Driver/interfaccia per hardware di acquisizione compatibile; PicoCalc da solo non sostituisce il data logger |
| Picture Plot | MANCANTE | Importazione/gestione immagini e strumenti di plot |
| Finanza | MANCANTE | Interesse semplice/composto e calcoli data, con test |
| Costanti e conversioni | MANCANTE | Cataloghi scientifici, unità e conversioni |
| Comunicazione/periferiche | MANCANTE | Protocolli, USB/seriale e compatibilità con periferiche effettivamente presenti |
| Modalità esame | MANCANTE | Profilo con restrizioni verificabili e indicazione visibile dello stato |
| UI, tastiera e display | HARDWARE/PARZIALE | Sequenza init LCD verificata, flush affidabile, mappatura tasti, eventi, focus, menu, schermate e test sul PicoCalc |

## Vincoli e distinzione importante

- L'emulatore `casio-cg50` è un emulatore desktop SH7305/SH-4A. Non è un firmware per RP2350 e non rende automaticamente disponibili le funzioni Casio nel nostro progetto.
- L'avvio del sistema operativo Casio nell'emulatore richiede un dump fornito dal proprietario della calcolatrice; il repository non distribuisce il firmware Casio. Questo progetto non incorpora dump, OS, asset o codice proprietario Casio.
- PicoCalc usa RP2350, LCD ST7365P 320x320 e tastiera I²C; la fx-CG50 ha hardware e display differenti. Possiamo realizzare un prodotto indipendente con funzioni equivalenti dove fattibile, ma non una copia pixel-per-pixel o un clone binario del firmware originale.
- Alcune funzioni Casio dipendono da periferiche esterne (per esempio acquisizione dati). Senza hardware equivalente non è possibile promettere la stessa funzione fisica.
- Memoria, velocità, precisione numerica, autonomia e capacità di grafica 3D devono essere misurate sul Pico 2 W; non si può dedurre la parità dal fatto che il codice compili su PC.

## Criterio per dichiarare una funzione completata

Una voce passa a `COMPLETA` solo quando:
1. esiste l'algoritmo o il servizio;
2. esiste una schermata/menu raggiungibile;
3. tastiera e input funzionano sul dispositivo;
4. output e messaggi di errore sono gestiti;
5. esistono test automatici per casi normali, limiti ed errori;
6. la funzione è provata su PicoCalc reale, non soltanto su host.

## Ordine di sviluppo raccomandato

1. Stabilizzare display, tastiera, launcher e sistema di eventi.
2. Completare Run-Matrix e il renderer Natural V.P.A.M. 2D.
3. Collegare le modalità già presenti (matrici, vettori, liste, equazioni, statistiche) alla UI.
4. Completare grafica 2D e tabella.
5. Completare statistica avanzata, distribuzioni e grafica 3D.
6. Aggiungere Geometry, Spreadsheet, programmazione, finanza, eActivity e strumenti.
7. Aggiungere servizi esterni solo quando esiste l'hardware necessario e una specifica verificabile.

Ogni release deve aggiornare questa matrice con test eseguiti e risultati reali. Non dichiarare mai "parità totale" sulla base di soli test host.


## Update v1.2 — punto A (backend)

Sono state aggiunte API host-tested per sessione Run-Matrix, operazioni avanzate matrici/vettori, aritmetica frazionaria, funzioni complesse, Base-N/logica, calcolo numerico, conversioni, prefissi SI e costanti. La voce A non può ancora essere dichiarata completamente conclusa: editor Natural V.P.A.M. 2D, integrazione UI, parser complesso, cataloghi completi e test differenziali con fx-CG50 restano necessari. Dettagli in `POINT_A_COMPLETION.md`.


## Aggiornamento v1.4 — tastiera e Run-Matrix

Il progetto ora contiene un decoder del FIFO I²C e un controller applicativo host-tested che collega launcher, editor Natural V.P.A.M. e `RunMatrixSession`. I test verificano i codici tasto derivati dal firmware ClockworkPi, l'inserimento di `1+2` e `1/3`, la valutazione e la gestione degli errori. La UI Run-Matrix è raggiungibile nel loop principale.

La voce Run-Matrix resta **PARZIALE**: nessun test è stato eseguito sulla tastiera/display fisici e non è stata prodotta una UF2. Restano scrolling/wrapping, undo/redo, selezione/clipboard, gestione completa dei modificatori, catalogo comandi e validazione sul PicoCalc.
