// Formattazione dei valori (v1.20.0): printf del C con i controlli sugli
// argomenti, numeri reali di .NET (il piu' corto che torna uguale, notazione
// E oltre 1E+15), di Java (Double.toString) e degli stream del C++
// (precisione, fixed, scientific), formati compositi di C# ({0,5:F2},
// N2, D3, X, 0.00), testo dei valori per i linguaggi, viste del debug.
#include "program_vm.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fx::prog {

namespace {
std::string hexOf(std::uint64_t v, bool upper) {
    char buf[32];
    std::snprintf(buf, sizeof buf, upper ? "%llX" : "%llx", static_cast<unsigned long long>(v));
    return buf;
}
} // namespace

// Arrotondamento decimale sul valore esatto del double.
std::string fixedDecimal(double d, int decimals, bool awayFromZero) {
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d > 0 ? "Infinity" : "-Infinity";
    decimals = std::max(0, std::min(decimals, 30));
    if (!awayFromZero) {
        std::vector<char> buf(static_cast<std::size_t>(decimals) + 340);
        std::snprintf(buf.data(), buf.size(), "%.*f", decimals, d);
        return buf.data();
    }
    // espansione esatta (fino a 60 cifre decimali), poi arrotondamento lontano da zero
    std::vector<char> buf(420);
    std::snprintf(buf.data(), buf.size(), "%.60f", std::fabs(d));
    std::string s = buf.data();
    const std::size_t dot = s.find('.');
    std::string ip = s.substr(0, dot), fp = s.substr(dot + 1);
    const char next = static_cast<std::size_t>(decimals) < fp.size() ? fp[static_cast<std::size_t>(decimals)] : '0';
    fp = fp.substr(0, static_cast<std::size_t>(decimals));
    std::string digits = ip + fp;
    if (next >= '5') {
        int i = static_cast<int>(digits.size()) - 1;
        while (i >= 0) {
            if (digits[static_cast<std::size_t>(i)] == '9') { digits[static_cast<std::size_t>(i)] = '0'; --i; continue; }
            ++digits[static_cast<std::size_t>(i)];
            break;
        }
        if (i < 0) digits = "1" + digits;
    }
    const std::size_t ipLen = digits.size() - static_cast<std::size_t>(decimals);
    std::string out = digits.substr(0, ipLen);
    if (decimals > 0) out += "." + digits.substr(ipLen);
    bool zero = true;
    for (char c : out) if (c >= '1' && c <= '9') zero = false;
    if (d < 0 && !zero) out = "-" + out;
    return out;
}

