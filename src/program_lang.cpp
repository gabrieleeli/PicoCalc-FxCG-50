// Compilatore dei programmi C, C++ e C# (v1.15.0, F9; solo Pimoroni Pico Plus 2 W).
//
// Questo e' l'analizzatore del LINGUAGGIO DEI PROGRAMMI, non delle espressioni
// della calcolatrice: le espressioni della calcolatrice hanno un solo parser
// (expression.cpp) e i programmi lo usano con fx::calc(). architecture_test
// ammette un analizzatore proprio solo qui (registro: servizio Interp).
//
// Il sorgente diventa direttamente bytecode (program_internal.hpp) in una sola
// passata: le funzioni chiamate prima della definizione, i goto e i break si
// sistemano alla fine. L'analisi e' a discesa ricorsiva con profondita'
// limitata da MaxNesting (Stack ERROR oltre), cosi' resta nei 32 KB di stack.
#include "program_internal.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>

namespace fx::prog {
namespace {

// ---------------------------------------------------------------- lessico
enum class T : std::uint8_t { End, Ident, Int, Float, Char, Str, Op, InterpStart, InterpEnd, ExprStart, ExprEnd };
struct Tok {
    T t{T::End};
    std::string text;
    std::int64_t i{0};
    double d{0};
    int line{1}, col{1};
};

[[noreturn]] void fail(const std::string& msg, int line, int col) { throw Error("Syntax ERROR: " + msg, line, col); }

class SourceLexer {
public:
    SourceLexer(const std::string& src, Dialect d, int line = 1, int col = 1) : s_(src), dialect_(d), line_(line), col_(col) {}
    std::vector<Tok> run(int depth = 0) {
        if (depth > 4) fail("stringhe interpolate troppo annidate", line_, col_);
        std::vector<Tok> out;
        bool lineStart = true;
        while (true) {
            skipSpace(lineStart);
            if (p_ >= s_.size()) { out.push_back(make(T::End, "")); return out; }
            const char c = s_[p_];
            if (c == '#' && lineStart) { directive(); continue; }
            lineStart = false;
            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && p_ + 1 < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_ + 1])))) { out.push_back(number()); continue; }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                Tok t = make(T::Ident, "");
                while (p_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[p_])) || s_[p_] == '_')) t.text += take();
                const auto m = macros_.find(t.text);
                if (m != macros_.end()) {                         // #define NOME valore
                    SourceLexer sub(m->second, dialect_, t.line, t.col);
                    auto toks = sub.run(depth + 1);
                    toks.pop_back();
                    out.insert(out.end(), toks.begin(), toks.end());
                    continue;
                }
                out.push_back(std::move(t));
                continue;
            }
            if (c == '"') { out.push_back(stringLit()); continue; }
            if (c == '\'') { out.push_back(charLit()); continue; }
            if (c == '$' && dialect_ == Dialect::CSharp && p_ + 1 < s_.size() && s_[p_ + 1] == '"') { interpolated(out, depth); continue; }
            if (c == '@' && dialect_ == Dialect::CSharp && p_ + 1 < s_.size() && s_[p_ + 1] == '"') {
                Tok t = make(T::Str, ""); take(); take();
                while (p_ < s_.size()) {
                    if (s_[p_] == '"') { if (p_ + 1 < s_.size() && s_[p_ + 1] == '"') { take(); take(); t.text += '"'; continue; } take(); break; }
                    t.text += take();
                }
                out.push_back(std::move(t));
                continue;
            }
            out.push_back(op());
        }
    }
private:
    Tok make(T t, std::string text) const { Tok k; k.t = t; k.text = std::move(text); k.line = line_; k.col = col_; return k; }
    char take() {
        const char c = s_[p_++];
        if (c == '\n') { ++line_; col_ = 1; }
        else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++col_;
        return c;
    }
    void skipSpace(bool& lineStart) {
        while (p_ < s_.size()) {
            const char c = s_[p_];
            if (c == '\n') { take(); lineStart = true; continue; }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { take(); continue; }
            if (c == '/' && p_ + 1 < s_.size() && s_[p_ + 1] == '/') { while (p_ < s_.size() && s_[p_] != '\n') take(); continue; }
            if (c == '/' && p_ + 1 < s_.size() && s_[p_ + 1] == '*') {
                const int l = line_, k = col_;
                take(); take();
                while (p_ < s_.size() && !(s_[p_] == '*' && p_ + 1 < s_.size() && s_[p_ + 1] == '/')) take();
                if (p_ >= s_.size()) fail("commento /* non chiuso", l, k);
                take(); take();
                continue;
            }
            break;
        }
    }
    void directive() {
        // #include e le altre direttive si ignorano; #define NOME testo (macro senza argomenti).
        std::string lineText;
        while (p_ < s_.size() && s_[p_] != '\n') {
            if (s_[p_] == '\\' && p_ + 1 < s_.size() && s_[p_ + 1] == '\n') { take(); take(); continue; }
            lineText += take();
        }
        std::size_t k = 1;
        while (k < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[k]))) ++k;
        if (lineText.compare(k, 6, "define") != 0) return;
        k += 6;
        while (k < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[k]))) ++k;
        std::size_t e = k;
        while (e < lineText.size() && (std::isalnum(static_cast<unsigned char>(lineText[e])) || lineText[e] == '_')) ++e;
        if (e == k) return;
        if (e < lineText.size() && lineText[e] == '(') fail("macro con argomenti non supportate", line_, 1);
        if (macros_.size() >= 256) fail("troppe macro", line_, 1);
        macros_[lineText.substr(k, e - k)] = lineText.substr(e);
    }
    Tok number() {
        Tok t = make(T::Int, "");
        const std::size_t b = p_;
        if (s_[p_] == '0' && p_ + 1 < s_.size() && (s_[p_ + 1] == 'x' || s_[p_ + 1] == 'X')) {
            take(); take();
            std::uint64_t v = 0;
            bool any = false;
            while (p_ < s_.size() && std::isxdigit(static_cast<unsigned char>(s_[p_]))) {
                const char h = take();
                v = v * 16 + static_cast<std::uint64_t>(std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : (std::tolower(static_cast<unsigned char>(h)) - 'a' + 10));
                any = true;
            }
            if (!any) fail("numero esadecimale senza cifre", t.line, t.col);
            t.i = static_cast<std::int64_t>(v);
        } else {
            bool real = false;
            while (p_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_]))) take();
            if (p_ < s_.size() && s_[p_] == '.') { real = true; take(); while (p_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_]))) take(); }
            if (p_ < s_.size() && (s_[p_] == 'e' || s_[p_] == 'E')) {
                std::size_t q = p_ + 1;
                if (q < s_.size() && (s_[q] == '+' || s_[q] == '-')) ++q;
                if (q < s_.size() && std::isdigit(static_cast<unsigned char>(s_[q]))) {
                    real = true;
                    while (p_ < q) take();
                    while (p_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_]))) take();
                }
            }
            const std::string text = s_.substr(b, p_ - b);
            if (real) { t.t = T::Float; t.d = std::strtod(text.c_str(), nullptr); }
            else {
                errno = 0;
                t.i = static_cast<std::int64_t>(std::strtoull(text.c_str(), nullptr, 10));
                if (errno == ERANGE) fail("intero troppo grande", t.line, t.col);
            }
        }
        // suffissi: f, F, d, D, m, u, l, L
        bool floatSuffix = false;
        while (p_ < s_.size() && std::strchr("fFdDmMuUlL", s_[p_])) {
            const char sfx = take();
            if (sfx == 'f' || sfx == 'F' || sfx == 'd' || sfx == 'D' || sfx == 'm' || sfx == 'M') floatSuffix = true;
        }
        if (floatSuffix && t.t == T::Int) { t.t = T::Float; t.d = static_cast<double>(t.i); }
        if (p_ < s_.size() && (std::isalpha(static_cast<unsigned char>(s_[p_])) || s_[p_] == '_')) fail("numero seguito da lettere", line_, col_);
        return t;
    }
    char escape() {
        const char e = take();
        switch (e) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
        default: fail(std::string("sequenza di escape \\") + e + " non valida", line_, col_);
        }
    }
    Tok stringLit() {
        Tok t = make(T::Str, "");
        take();
        while (true) {
            if (p_ >= s_.size() || s_[p_] == '\n') fail("stringa non chiusa", t.line, t.col);
            const char c = take();
            if (c == '"') break;
            if (c == '\\') { const char e = escape(); if (e == '\0') fail("carattere nullo nella stringa", line_, col_); t.text += e; }
            else t.text += c;
            if (t.text.size() > 4096) fail("stringa troppo lunga", t.line, t.col);
        }
        return t;
    }
    Tok charLit() {
        Tok t = make(T::Char, "");
        take();
        if (p_ >= s_.size()) fail("carattere non chiuso", t.line, t.col);
        char c = take();
        if (c == '\\') c = escape();
        if (p_ >= s_.size() || s_[p_] != '\'') fail("carattere non chiuso", t.line, t.col);
        take();
        t.i = static_cast<unsigned char>(c);
        return t;
    }
    void interpolated(std::vector<Tok>& out, int depth) {
        // C#: $"x = {x}, y = {y:F2}" -> InterpStart, Str, ExprStart, token..., ExprEnd, ..., InterpEnd
        out.push_back(make(T::InterpStart, ""));
        take(); take();
        std::string text;
        auto flush = [&] { if (!text.empty()) { Tok s = make(T::Str, text); out.push_back(std::move(s)); text.clear(); } };
        while (true) {
            if (p_ >= s_.size() || s_[p_] == '\n') fail("stringa interpolata non chiusa", line_, col_);
            const char c = take();
            if (c == '"') break;
            if (c == '\\') { text += escape(); continue; }
            if (c == '{' && p_ < s_.size() && s_[p_] == '{') { take(); text += '{'; continue; }
            if (c == '}' && p_ < s_.size() && s_[p_] == '}') { take(); text += '}'; continue; }
            if (c == '}') fail("'}' senza '{' nella stringa interpolata", line_, col_);
            if (c == '{') {
                flush();
                const int l = line_, k = col_;
                std::string inner, format;
                int nest = 0;
                while (true) {
                    if (p_ >= s_.size() || s_[p_] == '\n') fail("'{' non chiusa nella stringa interpolata", l, k);
                    const char d = take();
                    if (d == '(' || d == '[') ++nest;
                    if (d == ')' || d == ']') --nest;
                    if (d == '}' && nest <= 0) break;
                    if (d == ':' && nest <= 0) {
                        while (p_ < s_.size() && s_[p_] != '}') format += take();
                        continue;
                    }
                    inner += d;
                }
                Tok start = make(T::ExprStart, format);
                start.line = l; start.col = k;
                out.push_back(std::move(start));
                SourceLexer sub(inner, dialect_, l, k);
                auto toks = sub.run(depth + 1);
                toks.pop_back();
                out.insert(out.end(), toks.begin(), toks.end());
                out.push_back(make(T::ExprEnd, ""));
                continue;
            }
            text += c;
        }
        flush();
        out.push_back(make(T::InterpEnd, ""));
    }
    Tok op() {
        static const char* const ops[] = {"<<=", ">>=", "...", "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
                                          "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "??"};
        Tok t = make(T::Op, "");
        for (const char* o : ops) {
            const std::size_t n = std::strlen(o);
            if (s_.compare(p_, n, o) == 0) { for (std::size_t k = 0; k < n; ++k) take(); t.text = o; return t; }
        }
        const char c = s_[p_];
        if (std::strchr("+-*/%<>=!&|^~?:;,.(){}[]", c)) { take(); t.text = std::string(1, c); return t; }
        fail(std::string("carattere non valido '") + (static_cast<unsigned char>(c) < 0x80 ? std::string(1, c) : std::string("?")) + "'", line_, col_);
    }
    const std::string& s_;
    Dialect dialect_;
    std::size_t p_{0};
    int line_, col_;
    std::map<std::string, std::string> macros_;
};

