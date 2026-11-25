# Esempi di Program (v1.20.0)

Programmi completi per la modalità **Program** della Pimoroni Pico Plus 2 W
([PROGRAM.md](../../PROGRAM.md)). Si copiano sulla microSD nella cartella del
loro linguaggio (`PICOCALC/CODE/C`, `CODE/CPP`, `CODE/CS`), con il PC o con
`tools/picocalc_link.py`, e si aprono dal gestore dei file di Program.

| File | Linguaggio | Cosa mostra | Serve |
|---|---|---|---|
| [`cpp/Equazioni.cpp`](cpp/Equazioni.cpp) | C++ | equazioni di secondo grado lette fino a Ctrl+D, radici reali e complesse, `fx::setVar` (X e Y della calcolatrice) | — |
| [`cpp/Trasformazioni.cpp`](cpp/Trasformazioni.cpp) | C++ | matrici con `std::vector`, rotazione, simmetria, scala, traslazione e composizione in coordinate omogenee; disegno con `fx::line` (F6 GRAPH) | — |
| [`cs/StatisticheCSV.cs`](cs/StatisticheCSV.cs) | C# | file CSV nella cartella del programma (`File.ReadAllLines`), media, deviazione, mediana, `Fx.Stat1Var`, `Fx.Regression`, colonne nelle liste 1 e 2 per Statistics | — |
| [`cs/Duello.cs`](cs/Duello.cs) | C# | gioco a turni con classi, proprietà, `switch` su stringhe e un generatore pseudo-casuale proprio | — |
| [`c/FigureLibreria.c`](c/FigureLibreria.c) | C | uso di una libreria installata (`#include <trasformazioni.h>`, che dipende da `matrici`) | libreria `trasformazioni` (Program › LIB) |

Ogni esempio ha l'uscita attesa (`.out`) e, se legge l'input, le righe date
(`.in`). `libraries_test` li esegue tutti con l'interprete dopo aver
installato le librerie di [`libraries/`](../../../libraries) e confronta le
uscite; le stesse uscite si ottengono con g++, gcc (`-std=gnu11` per `M_PI`)
e Mono togliendo le sole chiamate alla libreria `fx`.