// ---------------------------------------------------------------- .NET
// double.ToString(): la rappresentazione piu' corta che si rilegge uguale,
// in notazione E (E+XX) se l'esponente e' >= 15 o < -5.
std::string formatDoubleCs(double d) {
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d > 0 ? "\xE2\x88\x9E" : "-\xE2\x88\x9E";
    if (d == 0) return std::signbit(d) ? "-0" : "0";
    char buf[64];
    int prec = 1;
    for (; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*e", prec - 1, d);
        if (std::strtod(buf, nullptr) == d) break;
    }
    // mantissa e esponente
    std::string s = buf;
    const std::size_t e = s.find('e');
    std::string mant = s.substr(0, e);
    const int exp = std::atoi(s.c_str() + e + 1);
    const bool neg = mant[0] == '-';
    if (neg) mant.erase(0, 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits += c;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    std::string out;
    if (exp >= 15 || exp < -5) {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        char eb[16];
        std::snprintf(eb, sizeof eb, "E%c%02d", exp < 0 ? '-' : '+', std::abs(exp));
        out += eb;
    } else if (exp < 0) {
        out = "0." + std::string(static_cast<std::size_t>(-exp - 1), '0') + digits;
    } else {
        if (digits.size() <= static_cast<std::size_t>(exp) + 1) out = digits + std::string(static_cast<std::size_t>(exp) + 1 - digits.size(), '0');
        else out = digits.substr(0, static_cast<std::size_t>(exp) + 1) + "." + digits.substr(static_cast<std::size_t>(exp) + 1);
    }
    return neg ? "-" + out : out;
}
static std::string formatFloatCs(double d) {
    const float f = static_cast<float>(d);
    if (std::isnan(f) || std::isinf(f)) return formatDoubleCs(d);
    if (f == 0) return std::signbit(f) ? "-0" : "0";
    char buf[64];
    int prec = 1;
    for (; prec <= 9; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*e", prec - 1, static_cast<double>(f));
        if (std::strtof(buf, nullptr) == f) break;
    }
    // stessa forma del double con le cifre del float
    std::string s = buf;
    const std::size_t e = s.find('e');
    std::string mant = s.substr(0, e);
    const int exp = std::atoi(s.c_str() + e + 1);
    const bool neg = mant[0] == '-';
    if (neg) mant.erase(0, 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits += c;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    std::string out;
    if (exp >= 15 || exp < -5) {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        char eb[16];
        std::snprintf(eb, sizeof eb, "E%c%02d", exp < 0 ? '-' : '+', std::abs(exp));
        out += eb;
    } else if (exp < 0) out = "0." + std::string(static_cast<std::size_t>(-exp - 1), '0') + digits;
    else if (digits.size() <= static_cast<std::size_t>(exp) + 1) out = digits + std::string(static_cast<std::size_t>(exp) + 1 - digits.size(), '0');
    else out = digits.substr(0, static_cast<std::size_t>(exp) + 1) + "." + digits.substr(static_cast<std::size_t>(exp) + 1);
    return neg ? "-" + out : out;
}
// Double.toString di Java: almeno una cifra dopo il punto, E se < 1e-3 o >= 1e7.
std::string formatDoubleJava(double d) {
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d > 0 ? "Infinity" : "-Infinity";
    if (d == 0) return std::signbit(d) ? "-0.0" : "0.0";
    char buf[64];
    int prec = 1;
    for (; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*e", prec - 1, d);
        if (std::strtod(buf, nullptr) == d) break;
    }
    std::string s = buf;
    const std::size_t e = s.find('e');
    std::string mant = s.substr(0, e);
    const int exp = std::atoi(s.c_str() + e + 1);
    const bool neg = mant[0] == '-';
    if (neg) mant.erase(0, 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits += c;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    std::string out;
    const double a = std::fabs(d);
    if (a >= 1e-3 && a < 1e7) {
        if (exp < 0) out = "0." + std::string(static_cast<std::size_t>(-exp - 1), '0') + digits;
        else if (digits.size() <= static_cast<std::size_t>(exp) + 1) out = digits + std::string(static_cast<std::size_t>(exp) + 1 - digits.size(), '0') + ".0";
        else out = digits.substr(0, static_cast<std::size_t>(exp) + 1) + "." + digits.substr(static_cast<std::size_t>(exp) + 1);
    } else {
        out = digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : std::string("0")) + "E" + std::to_string(exp);
    }
    return neg ? "-" + out : out;
}
// ostream << double del C++.
std::string formatDoubleCpp(double d, int precision, std::uint8_t mode, bool showpoint) {
    if (std::isnan(d)) return std::signbit(d) ? "-nan" : "nan";
    if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
    char buf[512];
    if (mode == 1) std::snprintf(buf, sizeof buf, "%.*f", std::min(precision, 100), d);
    else if (mode == 2) std::snprintf(buf, sizeof buf, "%.*e", std::min(precision, 100), d);
    else std::snprintf(buf, sizeof buf, showpoint ? "%#.*g" : "%.*g", precision <= 0 ? 1 : std::min(precision, 100), d);
    return buf;
}

// ---------------------------------------------------------------- printf
std::string formatPrintf(Vm& vm, const std::string& fmt, const Value* args, int argc, int& used) {
    std::string out;
    used = 0;
    auto next = [&](const char* what) -> const Value& {
        if (used >= argc) vm.fault(FaultKind::Argument, std::string("printf: manca l'argomento per ") + what);
        return args[used++];
    };
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        const char c = fmt[i];
        if (c != '%') { out += c; continue; }
        if (++i >= fmt.size()) vm.fault(FaultKind::Format, "printf: % alla fine del formato");
        if (fmt[i] == '%') { out += '%'; continue; }
        std::string spec = "%";
        while (i < fmt.size() && std::strchr("-+ #0", fmt[i])) spec += fmt[i++];
        if (i < fmt.size() && fmt[i] == '*') { spec += std::to_string(next("*").integer()); ++i; }
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) spec += fmt[i++];
        if (i < fmt.size() && fmt[i] == '.') {
            spec += fmt[i++];
            if (i < fmt.size() && fmt[i] == '*') { spec += std::to_string(std::max<std::int64_t>(0, next("*").integer())); ++i; }
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) spec += fmt[i++];
        }
        // modificatori di lunghezza (il valore e' gia' del tipo giusto)
        while (i < fmt.size() && std::strchr("hlLqjzt", fmt[i])) ++i;
        if (i >= fmt.size()) vm.fault(FaultKind::Format, "printf: conversione incompleta");
        const char conv = fmt[i];
        char buf[512];
        switch (conv) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c': {
            const Value& v = next(std::string(1, conv).c_str());
            if (!isInt(v.k) && v.k != VK::Bool) vm.fault(FaultKind::Argument, std::string("printf: %") + conv + " vuole un intero, non " + (isFloat(v.k) ? "un double" : "questo valore"));
            if (conv == 'c') { std::snprintf(buf, sizeof buf, (spec + "c").c_str(), static_cast<int>(static_cast<unsigned char>(v.i))); out += buf; break; }
            const std::string s2 = spec + "ll" + conv;
            if (conv == 'd' || conv == 'i') std::snprintf(buf, sizeof buf, s2.c_str(), static_cast<long long>(v.i));
            else {
                // senza segno: l'ampiezza del tipo del valore
                std::uint64_t u = static_cast<std::uint64_t>(v.i);
                const int bits = bitsOf(v.k);
                if (bits < 64) u &= (1ull << bits) - 1;
                std::snprintf(buf, sizeof buf, s2.c_str(), static_cast<unsigned long long>(u));
            }
            out += buf;
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            const Value& v = next(std::string(1, conv).c_str());
            if (!isFloat(v.k)) vm.fault(FaultKind::Argument, std::string("printf: %") + conv + " vuole un double, non un intero (usa %d o un cast)");
            std::snprintf(buf, sizeof buf, (spec + conv).c_str(), v.d);
            out += buf;
            break;
        }
        case 's': {
            const Value& v = next("s");
            std::string s;
            if (v.isNull()) s = "(null)";
            else if (v.holdsObj() && (v.o->kind == OK::Array || v.o->kind == OK::StrB || v.o->kind == OK::StrW)) s = vm.utf8(v);
            else vm.fault(FaultKind::Argument, "printf: %s vuole una stringa (char*)");
            // precisione: al piu' n caratteri
            const std::size_t dot = spec.find('.');
            if (dot != std::string::npos) { const long n = std::strtol(spec.c_str() + dot + 1, nullptr, 10); if (n >= 0 && static_cast<std::size_t>(n) < s.size()) s.resize(static_cast<std::size_t>(n)); spec.resize(dot); }
            const bool left = spec.find('-') != std::string::npos;
            std::size_t w = 0;
            for (char ch : spec) if (std::isdigit(static_cast<unsigned char>(ch))) w = w * 10 + static_cast<std::size_t>(ch - '0');
            if (s.size() < w) s = left ? s + std::string(w - s.size(), ' ') : std::string(w - s.size(), ' ') + s;
            out += s;
            break;
        }
        case 'p': {
            const Value& v = next("p");
            if (v.isNull()) out += "(nil)";
            else out += "0x" + hexOf(reinterpret_cast<std::uintptr_t>(v.holdsObj() ? static_cast<void*>(v.o) : nullptr) + (v.k == VK::Ptr && v.idx != Whole ? v.idx : 0), false);
            break;
        }
        case 'n': vm.fault(FaultKind::Format, "printf: %n non supportato");
        default: vm.fault(FaultKind::Format, std::string("printf: conversione %") + conv + " non valida");
        }
        if (out.size() > 1000000) vm.fault(FaultKind::Memory, "printf: testo troppo lungo");
    }
    return out;
}

