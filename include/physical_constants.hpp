#pragma once
// Catalogo delle costanti (v1.14.0): matematiche, fisiche CODATA 2022 / SI 2019
// e astronomiche IAU 2012/2015. E' l'unica fonte di verita' per il parser
// (nomi e valori), il renderer e la cronologia (glifi), il menu OPTN > CONST di
// RUN-MATRIX, le pagine COSTANTI dell'app SYMBOLS e pointa::physicalConstant().
// La tabella e' constexpr (in flash sul firmware): nessuna allocazione.
// Elenco completo, fonti e scelte: docs/PHYSICAL_CONSTANTS.md.
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace fx::physconst {

struct ConstDef {
    const char* parserName;   // minuscolo ASCII, es. "physme": il nome del testo canonico
    const char* altNames;     // alias minuscoli separati da '|', es. "electronmass" ("" se nessuno)
    const char* glyph;        // UTF-8 mostrato a video, es. "mₑ"
    const char* nameIT;       // nome italiano breve, es. "Massa dell'elettrone"
    double value;             // valore in unita' SI
    const char* unit;         // "kg", "J·s", "m⁻¹"... ("" se adimensionale)
    double relUncertainty;    // incertezza standard relativa, 0 se esatta
    std::uint8_t group;       // indice in constGroups()
};

struct ConstGroup {
    const char* name;         // voce del menu CONST, es. "Universali"
    const char* title;        // titolo della pagina di SYMBOLS, es. "COSTANTI FISICHE: UNIVERSALI"
    const char* example;      // esempio d'uso mostrato in SYMBOLS ("espressione ≈ valore unita'")
};

// Gruppi nell'ordine del menu CONST (0 = Matematiche).
std::span<const ConstGroup> constGroups() noexcept;
// Tutte le costanti, ordinate per gruppo.
std::span<const ConstDef> allConstants() noexcept;
// Le costanti di un gruppo (vuoto se l'indice non esiste).
std::span<const ConstDef> constantsOf(std::size_t group) noexcept;
// Cerca un nome minuscolo tra parserName e altNames; nullptr se non c'e'.
const ConstDef* findConstant(std::string_view lowerName) noexcept;
// Come findConstant ma senza distinguere maiuscole e minuscole (ASCII):
// "NA", "kB", "speedOfLight" (glifi di formatNatural e nomi di point_a).
const ConstDef* findConstantNoCase(std::string_view name) noexcept;
// La costante il cui glifo e' esattamente `glyph` ("c", "G", "Nₐ"...); nullptr se nessuna.
const ConstDef* findByGlyph(std::string_view glyph) noexcept;
inline bool isExact(const ConstDef& k) noexcept { return k.relUncertainty == 0.0; }
// Chiama f(alias) per ogni alias di k (senza allocare).
template <class F> void forEachAlias(const ConstDef& k, F&& f) {
    std::string_view rest(k.altNames);
    while (!rest.empty()) {
        const std::size_t bar = rest.find('|');
        f(rest.substr(0, bar));
        if (bar == std::string_view::npos) break;
        rest.remove_prefix(bar + 1);
    }
}

} // namespace fx::physconst
