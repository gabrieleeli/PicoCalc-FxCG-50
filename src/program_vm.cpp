// Macchina a pila dei programmi C, C++ e C# (v1.15.0, F9; solo Pimoroni Pico Plus 2 W).
//
// Esegue il bytecode di program_lang.cpp un'istruzione alla volta dentro un
// ciclo: le chiamate del programma sono frame in un vettore (al piu'
// MaxCallDepth, "Nesting ERROR" oltre), quindi l'esecuzione non usa la
// ricorsione nativa. Ogni istruzione consuma un'unita' del budget di step::Task:
// la modalita' la esegue a passi e AC la interrompe. Memoria: array e stringhe
// contano su un budget di celle ("Memory ERROR" oltre). WorkspaceHost collega
// i servizi della calcolatrice (graph_commands, parser unico, A-Z).
#include "program_internal.hpp"
#include "binding.hpp"
#include "expression.hpp"
#include "graph_commands.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace fx::prog {
namespace {

struct ArrayObj;
struct Value {
    enum K : std::uint8_t { Void, Int, Char, Dbl, Bool, Str, Arr } k{Void};
    std::int64_t i{0};
    double d{0};
    std::string s;
    std::shared_ptr<ArrayObj> a;
    static Value ofInt(std::int64_t v) { Value x; x.k = Int; x.i = v; return x; }
    static Value ofChar(std::int64_t v) { Value x; x.k = Char; x.i = v & 0xFF; return x; }
    static Value ofDbl(double v) { Value x; x.k = Dbl; x.d = v; return x; }
    static Value ofBool(bool v) { Value x; x.k = Bool; x.i = v ? 1 : 0; return x; }
    static Value ofStr(std::string v) { Value x; x.k = Str; x.s = std::move(v); return x; }
    bool integral() const noexcept { return k == Int || k == Char || k == Bool; }
    bool numeric() const noexcept { return integral() || k == Dbl; }
    double num() const noexcept { return k == Dbl ? d : static_cast<double>(i); }
};
struct ArrayObj {
    Type elem{Type::Int};
    std::size_t rows{0}, cols{1};
    std::vector<Value> data;
    std::size_t* counter{nullptr};
    ~ArrayObj() { if (counter) *counter -= std::min(*counter, data.size()); }
};
constexpr std::size_t MaxStringBytes = 65535;

Value zeroOf(Type t) {
    switch (t) {
    case Type::Double: return Value::ofDbl(0);
    case Type::Bool: return Value::ofBool(false);
    case Type::Char: return Value::ofChar(0);
    case Type::String: return Value::ofStr("");
    default: return Value::ofInt(0);
    }
}
std::string shortestDouble(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "\xE2\x88\x9E" : "-\xE2\x88\x9E";
    char buf[40];
    for (int p = 1; p <= 17; ++p) {
        std::snprintf(buf, sizeof buf, "%.*g", p, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    // C#: esponente come E+15 (senza zeri iniziali), mai notazione per numeri piccoli interi
    std::string s = buf;
    const std::size_t e = s.find('e');
    if (e != std::string::npos) {
        std::string mant = s.substr(0, e), ex = s.substr(e + 1);
        const char sign = ex[0] == '-' ? '-' : '+';
        std::size_t k = (ex[0] == '-' || ex[0] == '+') ? 1 : 0;
        while (k + 1 < ex.size() && ex[k] == '0') ++k;
        s = mant + "E" + sign + ex.substr(k);
    }
    return s;
}
std::string cppDouble(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[40];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

} // namespace

struct Machine::Impl {
    std::size_t cells{0};                                   // prima degli altri membri: distrutto per ultimo
    std::size_t maxCells;
    std::shared_ptr<const Program> prog;
    Host& host;
    std::vector<Value> stack, globals, locals;
    struct Frame { std::int32_t fn{-1}; std::int32_t ret{0}; std::size_t base{0}; std::size_t stackBase{0}; };
    std::vector<Frame> frames;
    std::int32_t pc{0};
    std::string inputBuffer;
    bool inputEof{false};
    std::uint32_t seed{1};
    Impl(std::shared_ptr<const Program> p, Host& h, std::size_t max) : maxCells(max), prog(std::move(p)), host(h) {
        globals.resize(prog->globalNames.size());
        for (std::size_t i = 0; i < globals.size(); ++i) globals[i] = zeroOf(prog->globalTypes[i]);
        pc = prog->init;
        stack.reserve(256);
    }
    ~Impl() {
        // gli array si liberano prima del contatore (vedi l'ordine dei membri)
        stack.clear(); globals.clear(); locals.clear();
    }
    [[noreturn]] void fail(const std::string& m) const {
        const std::size_t at = static_cast<std::size_t>(std::max<std::int32_t>(0, std::min<std::int32_t>(pc, static_cast<std::int32_t>(prog->where.size()) - 1)));
        const Where w = prog->where.empty() ? Where{} : prog->where[at];
        throw Error(m, w.line, w.column);
    }
    Value pop() {
        if (stack.empty()) fail("Stack ERROR: pila vuota (errore interno)");
        Value v = std::move(stack.back());
        stack.pop_back();
        return v;
    }
    void push(Value v) {
        if (stack.size() >= MaxStack) fail("Stack ERROR: troppi valori sulla pila");
        stack.push_back(std::move(v));
    }
    void reserveCells(std::size_t n) {
        if (n > maxCells || cells > maxCells - n) fail("Memory ERROR: memoria del programma esaurita");
        cells += n;
    }
    std::shared_ptr<ArrayObj> newArray(Type elem, std::int64_t rows, std::int64_t cols) {
        if (rows < 0 || cols < 0) fail("Dimension ERROR: dimensione dell'array negativa");
        const std::size_t n = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
        if (rows > 1000000 || cols > 1000000) fail("Memory ERROR: array troppo grande");
        reserveCells(n);
        auto a = std::make_shared<ArrayObj>();
        a->counter = &cells;
        a->elem = elem; a->rows = static_cast<std::size_t>(rows); a->cols = static_cast<std::size_t>(cols);
        try {
            a->data.assign(n, zeroOf(elem));
        } catch (const std::bad_alloc&) {
            a->counter = nullptr;
            cells -= n;
            fail("Memory ERROR: memoria del programma esaurita");
        }
        return a;
    }
    Value& varRef(bool global, std::int32_t slot) {
        if (global) return globals.at(static_cast<std::size_t>(slot));
        return locals.at(frames.back().base + static_cast<std::size_t>(slot));
    }
    Type varType(bool global, std::int32_t slot) const {
        if (global) return prog->globalTypes.at(static_cast<std::size_t>(slot));
        return prog->functions[static_cast<std::size_t>(frames.back().fn)].localTypes.at(static_cast<std::size_t>(slot));
    }

    // ---- conversioni e testo
    std::int64_t parseInt(const std::string& s, bool strict) const {
        const char* b = s.c_str();
        while (*b == ' ' || *b == '\t') ++b;
        char* e = nullptr;
        errno = 0;
        const long long v = std::strtoll(b, &e, 10);
        if (strict && (e == b || *e != '\0' || errno == ERANGE)) fail("Argument ERROR: '" + s + "' non e' un intero");
        return e == b ? 0 : v;
    }
    double parseDouble(const std::string& s, bool strict) const {
        const char* b = s.c_str();
        while (*b == ' ' || *b == '\t') ++b;
        char* e = nullptr;
        const double v = std::strtod(b, &e);
        if (strict && (e == b || *e != '\0')) fail("Argument ERROR: '" + s + "' non e' un numero");
        return e == b ? 0 : v;
    }
    Value convert(Value v, Type t) const {
        switch (t) {
        case Type::Int:
            if (v.k == Value::Dbl) {
                if (!std::isfinite(v.d) || std::abs(v.d) >= 9.2e18) fail("Math ERROR: valore fuori dall'intervallo degli interi");
                return Value::ofInt(static_cast<std::int64_t>(std::trunc(v.d)));
            }
            if (v.k == Value::Str) return Value::ofInt(parseInt(v.s, true));
            if (v.k == Value::Arr || v.k == Value::Void) fail("Argument ERROR: valore non numerico");
            return Value::ofInt(v.i);
        case Type::Char:
            if (v.k == Value::Str) return Value::ofChar(v.s.empty() ? 0 : static_cast<unsigned char>(v.s[0]));
            if (v.k == Value::Dbl) return Value::ofChar(static_cast<std::int64_t>(v.d));
            if (v.k == Value::Arr || v.k == Value::Void) fail("Argument ERROR: valore non numerico");
            return Value::ofChar(v.i);
        case Type::Double:
            if (v.k == Value::Str) return Value::ofDbl(parseDouble(v.s, true));
            if (v.k == Value::Arr || v.k == Value::Void) fail("Argument ERROR: valore non numerico");
            return Value::ofDbl(v.num());
        case Type::Bool:
            if (v.k == Value::Str) return Value::ofBool(!v.s.empty());
            if (v.k == Value::Arr) return Value::ofBool(true);
            if (v.k == Value::Void) fail("Argument ERROR: valore mancante");
            return Value::ofBool(v.k == Value::Dbl ? v.d != 0 : v.i != 0);
        case Type::String:
            if (v.k == Value::Str) return v;
            return Value::ofStr(text(v));
        default: return v;
        }
    }
    // Il valore come lo stampano cout (C++), Console (C#) o printf("%g") (C).
    std::string text(const Value& v) const {
        switch (v.k) {
        case Value::Int: return std::to_string(v.i);
        case Value::Char: return std::string(1, static_cast<char>(v.i));
        case Value::Bool: return prog->dialect == Dialect::CSharp ? (v.i ? "True" : "False") : (v.i ? "1" : "0");
        case Value::Dbl: return prog->dialect == Dialect::CSharp ? shortestDouble(v.d) : cppDouble(v.d);
        case Value::Str: return v.s;
        case Value::Arr: return prog->dialect == Dialect::CSharp ? "System.Array" : "[array]";
        default: return "";
        }
    }
    std::string checkedString(std::string s) const {
        if (s.size() > MaxStringBytes) fail("Memory ERROR: stringa troppo lunga");
        return s;
    }

    // ---- printf di C
    std::string printf(const std::vector<Value>& args) const {
        if (args.empty() || args[0].k != Value::Str) fail("Argument ERROR: printf vuole una stringa di formato");
        const std::string& f = args[0].s;
        std::string out;
        std::size_t next = 1;
        for (std::size_t i = 0; i < f.size(); ++i) {
            if (f[i] != '%') { out += f[i]; continue; }
            if (i + 1 < f.size() && f[i + 1] == '%') { out += '%'; ++i; continue; }
            std::string spec = "%";
            ++i;
            while (i < f.size() && std::strchr("-+ #0", f[i])) spec += f[i++];
            auto number = [&](void) {
                if (i < f.size() && f[i] == '*') {
                    if (next >= args.size()) fail("Argument ERROR: printf ha meno argomenti del formato");
                    spec += std::to_string(static_cast<int>(std::clamp<std::int64_t>(static_cast<std::int64_t>(args[next++].num()), -500, 500)));
                    ++i;
                    return;
                }
                std::size_t digits = 0;
                while (i < f.size() && std::isdigit(static_cast<unsigned char>(f[i]))) { if (++digits > 3) fail("Argument ERROR: larghezza di printf troppo grande"); spec += f[i++]; }
            };
            number();
            if (i < f.size() && f[i] == '.') { spec += f[i++]; number(); }
            while (i < f.size() && std::strchr("hlLqjzt", f[i])) ++i;
            if (i >= f.size()) fail("Argument ERROR: formato di printf incompleto");
            const char conv = f[i];
            if (next >= args.size()) fail("Argument ERROR: printf ha meno argomenti del formato");
            const Value& a = args[next++];
            char buf[600];
            switch (conv) {
            case 'd': case 'i':
                spec += "lld";
                std::snprintf(buf, sizeof buf, spec.c_str(), static_cast<long long>(a.k == Value::Dbl ? static_cast<long long>(a.d) : a.i));
                break;
            case 'u': case 'x': case 'X': case 'o':
                spec += "ll"; spec += conv;
                std::snprintf(buf, sizeof buf, spec.c_str(), static_cast<unsigned long long>(a.k == Value::Dbl ? static_cast<long long>(a.d) : a.i));
                break;
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
                spec += conv;
                std::snprintf(buf, sizeof buf, spec.c_str(), a.num());
                break;
            case 'c':
                spec += 'c';
                std::snprintf(buf, sizeof buf, spec.c_str(), static_cast<int>(static_cast<unsigned char>(a.k == Value::Str ? (a.s.empty() ? 0 : a.s[0]) : a.i)));
                break;
            case 's': {
                spec += 's';
                const std::string s = text(a);
                std::snprintf(buf, sizeof buf, spec.c_str(), s.c_str());
                break;
            }
            default: fail(std::string("Argument ERROR: formato %") + conv + " non supportato");
            }
            out += buf;
        }
        return checkedString(out);
    }
    // C#: {x:F2} {x:N0} {x:E3} {x:D4} {x:X}
    std::string format(const Value& v, const std::string& fmt) const {
        if (fmt.empty()) return text(v);
        const char kind = static_cast<char>(std::toupper(static_cast<unsigned char>(fmt[0])));
        int digits = -1;
        if (fmt.size() > 1) digits = static_cast<int>(parseInt(fmt.substr(1), true));
        if (digits > 30) fail("Argument ERROR: formato troppo lungo");
        char buf[96];
        switch (kind) {
        case 'F': std::snprintf(buf, sizeof buf, "%.*f", digits < 0 ? 2 : digits, v.num()); return buf;
        case 'E': std::snprintf(buf, sizeof buf, "%.*E", digits < 0 ? 6 : digits, v.num()); return buf;
        case 'G': return digits < 0 ? text(v) : (std::snprintf(buf, sizeof buf, "%.*G", digits, v.num()), std::string(buf));
        case 'D': std::snprintf(buf, sizeof buf, "%0*lld", digits < 0 ? 1 : digits, static_cast<long long>(v.num())); return buf;
        case 'X': std::snprintf(buf, sizeof buf, "%0*llX", digits < 0 ? 1 : digits, static_cast<unsigned long long>(static_cast<long long>(v.num()))); return buf;
        case 'N': {
            std::snprintf(buf, sizeof buf, "%.*f", digits < 0 ? 2 : digits, v.num());
            std::string s = buf, out;
            const std::size_t dot = s.find('.');
            const std::size_t intEnd = dot == std::string::npos ? s.size() : dot;
            std::size_t start = s[0] == '-' ? 1 : 0;
            out = s.substr(0, start);
            for (std::size_t k = start; k < intEnd; ++k) {
                out += s[k];
                const std::size_t left = intEnd - k - 1;
                if (left > 0 && left % 3 == 0) out += ',';
            }
            return out + s.substr(intEnd);
        }
        default: fail("Argument ERROR: formato '" + fmt + "' non supportato");
        }
    }

    // ---- aritmetica
    Value arith(Op op, const Value& a, const Value& b) const {
        if (op == Op::Add && (a.k == Value::Str || b.k == Value::Str)) {
            if (a.k == Value::Str && b.k == Value::Char) return Value::ofStr(checkedString(a.s + static_cast<char>(b.i)));
            return Value::ofStr(checkedString(text(a) + text(b)));
        }
        if (!a.numeric() || !b.numeric()) fail("Argument ERROR: operazione su valori non numerici");
        const bool ints = a.integral() && b.integral();
        switch (op) {
        case Op::Add: return ints ? Value::ofInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a.i) + static_cast<std::uint64_t>(b.i))) : Value::ofDbl(a.num() + b.num());
        case Op::Sub: return ints ? Value::ofInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a.i) - static_cast<std::uint64_t>(b.i))) : Value::ofDbl(a.num() - b.num());
        case Op::Mul: return ints ? Value::ofInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a.i) * static_cast<std::uint64_t>(b.i))) : Value::ofDbl(a.num() * b.num());
        case Op::Div:
            if (ints) {
                if (b.i == 0) fail("Math ERROR: divisione intera per zero");
                if (a.i == std::numeric_limits<std::int64_t>::min() && b.i == -1) fail("Math ERROR: risultato fuori dall'intervallo degli interi");
                return Value::ofInt(a.i / b.i);
            }
            return Value::ofDbl(a.num() / b.num());
        case Op::Mod:
            if (ints) {
                if (b.i == 0) fail("Math ERROR: resto della divisione per zero");
                if (b.i == -1) return Value::ofInt(0);
                return Value::ofInt(a.i % b.i);
            }
            return Value::ofDbl(std::fmod(a.num(), b.num()));
        case Op::BitAnd: case Op::BitOr: case Op::BitXor: case Op::Shl: case Op::Shr: {
            if (!ints) fail("Argument ERROR: operatore sui bit con un valore reale");
            switch (op) {
            case Op::BitAnd: return Value::ofInt(a.i & b.i);
            case Op::BitOr: return Value::ofInt(a.i | b.i);
            case Op::BitXor: return Value::ofInt(a.i ^ b.i);
            case Op::Shl: if (b.i < 0 || b.i > 63) fail("Math ERROR: spostamento fuori intervallo"); return Value::ofInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a.i) << b.i));
            default: if (b.i < 0 || b.i > 63) fail("Math ERROR: spostamento fuori intervallo"); return Value::ofInt(a.i >> b.i);
            }
        }
        default: fail("Syntax ERROR: operatore non valido (errore interno)");
        }
    }
    bool compare(Op op, const Value& a, const Value& b) const {
        int c = 0;
        if (a.k == Value::Str && b.k == Value::Str) c = a.s < b.s ? -1 : a.s > b.s ? 1 : 0;
        else if (a.k == Value::Str || b.k == Value::Str) {
            if (op == Op::Eq) return false;
            if (op == Op::Ne) return true;
            fail("Argument ERROR: confronto tra una stringa e un numero");
        } else if (!a.numeric() || !b.numeric()) fail("Argument ERROR: confronto non valido");
        else if (a.integral() && b.integral()) c = a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
        else { const double x = a.num(), y = b.num(); if (std::isnan(x) || std::isnan(y)) return op == Op::Ne; c = x < y ? -1 : x > y ? 1 : 0; }
        switch (op) {
        case Op::Eq: return c == 0;
        case Op::Ne: return c != 0;
        case Op::Lt: return c < 0;
        case Op::Le: return c <= 0;
        case Op::Gt: return c > 0;
        default: return c >= 0;
        }
    }
    Value& element(const Value& arr, const Value& idx) {
        if (arr.k != Value::Arr || !arr.a) fail("Argument ERROR: [] su un valore che non e' un array");
        if (!idx.integral()) fail("Argument ERROR: indice non intero");
        if (idx.i < 0 || static_cast<std::size_t>(idx.i) >= arr.a->data.size() || arr.a->cols != 1)
            fail("Dimension ERROR: indice " + std::to_string(idx.i) + " fuori dall'array di " + std::to_string(arr.a->data.size()) + " elementi");
        return arr.a->data[static_cast<std::size_t>(idx.i)];
    }
    Value& element2(const Value& arr, const Value& r, const Value& c) {
        if (arr.k != Value::Arr || !arr.a) fail("Argument ERROR: [][] su un valore che non e' un array");
        if (!r.integral() || !c.integral()) fail("Argument ERROR: indice non intero");
        if (r.i < 0 || c.i < 0 || static_cast<std::size_t>(r.i) >= arr.a->rows || static_cast<std::size_t>(c.i) >= arr.a->cols)
            fail("Dimension ERROR: indice [" + std::to_string(r.i) + "][" + std::to_string(c.i) + "] fuori dall'array");
        return arr.a->data[static_cast<std::size_t>(r.i) * arr.a->cols + static_cast<std::size_t>(c.i)];
    }
    // Indice in una stringa (s[i] di C, C++ e C#): carattere, sola lettura qui.
    Value stringChar(const Value& s, const Value& idx) const {
        if (!idx.integral()) fail("Argument ERROR: indice non intero");
        if (idx.i < 0 || static_cast<std::size_t>(idx.i) > s.s.size()) fail("Dimension ERROR: indice fuori dalla stringa");
        if (static_cast<std::size_t>(idx.i) == s.s.size()) return Value::ofChar(0);   // il terminatore di C
        return Value::ofChar(static_cast<unsigned char>(s.s[static_cast<std::size_t>(idx.i)]));
    }
    Value incremented(const Value& v, bool dec) const {
        const Value one = Value::ofInt(dec ? -1 : 1);
        if (v.k == Value::Char) return Value::ofChar(v.i + one.i);
        if (!v.numeric()) fail("Argument ERROR: ++/-- su un valore non numerico");
        return arith(Op::Add, v, one);
    }

    // ---- input
    bool fillInput() {
        if (!inputBuffer.empty()) return true;
        std::string line;
        if (!host.readLine(line)) return false;
        inputBuffer = line + "\n";
        return true;
    }
    // Restituisce false se l'input non c'e' ancora (la macchina aspetta).
    bool readToken(std::string& out) {
        while (true) {
            std::size_t k = 0;
            while (k < inputBuffer.size() && std::isspace(static_cast<unsigned char>(inputBuffer[k]))) ++k;
            inputBuffer.erase(0, k);
            if (!inputBuffer.empty()) break;
            if (!fillInput()) return false;
        }
        std::size_t e = 0;
        while (e < inputBuffer.size() && !std::isspace(static_cast<unsigned char>(inputBuffer[e]))) ++e;
        out = inputBuffer.substr(0, e);
        inputBuffer.erase(0, e);
        return true;
    }
    bool readLine(std::string& out) {
        if (inputBuffer.empty() && !fillInput()) return false;
        const std::size_t nl = inputBuffer.find('\n');
        out = inputBuffer.substr(0, nl);
        inputBuffer.erase(0, nl == std::string::npos ? inputBuffer.size() : nl + 1);
        return true;
    }
    bool readChar(std::int64_t& out) {
        if (inputBuffer.empty() && !fillInput()) return false;
        out = static_cast<unsigned char>(inputBuffer[0]);
        inputBuffer.erase(0, 1);
        return true;
    }

    // ---- variabili per fx::calc (numeriche della funzione corrente e globali)
    std::vector<std::pair<std::string, double>> visibleNumbers() const {
        std::vector<std::pair<std::string, double>> out;
        auto add = [&](const std::string& n, const Value& v) {
            if (!v.numeric() || n.empty()) return;
            for (const auto& e : out) if (e.first == n) return;
            out.emplace_back(n, v.num());
        };
        if (!frames.empty()) {
            const Function& f = prog->functions[static_cast<std::size_t>(frames.back().fn)];
            for (std::size_t i = f.localNames.size(); i-- > 0;)
                if (frames.back().base + i < locals.size()) add(f.localNames[i], locals[frames.back().base + i]);
        }
        for (std::size_t i = 0; i < globals.size(); ++i) add(prog->globalNames[i], globals[i]);
        return out;
    }
    static std::string num(double v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.15g", v);
        return buf;
    }
    char letterOf(const Value& v) const {
        char c = 0;
        if (v.k == Value::Str && v.s.size() == 1) c = v.s[0];
        else if (v.k == Value::Char) c = static_cast<char>(v.i);
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (c < 'A' || c > 'Z') fail("Argument ERROR: attesa una lettera da A a Z");
        return c;
    }
    std::string commandText(const Value& v) const { if (v.k != Value::Str) fail("Argument ERROR: attesa una stringa"); return v.s; }

    // ---- funzioni predefinite
    Value builtin(Fn fn, std::vector<Value>& a, bool& stop, int& exitCode) {
        auto d = [&](std::size_t k) { if (!a[k].numeric()) fail("Argument ERROR: argomento non numerico"); return a[k].num(); };
        auto real = [&](double v) { return Value::ofDbl(v); };
        switch (fn) {
        case Fn::Print: if (!a.empty()) host.print(text(a[0])); return Value{};
        case Fn::PrintLine: host.print((a.empty() ? std::string() : text(a[0])) + "\n"); return Value{};
        case Fn::Printf: { const std::string s = printf(a); host.print(s); return Value::ofInt(static_cast<std::int64_t>(s.size())); }
        case Fn::Puts: host.print(text(a[0]) + "\n"); return Value::ofInt(0);
        case Fn::Putchar: host.print(std::string(1, static_cast<char>(a[0].integral() ? a[0].i : static_cast<std::int64_t>(d(0))))); return a[0];
        case Fn::Format: return Value::ofStr(format(a[0], commandText(a[1])));
        case Fn::Sin: return real(std::sin(d(0)));
        case Fn::Cos: return real(std::cos(d(0)));
        case Fn::Tan: return real(std::tan(d(0)));
        case Fn::Asin: return real(std::asin(d(0)));
        case Fn::Acos: return real(std::acos(d(0)));
        case Fn::Atan: return real(std::atan(d(0)));
        case Fn::Atan2: return real(std::atan2(d(0), d(1)));
        case Fn::Sqrt: return real(std::sqrt(d(0)));
        case Fn::Pow: return real(std::pow(d(0), d(1)));
        case Fn::Exp: return real(std::exp(d(0)));
        case Fn::Log: return real(std::log(d(0)));
        case Fn::Log10: return real(std::log10(d(0)));
        case Fn::Fabs: return real(std::fabs(d(0)));
        case Fn::Abs:
            if (a[0].integral()) return Value::ofInt(a[0].i < 0 ? -a[0].i : a[0].i);
            return real(std::fabs(d(0)));
        case Fn::Floor: return real(std::floor(d(0)));
        case Fn::Ceil: return real(std::ceil(d(0)));
        case Fn::Round: return real(prog->dialect == Dialect::CSharp ? std::nearbyint(d(0)) : std::round(d(0)));   // C#: al pari
        case Fn::Trunc: return real(std::trunc(d(0)));
        case Fn::Fmod: return real(std::fmod(d(0), d(1)));
        case Fn::Sinh: return real(std::sinh(d(0)));
        case Fn::Cosh: return real(std::cosh(d(0)));
        case Fn::Tanh: return real(std::tanh(d(0)));
        case Fn::Min: case Fn::Max: {
            if (a[0].integral() && a[1].integral()) return Value::ofInt(fn == Fn::Min ? std::min(a[0].i, a[1].i) : std::max(a[0].i, a[1].i));
            return real(fn == Fn::Min ? std::fmin(d(0), d(1)) : std::fmax(d(0), d(1)));
        }
        case Fn::Strlen:
            if (a[0].k == Value::Arr && a[0].a) return Value::ofInt(static_cast<std::int64_t>(a[0].a->data.size()));
            if (a[0].k != Value::Str) fail("Argument ERROR: attesa una stringa");
            return Value::ofInt(static_cast<std::int64_t>(a[0].s.size()));
        case Fn::Strcmp: {
            const int c = commandText(a[0]).compare(commandText(a[1]));
            return Value::ofInt(c < 0 ? -1 : c > 0 ? 1 : 0);
        }
        case Fn::Strcat: return Value::ofStr(checkedString(commandText(a[0]) + commandText(a[1])));
        case Fn::Substr: {
            const std::string& s = commandText(a[0]);
            const std::int64_t start = a.size() > 1 ? static_cast<std::int64_t>(d(1)) : 0;
            if (start < 0 || static_cast<std::size_t>(start) > s.size()) fail("Argument ERROR: inizio fuori dalla stringa");
            std::int64_t len = a.size() > 2 ? static_cast<std::int64_t>(d(2)) : static_cast<std::int64_t>(s.size());
            if (len < 0) fail("Argument ERROR: lunghezza negativa");
            if (prog->dialect == Dialect::CSharp && a.size() > 2 && static_cast<std::size_t>(start + len) > s.size()) fail("Argument ERROR: oltre la fine della stringa");
            return Value::ofStr(s.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(len)));
        }
        case Fn::IndexOf: {
            const std::string& s = commandText(a[0]);
            const std::string n = a[1].k == Value::Char ? std::string(1, static_cast<char>(a[1].i)) : commandText(a[1]);
            const std::size_t at = s.find(n);
            return Value::ofInt(at == std::string::npos ? -1 : static_cast<std::int64_t>(at));
        }
        case Fn::ToUpper: case Fn::ToLower: {
            if (a[0].k == Value::Str) {
                std::string s = a[0].s;
                for (char& c : s) c = static_cast<char>(fn == Fn::ToUpper ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)));
                return Value::ofStr(s);
            }
            const auto c = static_cast<unsigned char>(a[0].integral() ? a[0].i : 0);
            const Value r = Value::ofInt(fn == Fn::ToUpper ? std::toupper(c) : std::tolower(c));
            return a[0].k == Value::Char ? Value::ofChar(r.i) : r;
        }
        case Fn::ToInt: return convert(a[0].k == Value::Dbl ? Value::ofDbl(std::nearbyint(a[0].d)) : a[0], Type::Int);   // Convert.ToInt32 arrotonda
        case Fn::ToDouble: return convert(a[0], Type::Double);
        case Fn::ToStr:
            if (prog->dialect == Dialect::Cpp && a[0].k == Value::Dbl) { char buf[64]; std::snprintf(buf, sizeof buf, "%f", a[0].d); return Value::ofStr(buf); }
            return Value::ofStr(text(a[0]));
        case Fn::ParseInt:
            if (a[0].k != Value::Str) fail("Argument ERROR: attesa una stringa");
            return Value::ofInt(parseInt(a[0].s, prog->dialect != Dialect::C));
        case Fn::ParseDouble:
            if (a[0].k != Value::Str) fail("Argument ERROR: attesa una stringa");
            return Value::ofDbl(parseDouble(a[0].s, prog->dialect != Dialect::C));
        case Fn::Rand: seed = seed * 1103515245u + 12345u; return Value::ofInt(static_cast<std::int64_t>((seed >> 16) & 0x7FFF));
        case Fn::Srand: seed = static_cast<std::uint32_t>(static_cast<std::int64_t>(d(0))); return Value{};
        case Fn::Exit: exitCode = static_cast<int>(static_cast<std::int64_t>(d(0))); stop = true; return Value{};
        case Fn::FxCalc: return real(host.calc(commandText(a[0]), visibleNumbers()));
        case Fn::FxGetVar: return real(host.getVariable(letterOf(a[0])));
        case Fn::FxSetVar: host.setVariable(letterOf(a[0]), d(1)); return Value{};
        case Fn::FxCls: host.graphics("Cls"); return Value{};
        case Fn::FxViewWindow: host.graphics("ViewWindow " + num(d(0)) + "," + num(d(1)) + "," + num(d(2)) + "," + num(d(3)) + "," + num(d(4)) + "," + num(d(5))); return Value{};
        case Fn::FxPlot: host.graphics("Plot " + num(d(0)) + "," + num(d(1))); return Value{};
        case Fn::FxLine: host.graphics("F-Line " + num(d(0)) + "," + num(d(1)) + "," + num(d(2)) + "," + num(d(3))); return Value{};
        case Fn::FxCircle: host.graphics("Circle " + num(d(0)) + "," + num(d(1)) + "," + num(d(2))); return Value{};
        case Fn::FxText: {
            std::string s = text(a[2]);
            if (s.find('"') != std::string::npos) fail("Argument ERROR: il testo non puo' contenere virgolette");
            host.graphics("Text " + num(d(0)) + "," + num(d(1)) + ",\"" + s + "\"");
            return Value{};
        }
        case Fn::FxGraphY: host.graphics("Graph Y=" + commandText(a[0])); return Value{};
        case Fn::FxVertical: host.graphics("Vertical " + num(d(0))); return Value{};
        case Fn::FxHorizontal: host.graphics("Horizontal " + num(d(0))); return Value{};
        case Fn::FxCommand: host.graphics(commandText(a[0])); return Value{};
        case Fn::FxLocate: host.locate(static_cast<int>(d(0)), static_cast<int>(d(1)), text(a[2])); return Value{};
        case Fn::FxClrText: host.clearText(); return Value{};
        case Fn::FxGetKey: return Value::ofInt(host.getKey());
        default: fail("Syntax ERROR: funzione non disponibile (errore interno)");
        }
    }
};

