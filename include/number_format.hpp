#pragma once
#include "expression.hpp"
#include <cstdint>
#include <string>

namespace fx {

// fx-CG50 "Display" setup: Norm1/Norm2 (10 significant digits, automatic
// exponent), Fix 0-9 decimals, Sci 1-10 significant digits, Eng notation.
enum class DisplayMode : std::uint8_t { Norm1 = 0, Norm2 = 1, Fix = 2, Sci = 3, Eng = 4 };
struct DisplayFormat {
    DisplayMode mode{DisplayMode::Norm1};
    std::uint8_t digits{0};            // Fix: decimals 0..9, Sci: 1..10 (0 = 10)
};

struct FormattedNumber {
    std::string mantissa;              // includes the sign, e.g. "-1.25"
    int exponent{0};
    bool scientific{false};
    std::string text() const;          // linear form: "-1.25E+12"
};

FormattedNumber formatNumber(double value, DisplayFormat format = {});
std::string formatReal(double value, DisplayFormat format = {});
// Complex result in the requested form ("3+4i", "5∠53.13010235").
std::string formatValue(Complex value, ComplexMode mode, DisplayFormat format = {});

// Continued-fraction search for p/q with q <= maxDenominator that reproduces
// value to ~1e-11 relative precision (used for the S<->D fraction display).
bool exactFraction(double value, long long& numerator, long long& denominator,
                   long long maxDenominator = 9999) noexcept;

const char* displayModeName(DisplayFormat format) noexcept;   // "Norm1", "Fix3", ...
} // namespace fx
