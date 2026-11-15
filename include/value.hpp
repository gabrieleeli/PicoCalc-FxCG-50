#pragma once
// CalcValue: il valore unificato del motore delle espressioni: un numero, una
// lista, una matrice o un vettore (i tipi di dati di Run-Matrix della fx-CG50).
// Una sola rappresentazione e' condivisa dal valutatore, dalle memorie Mat/Vct/List
// del workspace, dall'editor MAT/VCT e dalla visualizzazione dei risultati.
//
// Gli elementi sono memorizzati per righe come double; le parti immaginarie
// stanno in un secondo vettore che esiste solo quando almeno un elemento e'
// complesso, cosi' una matrice reale costa 8 byte per elemento (RAM e record in flash).
//
// Dalla v1.11.0 un valore puo' portare anche la forma esatta di ogni elemento
// (`exact`, vuoto quando manca): le celle dell'editor MAT/VCT accettano
// espressioni come 1/3 o 2√3, e le memorie fino a kExactElements elementi le
// conservano, cosi' Mat A x 3 resta esatto.
#include "exact.hpp"
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {
using Complex = std::complex<double>;

enum class ValueType : std::uint8_t { Scalar = 0, List = 1, Matrix = 2, Vector = 3 };
// "numero", "lista", "matrice", "vettore" (per i messaggi).
const char* valueTypeName(ValueType type) noexcept;

struct CalcValue {
    ValueType type{ValueType::Scalar};
    std::uint16_t rows{0}, cols{0};       // lista: rows = lunghezza, cols = 1; numero: 1 x 1
    std::vector<double> re;               // rows * cols elementi (vuoto: nessun valore, "None")
    std::vector<double> im;               // vuoto quando tutti gli elementi sono reali
    std::vector<exact::Value> exact;      // vuoto, oppure una forma esatta per elemento (solo valori reali)
    static constexpr std::size_t ExactElements = 64;   // oltre, le forme esatte non si conservano

    bool empty() const noexcept { return re.empty(); }
    std::size_t size() const noexcept { return re.size(); }
    bool isScalar() const noexcept { return type == ValueType::Scalar; }
    bool isList() const noexcept { return type == ValueType::List; }
    bool isMatrixLike() const noexcept { return type == ValueType::Matrix || type == ValueType::Vector; }
    // fx-CG50: un vettore e' una matrice con una sola riga o una sola colonna.
    bool vectorShaped() const noexcept { return isMatrixLike() && !empty() && (rows == 1 || cols == 1); }
    bool complex() const noexcept { return !im.empty(); }
    Complex at(std::size_t i) const noexcept { return {re[i], im.empty() ? 0.0 : im[i]}; }
    Complex at(std::size_t row, std::size_t col) const noexcept { return at(row * cols + col); }
    // Crea il vettore delle parti immaginarie quando serve. Le forme esatte
    // restano solo se z e' un intero (altrimenti vengono tolte tutte).
    void set(std::size_t i, Complex z);
    // Elemento reale con la sua forma esatta (crea le forme degli altri elementi
    // quando sono interi; altrimenti, o oltre ExactElements, le toglie).
    void setExact(std::size_t i, const exact::Value& v);
    bool hasExact() const noexcept { return !exact.empty() && exact.size() == re.size(); }
    Complex scalar() const noexcept { return empty() ? Complex{} : at(0); }
    void normalize();                            // elimina le parti immaginarie quando sono tutte nulle
    bool operator==(const CalcValue& o) const noexcept;
    bool operator!=(const CalcValue& o) const noexcept { return !(*this == o); }

    static CalcValue scalarOf(Complex z);
    static CalcValue list(std::size_t length);                                     // piena di zeri
    static CalcValue matrix(std::size_t rows, std::size_t cols, ValueType type = ValueType::Matrix);
};

// "2×3", "1×3", "5" (lunghezza di una lista); "None" per un valore vuoto (editor fx-CG50).
std::string dimensionText(const CalcValue& v);
} // namespace fx