// ---------------------------------------------------------------- compilatore
struct Var {
    int slot{-1};
    bool global{false};
    Type type{Type::Auto};
    Type elem{Type::Auto};                                // array: tipo degli elementi
    int dims{0};
};
struct Expr {
    enum K : std::uint8_t { Value, Variable, Index1, Index2 } k{Value};
    Var v;
    Type type{Type::Auto};
};
struct Builtin { const char* name; Fn fn; int minArgs, maxArgs; };

const Builtin kBuiltins[] = {
    {"printf", Fn::Printf, 1, 32}, {"puts", Fn::Puts, 1, 1}, {"putchar", Fn::Putchar, 1, 1},
    {"sin", Fn::Sin, 1, 1}, {"cos", Fn::Cos, 1, 1}, {"tan", Fn::Tan, 1, 1}, {"asin", Fn::Asin, 1, 1}, {"acos", Fn::Acos, 1, 1},
    {"atan", Fn::Atan, 1, 1}, {"atan2", Fn::Atan2, 2, 2}, {"sqrt", Fn::Sqrt, 1, 1}, {"pow", Fn::Pow, 2, 2}, {"exp", Fn::Exp, 1, 1},
    {"log", Fn::Log, 1, 1}, {"log10", Fn::Log10, 1, 1}, {"fabs", Fn::Fabs, 1, 1}, {"abs", Fn::Abs, 1, 1}, {"floor", Fn::Floor, 1, 1},
    {"ceil", Fn::Ceil, 1, 1}, {"round", Fn::Round, 1, 1}, {"fmod", Fn::Fmod, 2, 2}, {"fmin", Fn::Min, 2, 2}, {"fmax", Fn::Max, 2, 2},
    {"min", Fn::Min, 2, 2}, {"max", Fn::Max, 2, 2}, {"sinh", Fn::Sinh, 1, 1}, {"cosh", Fn::Cosh, 1, 1}, {"tanh", Fn::Tanh, 1, 1},
    {"trunc", Fn::Trunc, 1, 1}, {"strlen", Fn::Strlen, 1, 1}, {"strcmp", Fn::Strcmp, 2, 2}, {"atoi", Fn::ParseInt, 1, 1},
    {"atof", Fn::ParseDouble, 1, 1}, {"rand", Fn::Rand, 0, 0}, {"srand", Fn::Srand, 1, 1}, {"exit", Fn::Exit, 1, 1},
    {"to_string", Fn::ToStr, 1, 1}, {"stoi", Fn::ParseInt, 1, 1}, {"stod", Fn::ParseDouble, 1, 1}, {"stol", Fn::ParseInt, 1, 1},
    {"stof", Fn::ParseDouble, 1, 1}, {"toupper", Fn::ToUpper, 1, 1}, {"tolower", Fn::ToLower, 1, 1},
    // C#
    {"Math.Sin", Fn::Sin, 1, 1}, {"Math.Cos", Fn::Cos, 1, 1}, {"Math.Tan", Fn::Tan, 1, 1}, {"Math.Asin", Fn::Asin, 1, 1},
    {"Math.Acos", Fn::Acos, 1, 1}, {"Math.Atan", Fn::Atan, 1, 1}, {"Math.Atan2", Fn::Atan2, 2, 2}, {"Math.Sqrt", Fn::Sqrt, 1, 1},
    {"Math.Pow", Fn::Pow, 2, 2}, {"Math.Exp", Fn::Exp, 1, 1}, {"Math.Log", Fn::Log, 1, 1}, {"Math.Log10", Fn::Log10, 1, 1},
    {"Math.Abs", Fn::Abs, 1, 1}, {"Math.Floor", Fn::Floor, 1, 1}, {"Math.Ceiling", Fn::Ceil, 1, 1}, {"Math.Round", Fn::Round, 1, 1},
    {"Math.Max", Fn::Max, 2, 2}, {"Math.Min", Fn::Min, 2, 2}, {"Math.Truncate", Fn::Trunc, 1, 1}, {"Math.Sinh", Fn::Sinh, 1, 1},
    {"Math.Cosh", Fn::Cosh, 1, 1}, {"Math.Tanh", Fn::Tanh, 1, 1},
    {"Console.Write", Fn::Print, 0, 1}, {"Console.WriteLine", Fn::PrintLine, 0, 1}, {"Console.Clear", Fn::FxClrText, 0, 0},
    {"int.Parse", Fn::ParseInt, 1, 1}, {"double.Parse", Fn::ParseDouble, 1, 1}, {"Convert.ToInt32", Fn::ToInt, 1, 1},
    {"Convert.ToDouble", Fn::ToDouble, 1, 1}, {"Convert.ToString", Fn::ToStr, 1, 1},
    // libreria della calcolatrice: fx::nome (C++), Fx.Nome (C#), fx_nome (C)
    {"fx::calc", Fn::FxCalc, 1, 1}, {"fx::getVar", Fn::FxGetVar, 1, 1}, {"fx::setVar", Fn::FxSetVar, 2, 2}, {"fx::cls", Fn::FxCls, 0, 0},
    {"fx::viewWindow", Fn::FxViewWindow, 6, 6}, {"fx::plot", Fn::FxPlot, 2, 2}, {"fx::line", Fn::FxLine, 4, 4}, {"fx::circle", Fn::FxCircle, 3, 3},
    {"fx::text", Fn::FxText, 3, 3}, {"fx::graphY", Fn::FxGraphY, 1, 1}, {"fx::vertical", Fn::FxVertical, 1, 1}, {"fx::horizontal", Fn::FxHorizontal, 1, 1},
    {"fx::locate", Fn::FxLocate, 3, 3}, {"fx::clrText", Fn::FxClrText, 0, 0}, {"fx::getKey", Fn::FxGetKey, 0, 0}, {"fx::command", Fn::FxCommand, 1, 1},
    {"Fx.Calc", Fn::FxCalc, 1, 1}, {"Fx.GetVar", Fn::FxGetVar, 1, 1}, {"Fx.SetVar", Fn::FxSetVar, 2, 2}, {"Fx.Cls", Fn::FxCls, 0, 0},
    {"Fx.ViewWindow", Fn::FxViewWindow, 6, 6}, {"Fx.Plot", Fn::FxPlot, 2, 2}, {"Fx.Line", Fn::FxLine, 4, 4}, {"Fx.Circle", Fn::FxCircle, 3, 3},
    {"Fx.Text", Fn::FxText, 3, 3}, {"Fx.GraphY", Fn::FxGraphY, 1, 1}, {"Fx.Vertical", Fn::FxVertical, 1, 1}, {"Fx.Horizontal", Fn::FxHorizontal, 1, 1},
    {"Fx.Locate", Fn::FxLocate, 3, 3}, {"Fx.ClrText", Fn::FxClrText, 0, 0}, {"Fx.GetKey", Fn::FxGetKey, 0, 0}, {"Fx.Command", Fn::FxCommand, 1, 1},
    {"fx_calc", Fn::FxCalc, 1, 1}, {"fx_getvar", Fn::FxGetVar, 1, 1}, {"fx_setvar", Fn::FxSetVar, 2, 2}, {"fx_cls", Fn::FxCls, 0, 0},
    {"fx_viewwindow", Fn::FxViewWindow, 6, 6}, {"fx_plot", Fn::FxPlot, 2, 2}, {"fx_line", Fn::FxLine, 4, 4}, {"fx_circle", Fn::FxCircle, 3, 3},
    {"fx_text", Fn::FxText, 3, 3}, {"fx_graphy", Fn::FxGraphY, 1, 1}, {"fx_vertical", Fn::FxVertical, 1, 1}, {"fx_horizontal", Fn::FxHorizontal, 1, 1},
    {"fx_locate", Fn::FxLocate, 3, 3}, {"fx_clrtext", Fn::FxClrText, 0, 0}, {"fx_getkey", Fn::FxGetKey, 0, 0}, {"fx_command", Fn::FxCommand, 1, 1},
};
const Builtin* findBuiltin(const std::string& name) {
    for (const Builtin& b : kBuiltins) if (name == b.name) return &b;
    return nullptr;
}
bool isTypeWord(const std::string& w, Dialect d) {
    static const char* const common[] = {"int", "long", "short", "char", "bool", "float", "double", "void", "unsigned", "signed", "const", "static"};
    for (const char* t : common) if (w == t) return true;
    if (d == Dialect::C) return w == "_Bool";
    if (d == Dialect::Cpp) return w == "string" || w == "auto" || w == "std" || w == "constexpr" || w == "inline";
    return w == "string" || w == "var" || w == "String" || w == "decimal" || w == "byte" || w == "uint" || w == "ulong" ||
           w == "public" || w == "private" || w == "readonly" || w == "internal" || w == "protected" || w == "Int32" || w == "Double";
}

