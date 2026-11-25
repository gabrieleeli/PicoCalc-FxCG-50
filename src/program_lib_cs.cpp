// Libreria di C# (v1.20.0): Console, Math, Convert, Parse/TryParse, char,
// string, StringBuilder, collezioni (List, Dictionary, HashSet, Queue,
// Stack), Random con la sequenza di .NET, Array, File, Directory, Path,
// StreamReader/StreamWriter confinati nella cartella del programma,
// Environment. Le eccezioni hanno i nomi e i messaggi di .NET.
#include "program_lib.hpp"
#include "file_store.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fx::prog {
using namespace lib;

namespace {
inline std::int32_t I32(const NativeCall& c, int i) { return static_cast<std::int32_t>(c.args[i].i); }
Value sw(Vm& vm, std::u16string_view s) { return wstr(vm, s); }
Value s8(Vm& vm, const std::string& s) { return wstr(vm, utf8To16(s)); }
std::u16string ws(Vm& vm, const NativeCall& c, int i) {
    if (c.args[i].isNull()) vm.raise("ArgumentNullException", "Value cannot be null.");
    return W(vm, c, i);
}
std::string text(Vm& vm, const NativeCall& c, int i) { return utf16To8(ws(vm, c, i)); }
[[noreturn]] void argRange(Vm& vm, const std::string& p, const std::string& msg) { vm.raise("ArgumentOutOfRangeException", msg + " (Parameter '" + p + "')"); }

// ================================================================ Console
void conWrite(Vm& vm, NativeCall& c) { if (c.argc > 0) vm.host().print(vm.format(c.args[0], Dialect::CSharp)); }
void conWriteLine(Vm& vm, NativeCall& c) { vm.host().print((c.argc > 0 ? vm.format(c.args[0], Dialect::CSharp) : std::string()) + "\n"); }
void conWriteFmt(Vm& vm, NativeCall& c) { vm.host().print(formatComposite(vm, text(vm, c, 0), c.args + 1, c.argc - 1)); }
void conWriteLineFmt(Vm& vm, NativeCall& c) { vm.host().print(formatComposite(vm, text(vm, c, 0), c.args + 1, c.argc - 1) + "\n"); }
void conReadLine(Vm& vm, NativeCall& c) {
    std::string line;
    if (!conLine(vm, line)) { c.ret = Value::null(); return; }
    c.ret = s8(vm, line);
}
void conRead(Vm& vm, NativeCall& c) { c.ret = Value::i32(conGet(vm)); }
void conReadKey(Vm& vm, NativeCall& c) {
    // tasto della PicoCalc (attesa): il carattere
    for (;;) {
        const int k = vm.host().getKey();
        if (k > 0) { c.ret = Value::ofInt(VK::C16, k < 0x10000 ? k : '?'); return; }
        vm.yieldNow();
        if (vm.abortRequested) throw ProgramAbort{};
    }
}
void conClear(Vm& vm, NativeCall&) { vm.host().clearText(); }
void keyChar(Vm&, NativeCall& c) { c.ret = c.args[0]; }

// ================================================================ Math
template <double (*F)(double)> void m1(Vm&, NativeCall& c) { c.ret = Value::f64(F(D(c, 0))); }
double mSqrt(double x) { return std::sqrt(x); }
double mSin(double x) { return std::sin(x); }
double mCos(double x) { return std::cos(x); }
double mTan(double x) { return std::tan(x); }
double mAsin(double x) { return std::asin(x); }
double mAcos(double x) { return std::acos(x); }
double mAtan(double x) { return std::atan(x); }
double mSinh(double x) { return std::sinh(x); }
double mCosh(double x) { return std::cosh(x); }
double mTanh(double x) { return std::tanh(x); }
double mExp(double x) { return std::exp(x); }
double mLog(double x) { return std::log(x); }
double mLog10(double x) { return std::log10(x); }
double mLog2(double x) { return std::log2(x); }
double mFloor(double x) { return std::floor(x); }
double mCeiling(double x) { return std::ceil(x); }
double mTruncate(double x) { return std::trunc(x); }
double mCbrt(double x) { return std::cbrt(x); }
void mPow(Vm&, NativeCall& c) { c.ret = Value::f64(std::pow(D(c, 0), D(c, 1))); }
void mAtan2(Vm&, NativeCall& c) { c.ret = Value::f64(std::atan2(D(c, 0), D(c, 1))); }
void mLogB(Vm&, NativeCall& c) { c.ret = Value::f64(std::log(D(c, 0)) / std::log(D(c, 1))); }
void mIeeeRem(Vm&, NativeCall& c) { c.ret = Value::f64(std::remainder(D(c, 0), D(c, 1))); }
void mAbsI(Vm& vm, NativeCall& c) {
    const std::int64_t v = c.args[0].i;
    if (c.args[0].k == VK::I32 && v == INT32_MIN) vm.raise("OverflowException", "Negating the minimum value of a twos complement number is invalid.");
    if (c.args[0].k == VK::I64 && v == INT64_MIN) vm.raise("OverflowException", "Negating the minimum value of a twos complement number is invalid.");
    c.ret = Value::ofInt(c.args[0].k, v < 0 ? -v : v);
}
void mAbsD(Vm&, NativeCall& c) { c.ret = c.args[0].k == VK::F32 ? Value::f32(std::fabs(D(c, 0))) : Value::f64(std::fabs(D(c, 0))); }
template <bool Max> void mMinMax(Vm&, NativeCall& c) {
    const Value& a = c.args[0];
    const Value& b = c.args[1];
    if (isFloat(a.k)) {
        const double x = a.d, y = b.d;
        double r;
        if (std::isnan(x) || std::isnan(y)) r = std::numeric_limits<double>::quiet_NaN();
        else r = Max ? (x > y ? x : y) : (x < y ? x : y);
        c.ret = a.k == VK::F32 ? Value::f32(r) : Value::f64(r);
        return;
    }
    if (a.k == VK::U64) { c.ret = Max ? (a.u > b.u ? a : b) : (a.u < b.u ? a : b); return; }
    c.ret = Max ? (a.i > b.i ? a : b) : (a.i < b.i ? a : b);
}
void mRound(Vm&, NativeCall& c) { c.ret = Value::f64(std::nearbyint(D(c, 0))); }
void mRoundDigits(Vm& vm, NativeCall& c) {
    const std::int64_t n = I(c, 1);
    if (n < 0 || n > 15) argRange(vm, "digits", "Rounding digits must be between 0 and 15, inclusive.");
    const double d = D(c, 0);
    const double p = std::pow(10.0, static_cast<double>(n));
    // arrotondamento bancario sul valore scalato
    double s = d * p;
    if (std::fabs(s) < 1e16) {
        const double fl = std::floor(s);
        const double diff = s - fl;
        if (diff > 0.5) s = fl + 1;
        else if (diff < 0.5) s = fl;
        else s = std::fmod(fl, 2.0) == 0 ? fl : fl + 1;
        c.ret = Value::f64(s / p);
    } else c.ret = Value::f64(d);
}
void mSign(Vm& vm, NativeCall& c) {
    const Value& a = c.args[0];
    if (isFloat(a.k)) {
        if (std::isnan(a.d)) vm.raise("ArithmeticException", "Function does not accept floating point Not-a-Number values.");
        c.ret = Value::i32(a.d > 0 ? 1 : a.d < 0 ? -1 : 0);
        return;
    }
    c.ret = Value::i32(a.i > 0 ? 1 : a.i < 0 ? -1 : 0);
}
void mClamp(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    const Value& lo = c.args[1];
    const Value& hi = c.args[2];
    if (vm.compareValues(lo, hi) > 0) vm.raise("ArgumentException", "'" + vm.format(lo, Dialect::CSharp) + "' cannot be greater than " + vm.format(hi, Dialect::CSharp) + ".");
    c.ret = vm.compareValues(v, lo) < 0 ? lo : vm.compareValues(v, hi) > 0 ? hi : v;
}

// ================================================================ Convert, Parse
std::int64_t toIntChecked(Vm& vm, double d, std::int64_t lo, std::int64_t hi, const char* type) {
    if (std::isnan(d)) vm.raise("OverflowException", std::string("Value was either too large or too small for an ") + type + ".");
    const double r = std::nearbyint(d);
    if (r < static_cast<double>(lo) || r > static_cast<double>(hi)) vm.raise("OverflowException", std::string("Value was either too large or too small for an ") + type + ".");
    return static_cast<std::int64_t>(r);
}
std::string trimWs(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
bool parseIntCs(const std::string& s0, std::int64_t lo, std::int64_t hi, std::int64_t& out, bool& overflow) {
    const std::string s = trimWs(s0);
    overflow = false;
    if (s.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (s[i] == '+' || s[i] == '-') { neg = s[i] == '-'; ++i; }
    if (i >= s.size()) return false;
    std::uint64_t v = 0;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        if (v > (UINT64_MAX - 9) / 10) { overflow = true; v = UINT64_MAX / 2; }
        else v = v * 10 + static_cast<std::uint64_t>(s[i] - '0');
    }
    if (overflow) return false;
    const double asD = neg ? -static_cast<double>(v) : static_cast<double>(v);
    if (neg) {
        if (v > static_cast<std::uint64_t>(-(lo + 1)) + 1u) { overflow = true; return false; }
        out = static_cast<std::int64_t>(0 - v);
    } else {
        if (hi >= 0 && v > static_cast<std::uint64_t>(hi)) { overflow = true; return false; }
        out = static_cast<std::int64_t>(v);
    }
    (void)asD;
    return true;
}
bool parseDoubleCs(const std::string& s0, double& out) {
    const std::string s = trimWs(s0);
    if (s.empty()) return false;
    if (s == "NaN") { out = std::numeric_limits<double>::quiet_NaN(); return true; }
    if (s == "Infinity" || s == "+Infinity" || s == "\xE2\x88\x9E") { out = std::numeric_limits<double>::infinity(); return true; }
    if (s == "-Infinity" || s == "-\xE2\x88\x9E") { out = -std::numeric_limits<double>::infinity(); return true; }
    // cifre, un punto, esponente; niente esadecimale ne' "inf" di C
    std::size_t i = 0;
    if (s[i] == '+' || s[i] == '-') ++i;
    bool digits = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; }
    if (i < s.size() && s[i] == '.') { ++i; while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; } }
    if (!digits) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        bool ed = false;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; ed = true; }
        if (!ed) return false;
    }
    if (i != s.size()) return false;
    return parseDouble(s, 0, out) == s.size();
}
template <VK K> void parseInt(Vm& vm, NativeCall& c) {
    if (c.args[0].isNull()) vm.raise("ArgumentNullException", "Value cannot be null. (Parameter 's')");
    const std::string s = text(vm, c, 0);
    const std::int64_t lo = K == VK::I32 ? INT32_MIN : K == VK::I16 ? INT16_MIN : K == VK::U8 ? 0 : K == VK::I8 ? -128 : K == VK::U32 ? 0 : K == VK::U16 ? 0 : INT64_MIN;
    const std::int64_t hi = K == VK::I32 ? INT32_MAX : K == VK::I16 ? INT16_MAX : K == VK::U8 ? 255 : K == VK::I8 ? 127 : K == VK::U32 ? UINT32_MAX : K == VK::U16 ? 65535 : INT64_MAX;
    std::int64_t v = 0;
    bool ovf = false;
    if (!parseIntCs(s, lo, hi, v, ovf)) {
        if (ovf) vm.raise("OverflowException", std::string("Value was either too large or too small for an ") + (K == VK::I32 ? "Int32" : K == VK::I64 ? "Int64" : "integer") + ".");
        vm.raise("FormatException", "The input string '" + s + "' was not in a correct format.");
    }
    if (v < lo || v > hi) vm.raise("OverflowException", "Value was either too large or too small.");
    c.ret = Value::ofInt(K, v);
}
template <VK K> void tryParseInt(Vm& vm, NativeCall& c) {
    const std::int64_t lo = K == VK::I32 ? INT32_MIN : INT64_MIN, hi = K == VK::I32 ? INT32_MAX : INT64_MAX;
    std::int64_t v = 0;
    bool ovf = false;
    const bool ok = !c.args[0].isNull() && parseIntCs(text(vm, c, 0), lo, hi, v, ovf) && v >= lo && v <= hi;
    vm.storePtr(c.args[1], Value::ofInt(K, ok ? v : 0));
    c.ret = Value::boolean(ok);
}
void parseDoubleV(Vm& vm, NativeCall& c) {
    if (c.args[0].isNull()) vm.raise("ArgumentNullException", "Value cannot be null. (Parameter 's')");
    const std::string s = text(vm, c, 0);
    double d = 0;
    if (!parseDoubleCs(s, d)) vm.raise("FormatException", "The input string '" + s + "' was not in a correct format.");
    c.ret = Value::f64(d);
}
void parseFloatV(Vm& vm, NativeCall& c) { parseDoubleV(vm, c); c.ret = Value::f32(c.ret.d); }
void tryParseDouble(Vm& vm, NativeCall& c) {
    double d = 0;
    const bool ok = !c.args[0].isNull() && parseDoubleCs(text(vm, c, 0), d);
    vm.storePtr(c.args[1], Value::f64(ok ? d : 0));
    c.ret = Value::boolean(ok);
}
void parseBool(Vm& vm, NativeCall& c) {
    std::string s = trimWs(text(vm, c, 0));
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (s == "true") c.ret = Value::boolean(true);
    else if (s == "false") c.ret = Value::boolean(false);
    else vm.raise("FormatException", "String '" + text(vm, c, 0) + "' was not recognized as a valid Boolean.");
}
void tryParseBool(Vm& vm, NativeCall& c) {
    std::string s = c.args[0].isNull() ? std::string() : trimWs(text(vm, c, 0));
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const bool ok = s == "true" || s == "false";
    vm.storePtr(c.args[1], Value::boolean(s == "true"));
    c.ret = Value::boolean(ok);
}
void parseChar(Vm& vm, NativeCall& c) {
    const std::u16string s = ws(vm, c, 0);
    if (s.size() != 1) vm.raise("FormatException", "String must be exactly one character long.");
    c.ret = Value::ofInt(VK::C16, s[0]);
}
void cvToInt32(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    if (isFloat(v.k)) { c.ret = Value::i32(toIntChecked(vm, v.d, INT32_MIN, INT32_MAX, "Int32")); return; }
    if (v.holdsObj() && v.o->kind == OK::StrW) { NativeCall p = c; parseInt<VK::I32>(vm, p); c.ret = p.ret; return; }
    if (v.isNull()) { c.ret = Value::i32(0); return; }
    if (v.k == VK::Bool) { c.ret = Value::i32(v.i ? 1 : 0); return; }
    if (v.i < INT32_MIN || v.i > INT32_MAX || (v.k == VK::U64 && v.u > INT32_MAX)) vm.raise("OverflowException", "Value was either too large or too small for an Int32.");
    c.ret = Value::i32(v.i);
}
void cvToInt64(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    if (isFloat(v.k)) { c.ret = Value::i64(toIntChecked(vm, v.d, INT64_MIN, INT64_MAX, "Int64")); return; }
    if (v.holdsObj() && v.o->kind == OK::StrW) { NativeCall p = c; parseInt<VK::I64>(vm, p); c.ret = p.ret; return; }
    if (v.isNull()) { c.ret = Value::i64(0); return; }
    c.ret = Value::i64(v.i);
}
void cvToDouble(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    if (v.holdsObj() && v.o->kind == OK::StrW) { NativeCall p = c; parseDoubleV(vm, p); c.ret = p.ret; return; }
    if (v.isNull()) { c.ret = Value::f64(0); return; }
    c.ret = Value::f64(v.num());
}
void cvToString(Vm& vm, NativeCall& c) { c.ret = c.args[0].isNull() ? s8(vm, "") : vm.toLangString(c.args[0], Dialect::CSharp); }
void cvToBoolean(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    if (v.holdsObj() && v.o->kind == OK::StrW) { NativeCall p = c; parseBool(vm, p); c.ret = p.ret; return; }
    c.ret = Value::boolean(vm.truthy(v));
}
void cvToChar(Vm& vm, NativeCall& c) {
    const Value& v = c.args[0];
    if (v.holdsObj() && v.o->kind == OK::StrW) { NativeCall p = c; parseChar(vm, p); c.ret = p.ret; return; }
    if (v.i < 0 || v.i > 65535) vm.raise("OverflowException", "Value was either too large or too small for a character.");
    c.ret = Value::ofInt(VK::C16, v.i);
}

