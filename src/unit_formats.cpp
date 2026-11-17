// Conversioni di formato di CONVERSION (vedi unit_formats.hpp).
#include "unit_formats.hpp"
#include "calc_library.hpp"
#include "exact.hpp"
#include "expression.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace fx::unitfmt {
namespace {
constexpr double kPi = 3.14159265358979323846;
using u64 = std::uint64_t;
using i64 = std::int64_t;

[[noreturn]] void syntax(const std::string& what) { throw SyntaxError("Syntax ERROR: " + what); }
[[noreturn]] void argument(const std::string& what) { throw MathError("Argument ERROR: " + what); }
void need(bool ok, const char* what) { if (!ok) argument(what); }

// ---------------------------------------------------------------- testo
std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}
std::string upper(std::string s) { for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }
// Sostituisce le sequenze UTF-8 date (° ′ ″ ∠ ...) con un carattere ASCII.
std::string ascii(std::string s, const char* const* from, char to) {
    for (; *from; ++from) {
        const std::size_t n = std::strlen(*from);
        for (std::size_t p; (p = s.find(*from)) != std::string::npos;) s.replace(p, n, 1, to);
    }
    return s;
}
// Numero decimale completo (niente inf/nan, niente testo in coda).
double number(const std::string& t) {
    const std::string s = trim(t);
    if (s.empty()) syntax("numero mancante");
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c)) && !std::strchr("+-.eE", c)) syntax("numero non valido \"" + s + "\"");
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (!end || *end) syntax("numero non valido \"" + s + "\"");
    if (!std::isfinite(v)) argument("numero fuori intervallo");
    return v;
}
// Numeri separati da virgole, punti e virgola o spazi.
std::vector<double> numbers(const std::string& text, std::size_t count, const char* what) {
    static const char* const marks[] = {"\xC2\xB0", "%", nullptr};
    std::string s = ascii(text, marks, ' ');
    for (char& c : s) if (c == ',' || c == ';') c = ' ';
    std::vector<double> out;
    std::size_t p = 0;
    while (p < s.size()) {
        while (p < s.size() && s[p] == ' ') ++p;
        if (p >= s.size()) break;
        const std::size_t e = s.find(' ', p);
        out.push_back(number(s.substr(p, e == std::string::npos ? std::string::npos : e - p)));
        p = e == std::string::npos ? s.size() : e;
    }
    if (out.size() != count) syntax(std::string("servono ") + std::to_string(count) + " numeri (" + what + ")");
    return out;
}
std::string num(double v, const Options& o) { return formatReal(v == 0 ? 0.0 : v, o.display); }
// Azzera le componenti dovute solo all'arrotondamento di seno e coseno (cos 90° = 6E-17).
double snap(double v, double scale) { return std::fabs(v) <= 1e-13 * std::fabs(scale) ? 0.0 : v; }
std::string fixed(double v, int decimals) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", decimals, v);
    std::string s = b;
    if (s.find('.') != std::string::npos) { while (s.back() == '0') s.pop_back(); if (s.back() == '.') s.pop_back(); }
    return s == "-0" ? "0" : s;
}
std::string hex(u64 v, int digits) {
    char b[24];
    std::snprintf(b, sizeof b, "%0*llX", digits, static_cast<unsigned long long>(v));
    return b;
}
u64 parseUnsigned(const std::string& text, unsigned base, const char* what) {
    std::string s = upper(trim(text));
    if (base == 16 && s.size() > 2 && s[0] == '0' && s[1] == 'X') s = s.substr(2);
    if (base == 2 && s.size() > 2 && s[0] == '0' && s[1] == 'B') s = s.substr(2);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    if (s.empty()) syntax(std::string(what) + " mancante");
    for (char c : s) {
        const unsigned d = std::isdigit(static_cast<unsigned char>(c)) ? static_cast<unsigned>(c - '0') : std::isalpha(static_cast<unsigned char>(c)) ? static_cast<unsigned>(c - 'A' + 10) : 99;
        if (d >= base) syntax(std::string("cifra non valida in ") + what);
    }
    try { return fromBase(s, base); } catch (const std::exception&) { argument(std::string(what) + " oltre 64 bit"); }
}
double angleToRad(double a, const Options& o) { return o.angle == AngleMode::Degrees ? a * kPi / 180 : o.angle == AngleMode::Gradians ? a * kPi / 200 : a; }
double radToAngle(double r, const Options& o) { return o.angle == AngleMode::Degrees ? r * 180 / kPi : o.angle == AngleMode::Gradians ? r * 200 / kPi : r; }
const char* angleMark(const Options& o) { return o.angle == AngleMode::Degrees ? "\xC2\xB0" : o.angle == AngleMode::Gradians ? "g" : "r"; }

// ---------------------------------------------------------------- basi numeriche
struct Real { bool neg{}; u64 integer{}; double frac{}; };
Real parseBase(const std::string& text, unsigned base) {
    std::string s = upper(trim(text));
    Real r;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) { r.neg = s[0] == '-'; s = s.substr(1); }
    const auto dot = s.find('.');
    const std::string ip = s.substr(0, dot), fp = dot == std::string::npos ? "" : s.substr(dot + 1);
    if (ip.empty() && fp.empty()) syntax("numero mancante");
    r.integer = ip.empty() ? 0 : parseUnsigned(ip, base, "parte intera");
    // Cifre frazionarie: numeratore e potenza della base esatti finche' stanno
    // in 53 bit (un solo arrotondamento), poi somma delle cifre restanti.
    double numer = 0, denom = 1, scale = 0;
    for (char c : fp) {
        const unsigned d = std::isdigit(static_cast<unsigned char>(c)) ? static_cast<unsigned>(c - '0') : std::isalpha(static_cast<unsigned char>(c)) ? static_cast<unsigned>(c - 'A' + 10) : 99;
        if (d >= base) syntax("cifra non valida nella parte frazionaria");
        if (scale == 0 && denom * base <= 9007199254740992.0) { numer = numer * base + d; denom *= base; }
        else { if (scale == 0) { r.frac = numer / denom; scale = 1 / (denom * base); } else scale /= base; r.frac += d * scale; }
    }
    if (scale == 0) r.frac = numer / denom;
    if (r.frac >= 1) { r.frac = 0; if (r.integer == UINT64_MAX) argument("oltre 64 bit"); ++r.integer; }
    return r;
}
std::string formatBase(Real r, unsigned base) {
    std::string out;
    if (base == 10 && r.frac > 0) {
        std::string f = fixed(r.frac, 15);
        if (f == "1") { if (r.integer == UINT64_MAX) argument("oltre 64 bit"); ++r.integer; f = "0"; }
        out = toBase(r.integer, 10) + (f == "0" ? "" : f.substr(1));
    } else {
        out = toBase(r.integer, base);
        if (r.frac > 0) {
            const int maxDigits = std::min(32, static_cast<int>(std::ceil(53 / std::log2(static_cast<double>(base)))) + 1);
            out += '.';
            double f = r.frac;
            for (int k = 0; k < maxDigits && f > 0; ++k) {
                f *= base;
                const int d = static_cast<int>(f);
                out += "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[d];
                f -= d;
            }
            if (f > 0) out += "\xE2\x80\xA6";
        }
    }
    return (r.neg && (r.integer || r.frac > 0) ? "-" : "") + out;
}
unsigned baseOf(std::size_t rep, const Options& o) {
    static const unsigned b[] = {10, 16, 2, 8};
    if (rep < 4) return b[rep];
    const double p = o.param;
    need(p == std::floor(p) && p >= 2 && p <= 36, "base da 2 a 36");
    return static_cast<unsigned>(p);
}