class Compiler {
public:
    Compiler(std::vector<Tok> toks, Dialect d) : t_(std::move(toks)), d_(d) {
        prog_ = std::make_shared<Program>();
        prog_->dialect = d;
        out_ = &prog_->code; where_ = &prog_->where;
    }
    std::shared_ptr<Program> run() {
        emit(Op::Jmp, 0, 0);                                     // verso l'inizializzazione (in fondo)
        while (!at(T::End)) topLevel();
        // funzioni chiamate ma mai definite
        for (const auto& f : prog_->functions) if (f.entry < 0) fail("funzione '" + f.name + "' dichiarata ma non definita", f.line, f.column);
        for (const auto& c : pendingCalls_) {                     // chiamate scritte prima della definizione
            const Function& f = prog_->functions[static_cast<std::size_t>(c.first)];
            if (f.params.size() != static_cast<std::size_t>(c.second))
                fail("'" + f.name + "' vuole " + std::to_string(f.params.size()) + " argomenti", f.line, f.column);
        }
        const std::int32_t mainIdx = findFunction(d_ == Dialect::CSharp ? "Main" : "main");
        if (mainIdx < 0) fail(d_ == Dialect::CSharp ? "manca static void Main()" : "manca la funzione main()", t_.back().line, t_.back().col);
        prog_->mainFunction = mainIdx;
        // inizializzazione: globali, poi main e Halt
        const std::int32_t base = static_cast<std::int32_t>(prog_->code.size());
        for (std::size_t i = 0; i < init_.size(); ++i) {
            Instr ins = init_[i];
            if (ins.op == Op::Jmp || ins.op == Op::Jz || ins.op == Op::Jnz || ins.op == Op::JzKeep || ins.op == Op::JnzKeep) ins.a += base;
            prog_->code.push_back(ins);
            prog_->where.push_back(initWhere_[i]);
        }
        prog_->code[0].a = base;
        prog_->init = base;
        const Function& m = prog_->functions[static_cast<std::size_t>(mainIdx)];
        for (Type pt : m.params) {                                 // main(int argc, char** argv) / Main(string[] args)
            if (pt == Type::Array) { emit(Op::PushI, 0, constInt(0)); emit(Op::NewArr, 1, static_cast<std::int32_t>(Type::String)); }
            else emit(Op::PushI, 0, constInt(0));
        }
        emit(Op::Call, static_cast<std::uint8_t>(m.params.size()), mainIdx);
        emit(Op::Halt, 0, 0);
        if (prog_->code.size() > MaxCode) fail("programma troppo lungo", 1, 1);
        return prog_;
    }

private:
    // ---- token
    const Tok& cur() const { return t_[p_]; }
    const Tok& peek(std::size_t k = 1) const { return t_[std::min(p_ + k, t_.size() - 1)]; }
    bool at(T t) const { return cur().t == t; }
    bool isOp(const char* o) const { return cur().t == T::Op && cur().text == o; }
    bool isWord(const char* w) const { return cur().t == T::Ident && cur().text == w; }
    bool accept(const char* o) { if (isOp(o)) { ++p_; return true; } return false; }
    bool acceptWord(const char* w) { if (isWord(w)) { ++p_; return true; } return false; }
    void expect(const char* o) { if (!accept(o)) fail(std::string("atteso '") + o + "'", cur().line, cur().col); }
    std::string ident() {
        if (!at(T::Ident)) fail("atteso un nome", cur().line, cur().col);
        return t_[p_++].text;
    }
    [[noreturn]] void error(const std::string& m) const { fail(m, cur().line, cur().col); }
    struct Depth {
        explicit Depth(Compiler& c) : c_(c) {
            if (++c_.depth_ > MaxNesting) throw Error("Stack ERROR: annidamento oltre " + std::to_string(MaxNesting) + " livelli", c_.cur().line, c_.cur().col);
        }
        ~Depth() { --c_.depth_; }
        Compiler& c_;
    };

    // ---- emissione
    std::int32_t emit(Op op, std::uint8_t b, std::int32_t a) {
        out_->push_back(Instr{op, b, a});
        where_->push_back(Where{cur().line, cur().col});
        if (out_->size() > MaxCode) fail("programma troppo lungo", cur().line, cur().col);
        return static_cast<std::int32_t>(out_->size() - 1);
    }
    std::int32_t here() const { return static_cast<std::int32_t>(out_->size()); }
    void patch(std::int32_t at, std::int32_t target) { (*out_)[static_cast<std::size_t>(at)].a = target; }
    std::int32_t constInt(std::int64_t v) {
        for (std::size_t i = 0; i < prog_->ints.size(); ++i) if (prog_->ints[i] == v) return static_cast<std::int32_t>(i);
        prog_->ints.push_back(v);
        return static_cast<std::int32_t>(prog_->ints.size() - 1);
    }
    std::int32_t constDouble(double v) {
        for (std::size_t i = 0; i < prog_->doubles.size(); ++i) if (std::memcmp(&prog_->doubles[i], &v, sizeof v) == 0) return static_cast<std::int32_t>(i);
        prog_->doubles.push_back(v);
        return static_cast<std::int32_t>(prog_->doubles.size() - 1);
    }
    std::int32_t constString(const std::string& v) {
        for (std::size_t i = 0; i < prog_->strings.size(); ++i) if (prog_->strings[i] == v) return static_cast<std::int32_t>(i);
        prog_->strings.push_back(v);
        return static_cast<std::int32_t>(prog_->strings.size() - 1);
    }
    void pushString(const std::string& s) { emit(Op::PushS, 0, constString(s)); }
    void builtin(Fn f, int argc) { emit(Op::Builtin, static_cast<std::uint8_t>(argc), static_cast<std::int32_t>(f)); }

    // ---- nomi
    std::int32_t findFunction(const std::string& n) const {
        for (std::size_t i = 0; i < prog_->functions.size(); ++i) if (prog_->functions[i].name == n) return static_cast<std::int32_t>(i);
        return -1;
    }
    bool lookup(const std::string& n, Var& out) const {
        for (auto s = scopes_.rbegin(); s != scopes_.rend(); ++s) {
            const auto it = s->find(n);
            if (it != s->end()) { out = it->second; return true; }
        }
        const auto g = globals_.find(n);
        if (g != globals_.end()) { out = g->second; return true; }
        return false;
    }
    Var declare(const std::string& n, Type type, Type elem, int dims) {
        Var v;
        v.type = type; v.elem = elem; v.dims = dims;
        if (fn_ < 0) {
            if (globals_.count(n)) error("'" + n + "' gia' dichiarata");
            v.global = true;
            v.slot = static_cast<int>(prog_->globalNames.size());
            prog_->globalNames.push_back(n);
            prog_->globalTypes.push_back(type);
            globals_[n] = v;
        } else {
            if (scopes_.back().count(n)) error("'" + n + "' gia' dichiarata in questo blocco");
            Function& f = prog_->functions[static_cast<std::size_t>(fn_)];
            v.slot = static_cast<int>(f.localNames.size());
            f.localNames.push_back(n);
            f.localTypes.push_back(type);
            scopes_.back()[n] = v;
        }
        return v;
    }