// ---------------------------------------------------------------- formati di C#
namespace {
std::string groupThousands(const std::string& digits) {
    std::string out;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        out += digits[i];
        const std::size_t rest = n - i - 1;
        if (rest > 0 && rest % 3 == 0) out += ',';
    }
    return out;
}
std::string customNumeric(Vm& vm, double d, const std::string& spec) {
    // formati personalizzati semplici: 0.00, #.##, 0.###, 00, #,##0.00, 0%
    bool percent = spec.find('%') != std::string::npos;
    if (percent) d *= 100;
    const std::size_t dot = spec.find('.');
    std::string ip = dot == std::string::npos ? spec : spec.substr(0, dot);
    std::string fp = dot == std::string::npos ? std::string() : spec.substr(dot + 1);
    ip.erase(std::remove(ip.begin(), ip.end(), '%'), ip.end());
    fp.erase(std::remove(fp.begin(), fp.end(), '%'), fp.end());
    const bool group = ip.find(',') != std::string::npos;
    int minInt = 0;
    for (char c : ip) if (c == '0') ++minInt;
    int maxDec = 0, minDec = 0;
    for (char c : fp) { if (c == '0') { ++maxDec; ++minDec; } else if (c == '#') ++maxDec; else vm.fault(FaultKind::Format, "formato numerico non supportato: " + spec); }
    std::string s = fixedDecimal(d, maxDec, true);
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s.erase(0, 1);
    std::string a = s, b;
    const std::size_t p = s.find('.');
    if (p != std::string::npos) { a = s.substr(0, p); b = s.substr(p + 1); }
    while (static_cast<int>(b.size()) > minDec && !b.empty() && b.back() == '0') b.pop_back();
    if (a == "0" && minInt == 0) a.clear();
    while (static_cast<int>(a.size()) < minInt) a = "0" + a;
    if (group) a = groupThousands(a);
    std::string out = a;
    if (!b.empty()) out += "." + b;
    if (out.empty()) out = "0";
    if (neg && out.find_first_not_of("0.,") != std::string::npos) out = "-" + out;
    if (percent) out += "%";
    return out;
}
} // namespace

