// Lexer comune e preprocessore di C/C++ (v1.20.0): vedi program_ast.hpp.
//
// Il lexer riconosce i letterali di tutti i dialetti (separatori di cifre di
// C++14/C#/Java, stringhe verbatim e interpolate di C#, caratteri UTF-16) e
// conta le colonne in caratteri UTF-8. Il preprocessore lavora sui token:
// #include (progetto, librerie e intestazioni standard virtuali), #define con e
// senza parametri, #undef, #if/#ifdef/#ifndef/#elif/#else/#endif con
// defined(), #pragma once, #error; # e ## e __VA_ARGS__ danno un errore chiaro.
#include "program_ast.hpp"
#include "program_natives.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

namespace fx::prog::ast {

Sym Symbols::intern(const std::string& s) {
    const auto it = map_.find(s);
    if (it != map_.end()) return it->second;
    const Sym id = static_cast<Sym>(names_.size());
    names_.push_back(s);
    map_.emplace(s, id);
    return id;
}

namespace {
const std::vector<std::string>* g_failFiles = nullptr;
} // namespace

void setFailFiles(const std::vector<std::string>* files) { g_failFiles = files; }

[[noreturn]] void fail(const std::string& msg, Pos pos, const std::vector<std::string>* files) {
    const std::vector<std::string>* f = files ? files : g_failFiles;
    std::string name;
    if (f && pos.file < f->size()) name = (*f)[pos.file];
    const bool tagged = msg.find("ERROR") != std::string::npos || msg.find("Exception") != std::string::npos || msg.find("Break") == 0;
    throw Error(tagged ? msg : "Syntax ERROR: " + msg, static_cast<int>(pos.line), pos.col, name);
}

const char* punctText(P p) noexcept {
    static const char* const t[PCount] = {"", "(", ")", "{", "}", "[", "]", ";", ",", ".", "->", "::", "?", ":",
        "+", "-", "*", "/", "%", "&", "|", "^", "~", "!", "=", "<", ">", "<=", ">=", "==", "!=", "&&", "||",
        "++", "--", "<<", ">>", ">>>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", ">>>=", "...", "#", "##",
        "??", "?\?=", "?.", "=>", "@", "->*", ".*"};
    return p < PCount ? t[p] : "";
}

namespace {

struct PunctDef { const char* text; P p; std::uint8_t dialects; };   // bit d = disponibile nel dialetto d
constexpr std::uint8_t AllD = 0x0F, CppD = 0x02, CD = 0x03, CsD = 0x04, JavaD = 0x08, CsJ = 0x0C;
const PunctDef kPuncts[] = {
    {">>>=", UShrEq, JavaD}, {"<<=", ShlEq, AllD}, {">>=", ShrEq, AllD}, {">>>", UShr, JavaD}, {"...", Ellipsis, AllD}, {"->*", ArrowStar, CppD},
    {"?\?=", QQEq, CsD}, {"::", Scope, static_cast<std::uint8_t>(CD | CsD | JavaD)}, {"->", Arrow, static_cast<std::uint8_t>(CD | JavaD)}, {"++", PlusPlus, AllD}, {"--", MinusMinus, AllD},
    {"<<", Shl, AllD}, {">>", Shr, AllD}, {"<=", Le, AllD}, {">=", Ge, AllD}, {"==", EqEq, AllD}, {"!=", Ne, AllD},
    {"&&", AndAnd, AllD}, {"||", OrOr, AllD}, {"+=", PlusEq, AllD}, {"-=", MinusEq, AllD}, {"*=", StarEq, AllD},
    {"/=", SlashEq, AllD}, {"%=", PercentEq, AllD}, {"&=", AmpEq, AllD}, {"|=", PipeEq, AllD}, {"^=", CaretEq, AllD},
    {"##", HashHash, CD}, {"??", QQ, CsD}, {"?.", QDot, CsD}, {"=>", FatArrow, CsD}, {".*", DotStar, CppD},
};
P singlePunct(char c) noexcept {
    switch (c) {
    case '(': return LParen; case ')': return RParen; case '{': return LBrace; case '}': return RBrace;
    case '[': return LBracket; case ']': return RBracket; case ';': return Semi; case ',': return Comma;
    case '.': return Dot; case '?': return Question; case ':': return Colon; case '+': return Plus; case '-': return Minus;
    case '*': return Star; case '/': return Slash; case '%': return Percent; case '&': return Amp; case '|': return Pipe;
    case '^': return Caret; case '~': return Tilde; case '!': return Bang; case '=': return Assign; case '<': return Lt;
    case '>': return Gt; case '#': return Hash; case '@': return At;
    default: return PNone;
    }
}

class SourceLexer {
public:
    SourceLexer(const std::string& s, const LexOptions& o, Symbols& syms, Lexed& out) : s_(s), o_(o), syms_(syms), out_(out) {}
    void run() {
        bool lineStart = true, space = false;
        // BOM UTF-8 dei file del PC
        if (s_.size() >= 3 && static_cast<unsigned char>(s_[0]) == 0xEF && static_cast<unsigned char>(s_[1]) == 0xBB && static_cast<unsigned char>(s_[2]) == 0xBF) p_ = 3;
        while (true) {
            space = skipSpace(lineStart) || space;
            if (p_ >= s_.size()) break;
            const char c = s_[p_];
            Token t;
            t.pos = here();
            t.lineStart = lineStart;
            t.spaceBefore = space;
            lineStart = false;
            space = false;
            if (o_.dialect == Dialect::CSharp && c == '#' && t.lineStart) { csDirective(t.pos); lineStart = true; continue; }
            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && p_ + 1 < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_ + 1])))) {
                number(t);
            } else if (identStart(c)) {
                // prefissi delle stringhe: L"", u8"", u"", U"" (C/C++), R"()" (C++)
                if ((c == 'L' || c == 'u' || c == 'U' || c == 'R') && isCFamily()) {
                    std::size_t q = p_ + 1;
                    if (c == 'u' && q < s_.size() && s_[q] == '8') ++q;
                    if (q < s_.size() && s_[q] == 'R' && c != 'R') ++q;
                    if (q < s_.size() && (s_[q] == '"' || (s_[q] == '\'' && c != 'R'))) {
                        if (s_[q - 1] == 'R' || c == 'R') fail("stringhe grezze R\"(...)\" non supportate", t.pos);
                        const bool u8 = c == 'u' && s_[p_ + 1] == '8';
                        if (!u8) fail(std::string("prefisso ") + c + " dei letterali larghi non supportato", t.pos);
                        while (p_ < q) take();
                        if (s_[p_] == '"') stringLit(t); else charLit(t);
                        out_.tokens.push_back(t);
                        continue;
                    }
                }
                ident(t);
            } else if (c == '"') {
                stringLit(t);
            } else if (c == '\'') {
                charLit(t);
            } else if (c == '@' && o_.dialect == Dialect::CSharp && p_ + 1 < s_.size() && s_[p_ + 1] == '"') {
                take();
                verbatim(t);
            } else if (c == '@' && o_.dialect == Dialect::CSharp && p_ + 1 < s_.size() && identStart(s_[p_ + 1])) {
                take();                                  // @nome: identificatore con il nome di una parola chiave
                ident(t);
            } else if (c == '$' && o_.dialect == Dialect::CSharp && p_ + 1 < s_.size() && (s_[p_ + 1] == '"' || (s_[p_ + 1] == '@' && p_ + 2 < s_.size() && s_[p_ + 2] == '"'))) {
                interpolated(t);
            } else if (c == '@' && o_.dialect == Dialect::CSharp && p_ + 2 < s_.size() && s_[p_ + 1] == '$' && s_[p_ + 2] == '"') {
                take();
                interpolated(t, true);
            } else if (c == '"' ) {
                stringLit(t);
            } else {
                punct(t);
            }
            out_.tokens.push_back(t);
            if (out_.tokens.size() > 4000000) fail("sorgente troppo lungo", t.pos);
        }
        Token e;
        e.t = T::End;
        e.pos = here();
        e.lineStart = true;
        out_.tokens.push_back(e);
        if (o_.dialect == Dialect::CSharp && !cs_.empty()) fail("#if senza #endif", cs_.back().pos);
    }

