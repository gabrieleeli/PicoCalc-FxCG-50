#pragma once
// CalcValue: il valore unificato del motore delle espressioni: un numero, una
// lista, una matrice o un vettore (i tipi di dati di Run-Matrix della fx-CG50).
// Una sola rappresentazione e' condivisa dal valutatore, dalle memorie Mat/Vct/List
// del workspace, dall'editor MAT/VCT e dalla visualizzazione dei risultati.
//
// Gli elementi sono memorizzati per righe come double; le parti immaginarie
// stanno in un secondo vettore che esiste solo quando almeno un elemento e'
// complesso, cosi' una matrice reale costa 8 byte per elemento (RAM e record in flash).
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
    void set(std::size_t i, Complex z);          // crea il vettore delle parti immaginarie quando serve
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