std::string formatCsSpec(Vm& vm, const Value& v, const std::string& specIn) {
    std::string spec = specIn;
    while (!spec.empty() && spec.back() == ' ') spec.pop_back();
    if (spec.empty()) return vm.format(v, Dialect::CSharp);
    const bool numeric = isNum(v.k) && v.k != VK::Bool;
    if (!numeric) return vm.format(v, Dialect::CSharp);
    const char f = spec[0];
    const bool hasDigits = spec.size() > 1 && std::isdigit(static_cast<unsigned char>(spec[1]));
    int prec = -1;
    if (hasDigits || spec.size() == 1) prec = spec.size() > 1 ? std::atoi(spec.c_str() + 1) : -1;
    const bool standard = (spec.size() == 1 || hasDigits) && std::strchr("CcDdEeFfGgNnPpXxRr", f);
    if (!standard) return customNumeric(vm, v.num(), spec);
    const bool isI = isInt(v.k);
    switch (f) {
    case 'D': case 'd': {
        if (!isI) vm.fault(FaultKind::Format, "il formato D vale solo per gli interi");
        std::string s = std::to_string(v.k == VK::U64 ? 0 : std::llabs(v.i));
        if (v.k == VK::U64) s = std::to_string(v.u);
        if (prec > 0 && static_cast<int>(s.size()) < prec) s = std::string(static_cast<std::size_t>(prec) - s.size(), '0') + s;
        return (v.k != VK::U64 && v.i < 0) ? "-" + s : s;
    }
    case 'X': case 'x': {
        if (!isI) vm.fault(FaultKind::Format, "il formato X vale solo per gli interi");
        std::uint64_t u = static_cast<std::uint64_t>(v.i);
        const int bits = bitsOf(v.k);
        if (bits < 64) u &= (1ull << bits) - 1;
        std::string s = hexOf(u, f == 'X');
        if (prec > 0 && static_cast<int>(s.size()) < prec) s = std::string(static_cast<std::size_t>(prec) - s.size(), '0') + s;
        return s;
    }
    case 'F': case 'f': return fixedDecimal(v.num(), prec < 0 ? 2 : prec, true);
    case 'N': case 'n': {
        std::string s = fixedDecimal(v.num(), prec < 0 ? 2 : prec, true);
        const bool neg = !s.empty() && s[0] == '-';
        if (neg) s.erase(0, 1);
        const std::size_t p = s.find('.');
        std::string a = p == std::string::npos ? s : s.substr(0, p);
        std::string out = groupThousands(a) + (p == std::string::npos ? std::string() : s.substr(p));
        return neg ? "-" + out : out;
    }
    case 'P': case 'p': {
        std::string s = fixedDecimal(v.num() * 100, prec < 0 ? 2 : prec, true);
        return s + " %";
    }
    case 'E': case 'e': {
        char buf[64];
        std::snprintf(buf, sizeof buf, f == 'E' ? "%.*E" : "%.*e", prec < 0 ? 6 : prec, v.num());
        // .NET: esponente con 3 cifre (E+003)
        std::string s = buf;
        const std::size_t e = s.find_first_of("eE");
        if (e != std::string::npos) {
            std::string ex = s.substr(e + 2);
            while (ex.size() < 3) ex = "0" + ex;
            s = s.substr(0, e + 2) + ex;
        }
        return s;
    }
    case 'G': case 'g':
        if (prec <= 0) return vm.format(v, Dialect::CSharp);
        {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.*G", prec, v.num());
            return buf;
        }
    case 'C': case 'c': return "\xC2\xA4" + fixedDecimal(v.num(), prec < 0 ? 2 : prec, true);
    case 'R': case 'r': return vm.format(v, Dialect::CSharp);
    default: return vm.format(v, Dialect::CSharp);
    }
}