// ================================================================ char
template <int Kind> void chIs(Vm&, NativeCall& c) {
    const char16_t ch = static_cast<char16_t>(c.args[0].i);
    bool r = false;
    const bool ascii = ch < 128;
    switch (Kind) {
    case 0: r = ch >= '0' && ch <= '9'; break;
    case 1: r = ascii ? std::isalpha(ch) != 0 : (ch >= 0xC0 && ch != 0xD7 && ch != 0xF7); break;
    case 2: r = (ch >= '0' && ch <= '9') || (ascii ? std::isalpha(ch) != 0 : ch >= 0xC0); break;
    case 3: r = ch == ' ' || (ch >= 9 && ch <= 13) || ch == 0x85 || ch == 0xA0; break;
    case 4: r = ascii ? std::isupper(ch) != 0 : (ch >= 0xC0 && ch <= 0xDE && ch != 0xD7); break;
    case 5: r = ascii ? std::islower(ch) != 0 : (ch >= 0xDF && ch <= 0xFF && ch != 0xF7); break;
    case 6: r = ascii && std::ispunct(ch) && !std::strchr("$+<=>^`|~", static_cast<char>(ch)); break;
    case 7: r = ch < 32 || (ch >= 127 && ch < 160); break;
    case 8: r = ascii && std::strchr("$+<=>^`|~", static_cast<char>(ch)) != nullptr; break;
    default: r = (ascii && std::isalnum(ch)) || ch >= 0xC0; break;
    }
    c.ret = Value::boolean(r);
}
char16_t upper16(char16_t ch) { if (ch < 128) return static_cast<char16_t>(std::toupper(ch)); if (ch >= 0xE0 && ch <= 0xFE && ch != 0xF7) return static_cast<char16_t>(ch - 32); return ch; }
char16_t lower16(char16_t ch) { if (ch < 128) return static_cast<char16_t>(std::tolower(ch)); if (ch >= 0xC0 && ch <= 0xDE && ch != 0xD7) return static_cast<char16_t>(ch + 32); return ch; }
void chToUpper(Vm&, NativeCall& c) { c.ret = Value::ofInt(VK::C16, upper16(static_cast<char16_t>(c.args[0].i))); }
void chToLower(Vm&, NativeCall& c) { c.ret = Value::ofInt(VK::C16, lower16(static_cast<char16_t>(c.args[0].i))); }
void chNumeric(Vm&, NativeCall& c) { const char16_t ch = static_cast<char16_t>(c.args[0].i); c.ret = Value::f64(ch >= '0' && ch <= '9' ? ch - '0' : -1.0); }

