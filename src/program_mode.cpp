// Nucleo della modalita' Program (v1.17.0): vedi program_mode.hpp.
#include "program_mode.hpp"
#include "mode_codec.hpp"
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
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1); w.str(s.lastFile); w.u32(s.cursorLine); w.u32(s.cursorColumn); w.u8(s.newDialect);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    t.lastFile = r.str(64); t.cursorLine = r.u32(); t.cursorColumn = r.u32(); t.newDialect = r.u8();
    if (!r.end() || t.newDialect > 2) return false;
    s = std::move(t);
    return true;
}
} // namespace fx::progmode
