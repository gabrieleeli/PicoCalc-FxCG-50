// Nucleo della modalita' Program (v1.17.0): vedi program_mode.hpp.
#include "program_mode.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx::progmode {
namespace {
const char* const kCommon[] = {"if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "goto",
                               "int", "long", "double", "float", "char", "bool", "void", "const", "true", "false", nullptr};
const char* const kCpp[] = {"std", "string", "cout", "cin", "endl", "auto", "namespace", "using", "include", nullptr};
const char* const kCs[] = {"string", "static", "class", "public", "Console", "Math", "using", "var", "new", nullptr};
const char* const kC[] = {"printf", "scanf", "puts", "include", "unsigned", nullptr};
bool inList(const char* const* l, const std::string& w) { for (; *l; ++l) if (w == *l) return true; return false; }
bool identStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
} // namespace

std::string encodeProgram(prog::Dialect d, const std::string& source) { return std::string(MarkerPrefix) + prog::dialectName(d) + "\n" + source; }
std::string decodeProgram(const std::string& text, prog::Dialect fallback, prog::Dialect& dialect) {
    dialect = fallback;
    const std::size_t prefix = std::strlen(MarkerPrefix);
    if (text.compare(0, prefix, MarkerPrefix) != 0) return text;
    const std::size_t eol = text.find('\n');
    std::string name = text.substr(prefix, eol == std::string::npos ? std::string::npos : eol - prefix);
    while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
    if (name == "C") dialect = prog::Dialect::C;
    else if (name == "C#") dialect = prog::Dialect::CSharp;
    else if (name == "C++") dialect = prog::Dialect::Cpp;
    else return text;                                   // riga sconosciuta: resta nel testo
    return eol == std::string::npos ? std::string() : text.substr(eol + 1);
}
std::vector<Tok> highlight(const std::string& line, prog::Dialect d, bool& inComment) {
    std::vector<Tok> out(line.size(), Tok::Plain);
    std::size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    if (!inComment && i < line.size() && line[i] == '#') { for (std::size_t k = i; k < line.size(); ++k) out[k] = Tok::Preproc; return out; }
    for (i = 0; i < line.size();) {
        if (inComment) {
            const std::size_t e = line.find("*/", i);
            const std::size_t end = e == std::string::npos ? line.size() : e + 2;
            for (std::size_t k = i; k < end; ++k) out[k] = Tok::Comment;
            if (e != std::string::npos) inComment = false;
            i = end;
            continue;
        }
        const char c = line[i];
        if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') { for (std::size_t k = i; k < line.size(); ++k) out[k] = Tok::Comment; break; }
        if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') { inComment = true; out[i] = out[i + 1] = Tok::Comment; i += 2; continue; }
        if (c == '"' || c == '\'') {
            std::size_t k = i + 1;
            while (k < line.size() && line[k] != c) { if (line[k] == '\\') ++k; ++k; }
            const std::size_t end = k < line.size() ? k + 1 : line.size();
            for (std::size_t j = i; j < end; ++j) out[j] = Tok::String;
            i = end;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t k = i;
            while (k < line.size() && (identChar(line[k]) || line[k] == '.')) out[k++] = Tok::Number;
            i = k;
            continue;
        }
        if (identStart(c)) {
            std::size_t k = i;
            while (k < line.size() && identChar(line[k])) ++k;
            const std::string w = line.substr(i, k - i);
            const bool kw = inList(kCommon, w) || inList(d == prog::Dialect::Cpp ? kCpp : d == prog::Dialect::CSharp ? kCs : kC, w);
            if (kw) for (std::size_t j = i; j < k; ++j) out[j] = Tok::Keyword;
            i = k;
            continue;
        }
        ++i;
    }
    return out;
}
std::string templateFor(prog::Dialect d) {
    switch (d) {
    case prog::Dialect::C: return "#include <stdio.h>\nint main() {\n    printf(\"Ciao\\n\");\n    return 0;\n}\n";
    case prog::Dialect::CSharp: return "using System;\nclass Program {\n    static void Main() {\n        Console.WriteLine(\"Ciao\");\n    }\n}\n";
    default: return "#include <iostream>\nint main() {\n    std::cout << \"Ciao\" << std::endl;\n    return 0;\n}\n";
    }
}
void reset(State& s) { s = State{}; }
void addRecent(State& s, const std::string& path) {
    if (path.empty()) return;
    s.recent.erase(std::remove(s.recent.begin(), s.recent.end(), path), s.recent.end());
    s.recent.insert(s.recent.begin(), path);
    if (s.recent.size() > MaxRecent) s.recent.resize(MaxRecent);
}
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(2); w.str(s.lastFile); w.u32(s.cursorLine); w.u32(s.cursorColumn); w.u8(s.newDialect);
    w.u8(static_cast<std::uint8_t>(std::min<std::size_t>(s.recent.size(), MaxRecent)));
    for (std::size_t i = 0; i < s.recent.size() && i < MaxRecent; ++i) w.str(s.recent[i]);
    w.str(s.folder); w.str(s.libIndexUrl); w.u8(s.migrationAsked ? 1 : 0); w.u8(s.sort);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    const std::uint8_t v = r.u8();
    if (v != 1 && v != 2) return false;
    State t;
    t.lastFile = r.str(v == 1 ? 64 : 240); t.cursorLine = r.u32(); t.cursorColumn = r.u32(); t.newDialect = r.u8();
    if (v == 2) {
        const std::uint8_t n = r.u8();
        if (n > MaxRecent) return false;
        for (std::uint8_t i = 0; i < n; ++i) t.recent.push_back(r.str(240));
        t.folder = r.str(240);
        t.libIndexUrl = r.str(300);
        const std::uint8_t m = r.u8();
        if (m > 1) return false;
        t.migrationAsked = m != 0;
        t.sort = r.u8();
        if (t.sort > 2) return false;
    }
    if (!r.end() || t.newDialect > 3) return false;
    s = std::move(t);
    return true;
}