private:
    bool isCFamily() const noexcept { return o_.dialect == Dialect::C || o_.dialect == Dialect::Cpp; }
    static bool identStart(char c) noexcept { return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || static_cast<unsigned char>(c) >= 0x80; }
    bool identChar(char c) const noexcept {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || static_cast<unsigned char>(c) >= 0x80 || (c == '$' && o_.dialect == Dialect::Java);
    }
    Pos here() const noexcept { Pos p; p.file = o_.file; p.line = line_; p.col = static_cast<std::uint16_t>(std::min<std::uint32_t>(col_, 65535)); return p; }
    char take() {
        const char c = s_[p_++];
        if (c == '\n') { ++line_; col_ = 1; }
        else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++col_;
        return c;
    }
    char peekc(std::size_t k = 0) const noexcept { return p_ + k < s_.size() ? s_[p_ + k] : '\0'; }
    // Spazi, commenti e continuazioni di riga; true se ne ha saltati.
    bool skipSpace(bool& lineStart) {
        bool any = false;
        while (p_ < s_.size()) {
            const char c = s_[p_];
            if (c == '\n') { take(); lineStart = true; any = true; continue; }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { take(); any = true; continue; }
            if (c == '\\' && isCFamily() && (peekc(1) == '\n' || (peekc(1) == '\r' && peekc(2) == '\n'))) {
                take(); if (peekc() == '\r') take(); take(); any = true; continue;
            }
            if (c == '/' && peekc(1) == '/') {
                while (p_ < s_.size() && s_[p_] != '\n') {
                    if (s_[p_] == '\\' && isCFamily() && peekc(1) == '\n') take();
                    take();
                }
                any = true;
                continue;
            }
            if (c == '/' && peekc(1) == '*') {
                const Pos start = here();
                take(); take();
                while (p_ < s_.size() && !(s_[p_] == '*' && peekc(1) == '/')) take();
                if (p_ >= s_.size()) fail("commento /* non chiuso", start);
                take(); take();
                any = true;
                continue;
            }
            break;
        }
        return any;
    }
    void ident(Token& t) {
        const std::size_t b = p_;
        while (p_ < s_.size() && identChar(s_[p_])) take();
        t.t = T::Ident;
        t.sym = syms_.intern(s_.substr(b, p_ - b));
        if (p_ - b > 255) fail("nome troppo lungo", t.pos);
    }
    bool digitSep(char c) const noexcept {
        if (c == '\'' && o_.dialect == Dialect::Cpp) return true;
        if (c == '_' && (o_.dialect == Dialect::CSharp || o_.dialect == Dialect::Java)) return true;
        return false;
    }
    void number(Token& t) {
        std::string digits;
        bool real = false;
        int base = 10;
        auto isDigitOf = [](char c, int b) {
            if (b == 16) return std::isxdigit(static_cast<unsigned char>(c)) != 0;
            if (b == 2) return c == '0' || c == '1';
            if (b == 8) return c >= '0' && c <= '7';
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        };
        if (peekc() == '0' && (peekc(1) == 'x' || peekc(1) == 'X')) { take(); take(); base = 16; }
        else if (peekc() == '0' && (peekc(1) == 'b' || peekc(1) == 'B') && (peekc(2) == '0' || peekc(2) == '1')) { take(); take(); base = 2; }
        if (base != 10) {
            while (p_ < s_.size() && (isDigitOf(s_[p_], base) || (digitSep(s_[p_]) && isDigitOf(peekc(1), base)))) { const char c = take(); if (!digitSep(c)) digits += c; }
            if (digits.empty()) fail("numero senza cifre", t.pos);
        } else {
            while (p_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[p_])) || (digitSep(s_[p_]) && std::isdigit(static_cast<unsigned char>(peekc(1)))))) { const char c = take(); if (!digitSep(c)) digits += c; }
            if (peekc() == '.' && std::isdigit(static_cast<unsigned char>(peekc(1)))) {
                real = true; digits += take();
                while (p_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[p_])) || (digitSep(s_[p_]) && std::isdigit(static_cast<unsigned char>(peekc(1)))))) { const char c = take(); if (!digitSep(c)) digits += c; }
            } else if (peekc() == '.' && !identStart(peekc(1)) && peekc(1) != '.') {
                real = true; digits += take();                   // "1." (C/C++/Java)
            }
            if (peekc() == 'e' || peekc() == 'E') {
                std::size_t q = 1;
                if (peekc(q) == '+' || peekc(q) == '-') ++q;
                if (std::isdigit(static_cast<unsigned char>(peekc(q)))) {
                    real = true;
                    digits += take();
                    if (peekc() == '+' || peekc() == '-') digits += take();
                    while (p_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[p_]))) digits += take();
                }
            }
        }
        // suffissi
        std::uint8_t flags = 0;
        bool floatSuffix = false, doubleSuffix = false, decimalSuffix = false;
        while (p_ < s_.size() && std::strchr("uUlLfFdDmM", s_[p_])) {
            const char x = take();
            if (x == 'u' || x == 'U') flags |= 1;
            else if (x == 'l' || x == 'L') { if (flags & 2) { flags = static_cast<std::uint8_t>(flags | 4); } flags |= 2; }
            else if (x == 'f' || x == 'F') { if (base != 10) fail("suffisso f su un numero non decimale", t.pos); floatSuffix = true; }
            else if (x == 'd' || x == 'D') { if (base != 10) fail("suffisso d su un numero non decimale", t.pos); doubleSuffix = true; }
            else decimalSuffix = true;
        }
        if (p_ < s_.size() && identChar(s_[p_])) fail("numero seguito da lettere", here());
        if (decimalSuffix) fail("decimal non supportato: usa double", t.pos);
        if (floatSuffix || doubleSuffix) real = true;
        if (real) {
            if (base != 10) fail("numero reale non decimale non supportato", t.pos);
            t.t = T::Float;
            t.d = std::strtod(digits.c_str(), nullptr);
            t.flags = floatSuffix ? 1 : 0;
            return;
        }
        t.t = T::Int;
        t.flags = flags;
        // ottale del C/C++/Java: 0 seguito da cifre
        if (base == 10 && digits.size() > 1 && digits[0] == '0' && o_.dialect != Dialect::CSharp) {
            base = 8;
            for (char c : digits) if (c > '7') fail("cifra non ottale in un numero che inizia con 0", t.pos);
        }
        errno = 0;
        const unsigned long long v = std::strtoull(digits.c_str(), nullptr, base);
        if (errno == ERANGE) fail("intero troppo grande", t.pos);
        t.i = static_cast<std::int64_t>(v);
    }
    // Una sequenza di escape (dopo '\'): byte UTF-8 aggiunti a `out`.
    void escape(std::string& out, bool wideChars) {
        const Pos at = here();
        const char e = take();
        switch (e) {
        case 'n': out += '\n'; return;
        case 't': out += '\t'; return;
        case 'r': out += '\r'; return;
        case '0':
            if (isCFamily() && std::isdigit(static_cast<unsigned char>(peekc()))) break;   // ottale \0nn
            out += '\0'; return;
        case 'a': out += '\a'; return;
        case 'b': out += '\b'; return;
        case 'f': out += '\f'; return;
        case 'v': out += '\v'; return;
        case '\\': out += '\\'; return;
        case '\'': out += '\''; return;
        case '"': out += '"'; return;
        case '?': out += '?'; return;
        case 's': if (o_.dialect == Dialect::Java) { out += ' '; return; } break;
        case 'x': {
            unsigned v = 0;
            int n = 0;
            while (std::isxdigit(static_cast<unsigned char>(peekc())) && n < (wideChars ? 4 : 2)) {
                const char h = take();
                v = v * 16 + static_cast<unsigned>(std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : std::tolower(static_cast<unsigned char>(h)) - 'a' + 10);
                ++n;
            }
            if (n == 0) fail("\\x senza cifre esadecimali", at);
            if (wideChars) appendUtf8(out, v); else out += static_cast<char>(v);
            return;
        }
        case 'u': case 'U': {
            if (isCFamily() && !wideChars && false) break;
            const int want = e == 'u' ? 4 : 8;
            unsigned v = 0;
            for (int k = 0; k < want; ++k) {
                if (!std::isxdigit(static_cast<unsigned char>(peekc()))) fail("\\u vuole 4 cifre esadecimali", at);
                const char h = take();
                v = v * 16 + static_cast<unsigned>(std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : std::tolower(static_cast<unsigned char>(h)) - 'a' + 10);
            }
            appendUtf8(out, v);
            return;
        }
        default: break;
        }
        if (e >= '0' && e <= '7' && (isCFamily() || o_.dialect == Dialect::Java)) {
            unsigned v = static_cast<unsigned>(e - '0');
            int n = 1;
            while (n < 3 && peekc() >= '0' && peekc() <= '7') { v = v * 8 + static_cast<unsigned>(take() - '0'); ++n; }
            out += static_cast<char>(v & 0xFF);
            return;
        }
        fail(std::string("sequenza di escape \\") + e + " non valida", at);
    }
    static void appendUtf8(std::string& out, unsigned v) {
        if (v < 0x80) out += static_cast<char>(v);
        else if (v < 0x800) { out += static_cast<char>(0xC0 | (v >> 6)); out += static_cast<char>(0x80 | (v & 0x3F)); }
        else if (v < 0x10000) { out += static_cast<char>(0xE0 | (v >> 12)); out += static_cast<char>(0x80 | ((v >> 6) & 0x3F)); out += static_cast<char>(0x80 | (v & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (v >> 18)); out += static_cast<char>(0x80 | ((v >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((v >> 6) & 0x3F)); out += static_cast<char>(0x80 | (v & 0x3F)); }
    }
    std::uint32_t addString(std::string s) {
        out_.strings.push_back(std::move(s));
        return static_cast<std::uint32_t>(out_.strings.size() - 1);
    }
    void stringLit(Token& t) {
        take();
        std::string text;
        const bool wide = !isCFamily();
        while (true) {
            if (p_ >= s_.size() || s_[p_] == '\n') fail("stringa non chiusa", t.pos);
            const char c = take();
            if (c == '"') break;
            if (c == '\\') escape(text, wide);
            else text += c;
            if (text.size() > 65536) fail("stringa troppo lunga", t.pos);
        }
        t.t = T::Str;
        t.flags = wide ? 2 : 0;
        t.str = addString(std::move(text));
    }
    void verbatim(Token& t) {
        take();                                          // "
        std::string text;
        while (true) {
            if (p_ >= s_.size()) fail("stringa @\"...\" non chiusa", t.pos);
            const char c = take();
            if (c == '"') { if (peekc() == '"') { take(); text += '"'; continue; } break; }
            if (c == '\r') continue;
            text += c;
            if (text.size() > 65536) fail("stringa troppo lunga", t.pos);
        }
        t.t = T::Str;
        t.flags = 2 | 1;
        t.str = addString(std::move(text));
    }
    // $"..." e $@"...": il contenuto grezzo (escape risolti nelle parti di testo
    // dal parser); i {buchi} si analizzano con lex() nel parser.
    void interpolated(Token& t, bool verbatimFirst = false) {
        take();                                          // $
        bool verb = verbatimFirst;
        if (peekc() == '@') { take(); verb = true; }
        take();                                          // "
        std::string raw;
        int depth = 0;
        while (true) {
            if (p_ >= s_.size() || (!verb && s_[p_] == '\n')) fail("stringa interpolata non chiusa", t.pos);
            const char c = take();
            if (depth == 0 && c == '"') {
                if (verb && peekc() == '"') { take(); raw += "\"\""; continue; }
                break;
            }
            if (c == '{') { if (depth == 0 && peekc() == '{') { take(); raw += "{{"; continue; } ++depth; }
            else if (c == '}') { if (depth == 0) { if (peekc() == '}') { take(); raw += "}}"; continue; } fail("'}' senza '{' nella stringa interpolata", here()); } --depth; }
            else if (c == '\\' && !verb && depth == 0) { raw += c; raw += take(); continue; }
            else if (c == '"' && depth > 0) {                    // stringa dentro un buco
                raw += c;
                while (p_ < s_.size() && s_[p_] != '"') { if (s_[p_] == '\\') raw += take(); raw += take(); }
                if (p_ < s_.size()) raw += take();
                continue;
            }
            raw += c;
            if (raw.size() > 65536) fail("stringa troppo lunga", t.pos);
        }
        t.t = T::Interp;
        t.flags = verb ? 1 : 0;
        t.str = addString(std::move(raw));
    }
    void charLit(Token& t) {
        take();
        std::string text;
        const bool wide = !isCFamily();
        if (peekc() == '\'') fail("carattere vuoto ''", t.pos);
        while (true) {
            if (p_ >= s_.size() || s_[p_] == '\n') fail("carattere non chiuso", t.pos);
            const char c = take();
            if (c == '\'') break;
            if (c == '\\') escape(text, wide); else text += c;
            if (text.size() > 8) fail("carattere non chiuso", t.pos);
        }
        // un solo carattere (UTF-8 -> codice)
        unsigned cp = 0;
        std::size_t n = 0;
        const unsigned char b0 = static_cast<unsigned char>(text[0]);
        if (b0 < 0x80) { cp = b0; n = 1; }
        else if ((b0 & 0xE0) == 0xC0 && text.size() >= 2) { cp = ((b0 & 0x1Fu) << 6) | (static_cast<unsigned char>(text[1]) & 0x3Fu); n = 2; }
        else if ((b0 & 0xF0) == 0xE0 && text.size() >= 3) { cp = ((b0 & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[2]) & 0x3Fu); n = 3; }
        else { cp = b0; n = 1; }
        if (isCFamily()) {
            if (text.size() != 1) {
                if (n == text.size() && cp > 0xFF) fail("carattere non ASCII in un char del C: usa una stringa", t.pos);
                if (text.size() > 1 && n != text.size()) fail("costante di piu' caratteri non supportata", t.pos);
                cp = static_cast<unsigned char>(text[0]);
            }
        } else if (n != text.size()) {
            fail("un char contiene un solo carattere", t.pos);
        }
        if (!isCFamily() && cp > 0xFFFF) fail("carattere fuori dal piano di base (usa una stringa)", t.pos);
        t.t = T::Char;
        t.i = static_cast<std::int64_t>(cp);
        t.flags = wide ? 2 : 0;
    }
    void punct(Token& t) {
        const unsigned mask = 1u << static_cast<unsigned>(o_.dialect);
        for (const PunctDef& d : kPuncts) {
            if (!(d.dialects & mask)) continue;
            const std::size_t n = std::strlen(d.text);
            if (s_.compare(p_, n, d.text) == 0) {
                for (std::size_t k = 0; k < n; ++k) take();
                t.t = T::Punct;
                t.p = d.p;
                return;
            }
        }
        const char c = s_[p_];
        const P p = singlePunct(c);
        if (p == PNone || (p == Hash && !isCFamily()) || (p == At && o_.dialect != Dialect::Java)) {
            const unsigned char u = static_cast<unsigned char>(c);
            fail(std::string("carattere non valido '") + (u >= 0x20 && u < 0x7F ? std::string(1, c) : "\\x" + std::to_string(u)) + "'", t.pos);
        }
        take();
        t.t = T::Punct;
        t.p = p;
    }
    // Direttive di C#: #region/#endregion/#pragma/#nullable ignorate;
    // #define, #undef, #if, #elif, #else, #endif con simboli, !, &&, ||, ( ).
    struct CsIf { bool active, taken, parentActive; Pos pos; };
    void csDirective(Pos at) {
        std::string lineText;
        while (p_ < s_.size() && s_[p_] != '\n') lineText += take();
        std::size_t k = 1;
        while (k < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[k]))) ++k;
        std::size_t e = k;
        while (e < lineText.size() && std::isalpha(static_cast<unsigned char>(lineText[e]))) ++e;
        const std::string word = lineText.substr(k, e - k);
        std::string rest = lineText.substr(e);
        const auto cut = rest.find("//");
        if (cut != std::string::npos) rest.erase(cut);
        const bool active = cs_.empty() || cs_.back().active;
        if (word == "region" || word == "endregion" || word == "pragma" || word == "nullable" || word == "warning" || word == "line") { if (active) return; }
        if (word == "define" || word == "undef") {
            if (!active) { skipInactive(); return; }
            std::string name;
            for (char c : rest) if (!std::isspace(static_cast<unsigned char>(c))) name += c;
            if (word == "define") csSymbols_.insert(name); else csSymbols_.erase(name);
            return;
        }
        if (word == "if") { const bool v = active && csEval(rest, at); cs_.push_back({v, v, active, at}); if (!v) skipInactive(); return; }
        if (word == "elif") {
            if (cs_.empty()) fail("#elif senza #if", at);
            CsIf& c = cs_.back();
            const bool v = c.parentActive && !c.taken && csEval(rest, at);
            c.active = v; c.taken = c.taken || v;
            if (!v) skipInactive();
            return;
        }
        if (word == "else") {
            if (cs_.empty()) fail("#else senza #if", at);
            CsIf& c = cs_.back();
            c.active = c.parentActive && !c.taken; c.taken = true;
            if (!c.active) skipInactive();
            return;
        }
        if (word == "endif") {
            if (cs_.empty()) fail("#endif senza #if", at);
            cs_.pop_back();
            if (!cs_.empty() && !cs_.back().active) skipInactive();
            return;
        }
        if (word == "error") fail("#error" + rest, at);
        if (!active) { skipInactive(); return; }
        fail("direttiva #" + word + " non supportata", at);
    }
    // Salta le righe di un ramo inattivo fino alla prossima direttiva.
    void skipInactive() {
        while (p_ < s_.size()) {
            if (s_[p_] == '\n') { take(); continue; }
            std::size_t q = p_;
            while (q < s_.size() && (s_[q] == ' ' || s_[q] == '\t')) ++q;
            if (q < s_.size() && s_[q] == '#') {
                while (p_ < q) take();
                csDirective(here());
                return;
            }
            while (p_ < s_.size() && s_[p_] != '\n') take();
        }
    }
    bool csEval(const std::string& e, Pos at) {
        std::size_t i = 0;
        std::function<bool()> orE, andE, unary;
        auto ws = [&] { while (i < e.size() && std::isspace(static_cast<unsigned char>(e[i]))) ++i; };
        unary = [&]() -> bool {
            ws();
            if (i < e.size() && e[i] == '!') { ++i; return !unary(); }
            if (i < e.size() && e[i] == '(') { ++i; const bool v = orE(); ws(); if (i >= e.size() || e[i] != ')') fail("')' attesa in #if", at); ++i; return v; }
            std::string name;
            while (i < e.size() && (std::isalnum(static_cast<unsigned char>(e[i])) || e[i] == '_')) name += e[i++];
            if (name.empty()) fail("espressione di #if non valida", at);
            if (name == "true") return true;
            if (name == "false") return false;
            return csSymbols_.count(name) != 0;
        };
        andE = [&]() -> bool { bool v = unary(); ws(); while (e.compare(i, 2, "&&") == 0) { i += 2; const bool r = unary(); v = v && r; ws(); } return v; };
        orE = [&]() -> bool { bool v = andE(); ws(); while (e.compare(i, 2, "||") == 0) { i += 2; const bool r = andE(); v = v || r; ws(); } return v; };
        const bool v = orE();
        ws();
        if (i != e.size()) fail("espressione di #if non valida", at);
        return v;
    }
    const std::string& s_;
    const LexOptions& o_;
    Symbols& syms_;
    Lexed& out_;
    std::size_t p_{0};
    std::uint32_t line_{1};
    std::uint32_t col_{1};
    std::vector<CsIf> cs_;
    std::set<std::string> csSymbols_;
};

} // namespace

