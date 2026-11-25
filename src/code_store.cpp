// Archivio dei programmi sulla microSD (v1.20.0): vedi code_store.hpp.
#include "code_store.hpp"
#include "lib_manager.hpp"
#include "program_mode.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx::code {

// ================================================================ linguaggi
const char* folderOf(Lang l) noexcept {
    switch (l) {
    case Lang::C: return "C";
    case Lang::Cpp: return "CPP";
    case Lang::CSharp: return "CS";
    case Lang::Java: return "JAVA";
    case Lang::Python: return "PY";
    default: return "CPP";
    }
}
const char* extensionOf(Lang l) noexcept {
    switch (l) {
    case Lang::C: return ".c";
    case Lang::Cpp: return ".cpp";
    case Lang::CSharp: return ".cs";
    case Lang::Java: return ".java";
    case Lang::Python: return ".py";
    default: return ".cpp";
    }
}
const char* nameOf(Lang l) noexcept {
    switch (l) {
    case Lang::C: return "C";
    case Lang::Cpp: return "C++";
    case Lang::CSharp: return "C#";
    case Lang::Java: return "Java";
    case Lang::Python: return "Python";
    default: return "C++";
    }
}
const char* libLanguage(Lang l) noexcept {
    switch (l) {
    case Lang::C: return "c";
    case Lang::Cpp: return "cpp";
    case Lang::CSharp: return "cs";
    case Lang::Java: return "java";
    case Lang::Python: return "py";
    default: return "cpp";
    }
}
std::string rootOf(Lang l) { return std::string(CodeRoot) + "/" + folderOf(l); }
namespace {
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string extOf(const std::string& f) {
    const std::size_t slash = f.rfind('/');
    const std::size_t dot = f.rfind('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash) || dot == 0) return {};
    return lower(f.substr(dot));
}
std::string baseName(const std::string& p) { const std::size_t s = p.rfind('/'); return s == std::string::npos ? p : p.substr(s + 1); }
std::string dirName(const std::string& p) { const std::size_t s = p.rfind('/'); return s == std::string::npos ? std::string() : p.substr(0, s); }
// La password protegge la modifica come sulla fx-CG50 (non e' una cifratura): l'impronta
// non dipende dal percorso, cosi' resta valida dopo RENAME, MOVE e COPY.
std::string pwHash(const std::string& pw) { return crypto::sha256Hex("picocalc-code:" + pw); }
// `p` e' `base` o sta dentro la cartella `base`? (senza maiuscole)
bool underPath(const std::string& p, const std::string& base) {
    const std::string lp = lower(p), lb = lower(base);
    return lp == lb || lp.rfind(lb + "/", 0) == 0;
}
} // namespace
bool langOfFile(const std::string& f, Lang& out) noexcept {
    const std::string e = extOf(f);
    if (e == ".c" || e == ".h") { out = Lang::C; return true; }
    if (e == ".cpp" || e == ".cc" || e == ".cxx" || e == ".hpp" || e == ".hh") { out = Lang::Cpp; return true; }
    if (e == ".cs") { out = Lang::CSharp; return true; }
    if (e == ".java") { out = Lang::Java; return true; }
    if (e == ".py") { out = Lang::Python; return true; }
    return false;
}
bool isSource(const std::string& f) noexcept { Lang l; return langOfFile(f, l); }
prog::Dialect dialectOf(Lang l) noexcept {
    switch (l) {
    case Lang::C: return prog::Dialect::C;
    case Lang::CSharp: return prog::Dialect::CSharp;
    case Lang::Java: return prog::Dialect::Java;
    default: return prog::Dialect::Cpp;
    }
}
std::string message(files::Status st) {
    switch (st) {
    case files::Status::Ok: return "";
    case files::Status::NoCard: return "Inserire la scheda SD";
    case files::Status::Full: return "SD piena";
    case files::Status::IoError: return "SD non scrivibile o errore di lettura/scrittura (sola lettura? scheda rimossa?)";
    case files::Status::Unformatted: return "SD non formattata: formattarla in FAT32 o exFAT dal computer";
    case files::Status::NotFound: return "file non trovato";
    case files::Status::Exists: return "nome gia' usato";
    case files::Status::InvalidName: return "nome non valido";
    case files::Status::TooLarge: return "file troppo grande";
    default: return files::statusMessage(st);
    }
}
bool validItemName(const std::string& n) noexcept {
    if (n.empty() || n.size() > 40 || n[0] == '.' || n.back() == '.' || n.back() == ' ') return false;
    for (char c : n) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
    return n.find("..") == std::string::npos;
}

// ================================================================ archivio
files::Status Store::ready() {
    if (!fs_) return files::Status::NoCard;
    files::Status st = fs_->status();
    if (st != files::Status::Ok) return st;
    for (std::size_t i = 0; i < LangCount; ++i) {
        st = fs_->makeDirs(rootOf(static_cast<Lang>(i)));
        if (st != files::Status::Ok) return st;
    }
    return files::Status::Ok;
}
files::Status Store::list(const std::string& dir, std::vector<Item>& out, SortBy by) {
    out.clear();
    if (!fs_) return files::Status::NoCard;
    (void)files::recover(*fs_, dir);
    std::vector<files::Entry> entries;
    const files::Status st = fs_->list(dir, entries);
    if (st != files::Status::Ok) return st;
    std::vector<std::pair<std::string, std::string>> pw;
    (void)loadPasswords(pw);
    for (const auto& e : entries) {
        if (e.name.empty() || e.name[0] == '.') continue;
        const std::string le = lower(e.name);
        if (!e.directory && (le.size() > 4 && (le.compare(le.size() - 4, 4, ".tmp") == 0 || le.compare(le.size() - 4, 4, ".bak") == 0))) continue;
        Item it;
        it.name = e.name;
        it.dir = e.directory;
        it.size = e.size;
        const std::string full = lower(dir + "/" + e.name);
        for (const auto& p : pw) if (lower(p.first) == full) it.locked = true;
        out.push_back(std::move(it));
    }
    std::sort(out.begin(), out.end(), [by](const Item& a, const Item& b) {
        if (a.dir != b.dir) return a.dir;                        // cartelle prima
        if (by == SortBy::Size && a.size != b.size) return a.size > b.size;
        if (by == SortBy::Type) {
            const std::string ea = extOf(a.name), eb = extOf(b.name);
            if (ea != eb) return ea < eb;
        }
        return lower(a.name) < lower(b.name);
    });
    return files::Status::Ok;
}
files::Status Store::readText(const std::string& path, std::string& out, std::size_t maxBytes) {
    out.clear();
    if (!fs_) return files::Status::NoCard;
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(*fs_, path, data, maxBytes);
    if (st != files::Status::Ok) return st;
    out.assign(data.begin(), data.end());
    return files::Status::Ok;
}
files::Status Store::fileSize(const std::string& path, std::uint64_t& size) {
    size = 0;
    if (!fs_) return files::Status::NoCard;
    files::Entry e;
    const files::Status st = fs_->stat(path, e);
    if (st == files::Status::Ok) size = e.size;
    return st;
}
files::Status Store::readBlock(const std::string& path, std::uint64_t offset, std::size_t length, std::string& out, std::uint64_t* total) {
    out.clear();
    if (!fs_) return files::Status::NoCard;
    std::uint64_t size = 0;
    files::Status st = fileSize(path, size);
    if (st != files::Status::Ok) return st;
    if (total) *total = size;
    std::unique_ptr<files::Reader> r;
    st = fs_->openRead(path, r);
    if (st != files::Status::Ok) return st;
    char buf[1024];
    std::uint64_t pos = 0;
    while (pos < offset + length) {
        std::size_t got = 0;
        st = r->read(buf, sizeof buf, got);
        if (st != files::Status::Ok) return st;
        if (got == 0) break;
        const std::uint64_t a = pos, b = pos + got;
        pos = b;
        if (b <= offset) continue;
        const std::uint64_t from = std::max(a, offset), to = std::min<std::uint64_t>(b, offset + length);
        if (to > from) out.append(buf + (from - a), static_cast<std::size_t>(to - from));
    }
    return files::Status::Ok;
}
files::Status Store::writeText(const std::string& path, const std::string& text) {
    if (!fs_) return files::Status::NoCard;
    const files::Status st = fs_->makeDirs(dirName(path));
    if (st != files::Status::Ok) return st;
    return files::writeAtomic(*fs_, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}
files::Status Store::spliceBlock(const std::string& path, std::uint64_t offset, std::uint64_t oldLength, const std::string& block) {
    if (!fs_) return files::Status::NoCard;
    files::FileSystem& fs = *fs_;
    return files::writeAtomic(fs, path, [&](files::Writer& w) -> files::Status {
        std::unique_ptr<files::Reader> r;
        files::Status st = fs.openRead(path, r);
        if (st != files::Status::Ok) return st;
        char buf[1024];
        std::uint64_t pos = 0;
        bool written = false;
        for (;;) {
            std::size_t got = 0;
            st = r->read(buf, sizeof buf, got);
            if (st != files::Status::Ok) return st;
            if (got == 0) break;
            for (std::size_t i = 0; i < got;) {
                const std::uint64_t at = pos + i;
                if (at < offset) {
                    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(got - i, offset - at));
                    st = w.write(buf + i, n);
                    if (st != files::Status::Ok) return st;
                    i += n;
                } else if (at < offset + oldLength) {
                    if (!written) { st = w.write(block.data(), block.size()); if (st != files::Status::Ok) return st; written = true; }
                    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(got - i, offset + oldLength - at));
                    i += n;
                } else {
                    if (!written) { st = w.write(block.data(), block.size()); if (st != files::Status::Ok) return st; written = true; }
                    st = w.write(buf + i, got - i);
                    if (st != files::Status::Ok) return st;
                    i = got;
                }
            }
            pos += got;
        }
        if (!written) return w.write(block.data(), block.size());
        return files::Status::Ok;
    });
}
files::Status Store::makeFolder(const std::string& path) {
    if (!fs_) return files::Status::NoCard;
    if (!validItemName(baseName(path))) return files::Status::InvalidName;
    if (exists(path)) return files::Status::Exists;
    return fs_->makeDirs(path);
}
bool Store::exists(const std::string& path) { files::Entry e; return fs_ && fs_->stat(path, e) == files::Status::Ok; }
bool Store::isFolder(const std::string& path) { files::Entry e; return fs_ && fs_->stat(path, e) == files::Status::Ok && e.directory; }
files::Status Store::rename(const std::string& path, const std::string& newName) {
    if (!fs_) return files::Status::NoCard;
    if (!validItemName(newName)) return files::Status::InvalidName;
    const std::string to = dirName(path) + "/" + newName;
    if (lower(to) != lower(path) && exists(to)) return files::Status::Exists;
    const files::Status st = fs_->rename(path, to);
    if (st == files::Status::Ok) movePasswords(path, to, false);    // la password segue il file
    return st;
}
// Le password di `from` (file, o cartella con i file dentro) passano a `to`;
// con `keep` restano anche le vecchie (COPY).
void Store::movePasswords(const std::string& from, const std::string& to, bool keep) {
    std::vector<std::pair<std::string, std::string>> pw;
    if (loadPasswords(pw) != files::Status::Ok) return;
    std::vector<std::pair<std::string, std::string>> added;
    bool changed = false;
    for (auto& p : pw) {
        if (!underPath(p.first, from)) continue;
        const std::string moved = to + p.first.substr(from.size());
        if (keep) added.emplace_back(moved, p.second);
        else p.first = moved;
        changed = true;
    }
    if (!changed) return;
    for (auto& a : added) pw.push_back(std::move(a));
    (void)savePasswords(pw);
}
files::Status Store::copyTree(const std::string& from, const std::string& to) {
    files::Entry e;
    files::Status st = fs_->stat(from, e);
    if (st != files::Status::Ok) return st;
    if (!e.directory) {
        std::vector<std::uint8_t> data;
        st = files::readAll(*fs_, from, data, MaxFileBytes);
        if (st != files::Status::Ok) return st;
        return files::writeAtomic(*fs_, to, data.data(), data.size());
    }
    st = fs_->makeDirs(to);
    if (st != files::Status::Ok) return st;
    std::vector<files::Entry> entries;
    st = fs_->list(from, entries);
    if (st != files::Status::Ok) return st;
    for (const auto& c : entries) {
        st = copyTree(from + "/" + c.name, to + "/" + c.name);
        if (st != files::Status::Ok) return st;
    }
    return files::Status::Ok;
}
files::Status Store::copy(const std::string& from, const std::string& toDir, std::string* newPath) {
    if (!fs_) return files::Status::NoCard;
    if (!exists(from)) return files::Status::NotFound;
    std::string name = baseName(from);
    std::string to = toDir + "/" + name;
    if (lower(to) == lower(from) || exists(to)) {
        // copia nella stessa cartella: NOME_1, NOME_2...
        const std::string e = isFolder(from) ? std::string() : extOf(name);
        const std::string stem = e.empty() ? name : name.substr(0, name.size() - e.size());
        for (int k = 1; k < 100; ++k) {
            to = toDir + "/" + stem + "_" + std::to_string(k) + e;
            if (!exists(to)) break;
        }
        if (exists(to)) return files::Status::Exists;
    }
    if (isFolder(from) && (lower(to) + "/").rfind(lower(from) + "/", 0) == 0) return files::Status::InvalidName;   // dentro se stessa
    const files::Status st = copyTree(from, to);
    if (st != files::Status::Ok) { (void)libs::removeTree(*fs_, to); return st; }
    movePasswords(from, to, true);
    if (newPath) *newPath = to;
    return st;
}
files::Status Store::move(const std::string& from, const std::string& toDir) {
    if (!fs_) return files::Status::NoCard;
    const std::string to = toDir + "/" + baseName(from);
    if (lower(to) == lower(from)) return files::Status::Ok;
    if (exists(to)) return files::Status::Exists;
    if (isFolder(from) && (lower(toDir) + "/").rfind(lower(from) + "/", 0) == 0) return files::Status::InvalidName;
    files::Status st = fs_->rename(from, to);
    if (st != files::Status::Ok) {
        // altri volumi o rinomina non ammessa: copia e cancellazione
        st = copyTree(from, to);
        if (st != files::Status::Ok) { (void)libs::removeTree(*fs_, to); return st; }
        st = libs::removeTree(*fs_, from);
    }
    if (st == files::Status::Ok) movePasswords(from, to, false);
    return st;
}
files::Status Store::remove(const std::string& path) {
    if (!fs_) return files::Status::NoCard;
    const files::Status st = libs::removeTree(*fs_, path);
    if (st == files::Status::Ok) {
        std::vector<std::pair<std::string, std::string>> pw;
        if (loadPasswords(pw) == files::Status::Ok) {
            const std::size_t before = pw.size();
            pw.erase(std::remove_if(pw.begin(), pw.end(), [&](const auto& p) { return underPath(p.first, path); }), pw.end());
            if (pw.size() != before) (void)savePasswords(pw);
        }
    }
    return st;
}
files::Status Store::removeAll(const std::string& dir) {
    std::vector<Item> items;
    files::Status st = list(dir, items);
    if (st != files::Status::Ok) return st;
    for (const Item& it : items) {
        st = remove(dir + "/" + it.name);
        if (st != files::Status::Ok) return st;
    }
    return files::Status::Ok;
}
std::vector<std::string> Store::search(const std::string& dir, const std::string& prefix, std::size_t maxResults) {
    std::vector<std::string> out;
    if (!fs_ || prefix.empty()) return out;
    const std::string want = lower(prefix);
    std::vector<std::string> todo{dir};
    while (!todo.empty() && out.size() < maxResults) {
        const std::string d = todo.back();
        todo.pop_back();
        std::vector<Item> items;
        if (list(d, items) != files::Status::Ok) continue;
        for (const Item& it : items) {
            if (lower(it.name).rfind(want, 0) == 0) out.push_back(d + "/" + it.name);
            if (it.dir) todo.push_back(d + "/" + it.name);
            if (out.size() >= maxResults) break;
        }
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    return out;
}

// ---------------------------------------------------------------- password
namespace {
const char* passwordFile() { return "PICOCALC/CODE/.passwords"; }

} // namespace
files::Status Store::loadPasswords(std::vector<std::pair<std::string, std::string>>& out) {
    out.clear();
    if (!fs_) return files::Status::NoCard;
    std::string text;
    const files::Status st = readText(passwordFile(), text, 64 * 1024);
    if (st == files::Status::NotFound) return files::Status::Ok;
    if (st != files::Status::Ok) return st;
    std::size_t p = 0;
    while (p < text.size()) {
        std::size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        const std::string line = text.substr(p, e - p);
        p = e + 1;
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string h = line.substr(tab + 1);
        if (!crypto::validDigest(h)) continue;
        out.emplace_back(line.substr(0, tab), h);
    }
    return files::Status::Ok;
}
files::Status Store::savePasswords(const std::vector<std::pair<std::string, std::string>>& in) {
    std::string text;
    for (const auto& p : in) text += p.first + "\t" + p.second + "\n";
    if (text.empty()) {
        if (exists(passwordFile())) return fs_->remove(passwordFile());
        return files::Status::Ok;
    }
    return writeText(passwordFile(), text);
}
bool Store::locked(const std::string& path) {
    std::vector<std::pair<std::string, std::string>> pw;
    if (loadPasswords(pw) != files::Status::Ok) return false;
    for (const auto& p : pw) if (lower(p.first) == lower(path)) return true;
    return false;
}
bool Store::checkPassword(const std::string& path, const std::string& password) {
    std::vector<std::pair<std::string, std::string>> pw;
    if (loadPasswords(pw) != files::Status::Ok) return false;
    for (const auto& p : pw) if (lower(p.first) == lower(path)) return crypto::sameDigest(p.second, pwHash(password));
    return true;                                         // nessuna password
}
files::Status Store::setPassword(const std::string& path, const std::string& password) {
    if (!fs_) return files::Status::NoCard;
    std::vector<std::pair<std::string, std::string>> pw;
    files::Status st = loadPasswords(pw);
    if (st != files::Status::Ok) return st;
    const std::size_t before = pw.size();
    pw.erase(std::remove_if(pw.begin(), pw.end(), [&](const auto& p) { return lower(p.first) == lower(path); }), pw.end());
    if (!password.empty()) {
        if (password.size() > 8) return files::Status::InvalidName;      // fx-CG50: al piu' 8 caratteri
        pw.emplace_back(path, pwHash(password));
    } else if (pw.size() == before) return files::Status::Ok;
    return savePasswords(pw);
}

// ---------------------------------------------------------------- modelli
std::string Store::templateFor(Lang l, const std::string& name, bool project) {
    std::string cls = name.empty() ? std::string("Programma") : name;
    for (char& c : cls) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) c = '_';
    if (!cls.empty() && std::isdigit(static_cast<unsigned char>(cls[0]))) cls = "P" + cls;
    if (!cls.empty()) cls[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(cls[0])));
    switch (l) {
    case Lang::C:
        return project ? "#include <stdio.h>\n#include \"util.h\"\n\nint main(void) {\n    printf(\"Ciao da %s\\n\", nome());\n    return 0;\n}\n"
                       : "#include <stdio.h>\n\nint main(void) {\n    printf(\"Ciao\\n\");\n    return 0;\n}\n";
    case Lang::Cpp:
        return project ? "#include <iostream>\n#include \"util.h\"\n\nint main() {\n    std::cout << \"Ciao da \" << nome() << std::endl;\n    return 0;\n}\n"
                       : "#include <iostream>\n\nint main() {\n    std::cout << \"Ciao\" << std::endl;\n    return 0;\n}\n";
    case Lang::CSharp:
        return "using System;\n\nclass " + cls + "\n{\n    static void Main()\n    {\n        Console.WriteLine(\"Ciao\");\n    }\n}\n";
    case Lang::Java:
        return "public class " + cls + " {\n    public static void main(String[] args) {\n        System.out.println(\"Ciao\");\n    }\n}\n";
    case Lang::Python:
        return "print(\"Ciao\")\n";
    default: return {};
    }
}
files::Status Store::newFile(Lang l, const std::string& dir, const std::string& name, std::string& path) {
    if (!fs_) return files::Status::NoCard;
    std::string n = name;
    if (extOf(n).empty()) n += extensionOf(l);
    if (!validItemName(n)) return files::Status::InvalidName;
    path = dir + "/" + n;
    if (exists(path)) return files::Status::Exists;
    std::string stem = n.substr(0, n.size() - extOf(n).size());
    return writeText(path, templateFor(l, stem, false));
}
files::Status Store::newProject(Lang l, const std::string& dir, const std::string& name, std::string& path) {
    if (!fs_) return files::Status::NoCard;
    if (!validItemName(name) || name.find('.') != std::string::npos) return files::Status::InvalidName;
    path = dir + "/" + name;
    if (exists(path)) return files::Status::Exists;
    files::Status st = fs_->makeDirs(path);
    if (st != files::Status::Ok) return st;
    switch (l) {
    case Lang::C: case Lang::Cpp: {
        const bool cpp = l == Lang::Cpp;
        st = writeText(path + "/main" + extensionOf(l), templateFor(l, name, true));
        if (st == files::Status::Ok) st = writeText(path + "/util.h", cpp ? "#pragma once\nconst char* nome();\n" : "#pragma once\nconst char* nome(void);\n");
        if (st == files::Status::Ok) st = writeText(path + "/util" + std::string(extensionOf(l)), cpp ? "#include \"util.h\"\nconst char* nome() { return \"" + name + "\"; }\n"
                                                                                                  : "#include \"util.h\"\nconst char* nome(void) { return \"" + name + "\"; }\n");
        break;
    }
    case Lang::CSharp:
        st = writeText(path + "/Program.cs", "using System;\n\nclass Program\n{\n    static void Main()\n    {\n        Console.WriteLine(\"Ciao da \" + Util.Nome());\n    }\n}\n");
        if (st == files::Status::Ok) st = writeText(path + "/Util.cs", "static class Util\n{\n    public static string Nome() => \"" + name + "\";\n}\n");
        break;
    case Lang::Java:
        st = writeText(path + "/Main.java", "public class Main {\n    public static void main(String[] args) {\n        System.out.println(\"Ciao da " + name + "\");\n    }\n}\n");
        break;
    case Lang::Python:
        st = writeText(path + "/main.py", "import util\n\nprint(\"Ciao da\", util.nome())\n");
        if (st == files::Status::Ok) st = writeText(path + "/util.py", "def nome():\n    return \"" + name + "\"\n");
        break;
    default: break;
    }
    if (st != files::Status::Ok) (void)libs::removeTree(*fs_, path);
    return st;
}