// ---------------------------------------------------------------- complemento a due, BCD, Gray
unsigned widthOf(const Options& o) {
    need(o.param == 8 || o.param == 16 || o.param == 32 || o.param == 64, "bit: 8, 16, 32 o 64");
    return static_cast<unsigned>(o.param);
}
u64 maskOf(unsigned w) { return w == 64 ? UINT64_MAX : (u64{1} << w) - 1; }
i64 parseSigned(const std::string& text) {
    std::string s = trim(text);
    bool neg = false;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) { neg = s[0] == '-'; s = s.substr(1); }
    if (s.find('.') != std::string::npos) argument("serve un intero");
    const u64 m = parseUnsigned(s, 10, "numero");
    if (neg) { if (m > u64{1} << 63) argument("fuori intervallo (64 bit)"); return m == u64{1} << 63 ? INT64_MIN : -static_cast<i64>(m); }
    if (m > static_cast<u64>(INT64_MAX)) argument("fuori intervallo (64 bit)");
    return static_cast<i64>(m);
}
std::string bits(u64 v, unsigned w) { std::string s; for (unsigned i = w; i-- > 0;) s += (v >> i) & 1 ? '1' : '0'; return s; }

// ---------------------------------------------------------------- IEEE-754
u64 bitsOf(double d) { u64 b; std::memcpy(&b, &d, 8); return b; }
double fromBits(u64 b) { double d; std::memcpy(&d, &b, 8); return d; }
std::uint16_t toHalf(double d) {
    const std::uint16_t sign = std::signbit(d) ? 0x8000 : 0;
    if (std::isnan(d)) return static_cast<std::uint16_t>(sign | 0x7E00);
    const double a = std::fabs(d);
    if (a >= 65520.0) return static_cast<std::uint16_t>(sign | 0x7C00);
    if (a < 6.103515625e-05) return static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>(std::nearbyint(a * 16777216.0)));
    int ex = 0;
    (void)std::frexp(a, &ex);
    int e = ex - 1;
    double m = std::nearbyint((std::ldexp(a, -e) - 1) * 1024);
    if (m >= 1024) { m = 0; ++e; }
    if (e > 15) return static_cast<std::uint16_t>(sign | 0x7C00);
    return static_cast<std::uint16_t>(sign | ((e + 15) << 10) | static_cast<std::uint16_t>(m));
}
double fromHalf(std::uint16_t h) {
    const int e = (h >> 10) & 0x1F, m = h & 0x3FF;
    double v = e == 0 ? std::ldexp(m, -24) : e == 31 ? (m ? NAN : INFINITY) : std::ldexp(1024 + m, e - 25);
    return h & 0x8000 ? -v : v;
}
std::string ieeeText(u64 b, unsigned expBits, unsigned mantBits) {
    const unsigned w = 1 + expBits + mantBits;
    return hex(b, static_cast<int>(w / 4)) + "  s=" + std::to_string(b >> (w - 1)) + " e=" + std::to_string((b >> mantBits) & ((1u << expBits) - 1)) +
           " m=" + hex(b & ((u64{1} << mantBits) - 1), static_cast<int>((mantBits + 3) / 4));
}
std::string shortest(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-Inf" : "Inf";
    char b[40];
    for (int digits = 1; digits <= 17; ++digits) {
        std::snprintf(b, sizeof b, "%.*g", digits, v);
        if (std::strtod(b, nullptr) == v) break;
    }
    return b;
}

// ---------------------------------------------------------------- numeri romani
std::string roman(unsigned n) {
    static const unsigned val[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char* const sym[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    std::string s;
    for (int i = 0; i < 13; ++i) while (n >= val[i]) { s += sym[i]; n -= val[i]; }
    return s;
}
std::string romanVinculum(unsigned n) { return n >= 4000 ? "(" + roman(n / 1000) + ")" + roman(n % 1000) : roman(n); }
unsigned romanValue(const std::string& s) {
    auto v = [](char c) -> unsigned { switch (c) { case 'I': return 1; case 'V': return 5; case 'X': return 10; case 'L': return 50; case 'C': return 100; case 'D': return 500; case 'M': return 1000; default: return 0; } };
    i64 total = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned a = v(s[i]);
        if (!a) syntax("carattere non romano");
        const unsigned b = i + 1 < s.size() ? v(s[i + 1]) : 0;
        if (a < b) total -= a; else total += a;
        if (total > 4000000) argument("numero romano troppo grande");
    }
    if (total < 1) argument("numero romano non canonico");
    return static_cast<unsigned>(total);
}
unsigned parseRoman(const std::string& text, bool vinculum) {
    const std::string s = upper(trim(text));
    if (s.empty()) syntax("numero romano mancante");
    unsigned n = 0;
    if (s[0] == '(') {
        if (!vinculum) argument("vinculum: usare la rappresentazione con le parentesi");
        const auto close = s.find(')');
        if (close == std::string::npos || close == 1) syntax("parentesi del vinculum");
        n = romanValue(s.substr(1, close - 1)) * 1000 + (close + 1 < s.size() ? romanValue(s.substr(close + 1)) : 0);
    } else n = romanValue(s);
    need(n >= 1 && n <= (vinculum ? 3999999u : 3999u), vinculum ? "romani da 1 a 3999999" : "romani da 1 a 3999");
    if ((vinculum ? romanVinculum(n) : roman(n)) != s) argument("numero romano non canonico (" + (vinculum ? romanVinculum(n) : roman(n)) + ")");
    return n;
}

// ---------------------------------------------------------------- Unicode
std::vector<std::uint32_t> decodeUtf8(const std::string& s) {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (!n || i + static_cast<std::size_t>(n) > s.size()) argument("UTF-8 non valido");
        std::uint32_t cp = n == 1 ? c : c & (0x7F >> n);
        for (int k = 1; k < n; ++k) {
            const unsigned char d = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((d & 0xC0) != 0x80) argument("UTF-8 non valido");
            cp = (cp << 6) | (d & 0x3F);
        }
        static const std::uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < minimum[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) argument("UTF-8 non valido");
        out.push_back(cp);
        i += static_cast<std::size_t>(n);
    }
    return out;
}
void encodeUtf8(std::uint32_t cp, std::string& out) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
}
std::vector<std::string> tokens(const std::string& text) {
    std::string s = text;
    for (char& c : s) if (c == ',' || c == ';') c = ' ';
    std::vector<std::string> out;
    std::size_t p = 0;
    while (p < s.size()) {
        while (p < s.size() && s[p] == ' ') ++p;
        if (p >= s.size()) break;
        const std::size_t e = std::min(s.find(' ', p), s.size());
        out.push_back(s.substr(p, e - p));
        p = e;
    }
    return out;
}