    // ---- tipi
    struct TypeSpec { Type base{Type::Auto}; bool array{false}; int dims{0}; bool any{false}; };
    bool startsType() const {
        if (!at(T::Ident)) return false;
        const std::string& w = cur().text;
        if (!isTypeWord(w, d_)) return false;
        if (w == "std") return peek().t == T::Op && peek().text == "::" && peek(2).t == T::Ident && peek(2).text == "string";
        return true;
    }
    TypeSpec typeSpec() {
        TypeSpec ts;
        bool unsignedWord = false;
        while (at(T::Ident) && isTypeWord(cur().text, d_)) {
            const std::string w = cur().text;
            if (w == "std") {
                if (!(peek().t == T::Op && peek().text == "::")) break;
                p_ += 2;
                if (!isWord("string")) error("tipo std:: non supportato");
                ++p_;
                ts.base = Type::String; ts.any = true;
                continue;
            }
            if (w == "const" || w == "static" || w == "public" || w == "private" || w == "readonly" || w == "internal" ||
                w == "protected" || w == "constexpr" || w == "inline" || w == "signed") { ++p_; continue; }
            if (w == "unsigned") { unsignedWord = true; ++p_; if (ts.base == Type::Auto) ts.base = Type::Int; ts.any = true; continue; }
            ++p_;
            ts.any = true;
            if (w == "int" || w == "long" || w == "short" || w == "byte" || w == "uint" || w == "ulong" || w == "Int32") { if (ts.base != Type::Char) ts.base = Type::Int; }
            else if (w == "char") ts.base = Type::Char;
            else if (w == "bool" || w == "_Bool") ts.base = Type::Bool;
            else if (w == "float" || w == "double" || w == "decimal" || w == "Double") ts.base = Type::Double;
            else if (w == "void") ts.base = Type::Void;
            else if (w == "string" || w == "String") ts.base = Type::String;
            else if (w == "auto" || w == "var") ts.base = Type::Auto;
        }
        (void)unsignedWord;
        if (!ts.any) error("atteso un tipo");
        // char* / const char* = stringa; altri puntatori non sono supportati
        while (isOp("*")) {
            if (ts.base != Type::Char) error("puntatori non supportati (usa gli array)");
            ++p_;
            ts.base = Type::String;
            if (isWord("const")) ++p_;
        }
        if (isOp("&") || isOp("&&")) error("parametri e variabili per riferimento non supportati");
        // C#: int[] e double[,]
        if (d_ == Dialect::CSharp && isOp("[")) {
            ++p_;
            ts.array = true; ts.dims = 1;
            while (accept(",")) ++ts.dims;
            expect("]");
            if (ts.dims > 2) error("array con piu' di 2 dimensioni non supportati");
        }
        return ts;
    }