Machine::Machine(std::shared_ptr<const Program> program, Host& host, std::size_t memoryCells)
    : impl_(std::make_unique<Impl>(std::move(program), host, memoryCells)) {}
Machine::~Machine() = default;

void Machine::cancelled() {
    if (state_ == State::Running || state_ == State::WaitingInput || state_ == State::Ready) state_ = State::Failed;
}
int Machine::currentLine() const noexcept {
    const auto& w = impl_->prog->where;
    if (impl_->pc < 0 || static_cast<std::size_t>(impl_->pc) >= w.size()) return 0;
    return w[static_cast<std::size_t>(impl_->pc)].line;
}

void Machine::runToEnd(std::size_t maxSteps) {
    // Riprende anche una macchina in attesa di input (l'input potrebbe essere arrivato).
    step::Budget budget(maxSteps, 0, nullptr);
    while (state_ != State::Done && state_ != State::Failed) {
        if (budget.exhausted()) throw Error("Time Out: il programma non termina", currentLine(), 0);
        (void)step(budget);
        if (state_ == State::WaitingInput) break;
    }
}

bool Machine::step(step::Budget& budget) {
    if (state_ == State::Done || state_ == State::Failed) return true;
    Impl& m = *impl_;
    const Program& p = *m.prog;
    state_ = State::Running;
    try {
        while (true) {
            if (m.pc < 0 || static_cast<std::size_t>(m.pc) >= p.code.size()) m.fail("Syntax ERROR: salto fuori dal programma (errore interno)");
            const Instr& in = p.code[static_cast<std::size_t>(m.pc)];
            std::int32_t next = m.pc + 1;
            switch (in.op) {
            case Op::PushI: m.push(Value::ofInt(p.ints[static_cast<std::size_t>(in.a)])); break;
            case Op::PushD: m.push(Value::ofDbl(p.doubles[static_cast<std::size_t>(in.a)])); break;
            case Op::PushS: m.push(Value::ofStr(p.strings[static_cast<std::size_t>(in.a)])); break;
            case Op::PushB: m.push(Value::ofBool(in.b != 0)); break;
            case Op::PushVoid: m.push(Value{}); break;
            case Op::LoadG: case Op::LoadL: m.push(m.varRef(in.op == Op::LoadG, in.a)); break;
            case Op::StoreG: case Op::StoreL: {
                Value v = m.pop();
                Value& dst = m.varRef(in.op == Op::StoreG, in.a);
                if (v.k == Value::Str) m.reserveCells(0);
                dst = std::move(v);
                break;
            }
            case Op::LoadIdx: {
                const Value idx = m.pop(), arr = m.pop();
                if (arr.k == Value::Str) m.push(m.stringChar(arr, idx));
                else m.push(m.element(arr, idx));
                break;
            }
            case Op::StoreIdx: {
                Value v = m.pop();
                const Value idx = m.pop(), arr = m.pop();
                if (arr.k == Value::Str) m.fail("Argument ERROR: stringa in sola lettura qui (usa una variabile)");
                Value& e = m.element(arr, idx);
                e = m.convert(std::move(v), arr.a->elem);
                break;
            }
            case Op::LoadIdx2: {
                const Value c = m.pop(), r = m.pop(), arr = m.pop();
                m.push(m.element2(arr, r, c));
                break;
            }
            case Op::StoreIdx2: {
                Value v = m.pop();
                const Value c = m.pop(), r = m.pop(), arr = m.pop();
                Value& e = m.element2(arr, r, c);
                e = m.convert(std::move(v), arr.a->elem);
                break;
            }
            case Op::IncL: case Op::IncG: {
                const bool global = in.op == Op::IncG, post = (in.b & 1) != 0, dec = (in.b & 2) != 0;
                Value& v = m.varRef(global, in.a);
                const Value old = v;
                const Type t = m.varType(global, in.a);
                v = m.convert(m.incremented(old, dec), t == Type::Auto ? (old.k == Value::Dbl ? Type::Double : old.k == Value::Char ? Type::Char : Type::Int) : t);
                m.push(post ? old : v);
                break;
            }
            case Op::IncIdx: case Op::IncIdx2: {
                const bool post = (in.b & 1) != 0, dec = (in.b & 2) != 0;
                Value* e;
                Value arr;
                if (in.op == Op::IncIdx) { const Value idx = m.pop(); arr = m.pop(); e = &m.element(arr, idx); }
                else { const Value c = m.pop(), r = m.pop(); arr = m.pop(); e = &m.element2(arr, r, c); }
                const Value old = *e;
                *e = m.convert(m.incremented(old, dec), arr.a->elem);
                m.push(post ? old : *e);
                break;
            }
            case Op::CompIdx: case Op::CompIdx2: {
                Value rhs = m.pop();
                Value* e;
                Value arr;
                if (in.op == Op::CompIdx) { const Value idx = m.pop(); arr = m.pop(); if (arr.k == Value::Str) m.fail("Argument ERROR: stringa in sola lettura qui (usa una variabile)"); e = &m.element(arr, idx); }
                else { const Value c = m.pop(), r = m.pop(); arr = m.pop(); e = &m.element2(arr, r, c); }
                Value result = in.b == 0 ? std::move(rhs) : m.arith(static_cast<Op>(in.b), *e, rhs);
                *e = m.convert(std::move(result), arr.a->elem);
                m.push(*e);
                break;
            }
            case Op::Conv: m.push(m.convert(m.pop(), static_cast<Type>(in.b))); break;
            case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: case Op::Mod:
            case Op::BitAnd: case Op::BitOr: case Op::BitXor: case Op::Shl: case Op::Shr: {
                const Value b = m.pop(), a = m.pop();
                m.push(m.arith(in.op, a, b));
                break;
            }
            case Op::Neg: {
                const Value a = m.pop();
                if (!a.numeric()) m.fail("Argument ERROR: segno meno su un valore non numerico");
                m.push(a.k == Value::Dbl ? Value::ofDbl(-a.d) : Value::ofInt(-a.i));
                break;
            }
            case Op::Not: m.push(Value::ofBool(!m.convert(m.pop(), Type::Bool).i)); break;
            case Op::BitNot: {
                const Value a = m.pop();
                if (!a.integral()) m.fail("Argument ERROR: ~ su un valore non intero");
                m.push(Value::ofInt(~a.i));
                break;
            }
            case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
                const Value b = m.pop(), a = m.pop();
                m.push(Value::ofBool(m.compare(in.op, a, b)));
                break;
            }
            case Op::Jmp: next = in.a; break;
            case Op::Jz: if (!m.convert(m.pop(), Type::Bool).i) next = in.a; break;
            case Op::Jnz: if (m.convert(m.pop(), Type::Bool).i) next = in.a; break;
            case Op::JzKeep: if (!m.convert(m.stack.empty() ? Value::ofBool(false) : m.stack.back(), Type::Bool).i) next = in.a; break;
            case Op::JnzKeep: if (m.convert(m.stack.empty() ? Value::ofBool(false) : m.stack.back(), Type::Bool).i) next = in.a; break;
            case Op::Pop: (void)m.pop(); break;
            case Op::Dup: if (m.stack.empty()) m.fail("Stack ERROR: pila vuota (errore interno)"); m.push(m.stack.back()); break;
            case Op::Call: {
                if (m.frames.size() >= MaxCallDepth) m.fail("Nesting ERROR: piu' di " + std::to_string(MaxCallDepth) + " chiamate annidate");
                const Function& f = p.functions[static_cast<std::size_t>(in.a)];
                const std::size_t argc = in.b;
                if (m.stack.size() < argc) m.fail("Stack ERROR: argomenti mancanti (errore interno)");
                Impl::Frame fr;
                fr.fn = in.a; fr.ret = next; fr.base = m.locals.size();
                const std::size_t first = m.stack.size() - argc;
                fr.stackBase = first;
                m.locals.resize(fr.base + f.localNames.size());
                for (std::size_t k = f.localNames.size(); k-- > 0;) m.locals[fr.base + k] = zeroOf(f.localTypes[k]);
                for (std::size_t k = 0; k < argc; ++k) {
                    const Type t = k < f.params.size() ? f.params[k] : Type::Auto;
                    Value v = std::move(m.stack[first + k]);
                    m.locals[fr.base + k] = (t == Type::Auto || t == Type::Array) ? std::move(v) : m.convert(std::move(v), t);
                }
                m.stack.resize(first);
                m.frames.push_back(fr);
                next = f.entry;
                break;
            }
            case Op::Ret: {
                Value r = in.b ? m.pop() : Value{};
                if (m.frames.empty()) m.fail("Stack ERROR: return fuori da una funzione (errore interno)");
                const Impl::Frame fr = m.frames.back();
                m.frames.pop_back();
                m.locals.resize(fr.base);
                m.stack.resize(std::min(m.stack.size(), fr.stackBase));
                if (m.frames.empty() && fr.fn == p.mainFunction) exitCode_ = r.integral() ? static_cast<int>(r.i) : 0;
                m.push(std::move(r));
                next = fr.ret;
                break;
            }
            case Op::Builtin: {
                const std::size_t argc = in.b;
                if (m.stack.size() < argc) m.fail("Stack ERROR: argomenti mancanti (errore interno)");
                std::vector<Value> args(std::make_move_iterator(m.stack.end() - static_cast<std::ptrdiff_t>(argc)), std::make_move_iterator(m.stack.end()));
                m.stack.resize(m.stack.size() - argc);
                bool stop = false;
                int code = 0;
                Value r = m.builtin(static_cast<Fn>(in.a), args, stop, code);
                if (stop) { exitCode_ = code; state_ = State::Done; ++executed_; return true; }
                m.push(std::move(r));
                break;
            }
            case Op::NewArr: {
                const Type elem = static_cast<Type>(in.a);
                std::int64_t rows = 0, cols = 1;
                if (in.b == 2) { cols = m.convert(m.pop(), Type::Int).i; rows = m.convert(m.pop(), Type::Int).i; }
                else rows = m.convert(m.pop(), Type::Int).i;
                Value v; v.k = Value::Arr; v.a = m.newArray(elem, rows, cols);
                if (in.b == 2) { v.a->rows = static_cast<std::size_t>(rows); v.a->cols = static_cast<std::size_t>(cols); }
                m.push(std::move(v));
                break;
            }
            case Op::ArrLit: {
                const Type elem = static_cast<Type>(in.a & 0xFF);
                const std::size_t count = static_cast<std::size_t>(in.a) >> 8;
                std::int64_t rows = static_cast<std::int64_t>(count), cols = 1;
                std::vector<Value> values;
                if (in.b == 2) {
                    cols = m.convert(m.pop(), Type::Int).i;
                    rows = m.convert(m.pop(), Type::Int).i;
                }
                if (m.stack.size() < count) m.fail("Stack ERROR: valori mancanti (errore interno)");
                values.assign(std::make_move_iterator(m.stack.end() - static_cast<std::ptrdiff_t>(count)), std::make_move_iterator(m.stack.end()));
                m.stack.resize(m.stack.size() - count);
                if (in.b == 3) {
                    rows = m.convert(m.pop(), Type::Int).i;
                    if (rows < static_cast<std::int64_t>(count)) m.fail("Dimension ERROR: piu' valori della dimensione dell'array");
                }
                Value v; v.k = Value::Arr; v.a = m.newArray(elem, rows, cols);
                for (std::size_t k = 0; k < count; ++k) v.a->data[k] = elem == Type::Auto ? std::move(values[k]) : m.convert(std::move(values[k]), elem);
                m.push(std::move(v));
                break;
            }
            case Op::ReadTok: case Op::ReadLine: case Op::ReadChar: {
                if (in.op == Op::ReadChar) {
                    std::int64_t c = 0;
                    if (!m.readChar(c)) { state_ = State::WaitingInput; return false; }
                    m.push(Value::ofChar(c));
                } else {
                    std::string s;
                    if (!(in.op == Op::ReadTok ? m.readToken(s) : m.readLine(s))) { state_ = State::WaitingInput; return false; }
                    m.push(Value::ofStr(s));
                }
                break;
            }
            case Op::Switch: {
                const Value v = m.pop();
                const SwitchTable& t = p.switches[static_cast<std::size_t>(in.a)];
                next = t.fallback;
                for (const auto& c : t.cases) if (c.first == v.i) { next = c.second; break; }
                break;
            }
            case Op::Halt: state_ = State::Done; ++executed_; return true;
            }
            m.pc = next;
            ++executed_;
            if (!budget.consume()) return false;
        }
    } catch (...) {
        state_ = State::Failed;
        throw;
    }
}

// ---------------------------------------------------------------- WorkspaceHost
bool WorkspaceHost::readLine(std::string& out) {
    if (input.empty()) return false;
    out = input.front();
    input.erase(input.begin());
    return true;
}
void WorkspaceHost::graphics(const std::string& command) {
    commands.push_back(command);
    graphcmd::execute(ws_, command);                            // lo stesso interprete dei comandi grafici di Run-Matrix
}
double WorkspaceHost::calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
    std::vector<std::string> names;
    bind::BindingResolver resolver(&ws_, &ws_);
    for (const auto& l : locals) {
        names.push_back(l.first);
        std::string lower = l.first;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        resolver.bind("var_" + lower, l.second);
    }
    ParseOptions opt;
    opt.boundNames = &names;
    const auto tree = parseExpression(expression, opt);
    return evaluateReal(*tree, resolver.context(ws_.evalContext()));
}
double WorkspaceHost::getVariable(char letter) { return ws_.memory.recall(letter); }
void WorkspaceHost::setVariable(char letter, double value) { ws_.memory.store(letter, value); }

} // namespace fx::prog