// ---------------------------------------------------------------- angoli, tempo, DMS
// "12°30′15.5″", "12 30 15.5", "12:30:15.5", "12d30m15.5s", "-0°30'": gradi (o ore).
double parseSexagesimal(const std::string& text, double& hemisphereSign, const char* letters) {
    static const char* const deg[] = {"\xC2\xB0", nullptr};
    static const char* const min[] = {"\xE2\x80\xB2", nullptr};
    static const char* const sec[] = {"\xE2\x80\xB3", nullptr};
    std::string s = ascii(ascii(ascii(trim(text), deg, 'd'), min, '\''), sec, '"');
    hemisphereSign = 1;
    if (letters && !s.empty()) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s.back())));
        const char* p = std::strchr(letters, c);
        if (p && c) { hemisphereSign = (p - letters) % 2 ? -1 : 1; s.pop_back(); s = trim(s); }
    }
    bool neg = false;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) { neg = s[0] == '-'; s = s.substr(1); }
    for (char& c : s) if (c == 'd' || c == 'D' || c == 'h' || c == 'H' || c == ':' || c == '\'' || c == 'm' || c == 'M' || c == '"' || c == 's' || c == 'S') c = ' ';
    const auto parts = tokens(s);
    if (parts.empty() || parts.size() > 3) syntax("formato gradi minuti secondi");
    double v[3] = {0, 0, 0};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        v[i] = number(parts[i]);
        if (v[i] < 0) syntax("segno solo davanti al primo campo");
    }
    need(v[1] < 60 && v[2] < 60, "minuti e secondi minori di 60");
    if (parts.size() > 1) need(v[0] == std::floor(v[0]), "gradi interi se ci sono i minuti");
    if (parts.size() > 2) need(v[1] == std::floor(v[1]), "minuti interi se ci sono i secondi");
    const double d = v[0] + v[1] / 60 + v[2] / 3600;
    return neg ? -d : d;
}
std::string sexagesimal(double v, const char* a, const char* b, const char* c, int secDecimals) {
    const bool neg = v < 0;
    double x = std::fabs(v);
    double d = std::floor(x), m = std::floor((x - d) * 60), s = ((x - d) * 60 - m) * 60;
    const double unit = std::pow(10.0, -secDecimals);
    s = std::round(s / unit) * unit;
    if (s >= 60) { s -= 60; ++m; }
    if (m >= 60) { m -= 60; ++d; }
    const bool clock = a[0] == ':';
    const std::string ms = fixed(m, 0), ss = fixed(s, secDecimals);
    return std::string(neg ? "-" : "") + fixed(d, 0) + a + (clock && m < 10 ? "0" : "") + ms + b + (clock && s < 10 ? "0" : "") + ss + c;
}

// ---------------------------------------------------------------- date
i64 jdnOf(i64 y, i64 m, i64 d) {
    const i64 a = (14 - m) / 12, yy = y + 4800 - a, mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}
void ymdOf(i64 jdn, i64& y, i64& m, i64& d) {
    const i64 a = jdn + 32044, b = (4 * a + 3) / 146097, c = a - 146097 * b / 4, dd = (4 * c + 3) / 1461, e = c - 1461 * dd / 4, mm = (5 * e + 2) / 153;
    d = e - (153 * mm + 2) / 5 + 1; m = mm + 3 - 12 * (mm / 10); y = 100 * b + dd - 4800 + mm / 10;
}
bool leap(i64 y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
int monthDays(i64 y, i64 m) { static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; return d[m - 1] + (m == 2 && leap(y) ? 1 : 0); }
constexpr i64 kJdnMin = 1721426, kJdnMax = 5373484;     // 0001-01-01 .. 9999-12-31 (gregoriano prolettico)
i64 checkedJdn(i64 j) { need(j >= kJdnMin && j <= kJdnMax, "date dall'anno 1 al 9999"); return j; }
i64 integerField(const std::string& t, const char* what) {
    const std::string s = trim(t);
    if (s.empty() || s.size() > 9) syntax(std::string(what) + " non valido");
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) syntax(std::string(what) + " non valido");
    return std::atoll(s.c_str());
}
i64 parseDate(const std::string& text) {
    const std::string s = trim(text);
    i64 y, m, d;
    const auto dash = s.find('-'), slash = s.find('/');
    if (dash != std::string::npos && dash > 0) {
        const auto dash2 = s.find('-', dash + 1);
        if (dash2 == std::string::npos) syntax("data AAAA-MM-GG");
        y = integerField(s.substr(0, dash), "anno"); m = integerField(s.substr(dash + 1, dash2 - dash - 1), "mese"); d = integerField(s.substr(dash2 + 1), "giorno");
    } else if (slash != std::string::npos) {
        const auto slash2 = s.find('/', slash + 1);
        if (slash2 == std::string::npos) syntax("data GG/MM/AAAA");
        d = integerField(s.substr(0, slash), "giorno"); m = integerField(s.substr(slash + 1, slash2 - slash - 1), "mese"); y = integerField(s.substr(slash2 + 1), "anno");
    } else syntax("data AAAA-MM-GG o GG/MM/AAAA");
    need(y >= 1 && y <= 9999, "anno da 1 a 9999");
    need(m >= 1 && m <= 12, "mese da 1 a 12");
    need(d >= 1 && d <= monthDays(y, m), "giorno inesistente");
    return jdnOf(y, m, d);
}
std::string dateText(i64 jdn) {
    i64 y, m, d; ymdOf(jdn, y, m, d);
    char b[24]; std::snprintf(b, sizeof b, "%04lld-%02lld-%02lld", static_cast<long long>(y), static_cast<long long>(m), static_cast<long long>(d));
    return b;
}
int isoWeekday(i64 jdn) { return static_cast<int>(jdn % 7) + 1; }   // 1 = lunedi'
void isoWeek(i64 jdn, i64& year, i64& week) {
    const i64 thursday = jdn - (isoWeekday(jdn) - 1) + 3;
    i64 m, d; ymdOf(thursday, year, m, d);
    week = (thursday - jdnOf(year, 1, 1)) / 7 + 1;
}
i64 parseIsoWeek(const std::string& text) {
    const std::string s = upper(trim(text));
    const auto w = s.find("-W");
    if (w == std::string::npos) syntax("settimana AAAA-Wss-g");
    const auto dash = s.find('-', w + 2);
    const i64 y = integerField(s.substr(0, w), "anno"), week = integerField(s.substr(w + 2, dash == std::string::npos ? std::string::npos : dash - w - 2), "settimana");
    const i64 wd = dash == std::string::npos ? 1 : integerField(s.substr(dash + 1), "giorno");
    need(y >= 1 && y <= 9999 && week >= 1 && week <= 53 && wd >= 1 && wd <= 7, "settimana ISO 1-53, giorno 1-7");
    const i64 jan4 = jdnOf(y, 1, 4);
    const i64 j = jan4 - (isoWeekday(jan4) - 1) + (week - 1) * 7 + (wd - 1);
    i64 yy, ww; isoWeek(j, yy, ww);
    need(yy == y, "settimana inesistente in quell'anno");
    return j;
}
i64 dayDifference(i64 a, i64 b) {
    i64 y1, m1, d1, y2, m2, d2;
    ymdOf(a, y1, m1, d1); ymdOf(b, y2, m2, d2);
    // Days_Prd di Run-Matrix (1902-2097) quando le date sono nel suo intervallo.
    if (y1 >= 1902 && y1 <= 2097 && y2 >= 1902 && y2 <= 2097)
        return static_cast<i64>(calclib::daysPrd(static_cast<double>(m1), static_cast<double>(d1), static_cast<double>(y1),
                                                 static_cast<double>(m2), static_cast<double>(d2), static_cast<double>(y2)));
    return b - a;
}

