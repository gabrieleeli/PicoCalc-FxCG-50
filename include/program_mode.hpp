#pragma once
// Nucleo della modalita' Program (v1.17.0, Base; solo Pimoroni Pico Plus 2 W).
// L'interprete C/C++/C# e' quello della v1.15.0 (program.hpp, servizio
// Interp); qui ci sono il formato dei file .pcprog, l'evidenziazione minima
// dell'editor e lo stato della modalita' nel workspace.
//
// File .pcprog: testo UTF-8 (si modifica anche sul PC) con la prima riga
// "// pcprog: C" | "C++" | "C#" che dice il dialetto; i .c, .cpp e .cs copiati
// dal PC si aprono come testo con il dialetto dell'estensione.
#include "program.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::progmode {
constexpr const char* MarkerPrefix = "// pcprog: ";
std::string encodeProgram(prog::Dialect d, const std::string& source);
// Toglie la riga del dialetto (se c'e') e restituisce il dialetto; senza la riga
// usa `fallback` (quello dell'estensione del file).
std::string decodeProgram(const std::string& text, prog::Dialect fallback, prog::Dialect& dialect);
// Classi dei caratteri per l'evidenziazione di una riga.
enum class Tok : std::uint8_t { Plain = 0, Keyword = 1, Comment = 2, String = 3, Number = 4, Preproc = 5 };
// Una classe per byte della riga; `inComment` porta lo stato di /* ... */ tra le righe.
std::vector<Tok> highlight(const std::string& line, prog::Dialect d, bool& inComment);
// Programma d'esempio per NEW (un saluto nel dialetto).
std::string templateFor(prog::Dialect d);
struct State {
    std::string lastFile;                             // ultimo programma aperto
    std::uint32_t cursorLine{0}, cursorColumn{0};
    std::uint8_t newDialect{1};                       // dialetto dei programmi nuovi (prog::Dialect)
    bool operator==(const State& o) const noexcept { return lastFile == o.lastFile && cursorLine == o.cursorLine && cursorColumn == o.cursorColumn && newDialect == o.newDialect; }
};
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
} // namespace fx::progmode
