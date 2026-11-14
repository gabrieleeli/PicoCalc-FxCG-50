#include "value.hpp"
#include <algorithm>

namespace fx {
const char* valueTypeName(ValueType t) noexcept {
    switch (t) {
    case ValueType::Scalar: return "numero";
    case ValueType::List: return "lista";
    case ValueType::Matrix: return "matrice";
    case ValueType::Vector: return "vettore";
    }
    return "?";
}

void CalcValue::set(std::size_t i, Complex z) {
    re[i] = z.real();
    if (z.imag() != 0.0 && im.empty()) im.assign(re.size(), 0.0);   // puo' lanciare std::bad_alloc
    if (!im.empty()) im[i] = z.imag();
}
void CalcValue::normalize() {
    if (im.empty()) return;
    if (std::all_of(im.begin(), im.end(), [](double d) { return d == 0.0; })) { im.clear(); im.shrink_to_fit(); }
}
bool CalcValue::operator==(const CalcValue& o) const noexcept {
    if (type != o.type || rows != o.rows || cols != o.cols || re != o.re) return false;
    if (im.empty() != o.im.empty()) {
        const auto& some = im.empty() ? o.im : im;
        return std::all_of(some.begin(), some.end(), [](double d) { return d == 0.0; });
    }
    return im == o.im;
}
CalcValue CalcValue::scalarOf(Complex z) {
    CalcValue v;
    v.type = ValueType::Scalar; v.rows = v.cols = 1;
    v.re.assign(1, z.real());
    if (z.imag() != 0.0) v.im.assign(1, z.imag());
    return v;
}
CalcValue CalcValue::list(std::size_t length) {
    CalcValue v;
    v.type = ValueType::List; v.rows = static_cast<std::uint16_t>(length); v.cols = 1;
    v.re.assign(length, 0.0);
    return v;
}
CalcValue CalcValue::matrix(std::size_t rows, std::size_t cols, ValueType type) {
    CalcValue v;
    v.type = type; v.rows = static_cast<std::uint16_t>(rows); v.cols = static_cast<std::uint16_t>(cols);
    v.re.assign(rows * cols, 0.0);
    return v;
}
std::string dimensionText(const CalcValue& v) {
    if (v.empty()) return "None";
    if (v.isList()) return std::to_string(v.rows);
    return std::to_string(v.rows) + "\xC3\x97" + std::to_string(v.cols);
}
} // namespace fx