// ---------------------------------------------------------------- colori
struct Rgb { double r, g, b; };
double hue(double h) { h = std::fmod(h, 360); return h < 0 ? h + 360 : h; }
Rgb fromHsl(double h, double s, double l) {
    const double c = (1 - std::fabs(2 * l - 1)) * s, hp = hue(h) / 60, x = c * (1 - std::fabs(std::fmod(hp, 2) - 1)), m = l - c / 2;
    double r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    return {(r + m) * 255, (g + m) * 255, (b + m) * 255};
}
void toHsx(const Rgb& c, double& h, double& max, double& min) {
    const double r = c.r / 255, g = c.g / 255, b = c.b / 255;
    max = std::max({r, g, b}); min = std::min({r, g, b});
    const double d = max - min;
    h = d == 0 ? 0 : max == r ? 60 * std::fmod((g - b) / d, 6) : max == g ? 60 * ((b - r) / d + 2) : 60 * ((r - g) / d + 4);
    h = hue(h);
}
int byte(double v) { return static_cast<int>(std::lround(std::clamp(v, 0.0, 255.0))); }
std::uint16_t rgb565(const Rgb& c) {
    return static_cast<std::uint16_t>(((byte(c.r) >> 3) << 11) | ((byte(c.g) >> 2) << 5) | (byte(c.b) >> 3));
}
Rgb parseColor(std::size_t rep, const std::string& text) {
    switch (rep) {
    case 0: {
        std::string s = trim(text);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        if (s.size() == 3) s = std::string{s[0], s[0], s[1], s[1], s[2], s[2]};
        if (s.size() != 6) syntax("colore #RRGGBB");
        const u64 v = parseUnsigned(s, 16, "colore");
        return {static_cast<double>(v >> 16), static_cast<double>((v >> 8) & 0xFF), static_cast<double>(v & 0xFF)};
    }
    case 1: {
        const auto v = numbers(text, 3, "R, G, B");
        for (double x : v) need(x >= 0 && x <= 255, "R, G, B da 0 a 255");
        return {v[0], v[1], v[2]};
    }
    case 2: case 3: {
        const auto v = numbers(text, 3, rep == 2 ? "H, S, L" : "H, S, V");
        need(v[0] >= 0 && v[0] <= 360 && v[1] >= 0 && v[1] <= 100 && v[2] >= 0 && v[2] <= 100, "H 0-360, S e L/V 0-100");
        const double s = v[1] / 100, x = v[2] / 100;
        if (rep == 2) return fromHsl(v[0], s, x);
        const double l = x * (1 - s / 2), sl = (l == 0 || l == 1) ? 0 : (x - l) / std::min(l, 1 - l);
        return fromHsl(v[0], sl, l);
    }
    case 4: {
        const auto v = numbers(text, 4, "C, M, Y, K");
        for (double x : v) need(x >= 0 && x <= 100, "C, M, Y, K da 0 a 100");
        const double k = 1 - v[3] / 100;
        return {255 * (1 - v[0] / 100) * k, 255 * (1 - v[1] / 100) * k, 255 * (1 - v[2] / 100) * k};
    }
    default: {
        const u64 v = parseUnsigned(text, 16, "RGB565");
        need(v <= 0xFFFF, "RGB565 da 0000 a FFFF");
        const unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        return {static_cast<double>((r << 3) | (r >> 2)), static_cast<double>((g << 2) | (g >> 4)), static_cast<double>((b << 3) | (b >> 2))};
    }
    }
}
std::string formatColor(std::size_t rep, const Rgb& c) {
    switch (rep) {
    case 0: return "#" + hex(static_cast<u64>((byte(c.r) << 16) | (byte(c.g) << 8) | byte(c.b)), 6);
    case 1: return std::to_string(byte(c.r)) + ", " + std::to_string(byte(c.g)) + ", " + std::to_string(byte(c.b));
    case 2: case 3: {
        double h, mx, mn; toHsx(c, h, mx, mn);
        const double l = (mx + mn) / 2;
        const double s = rep == 2 ? (mx == mn ? 0 : (mx - mn) / (1 - std::fabs(2 * l - 1))) : (mx == 0 ? 0 : (mx - mn) / mx);
        return fixed(h, 1) + "\xC2\xB0, " + fixed(s * 100, 1) + "%, " + fixed((rep == 2 ? l : mx) * 100, 1) + "%";
    }
    case 4: {
        const double r = c.r / 255, g = c.g / 255, b = c.b / 255, k = 1 - std::max({r, g, b});
        if (k >= 1) return "0%, 0%, 0%, 100%";
        return fixed((1 - r - k) / (1 - k) * 100, 1) + "%, " + fixed((1 - g - k) / (1 - k) * 100, 1) + "%, " + fixed((1 - b - k) / (1 - k) * 100, 1) + "%, " + fixed(k * 100, 1) + "%";
    }
    default: return "0x" + hex(rgb565(c), 4);
    }
}