// ---------------------------------------------------------------- v1.20.0: evidenziazione per linguaggio
namespace {
const char* const kKwC[] = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for",
    "goto", "if", "inline", "int", "long", "register", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", "bool", "true", "false", "NULL", nullptr};
const char* const kKwCpp[] = {"class", "public", "private", "protected", "virtual", "override", "final", "template", "typename", "namespace", "using",
    "new", "delete", "this", "nullptr", "try", "catch", "throw", "operator", "friend", "explicit", "constexpr", "static_cast", "dynamic_cast",
    "const_cast", "std", "string", "vector", "map", "set", "cout", "cin", "endl", nullptr};
const char* const kKwCs[] = {"abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "class", "const", "continue", "default",
    "do", "double", "else", "enum", "false", "finally", "float", "for", "foreach", "if", "in", "int", "interface", "internal", "is", "long",
    "namespace", "new", "null", "object", "out", "override", "params", "private", "protected", "public", "readonly", "ref", "return", "sealed",
    "short", "static", "string", "struct", "switch", "this", "throw", "true", "try", "uint", "ulong", "using", "var", "virtual", "void", "while",
    "get", "set", "Console", "Math", "List", "Dictionary", nullptr};
const char* const kKwJava[] = {"abstract", "boolean", "break", "byte", "case", "catch", "char", "class", "continue", "default", "do", "double",
    "else", "enum", "extends", "final", "finally", "float", "for", "if", "implements", "import", "instanceof", "int", "interface", "long", "new",
    "null", "package", "private", "protected", "public", "return", "short", "static", "super", "switch", "this", "throw", "throws", "true",
    "false", "try", "void", "while", "var", "String", "System", nullptr};
const char* const kKwPy[] = {"False", "None", "True", "and", "as", "assert", "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while",
    "with", "yield", "print", "range", "len", nullptr};
const char kTripleD[] = "\"\"\"";
const char kTripleS[] = "'''";
} // namespace
std::vector<Tok> highlightCode(const std::string& line, int lang, int& state) {
    std::vector<Tok> out(line.size(), Tok::Plain);
    const bool py = lang == 4;
    const bool preproc = lang == 0 || lang == 1;
    std::size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    if (preproc && state == 0 && i < line.size() && line[i] == '#') { for (std::size_t k = i; k < line.size(); ++k) out[k] = Tok::Preproc; return out; }
    for (i = 0; i < line.size();) {
        if (state == 1) {
            const std::size_t e = line.find("*/", i);
            const std::size_t end = e == std::string::npos ? line.size() : e + 2;
            for (std::size_t k = i; k < end; ++k) out[k] = Tok::Comment;
            if (e != std::string::npos) state = 0;
            i = end;
            continue;
        }
        if (state == 2 || state == 3) {
            const std::size_t e = line.find(state == 2 ? kTripleD : kTripleS, i);
            const std::size_t end = e == std::string::npos ? line.size() : e + 3;
            for (std::size_t k = i; k < end; ++k) out[k] = Tok::String;
            if (e != std::string::npos) state = 0;
            i = end;
            continue;
        }
        const char c = line[i];
        if (py && c == '#') { for (std::size_t k = i; k < line.size(); ++k) out[k] = Tok::Comment; break; }
        if (!py && c == '/' && i + 1 < line.size() && line[i + 1] == '/') { for (std::size_t k = i; k < line.size(); ++k) out[k] = Tok::Comment; break; }
        if (!py && c == '/' && i + 1 < line.size() && line[i + 1] == '*') { state = 1; out[i] = out[i + 1] = Tok::Comment; i += 2; continue; }
        if (py && (line.compare(i, 3, kTripleD) == 0 || line.compare(i, 3, kTripleS) == 0)) {
            state = line[i] == '"' ? 2 : 3;
            out[i] = out[i + 1] = out[i + 2] = Tok::String;
            i += 3;
            continue;
        }
        if (c == '"' || c == '\'') {
            std::size_t k = i + 1;
            while (k < line.size() && line[k] != c) { if (line[k] == '\\') ++k; ++k; }
            const std::size_t end = k < line.size() ? k + 1 : line.size();
            for (std::size_t j = i; j < end; ++j) out[j] = Tok::String;
            i = end;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t k = i;
            while (k < line.size() && (identChar(line[k]) || line[k] == '.')) out[k++] = Tok::Number;
            i = k;
            continue;
        }
        if (identStart(c) || (c == '@' && (lang == 2 || lang == 3))) {
            std::size_t k = i + 1;
            while (k < line.size() && identChar(line[k])) ++k;
            const std::string w = line.substr(i, k - i);
            bool kw = false;
            switch (lang) {
            case 0: kw = inList(kKwC, w); break;
            case 1: kw = inList(kKwC, w) || inList(kKwCpp, w); break;
            case 2: kw = inList(kKwCs, w); break;
            case 3: kw = inList(kKwJava, w); break;
            default: kw = inList(kKwPy, w); break;
            }
            if (kw) for (std::size_t j = i; j < k; ++j) out[j] = Tok::Keyword;
            i = k;
            continue;
        }
        ++i;
    }
    return out;
}
} // namespace fx::progmode