// ---------------------------------------------------------------- migrazione
bool Store::hasLegacy(files::FileSystem& old) {
    for (const char* d : {"PICOCALC/PROG", "PICOCALC/PY"}) {
        std::vector<files::Entry> entries;
        if (old.list(d, entries) != files::Status::Ok) continue;
        for (const auto& e : entries) {
            if (e.directory) continue;
            const std::string x = extOf(e.name);
            if (x == ".pcprog" || x == ".c" || x == ".cpp" || x == ".cs" || x == ".py") return true;
        }
    }
    return false;
}
Store::Migration Store::migrate(files::FileSystem& old, bool removeOld) {
    Migration m;
    if (ready() != files::Status::Ok) { m.failed = 1; m.notes.push_back(message(files::Status::NoCard)); return m; }
    for (const char* d : {"PICOCALC/PROG", "PICOCALC/PY"}) {
        std::vector<files::Entry> entries;
        if (old.list(d, entries) != files::Status::Ok) continue;
        std::sort(entries.begin(), entries.end(), [](const files::Entry& a, const files::Entry& b) { return a.name < b.name; });
        for (const auto& e : entries) {
            if (e.directory) continue;
            const std::string x = extOf(e.name);
            if (x != ".pcprog" && x != ".c" && x != ".cpp" && x != ".cs" && x != ".py") continue;
            const std::string src = std::string(d) + "/" + e.name;
            std::vector<std::uint8_t> data;
            if (files::readAll(old, src, data, MaxEditBytes) != files::Status::Ok) { ++m.failed; m.notes.push_back(e.name + ": illeggibile"); continue; }
            std::string text(data.begin(), data.end());
            Lang l = Lang::Cpp;
            if (x == ".pcprog") {
                // il dialetto dalla riga "// pcprog: ..." (tolta); senza riga: C++
                prog::Dialect dl = prog::Dialect::Cpp;
                text = progmode::decodeProgram(text, prog::Dialect::Cpp, dl);
                l = dl == prog::Dialect::C ? Lang::C : dl == prog::Dialect::CSharp ? Lang::CSharp : Lang::Cpp;
            } else (void)langOfFile(e.name, l);
            const std::string stem = e.name.substr(0, e.name.size() - x.size());
            std::string dest = rootOf(l) + "/" + stem + extensionOf(l);
            if (exists(dest)) {
                std::string have;
                if (readText(dest, have) == files::Status::Ok && have == text) { ++m.skipped; continue; }   // gia' migrato
                for (int k = 1; k < 100 && exists(dest); ++k) dest = rootOf(l) + "/" + stem + "_" + std::to_string(k) + extensionOf(l);
            }
            const files::Status st = writeText(dest, text);
            if (st != files::Status::Ok) { ++m.failed; m.notes.push_back(e.name + ": " + message(st)); continue; }
            ++m.converted;
            m.notes.push_back(e.name + " -> " + dest.substr(std::strlen("PICOCALC/")));
            if (removeOld && &old != fs_) (void)old.remove(src);
        }
    }
    return m;
}