// ---------------------------------------------------------------- notazioni
struct Notation { double v{}; bool rational{}; exact::Rational r; };
const char* const kSiSymbols[] = {"q", "r", "y", "z", "a", "f", "p", "n", "\xC2\xB5", "m", "", "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};
Notation parseNotation(std::size_t rep, const std::string& text) {
    std::string s = trim(text);
    Notation n;
    if (s.empty()) syntax("numero mancante");
    const bool neg = s[0] == '-';
    std::string body = (s[0] == '-' || s[0] == '+') ? trim(s.substr(1)) : s;
    auto setRational = [&](const exact::Rational& r) {
        n.rational = true; n.r = r;
        if (neg && !exact::mul(n.r, exact::Rational::integer(-1), n.r)) argument("fuori intervallo");
        n.v = n.r.toDouble();
    };
    if (rep == 1 || rep == 2) {                     // frazione n/d o mista a b/c
        const auto slash = body.find('/');
        if (slash == std::string::npos) syntax("frazione n/d");
        const auto space = body.find(' ');
        i64 whole = 0;
        std::string fr = body;
        if (space != std::string::npos && space < slash) { whole = integerField(body.substr(0, space), "intero"); fr = trim(body.substr(space + 1)); }
        const auto sl = fr.find('/');
        const i64 a = integerField(fr.substr(0, sl), "numeratore"), b = integerField(fr.substr(sl + 1), "denominatore");
        if (b == 0) throw MathError("Math ERROR: denominatore zero");
        exact::Rational r, w = exact::Rational::integer(whole);
        if (!exact::Rational::make(a, b, r) || !exact::add(r, w, r)) argument("fuori intervallo");
        setRational(r);
        return n;
    }
    if (rep == 3) {                                 // periodico 0.1(6)
        const auto open = body.find('(');
        std::string t = body;
        if (open != std::string::npos) {
            if (body.back() != ')' || body.find('.') == std::string::npos || body.find('.') > open) syntax("periodo: 0.1(6)");
            t = body.substr(0, open) + "_" + body.substr(open + 1, body.size() - open - 2);
        }
        exact::Rational r;
        if (!exact::parseDecimal(t, r)) syntax("numero periodico non valido");
        setRational(r);
        return n;
    }
    if (rep == 5) {                                 // simbolo SI in coda
        for (int i = 0; i < 21; ++i) {
            const char* sym = kSiSymbols[i];
            const std::size_t len = std::strlen(sym);
            if (len && body.size() > len && body.compare(body.size() - len, len, sym) == 0) {
                n.v = number(body.substr(0, body.size() - len)) * std::pow(10.0, (i - 10) * 3);
                if (neg) n.v = -n.v;
                return n;
            }
        }
    }
    exact::Rational r;
    if (exact::parseDecimal(body, r)) { setRational(r); return n; }
    n.v = number(body);
    if (neg) n.v = -n.v;
    return n;
}
std::string formatNotation(std::size_t rep, const Notation& n) {
    const double v = n.v;
    switch (rep) {
    case 0: { char b[32]; std::snprintf(b, sizeof b, "%.15g", v); return b; }
    case 1: case 2: {
        exact::Rational r = n.r;
        if (!n.rational) {
            long long a = 0, b = 1;
            if (!exactFraction(v, a, b, 1000000)) argument("nessuna frazione con denominatore <= 1E6");
            if (!exact::Rational::make(a, b, r)) argument("fuori intervallo");
        }
        if (rep == 1 || r.den == 1 || (r.num > -r.den && r.num < r.den)) return std::to_string(r.num) + (r.den == 1 ? "" : "/" + std::to_string(r.den));
        const i64 whole = r.num / r.den, rem = r.num % r.den;
        return std::to_string(whole) + " " + std::to_string(rem < 0 ? -rem : rem) + "/" + std::to_string(r.den);
    }
    case 3: {
        exact::Rational r = n.r;
        if (!n.rational) {
            long long a = 0, b = 1;
            if (!exactFraction(v, a, b, 1000000) || !exact::Rational::make(a, b, r)) argument("non e' un numero razionale");
        }
        const exact::Expansion e = exact::decimalExpansion(r, 60);
        std::string s = (e.negative ? "-" : "") + e.integer;
        if (!e.fixed.empty() || !e.period.empty()) s += "." + e.fixed;
        if (!e.period.empty()) s += "(" + e.period + ")" + (e.truncated ? "\xE2\x80\xA6" : "");
        return s;
    }
    default: {
        if (v == 0) return rep == 4 ? "0E+00" : rep == 5 ? "0" : "0E+00";
        int e = static_cast<int>(std::floor(std::log10(std::fabs(v))));
        if (rep != 4) e = static_cast<int>(std::floor(e / 3.0)) * 3;
        double m = v / std::pow(10.0, e);
        std::string ms = fixed(m, 11);
        if (std::fabs(std::strtod(ms.c_str(), nullptr)) >= (rep == 4 ? 10 : 1000)) { e += rep == 4 ? 1 : 3; ms = fixed(v / std::pow(10.0, e), 11); }
        if (rep == 5 && e >= -30 && e <= 30) return ms + kSiSymbols[e / 3 + 10];
        char b[16]; std::snprintf(b, sizeof b, "E%+03d", e);
        return ms + b;
    }
    }
}

// ---------------------------------------------------------------- anelli (approssimato)
constexpr double kUkStart = 37.5, kUkStep = 1.25, kUsBase = 11.63, kUsStep = 0.8128;
double parseRing(std::size_t rep, const std::string& text) {
    switch (rep) {
    case 0: { const double d = number(text); need(d >= 10 && d <= 30, "diametro da 10 a 30 mm"); return d; }
    case 1: { const double c = number(text); need(c >= 31.4 && c <= 94.3, "circonferenza da 31,4 a 94,3 mm"); return c / kPi; }
    case 2: { const double u = number(text); need(u >= 0 && u <= 16, "misura US da 0 a 16"); return kUsBase + kUsStep * u; }
    default: {
        std::string s = upper(trim(text));
        if (s.empty() || s[0] < 'A' || s[0] > 'Z') syntax("misura UK: lettera A-Z (con \xC2\xBD)");
        double k = s[0] - 'A';
        const std::string rest = trim(s.substr(1));
        if (rest == "\xC2\xBD" || rest == "1/2" || rest == ".5") k += 0.5;
        else if (!rest.empty()) syntax("misura UK: lettera A-Z (con \xC2\xBD)");
        return (kUkStart + kUkStep * k) / kPi;
    }
    }
}
std::string formatRing(std::size_t rep, double d, const Options& o) {
    switch (rep) {
    case 0: return num(d, o);
    case 1: return num(d * kPi, o);
    case 2: { const double u = (d - kUsBase) / kUsStep; need(u >= -0.25 && u <= 16.25, "fuori dalla tabella US"); return fixed(std::round(u * 4) / 4, 2); }
    default: {
        const double k = std::round((d * kPi - kUkStart) / kUkStep * 2) / 2;
        need(k >= 0 && k <= 25.5, "fuori dalla tabella UK (A-Z)");
        std::string s(1, static_cast<char>('A' + static_cast<int>(k)));
        if (k != std::floor(k)) s += "\xC2\xBD";
        return s;
    }
    }
}

// ---------------------------------------------------------------- tabella
using G = units::Group;
const char* const kBases[] = {"decimale", "esadecimale", "binario", "ottale", "base N", nullptr};
const char* const kTwos[] = {"decimale con segno", "esadecimale", "binario", nullptr};
const char* const kBcd[] = {"decimale", "BCD (binario)", nullptr};
const char* const kGray[] = {"decimale", "binario", "codice Gray", nullptr};
const char* const kIeee[] = {"valore", "half (16 bit)", "single (32 bit)", "double (64 bit)", nullptr};
const char* const kRoman[] = {"decimale", "romano (1-3999)", "romano con vinculum (x1000)", nullptr};
const char* const kText[] = {"testo", "Unicode U+", "codici decimali", "byte UTF-8", nullptr};
const char* const kAngles[] = {"gradi decimali", "gradi \xC2\xB0 \xE2\x80\xB2 \xE2\x80\xB3", "ore angolari h m s", "radianti", "gradianti", nullptr};
const char* const kTimes[] = {"ore decimali", "h:m:s", "secondi", "minuti decimali", "giorni d h:m:s", nullptr};
const char* const kDates[] = {"data AAAA-MM-GG", "giorno giuliano JD", "MJD", "giorno dell'anno", "settimana ISO", "timestamp Unix", "giorno della settimana", nullptr};
const char* const kDiff[] = {"due date (A B)", "giorni (B-A)", "settimane e giorni", "anni, mesi, giorni", nullptr};
const char* const k2d[] = {"rettangolari x, y", "polari r, \xCE\xB8", nullptr};
const char* const k3d[] = {"cartesiane x, y, z", "cilindriche \xCF\x81, \xCF\x86, z", "sferiche r, \xCE\xB8, \xCF\x86", nullptr};
const char* const kGeo[] = {"gradi decimali lat, lon", "DMS con N/S E/W", nullptr};
const char* const kComplex[] = {"a+bi", "r\xE2\x88\xA0\xCE\xB8", nullptr};
const char* const kNotation[] = {"decimale", "frazione n/d", "frazione mista a b/c", "periodico 0.1(6)", "scientifica", "ingegneristica (simboli SI)", nullptr};
const char* const kColors[] = {"HEX #RRGGBB", "RGB", "HSL", "HSV", "CMYK", "RGB565 (display PicoCalc)", nullptr};
const char* const kRates[] = {"nominale APR %", "effettivo EFF %", "per periodo %", nullptr};
const char* const kRings[] = {"diametro interno mm", "circonferenza mm (EU/ISO)", "US/Canada", "UK/Australia", nullptr};
enum : std::size_t { C_BASE, C_TWOS, C_BCD, C_GRAY, C_IEEE, C_ROMAN, C_TEXT, C_ANGLE, C_TIME, C_DATE, C_DIFF, C_2D, C_3D, C_GEO,
                     C_COMPLEX, C_NOTATION, C_COLOR, C_RATE, C_RING, C_COUNT };
const Converter kConverters[C_COUNT] = {
    {"Basi numeriche", G::Formats, kBases, 0x1F, "base N", 12, 2, 36, "parte frazionaria con il punto; Base-N di Run-Matrix"},
    {"Complemento a due", G::Formats, kTwos, 0x7, "bit", 16, 8, 64, "8, 16, 32 o 64 bit"},
    {"BCD", G::Formats, kBcd, 0x3, nullptr, 0, 0, 0, "una cifra decimale per gruppo di 4 bit"},
    {"Codice Gray", G::Formats, kGray, 0x7, nullptr, 0, 0, 0, "Gray = n XOR (n >> 1)"},
    {"IEEE-754", G::Formats, kIeee, 0xF, nullptr, 0, 0, 0, "esadecimale, segno s, esponente e, mantissa m"},
    {"Numeri romani", G::Formats, kRoman, 0x7, nullptr, 0, 0, 0, "vinculum: (IV) = 4000"},
    {"Testo \xE2\x86\x94 Unicode", G::Formats, kText, 0xF, nullptr, 0, 0, 0, nullptr},
    {"Angoli (DMS, HMS)", G::Formats, kAngles, 0x1F, nullptr, 0, 0, 0, "12\xC2\xB0" "30\xE2\x80\xB2" "15\xE2\x80\xB3 anche come 12 30 15 o 12:30:15"},
    {"Tempo (h:m:s)", G::Formats, kTimes, 0x1F, nullptr, 0, 0, 0, nullptr},
    {"Date", G::Formats, kDates, 0x3F, nullptr, 0, 0, 0, "calendario gregoriano prolettico, anni 1-9999; JD alle 0:00 TU"},
    {"Differenza tra date", G::Formats, kDiff, 0x1, nullptr, 0, 0, 0, "Days_Prd di Run-Matrix (1902-2097), giorno giuliano altrove"},
    {"Coordinate 2D", G::Formats, k2d, 0x3, nullptr, 0, 0, 0, "angoli nell'unita' del SET UP"},
    {"Coordinate 3D", G::Formats, k3d, 0x7, nullptr, 0, 0, 0, "\xCE\xB8 dall'asse z, \xCF\x86 azimut (ISO 80000-2); angoli del SET UP"},
    {"Coordinate geografiche", G::Formats, kGeo, 0x3, nullptr, 0, 0, 0, "latitudine +-90, longitudine +-180"},
    {"Numeri complessi", G::Formats, kComplex, 0x3, nullptr, 0, 0, 0, "angolo nell'unita' del SET UP"},
    {"Notazioni numeriche", G::Formats, kNotation, 0x3F, nullptr, 0, 0, 0, "S\xE2\x87\x94" "D, ab/c, ENG come in Run-Matrix"},
    {"Colori", G::Formats, kColors, 0x3F, nullptr, 0, 0, 0, nullptr},
    {"Tassi APR \xE2\x86\x94 EFF", G::Formats, kRates, 0x7, "capitalizzazioni/anno", 12, 1, 1e6, "Cnvt_EFF / Cnvt_APR di Run-Matrix"},
    {"Anelli", G::Sizes, kRings, 0xF, nullptr, 0, 0, 0, "APPROSSIMATO: UK A = 37,5 mm (+1,25 per lettera), US d = 11,63 + 0,8128\xC2\xB7n mm"},
};

// Valore intermedio di un convertitore.
struct State {
    Real real;                  // basi
    u64 bitsValue{};            // complemento a due, BCD, Gray, IEEE (double)
    double d[4]{};              // angoli, tempo, coordinate, colori, tassi, anelli
    i64 day{};                  // date (giorno giuliano a mezzogiorno)
    i64 day2{};                 // differenza tra date
    std::vector<std::uint32_t> text;
    Notation notation;
    Rgb color{};
};

State parse(std::size_t c, std::size_t rep, const std::string& in, const Options& o) {
    State st;
    const std::string t = trim(in);
    if (t.empty() && c != C_TEXT) syntax("valore mancante");
    switch (c) {
    case C_BASE: st.real = parseBase(t, baseOf(rep, o)); break;
    case C_TWOS: {
        const unsigned w = widthOf(o);
        if (rep == 0) {
            const i64 v = parseSigned(t);
            const i64 lo = w == 64 ? INT64_MIN : -(i64{1} << (w - 1)), hi = w == 64 ? INT64_MAX : (i64{1} << (w - 1)) - 1;
            need(v >= lo && v <= hi, "fuori dall'intervallo dei bit scelti");
            st.bitsValue = static_cast<u64>(v) & maskOf(w);
        } else {
            st.bitsValue = parseUnsigned(t, rep == 1 ? 16 : 2, "numero");
            need((st.bitsValue & ~maskOf(w)) == 0, "piu' bit di quelli scelti");
        }
        break;
    }
    case C_BCD:
        if (rep == 0) { st.bitsValue = parseUnsigned(t, 10, "numero"); need(st.bitsValue <= 9999999999999999ull, "al massimo 16 cifre"); }
        else {
            std::string s = t;
            s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
            if (s.empty() || s.size() > 64) syntax("BCD: da 1 a 64 bit");
            while (s.size() % 4) s.insert(s.begin(), '0');
            u64 v = 0;
            for (std::size_t i = 0; i < s.size(); i += 4) {
                const u64 nib = parseUnsigned(s.substr(i, 4), 2, "BCD");
                need(nib <= 9, "gruppo BCD maggiore di 1001");
                v = v * 10 + nib;
            }
            st.bitsValue = v;
        }
        break;
    case C_GRAY: {
        u64 v = parseUnsigned(t, rep == 0 ? 10 : 2, "numero");
        if (rep == 2) { for (u64 s = v >> 1; s; s >>= 1) v ^= s; }
        st.bitsValue = v;
        break;
    }
    case C_IEEE: {
        if (rep == 0) {
            const std::string u = upper(t);
            if (u == "NAN") st.d[0] = NAN;
            else if (u == "INF" || u == "+INF") st.d[0] = INFINITY;
            else if (u == "-INF") st.d[0] = -INFINITY;
            else {
                if (t.find_first_not_of("0123456789+-.eE") != std::string::npos) syntax("numero non valido");
                char* end = nullptr;
                st.d[0] = std::strtod(t.c_str(), &end);
                if (!end || *end) syntax("numero non valido");
            }
        } else {
            const auto first = tokens(t);
            const unsigned w = rep == 1 ? 16 : rep == 2 ? 32 : 64;
            const u64 b = parseUnsigned(first.empty() ? "" : first[0], 16, "esadecimale");
            need((b & ~maskOf(w)) == 0, "troppe cifre esadecimali");
            if (rep == 1) st.d[0] = fromHalf(static_cast<std::uint16_t>(b));
            else if (rep == 2) { const std::uint32_t x = static_cast<std::uint32_t>(b); float f; std::memcpy(&f, &x, 4); st.d[0] = f; }
            else st.d[0] = fromBits(b);
        }
        break;
    }
    case C_ROMAN:
        if (rep == 0) { const i64 v = parseSigned(t); need(v >= 1 && v <= 3999999, "da 1 a 3999999"); st.bitsValue = static_cast<u64>(v); }
        else st.bitsValue = parseRoman(t, rep == 2);
        break;
    case C_TEXT:
        if (rep == 0) st.text = decodeUtf8(in);
        else if (rep == 3) {
            std::string s = upper(t);
            s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
            if (s.size() % 2) syntax("byte UTF-8: coppie di cifre esadecimali");
            std::string bytes;
            for (std::size_t i = 0; i < s.size(); i += 2) bytes += static_cast<char>(parseUnsigned(s.substr(i, 2), 16, "byte"));
            st.text = decodeUtf8(bytes);
        } else {
            for (std::string tok : tokens(t)) {
                if (rep == 1 && tok.size() > 2 && (tok[0] == 'U' || tok[0] == 'u') && tok[1] == '+') tok = tok.substr(2);
                const u64 cp = parseUnsigned(tok, rep == 1 ? 16 : 10, "codice");
                need(cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF), "codice Unicode non valido");
                st.text.push_back(static_cast<std::uint32_t>(cp));
            }
        }
        if (st.text.size() > 64) argument("al massimo 64 caratteri");
        break;
    case C_ANGLE: {
        double sign = 1;
        if (rep == 1) st.d[0] = parseSexagesimal(t, sign, nullptr);
        else if (rep == 2) st.d[0] = parseSexagesimal(t, sign, nullptr) * 15;
        else { const double v = number(t); st.d[0] = rep == 0 ? v : rep == 3 ? v * 180 / kPi : v * 0.9; }
        break;
    }
    case C_TIME: {
        double sign = 1;
        if (rep == 0) st.d[0] = number(t) * 3600;
        else if (rep == 1) st.d[0] = parseSexagesimal(t, sign, nullptr) * 3600;
        else if (rep == 2) st.d[0] = number(t);
        else if (rep == 3) st.d[0] = number(t) * 60;
        else {
            std::string s = t;
            const auto dpos = s.find_first_of("dDgG");
            double days = 0;
            if (dpos != std::string::npos) { days = number(s.substr(0, dpos)); s = trim(s.substr(dpos + 1)); }
            if (days < 0) syntax("giorni negativi");
            st.d[0] = days * 86400 + (s.empty() ? 0 : parseSexagesimal(s, sign, nullptr) * 3600);
        }
        break;
    }
    case C_DATE:
        switch (rep) {
        case 0: st.day = parseDate(t); break;
        case 1: { const double jd = number(t); need(jd >= kJdnMin - 0.5 && jd < kJdnMax + 0.5, "date dall'anno 1 al 9999"); st.day = static_cast<i64>(std::floor(jd + 0.5)); break; }
        case 2: { const double mjd = number(t); st.day = checkedJdn(static_cast<i64>(std::floor(std::clamp(mjd, -1e9, 1e9))) + 2400001); break; }
        case 3: {
            const auto dash = t.find('-');
            if (dash == std::string::npos) syntax("giorno dell'anno AAAA-GGG");
            const i64 y = integerField(t.substr(0, dash), "anno"), n = integerField(t.substr(dash + 1), "giorno");
            need(y >= 1 && y <= 9999 && n >= 1 && n <= (leap(y) ? 366 : 365), "giorno dell'anno inesistente");
            st.day = jdnOf(y, 1, 1) + n - 1;
            break;
        }
        case 4: st.day = parseIsoWeek(t); break;
        case 5: { const double s = number(t); st.day = checkedJdn(static_cast<i64>(std::floor(std::clamp(s, -1e14, 1e14) / 86400)) + 2440588); break; }
        default: argument("rappresentazione solo in uscita");
        }
        st.day = checkedJdn(st.day);
        break;
    case C_DIFF: {
        auto parts = tokens(t);
        if (parts.size() != 2) syntax("due date separate da uno spazio");
        st.day = checkedJdn(parseDate(parts[0])); st.day2 = checkedJdn(parseDate(parts[1]));
        break;
    }
    case C_2D: {
        const auto v = numbers(t, 2, rep == 0 ? "x, y" : "r, \xCE\xB8");
        if (rep == 0) { st.d[0] = v[0]; st.d[1] = v[1]; }
        else { need(v[0] >= 0, "r >= 0"); const double a = angleToRad(v[1], o); st.d[0] = snap(v[0] * std::cos(a), v[0]); st.d[1] = snap(v[0] * std::sin(a), v[0]); }
        break;
    }
    case C_3D: {
        const auto v = numbers(t, 3, rep == 0 ? "x, y, z" : rep == 1 ? "\xCF\x81, \xCF\x86, z" : "r, \xCE\xB8, \xCF\x86");
        if (rep == 0) { st.d[0] = v[0]; st.d[1] = v[1]; st.d[2] = v[2]; }
        else if (rep == 1) { need(v[0] >= 0, "\xCF\x81 >= 0"); const double p = angleToRad(v[1], o); st.d[0] = snap(v[0] * std::cos(p), v[0]); st.d[1] = snap(v[0] * std::sin(p), v[0]); st.d[2] = v[2]; }
        else {
            need(v[0] >= 0, "r >= 0");
            const double th = angleToRad(v[1], o), ph = angleToRad(v[2], o);
            need(th >= -1e-12 && th <= kPi + 1e-12, "\xCE\xB8 da 0 a 180\xC2\xB0");
            st.d[0] = snap(v[0] * std::sin(th) * std::cos(ph), v[0]); st.d[1] = snap(v[0] * std::sin(th) * std::sin(ph), v[0]); st.d[2] = snap(v[0] * std::cos(th), v[0]);
        }
        break;
    }
    case C_GEO: {
        if (rep == 0) { const auto v = numbers(t, 2, "lat, lon"); st.d[0] = v[0]; st.d[1] = v[1]; }
        else {
            const std::string s = upper(t);
            std::string a, b;
            const auto comma = s.find(',');
            if (comma != std::string::npos) { a = s.substr(0, comma); b = s.substr(comma + 1); }
            else {
                const auto h = s.find_first_of("NS");
                if (h == std::string::npos) syntax("latitudine, longitudine (N/S, E/W)");
                a = s.substr(0, h + 1); b = s.substr(h + 1);
            }
            double h1 = 1, h2 = 1;
            st.d[0] = parseSexagesimal(a, h1, "NS") * h1;
            st.d[1] = parseSexagesimal(b, h2, "EWEO") * h2;
        }
        need(std::fabs(st.d[0]) <= 90 && std::fabs(st.d[1]) <= 180, "latitudine +-90, longitudine +-180");
        break;
    }
    case C_COMPLEX: {
        static const char* const angle[] = {"\xE2\x88\xA0", nullptr};
        std::string s = ascii(t, angle, '<');
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        if (rep == 1) {
            const auto p = s.find('<');
            if (p == std::string::npos) syntax("forma polare r\xE2\x88\xA0\xCE\xB8");
            const double r = number(s.substr(0, p)), a = angleToRad(number(s.substr(p + 1)), o);
            need(r >= 0, "r >= 0");
            st.d[0] = snap(r * std::cos(a), r); st.d[1] = snap(r * std::sin(a), r);
        } else if (!s.empty() && s.back() == 'i') {
            std::size_t split = std::string::npos;
            for (std::size_t i = s.size() - 1; i-- > 1;) if ((s[i] == '+' || s[i] == '-') && s[i - 1] != 'e' && s[i - 1] != 'E') { split = i; break; }
            const std::string re = split == std::string::npos ? "" : s.substr(0, split);
            std::string im = s.substr(split == std::string::npos ? 0 : split, s.size() - (split == std::string::npos ? 0 : split) - 1);
            if (im.empty() || im == "+") im = "1"; else if (im == "-") im = "-1";
            st.d[0] = re.empty() ? 0 : number(re); st.d[1] = number(im);
        } else { st.d[0] = number(s); st.d[1] = 0; }
        break;
    }
    case C_NOTATION: st.notation = parseNotation(rep, t); break;
    case C_COLOR: st.color = parseColor(rep, t); break;
    case C_RATE: {
        need(o.param >= 1 && o.param <= 1e6, "capitalizzazioni da 1 a 1E6");
        const double v = number(t);
        st.d[0] = rep == 0 ? v : rep == 1 ? calclib::cnvtAPR(o.param, v) : v * o.param;
        if (!std::isfinite(st.d[0])) throw MathError("Math ERROR: tasso fuori dominio");
        break;
    }
    case C_RING: st.d[0] = parseRing(rep, t); break;
    default: argument("convertitore sconosciuto");
    }
    return st;
}