// ================================================================ string
const std::pmr::u16string& selfC(Vm& vm, const NativeCall& c) {
    if (c.args[0].isNull()) vm.fault(FaultKind::NullPointer, "riferimento a un oggetto null (string)");
    return strwArg(vm, c.args[0])->s;
}
std::u16string_view sv(Vm& vm, const NativeCall& c) { const auto& s = selfC(vm, c); return std::u16string_view(s.data(), s.size()); }
std::u16string needle(Vm& vm, const NativeCall& c, int i) {
    const Value& v = c.args[i];
    if (v.k == VK::C16) return std::u16string(1, static_cast<char16_t>(v.i));
    return ws(vm, c, i);
}
void strLength(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int64_t>(sv(vm, c).size())); }
void strIndex(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= s.size()) vm.raise("IndexOutOfRangeException", "Index was outside the bounds of the array.");
    c.ret = Value::ofInt(VK::C16, s[static_cast<std::size_t>(i)]);
}
void strSubstring(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    const std::int64_t a = I(c, 1);
    if (a < 0 || static_cast<std::size_t>(a) > s.size()) argRange(vm, "startIndex", "startIndex cannot be larger than length of string.");
    std::int64_t n = static_cast<std::int64_t>(s.size()) - a;
    if (c.argc > 2) {
        n = I(c, 2);
        if (n < 0 || a + n > static_cast<std::int64_t>(s.size())) argRange(vm, "length", "Index and length must refer to a location within the string.");
    }
    c.ret = sw(vm, s.substr(static_cast<std::size_t>(a), static_cast<std::size_t>(n)));
}
template <bool Last> void strIndexOf(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    const std::u16string n = needle(vm, c, 1);
    std::size_t r;
    if (c.argc > 2) {
        const std::int64_t from = I(c, 2);
        if (from < 0 || static_cast<std::size_t>(from) > s.size()) argRange(vm, "startIndex", "Index was out of range. Must be non-negative and less than or equal to the size of the collection.");
        r = Last ? s.rfind(n, static_cast<std::size_t>(from)) : s.find(n, static_cast<std::size_t>(from));
    } else r = Last ? s.rfind(n) : s.find(n);
    c.ret = Value::i32(r == std::u16string::npos ? -1 : static_cast<std::int64_t>(r));
}
void strContains(Vm& vm, NativeCall& c) { c.ret = Value::boolean(sv(vm, c).find(needle(vm, c, 1)) != std::u16string::npos); }
void strStartsWith(Vm& vm, NativeCall& c) { const auto s = sv(vm, c); const std::u16string n = needle(vm, c, 1); c.ret = Value::boolean(s.substr(0, n.size()) == n); }
void strEndsWith(Vm& vm, NativeCall& c) { const auto s = sv(vm, c); const std::u16string n = needle(vm, c, 1); c.ret = Value::boolean(s.size() >= n.size() && s.substr(s.size() - n.size()) == n); }
void strToUpper(Vm& vm, NativeCall& c) { std::u16string s(sv(vm, c)); for (auto& ch : s) ch = upper16(ch); c.ret = sw(vm, s); }
void strToLower(Vm& vm, NativeCall& c) { std::u16string s(sv(vm, c)); for (auto& ch : s) ch = lower16(ch); c.ret = sw(vm, s); }
bool isWs16(char16_t ch) { return ch == ' ' || (ch >= 9 && ch <= 13) || ch == 0x85 || ch == 0xA0; }
template <int Side> void strTrim(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    std::u16string set;
    for (int i = 1; i < c.argc; ++i) {
        const Value& v = c.args[i];
        if (v.k == VK::C16) set += static_cast<char16_t>(v.i);
        else if (v.holdsObj() && v.o->kind == OK::Array) { ArrayObj* a = static_cast<ArrayObj*>(v.o); for (std::uint32_t k = 0; k < a->count; ++k) set += static_cast<char16_t>(a->get(k).i); }
    }
    auto trim = [&](char16_t ch) { return set.empty() ? isWs16(ch) : set.find(ch) != std::u16string::npos; };
    std::size_t a = 0, b = s.size();
    if (Side != 2) while (a < b && trim(s[a])) ++a;
    if (Side != 1) while (b > a && trim(s[b - 1])) --b;
    c.ret = sw(vm, s.substr(a, b - a));
}
void strReplace(Vm& vm, NativeCall& c) {
    std::u16string s(sv(vm, c));
    const std::u16string a = needle(vm, c, 1);
    const std::u16string b = c.args[2].isNull() ? std::u16string() : needle(vm, c, 2);
    if (a.empty()) vm.raise("ArgumentException", "String cannot be of zero length. (Parameter 'oldValue')");
    std::u16string out;
    std::size_t p = 0;
    for (;;) {
        const std::size_t q = s.find(a, p);
        if (q == std::u16string::npos) { out += s.substr(p); break; }
        out += s.substr(p, q - p);
        out += b;
        p = q + a.size();
    }
    c.ret = sw(vm, out);
}
void strSplit(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    std::vector<std::u16string> seps;
    bool removeEmpty = false, trim = false;
    for (int i = 1; i < c.argc; ++i) {
        const Value& v = c.args[i];
        if (v.k == VK::C16) seps.push_back(std::u16string(1, static_cast<char16_t>(v.i)));
        else if (v.k == VK::I32) { removeEmpty = (v.i & 1) != 0; trim = (v.i & 2) != 0; }   // StringSplitOptions: RemoveEmptyEntries 1, TrimEntries 2
        else if (v.holdsObj() && v.o->kind == OK::StrW) seps.push_back(std::u16string(static_cast<StrWObj*>(v.o)->s));
        else if (v.holdsObj() && v.o->kind == OK::Array) {
            ArrayObj* a = static_cast<ArrayObj*>(v.o);
            for (std::uint32_t k = 0; k < a->count; ++k) {
                const Value e = a->get(k);
                if (e.k == VK::C16) seps.push_back(std::u16string(1, static_cast<char16_t>(e.i)));
                else if (e.holdsObj()) seps.push_back(vm.utf16(e));
            }
        }
    }
    std::vector<std::u16string> parts;
    if (seps.empty()) {
        // nessun separatore: gli spazi
        std::size_t p = 0;
        for (std::size_t i = 0; i <= s.size(); ++i) if (i == s.size() || isWs16(s[i])) { parts.push_back(std::u16string(s.substr(p, i - p))); p = i + 1; }
    } else {
        std::size_t p = 0, i = 0;
        while (i <= s.size()) {
            bool hit = false;
            std::size_t len = 0;
            if (i < s.size()) for (const auto& sep : seps) if (!sep.empty() && s.compare(i, sep.size(), sep) == 0) { hit = true; len = sep.size(); break; }
            if (i == s.size() || hit) {
                parts.push_back(std::u16string(s.substr(p, i - p)));
                if (i == s.size()) break;
                i += len;
                p = i;
                continue;
            }
            ++i;
        }
    }
    if (trim)
        for (auto& x : parts) {
            std::size_t b = 0, e = x.size();
            while (b < e && isWs16(x[b])) ++b;
            while (e > b && isWs16(x[e - 1])) --e;
            x = x.substr(b, e - b);
        }
    if (removeEmpty) parts.erase(std::remove_if(parts.begin(), parts.end(), [](const std::u16string& x) { return x.empty(); }), parts.end());
    ArrayObj* a = vm.newArray(vm.program().csString, static_cast<std::uint32_t>(parts.size()));
    vm.root(a);
    for (std::size_t i = 0; i < parts.size(); ++i) a->set(static_cast<std::uint32_t>(i), sw(vm, parts[i]));
    c.ret = Value::ref(a);
}
void strInsert(Vm& vm, NativeCall& c) {
    std::u16string s(sv(vm, c));
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) > s.size()) argRange(vm, "startIndex", "Index was out of range. Must be non-negative and less than or equal to the size of the collection.");
    s.insert(static_cast<std::size_t>(i), ws(vm, c, 2));
    c.ret = sw(vm, s);
}
void strRemove(Vm& vm, NativeCall& c) {
    std::u16string s(sv(vm, c));
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) > s.size()) argRange(vm, "startIndex", "startIndex must be less than length of string.");
    std::int64_t n = static_cast<std::int64_t>(s.size()) - i;
    if (c.argc > 2) { n = I(c, 2); if (n < 0 || i + n > static_cast<std::int64_t>(s.size())) argRange(vm, "count", "Index and count must refer to a location within the string."); }
    s.erase(static_cast<std::size_t>(i), static_cast<std::size_t>(n));
    c.ret = sw(vm, s);
}
template <bool Left> void strPad(Vm& vm, NativeCall& c) {
    std::u16string s(sv(vm, c));
    const std::int64_t w = I(c, 1);
    if (w < 0) argRange(vm, "totalWidth", "Non-negative number required.");
    const char16_t ch = c.argc > 2 ? static_cast<char16_t>(c.args[2].i) : u' ';
    if (static_cast<std::size_t>(w) > s.size()) {
        const std::u16string pad(static_cast<std::size_t>(w) - s.size(), ch);
        s = Left ? pad + s : s + pad;
    }
    c.ret = sw(vm, s);
}
void strEquals(Vm& vm, NativeCall& c) { c.ret = Value::boolean(!c.args[1].isNull() && sv(vm, c) == vm.utf16(c.args[1])); }
// string.Equals(a, b): statico, anche con null
void strEqualsStatic(Vm& vm, NativeCall& c) {
    const bool an = c.args[0].isNull(), bn = c.args[1].isNull();
    c.ret = Value::boolean(an || bn ? an && bn : vm.utf16(c.args[0]) == vm.utf16(c.args[1]));
}
// double.IsNaN, IsInfinity, IsPositiveInfinity, IsNegativeInfinity, IsFinite
template <int K> void dblIs(Vm&, NativeCall& c) {
    const double d = D(c, 0);
    bool r = false;
    switch (K) {
    case 0: r = std::isnan(d); break;
    case 1: r = std::isinf(d); break;
    case 2: r = std::isinf(d) && d > 0; break;
    case 3: r = std::isinf(d) && d < 0; break;
    default: r = std::isfinite(d); break;
    }
    c.ret = Value::boolean(r);
}
void strCompareTo(Vm& vm, NativeCall& c) { c.ret = Value::i32(c.args[1].isNull() ? 1 : vm.compareValues(c.args[0], c.args[1])); }
void strToCharArray(Vm& vm, NativeCall& c) {
    const auto s = sv(vm, c);
    ArrayObj* a = vm.newArray(vm.program().prim[static_cast<int>(TK::C16)], static_cast<std::uint32_t>(s.size()));
    for (std::size_t i = 0; i < s.size(); ++i) a->set(static_cast<std::uint32_t>(i), Value::ofInt(VK::C16, s[i]));
    c.ret = Value::ref(a);
}
void strIsNullOrEmpty(Vm& vm, NativeCall& c) { c.ret = Value::boolean(c.args[0].isNull() || vm.utf16(c.args[0]).empty()); }
void strIsNullOrWhiteSpace(Vm& vm, NativeCall& c) {
    if (c.args[0].isNull()) { c.ret = Value::boolean(true); return; }
    const std::u16string s = vm.utf16(c.args[0]);
    c.ret = Value::boolean(std::all_of(s.begin(), s.end(), isWs16));
}
std::vector<Value> itemsOf(Vm& vm, const Value& v) {
    std::vector<Value> out;
    if (v.isNull()) vm.raise("ArgumentNullException", "Value cannot be null. (Parameter 'values')");
    Obj* o = objArg(vm, v);
    if (o->kind == OK::Array) { ArrayObj* a = static_cast<ArrayObj*>(o); for (std::uint32_t i = 0; i < a->count; ++i) out.push_back(a->get(i)); }
    else if (o->kind == OK::Vec) { for (const Value& x : static_cast<VecObj*>(o)->items) out.push_back(x); }
    else if (o->kind == OK::Map) { MapObj* m = static_cast<MapObj*>(o); for (std::uint32_t cell : mapCells(m)) out.push_back(m->isSet ? m->keys[cell] : Value::ref(vm.newPair(nullptr, m->keys[cell], m->vals[cell]))); }
    else if (o->kind == OK::StrW) { for (char16_t ch : static_cast<StrWObj*>(o)->s) out.push_back(Value::ofInt(VK::C16, ch)); }
    else vm.fault(FaultKind::Argument, "attesa una collezione");
    return out;
}
void strJoin(Vm& vm, NativeCall& c) {
    const std::u16string sep = c.args[0].k == VK::C16 ? std::u16string(1, static_cast<char16_t>(c.args[0].i)) : ws(vm, c, 0);
    std::u16string out;
    std::vector<Value> items;
    if (c.argc == 2) items = itemsOf(vm, c.args[1]);
    else for (int i = 1; i < c.argc; ++i) items.push_back(c.args[i]);
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += sep;
        if (!items[i].isNull()) out += utf8To16(vm.format(items[i], Dialect::CSharp));
    }
    c.ret = sw(vm, out);
}
void strConcat(Vm& vm, NativeCall& c) {
    std::u16string out;
    for (int i = 0; i < c.argc; ++i) if (!c.args[i].isNull()) out += utf8To16(vm.format(c.args[i], Dialect::CSharp));
    c.ret = sw(vm, out);
}
void strFormat(Vm& vm, NativeCall& c) { c.ret = s8(vm, formatComposite(vm, text(vm, c, 0), c.args + 1, c.argc - 1)); }
void strCompare(Vm& vm, NativeCall& c) { c.ret = Value::i32(vm.compareValues(c.args[0], c.args[1])); }
// string.CompareOrdinal: differenza delle prime unita' UTF-16 diverse, poi delle lunghezze (come .NET)
void strCompareOrdinal(Vm& vm, NativeCall& c) {
    const bool an = c.args[0].isNull(), bn = c.args[1].isNull();
    if (an || bn) { c.ret = Value::i32(an && bn ? 0 : an ? -1 : 1); return; }
    const std::u16string a = vm.utf16(c.args[0]), b = vm.utf16(c.args[1]);
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] != b[i]) { c.ret = Value::i32(static_cast<int>(a[i]) - static_cast<int>(b[i])); return; }
    c.ret = Value::i32(static_cast<int>(a.size()) - static_cast<int>(b.size()));
}
void strCtorFill(Vm& vm, NativeCall& c) { c.ret = sw(vm, std::u16string(static_cast<std::size_t>(std::max<std::int64_t>(0, I(c, 1))), static_cast<char16_t>(c.args[0].i))); }
void strCtorChars(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    std::u16string s;
    for (std::uint32_t i = 0; i < a->count; ++i) s += static_cast<char16_t>(a->get(i).i);
    c.ret = sw(vm, s);
}