// ================================================================ fonti del compilatore
bool ProjectSources::read(const std::string& path, std::string& text, std::string& error) {
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(fs_, root_.empty() ? path : root_ + "/" + path, data, prog::MaxSourceBytes);
    if (st != files::Status::Ok) { error = message(st); return false; }
    text.assign(data.begin(), data.end());
    return true;
}
bool ProjectSources::exists(const std::string& path) { files::Entry e; return fs_.stat(root_.empty() ? path : root_ + "/" + path, e) == files::Status::Ok; }
bool ProjectSources::readLibrary(const prog::LibraryRef& lib, const std::string& path, std::string& text, std::string& error) {
    if (!libs::safeRelativePath(path)) { error = "percorso non ammesso"; return false; }
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(fs_, lib.root + "/" + path, data, prog::MaxSourceBytes);
    if (st != files::Status::Ok) { error = message(st); return false; }
    text.assign(data.begin(), data.end());
    // i file della libreria devono essere quelli del manifest (VERIFY)
    for (const auto& f : lib.files) {
        if (lower(f.first) != lower(path)) continue;
        if (!crypto::sameDigest(crypto::sha256Hex(text), f.second)) { error = "file modificato dopo l'installazione (LIB > VERIFY)"; return false; }
        return true;
    }
    error = "file non elencato nel manifest della libreria";
    return false;
}
std::vector<prog::LibraryRef> ProjectSources::libraries(const std::string& language) {
    std::vector<prog::LibraryRef> out;
    for (const libs::Installed& l : libs::listInstalled(fs_, language)) if (l.manifestOk) out.push_back(libs::toRef(l));
    return out;
}