std::string format(std::size_t c, std::size_t rep, const State& st, const Options& o) {
    switch (c) {
    case C_BASE: return formatBase(st.real, baseOf(rep, o));
    case C_TWOS: {
        const unsigned w = widthOf(o);
        const u64 v = st.bitsValue;
        if (rep == 1) return hex(v, static_cast<int>(w / 4));
        if (rep == 2) return bits(v, w);
        const bool neg = w < 64 ? (v >> (w - 1)) & 1 : (v >> 63) & 1;
        if (!neg) return std::to_string(v);
        const u64 mag = (~v + 1) & maskOf(w);
        return "-" + std::to_string(mag == 0 ? (u64{1} << 63) : mag);
    }
    case C_BCD: {
        if (rep == 0) return std::to_string(st.bitsValue);
        std::string d = std::to_string(st.bitsValue), out;
        for (char ch : d) { if (!out.empty()) out += ' '; out += bits(static_cast<u64>(ch - '0'), 4); }
        return out;
    }
    case C_GRAY: {
        const u64 v = st.bitsValue;
        return rep == 0 ? std::to_string(v) : toBase(rep == 1 ? v : v ^ (v >> 1), 2);
    }
    case C_IEEE: {
        const double v = st.d[0];
        if (rep == 0) return shortest(v);
        if (rep == 1) return ieeeText(toHalf(v), 5, 10);
        if (rep == 2) {
            const float f = static_cast<float>(v);
            if (std::isfinite(v) && !std::isfinite(f)) argument("oltre il massimo di single (3,4E38)");
            std::uint32_t x; std::memcpy(&x, &f, 4);
            return ieeeText(x, 8, 23);
        }
        return ieeeText(bitsOf(v), 11, 52);
    }
    case C_ROMAN: {
        const auto n = static_cast<unsigned>(st.bitsValue);
        if (rep == 0) return std::to_string(n);
        if (rep == 1) { need(n <= 3999, "romani senza vinculum fino a 3999"); return roman(n); }
        return romanVinculum(n);
    }
    case C_TEXT: {
        std::string out;
        char b[16];
        for (std::uint32_t cp : st.text) {
            if (rep == 0) { encodeUtf8(cp, out); continue; }
            if (!out.empty()) out += ' ';
            if (rep == 1) { std::snprintf(b, sizeof b, "U+%04X", static_cast<unsigned>(cp)); out += b; }
            else if (rep == 2) out += std::to_string(cp);
            else { std::string u; encodeUtf8(cp, u); for (std::size_t i = 0; i < u.size(); ++i) { if (i) out += ' '; out += hex(static_cast<unsigned char>(u[i]), 2); } }
        }
        return out;
    }
    case C_ANGLE: {
        const double d = st.d[0];
        switch (rep) {
        case 0: return num(d, o);
        case 1: return sexagesimal(d, "\xC2\xB0", "\xE2\x80\xB2", "\xE2\x80\xB3", 4);
        case 2: return sexagesimal(d / 15, "h", "m", "s", 4);
        case 3: return num(d * kPi / 180, o);
        default: return num(d / 0.9, o);
        }
    }
    case C_TIME: {
        const double s = st.d[0];
        switch (rep) {
        case 0: return num(s / 3600, o);
        case 1: return sexagesimal(s / 3600, ":", ":", "", 3);
        case 2: return num(s, o);
        case 3: return num(s / 60, o);
        default: {
            const bool neg = s < 0;
            const double a = std::fabs(s), days = std::floor(a / 86400);
            return std::string(neg ? "-" : "") + fixed(days, 0) + "d " + sexagesimal((a - days * 86400) / 3600, ":", ":", "", 3);
        }
        }
    }
    case C_DATE: {
        const i64 j = st.day;
        char b[40];
        switch (rep) {
        case 0: return dateText(j);
        case 1: std::snprintf(b, sizeof b, "%lld.5", static_cast<long long>(j - 1)); return b;
        case 2: return std::to_string(j - 2400001);
        case 3: { i64 y, m, d; ymdOf(j, y, m, d); std::snprintf(b, sizeof b, "%04lld-%03lld", static_cast<long long>(y), static_cast<long long>(j - jdnOf(y, 1, 1) + 1)); return b; }
        case 4: { i64 y, w; isoWeek(j, y, w); std::snprintf(b, sizeof b, "%04lld-W%02lld-%d", static_cast<long long>(y), static_cast<long long>(w), isoWeekday(j)); return b; }
        case 5: return std::to_string((j - 2440588) * 86400);
        default: {
            static const char* const names[] = {"lunedi'", "martedi'", "mercoledi'", "giovedi'", "venerdi'", "sabato", "domenica"};
            return names[isoWeekday(j) - 1];
        }
        }
    }
    case C_DIFF: {
        if (rep == 0) return dateText(st.day) + " " + dateText(st.day2);
        const i64 n = dayDifference(st.day, st.day2);
        if (rep == 1) return std::to_string(n);
        const i64 a = n < 0 ? -n : n;
        if (rep == 2) return std::string(n < 0 ? "-" : "") + std::to_string(a / 7) + " sett. " + std::to_string(a % 7) + " g";
        i64 y1, m1, d1, y2, m2, d2;
        const bool neg = st.day2 < st.day;
        ymdOf(neg ? st.day2 : st.day, y1, m1, d1); ymdOf(neg ? st.day : st.day2, y2, m2, d2);
        i64 y = y2 - y1, m = m2 - m1, d = d2 - d1;
        if (d < 0) { --m; const i64 pm = m2 == 1 ? 12 : m2 - 1, py = m2 == 1 ? y2 - 1 : y2; d += monthDays(py, pm); }
        if (m < 0) { --y; m += 12; }
        return std::string(neg ? "-" : "") + std::to_string(y) + " a " + std::to_string(m) + " m " + std::to_string(d) + " g";
    }
    case C_2D:
        if (rep == 0) return num(st.d[0], o) + ", " + num(st.d[1], o);
        return num(std::hypot(st.d[0], st.d[1]), o) + ", " + num(radToAngle(std::atan2(st.d[1], st.d[0]), o), o) + angleMark(o);
    case C_3D: {
        const double x = st.d[0], y = st.d[1], z = st.d[2];
        if (rep == 0) return num(x, o) + ", " + num(y, o) + ", " + num(z, o);
        const double rho = std::hypot(x, y), phi = radToAngle(std::atan2(y, x), o);
        if (rep == 1) return num(rho, o) + ", " + num(phi, o) + angleMark(o) + ", " + num(z, o);
        const double r = std::sqrt(x * x + y * y + z * z);
        return num(r, o) + ", " + num(r == 0 ? 0 : radToAngle(std::acos(std::clamp(z / r, -1.0, 1.0)), o), o) + angleMark(o) + ", " + num(phi, o) + angleMark(o);
    }
    case C_GEO:
        if (rep == 0) return num(st.d[0], o) + ", " + num(st.d[1], o);
        return sexagesimal(std::fabs(st.d[0]), "\xC2\xB0", "\xE2\x80\xB2", "\xE2\x80\xB3", 2) + (st.d[0] < 0 ? "S, " : "N, ") +
               sexagesimal(std::fabs(st.d[1]), "\xC2\xB0", "\xE2\x80\xB2", "\xE2\x80\xB3", 2) + (st.d[1] < 0 ? "W" : "E");
    case C_COMPLEX: {
        const double a = st.d[0], b = st.d[1];
        if (rep == 1) return num(std::hypot(a, b), o) + "\xE2\x88\xA0" + num(radToAngle(std::atan2(b, a), o), o);
        if (b == 0) return num(a, o);
        const std::string im = std::fabs(b) == 1 ? "" : num(std::fabs(b), o);
        return (a == 0 ? (b < 0 ? "-" : "") : num(a, o) + (b < 0 ? "-" : "+")) + im + "i";
    }
    case C_NOTATION: return formatNotation(rep, st.notation);
    case C_COLOR: return formatColor(rep, st.color);
    case C_RATE: {
        need(o.param >= 1 && o.param <= 1e6, "capitalizzazioni da 1 a 1E6");
        const double v = rep == 0 ? st.d[0] : rep == 1 ? calclib::cnvtEFF(o.param, st.d[0]) : st.d[0] / o.param;
        if (!std::isfinite(v)) throw MathError("Math ERROR: tasso fuori dominio");
        return num(v, o);
    }
    case C_RING: return formatRing(rep, st.d[0], o);
    default: argument("convertitore sconosciuto");
    }
}
} // namespace