// ================================================================ StringBuilder
std::pmr::u16string& sb(Vm& vm, const NativeCall& c) { return sbArg(vm, c.args[0])->s; }
void sbCtor(Vm& vm, NativeCall& c) { if (c.argc > 1 && c.args[1].holdsObj()) { const std::u16string s = vm.utf16(c.args[1]); sb(vm, c).assign(s.data(), s.size()); } }
void sbAppend(Vm& vm, NativeCall& c) {
    const Value& v = c.args[1];
    std::u16string t = v.k == VK::C16 ? std::u16string(1, static_cast<char16_t>(v.i)) : v.isNull() ? std::u16string() : utf8To16(vm.format(v, Dialect::CSharp));
    sb(vm, c).append(t.data(), t.size());
    c.ret = c.args[0];
}
// AppendFormat("{0,5:F2}", ...): formato composito come string.Format
void sbAppendFormat(Vm& vm, NativeCall& c) {
    const std::u16string t = utf8To16(formatComposite(vm, text(vm, c, 1), c.args + 2, c.argc - 2));
    sb(vm, c).append(t.data(), t.size());
    c.ret = c.args[0];
}
void sbAppendLine(Vm& vm, NativeCall& c) {
    if (c.argc > 1) { NativeCall a = c; sbAppend(vm, a); }
    sb(vm, c).push_back(u'\n');
    c.ret = c.args[0];
}
void sbInsert(Vm& vm, NativeCall& c) {
    auto& s = sb(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) > s.size()) argRange(vm, "index", "Index was out of range. Must be non-negative and less than or equal to the size of the collection.");
    const Value& v = c.args[2];
    const std::u16string t = v.k == VK::C16 ? std::u16string(1, static_cast<char16_t>(v.i)) : utf8To16(vm.format(v, Dialect::CSharp));
    s.insert(static_cast<std::size_t>(i), t.data(), t.size());
    c.ret = c.args[0];
}
void sbRemove(Vm& vm, NativeCall& c) {
    auto& s = sb(vm, c);
    const std::int64_t i = I(c, 1), n = I(c, 2);
    if (i < 0 || n < 0 || static_cast<std::size_t>(i + n) > s.size()) argRange(vm, "length", "Index was out of range. Must be non-negative and less than the size of the collection.");
    s.erase(static_cast<std::size_t>(i), static_cast<std::size_t>(n));
    c.ret = c.args[0];
}
void sbReplace(Vm& vm, NativeCall& c) {
    auto& s = sb(vm, c);
    const std::u16string a = needle(vm, c, 1), b = needle(vm, c, 2);
    if (a.empty()) vm.raise("ArgumentException", "String cannot be of zero length. (Parameter 'oldValue')");
    std::u16string in(s.data(), s.size()), out;
    std::size_t p = 0;
    for (;;) { const std::size_t q = in.find(a, p); if (q == std::u16string::npos) { out += in.substr(p); break; } out += in.substr(p, q - p); out += b; p = q + a.size(); }
    s.assign(out.data(), out.size());
    c.ret = c.args[0];
}
void sbClear(Vm& vm, NativeCall& c) { sb(vm, c).clear(); c.ret = c.args[0]; }
void sbToString(Vm& vm, NativeCall& c) { const auto& s = sb(vm, c); c.ret = sw(vm, std::u16string_view(s.data(), s.size())); }
void sbLength(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int64_t>(sb(vm, c).size())); }
void sbGet(Vm& vm, NativeCall& c) {
    const auto& s = sb(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= s.size()) vm.raise("IndexOutOfRangeException", "Index was outside the bounds of the array.");
    c.ret = Value::ofInt(VK::C16, s[static_cast<std::size_t>(i)]);
}
void sbSet(Vm& vm, NativeCall& c) {
    auto& s = sb(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= s.size()) argRange(vm, "index", "Index was out of range. Must be non-negative and less than the size of the collection.");
    s[static_cast<std::size_t>(i)] = static_cast<char16_t>(c.args[2].i);
}

// ================================================================ List<T>
VecObj* lst(Vm& vm, const NativeCall& c) {
    if (c.args[0].isNull()) vm.fault(FaultKind::NullPointer, "riferimento a un oggetto null");
    return vecArg(vm, c.args[0]);
}
TypeP elemOf(const NativeCall& c) { return typeArg(c, 0); }
void touch(VecObj* v) { ++v->version; }
[[noreturn]] void listIndex(Vm& vm) { argRange(vm, "index", "Index was out of range. Must be non-negative and less than the size of the collection."); }
void lCtor(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    if (c.argc > 1) {
        const Value& a = c.args[1];
        if (isInt(a.k)) { if (a.i < 0) argRange(vm, "capacity", "Non-negative number required."); v->items.reserve(static_cast<std::size_t>(std::min<std::int64_t>(a.i, 1 << 16))); return; }
        for (const Value& x : itemsOf(vm, a)) v->items.push_back(stored(vm, x, elemOf(c)));
    }
}
void lAdd(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); v->items.push_back(stored(vm, c.args[1], elemOf(c))); touch(v); }
void lAddRange(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); for (const Value& x : itemsOf(vm, c.args[1])) v->items.push_back(stored(vm, x, elemOf(c))); touch(v); }
void lInsert(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) > v->items.size()) argRange(vm, "index", "Index must be within the bounds of the List.");
    v->items.insert(v->items.begin() + i, stored(vm, c.args[2], elemOf(c)));
    touch(v);
}
std::int64_t indexOf(Vm& vm, VecObj* v, const Value& x, bool last) {
    if (last) { for (std::size_t i = v->items.size(); i-- > 0;) if (vm.equalValues(v->items[i], x)) return static_cast<std::int64_t>(i); return -1; }
    for (std::size_t i = 0; i < v->items.size(); ++i) if (vm.equalValues(v->items[i], x)) return static_cast<std::int64_t>(i);
    return -1;
}
void lRemove(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = indexOf(vm, v, c.args[1], false);
    if (i >= 0) { v->items.erase(v->items.begin() + i); touch(v); }
    c.ret = Value::boolean(i >= 0);
}
void lRemoveAt(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= v->items.size()) listIndex(vm);
    v->items.erase(v->items.begin() + i);
    touch(v);
}
void lRemoveRange(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1), n = I(c, 2);
    if (i < 0 || n < 0) argRange(vm, i < 0 ? "index" : "count", "Non-negative number required.");
    if (static_cast<std::size_t>(i + n) > v->items.size()) vm.raise("ArgumentException", "Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.");
    v->items.erase(v->items.begin() + i, v->items.begin() + i + n);
    touch(v);
}
void lRemoveAll(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    std::vector<Value> keep;
    std::int64_t removed = 0;
    const std::vector<Value> items(v->items.begin(), v->items.end());
    for (const Value& x : items) { Value a = x; if (vm.truthy(vm.invoke(c.args[1], &a, 1))) ++removed; else keep.push_back(x); }
    v->items.assign(keep.begin(), keep.end());
    touch(v);
    c.ret = Value::i32(removed);
}
void lClear(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); v->items.clear(); touch(v); }
void lContains(Vm& vm, NativeCall& c) { c.ret = Value::boolean(indexOf(vm, lst(vm, c), c.args[1], false) >= 0); }
void lIndexOf(Vm& vm, NativeCall& c) { c.ret = Value::i32(indexOf(vm, lst(vm, c), c.args[1], false)); }
void lLastIndexOf(Vm& vm, NativeCall& c) { c.ret = Value::i32(indexOf(vm, lst(vm, c), c.args[1], true)); }
template <int Kind> void lFind(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::vector<Value> items(v->items.begin(), v->items.end());
    const std::size_t n = items.size();
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t i = Kind == 2 || Kind == 4 ? n - 1 - k : k;
        Value a = items[i];
        if (vm.truthy(vm.invoke(c.args[1], &a, 1))) {
            if (Kind == 0 || Kind == 2) c.ret = items[i];
            else if (Kind == 1 || Kind == 4) c.ret = Value::i32(static_cast<std::int64_t>(i));
            else c.ret = Value::boolean(true);
            return;
        }
    }
    if (Kind == 0 || Kind == 2) c.ret = vm.defaultValue(elemOf(c));
    else if (Kind == 1 || Kind == 4) c.ret = Value::i32(-1);
    else c.ret = Value::boolean(false);
}
void lFindAll(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    VecObj* out = vm.newVec(v->type);
    vm.root(out);
    const std::vector<Value> items(v->items.begin(), v->items.end());
    for (const Value& x : items) { Value a = x; if (vm.truthy(vm.invoke(c.args[1], &a, 1))) out->items.push_back(x); }
    c.ret = Value::ref(out);
}
void lTrueForAll(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::vector<Value> items(v->items.begin(), v->items.end());
    for (const Value& x : items) { Value a = x; if (!vm.truthy(vm.invoke(c.args[1], &a, 1))) { c.ret = Value::boolean(false); return; } }
    c.ret = Value::boolean(true);
}
void lCount(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int64_t>(lst(vm, c)->items.size())); }
void lCapacity(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int64_t>(lst(vm, c)->items.capacity())); }
void lGet(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= v->items.size()) listIndex(vm);
    c.ret = v->items[static_cast<std::size_t>(i)];
}
void lSet(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1);
    if (i < 0 || static_cast<std::size_t>(i) >= v->items.size()) listIndex(vm);
    v->items[static_cast<std::size_t>(i)] = stored(vm, c.args[2], elemOf(c));
    touch(v);
}
void lSort(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    std::vector<Value> items(v->items.begin(), v->items.end());
    const Value* cmp = c.argc > 1 ? &c.args[1] : nullptr;
    stableSort(vm, items, [&](const Value& a, const Value& b) { return lessWith(vm, cmp, a, b, true); });
    v->items.assign(items.begin(), items.end());
    touch(v);
}
void lReverse(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); std::reverse(v->items.begin(), v->items.end()); touch(v); }
void lToArray(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    ArrayObj* a = vm.newArray(elemOf(c), static_cast<std::uint32_t>(v->items.size()));
    vm.root(a);
    const bool rev = v->type && v->type->nk == NK::CsStack;
    for (std::size_t i = 0; i < v->items.size(); ++i) a->set(static_cast<std::uint32_t>(i), v->items[rev ? v->items.size() - 1 - i : i]);
    c.ret = Value::ref(a);
}
void lForEach(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::uint32_t ver = v->version;
    for (std::size_t i = 0; i < v->items.size(); ++i) {
        Value a = v->items[i];
        vm.invoke(c.args[1], &a, 1);
        if (v->version != ver) vm.raise("InvalidOperationException", "Collection was modified; enumeration operation may not execute.");
    }
}
void lGetRange(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    const std::int64_t i = I(c, 1), n = I(c, 2);
    if (i < 0 || n < 0) argRange(vm, i < 0 ? "index" : "count", "Non-negative number required.");
    if (static_cast<std::size_t>(i + n) > v->items.size()) vm.raise("ArgumentException", "Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.");
    VecObj* out = vm.newVec(v->type);
    vm.root(out);
    out->items.assign(v->items.begin() + i, v->items.begin() + i + n);
    c.ret = Value::ref(out);
}
void lBinarySearch(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    std::int64_t lo = 0, hi = static_cast<std::int64_t>(v->items.size()) - 1;
    while (lo <= hi) {
        const std::int64_t mid = lo + (hi - lo) / 2;
        const int r = vm.compareValues(v->items[static_cast<std::size_t>(mid)], c.args[1]);
        if (r == 0) { c.ret = Value::i32(mid); return; }
        if (r < 0) lo = mid + 1; else hi = mid - 1;
    }
    c.ret = Value::i32(~lo);
}
// Queue e Stack
void qEnqueue(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); v->items.push_back(stored(vm, c.args[1], elemOf(c))); touch(v); }
void qDequeue(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    if (v->items.empty()) vm.raise("InvalidOperationException", "Queue empty.");
    c.ret = v->items.front();
    v->items.erase(v->items.begin());
    touch(v);
}
void qPeek(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); if (v->items.empty()) vm.raise("InvalidOperationException", "Queue empty."); c.ret = v->items.front(); }
void stPop(Vm& vm, NativeCall& c) {
    VecObj* v = lst(vm, c);
    if (v->items.empty()) vm.raise("InvalidOperationException", "Stack empty.");
    c.ret = v->items.back();
    v->items.pop_back();
    touch(v);
}
void stPeek(Vm& vm, NativeCall& c) { VecObj* v = lst(vm, c); if (v->items.empty()) vm.raise("InvalidOperationException", "Stack empty."); c.ret = v->items.back(); }
template <bool Stack> void qTryX(Vm& vm, NativeCall& c, bool remove) {
    VecObj* v = lst(vm, c);
    if (v->items.empty()) { vm.storePtr(c.args[1], vm.defaultValue(elemOf(c))); c.ret = Value::boolean(false); return; }
    const Value x = Stack ? v->items.back() : v->items.front();
    if (remove) { if (Stack) v->items.pop_back(); else v->items.erase(v->items.begin()); touch(v); }
    vm.storePtr(c.args[1], x);
    c.ret = Value::boolean(true);
}
void qTryDequeue(Vm& vm, NativeCall& c) { qTryX<false>(vm, c, true); }
void qTryPeek(Vm& vm, NativeCall& c) { qTryX<false>(vm, c, false); }
void stTryPop(Vm& vm, NativeCall& c) { qTryX<true>(vm, c, true); }
void stTryPeek(Vm& vm, NativeCall& c) { qTryX<true>(vm, c, false); }

