// Caricatore dei progetti e pipeline di compilazione (v1.20.0): file del
// programma, intestazioni del progetto ("x.h": cartella del file, poi radice
// del progetto), intestazioni standard virtuali (<stdio.h>, <vector>...),
// librerie installate (<matrici.h> del C/C++, using Matrici; di C#) con i loro
// file sorgente, limiti di dimensione; poi lexer/preprocessore, parser e
// analisi. Il risultato vive solo in RAM (docs/reports/ANALISI_FONTI_v1.20.0.md §5.1).
#include "program_compile.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace fx::prog {

// ================================================================ nomi e dialetti
namespace {
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string extOf(const std::string& f) {
    const std::size_t slash = f.find_last_of("/\\");
    const std::size_t dot = f.rfind('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    return lower(f.substr(dot));
}
} // namespace
Dialect dialectOf(const std::string& fileName) noexcept {
    const std::string e = extOf(fileName);
    if (e == ".c" || e == ".h") return Dialect::C;
    if (e == ".cs") return Dialect::CSharp;
    if (e == ".java") return Dialect::Java;
    return Dialect::Cpp;
}
const char* dialectName(Dialect d) noexcept {
    switch (d) {
    case Dialect::C: return "C";
    case Dialect::Cpp: return "C++";
    case Dialect::CSharp: return "C#";
    case Dialect::Java: return "Java";
    }
    return "C++";
}
bool isTranslationUnit(const std::string& fileName, Dialect d) noexcept {
    const std::string e = extOf(fileName);
    switch (d) {
    case Dialect::C: return e == ".c";
    case Dialect::Cpp: return e == ".cpp" || e == ".cc" || e == ".cxx" || e == ".c";
    case Dialect::CSharp: return e == ".cs";
    case Dialect::Java: return e == ".java";
    }
    return false;
}
Dialect unitDialect(const std::string& file) noexcept { return dialectOf(file); }
std::size_t codeSize(const Program& program) noexcept {
    std::size_t n = 0;
    for (const auto& f : program.functions) n += f->code.size();
    return n;
}
std::vector<std::string> programFiles(const Program& program) { return program.files; }

const NativeEntry* libJavaEntries(std::size_t& n) { n = 0; return nullptr; }

// ================================================================ caricatore
namespace {
std::string normalize(const std::string& p) {
    std::vector<std::string> parts;
    std::string cur;
    std::string s = p;
    for (char& c : s) if (c == '\\') c = '/';
    std::size_t b = 0;
    while (b <= s.size()) {
        std::size_t e = s.find('/', b);
        if (e == std::string::npos) e = s.size();
        const std::string seg = s.substr(b, e - b);
        if (seg == "..") { if (!parts.empty()) parts.pop_back(); else parts.push_back(".."); }
        else if (!seg.empty() && seg != ".") parts.push_back(seg);
        b = e + 1;
    }
    std::string out;
    for (const std::string& x : parts) out += (out.empty() ? "" : "/") + x;
    return out;
}
std::string dirOf(const std::string& p) { const std::size_t s = p.rfind('/'); return s == std::string::npos ? std::string() : p.substr(0, s); }
const char* langKey(Dialect d) {
    switch (d) {
    case Dialect::C: return "c";
    case Dialect::Cpp: return "cpp";
    case Dialect::CSharp: return "cs";
    case Dialect::Java: return "java";
    }
    return "cpp";
}
std::string mainName(Dialect d) {
    switch (d) {
    case Dialect::C: return "main.c";
    case Dialect::Cpp: return "main.cpp";
    case Dialect::CSharp: return "Program.cs";
    case Dialect::Java: return "Main.java";
    }
    return "main.cpp";
}

struct Loader {
    Project& project;
    Program& prog;
    ast::Symbols syms;
    ast::NodePool pool;
    std::vector<ast::Unit> units;
    std::map<std::string, std::string> projectText;      // percorso -> testo (file del progetto noti)
    std::size_t totalBytes{0};
    std::vector<LibraryRef> libs;                        // librerie installate del linguaggio (cache)
    bool libsLoaded{false};
    std::set<std::string> libsUsed;                      // nomi delle librerie gia' aggiunte
    struct PendingUnit { std::string path; std::string text; std::int32_t library; Dialect dialect; };
    std::vector<PendingUnit> pending;

    Loader(Project& p, Program& g) : project(p), prog(g) {}