std::size_t converterCount() noexcept { return C_COUNT; }
const Converter& converter(std::size_t i) noexcept { return kConverters[i < C_COUNT ? i : 0]; }
std::size_t repCount(const Converter& c) noexcept { std::size_t n = 0; while (c.reps[n]) ++n; return n; }
bool readable(const Converter& c, std::size_t rep) noexcept { return rep < 32 && ((c.inputMask >> rep) & 1u) != 0; }
std::size_t colorConverter() noexcept { return C_COLOR; }

std::string convert(std::size_t c, std::size_t from, std::size_t to, const std::string& input, const Options& opt) {
    if (c >= C_COUNT || from >= repCount(kConverters[c]) || to >= repCount(kConverters[c])) argument("rappresentazione sconosciuta");
    if (!readable(kConverters[c], from)) argument("rappresentazione solo in uscita");
    if (input.size() > 256) argument("testo troppo lungo");
    return format(c, to, parse(c, from, input, opt), opt);
}
bool colorOf(std::size_t c, std::size_t from, const std::string& input, const Options& opt, std::uint16_t& out) {
    if (c != C_COLOR) return false;
    try { out = rgb565(parse(c, from, input, opt).color); return true; } catch (const std::exception&) { return false; }
}

} // namespace fx::unitfmt