// ================================================================ Dictionary e HashSet
MapObj* dict(Vm& vm, const NativeCall& c) {
    if (c.args[0].isNull()) vm.fault(FaultKind::NullPointer, "riferimento a un oggetto null");
    return mapArg(vm, c.args[0]);
}
void checkKey(Vm& vm, const Value& k) { if (k.isNull()) vm.raise("ArgumentNullException", "Value cannot be null. (Parameter 'key')"); }
void dCtor(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    if (c.argc > 1 && c.args[1].holdsObj()) {
        Obj* o = objArg(vm, c.args[1]);
        if (o->kind == OK::Map) {
            MapObj* src = static_cast<MapObj*>(o);
            for (std::uint32_t cell : mapCells(src)) mapInsert(vm, m, src->keys[cell], src->vals[cell], nullptr);
        } else for (const Value& x : itemsOf(vm, c.args[1])) mapInsert(vm, m, stored(vm, x, typeArg(c, 0)), Value(), nullptr);
    }
}
void dAdd(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    checkKey(vm, c.args[1]);
    const Value k = stored(vm, c.args[1], typeArg(c, 0));
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, c.args[2], typeArg(c, 1));
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool ins = false;
    mapInsert(vm, m, k, v, &ins);
    if (!ins) vm.raise("ArgumentException", "An item with the same key has already been added. Key: " + vm.format(k, Dialect::CSharp));
}
void dTryAdd(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    checkKey(vm, c.args[1]);
    bool ins = false;
    mapInsert(vm, m, stored(vm, c.args[1], typeArg(c, 0)), stored(vm, c.args[2], typeArg(c, 1)), &ins);
    c.ret = Value::boolean(ins);
}
void dGet(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    checkKey(vm, c.args[1]);
    const std::int32_t f = mapFind(vm, m, c.args[1]);
    if (f < 0) vm.raise("KeyNotFoundException", "The given key '" + vm.format(c.args[1], Dialect::CSharp) + "' was not present in the dictionary.");
    c.ret = m->vals[static_cast<std::size_t>(f)];
}
void dSet(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    checkKey(vm, c.args[1]);
    const Value k = stored(vm, c.args[1], typeArg(c, 0));
    vm.root(k.holdsObj() ? k.o : nullptr);
    const Value v = stored(vm, c.args[2], typeArg(c, 1));
    vm.root(v.holdsObj() ? v.o : nullptr);
    bool ins = false;
    const std::uint32_t cell = mapInsert(vm, m, k, v, &ins);
    if (!ins) { m->vals[cell] = v; ++m->version; }
}
void dContainsKey(Vm& vm, NativeCall& c) { checkKey(vm, c.args[1]); c.ret = Value::boolean(mapFind(vm, dict(vm, c), c.args[1]) >= 0); }
void dContainsValue(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    for (std::uint32_t cell : mapCells(m)) if (vm.equalValues(m->vals[cell], c.args[1])) { c.ret = Value::boolean(true); return; }
    c.ret = Value::boolean(false);
}
void dTryGetValue(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    checkKey(vm, c.args[1]);
    const std::int32_t f = mapFind(vm, m, c.args[1]);
    vm.storePtr(c.args[2], f >= 0 ? m->vals[static_cast<std::size_t>(f)] : vm.defaultValue(typeArg(c, 1)));
    c.ret = Value::boolean(f >= 0);
}
void dGetOrDefault(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    const std::int32_t f = mapFind(vm, m, c.args[1]);
    c.ret = f >= 0 ? m->vals[static_cast<std::size_t>(f)] : (c.argc > 2 ? c.args[2] : vm.defaultValue(typeArg(c, 1)));
}
void dRemove(Vm& vm, NativeCall& c) { checkKey(vm, c.args[1]); c.ret = Value::boolean(mapErase(vm, dict(vm, c), c.args[1])); }
void dCount(Vm& vm, NativeCall& c) { c.ret = Value::i32(dict(vm, c)->count); }
void dClear(Vm& vm, NativeCall& c) { mapClear(dict(vm, c)); }
template <bool Keys> void dKeys(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    VecObj* out = vm.newVec(nullptr);
    vm.root(out);
    for (std::uint32_t cell : mapCells(m)) out->items.push_back(Keys ? m->keys[cell] : m->vals[cell]);
    c.ret = Value::ref(out);
}
void hsAdd(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    bool ins = false;
    const Value k = stored(vm, c.args[1], typeArg(c, 0));
    vm.root(k.holdsObj() ? k.o : nullptr);
    mapInsert(vm, m, k, Value(), &ins);
    c.ret = Value::boolean(ins);
}
void hsContains(Vm& vm, NativeCall& c) { c.ret = Value::boolean(mapFind(vm, dict(vm, c), c.args[1]) >= 0); }
void hsRemove(Vm& vm, NativeCall& c) { c.ret = Value::boolean(mapErase(vm, dict(vm, c), c.args[1])); }
void hsUnion(Vm& vm, NativeCall& c) { MapObj* m = dict(vm, c); for (const Value& x : itemsOf(vm, c.args[1])) mapInsert(vm, m, stored(vm, x, typeArg(c, 0)), Value(), nullptr); }
void hsIntersect(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    MapObj* other = vm.newMap(m->type);
    vm.root(other);
    other->mode = 1;
    other->isSet = true;
    for (const Value& x : itemsOf(vm, c.args[1])) mapInsert(vm, other, x, Value(), nullptr);
    for (std::uint32_t cell : mapCells(m)) if (mapFind(vm, other, m->keys[cell]) < 0) mapEraseCell(vm, m, cell);
}
void hsExcept(Vm& vm, NativeCall& c) { MapObj* m = dict(vm, c); for (const Value& x : itemsOf(vm, c.args[1])) mapErase(vm, m, x); }
void hsSubset(Vm& vm, NativeCall& c) {
    MapObj* m = dict(vm, c);
    MapObj* other = vm.newMap(m->type);
    vm.root(other);
    other->mode = 1;
    other->isSet = true;
    for (const Value& x : itemsOf(vm, c.args[1])) mapInsert(vm, other, x, Value(), nullptr);
    for (std::uint32_t cell : mapCells(m)) if (mapFind(vm, other, m->keys[cell]) < 0) { c.ret = Value::boolean(false); return; }
    c.ret = Value::boolean(true);
}

// ================================================================ Random (.NET, generatore sottrattivo di Knuth)
constexpr std::int32_t MBIG = 2147483647, MSEED = 161803398;
void rndSeed(NativeObj* r, std::int32_t seed) {
    r->state.assign(58, 0);
    auto& sa = r->state;
    std::int32_t subtraction = seed == INT32_MIN ? INT32_MAX : (seed < 0 ? -seed : seed);
    std::int32_t mj = MSEED - subtraction;
    sa[55] = mj;
    std::int32_t mk = 1;
    for (int i = 1; i < 55; ++i) {
        const int ii = (21 * i) % 55;
        sa[static_cast<std::size_t>(ii)] = mk;
        mk = mj - mk;
        if (mk < 0) mk += MBIG;
        mj = static_cast<std::int32_t>(sa[static_cast<std::size_t>(ii)]);
    }
    for (int k = 1; k < 5; ++k) {
        for (int i = 1; i < 56; ++i) {
            std::int64_t v = sa[static_cast<std::size_t>(i)] - sa[static_cast<std::size_t>(1 + (i + 30) % 55)];
            v = static_cast<std::int32_t>(v);
            if (v < 0) v += MBIG;
            sa[static_cast<std::size_t>(i)] = v;
        }
    }
    sa[56] = 0;
    sa[57] = 21;
}
std::int32_t rndInternal(NativeObj* r) {
    auto& sa = r->state;
    std::int64_t inext = sa[56], inextp = sa[57];
    if (++inext >= 56) inext = 1;
    if (++inextp >= 56) inextp = 1;
    std::int64_t ret = sa[static_cast<std::size_t>(inext)] - sa[static_cast<std::size_t>(inextp)];
    ret = static_cast<std::int32_t>(ret);
    if (ret == MBIG) --ret;
    if (ret < 0) ret += MBIG;
    sa[static_cast<std::size_t>(inext)] = ret;
    sa[56] = inext;
    sa[57] = inextp;
    return static_cast<std::int32_t>(ret);
}
double rndSample(NativeObj* r) { return rndInternal(r) * (1.0 / MBIG); }
NativeObj* rnd(Vm& vm, const NativeCall& c) {
    NativeObj* r = nativeArg(vm, c.args[0]);
    if (r->state.size() != 58) rndSeed(r, static_cast<std::int32_t>(vm.host().millis() * 2654435761u));
    return r;
}
void rCtor(Vm& vm, NativeCall& c) {
    NativeObj* r = nativeArg(vm, c.args[0]);
    rndSeed(r, c.argc > 1 ? I32(c, 1) : static_cast<std::int32_t>(vm.host().millis() * 2654435761u + 12345u));
}
void rNext(Vm& vm, NativeCall& c) {
    NativeObj* r = rnd(vm, c);
    if (c.argc == 1) { c.ret = Value::i32(rndInternal(r)); return; }
    if (c.argc == 2) {
        const std::int64_t mx = I(c, 1);
        if (mx < 0) argRange(vm, "maxValue", "'maxValue' must be greater than or equal to zero.");
        c.ret = Value::i32(static_cast<std::int64_t>(rndSample(r) * static_cast<double>(mx)));
        return;
    }
    const std::int64_t mn = I(c, 1), mx = I(c, 2);
    if (mn > mx) argRange(vm, "minValue", "'minValue' cannot be greater than maxValue.");
    const std::int64_t range = mx - mn;
    if (range <= INT32_MAX) { c.ret = Value::i32(static_cast<std::int64_t>(rndSample(r) * static_cast<double>(range)) + mn); return; }
    // intervallo grande (GetSampleForLargeRange)
    std::int32_t result = rndInternal(r);
    if (rndInternal(r) % 2 == 0) result = -result;
    double d = result;
    d += (INT32_MAX - 1);
    d /= 2.0 * INT32_MAX - 1;
    c.ret = Value::i32(static_cast<std::int64_t>(d * static_cast<double>(range)) + mn);
}
void rNextDouble(Vm& vm, NativeCall& c) { c.ret = Value::f64(rndSample(rnd(vm, c))); }