bool loadProject(Store& store, const std::string& path, prog::Project& out, std::unique_ptr<ProjectSources>& sources, std::string& folder,
                 std::string& error, const std::string& editedPath, const std::string* editedText) {
    out = prog::Project();
    files::FileSystem* fs = store.fileSystem();
    if (!fs) { error = message(files::Status::NoCard); return false; }
    const files::Status rs = fs->status();
    if (rs != files::Status::Ok) { error = message(rs); return false; }
    Lang lang;
    const bool dir = store.isFolder(path);
    std::string root = dir ? path : dirName(path);
    std::vector<std::string> unitFiles;                      // relativi a root
    if (dir) {
        // tutti i sorgenti del progetto (anche nelle sottocartelle, per i package)
        std::vector<std::pair<std::string, std::string>> todo{{path, ""}};
        bool haveLang = false;
        while (!todo.empty()) {
            const auto [d, rel] = todo.back();
            todo.pop_back();
            std::vector<Item> items;
            if (store.list(d, items) != files::Status::Ok) continue;
            for (const Item& it : items) {
                const std::string r = rel.empty() ? it.name : rel + "/" + it.name;
                if (it.dir) { if (todo.size() < 32) todo.push_back({d + "/" + it.name, r}); continue; }
                Lang l;
                if (!langOfFile(it.name, l)) continue;
                const std::string e = extOf(it.name);
                if (e == ".h" || e == ".hpp" || e == ".hh") continue;
                if (!haveLang) { lang = l == Lang::C && e == ".c" ? Lang::C : l; haveLang = true; }
                unitFiles.push_back(r);
                if (unitFiles.size() > MaxProjectFiles) { error = "Memory ERROR: troppi file nel progetto (massimo " + std::to_string(MaxProjectFiles) + ")"; return false; }
            }
        }
        if (!haveLang) { error = "nessun sorgente nel progetto"; return false; }
        // C++ se c'e' almeno un .cpp
        for (const std::string& f : unitFiles) { Lang l; if (langOfFile(f, l) && l == Lang::Cpp) lang = Lang::Cpp; }
    } else {
        if (!langOfFile(path, lang)) { error = "tipo di file sconosciuto: " + baseName(path); return false; }
        const std::string e = extOf(path);
        if (e == ".h" || e == ".hpp" || e == ".hh") { error = "un'intestazione non si esegue: apri il file con main"; return false; }
        unitFiles.push_back(baseName(path));
        // nella cartella di un progetto (main in un altro file): tutto il progetto
        // resta un programma di un solo file salvo che sia nella cartella di un progetto
    }
    if (lang == Lang::Python) { error = "i programmi Python si eseguono nella modalita' Python"; return false; }
    if (lang == Lang::Java) { error = "Java arriva nella v1.21.0"; return false; }
    out.dialect = dialectOf(lang);
    sources = std::make_unique<ProjectSources>(*fs, root);
    out.provider = sources.get();
    const std::string editedRel = !editedPath.empty() && editedPath.size() > root.size() && lower(editedPath.substr(0, root.size() + 1)) == lower(root + "/")
                                      ? editedPath.substr(root.size() + 1) : std::string();
    for (const std::string& f : unitFiles) {
        prog::SourceFile sf;
        sf.path = f;
        if (editedText && !editedRel.empty() && lower(f) == lower(editedRel)) sf.text = *editedText;
        else if (!sources->read(f, sf.text, error)) { error = f + ": " + error; return false; }
        // la dimensione ammessa: prog::MaxSourceBytes per file
        out.files.push_back(std::move(sf));
    }
    // il file aperto (o con main) per primo
    if (!dir) out.entry = baseName(path);
    else {
        for (const prog::SourceFile& sf : out.files) {
            const std::string b = lower(baseName(sf.path));
            if (b == "main.c" || b == "main.cpp" || b == "program.cs" || b == "main.java") { out.entry = sf.path; break; }
        }
    }
    folder = root;
    return true;
}

} // namespace fx::code