// string.Format("{0} e {1,5:F2}", a, b)
std::string formatComposite(Vm& vm, const std::string& fmt, const Value* args, int argc) {
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        const char c = fmt[i];
        if (c == '{') {
            if (i + 1 < fmt.size() && fmt[i + 1] == '{') { out += '{'; ++i; continue; }
            const std::size_t end = fmt.find('}', i);
            if (end == std::string::npos) vm.fault(FaultKind::Format, "formato composito: '{' senza '}'");
            const std::string item = fmt.substr(i + 1, end - i - 1);
            std::size_t p = 0;
            while (p < item.size() && std::isdigit(static_cast<unsigned char>(item[p]))) ++p;
            if (p == 0) vm.fault(FaultKind::Format, "formato composito: indice mancante");
            const int idx = std::atoi(item.c_str());
            if (idx < 0 || idx >= argc) vm.fault(FaultKind::Format, "l'indice {" + std::to_string(idx) + "} e' oltre gli argomenti");
            int align = 0;
            std::string spec;
            if (p < item.size() && item[p] == ',') {
                std::size_t q = p + 1;
                while (q < item.size() && item[q] == ' ') ++q;
                align = std::atoi(item.c_str() + q);
                while (q < item.size() && item[q] != ':') ++q;
                p = q;
            }
            if (p < item.size() && item[p] == ':') spec = item.substr(p + 1);
            std::string s = spec.empty() ? vm.format(args[idx], Dialect::CSharp) : formatCsSpec(vm, args[idx], spec);
            const std::size_t w = static_cast<std::size_t>(std::abs(align));
            if (s.size() < w) s = align > 0 ? std::string(w - s.size(), ' ') + s : s + std::string(w - s.size(), ' ');
            out += s;
            i = end;
            continue;
        }
        if (c == '}') {
            if (i + 1 < fmt.size() && fmt[i + 1] == '}') { out += '}'; ++i; continue; }
            vm.fault(FaultKind::Format, "formato composito: '}' senza '{'");
        }
        out += c;
    }
    return out;
}