// ================================================================ Array
void arrSort(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    std::vector<Value> items;
    for (std::uint32_t i = 0; i < a->count; ++i) items.push_back(a->get(i));
    const Value* cmp = c.argc > 1 ? &c.args[1] : nullptr;
    stableSort(vm, items, [&](const Value& x, const Value& y) { return lessWith(vm, cmp, x, y, true); });
    for (std::uint32_t i = 0; i < a->count; ++i) a->set(i, items[i]);
}
void arrReverse(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    for (std::uint32_t i = 0, j = a->count; i + 1 < j; ++i, --j) { const Value x = a->get(i), y = a->get(j - 1); a->set(i, y); a->set(j - 1, x); }
}
void arrIndexOf(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    for (std::uint32_t i = 0; i < a->count; ++i) if (vm.equalValues(a->get(i), c.args[1])) { c.ret = Value::i32(i); return; }
    c.ret = Value::i32(-1);
}
void arrCopy(Vm& vm, NativeCall& c) {
    ArrayObj* src = arrayArg(vm, c.args[0]);
    std::int64_t si = 0, di = 0, n = 0;
    ArrayObj* dst = nullptr;
    if (c.argc == 3) { dst = arrayArg(vm, c.args[1]); n = I(c, 2); }
    else { si = I(c, 1); dst = arrayArg(vm, c.args[2]); di = I(c, 3); n = I(c, 4); }
    if (n < 0 || si < 0 || di < 0) argRange(vm, "length", "Non-negative number required.");
    if (si + n > src->count || di + n > dst->count) vm.raise("ArgumentException", "Destination array was not long enough. Check the destination index, length, and the array's lower bounds.");
    std::vector<Value> tmp;
    for (std::int64_t i = 0; i < n; ++i) tmp.push_back(src->get(static_cast<std::uint32_t>(si + i)));
    for (std::int64_t i = 0; i < n; ++i) dst->set(static_cast<std::uint32_t>(di + i), tmp[static_cast<std::size_t>(i)]);
}
void arrResize(Vm& vm, NativeCall& c) {
    const Value cur = vm.loadPtr(c.args[0]);
    const std::int64_t n = I(c, 1);
    if (n < 0) argRange(vm, "newSize", "Non-negative number required.");
    TypeP et = nullptr;
    ArrayObj* old = cur.isNull() ? nullptr : arrayArg(vm, cur);
    if (old) et = old->elem;
    else et = c.type && c.type->k == TK::Array ? c.type->elem : nullptr;
    ArrayObj* a = vm.newArray(et, static_cast<std::uint32_t>(n));
    vm.root(a);
    a->type = old ? old->type : nullptr;
    if (old) for (std::uint32_t i = 0; i < old->count && i < a->count; ++i) a->set(i, old->get(i));
    vm.storePtr(c.args[0], Value::ref(a));
}
void arrFill(Vm& vm, NativeCall& c) { ArrayObj* a = arrayArg(vm, c.args[0]); for (std::uint32_t i = 0; i < a->count; ++i) a->set(i, stored(vm, c.args[1], a->elem)); }
void arrClear(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    const std::int64_t i0 = c.argc > 1 ? I(c, 1) : 0, n = c.argc > 2 ? I(c, 2) : a->count;
    if (i0 < 0 || n < 0 || i0 + n > a->count) vm.raise("IndexOutOfRangeException", "Index was outside the bounds of the array.");
    for (std::int64_t i = 0; i < n; ++i) a->set(static_cast<std::uint32_t>(i0 + i), vm.defaultValue(a->elem));
}
template <int Kind> void arrFind(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    for (std::uint32_t i = 0; i < a->count; ++i) {
        Value x = a->get(i);
        if (vm.truthy(vm.invoke(c.args[1], &x, 1))) {
            if (Kind == 0) c.ret = a->get(i);
            else if (Kind == 1) c.ret = Value::i32(i);
            else c.ret = Value::boolean(true);
            return;
        }
    }
    if (Kind == 0) c.ret = vm.defaultValue(a->elem);
    else if (Kind == 1) c.ret = Value::i32(-1);
    else c.ret = Value::boolean(false);
}
void arrBinarySearch(Vm& vm, NativeCall& c) {
    ArrayObj* a = arrayArg(vm, c.args[0]);
    std::int64_t lo = 0, hi = static_cast<std::int64_t>(a->count) - 1;
    while (lo <= hi) {
        const std::int64_t mid = lo + (hi - lo) / 2;
        const int r = vm.compareValues(a->get(static_cast<std::uint32_t>(mid)), c.args[1]);
        if (r == 0) { c.ret = Value::i32(mid); return; }
        if (r < 0) lo = mid + 1; else hi = mid - 1;
    }
    c.ret = Value::i32(~lo);
}

// ================================================================ File, Directory, Path
[[noreturn]] void ioError(Vm& vm, const std::string& path, const std::string& why) {
    if (why.find("non ammesso") != std::string::npos || why.find("assoluto") != std::string::npos) vm.raise("UnauthorizedAccessException", "Access to the path '" + path + "' is denied: " + why);
    if (why == "file non trovato") vm.raise("FileNotFoundException", "Could not find file '" + path + "'.");
    vm.raise("IOException", why + " ('" + path + "')");
}
void fReadAllText(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string out, why;
    if (!fileReadAll(vm, p, out, why)) ioError(vm, p, why);
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) out.erase(0, 3);
    c.ret = s8(vm, out);
}
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::size_t p = 0;
    while (p < s.size()) {
        std::size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        std::string l = s.substr(p, e - p);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        p = e + 1;
    }
    return lines;
}
void fReadAllLines(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string out, why;
    if (!fileReadAll(vm, p, out, why)) ioError(vm, p, why);
    const std::vector<std::string> lines = splitLines(out);
    ArrayObj* a = vm.newArray(vm.program().csString, static_cast<std::uint32_t>(lines.size()));
    vm.root(a);
    for (std::size_t i = 0; i < lines.size(); ++i) a->set(static_cast<std::uint32_t>(i), s8(vm, lines[i]));
    c.ret = Value::ref(a);
}
void fWriteAllText(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string why;
    if (!fileWriteAll(vm, p, c.args[1].isNull() ? std::string() : text(vm, c, 1), false, why)) ioError(vm, p, why);
}
void fAppendAllText(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string why;
    if (!fileWriteAll(vm, p, c.args[1].isNull() ? std::string() : text(vm, c, 1), true, why)) ioError(vm, p, why);
}
void fWriteAllLines(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string all;
    for (const Value& x : itemsOf(vm, c.args[1])) all += (x.isNull() ? std::string() : vm.utf8(x)) + "\n";
    std::string why;
    if (!fileWriteAll(vm, p, all, false, why)) ioError(vm, p, why);
}
void fExists(Vm& vm, NativeCall& c) { c.ret = Value::boolean(!c.args[0].isNull() && fileExists(vm, text(vm, c, 0))); }
void fDelete(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    if (!fileExists(vm, p)) {
        std::string why;
        if (confine(vm, p, why).empty()) ioError(vm, p, why);
        return;
    }
    std::string why;
    if (!fileRemove(vm, p, why)) ioError(vm, p, why);
}
void fMove(Vm& vm, NativeCall& c) {
    const std::string a = text(vm, c, 0), b = text(vm, c, 1);
    if (!fileExists(vm, a)) vm.raise("FileNotFoundException", "Could not find file '" + a + "'.");
    std::string why;
    if (!fileRename(vm, a, b, why)) ioError(vm, b, why);
}
void fCopy(Vm& vm, NativeCall& c) {
    const std::string a = text(vm, c, 0), b = text(vm, c, 1);
    std::string data, why;
    if (!fileReadAll(vm, a, data, why)) ioError(vm, a, why);
    if (fileExists(vm, b) && !(c.argc > 2 && vm.truthy(c.args[2]))) vm.raise("IOException", "The file '" + b + "' already exists.");
    if (!fileWriteAll(vm, b, data, false, why)) ioError(vm, b, why);
}
void dirExists(Vm& vm, NativeCall& c) {
    std::string why;
    const std::string full = confine(vm, text(vm, c, 0), why);
    files::FileSystem* fs = vm.host().files();
    files::Entry e;
    c.ret = Value::boolean(!full.empty() && fs && fs->stat(full, e) == files::Status::Ok && e.directory);
}
void dirCreate(Vm& vm, NativeCall& c) {
    const std::string p = text(vm, c, 0);
    std::string why;
    const std::string full = confine(vm, p, why);
    if (full.empty()) ioError(vm, p, why);
    files::FileSystem* fs = vm.host().files();
    if (!fs) ioError(vm, p, "Inserire la scheda SD");
    const files::Status st = fs->makeDirs(full);
    if (st != files::Status::Ok) ioError(vm, p, st == files::Status::Full ? "SD piena" : files::statusMessage(st));
}
template <bool Dirs> void dirList(Vm& vm, NativeCall& c) {
    const std::string p = c.argc > 0 ? text(vm, c, 0) : std::string(".");
    std::string why;
    const std::string full = p == "." || p.empty() ? vm.host().programFolder() : confine(vm, p, why);
    if (full.empty()) ioError(vm, p, why.empty() ? "nessuna cartella del programma" : why);
    files::FileSystem* fs = vm.host().files();
    if (!fs) ioError(vm, p, "Inserire la scheda SD");
    std::vector<files::Entry> entries;
    if (fs->list(full, entries) != files::Status::Ok) vm.raise("DirectoryNotFoundException", "Could not find a part of the path '" + p + "'.");
    std::vector<std::string> names;
    for (const auto& e : entries) if (e.directory == Dirs) names.push_back(p == "." || p.empty() ? e.name : p + "/" + e.name);
    std::sort(names.begin(), names.end());
    ArrayObj* a = vm.newArray(vm.program().csString, static_cast<std::uint32_t>(names.size()));
    vm.root(a);
    for (std::size_t i = 0; i < names.size(); ++i) a->set(static_cast<std::uint32_t>(i), s8(vm, names[i]));
    c.ret = Value::ref(a);
}
void pathCombine(Vm& vm, NativeCall& c) {
    std::string out;
    for (int i = 0; i < c.argc; ++i) {
        const std::string s = text(vm, c, i);
        if (s.empty()) continue;
        if (s[0] == '/' || s[0] == '\\') out = s;
        else if (out.empty() || out.back() == '/' || out.back() == '\\') out += s;
        else out += "/" + s;
    }
    c.ret = s8(vm, out);
}
std::string fileName(const std::string& p) { const std::size_t s = p.find_last_of("/\\"); return s == std::string::npos ? p : p.substr(s + 1); }
void pathFileName(Vm& vm, NativeCall& c) { c.ret = s8(vm, fileName(text(vm, c, 0))); }
void pathExtension(Vm& vm, NativeCall& c) { const std::string f = fileName(text(vm, c, 0)); const std::size_t d = f.rfind('.'); c.ret = s8(vm, d == std::string::npos ? std::string() : f.substr(d)); }
void pathStem(Vm& vm, NativeCall& c) { const std::string f = fileName(text(vm, c, 0)); const std::size_t d = f.rfind('.'); c.ret = s8(vm, d == std::string::npos ? f : f.substr(0, d)); }
void pathDir(Vm& vm, NativeCall& c) { const std::string p = text(vm, c, 0); const std::size_t s = p.find_last_of("/\\"); c.ret = s8(vm, s == std::string::npos ? std::string() : p.substr(0, s)); }
// StreamReader / StreamWriter
FileObj* fobj(Vm& vm, const NativeCall& c) { return fileArg(vm, c.args[0]); }
void srCtor(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    const std::string p = text(vm, c, 1);
    std::string why;
    if (!fileOpen(vm, f, p, "r", why)) ioError(vm, p, why);
}
void swCtor(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    const std::string p = text(vm, c, 1);
    const bool append = c.argc > 2 && vm.truthy(c.args[2]);
    std::string why;
    if (!fileOpen(vm, f, p, append ? "a" : "w", why)) ioError(vm, p, why);
}
void checkOpen(Vm& vm, FileObj* f) { if (!f->open) vm.raise("ObjectDisposedException", "Cannot access a closed file."); }
void srReadLine(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    checkOpen(vm, f);
    std::string line;
    if (!fileLine(vm, f, line, '\n')) { c.ret = Value::null(); return; }
    c.ret = s8(vm, line);
}
void srReadToEnd(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    checkOpen(vm, f);
    std::string all;
    for (int ch = fileGet(vm, f); ch >= 0; ch = fileGet(vm, f)) { all += static_cast<char>(ch); if ((all.size() & 4095) == 0) vm.poll(); }
    c.ret = s8(vm, all);
}
void srEnd(Vm& vm, NativeCall& c) { FileObj* f = fobj(vm, c); checkOpen(vm, f); c.ret = Value::boolean(filePeek(vm, f) < 0); }
void srPeek(Vm& vm, NativeCall& c) { FileObj* f = fobj(vm, c); checkOpen(vm, f); c.ret = Value::i32(filePeek(vm, f)); }
void srRead(Vm& vm, NativeCall& c) { FileObj* f = fobj(vm, c); checkOpen(vm, f); c.ret = Value::i32(fileGet(vm, f)); }
void swWrite(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    checkOpen(vm, f);
    if (c.argc > 1) filePut(vm, f, c.args[1].isNull() ? std::string() : vm.format(c.args[1], Dialect::CSharp));
}
void swWriteLine(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    checkOpen(vm, f);
    filePut(vm, f, (c.argc > 1 && !c.args[1].isNull() ? vm.format(c.args[1], Dialect::CSharp) : std::string()) + "\n");
}
void swWriteFmt(Vm& vm, NativeCall& c) { FileObj* f = fobj(vm, c); checkOpen(vm, f); filePut(vm, f, formatComposite(vm, text(vm, c, 1), c.args + 2, c.argc - 2)); }
void swWriteLineFmt(Vm& vm, NativeCall& c) { FileObj* f = fobj(vm, c); checkOpen(vm, f); filePut(vm, f, formatComposite(vm, text(vm, c, 1), c.args + 2, c.argc - 2) + "\n"); }
void fsClose(Vm& vm, NativeCall& c) {
    FileObj* f = fobj(vm, c);
    if (!f->open) return;
    if (!fileClose(vm, f)) vm.raise("IOException", "scrittura non riuscita: SD piena o rimossa ('" + f->path + "')");
}
void swFlush(Vm&, NativeCall&) {}
// Environment
void envTick(Vm& vm, NativeCall& c) { c.ret = Value::i32(static_cast<std::int32_t>(vm.host().millis())); }
void envTick64(Vm& vm, NativeCall& c) { c.ret = Value::i64(vm.host().millis()); }
void envExit(Vm&, NativeCall& c) { throw ProgramExit{static_cast<int>(I(c, 0))}; }