void lex(const std::string& source, const LexOptions& opt, Symbols& syms, Lexed& out) {
    SourceLexer l(source, opt, syms, out);
    l.run();
}

// ================================================================ preprocessore
namespace {

struct Macro {
    bool function{false};
    std::vector<Sym> params;
    std::vector<Token> body;
    Pos defined;
};

class Preprocessor {
public:
    Preprocessor(PreContext& ctx, Lexed& out) : ctx_(ctx), out_(out) {
        symDefined_ = ctx_.syms->intern("defined");
        symLine_ = ctx_.syms->intern("__LINE__");
        symFile_ = ctx_.syms->intern("__FILE__");
        symVaArgs_ = ctx_.syms->intern("__VA_ARGS__");
        // macro predefinite
        auto define = [&](const char* name, std::int64_t v) {
            Macro m;
            Token t; t.t = T::Int; t.i = v; t.flags = 2;
            m.body.push_back(t);
            macros_[ctx_.syms->intern(name)] = m;
        };
        define("__PICOCALC__", 1);
        define("__STDC__", 1);
        if (ctx_.dialect == Dialect::Cpp) define("__cplusplus", 201703);
        else define("__STDC_VERSION__", 201112);
    }
    void file(const std::string& source, std::uint16_t fileIndex, int depth) {
        if (depth > 16) fail("#include annidati oltre 16 livelli", Pos{fileIndex, 1, 1});
        Lexed raw;
        LexOptions lo;
        lo.dialect = ctx_.dialect;
        lo.file = fileIndex;
        lex(source, lo, *ctx_.syms, raw);
        // i letterali del file vanno nella tabella comune: si rinumerano
        const std::uint32_t base = static_cast<std::uint32_t>(out_.strings.size());
        for (auto& s : raw.strings) out_.strings.push_back(std::move(s));
        for (Token& t : raw.tokens) if (t.t == T::Str || t.t == T::Interp) t.str += base;
        const std::size_t condDepth = conds_.size();
        std::size_t i = 0;
        const auto& toks = raw.tokens;
        while (i < toks.size() && toks[i].t != T::End) {
            if (++polls_ % 4096 == 0 && ctx_.poll) ctx_.poll();
            const Token& t = toks[i];
            if (t.t == T::Punct && t.p == Hash && t.lineStart) {
                std::size_t e = i + 1;
                while (e < toks.size() && toks[e].t != T::End && !toks[e].lineStart) ++e;
                directive(toks, i + 1, e, t.pos, fileIndex, depth);
                i = e;
                continue;
            }
            if (!active()) { ++i; continue; }
            // token normale (eventuale macro)
            std::vector<Token> pending;
            i = expandAt(toks, i, pending);
            for (Token& p : pending) emit(p);
        }
        if (conds_.size() != condDepth) fail("#if senza #endif", conds_.back().pos);
    }
private:
    struct Cond { bool active, taken, parent; Pos pos; };
    bool active() const noexcept { return conds_.empty() || conds_.back().active; }
    void emit(const Token& t) {
        out_.tokens.push_back(t);
        if (out_.tokens.size() > ctx_.maxTokens) fail("Memory ERROR: sorgente troppo lungo dopo il preprocessore", t.pos);
    }
    // Espande il token in posizione i (macro o no); restituisce l'indice seguente.
    std::size_t expandAt(const std::vector<Token>& toks, std::size_t i, std::vector<Token>& out) {
        const Token& t = toks[i];
        if (t.t != T::Ident) { out.push_back(t); return i + 1; }
        if (t.sym == symLine_) { Token n = t; n.t = T::Int; n.i = t.pos.line; out.push_back(n); return i + 1; }
        if (t.sym == symFile_) {
            Token n = t; n.t = T::Str; n.flags = 0;
            out_.strings.push_back(ctx_.files && t.pos.file < ctx_.files->size() ? (*ctx_.files)[t.pos.file] : std::string());
            n.str = static_cast<std::uint32_t>(out_.strings.size() - 1);
            out.push_back(n);
            return i + 1;
        }
        const auto m = macros_.find(t.sym);
        if (m == macros_.end() || std::find(active_.begin(), active_.end(), t.sym) != active_.end()) { out.push_back(t); return i + 1; }
        const Macro& mac = m->second;
        std::vector<Token> body;
        std::size_t next = i + 1;
        if (mac.function) {
            if (next >= toks.size() || toks[next].t != T::Punct || toks[next].p != LParen) { out.push_back(t); return i + 1; }
            // argomenti tra parentesi bilanciate
            std::vector<std::vector<Token>> args(1);
            int depth = 0;
            std::size_t k = next + 1;
            for (;; ++k) {
                if (k >= toks.size() || toks[k].t == T::End) fail("argomenti della macro non chiusi", t.pos);
                const Token& a = toks[k];
                if (a.t == T::Punct && (a.p == LParen || a.p == LBracket || a.p == LBrace)) ++depth;
                if (a.t == T::Punct && (a.p == RParen || a.p == RBracket || a.p == RBrace)) { if (depth == 0 && a.p == RParen) break; --depth; }
                if (a.t == T::Punct && a.p == Comma && depth == 0) { args.emplace_back(); continue; }
                args.back().push_back(a);
            }
            next = k + 1;
            if (mac.params.empty() && args.size() == 1 && args[0].empty()) args.clear();
            if (args.size() != mac.params.size())
                fail("la macro " + (*ctx_.syms)[t.sym] + " vuole " + std::to_string(mac.params.size()) + " argomenti", t.pos);
            for (const Token& b : mac.body) {
                if (b.t == T::Ident) {
                    const auto pi = std::find(mac.params.begin(), mac.params.end(), b.sym);
                    if (pi != mac.params.end()) {
                        for (Token a : args[static_cast<std::size_t>(pi - mac.params.begin())]) { a.pos = t.pos; body.push_back(a); }
                        continue;
                    }
                }
                Token c = b; c.pos = t.pos; body.push_back(c);
            }
        } else {
            for (const Token& b : mac.body) { Token c = b; c.pos = t.pos; body.push_back(c); }
        }
        if (++expansions_ > 1000000) fail("Memory ERROR: espansione delle macro troppo lunga", t.pos);
        // riesame del risultato con la macro disattivata
        active_.push_back(t.sym);
        if (active_.size() > 64) fail("macro annidate troppo a fondo", t.pos);
        Token endMark; endMark.t = T::End;
        body.push_back(endMark);
        for (std::size_t j = 0; j < body.size() && body[j].t != T::End;) j = expandAt(body, j, out);
        active_.pop_back();
        return next;
    }
    std::string spell(const Token& t) const {
        if (t.t == T::Ident) return (*ctx_.syms)[t.sym];
        if (t.t == T::Punct) return punctText(t.p);
        if (t.t == T::Int) return std::to_string(t.i);
        return "?";
    }
    void directive(const std::vector<Token>& toks, std::size_t b, std::size_t e, Pos at, std::uint16_t fileIndex, int depth) {
        if (b >= e) return;                              // "#" da solo
        const Token& d = toks[b];
        const std::string word = d.t == T::Ident ? (*ctx_.syms)[d.sym] : std::string();
        if (word == "if" || word == "ifdef" || word == "ifndef") {
            bool v = false;
            if (active()) {
                if (word == "if") v = evalIf(toks, b + 1, e, at) != 0;
                else {
                    if (b + 1 >= e || toks[b + 1].t != T::Ident) fail("#" + word + " vuole un nome", at);
                    const bool def = macros_.count(toks[b + 1].sym) != 0;
                    v = word == "ifdef" ? def : !def;
                }
            }
            conds_.push_back({v, v, active(), at});
            return;
        }
        if (word == "elif") {
            if (conds_.empty()) fail("#elif senza #if", at);
            Cond& c = conds_.back();
            const bool v = c.parent && !c.taken && evalIf(toks, b + 1, e, at) != 0;
            c.active = v; c.taken = c.taken || v;
            return;
        }
        if (word == "else") {
            if (conds_.empty()) fail("#else senza #if", at);
            Cond& c = conds_.back();
            c.active = c.parent && !c.taken; c.taken = true;
            return;
        }
        if (word == "endif") {
            if (conds_.empty()) fail("#endif senza #if", at);
            conds_.pop_back();
            return;
        }
        if (!active()) return;
        if (word == "define") {
            if (b + 1 >= e || toks[b + 1].t != T::Ident) fail("#define vuole un nome", at);
            const Sym name = toks[b + 1].sym;
            Macro m;
            m.defined = at;
            std::size_t k = b + 2;
            if (k < e && toks[k].t == T::Punct && toks[k].p == LParen && !toks[k].spaceBefore) {
                m.function = true;
                ++k;
                while (k < e && !(toks[k].t == T::Punct && toks[k].p == RParen)) {
                    if (toks[k].t == T::Punct && toks[k].p == Ellipsis) fail("macro variadiche (...) non supportate", toks[k].pos);
                    if (toks[k].t != T::Ident) fail("parametro della macro non valido", toks[k].pos);
                    m.params.push_back(toks[k].sym);
                    ++k;
                    if (k < e && toks[k].t == T::Punct && toks[k].p == Comma) ++k;
                }
                if (k >= e) fail("parametri della macro non chiusi", at);
                ++k;
            }
            for (; k < e; ++k) {
                if (toks[k].t == T::Punct && (toks[k].p == Hash || toks[k].p == HashHash)) fail("gli operatori # e ## delle macro non sono supportati", toks[k].pos);
                if (toks[k].t == T::Ident && toks[k].sym == symVaArgs_) fail("__VA_ARGS__ non supportato", toks[k].pos);
                m.body.push_back(toks[k]);
            }
            if (macros_.size() > 4096) fail("troppe macro", at);
            macros_[name] = std::move(m);
            return;
        }
        if (word == "undef") {
            if (b + 1 >= e || toks[b + 1].t != T::Ident) fail("#undef vuole un nome", at);
            macros_.erase(toks[b + 1].sym);
            return;
        }
        if (word == "include") {
            if (b + 1 >= e) fail("#include vuole un file", at);
            std::string name;
            bool quoted = false;
            const Token& f = toks[b + 1];
            if (f.t == T::Str) { name = out_.strings[f.str]; quoted = true; }
            else if (f.t == T::Punct && f.p == Lt) {
                // <nome.h>: token fino a '>' senza spazi
                for (std::size_t k = b + 2; k < e; ++k) {
                    if (toks[k].t == T::Punct && toks[k].p == Gt) break;
                    name += spell(toks[k]);
                }
            } else fail("#include vuole \"file.h\" o <file.h>", at);
            if (name.empty()) fail("#include senza nome", at);
            std::string text, canonical;
            bool standard = false;
            const std::string from = ctx_.files && fileIndex < ctx_.files->size() ? (*ctx_.files)[fileIndex] : std::string();
            if (!ctx_.include || !ctx_.include(name, quoted, from, text, canonical, standard)) {
                if (quoted) fail("file \"" + name + "\" non trovato nel progetto", f.pos);
                if (unsupportedHeader(name)) fail("intestazione <" + name + "> non supportata sulla PicoCalc", f.pos);
                fail("libreria <" + name + "> non trovata (installa con LIB)", f.pos);
            }
            if (standard) {
                if (std::find(ctx_.standardHeaders.begin(), ctx_.standardHeaders.end(), name) == ctx_.standardHeaders.end()) ctx_.standardHeaders.push_back(name);
                return;
            }
            if (once_.count(canonical)) return;
            // indice del file
            std::uint16_t idx = 0;
            auto& files = *ctx_.files;
            const auto it = std::find(files.begin(), files.end(), canonical);
            if (it != files.end()) idx = static_cast<std::uint16_t>(it - files.begin());
            else {
                if (files.size() >= 4000) fail("troppi file nel progetto", at);
                files.push_back(canonical);
                idx = static_cast<std::uint16_t>(files.size() - 1);
            }
            onceCandidate_ = canonical;
            file(text, idx, depth + 1);
            return;
        }
        if (word == "pragma") {
            if (b + 1 < e && toks[b + 1].t == T::Ident && (*ctx_.syms)[toks[b + 1].sym] == "once") {
                if (ctx_.files && fileIndex < ctx_.files->size()) once_.insert((*ctx_.files)[fileIndex]);
            }
            return;
        }
        if (word == "error") {
            std::string msg;
            for (std::size_t k = b + 1; k < e; ++k) msg += (msg.empty() ? "" : " ") + spell(toks[k]);
            fail("#error " + msg, at);
        }
        if (word == "warning" || word == "line" || word == "ident") return;
        fail("direttiva #" + word + " non supportata", at);
    }
    // Espressione intera di #if (macro espanse, defined X, identificatori = 0).
    std::int64_t evalIf(const std::vector<Token>& toks, std::size_t b, std::size_t e, Pos at) {
        std::vector<Token> in;
        for (std::size_t k = b; k < e; ++k) {
            const Token& t = toks[k];
            if (t.t == T::Ident && t.sym == symDefined_) {
                std::size_t q = k + 1;
                const bool paren = q < e && toks[q].t == T::Punct && toks[q].p == LParen;
                if (paren) ++q;
                if (q >= e || toks[q].t != T::Ident) fail("defined vuole un nome", at);
                Token n = t; n.t = T::Int; n.i = macros_.count(toks[q].sym) ? 1 : 0;
                in.push_back(n);
                k = q;
                if (paren) { ++k; if (k >= e || toks[k].t != T::Punct || toks[k].p != RParen) fail("')' attesa dopo defined", at); }
                continue;
            }
            in.push_back(t);
        }
        Token endMark; endMark.t = T::End;
        in.push_back(endMark);
        std::vector<Token> x;
        for (std::size_t j = 0; j < in.size() && in[j].t != T::End;) j = expandAt(in, j, x);
        for (Token& t : x) if (t.t == T::Ident) { const std::string& w = (*ctx_.syms)[t.sym]; t.t = T::Int; t.i = (w == "true") ? 1 : 0; }
        std::size_t i = 0;
        const std::int64_t v = condExpr(x, i, at, 0);
        if (i != x.size()) fail("espressione di #if non valida", at);
        return v;
    }
    std::int64_t condExpr(const std::vector<Token>& x, std::size_t& i, Pos at, int depth) {
        if (depth > 32) fail("espressione di #if troppo annidata", at);
        const std::int64_t c = binary(x, i, at, 0, depth);
        if (i < x.size() && x[i].t == T::Punct && x[i].p == Question) {
            ++i;
            const std::int64_t a = condExpr(x, i, at, depth + 1);
            if (i >= x.size() || x[i].t != T::Punct || x[i].p != Colon) fail("':' attesa in #if", at);
            ++i;
            const std::int64_t b = condExpr(x, i, at, depth + 1);
            return c ? a : b;
        }
        return c;
    }
    static int prec(P p) {
        switch (p) {
        case OrOr: return 1; case AndAnd: return 2; case Pipe: return 3; case Caret: return 4; case Amp: return 5;
        case EqEq: case Ne: return 6; case Lt: case Gt: case Le: case Ge: return 7; case Shl: case Shr: return 8;
        case Plus: case Minus: return 9; case Star: case Slash: case Percent: return 10;
        default: return -1;
        }
    }
    std::int64_t binary(const std::vector<Token>& x, std::size_t& i, Pos at, int minPrec, int depth) {
        std::int64_t l = unary(x, i, at, depth);
        while (i < x.size() && x[i].t == T::Punct) {
            const P p = x[i].p;
            const int pr = prec(p);
            if (pr < 0 || pr < minPrec) break;
            ++i;
            const std::int64_t r = binary(x, i, at, pr + 1, depth + 1);
            switch (p) {
            case OrOr: l = (l || r); break; case AndAnd: l = (l && r); break; case Pipe: l |= r; break; case Caret: l ^= r; break;
            case Amp: l &= r; break; case EqEq: l = l == r; break; case Ne: l = l != r; break; case Lt: l = l < r; break;
            case Gt: l = l > r; break; case Le: l = l <= r; break; case Ge: l = l >= r; break;
            case Shl: l = static_cast<std::int64_t>(static_cast<std::uint64_t>(l) << (r & 63)); break; case Shr: l >>= (r & 63); break;
            case Plus: l = static_cast<std::int64_t>(static_cast<std::uint64_t>(l) + static_cast<std::uint64_t>(r)); break;
            case Minus: l = static_cast<std::int64_t>(static_cast<std::uint64_t>(l) - static_cast<std::uint64_t>(r)); break;
            case Star: l = static_cast<std::int64_t>(static_cast<std::uint64_t>(l) * static_cast<std::uint64_t>(r)); break;
            case Slash: if (r == 0) fail("divisione per zero in #if", at); l = (l == INT64_MIN && r == -1) ? l : l / r; break;
            case Percent: if (r == 0) fail("divisione per zero in #if", at); l = (r == -1) ? 0 : l % r; break;
            default: break;
            }
        }
        return l;
    }
    std::int64_t unary(const std::vector<Token>& x, std::size_t& i, Pos at, int depth) {
        if (depth > 64) fail("espressione di #if troppo annidata", at);
        if (i >= x.size()) fail("espressione di #if incompleta", at);
        const Token& t = x[i];
        if (t.t == T::Int || t.t == T::Char) { ++i; return t.i; }
        if (t.t == T::Punct) {
            if (t.p == LParen) { ++i; const std::int64_t v = condExpr(x, i, at, depth + 1); if (i >= x.size() || x[i].t != T::Punct || x[i].p != RParen) fail("')' attesa in #if", at); ++i; return v; }
            if (t.p == Bang) { ++i; return !unary(x, i, at, depth + 1); }
            if (t.p == Minus) { ++i; return static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(unary(x, i, at, depth + 1))); }
            if (t.p == Plus) { ++i; return unary(x, i, at, depth + 1); }
            if (t.p == Tilde) { ++i; return ~unary(x, i, at, depth + 1); }
        }
        fail("espressione di #if non valida", at);
    }
    PreContext& ctx_;
    Lexed& out_;
    std::unordered_map<Sym, Macro> macros_;
    std::vector<Cond> conds_;
    std::vector<Sym> active_;
    std::set<std::string> once_;
    std::string onceCandidate_;
    Sym symDefined_{0}, symLine_{0}, symFile_{0}, symVaArgs_{0};
    std::size_t expansions_{0}, polls_{0};
};

} // namespace

void preprocess(const std::string& source, std::uint16_t fileIndex, PreContext& ctx, Lexed& out) {
    Preprocessor pp(ctx, out);
    pp.file(source, fileIndex, 0);
    Token e;
    e.t = T::End;
    e.lineStart = true;
    e.pos = out.tokens.empty() ? Pos{fileIndex, 1, 1} : out.tokens.back().pos;
    out.tokens.push_back(e);
}

} // namespace fx::prog::ast