    // ---- livello superiore
    void topLevel() {
        Depth guard(*this);
        if (acceptWord("using")) { while (!isOp(";") && !at(T::End)) ++p_; expect(";"); return; }
        if (acceptWord("namespace")) {
            while (at(T::Ident) || isOp(".") || isOp("::")) ++p_;
            if (accept(";")) return;                               // namespace di file (C# 10)
            expect("{");
            while (!isOp("}") && !at(T::End)) topLevel();
            expect("}");
            return;
        }
        if (d_ == Dialect::CSharp) {                                // public static class Program { ... }
            std::size_t q = p_;
            auto modifier = [](const std::string& w) { return w == "public" || w == "internal" || w == "static" || w == "sealed" || w == "partial" || w == "abstract"; };
            while (q < t_.size() && t_[q].t == T::Ident && modifier(t_[q].text)) ++q;
            if (q < t_.size() && t_[q].t == T::Ident && (t_[q].text == "class" || t_[q].text == "struct")) p_ = q;
        }
        if (isWord("class") || (d_ == Dialect::CSharp && isWord("struct"))) {
            if (d_ != Dialect::CSharp) error("classi non supportate (solo in C# per contenere i metodi static)");
            ++p_;
            (void)ident();
            expect("{");
            while (!isOp("}") && !at(T::End)) member();
            expect("}");
            accept(";");
            return;
        }
        if (isWord("struct") || isWord("enum") || isWord("union") || isWord("typedef") || isWord("template")) error("'" + cur().text + "' non supportato");
        definition(false);
    }
    void member() {
        Depth guard(*this);
        if (isOp(";")) { ++p_; return; }
        definition(true);
    }
    // Tipo nome ( ... ) { ... }   oppure   Tipo nome [= init], ...;
    void definition(bool csharpMember) {
        if (!startsType()) error("attesa una dichiarazione o una funzione");
        (void)csharpMember;                                        // in C# i campi static sono variabili globali
        const TypeSpec ts = typeSpec();
        const Tok nameTok = cur();
        const std::string name = ident();
        if (isOp("(")) { functionDefinition(ts, name, nameTok); return; }
        declarators(ts, name, nameTok, true);
    }
    void functionDefinition(const TypeSpec& ts, const std::string& name, const Tok& nameTok) {
        if (fn_ >= 0) error("funzioni annidate non supportate");
        if (findBuiltin(name)) fail("'" + name + "' e' una funzione della libreria", nameTok.line, nameTok.col);
        expect("(");
        std::vector<std::pair<std::string, TypeSpec>> params;
        if (isWord("void") && peek().t == T::Op && peek().text == ")") ++p_;
        while (!isOp(")")) {
            TypeSpec pt = typeSpec();
            std::string pn = at(T::Ident) ? ident() : std::string("_p") + std::to_string(params.size());
            if (accept("[")) {                                    // int a[] / int a[10] / char* argv[]
                while (!isOp("]") && !at(T::End)) ++p_;
                expect("]");
                if (pt.base == Type::Char) pt.base = Type::String;
                else { pt.array = true; pt.dims = 1; }
                if (accept("[")) { while (!isOp("]") && !at(T::End)) ++p_; expect("]"); pt.array = true; pt.dims = 2; }
            }
            params.emplace_back(pn, pt);
            if (!accept(",")) break;
        }
        expect(")");
        std::int32_t idx = findFunction(name);
        if (idx < 0) {
            Function f;
            f.name = name; f.line = nameTok.line; f.column = nameTok.col;
            prog_->functions.push_back(f);
            idx = static_cast<std::int32_t>(prog_->functions.size() - 1);
        } else if (prog_->functions[static_cast<std::size_t>(idx)].entry >= 0 && !isOp(";")) {
            fail("funzione '" + name + "' definita due volte", nameTok.line, nameTok.col);
        }
        Function& f = prog_->functions[static_cast<std::size_t>(idx)];
        f.params.clear();
        for (const auto& p : params) f.params.push_back(p.second.array ? Type::Array : p.second.base);
        f.ret = ts.array ? Type::Array : ts.base;
        if (accept(";")) return;                                   // prototipo
        // corpo
        f.localNames.clear(); f.localTypes.clear();
        f.entry = here();
        fn_ = idx;
        labels_.clear(); gotos_.clear();
        scopes_.emplace_back();
        for (const auto& p : params) declare(p.first, p.second.array ? Type::Array : p.second.base, p.second.base, p.second.dims);
        expect("{");
        while (!isOp("}")) {
            if (at(T::End)) error("manca '}' alla fine della funzione " + name);
            statement();
        }
        ++p_;
        scopes_.pop_back();
        // ritorno implicito (main: 0)
        const Function& g = prog_->functions[static_cast<std::size_t>(idx)];
        if (g.ret == Type::Void) emit(Op::Ret, 0, 0);
        else { emit(Op::PushI, 0, constInt(0)); emit(Op::Ret, 1, 0); }
        for (const auto& go : gotos_) {
            const auto it = labels_.find(go.second.first);
            if (it == labels_.end()) throw Error("Go ERROR: etichetta '" + go.second.first + "' non trovata", go.second.second.first, go.second.second.second);
            patch(go.first, it->second);
        }
        fn_ = -1;
    }
    void declarators(const TypeSpec& ts, std::string name, Tok nameTok, bool requireSemicolon) {
        while (true) {
            declaration(ts, name, nameTok);
            if (!accept(",")) break;
            nameTok = cur();
            name = ident();
        }
        if (requireSemicolon) expect(";");
    }
    void declaration(const TypeSpec& ts, const std::string& name, const Tok& nameTok) {
        (void)nameTok;
        // C/C++: nome[dim][dim] (char nome[n] e' una stringa)
        std::vector<bool> sized;
        int cdims = 0;
        const bool global = fn_ < 0;
        std::vector<Instr>* saveOut = out_;
        std::vector<Where>* saveWhere = where_;
        if (global) { out_ = &init_; where_ = &initWhere_; }
        struct Restore { Compiler& c; std::vector<Instr>* o; std::vector<Where>* w; ~Restore() { c.out_ = o; c.where_ = w; } } restore{*this, saveOut, saveWhere};
        Type base = ts.base;
        bool cString = false;
        int dims = ts.array ? ts.dims : 0;
        Type elem = base;
        if (!ts.array && isOp("[")) {
            if (base == Type::Char) {                             // char s[20]: una stringa
                ++p_;
                while (!isOp("]") && !at(T::End)) ++p_;
                expect("]");
                cString = true;
            } else {
                while (accept("[")) {
                    ++cdims;
                    if (cdims > 2) error("array con piu' di 2 dimensioni non supportati");
                    if (isOp("]")) { sized.push_back(false); ++p_; continue; }
                    // la dimensione e' un'espressione intera valutata ora
                    expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
                    sized.push_back(true);
                    expect("]");
                }
                dims = cdims;
                // int a[2][3] = {...}: le dimensioni vengono dai valori; int a[5] = {1, 2}: 5 elementi
                if (isOp("=") && cdims == 2) { if (sized[0]) emit(Op::Pop, 0, 0); if (sized[1]) emit(Op::Pop, 0, 0); }
            }
        }
        if (base == Type::Void && dims == 0) error("variabile di tipo void");
        const Type varType = dims > 0 ? Type::Array : (cString ? Type::String : base);
        // Inizializzatore (prima di dichiarare il nome: int x = x non vede la nuova x)
        bool initialised = false;
        if (accept("=")) {
            initialised = true;
            if (dims > 0 && isOp("{")) arrayLiteral(elem, dims, cdims == 1 && sized[0]);
            else if (dims > 0 && cdims == 1 && sized[0]) error("un array si inizializza con { ... }");
            else if (dims > 0 && d_ == Dialect::CSharp && isWord("new")) newArray(elem, dims);
            else { expression(); }
        } else if (d_ == Dialect::Cpp && dims == 0 && isOp("{")) {   // int x{5};
            ++p_;
            if (!isOp("}")) expression(); else emit(Op::PushI, 0, constInt(0));
            expect("}");
            initialised = true;
        } else if (dims > 0 && cdims > 0) {
            if (cdims == 1 && !sized[0]) error("array senza dimensione ne' valori");
            if (cdims == 2 && (!sized[0] || !sized[1])) error("array senza dimensioni");
            emit(Op::NewArr, static_cast<std::uint8_t>(cdims), static_cast<std::int32_t>(elem));
            initialised = true;
        }
        const Var v = declare(name, varType, elem, dims);
        if (!initialised) {
            // valore iniziale del tipo (C locali: comunque 0, comportamento definito)
            switch (varType) {
            case Type::Double: emit(Op::PushD, 0, constDouble(0)); break;
            case Type::Bool: emit(Op::PushB, 0, 0); break;
            case Type::String: pushString(""); break;
            case Type::Char: case Type::Int: case Type::Auto: emit(Op::PushI, 0, constInt(0)); break;
            default: emit(Op::PushVoid, 0, 0); break;
            }
        }
        if (dims == 0 && varType != Type::Auto) emit(Op::Conv, static_cast<std::uint8_t>(varType), 0);
        store(v);
    }
    // { a, b, c } oppure { {..}, {..} }: valori in ordine di righe
    // sized: la dimensione (1D) e' gia' sulla pila sotto i valori (int a[5] = {1, 2}).
    void arrayLiteral(Type elem, int dims, bool sized) {
        NoComma nc(*this);
        std::size_t count = 0;
        int rows = 0, cols = -1;
        expect("{");
        if (dims == 1) {
            while (!isOp("}")) {
                expression();
                if (elem != Type::Auto) emit(Op::Conv, static_cast<std::uint8_t>(elem), 0);
                if (++count > 4096) error("troppi valori nell'inizializzazione");
                if (!accept(",")) break;
            }
            expect("}");
            // a: tipo degli elementi e numero dei valori; b: 1 senza dimensione, 3 con la dimensione sotto
            emit(Op::ArrLit, sized ? 3 : 1, static_cast<std::int32_t>(elem) | static_cast<std::int32_t>(count << 8));
            return;
        }
        while (!isOp("}")) {
            expect("{");
            int c = 0;
            while (!isOp("}")) {
                expression();
                if (elem != Type::Auto) emit(Op::Conv, static_cast<std::uint8_t>(elem), 0);
                ++c;
                if (!accept(",")) break;
            }
            expect("}");
            if (cols >= 0 && c != cols) error("righe di lunghezza diversa nell'inizializzazione");
            cols = c;
            ++rows;
            count += static_cast<std::size_t>(c);
            if (count > 4096) error("troppi valori nell'inizializzazione");
            if (!accept(",")) break;
        }
        expect("}");
        if (rows == 0 || cols <= 0) error("inizializzazione vuota");
        emit(Op::PushI, 0, constInt(rows));
        emit(Op::PushI, 0, constInt(cols));
        emit(Op::ArrLit, 2, static_cast<std::int32_t>(elem) | static_cast<std::int32_t>(count << 8));
    }
    // C#: new int[n] / new double[r, c] / new int[] {1, 2}
    void newArray(Type elem, int dims) {
        ++p_;                                                      // new
        const TypeSpec ts = baseTypeOnly();
        if (ts.base != Type::Auto) elem = ts.base;
        expect("[");
        if (isOp("]") || isOp(",")) {
            int d = 1;
            while (accept(",")) ++d;
            expect("]");
            if (d != dims) error("dimensioni dell'array diverse dalla dichiarazione");
            arrayLiteral(elem, dims, false);
            return;
        }
        expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
        int d = 1;
        while (accept(",")) { expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0); ++d; }
        expect("]");
        if (d != dims) error("dimensioni dell'array diverse dalla dichiarazione");
        if (isOp("{")) error("dimensione e valori insieme non supportati: usa new T[] { ... }");
        emit(Op::NewArr, static_cast<std::uint8_t>(d), static_cast<std::int32_t>(elem));
    }
    TypeSpec baseTypeOnly() {
        TypeSpec ts;
        if (!at(T::Ident)) error("atteso un tipo dopo new");
        const std::string w = ident();
        if (w == "int" || w == "long" || w == "short" || w == "byte") ts.base = Type::Int;
        else if (w == "double" || w == "float" || w == "decimal") ts.base = Type::Double;
        else if (w == "bool") ts.base = Type::Bool;
        else if (w == "char") ts.base = Type::Char;
        else if (w == "string" || w == "String") ts.base = Type::String;
        else error("tipo '" + w + "' non supportato in new");
        return ts;
    }

    // ---- istruzioni
    struct Loop { std::vector<std::int32_t> breaks, continues; bool isSwitch{false}; };
    std::vector<Loop> loops_;
    void block() {
        expect("{");
        scopes_.emplace_back();
        while (!isOp("}")) {
            if (at(T::End)) error("manca '}'");
            statement();
        }
        ++p_;
        scopes_.pop_back();
    }
    void statement() {
        Depth guard(*this);
        if (isOp("{")) { block(); return; }
        if (accept(";")) return;
        if (at(T::Ident)) {
            const std::string& w = cur().text;
            if (peek().t == T::Op && peek().text == ":" && w != "default" && !isTypeWord(w, d_)) {   // etichetta:
                labels_[w] = here();
                p_ += 2;
                if (isOp("}")) return;
                statement();
                return;
            }
            if (w == "if") { ifStatement(); return; }
            if (w == "while") { whileStatement(); return; }
            if (w == "do") { doStatement(); return; }
            if (w == "for") { forStatement(); return; }
            if (w == "foreach") error("foreach non supportato: usa for");
            if (w == "switch") { switchStatement(); return; }
            if (w == "break") {
                ++p_;
                if (loops_.empty()) error("break fuori da un ciclo o da uno switch");
                loops_.back().breaks.push_back(emit(Op::Jmp, 0, -1));
                expect(";");
                return;
            }
            if (w == "continue") {
                ++p_;
                auto it = std::find_if(loops_.rbegin(), loops_.rend(), [](const Loop& l) { return !l.isSwitch; });
                if (it == loops_.rend()) error("continue fuori da un ciclo");
                it->continues.push_back(emit(Op::Jmp, 0, -1));
                expect(";");
                return;
            }
            if (w == "return") {
                ++p_;
                const Function& f = prog_->functions[static_cast<std::size_t>(fn_)];
                if (isOp(";")) {
                    if (f.ret != Type::Void) error("return senza valore in una funzione che restituisce un valore");
                    emit(Op::Ret, 0, 0);
                } else {
                    if (f.ret == Type::Void) error("return con un valore in una funzione void");
                    expression();
                    if (f.ret != Type::Auto && f.ret != Type::Array) emit(Op::Conv, static_cast<std::uint8_t>(f.ret), 0);
                    emit(Op::Ret, 1, 0);
                }
                expect(";");
                return;
            }
            if (w == "goto") {
                ++p_;
                const Tok at = cur();
                const std::string label = ident();
                gotos_.push_back({emit(Op::Jmp, 0, -1), {label, {at.line, at.col}}});
                expect(";");
                return;
            }
            if (w == "case" || w == "default") error("'" + w + "' fuori da uno switch");
            if ((d_ == Dialect::Cpp && (w == "cout" || w == "cerr" || w == "cin" || w == "getline")) ||
                (d_ == Dialect::Cpp && w == "std" && peek().t == T::Op && peek().text == "::" && peek(2).t == T::Ident &&
                 (peek(2).text == "cout" || peek(2).text == "cerr" || peek(2).text == "cin" || peek(2).text == "getline"))) {
                streamStatement();
                return;
            }
            if (d_ == Dialect::C && w == "scanf") { scanfStatement(); return; }
            if (d_ == Dialect::C && (w == "strcpy" || w == "strcat")) { stringCopyStatement(w == "strcat"); return; }
            if (startsType() && !(peek().t == T::Op && (peek().text == "(" || peek().text == "."))) {
                const TypeSpec ts = typeSpec();
                const Tok nameTok = cur();
                const std::string name = ident();
                declarators(ts, name, nameTok, true);
                return;
            }
        }
        expression();
        emit(Op::Pop, 0, 0);
        expect(";");
    }
    void condition() {
        expect("(");
        expression();
        emit(Op::Conv, static_cast<std::uint8_t>(Type::Bool), 0);
        expect(")");
    }
    void ifStatement() {
        ++p_;
        condition();
        const std::int32_t jz = emit(Op::Jz, 0, -1);
        statement();
        if (acceptWord("else")) {
            const std::int32_t jmp = emit(Op::Jmp, 0, -1);
            patch(jz, here());
            statement();
            patch(jmp, here());
        } else {
            patch(jz, here());
        }
    }
    void closeLoop(std::int32_t continueTarget, std::int32_t breakTarget) {
        Loop& l = loops_.back();
        for (std::int32_t j : l.continues) patch(j, continueTarget);
        for (std::int32_t j : l.breaks) patch(j, breakTarget);
        loops_.pop_back();
    }
    void whileStatement() {
        ++p_;
        const std::int32_t top = here();
        condition();
        const std::int32_t jz = emit(Op::Jz, 0, -1);
        loops_.emplace_back();
        statement();
        emit(Op::Jmp, 0, top);
        patch(jz, here());
        closeLoop(top, here());
    }
    void doStatement() {
        ++p_;
        const std::int32_t top = here();
        loops_.emplace_back();
        statement();
        const std::int32_t cont = here();
        if (!acceptWord("while")) error("atteso while dopo il corpo di do");
        condition();
        emit(Op::Jnz, 0, top);
        expect(";");
        closeLoop(cont, here());
    }
    void forStatement() {
        ++p_;
        expect("(");
        scopes_.emplace_back();
        if (!accept(";")) {
            if (startsType()) {
                const TypeSpec ts = typeSpec();
                const Tok nameTok = cur();
                const std::string name = ident();
                declarators(ts, name, nameTok, false);
            } else {
                expression(); emit(Op::Pop, 0, 0);
                while (accept(",")) { expression(); emit(Op::Pop, 0, 0); }
            }
            expect(";");
        }
        const std::int32_t top = here();
        std::int32_t jz = -1;
        if (!isOp(";")) {
            expression();
            emit(Op::Conv, static_cast<std::uint8_t>(Type::Bool), 0);
            jz = emit(Op::Jz, 0, -1);
        }
        expect(";");
        // il passo si compila a parte e va dopo il corpo
        std::vector<Instr> stepCode;
        std::vector<Where> stepWhere;
        if (!isOp(")")) {
            std::vector<Instr>* o = out_;
            std::vector<Where>* w = where_;
            out_ = &stepCode; where_ = &stepWhere;
            try {
                expression(); emit(Op::Pop, 0, 0);
                while (accept(",")) { expression(); emit(Op::Pop, 0, 0); }
            } catch (...) { out_ = o; where_ = w; throw; }
            out_ = o; where_ = w;
        }
        expect(")");
        loops_.emplace_back();
        statement();
        const std::int32_t cont = here();
        const std::int32_t base = here();
        for (std::size_t i = 0; i < stepCode.size(); ++i) {
            Instr ins = stepCode[i];
            if (ins.op == Op::Jmp || ins.op == Op::Jz || ins.op == Op::Jnz || ins.op == Op::JzKeep || ins.op == Op::JnzKeep) ins.a += base;
            out_->push_back(ins);
            where_->push_back(stepWhere[i]);
        }
        emit(Op::Jmp, 0, top);
        if (jz >= 0) patch(jz, here());
        closeLoop(cont, here());
        scopes_.pop_back();
    }
    std::int64_t caseConstant() {
        bool neg = accept("-");
        std::int64_t v = 0;
        if (at(T::Int) || at(T::Char)) v = t_[p_++].i;
        else if (at(T::Str) && d_ == Dialect::CSharp) error("case con stringhe non supportato");
        else error("case: atteso un intero o un carattere costante");
        return neg ? -v : v;
    }
    void switchStatement() {
        ++p_;
        expect("(");
        expression();
        emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
        expect(")");
        const std::int32_t table = static_cast<std::int32_t>(prog_->switches.size());
        prog_->switches.emplace_back();
        emit(Op::Switch, 0, table);
        expect("{");
        loops_.emplace_back();
        loops_.back().isSwitch = true;
        scopes_.emplace_back();
        bool hasDefault = false;
        while (!isOp("}")) {
            if (at(T::End)) error("manca '}' alla fine dello switch");
            if (acceptWord("case")) {
                const std::int64_t v = caseConstant();
                expect(":");
                for (const auto& c : prog_->switches[static_cast<std::size_t>(table)].cases) if (c.first == v) error("case ripetuto");
                prog_->switches[static_cast<std::size_t>(table)].cases.emplace_back(v, here());
                continue;
            }
            if (acceptWord("default")) {
                expect(":");
                if (hasDefault) error("default ripetuto");
                hasDefault = true;
                prog_->switches[static_cast<std::size_t>(table)].fallback = here();
                continue;
            }
            statement();
        }
        ++p_;
        scopes_.pop_back();
        if (!hasDefault) prog_->switches[static_cast<std::size_t>(table)].fallback = here();
        closeLoop(here(), here());
    }
    // C++: cout << a << endl;  cin >> x >> y;  getline(cin, s);
    void streamStatement() {
        if (acceptWord("std")) expect("::");
        const std::string w = ident();
        if (w == "getline") {
            expect("(");
            if (acceptWord("std")) expect("::");
            if (!acceptWord("cin")) error("getline: atteso cin");
            expect(",");
            Expr e = postfix();
            if (e.k == Expr::Value) error("getline: atteso il nome di una stringa");
            emit(Op::ReadLine, 0, 0);
            assignTo(e, Type::String);
            expect(")");
            expect(";");
            return;
        }
        if (w == "cin") {
            while (accept(">>")) {
                Expr e = postfix();
                if (e.k == Expr::Value) error("cin >>: atteso il nome di una variabile");
                const Type t = targetType(e);
                emit(t == Type::Char ? Op::ReadChar : Op::ReadTok, 0, 0);
                assignTo(e, t);
            }
            expect(";");
            return;
        }
        // cout / cerr
        bool any = false;
        while (accept("<<")) {
            any = true;
            if (isWord("endl") || (isWord("std") && peek(2).t == T::Ident && peek(2).text == "endl")) {
                if (acceptWord("std")) expect("::");
                ++p_;
                pushString("\n");
            } else {
                binary(precedence("<<") + 1);
            }
            builtin(Fn::Print, 1);
            emit(Op::Pop, 0, 0);
        }
        if (!any) error("cout: atteso <<");
        expect(";");
    }
    // C: scanf("%d %lf", &a, &b);
    void scanfStatement() {
        ++p_;
        expect("(");
        if (!at(T::Str)) error("scanf: attesa la stringa di formato");
        const std::string fmt = t_[p_++].text;
        std::vector<char> convs;
        for (std::size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%') continue;
            ++i;
            if (i < fmt.size() && fmt[i] == '%') continue;
            while (i < fmt.size() && (std::isdigit(static_cast<unsigned char>(fmt[i])) || fmt[i] == 'l' || fmt[i] == 'h')) ++i;
            if (i < fmt.size()) convs.push_back(fmt[i]);
        }
        std::size_t n = 0;
        while (accept(",")) {
            accept("&");
            Expr e = postfix();
            if (e.k == Expr::Value) error("scanf: atteso &variabile");
            const char conv = n < convs.size() ? convs[n] : 'd';
            const Type t = conv == 'c' ? Type::Char : conv == 's' ? Type::String : (conv == 'f' || conv == 'e' || conv == 'g') ? Type::Double : Type::Int;
            emit(conv == 'c' ? Op::ReadChar : Op::ReadTok, 0, 0);
            assignTo(e, t);
            ++n;
        }
        if (n != convs.size()) error("scanf: il numero degli argomenti non corrisponde al formato");
        expect(")");
        expect(";");
    }
    // C: strcpy(s, t) e strcat(s, t) cambiano la stringa s
    void stringCopyStatement(bool append) {
        ++p_;
        expect("(");
        Expr dst = postfix();
        if (dst.k != Expr::Variable) error(append ? "strcat: atteso il nome di una stringa" : "strcpy: atteso il nome di una stringa");
        expect(",");
        if (append) { Expr copy = dst; discharge(copy); }
        expression();
        if (append) emit(Op::Add, 0, 0);
        assignTo(dst, Type::String);
        expect(")");
        expect(";");
    }
    Type targetType(const Expr& e) const {
        if (e.k == Expr::Variable) return e.v.type == Type::Array ? e.v.elem : e.v.type;
        return e.type;
    }
    // Il valore in cima alla pila diventa l'input letto (gia' sulla pila) convertito e memorizzato.
    void assignTo(const Expr& e, Type t) {
        if (t != Type::Auto) emit(Op::Conv, static_cast<std::uint8_t>(t), 0);
        switch (e.k) {
        case Expr::Variable: store(e.v); break;
        case Expr::Index1: emit(Op::StoreIdx, 0, 0); break;
        case Expr::Index2: emit(Op::StoreIdx2, 0, 0); break;
        default: error("assegnazione non valida");
        }
    }
    void store(const Var& v) { emit(v.global ? Op::StoreG : Op::StoreL, 0, v.slot); }
    void load(const Var& v) { emit(v.global ? Op::LoadG : Op::LoadL, 0, v.slot); }

    // ---- espressioni
    void discharge(Expr& e) {
        switch (e.k) {
        case Expr::Variable: load(e.v); e.type = e.v.type; break;
        case Expr::Index1: emit(Op::LoadIdx, 0, 0); break;
        case Expr::Index2: emit(Op::LoadIdx2, 0, 0); break;
        case Expr::Value: break;
        }
        e.k = Expr::Value;
    }
    void expression() {
        Expr e = assignment();
        discharge(e);
        // l'operatore virgola: (a, b) vale b
        while (isOp(",") && commaAllowed_ > 0) {
            ++p_;
            emit(Op::Pop, 0, 0);
            Expr f = assignment();
            discharge(f);
        }
    }
    int commaAllowed_{0};
    // Negli elenchi separati da virgole (argomenti, valori) la virgola non e' un operatore.
    struct NoComma {
        explicit NoComma(Compiler& c) : c_(c), saved_(c.commaAllowed_) { c_.commaAllowed_ = 0; }
        ~NoComma() { c_.commaAllowed_ = saved_; }
        Compiler& c_;
        int saved_;
    };
    static int precedence(const std::string& o) {
        static const std::pair<const char*, int> table[] = {
            {"??", 1}, {"||", 2}, {"&&", 3}, {"|", 4}, {"^", 5}, {"&", 6}, {"==", 7}, {"!=", 7}, {"<", 8}, {"<=", 8}, {">", 8}, {">=", 8},
            {"<<", 9}, {">>", 9}, {"+", 10}, {"-", 10}, {"*", 11}, {"/", 11}, {"%", 11}};
        for (const auto& e : table) if (o == e.first) return e.second;
        return -1;
    }
    Expr assignment() {
        Depth guard(*this);
        Expr lhs = ternary();
        static const char* const ops[] = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};
        for (const char* o : ops) {
            if (!isOp(o)) continue;
            if (lhs.k == Expr::Value) error("a sinistra di '" + std::string(o) + "' serve una variabile");
            ++p_;
            const std::string op = o;
            const Type t = targetType(lhs);
            if (op == "=") {
                Expr rhs = assignment();
                discharge(rhs);
                if (t != Type::Auto && lhs.k == Expr::Variable && lhs.v.type != Type::Array) emit(Op::Conv, static_cast<std::uint8_t>(t), 0);
                else if (t != Type::Auto && lhs.k != Expr::Variable) emit(Op::Conv, static_cast<std::uint8_t>(t), 0);
                if (lhs.k == Expr::Variable) { emit(Op::Dup, 0, 0); store(lhs.v); }
                else if (lhs.k == Expr::Index1) emit(Op::CompIdx, 0, static_cast<std::int32_t>(t));
                else emit(Op::CompIdx2, 0, static_cast<std::int32_t>(t));
            } else {
                const Op arith = arithmeticOf(op.substr(0, op.size() - 1));
                if (lhs.k == Expr::Variable) {
                    load(lhs.v);
                    Expr rhs = assignment();
                    discharge(rhs);
                    emit(arith, 0, 0);
                    if (t != Type::Auto) emit(Op::Conv, static_cast<std::uint8_t>(t), 0);
                    emit(Op::Dup, 0, 0);
                    store(lhs.v);
                } else {
                    Expr rhs = assignment();
                    discharge(rhs);
                    emit(lhs.k == Expr::Index1 ? Op::CompIdx : Op::CompIdx2, static_cast<std::uint8_t>(arith), static_cast<std::int32_t>(t));
                }
            }
            Expr r; r.type = t;
            return r;
        }
        return lhs;
    }
    static Op arithmeticOf(const std::string& o) {
        if (o == "+") return Op::Add;
        if (o == "-") return Op::Sub;
        if (o == "*") return Op::Mul;
        if (o == "/") return Op::Div;
        if (o == "%") return Op::Mod;
        if (o == "&") return Op::BitAnd;
        if (o == "|") return Op::BitOr;
        if (o == "^") return Op::BitXor;
        if (o == "<<") return Op::Shl;
        return Op::Shr;
    }
    Expr ternary() {
        Expr c = binary(1);
        if (!isOp("?")) return c;
        ++p_;
        discharge(c);
        emit(Op::Conv, static_cast<std::uint8_t>(Type::Bool), 0);
        const std::int32_t jz = emit(Op::Jz, 0, -1);
        Expr a = assignment();
        discharge(a);
        const std::int32_t jmp = emit(Op::Jmp, 0, -1);
        expect(":");
        patch(jz, here());
        Expr b = assignment();
        discharge(b);
        patch(jmp, here());
        return Expr{};
    }
    Expr binary(int minPrec) {
        Depth guard(*this);
        Expr lhs = unary();
        while (cur().t == T::Op) {
            const std::string o = cur().text;
            const int prec = precedence(o);
            if (prec < 0 || prec < minPrec) break;
            ++p_;
            discharge(lhs);
            if (o == "&&" || o == "||") {
                emit(Op::Conv, static_cast<std::uint8_t>(Type::Bool), 0);
                const std::int32_t j = emit(o == "&&" ? Op::JzKeep : Op::JnzKeep, 0, -1);
                emit(Op::Pop, 0, 0);
                Expr rhs = binary(prec + 1);
                discharge(rhs);
                emit(Op::Conv, static_cast<std::uint8_t>(Type::Bool), 0);
                patch(j, here());
                lhs = Expr{};
                lhs.type = Type::Bool;
                continue;
            }
            if (o == "??") error("operatore ?? non supportato");
            Expr rhs = binary(prec + 1);
            discharge(rhs);
            if (o == "==") emit(Op::Eq, 0, 0);
            else if (o == "!=") emit(Op::Ne, 0, 0);
            else if (o == "<") emit(Op::Lt, 0, 0);
            else if (o == "<=") emit(Op::Le, 0, 0);
            else if (o == ">") emit(Op::Gt, 0, 0);
            else if (o == ">=") emit(Op::Ge, 0, 0);
            else emit(arithmeticOf(o), 0, 0);
            lhs = Expr{};
        }
        return lhs;
    }
    Expr unary() {
        Depth guard(*this);
        if (isOp("-") || isOp("+") || isOp("!") || isOp("~")) {
            const std::string o = t_[p_++].text;
            Expr e = unary();
            discharge(e);
            if (o == "-") emit(Op::Neg, 0, 0);
            else if (o == "!") emit(Op::Not, 0, 0);
            else if (o == "~") emit(Op::BitNot, 0, 0);
            return Expr{};
        }
        if (isOp("++") || isOp("--")) {
            const bool dec = t_[p_++].text == "--";
            Expr e = unary();
            increment(e, false, dec);
            return Expr{};
        }
        if (isOp("&")) error("operatore & (indirizzo) non supportato fuori da scanf");
        if (isOp("*")) error("puntatori non supportati");
        // cast: (int) x, (double) x
        if (isOp("(") && peek().t == T::Ident && isTypeWord(peek().text, d_) && peek().text != "const" && peek().text != "static") {
            std::size_t q = p_ + 1;
            while (q < t_.size() && t_[q].t == T::Ident && isTypeWord(t_[q].text, d_)) ++q;
            if (q < t_.size() && t_[q].t == T::Op && t_[q].text == ")") {
                ++p_;
                const TypeSpec ts = typeSpec();
                expect(")");
                Expr e = unary();
                discharge(e);
                if (ts.base != Type::Auto && ts.base != Type::Void) emit(Op::Conv, static_cast<std::uint8_t>(ts.base), 0);
                Expr r; r.type = ts.base;
                return r;
            }
        }
        if (d_ == Dialect::Cpp && (isWord("static_cast") || isWord("int") || isWord("double")) && peek().t == T::Op && (peek().text == "<" || peek().text == "(")) {
            if (isWord("static_cast")) {
                ++p_;
                expect("<");
                const TypeSpec ts = typeSpec();
                expect(">");
                expect("(");
                expression();
                expect(")");
                if (ts.base != Type::Auto && ts.base != Type::Void) emit(Op::Conv, static_cast<std::uint8_t>(ts.base), 0);
                return Expr{};
            }
            const Type t = cur().text == "int" ? Type::Int : Type::Double;
            ++p_;
            expect("(");
            expression();
            expect(")");
            emit(Op::Conv, static_cast<std::uint8_t>(t), 0);
            return Expr{};
        }
        return postfix();
    }
    void increment(Expr& e, bool post, bool dec) {
        const std::uint8_t flags = static_cast<std::uint8_t>((post ? 1 : 0) | (dec ? 2 : 0));
        switch (e.k) {
        case Expr::Variable: emit(e.v.global ? Op::IncG : Op::IncL, flags, e.v.slot); break;
        case Expr::Index1: emit(Op::IncIdx, flags, 0); break;
        case Expr::Index2: emit(Op::IncIdx2, flags, 0); break;
        default: error(std::string(dec ? "--" : "++") + " vuole una variabile");
        }
        e.k = Expr::Value;
    }
    // Nome qualificato: std::sqrt, Math.Sqrt, Console.WriteLine, fx::calc, Fx.Calc
    std::string qualifiedName() {
        std::string n = ident();
        static const char* const classes[] = {"Math", "Console", "Convert", "Fx", "int", "double", "string", "String"};
        if (n == "std" && isOp("::")) { ++p_; return ident(); }
        if (n == "fx" && isOp("::")) { ++p_; return "fx::" + ident(); }
        for (const char* c : classes) {
            if (n == c && isOp(".")) { ++p_; return n + "." + ident(); }
        }
        return n;
    }
    Expr postfix() {
        Depth guard(*this);
        Expr e = primary();
        while (true) {
            if (isOp("[")) {
                if (e.k == Expr::Variable && e.v.type == Type::Array && e.v.dims == 2) {
                    discharge(e);
                    ++p_;
                    expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
                    if (d_ == Dialect::CSharp && accept(",")) {
                        expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
                        expect("]");
                    } else {
                        expect("]");
                        expect("[");
                        expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
                        expect("]");
                    }
                    Expr r; r.k = Expr::Index2; r.type = e.v.elem;
                    e = r;
                    continue;
                }
                const Type elem = e.k == Expr::Variable ? (e.v.type == Type::Array ? e.v.elem : (e.v.type == Type::String ? Type::Char : Type::Auto)) : Type::Auto;
                discharge(e);
                ++p_;
                expression(); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0);
                expect("]");
                Expr r; r.k = Expr::Index1; r.type = elem;
                e = r;
                continue;
            }
            if (isOp("++") || isOp("--")) {
                const bool dec = t_[p_++].text == "--";
                increment(e, true, dec);
                continue;
            }
            if (isOp(".") && peek().t == T::Ident) {
                ++p_;
                const std::string m = ident();
                discharge(e);
                memberCall(m);
                e = Expr{};
                continue;
            }
            break;
        }
        return e;
    }
    void memberCall(const std::string& m) {
        // stringa.size() / length() / Length / substr / Substring / ToString() / ToUpper() / IndexOf / find
        auto args = [&](int lo, int hi) {
            NoComma nc(*this);
            int n = 0;
            if (accept("(")) {
                while (!isOp(")")) { expression(); ++n; if (!accept(",")) break; }
                expect(")");
            }
            if (n < lo || n > hi) error("argomenti non validi per ." + m);
            return n;
        };
        if (m == "size" || m == "length" || m == "Length" || m == "Count") { if (m != "Length" && m != "Count") args(0, 0); builtin(Fn::Strlen, 1); return; }
        if (m == "substr" || m == "Substring") { const int n = args(1, 2); builtin(Fn::Substr, 1 + n); return; }
        if (m == "ToString") { args(0, 1); builtin(Fn::ToStr, 1); return; }
        if (m == "ToUpper") { args(0, 0); builtin(Fn::ToUpper, 1); return; }
        if (m == "ToLower") { args(0, 0); builtin(Fn::ToLower, 1); return; }
        if (m == "IndexOf" || m == "find") { args(1, 1); builtin(Fn::IndexOf, 2); return; }
        if (m == "at") { args(1, 1); emit(Op::LoadIdx, 0, 0); return; }
        if (m == "c_str") { args(0, 0); return; }
        error("metodo '." + m + "' non supportato");
    }
    Expr primary() {
        const Tok& t = cur();
        switch (t.t) {
        case T::Int: ++p_; emit(Op::PushI, 0, constInt(t.i)); return Expr{};
        case T::Float: ++p_; emit(Op::PushD, 0, constDouble(t.d)); return Expr{};
        case T::Char: ++p_; emit(Op::PushI, 0, constInt(t.i)); emit(Op::Conv, static_cast<std::uint8_t>(Type::Char), 0); return Expr{};
        case T::Str: {
            std::string s;
            while (at(T::Str)) s += t_[p_++].text;               // "a" "b" = "ab"
            pushString(s);
            Expr r; r.type = Type::String;
            return r;
        }
        case T::InterpStart: return interpolation();
        case T::Op:
            if (accept("(")) {
                ++commaAllowed_;
                expression();
                --commaAllowed_;
                expect(")");
                return Expr{};
            }
            error("espressione non valida prima di '" + t.text + "'");
        case T::Ident: break;
        default: error("espressione non valida");
        }
        const std::string w = t.text;
        if (w == "true" || w == "false") { ++p_; emit(Op::PushB, w == "true" ? 1 : 0, 0); return Expr{}; }
        if (w == "NULL" || w == "nullptr" || w == "null") error("puntatori nulli non supportati");
        if (w == "M_PI" || w == "RAND_MAX") { ++p_; if (w == "M_PI") emit(Op::PushD, 0, constDouble(3.14159265358979323846)); else emit(Op::PushI, 0, constInt(32767)); return Expr{}; }
        if (w == "Math" && peek().t == T::Op && peek().text == "." && (peek(2).text == "PI" || peek(2).text == "E")) {
            p_ += 2;
            const std::string c = ident();
            emit(Op::PushD, 0, constDouble(c == "PI" ? 3.14159265358979323846 : 2.71828182845904523536));
            return Expr{};
        }
        if (w == "Console" && peek().t == T::Op && peek().text == "." && peek(2).text == "ReadLine") {
            p_ += 3;
            expect("(");
            expect(")");
            emit(Op::ReadLine, 0, 0);
            Expr r; r.type = Type::String;
            return r;
        }
        if (d_ == Dialect::C && w == "getchar") { ++p_; expect("("); expect(")"); emit(Op::ReadChar, 0, 0); emit(Op::Conv, static_cast<std::uint8_t>(Type::Int), 0); return Expr{}; }
        if (w == "sizeof") error("sizeof non supportato");
        const std::size_t save = p_;
        const std::string name = qualifiedName();
        if (isOp("(")) { call(name, t); Expr r; return r; }
        if (name != w || name.find("::") != std::string::npos || name.find('.') != std::string::npos) {
            p_ = save;
            error("nome '" + w + "' non valido qui");
        }
        Var v;
        if (!lookup(name, v)) fail("variabile '" + name + "' non dichiarata", t.line, t.col);
        Expr e; e.k = Expr::Variable; e.v = v; e.type = v.type;
        return e;
    }
    Expr interpolation() {
        ++p_;                                                      // InterpStart
        pushString("");
        while (!at(T::InterpEnd)) {
            if (at(T::Str)) { pushString(t_[p_++].text); emit(Op::Add, 0, 0); continue; }
            if (at(T::ExprStart)) {
                const std::string format = t_[p_++].text;
                expression();
                if (!format.empty()) {
                    pushString(format);
                    builtin(Fn::Format, 2);
                } else {
                    builtin(Fn::ToStr, 1);
                }
                if (!at(T::ExprEnd)) error("espressione non valida nella stringa interpolata");
                ++p_;
                emit(Op::Add, 0, 0);
                continue;
            }
            error("stringa interpolata non valida");
        }
        ++p_;
        Expr r; r.type = Type::String;
        return r;
    }
    void call(const std::string& name, const Tok& at) {
        NoComma nc(*this);
        expect("(");
        int argc = 0;
        while (!isOp(")")) {
            expression();
            ++argc;
            if (argc > 32) error("troppi argomenti");
            if (!accept(",")) break;
        }
        expect(")");
        const std::int32_t f = findFunction(name);
        if (f >= 0) {
            const Function& fn = prog_->functions[static_cast<std::size_t>(f)];
            if (static_cast<std::size_t>(argc) != fn.params.size())
                fail("'" + name + "' vuole " + std::to_string(fn.params.size()) + " argomenti", at.line, at.col);
            emit(Op::Call, static_cast<std::uint8_t>(argc), f);
            return;
        }
        if (const Builtin* b = findBuiltin(name)) {
            if (argc < b->minArgs || argc > b->maxArgs) fail("numero di argomenti non valido per " + name, at.line, at.col);
            builtin(b->fn, argc);
            return;
        }
        // funzione definita piu' avanti: si registra (la definizione deve avere gli stessi argomenti)
        Function fn;
        fn.name = name; fn.line = at.line; fn.column = at.col;
        fn.params.assign(static_cast<std::size_t>(argc), Type::Auto);
        prog_->functions.push_back(fn);
        emit(Op::Call, static_cast<std::uint8_t>(argc), static_cast<std::int32_t>(prog_->functions.size() - 1));
        pendingCalls_.emplace_back(static_cast<std::int32_t>(prog_->functions.size() - 1), argc);
    }

    std::vector<Tok> t_;
    Dialect d_;
    std::size_t p_{0};
    int depth_{0};
    std::shared_ptr<Program> prog_;
    std::vector<Instr>* out_;
    std::vector<Where>* where_;
    std::vector<Instr> init_;
    std::vector<Where> initWhere_;
    std::map<std::string, Var> globals_;
    std::vector<std::map<std::string, Var>> scopes_;
    std::int32_t fn_{-1};
    std::map<std::string, std::int32_t> labels_;
    std::vector<std::pair<std::int32_t, std::pair<std::string, std::pair<int, int>>>> gotos_;
    std::vector<std::pair<std::int32_t, int>> pendingCalls_;
};

} // namespace

Dialect dialectOf(const std::string& fileName) noexcept {
    auto ends = [&](const char* ext) {
        const std::size_t n = std::strlen(ext);
        if (fileName.size() < n) return false;
        for (std::size_t i = 0; i < n; ++i)
            if (std::tolower(static_cast<unsigned char>(fileName[fileName.size() - n + i])) != ext[i]) return false;
        return true;
    };
    if (ends(".cs")) return Dialect::CSharp;
    if (ends(".c")) return Dialect::C;
    return Dialect::Cpp;
}
const char* dialectName(Dialect d) noexcept {
    switch (d) {
    case Dialect::C: return "C";
    case Dialect::Cpp: return "C++";
    case Dialect::CSharp: return "C#";
    }
    return "?";
}

std::shared_ptr<const Program> compile(const std::string& source, Dialect dialect) {
    if (source.size() > MaxSourceBytes) throw Error("Memory ERROR: sorgente troppo lungo", 1, 1);
    SourceLexer lexer(source, dialect);
    Compiler compiler(lexer.run(), dialect);
    return compiler.run();
}
std::size_t codeSize(const Program& program) noexcept { return program.code.size(); }

} // namespace fx::prog
