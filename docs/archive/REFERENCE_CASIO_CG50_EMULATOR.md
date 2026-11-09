# Riferimento tecnico: emulatore indipendente Casio fx-CG50

Risorsa indicata dall'utente: https://github.com/MaxSignal/casio-cg50

La pagina GitHub attualmente consultabile presenta il progetto sotto il repository
`hexbinoct/casio-cg50`. Prima di importare codice occorre verificare sempre il
repository effettivamente raggiunto e la licenza della revisione scaricata. La
README consultata dichiara licenza MIT per il codice originale del progetto.

## Cosa offre

- Emulazione hardware della famiglia fx-CG50 basata su SH7305 / SH-4A.
- Core Go principale (`emu_go/`) e modello Python di riferimento (`emu/`).
- Modello MMU/memoria e periferiche SH7305, LCD e tastiera.
- Strumenti di reverse engineering, disassemblatore, analisi framebuffer e mappatura tasti.
- Test di conformità CPU e tracce di avvio.
- L'emulatore può avviare il sistema operativo Casio solo se l'utente fornisce un dump
  della propria calcolatrice; il repository non distribuisce il firmware Casio.

## Come lo useremo in PicoCalc FX AI

Questa risorsa è un riferimento di comportamento e validazione, non un componente
firmware da copiare sul RP2350. Il suo scopo è aiutarci a confrontare:

1. gerarchia dei menu e flussi applicativi fx-CG50;
2. disposizione della schermata principale e delle schermate applicative;
3. semantica dei tasti e navigazione;
4. comportamento visivo della UI e aggiornamento del display;
5. test ripetibili dei flussi dell'interfaccia.

## Perché non si integra direttamente nel firmware PicoCalc

Il progetto emula un SoC SH7305 con CPU SH-4A, MMU, RAM e periferiche Casio.
PicoCalc FX AI gira invece sul Raspberry Pi Pico 2 W / RP2350 e sul display
ST7365P 320x320. Sono architetture e periferiche differenti: il core Go/Python
dell'emulatore desktop non è un driver LCD né un'applicazione da incorporare
direttamente nel firmware C++ del Pico. Inoltre l'avvio del vero sistema Casio
richiede un dump ottenuto dall'hardware dell'utente e non incluso nel progetto.

La risorsa emulatore viene quindi registrata come **oracolo esterno di riferimento**.
Il firmware PicoCalc resta un'implementazione indipendente, con interfaccia ispirata
alla fx-CG50 e funzioni matematiche proprie. Non includere dump ROM/flash Casio,
file di sistema, add-in proprietari o asset estratti dal sistema operativo.

## Differenze hardware da tenere separate

| Elemento | fx-CG50 emulata | PicoCalc FX AI |
|---|---|---|
| CPU | Renesas SH7305 / SH-4A | RP2350 / ARM |
| Display | Controller LCD Casio R61524 | ST7365P, 320x320 |
| Tastiera | KEYSC e matrice tasti Casio | MCU tastiera su I2C |
| Software | OS Casio da dump dell'utente | Firmware C++ indipendente |

Le informazioni sopra sono tratte dalla README del repository consultata il 6 novembre 2026.