constexpr std::uint8_t P = NF::Property, St = NF::Static, Ct = NF::Ctor;
#define S_(owner, name, sig, fn) {L::Cs, "System", owner, name, sig, fn, St}
#define M_(owner, name, sig, fn) {L::Cs, "System", owner, name, sig, fn, 0}
#define P_(owner, name, sig, fn) {L::Cs, "System", owner, name, sig, fn, P}
#define C_(owner, sig, fn) {L::Cs, "System", owner, "#ctor", sig, fn, Ct}
const NativeEntry kCs[] = {
    // Console
    S_("Console", "Write", "void(any)", conWrite), S_("Console", "WriteLine", "void()", conWriteLine), S_("Console", "WriteLine", "void(any)", conWriteLine),
    S_("Console", "Write", "void(string,any,...)", conWriteFmt), S_("Console", "WriteLine", "void(string,any,...)", conWriteLineFmt),
    S_("Console", "ReadLine", "string()", conReadLine), S_("Console", "Read", "int()", conRead), S_("Console", "ReadKey", "char()", conReadKey),
    S_("Console", "ReadKey", "char(bool)", conReadKey), S_("Console", "Clear", "void()", conClear),
    P_("char", "KeyChar", "char()", keyChar),
    // Math
    S_("Math", "Sqrt", "double(double)", m1<mSqrt>), S_("Math", "Sin", "double(double)", m1<mSin>), S_("Math", "Cos", "double(double)", m1<mCos>),
    S_("Math", "Tan", "double(double)", m1<mTan>), S_("Math", "Asin", "double(double)", m1<mAsin>), S_("Math", "Acos", "double(double)", m1<mAcos>),
    S_("Math", "Atan", "double(double)", m1<mAtan>), S_("Math", "Sinh", "double(double)", m1<mSinh>), S_("Math", "Cosh", "double(double)", m1<mCosh>),
    S_("Math", "Tanh", "double(double)", m1<mTanh>), S_("Math", "Exp", "double(double)", m1<mExp>), S_("Math", "Log", "double(double)", m1<mLog>),
    S_("Math", "Log", "double(double,double)", mLogB), S_("Math", "Log10", "double(double)", m1<mLog10>), S_("Math", "Log2", "double(double)", m1<mLog2>),
    S_("Math", "Floor", "double(double)", m1<mFloor>), S_("Math", "Ceiling", "double(double)", m1<mCeiling>), S_("Math", "Truncate", "double(double)", m1<mTruncate>),
    S_("Math", "Cbrt", "double(double)", m1<mCbrt>), S_("Math", "Pow", "double(double,double)", mPow), S_("Math", "Atan2", "double(double,double)", mAtan2),
    S_("Math", "IEEERemainder", "double(double,double)", mIeeeRem),
    S_("Math", "Round", "double(double)", mRound), S_("Math", "Round", "double(double,int)", mRoundDigits),
    S_("Math", "Abs", "int(int)", mAbsI), S_("Math", "Abs", "long(long)", mAbsI), S_("Math", "Abs", "short(short)", mAbsI),
    S_("Math", "Abs", "double(double)", mAbsD), S_("Math", "Abs", "float(float)", mAbsD),
    S_("Math", "Max", "int(int,int)", mMinMax<true>), S_("Math", "Max", "long(long,long)", mMinMax<true>), S_("Math", "Max", "uint(uint,uint)", mMinMax<true>),
    S_("Math", "Max", "ulong(ulong,ulong)", mMinMax<true>), S_("Math", "Max", "double(double,double)", mMinMax<true>), S_("Math", "Max", "float(float,float)", mMinMax<true>),
    S_("Math", "Min", "int(int,int)", mMinMax<false>), S_("Math", "Min", "long(long,long)", mMinMax<false>), S_("Math", "Min", "uint(uint,uint)", mMinMax<false>),
    S_("Math", "Min", "ulong(ulong,ulong)", mMinMax<false>), S_("Math", "Min", "double(double,double)", mMinMax<false>), S_("Math", "Min", "float(float,float)", mMinMax<false>),
    S_("Math", "Sign", "int(int)", mSign), S_("Math", "Sign", "int(long)", mSign), S_("Math", "Sign", "int(double)", mSign),
    S_("Math", "Clamp", "int(int,int,int)", mClamp), S_("Math", "Clamp", "long(long,long,long)", mClamp), S_("Math", "Clamp", "double(double,double,double)", mClamp),
    // Convert e Parse
    S_("Convert", "ToInt32", "int(any)", cvToInt32), S_("Convert", "ToInt64", "long(any)", cvToInt64), S_("Convert", "ToDouble", "double(any)", cvToDouble),
    S_("Convert", "ToString", "string(any)", cvToString), S_("Convert", "ToBoolean", "bool(any)", cvToBoolean), S_("Convert", "ToChar", "char(any)", cvToChar),
    S_("int", "Parse", "int(string)", parseInt<VK::I32>), S_("long", "Parse", "long(string)", parseInt<VK::I64>), S_("short", "Parse", "short(string)", parseInt<VK::I16>),
    S_("byte", "Parse", "byte(string)", parseInt<VK::U8>), S_("uint", "Parse", "uint(string)", parseInt<VK::U32>),
    S_("int", "TryParse", "bool(string,int&)", tryParseInt<VK::I32>), S_("long", "TryParse", "bool(string,long&)", tryParseInt<VK::I64>),
    S_("double", "Parse", "double(string)", parseDoubleV), S_("float", "Parse", "float(string)", parseFloatV),
    S_("double", "TryParse", "bool(string,double&)", tryParseDouble),
    S_("double", "IsNaN", "bool(double)", dblIs<0>), S_("double", "IsInfinity", "bool(double)", dblIs<1>),
    S_("double", "IsPositiveInfinity", "bool(double)", dblIs<2>), S_("double", "IsNegativeInfinity", "bool(double)", dblIs<3>),
    S_("double", "IsFinite", "bool(double)", dblIs<4>), S_("float", "IsNaN", "bool(double)", dblIs<0>), S_("float", "IsInfinity", "bool(double)", dblIs<1>),
    S_("bool", "Parse", "bool(string)", parseBool), S_("bool", "TryParse", "bool(string,bool&)", tryParseBool), S_("char", "Parse", "char(string)", parseChar),
    // char
    S_("char", "IsDigit", "bool(char)", chIs<0>), S_("char", "IsLetter", "bool(char)", chIs<1>), S_("char", "IsLetterOrDigit", "bool(char)", chIs<2>),
    S_("char", "IsWhiteSpace", "bool(char)", chIs<3>), S_("char", "IsUpper", "bool(char)", chIs<4>), S_("char", "IsLower", "bool(char)", chIs<5>),
    S_("char", "IsPunctuation", "bool(char)", chIs<6>), S_("char", "IsControl", "bool(char)", chIs<7>), S_("char", "IsSymbol", "bool(char)", chIs<8>),
    S_("char", "IsNumber", "bool(char)", chIs<0>), S_("char", "ToUpper", "char(char)", chToUpper), S_("char", "ToLower", "char(char)", chToLower),
    S_("char", "GetNumericValue", "double(char)", chNumeric),
    // string
    P_("string", "Length", "int()", strLength), M_("string", "operator[]", "char(int)", strIndex),
    M_("string", "Substring", "string(int)", strSubstring), M_("string", "Substring", "string(int,int)", strSubstring),
    M_("string", "IndexOf", "int(string)", strIndexOf<false>), M_("string", "IndexOf", "int(char)", strIndexOf<false>),
    M_("string", "IndexOf", "int(string,int)", strIndexOf<false>), M_("string", "IndexOf", "int(char,int)", strIndexOf<false>),
    M_("string", "LastIndexOf", "int(string)", strIndexOf<true>), M_("string", "LastIndexOf", "int(char)", strIndexOf<true>),
    M_("string", "Contains", "bool(string)", strContains), M_("string", "Contains", "bool(char)", strContains),
    M_("string", "StartsWith", "bool(string)", strStartsWith), M_("string", "StartsWith", "bool(char)", strStartsWith),
    M_("string", "EndsWith", "bool(string)", strEndsWith), M_("string", "EndsWith", "bool(char)", strEndsWith),
    M_("string", "ToUpper", "string()", strToUpper), M_("string", "ToLower", "string()", strToLower),
    M_("string", "ToUpperInvariant", "string()", strToUpper), M_("string", "ToLowerInvariant", "string()", strToLower),
    M_("string", "Trim", "string()", strTrim<0>), M_("string", "Trim", "string(char,...)", strTrim<0>),
    M_("string", "TrimStart", "string()", strTrim<1>), M_("string", "TrimStart", "string(char,...)", strTrim<1>),
    M_("string", "TrimEnd", "string()", strTrim<2>), M_("string", "TrimEnd", "string(char,...)", strTrim<2>),
    M_("string", "Replace", "string(string,string)", strReplace), M_("string", "Replace", "string(char,char)", strReplace),
    M_("string", "Split", "array<string>()", strSplit), M_("string", "Split", "array<string>(char,...)", strSplit),
    M_("string", "Split", "array<string>(string,...)", strSplit), M_("string", "Split", "array<string>(array<char>,...)", strSplit),
    M_("string", "Insert", "string(int,string)", strInsert), M_("string", "Remove", "string(int)", strRemove), M_("string", "Remove", "string(int,int)", strRemove),
    M_("string", "PadLeft", "string(int)", strPad<true>), M_("string", "PadLeft", "string(int,char)", strPad<true>),
    M_("string", "PadRight", "string(int)", strPad<false>), M_("string", "PadRight", "string(int,char)", strPad<false>),
    M_("string", "Equals", "bool(string)", strEquals), S_("string", "Equals", "bool(string,string)", strEqualsStatic), M_("string", "CompareTo", "int(string)", strCompareTo),
    M_("string", "ToCharArray", "array<char>()", strToCharArray),
    S_("string", "IsNullOrEmpty", "bool(string)", strIsNullOrEmpty), S_("string", "IsNullOrWhiteSpace", "bool(string)", strIsNullOrWhiteSpace),
    S_("string", "Join", "string(string,any)", strJoin), S_("string", "Join", "string(char,any)", strJoin), S_("string", "Join", "string(string,any,any,...)", strJoin),
    S_("string", "Concat", "string(any,...)", strConcat), S_("string", "Format", "string(string,any,...)", strFormat), S_("string", "Format", "string(string)", strFormat),
    S_("string", "Compare", "int(string,string)", strCompare), S_("string", "CompareOrdinal", "int(string,string)", strCompareOrdinal),
    S_("string", "#new", "string(char,int)", strCtorFill), S_("string", "#new", "string(array<char>)", strCtorChars),
    // StringBuilder
    C_("StringBuilder", "void()", sbCtor), C_("StringBuilder", "void(string)", sbCtor), C_("StringBuilder", "void(int)", sbCtor),
    M_("StringBuilder", "Append", "self(any)", sbAppend), M_("StringBuilder", "AppendFormat", "self(string,any,...)", sbAppendFormat), M_("StringBuilder", "AppendLine", "self()", sbAppendLine), M_("StringBuilder", "AppendLine", "self(any)", sbAppendLine),
    M_("StringBuilder", "Insert", "self(int,any)", sbInsert), M_("StringBuilder", "Remove", "self(int,int)", sbRemove),
    M_("StringBuilder", "Replace", "self(string,string)", sbReplace), M_("StringBuilder", "Replace", "self(char,char)", sbReplace),
    M_("StringBuilder", "Clear", "self()", sbClear), M_("StringBuilder", "ToString", "string()", sbToString), P_("StringBuilder", "Length", "int()", sbLength),
    M_("StringBuilder", "#get", "char(int)", sbGet), M_("StringBuilder", "#set", "void(int,char)", sbSet),
    // List<T>
    C_("List", "void()", lCtor), C_("List", "void(int)", lCtor), C_("List", "void(any)", lCtor),
    M_("List", "Add", "void(T)", lAdd), M_("List", "AddRange", "void(any)", lAddRange), M_("List", "Insert", "void(int,T)", lInsert),
    M_("List", "Remove", "bool(T)", lRemove), M_("List", "RemoveAt", "void(int)", lRemoveAt), M_("List", "RemoveRange", "void(int,int)", lRemoveRange),
    M_("List", "RemoveAll", "int(pred<T>)", lRemoveAll), M_("List", "Clear", "void()", lClear),
    M_("List", "Contains", "bool(T)", lContains), M_("List", "IndexOf", "int(T)", lIndexOf), M_("List", "LastIndexOf", "int(T)", lLastIndexOf),
    M_("List", "Find", "T(pred<T>)", lFind<0>), M_("List", "FindIndex", "int(pred<T>)", lFind<1>), M_("List", "FindLast", "T(pred<T>)", lFind<2>),
    M_("List", "FindLastIndex", "int(pred<T>)", lFind<4>), M_("List", "Exists", "bool(pred<T>)", lFind<3>), M_("List", "FindAll", "self(pred<T>)", lFindAll),
    M_("List", "TrueForAll", "bool(pred<T>)", lTrueForAll),
    P_("List", "Count", "int()", lCount), P_("List", "Capacity", "int()", lCapacity),
    M_("List", "#get", "T(int)", lGet), M_("List", "#set", "void(int,T)", lSet),
    M_("List", "Sort", "void()", lSort), M_("List", "Sort", "void(cmp<T>)", lSort), M_("List", "Reverse", "void()", lReverse),
    M_("List", "ToArray", "array<T>()", lToArray), M_("List", "ForEach", "void(action<T>)", lForEach), M_("List", "GetRange", "self(int,int)", lGetRange),
    M_("List", "BinarySearch", "int(T)", lBinarySearch),
    // Queue<T>, Stack<T>
    C_("Queue", "void()", lCtor), C_("Queue", "void(any)", lCtor),
    M_("Queue", "Enqueue", "void(T)", qEnqueue), M_("Queue", "Dequeue", "T()", qDequeue), M_("Queue", "Peek", "T()", qPeek),
    M_("Queue", "TryDequeue", "bool(T&)", qTryDequeue), M_("Queue", "TryPeek", "bool(T&)", qTryPeek),
    P_("Queue", "Count", "int()", lCount), M_("Queue", "Clear", "void()", lClear), M_("Queue", "Contains", "bool(T)", lContains), M_("Queue", "ToArray", "array<T>()", lToArray),
    C_("Stack", "void()", lCtor), C_("Stack", "void(any)", lCtor),
    M_("Stack", "Push", "void(T)", qEnqueue), M_("Stack", "Pop", "T()", stPop), M_("Stack", "Peek", "T()", stPeek),
    M_("Stack", "TryPop", "bool(T&)", stTryPop), M_("Stack", "TryPeek", "bool(T&)", stTryPeek),
    P_("Stack", "Count", "int()", lCount), M_("Stack", "Clear", "void()", lClear), M_("Stack", "Contains", "bool(T)", lContains), M_("Stack", "ToArray", "array<T>()", lToArray),
    // Dictionary<K,V>
    C_("Dictionary", "void()", dCtor), C_("Dictionary", "void(any)", dCtor),
    M_("Dictionary", "Add", "void(K,V)", dAdd), M_("Dictionary", "TryAdd", "bool(K,V)", dTryAdd),
    M_("Dictionary", "#get", "V(K)", dGet), M_("Dictionary", "#set", "void(K,V)", dSet),
    M_("Dictionary", "ContainsKey", "bool(K)", dContainsKey), M_("Dictionary", "ContainsValue", "bool(V)", dContainsValue),
    M_("Dictionary", "TryGetValue", "bool(K,V&)", dTryGetValue), M_("Dictionary", "GetValueOrDefault", "V(K)", dGetOrDefault),
    M_("Dictionary", "GetValueOrDefault", "V(K,V)", dGetOrDefault), M_("Dictionary", "Remove", "bool(K)", dRemove),
    P_("Dictionary", "Count", "int()", dCount), M_("Dictionary", "Clear", "void()", dClear),
    P_("Dictionary", "Keys", "list<K>()", dKeys<true>), P_("Dictionary", "Values", "list<V>()", dKeys<false>),
    // HashSet<T>
    C_("HashSet", "void()", dCtor), C_("HashSet", "void(any)", dCtor),
    M_("HashSet", "Add", "bool(T)", hsAdd), M_("HashSet", "Remove", "bool(T)", hsRemove), M_("HashSet", "Contains", "bool(T)", hsContains),
    P_("HashSet", "Count", "int()", dCount), M_("HashSet", "Clear", "void()", dClear),
    M_("HashSet", "UnionWith", "void(any)", hsUnion), M_("HashSet", "IntersectWith", "void(any)", hsIntersect), M_("HashSet", "ExceptWith", "void(any)", hsExcept),
    M_("HashSet", "IsSubsetOf", "bool(any)", hsSubset),
    // Random
    C_("Random", "void()", rCtor), C_("Random", "void(int)", rCtor),
    M_("Random", "Next", "int()", rNext), M_("Random", "Next", "int(int)", rNext), M_("Random", "Next", "int(int,int)", rNext),
    M_("Random", "NextDouble", "double()", rNextDouble),
    // Array
    S_("Array", "Sort", "void(any)", arrSort), S_("Array", "Sort", "void(any,cmp<E0>)", arrSort), S_("Array", "Reverse", "void(any)", arrReverse),
    S_("Array", "IndexOf", "int(any,any)", arrIndexOf), S_("Array", "Copy", "void(any,any,int)", arrCopy), S_("Array", "Copy", "void(any,int,any,int,int)", arrCopy),
    S_("Array", "Resize", "void(any&,int)", arrResize), S_("Array", "Fill", "void(any,any)", arrFill),
    S_("Array", "Clear", "void(any)", arrClear), S_("Array", "Clear", "void(any,int,int)", arrClear),
    S_("Array", "Find", "E0(any,pred<E0>)", arrFind<0>), S_("Array", "FindIndex", "int(any,pred<E0>)", arrFind<1>), S_("Array", "Exists", "bool(any,pred<E0>)", arrFind<2>),
    S_("Array", "BinarySearch", "int(any,any)", arrBinarySearch),
    // File, Directory, Path
    S_("File", "ReadAllText", "string(string)", fReadAllText), S_("File", "ReadAllLines", "array<string>(string)", fReadAllLines),
    S_("File", "WriteAllText", "void(string,string)", fWriteAllText), S_("File", "AppendAllText", "void(string,string)", fAppendAllText),
    S_("File", "WriteAllLines", "void(string,any)", fWriteAllLines), S_("File", "Exists", "bool(string)", fExists), S_("File", "Delete", "void(string)", fDelete),
    S_("File", "Move", "void(string,string)", fMove), S_("File", "Copy", "void(string,string)", fCopy), S_("File", "Copy", "void(string,string,bool)", fCopy),
    S_("Directory", "Exists", "bool(string)", dirExists), S_("Directory", "CreateDirectory", "void(string)", dirCreate),
    S_("Directory", "GetFiles", "array<string>(string)", dirList<false>), S_("Directory", "GetDirectories", "array<string>(string)", dirList<true>),
    S_("Path", "Combine", "string(string,string)", pathCombine), S_("Path", "Combine", "string(string,string,string)", pathCombine),
    S_("Path", "GetFileName", "string(string)", pathFileName), S_("Path", "GetExtension", "string(string)", pathExtension),
    S_("Path", "GetFileNameWithoutExtension", "string(string)", pathStem), S_("Path", "GetDirectoryName", "string(string)", pathDir),
    C_("StreamReader", "void(string)", srCtor),
    M_("StreamReader", "ReadLine", "string()", srReadLine), M_("StreamReader", "ReadToEnd", "string()", srReadToEnd),
    P_("StreamReader", "EndOfStream", "bool()", srEnd), M_("StreamReader", "Peek", "int()", srPeek), M_("StreamReader", "Read", "int()", srRead),
    M_("StreamReader", "Close", "void()", fsClose), M_("StreamReader", "Dispose", "void()", fsClose),
    C_("StreamWriter", "void(string)", swCtor), C_("StreamWriter", "void(string,bool)", swCtor),
    M_("StreamWriter", "Write", "void(any)", swWrite), M_("StreamWriter", "WriteLine", "void()", swWriteLine), M_("StreamWriter", "WriteLine", "void(any)", swWriteLine),
    M_("StreamWriter", "Write", "void(string,any,...)", swWriteFmt), M_("StreamWriter", "WriteLine", "void(string,any,...)", swWriteLineFmt),
    M_("StreamWriter", "Close", "void()", fsClose), M_("StreamWriter", "Dispose", "void()", fsClose), M_("StreamWriter", "Flush", "void()", swFlush),
    // Environment
    {L::Cs, "System", "Environment", "TickCount", "int()", envTick, St | P}, {L::Cs, "System", "Environment", "TickCount64", "long()", envTick64, St | P},
    S_("Environment", "Exit", "void(int)", envExit),
};
#undef S_
#undef M_
#undef P_
#undef C_
} // namespace

const NativeEntry* libCsEntries(std::size_t& n) { n = sizeof(kCs) / sizeof(kCs[0]); return kCs; }

} // namespace fx::prog
