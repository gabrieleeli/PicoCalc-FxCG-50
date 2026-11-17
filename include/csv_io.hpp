#pragma once
// File CSV di liste e matrici (fx-CG50: CSV dell'editor delle liste, manuale
// 3-18…3-20, e dell'editor MAT/VCT, 2-48). Nessuna interfaccia utente: le
// applicazioni chiamano queste funzioni e mostrano gli errori.
//
// Formato (come la calcolatrice):
//  * separatore ',' o ';', simbolo decimale '.' o ',' (Settings::csvSeparator,
//    csvDecimal; ',' + ',' non e' ammesso);
//  * fine riga CR, LF o CRLF; il tabulatore non e' un separatore;
//  * se la riga 1 contiene virgolette (" o ') e' un'intestazione e si ignora;
//  * solo numeri reali (gli elementi complessi non si salvano: "Complex Number
//    In Data"); i sottonomi delle liste non si scrivono nel file.
// I file stanno in PICOCALC/CSV sulla microSD o sulla flash interna
// (LittleFS) e si scrivono con files::writeAtomic: dopo un'interruzione resta
// il file vecchio o quello nuovo, mai un misto.
#include "file_store.hpp"
#include "value.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {
struct Workspace;
}

namespace fx::csv {

constexpr const char* Folder = "PICOCALC/CSV";
constexpr const char* Extension = ".csv";
constexpr std::size_t MaxNameLength = 8;                 // nome del file senza estensione (fx-CG50: 8)

struct Format {
    char separator{','};
    char decimal{'.'};
};
// Vuoto se il formato e' valido, altrimenti il messaggio d'errore.
std::string validate(const Format& f);
Format formatOf(const Workspace& ws);

// Tabella letta da un CSV: cells per righe (rows x cols); present indica le
// celle non vuote (le colonne delle liste possono avere lunghezze diverse).
struct Table {
    std::size_t rows{0}, cols{0};
    std::vector<double> cells;
    std::vector<std::uint8_t> present;
    double at(std::size_t r, std::size_t c) const { return cells[r * cols + c]; }
    bool has(std::size_t r, std::size_t c) const { return present[r * cols + c] != 0; }
};

// Testo CSV di colonne (liste) di lunghezze diverse: le celle mancanti restano vuote.
std::string encodeColumns(const std::vector<const std::vector<double>*>& columns, const Format& f);
// Testo CSV di una matrice o di un vettore; lancia MathError con elementi complessi.
std::string encodeMatrix(const CalcValue& m, const Format& f);
// Legge un CSV; lancia MathError ("Format ERROR: ...", "Dimension ERROR: ...")
// per testo non numerico o tabelle oltre maxRows x maxCols (o oltre maxCells
// celle). Le dimensioni si controllano prima di allocare la tabella (v1.13.1).
Table decode(const std::uint8_t* data, std::size_t size, const Format& f, std::size_t maxRows, std::size_t maxCols,
             std::size_t maxCells = static_cast<std::size_t>(-1));

// Byte massimi di un file CSV letto (dipendono dalla scheda: liste e matrici piu' grandi sulla Plus).
std::size_t maxFileBytes() noexcept;

// ---- file (stessa interfaccia per microSD e flash interna)
std::string pathOf(const std::string& name);              // "PICOCALC/CSV/NAME.csv"
// Nomi (senza estensione) dei CSV della cartella, in ordine alfabetico.
files::Status list(files::FileSystem& fs, std::vector<std::string>& names);
// Scrittura atomica (crea la cartella). Lancia MathError per un nome non valido.
files::Status save(files::FileSystem& fs, const std::string& name, const std::string& text);
files::Status load(files::FileSystem& fs, const std::string& name, std::vector<std::uint8_t>& out);

// ---- workspace
// Colonne del file -> List first+1, first+2, ... (tutto o niente: controlla
// lunghezze, numero di liste e budget della scheda prima di scrivere).
// Restituisce il numero di liste caricate.
std::size_t loadLists(Workspace& ws, std::size_t first, const Table& t);
// File -> Mat (vectors == false) o Vct della memoria slot (0..25), con i
// controlli di storeMatrix / storeVector.
void loadMatrix(Workspace& ws, bool vectors, std::size_t slot, const Table& t);
// Liste da salvare: List 1 .. l'ultima non vuota (almeno una).
std::string encodeLists(const Workspace& ws);

} // namespace fx::csv