    void account(const std::string& path, std::size_t bytes) {
        if (bytes > MaxSourceBytes) throw Error("Memory ERROR: il file " + path + " supera " + std::to_string(MaxSourceBytes / 1024) + " KB", 0, 0, path);
        totalBytes += bytes;
        if (totalBytes > project.projectBytes) throw Error("Memory ERROR: il progetto supera " + std::to_string(project.projectBytes / 1024) + " KB di sorgenti", 0, 0, path);
    }
    const std::vector<LibraryRef>& installed() {
        if (!libsLoaded) {
            libsLoaded = true;
            if (project.provider) libs = project.provider->libraries(langKey(project.dialect));
            if (project.dialect == Dialect::Cpp && project.provider) {
                // il C++ usa anche le librerie del C
                for (LibraryRef& l : project.provider->libraries("c")) libs.push_back(l);
            }
        }
        return libs;
    }
    std::int32_t addLibrary(const LibraryRef& lib) {
        for (std::size_t i = 0; i < prog.libraries.size(); ++i) if (prog.libraries[i].name == lib.name && prog.libraries[i].language == lib.language) return static_cast<std::int32_t>(i);
        prog.libraries.push_back(lib);
        project.libraries.push_back(lib);
        return static_cast<std::int32_t>(prog.libraries.size() - 1);
    }
    // Una libreria usata: i suoi file sorgente diventano unita' del progetto.
    void useLibrary(const LibraryRef& lib, std::int32_t index) {
        const std::string key = lib.language + "/" + lib.name;
        if (libsUsed.count(key)) return;
        libsUsed.insert(key);
        const Dialect d = lib.language == "c" ? Dialect::C : lib.language == "cs" ? Dialect::CSharp : lib.language == "java" ? Dialect::Java : Dialect::Cpp;
        for (const auto& f : lib.files) {
            if (!isTranslationUnit(f.first, d)) continue;
            std::string text, why;
            if (!project.provider || !project.provider->readLibrary(lib, f.first, text, why))
                throw Error("Syntax ERROR: libreria " + lib.name + ": " + f.first + " non leggibile (" + why + "): usa LIB > VERIFY", 0, 0, lib.root);
            const std::string path = lib.root + "/" + f.first;
            account(path, text.size());
            pending.push_back({path, std::move(text), index, d});
        }
    }
    bool include(const std::string& name, bool quoted, const std::string& from, std::string& text, std::string& canonical, bool& standard) {
        standard = false;
        if (quoted) {
            // cartella del file che include, poi radice del progetto
            std::vector<std::string> tries;
            const std::string d = dirOf(from);
            if (!d.empty()) tries.push_back(normalize(d + "/" + name));
            tries.push_back(normalize(name));
            for (const std::string& t : tries) {
                if (t.rfind("..", 0) == 0) continue;
                auto it = projectText.find(t);
                if (it != projectText.end()) { text = it->second; canonical = t; account(t, 0); return true; }
                std::string why;
                // file di una libreria che include un suo file
                for (std::size_t i = 0; i < prog.libraries.size(); ++i) {
                    const LibraryRef& lib = prog.libraries[i];
                    if (from.rfind(lib.root + "/", 0) == 0) {
                        const std::string rel = normalize(dirOf(from.substr(lib.root.size() + 1)) + "/" + name);
                        if (project.provider && project.provider->readLibrary(lib, rel, text, why)) { canonical = lib.root + "/" + rel; account(canonical, text.size()); return true; }
                    }
                }
                if (project.provider && project.provider->read(t, text, why)) { canonical = t; projectText[t] = text; account(t, text.size()); return true; }
            }
            return false;
        }
        if (standardHeader(project.dialect, name) || (project.dialect == Dialect::Cpp && standardHeader(Dialect::C, name))) { standard = true; return true; }
        // libreria installata: <nome.h> o <nome/file.h>
        const std::string n = normalize(name);
        for (const LibraryRef& lib : installed()) {
            const std::string stem = n.find('/') != std::string::npos ? n.substr(0, n.find('/')) : n.substr(0, n.find('.'));
            if (lower(stem) != lower(lib.name)) continue;
            std::string rel = n.find('/') != std::string::npos ? n.substr(n.find('/') + 1) : n;
            std::string why;
            if (!project.provider || !project.provider->readLibrary(lib, rel, text, why)) continue;
            const std::int32_t idx = addLibrary(lib);
            canonical = lib.root + "/" + rel;
            account(canonical, text.size());
            useLibrary(lib, idx);
            return true;
        }
        return false;
    }
    void parseUnit(const std::string& path, const std::string& text, Dialect d, std::int32_t library) {
        if (prog.files.size() >= 4000) throw Error("Memory ERROR: troppi file nel progetto", 0, 0, path);
        prog.files.push_back(path);
        const std::uint16_t fileIndex = static_cast<std::uint16_t>(prog.files.size() - 1);
        ast::Lexed lexed;
        ast::Unit u;
        u.dialect = d;
        u.file = fileIndex;
        u.library = library;
        if (d == Dialect::C || d == Dialect::Cpp) {
            ast::PreContext ctx;
            ctx.dialect = d;
            ctx.syms = &syms;
            ctx.files = &prog.files;
            ctx.include = [this](const std::string& name, bool quoted, const std::string& from, std::string& t, std::string& canonical, bool& standard) {
                return include(name, quoted, from, t, canonical, standard);
            };
            ast::preprocess(text, fileIndex, ctx, lexed);
            u.standardHeaders = ctx.standardHeaders;
        } else {
            ast::LexOptions opt;
            opt.dialect = d;
            opt.file = fileIndex;
            ast::lex(text, opt, syms, lexed);
        }
        ast::ParseContext pc;
        pc.syms = &syms;
        pc.pool = &pool;
        pc.files = &prog.files;
        if (d == Dialect::CSharp) ast::parseCSharp(lexed, u, pc);
        else if (d == Dialect::Java) ast::parseJava(lexed, u, pc);
        else ast::parseCFamily(lexed, u, pc);
        units.push_back(std::move(u));
    }
    // C#/Java: using/import di una libreria installata (campo namespace del manifest).
    void resolveUsings() {
        if (project.dialect != Dialect::CSharp && project.dialect != Dialect::Java) return;
        for (std::size_t k = 0; k < units.size(); ++k) {
            std::vector<std::string> us = units[k].usings;
            for (const std::string& u : us) {
                if (u == "System" || u.rfind("System.", 0) == 0 || u.rfind("java.", 0) == 0 || u.rfind("javax.", 0) == 0) continue;
                for (const LibraryRef& lib : installed()) {
                    const std::string ns = lib.namespaceName.empty() ? lib.name : lib.namespaceName;
                    if (ns != u && u.rfind(ns + ".", 0) != 0) continue;
                    useLibrary(lib, addLibrary(lib));
                }
            }
            // unita' delle librerie aggiunte (possono usare altre librerie)
            while (!pending.empty()) {
                PendingUnit p = std::move(pending.front());
                pending.erase(pending.begin());
                parseUnit(p.path, p.text, p.dialect, p.library);
            }
        }
    }
};
} // namespace

std::shared_ptr<const Program> compile(Project& project) {
    auto prog = std::make_shared<Program>();
    prog->dialect = project.dialect;
    struct FailFiles { explicit FailFiles(const std::vector<std::string>* f) { ast::setFailFiles(f); } ~FailFiles() { ast::setFailFiles(nullptr); } } failFiles(&prog->files);
    Loader ld(project, *prog);
    for (const SourceFile& f : project.files) ld.projectText[normalize(f.path)] = f.text;
    // unita' del programma: prima il file con main
    std::vector<const SourceFile*> order;
    for (const SourceFile& f : project.files) if (normalize(f.path) == normalize(project.entry)) order.push_back(&f);
    for (const SourceFile& f : project.files) if (std::find(order.begin(), order.end(), &f) == order.end()) order.push_back(&f);
    for (const SourceFile* f : order) {
        const std::string path = normalize(f->path).empty() ? mainName(project.dialect) : normalize(f->path);
        ld.account(path, f->text.size());
        const Dialect d = project.dialect == Dialect::Cpp && dialectOf(path) == Dialect::C && extOf(path) == ".c" ? Dialect::Cpp : project.dialect;
        ld.parseUnit(path, f->text, d, f->library);
        while (!ld.pending.empty()) {
            Loader::PendingUnit p = std::move(ld.pending.front());
            ld.pending.erase(ld.pending.begin());
            ld.parseUnit(p.path, p.text, p.dialect, p.library);
        }
    }
    ld.resolveUsings();
    prog->sourceBytes = ld.totalBytes;
    Compiler comp(*prog, ld.units, ld.syms, ld.pool, nullptr);
    comp.run();
    return prog;
}

std::shared_ptr<const Program> compile(const std::string& source, Dialect dialect) {
    Project p;
    p.dialect = dialect;
    p.files.push_back({mainName(dialect), source, -1});
    p.entry = mainName(dialect);
    return compile(p);
}

} // namespace fx::prog