// ---------------------------------------------------------------- testo dei valori
std::string Vm::format(const Value& v, Dialect d) {
    const bool cs = d == Dialect::CSharp, java = d == Dialect::Java;
    switch (v.k) {
    case VK::Void: return cs || java ? "" : "";
    case VK::Null: return java ? "null" : cs ? "" : "0";
    case VK::Bool: return cs ? (v.i ? "True" : "False") : java ? (v.i ? "true" : "false") : (v.i ? "1" : "0");
    case VK::C8: return std::string(1, static_cast<char>(v.i));
    case VK::C16: return utf16To8(std::u16string(1, static_cast<char16_t>(v.i)));
    case VK::U64: return std::to_string(v.u);
    case VK::I8: case VK::U8: case VK::I16: case VK::U16: case VK::I32: case VK::U32: case VK::I64:
        if (v.f == 0x45 && v.aux < prog_->classes.size()) {
            // enum di C# con il suo tipo: il nome del valore (o i nomi dei flag, o il numero)
            const ClassInfo* c = prog_->classes[v.aux].get();
            for (const auto& e : c->enumValues) if (e.second == v.i) return e.first;
            if (v.i != 0) {
                std::string names;
                std::int64_t rest = v.i;
                for (const auto& e : c->enumValues) {
                    if (e.second != 0 && (rest & e.second) == e.second) { names += (names.empty() ? "" : ", ") + e.first; rest &= ~e.second; }
                }
                if (rest == 0 && !names.empty()) return names;
            }
        }
        return std::to_string(v.i);
    case VK::F32: return cs ? formatFloatCs(v.d) : java ? formatDoubleJava(static_cast<double>(static_cast<float>(v.d))) : formatDoubleCpp(v.d, 6, 0, false);
    case VK::F64: return cs ? formatDoubleCs(v.d) : java ? formatDoubleJava(v.d) : formatDoubleCpp(v.d, 6, 0, false);
    case VK::Fn: return cs ? "System.Func" : java ? "lambda" : "funzione";
    case VK::Ref: case VK::Ptr: {
        if (!v.o) return java ? "null" : cs ? "" : "0";
        if (v.k == VK::Ptr && v.f == 2) return "0x" + hexOf(0x7ff00000u + v.idx * 8u, false);
        Obj* o = v.o;
        switch (o->kind) {
        case OK::StrB: return std::string(static_cast<StrBObj*>(o)->s);
        case OK::StrW: return utf16To8(static_cast<StrWObj*>(o)->s);
        case OK::Sb: return utf16To8(static_cast<SbObj*>(o)->s);
        case OK::Array: {
            ArrayObj* a = static_cast<ArrayObj*>(o);
            if ((d == Dialect::C || d == Dialect::Cpp) && (a->ek == VK::C8)) return cstring(v.k == VK::Ref ? Value::ptr(a, 0) : v);
            if (cs) return std::string("System.") + (a->elem ? (a->elem->k == TK::I32 ? "Int32" : a->elem->k == TK::F64 ? "Double" : a->elem->k == TK::Str ? "String" : a->elem->name()) : "Object") + "[]";
            if (java) return "[" + std::string(a->elem && a->elem->k == TK::I32 ? "I" : "L") + "@" + hexOf(reinterpret_cast<std::uintptr_t>(o) & 0xFFFFFF, false);
            return "0x" + hexOf(reinterpret_cast<std::uintptr_t>(o) + (v.k == VK::Ptr && v.idx != Whole ? v.idx : 0), false);
        }
        case OK::Inst: {
            InstObj* x = static_cast<InstObj*>(o);
            if (!x->cls) {
                if (cs) return "[" + format(x->fields[0], d) + ", " + format(x->fields[1], d) + "]";
                return "(" + format(x->fields[0], d) + ", " + format(x->fields[1], d) + ")";
            }
            if (cs && x->cls == prog_->typeClass && x->nfields > 1) return utf8(x->fields[1]);
            if (cs || java) {
                auto it = x->cls->selectors.find(java ? "toString/0" : "ToString/0");
                if (it != x->cls->selectors.end() && it->second->defined) {
                    const Value self = Value::ref(x);
                    const Value r = invokeFunction(it->second, &self, 1, true);
                    return r.isNull() ? "" : utf8(r);
                }
                if (x->cls->isException) {
                    const std::string m = x->nfields > 0 ? utf8(x->fields[0]) : std::string();
                    return (cs ? "System." : "java.lang.") + x->cls->name + (m.empty() ? "" : ": " + m);
                }
                std::string q = x->cls->qualified;
                for (std::size_t p = q.find("::"); p != std::string::npos; p = q.find("::")) q.replace(p, 2, ".");
                return java ? q + "@" + hexOf(reinterpret_cast<std::uintptr_t>(o) & 0xFFFFFF, false) : q;
            }
            return "0x" + hexOf(reinterpret_cast<std::uintptr_t>(o), false);
        }
        case OK::Vec: {
            VecObj* vv = static_cast<VecObj*>(o);
            if (java) {
                std::string s = "[";
                for (std::size_t i = 0; i < vv->items.size(); ++i) { s += (i ? ", " : "") + format(vv->items[i], d); if (s.size() > 100000) break; }
                return s + "]";
            }
            if (cs) return "System.Collections.Generic.List`1[" + std::string(vv->type && !vv->type->args.empty() ? vv->type->args[0]->name() : "System.Object") + "]";
            return "0x" + hexOf(reinterpret_cast<std::uintptr_t>(o), false);
        }
        case OK::Map: {
            MapObj* m = static_cast<MapObj*>(o);
            if (java) {
                std::string s = m->isSet ? "[" : "{";
                bool first = true;
                auto emitSlot = [&](std::uint32_t i) {
                    s += (first ? "" : ", ") + format(m->keys[i], d);
                    if (!m->isSet) s += "=" + format(m->vals[i], d);
                    first = false;
                };
                if (m->mode == 0) for (std::uint32_t i : m->order) emitSlot(i);
                else for (std::uint32_t i = 0; i < m->keys.size(); ++i) if (m->used[i]) emitSlot(i);
                return s + (m->isSet ? "]" : "}");
            }
            return cs ? "System.Collections.Generic.Dictionary`2" : "0x" + hexOf(reinterpret_cast<std::uintptr_t>(o), false);
        }
        case OK::Box: return format(static_cast<BoxObj*>(o)->v, d);
        default: return cs ? "System.Object" : "0x" + hexOf(reinterpret_cast<std::uintptr_t>(o), false);
        }
    }
    }
    return "?";
}

