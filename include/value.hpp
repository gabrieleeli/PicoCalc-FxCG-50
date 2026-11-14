#pragma once
// CalcValue: the unified value of the expression engine: a number, a list, a matrix or a
// vector (fx-CG50 Run-Matrix data types). One representation is shared by the
// evaluator, the Mat/Vct/List memories of the workspace, the MAT/VCT editor
// and the result display.
//
// Elements are stored row-major as doubles; the imaginary parts live in a
// second array that exists only when at least one element is complex, so a
// real matrix costs 8 bytes per element (RAM and flash record).
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {
using Complex = std::complex<double>;

enum class ValueType : std::uint8_t { Scalar = 0, List = 1, Matrix = 2, Vector = 3 };
// "numero", "lista", "matrice", "vettore" (for messages).
const char* valueTypeName(ValueType type) noexcept;

struct CalcValue {
    ValueType type{ValueType::Scalar};
    std::uint16_t rows{0}, cols{0};       // list: rows = length, cols = 1; scalar: 1 x 1
    std::vector<double> re;               // rows * cols elements (empty: no value / "None")
    std::vector<double> im;               // empty when every element is real

    bool empty() const noexcept { return re.empty(); }
    std::size_t size() const noexcept { return re.size(); }
    bool isScalar() const noexcept { return type == ValueType::Scalar; }
    bool isList() const noexcept { return type == ValueType::List; }
    bool isMatrixLike() const noexcept { return type == ValueType::Matrix || type == ValueType::Vector; }
    // fx-CG50: a vector is a matrix with one row or one column.
    bool vectorShaped() const noexcept { return isMatrixLike() && !empty() && (rows == 1 || cols == 1); }
    bool complex() const noexcept { return !im.empty(); }
    Complex at(std::size_t i) const noexcept { return {re[i], im.empty() ? 0.0 : im[i]}; }
    Complex at(std::size_t row, std::size_t col) const noexcept { return at(row * cols + col); }
    void set(std::size_t i, Complex z);          // allocates the imaginary array on demand
    Complex scalar() const noexcept { return empty() ? Complex{} : at(0); }
    void normalize();                            // drops the imaginary array when it is all zero
    bool operator==(const CalcValue& o) const noexcept;
    bool operator!=(const CalcValue& o) const noexcept { return !(*this == o); }

    static CalcValue scalarOf(Complex z);
    static CalcValue list(std::size_t length);                                     // zero-filled
    static CalcValue matrix(std::size_t rows, std::size_t cols, ValueType type = ValueType::Matrix);
};

// "2×3", "1×3", "5" (list length); "None" for an empty value (fx-CG50 editor).
std::string dimensionText(const CalcValue& v);
} // namespace fx