// ================================================================ debug: variabili
namespace {
std::string quoteText(const std::string& s, char q) {
    std::string out(1, q);
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == q) { out += '\\'; out += c; }
        else out += c;
        if (out.size() > 80) { out += "..."; break; }
    }
    return out + q;
}
} // namespace

std::string debugValue(Vm& vm, const Value& v, TypeP t, Dialect d, bool& expandable) {
    expandable = false;
    if (t && t->k == TK::Bool) return v.i ? (d == Dialect::CSharp ? "True" : "true") : (d == Dialect::CSharp ? "False" : "false");
    if (v.k == VK::C8 || v.k == VK::C16) {
        const std::string c = vm.format(v, d);
        return quoteText(c, '\'') + " (" + std::to_string(v.i) + ")";
    }
    if (t && t->k == TK::Enum && t->cls) {
        for (const auto& e : t->cls->enumValues) if (e.second == v.integer()) return e.first + " (" + std::to_string(v.integer()) + ")";
    }
    if (isNum(v.k)) return vm.format(v, d == Dialect::C ? Dialect::Cpp : d);
    if (v.k == VK::Void) return "(non inizializzata)";
    if (v.isNull()) return d == Dialect::C || d == Dialect::Cpp ? "NULL" : "null";
    if (v.k == VK::Fn) return "funzione";
    if (v.k == VK::Ptr && v.f == 2) return "-> variabile";
    Obj* o = v.o;
    if (o->dead()) return "(liberato)";
    switch (o->kind) {
    case OK::StrB: case OK::StrW: case OK::Sb: return quoteText(vm.utf8(v), '"');
    case OK::Array: {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        if (a->ek == VK::C8 && (d == Dialect::C || d == Dialect::Cpp)) {
            std::string s;
            const std::uint32_t start = v.k == VK::Ptr && v.idx != Whole ? v.idx : 0;
            for (std::uint32_t i = start; i < a->count && a->data[i]; ++i) s += static_cast<char>(a->data[i]);
            expandable = a->count > 0;
            return (v.k == VK::Ptr ? "-> " : "") + quoteText(s, '"');
        }
        expandable = a->count > 0;
        if (v.k == VK::Ptr && v.idx != Whole) return "-> [" + std::to_string(v.idx) + "] di " + std::to_string(a->count);
        return "[" + std::to_string(a->count) + " elementi]";
    }
    case OK::Inst: {
        InstObj* x = static_cast<InstObj*>(o);
        expandable = x->nfields > 0;
        std::string nm = x->cls ? x->cls->name : "coppia";
        if (v.k == VK::Ptr && v.idx != Whole) { bool e2 = false; return "-> " + debugValue(vm, vm.loadPtr(v), nullptr, d, e2); }
        return (v.k == VK::Ptr ? "-> " : "") + nm + " {...}";
    }
    case OK::Vec: expandable = !static_cast<VecObj*>(o)->items.empty(); return "[" + std::to_string(static_cast<VecObj*>(o)->items.size()) + " elementi]";
    case OK::Map: expandable = static_cast<MapObj*>(o)->count > 0; return "{" + std::to_string(static_cast<MapObj*>(o)->count) + " elementi}";
    case OK::File: return static_cast<FileObj*>(o)->open ? "file aperto" : "file chiuso";
    case OK::Box: return debugValue(vm, static_cast<BoxObj*>(o)->v, t, d, expandable);
    default: return "oggetto";
    }
}

} // namespace fx::prog
