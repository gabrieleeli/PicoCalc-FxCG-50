// Gestore delle librerie dei programmi (v1.20.0): vedi lib_manager.hpp.
#include "lib_manager.hpp"
#include "json_lite.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace fx::libs {

// ================================================================ versioni
bool Version::parse(std::string_view t, Version& out) noexcept {
    int parts[3] = {0, 0, 0};
    std::size_t i = 0;
    for (int k = 0; k < 3; ++k) {
        if (i >= t.size() || !std::isdigit(static_cast<unsigned char>(t[i]))) return false;
        if (t[i] == '0' && i + 1 < t.size() && std::isdigit(static_cast<unsigned char>(t[i + 1]))) return false;   // niente zeri iniziali
        long v = 0;
        while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) {
            v = v * 10 + (t[i] - '0');
            if (v > 99999) return false;
            ++i;
        }
        parts[k] = static_cast<int>(v);
        if (k < 2) {
            if (i >= t.size() || t[i] != '.') return false;
            ++i;
        }
    }
    if (i != t.size()) return false;
    out.major = parts[0];
    out.minor = parts[1];
    out.patch = parts[2];
    return true;
}
std::string Version::text() const { return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch); }
int Version::compare(const Version& o) const noexcept {
    if (major != o.major) return major < o.major ? -1 : 1;
    if (minor != o.minor) return minor < o.minor ? -1 : 1;
    if (patch != o.patch) return patch < o.patch ? -1 : 1;
    return 0;
}
namespace {
std::vector<std::string_view> splitSpaces(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') ++i;
        const std::size_t b = i;
        while (i < s.size() && s[i] != ' ') ++i;
        if (i > b) out.push_back(s.substr(b, i - b));
    }
    return out;
}
// Un termine: true se valido; `ok` se v lo soddisfa.
bool term(std::string_view t, const Version* v, bool& ok) {
    ok = true;
    if (t == "*") return true;
    Version x;
    auto ge = [&](const Version& a) { return !v || v->compare(a) >= 0; };
    auto lt = [&](const Version& a) { return !v || v->compare(a) < 0; };
    if (t[0] == '^' || t[0] == '~') {
        if (!Version::parse(t.substr(1), x)) return false;
        Version upper = x;
        if (t[0] == '^') {
            // come npm: ^1.2.3 = <2.0.0, ^0.2.3 = <0.3.0, ^0.0.3 = <0.0.4
            if (x.major > 0) { upper.major++; upper.minor = 0; upper.patch = 0; }
            else if (x.minor > 0) { upper.minor++; upper.patch = 0; }
            else upper.patch++;
        } else { upper.minor++; upper.patch = 0; }
        ok = ge(x) && lt(upper);
        return true;
    }
    if (t.rfind(">=", 0) == 0) { if (!Version::parse(t.substr(2), x)) return false; ok = ge(x); return true; }
    if (t.rfind("<=", 0) == 0) { if (!Version::parse(t.substr(2), x)) return false; ok = !v || v->compare(x) <= 0; return true; }
    if (t[0] == '>') { if (!Version::parse(t.substr(1), x)) return false; ok = !v || v->compare(x) > 0; return true; }
    if (t[0] == '<') { if (!Version::parse(t.substr(1), x)) return false; ok = lt(x); return true; }
    if (t[0] == '=') t = t.substr(1);
    if (!Version::parse(t, x)) return false;
    ok = !v || v->compare(x) == 0;
    return true;
}
} // namespace
bool validRange(std::string_view range) noexcept {
    if (range.empty() || range.size() > 64) return false;
    const auto ts = splitSpaces(range);
    if (ts.empty() || ts.size() > 4) return false;
    bool ok = false;
    for (auto t : ts) if (!term(t, nullptr, ok)) return false;
    return true;
}
bool rangeMatches(std::string_view range, const Version& v) noexcept {
    if (!validRange(range)) return false;
    for (auto t : splitSpaces(range)) {
        bool ok = false;
        term(t, &v, ok);
        if (!ok) return false;
    }
    return true;
}

// ================================================================ nomi e percorsi
bool validLibName(std::string_view n) noexcept {
    if (n.empty() || n.size() > 32 || !std::islower(static_cast<unsigned char>(n[0]))) return false;
    for (char c : n) if (!(std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
    return true;
}
bool validLanguage(std::string_view l) noexcept { return l == "c" || l == "cpp" || l == "cs" || l == "java" || l == "py"; }
bool safeRelativePath(std::string_view p) noexcept {
    if (p.empty() || p.size() > 120 || p[0] == '/' || p.back() == '/') return false;
    std::size_t b = 0;
    int parts = 0;
    while (b <= p.size()) {
        std::size_t e = p.find('/', b);
        if (e == std::string_view::npos) e = p.size();
        const std::string_view seg = p.substr(b, e - b);
        if (seg.empty() || seg == "." || seg == ".." || seg[0] == '.' || seg.size() > 40) return false;
        for (char c : seg) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
        if (++parts > 4) return false;
        b = e + 1;
    }
    return true;
}
std::string langFolder(std::string_view l) {
    if (l == "c") return "C";
    if (l == "cpp") return "CPP";
    if (l == "cs") return "CS";
    if (l == "java") return "JAVA";
    if (l == "py") return "PY";
    return "X";
}
std::string libraryRoot(std::string_view language, std::string_view name) { return std::string(LibRoot) + "/" + langFolder(language) + "/" + std::string(name); }
std::string dataRoot(std::string_view language, std::string_view name) { return std::string(DataRoot) + "/" + langFolder(language) + "/" + std::string(name); }

// ================================================================ manifest e indice
namespace {
bool str(const json::Value& o, const char* key, std::string& out, std::size_t maxLen, bool required, std::string& error) {
    const json::Value* v = o.get(key);
    if (!v) { if (required) error = std::string("campo '") + key + "' mancante"; return !required; }
    if (!v->isString()) { error = std::string("il campo '") + key + "' deve essere una stringa"; return false; }
    if (v->s.size() > maxLen) { error = std::string("il campo '") + key + "' e' troppo lungo"; return false; }
    out = v->s;
    return true;
}
bool deps(const json::Value& o, std::vector<Dependency>& out, std::string& error) {
    const json::Value* d = o.get("dependencies");
    if (!d) return true;
    if (!d->isObject()) { error = "'dependencies' deve essere un oggetto {\"nome\": \"versioni\"}"; return false; }
    if (d->fields.size() > MaxDependencies) { error = "troppe dipendenze"; return false; }
    for (const auto& f : d->fields) {
        if (!validLibName(f.first)) { error = "nome di dipendenza non valido: " + f.first; return false; }
        if (!f.second.isString() || !validRange(f.second.s)) { error = "intervallo di versioni non valido per " + f.first; return false; }
        out.push_back({f.first, f.second.s});
    }
    return true;
}
bool validNamespace(const std::string& s) {
    if (s.size() > 64) return false;
    bool start = true;
    for (char c : s) {
        if (c == '.') { if (start) return false; start = true; continue; }
        if (start ? !(std::isalpha(static_cast<unsigned char>(c)) || c == '_') : !(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
        start = false;
    }
    return !start || s.empty();
}
std::string lowerAscii(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
} // namespace

bool parseManifest(const std::string& text, Manifest& m, std::string& error) {
    m = Manifest();
    json::Limits lim;
    lim.maxBytes = MaxManifestBytes;
    json::Value root;
    std::string jerr;
    if (!json::parse(text, root, jerr, lim)) { error = "library.json non valido: " + jerr; return false; }
    if (!root.isObject()) { error = "library.json deve essere un oggetto"; return false; }
    std::string version, minFw;
    if (!str(root, "name", m.name, 32, true, error) || !str(root, "version", version, 20, true, error) || !str(root, "language", m.language, 8, true, error) ||
        !str(root, "description", m.description, 200, false, error) || !str(root, "author", m.author, 80, false, error) ||
        !str(root, "license", m.license, 40, false, error) || !str(root, "namespace", m.ns, 64, false, error) ||
        !str(root, "minFirmware", minFw, 20, false, error)) {
        error = "library.json: " + error;
        return false;
    }
    if (!validLibName(m.name)) { error = "library.json: nome non valido (minuscole, cifre, _ e -, da 1 a 32): " + m.name; return false; }
    if (!Version::parse(version, m.version)) { error = "library.json: versione non valida (MAJOR.MINOR.PATCH): " + version; return false; }
    if (!validLanguage(m.language)) { error = "library.json: linguaggio sconosciuto: " + m.language; return false; }
    if (!m.ns.empty() && !validNamespace(m.ns)) { error = "library.json: namespace non valido: " + m.ns; return false; }
    if (!minFw.empty() && !Version::parse(minFw, m.minFirmware)) { error = "library.json: minFirmware non valido: " + minFw; return false; }
    const json::Value* files = root.get("files");
    if (!files || !files->isArray() || files->items.empty()) { error = "library.json: 'files' deve essere un elenco non vuoto"; return false; }
    if (files->items.size() > MaxFiles) { error = "library.json: troppi file (massimo " + std::to_string(MaxFiles) + ")"; return false; }
    std::set<std::string> seen;
    std::uint64_t total = 0;
    for (const json::Value& f : files->items) {
        if (!f.isObject()) { error = "library.json: ogni file e' {path, sha256, size}"; return false; }
        LibFile lf;
        if (!str(f, "path", lf.path, 120, true, error) || !str(f, "sha256", lf.sha256, 64, true, error)) { error = "library.json: " + error; return false; }
        if (!safeRelativePath(lf.path)) { error = "library.json: percorso non ammesso: " + lf.path; return false; }
        if (lowerAscii(lf.path) == ManifestName) { error = "library.json: il manifest non e' un file della libreria"; return false; }
        if (!crypto::validDigest(lf.sha256)) { error = "library.json: SHA-256 non valido per " + lf.path; return false; }
        const json::Value* sz = f.get("size");
        if (!sz || !sz->isNumber() || sz->n < 0 || sz->n > static_cast<double>(MaxFileBytes) || sz->n != static_cast<double>(static_cast<std::uint32_t>(sz->n))) {
            error = "library.json: dimensione non valida per " + lf.path + " (massimo " + std::to_string(MaxFileBytes / 1024) + " KB)";
            return false;
        }
        lf.size = static_cast<std::uint32_t>(sz->n);
        total += lf.size;
        if (!seen.insert(lowerAscii(lf.path)).second) { error = "library.json: file ripetuto: " + lf.path; return false; }
        m.files.push_back(std::move(lf));
    }
    if (total > MaxLibraryBytes) { error = "library.json: la libreria supera " + std::to_string(MaxLibraryBytes / 1024) + " KB"; return false; }
    if (!deps(root, m.dependencies, error)) { error = "library.json: " + error; return false; }
    for (const auto& d : m.dependencies) if (d.name == m.name) { error = "library.json: la libreria dipende da se stessa"; return false; }
    return true;
}
std::string manifestJson(const Manifest& m) {
    std::string o = "{\n";
    o += "  \"name\": " + json::quote(m.name) + ",\n";
    o += "  \"version\": " + json::quote(m.version.text()) + ",\n";
    o += "  \"language\": " + json::quote(m.language) + ",\n";
    if (!m.description.empty()) o += "  \"description\": " + json::quote(m.description) + ",\n";
    if (!m.author.empty()) o += "  \"author\": " + json::quote(m.author) + ",\n";
    if (!m.license.empty()) o += "  \"license\": " + json::quote(m.license) + ",\n";
    if (!m.ns.empty()) o += "  \"namespace\": " + json::quote(m.ns) + ",\n";
    o += "  \"minFirmware\": " + json::quote(m.minFirmware.text()) + ",\n";
    o += "  \"dependencies\": {";
    for (std::size_t i = 0; i < m.dependencies.size(); ++i) o += (i ? ", " : "") + json::quote(m.dependencies[i].name) + ": " + json::quote(m.dependencies[i].range);
    o += "},\n  \"files\": [\n";
    for (std::size_t i = 0; i < m.files.size(); ++i) {
        o += "    {\"path\": " + json::quote(m.files[i].path) + ", \"sha256\": " + json::quote(m.files[i].sha256) + ", \"size\": " + std::to_string(m.files[i].size) + "}";
        o += i + 1 < m.files.size() ? ",\n" : "\n";
    }
    o += "  ]\n}\n";
    return o;
}
bool parseIndex(const std::string& text, std::vector<IndexEntry>& out, std::string& error) {
    out.clear();
    json::Limits lim;
    lim.maxBytes = MaxIndexBytes;
    lim.maxNodes = 16384;
    json::Value root;
    std::string jerr;
    if (!json::parse(text, root, jerr, lim)) { error = "index.json non valido: " + jerr; return false; }
    if (!root.isObject()) { error = "index.json deve essere un oggetto"; return false; }
    const json::Value* ver = root.get("version");
    if (!ver || !ver->isNumber() || ver->n != 1) { error = "index.json: versione del formato non supportata (serve 1)"; return false; }
    const json::Value* libs = root.get("libraries");
    if (!libs || !libs->isArray()) { error = "index.json: 'libraries' deve essere un elenco"; return false; }
    if (libs->items.size() > 512) { error = "index.json: troppe voci"; return false; }
    for (const json::Value& e : libs->items) {
        if (!e.isObject()) { error = "index.json: voce non valida"; return false; }
        IndexEntry ie;
        std::string version;
        if (!str(e, "name", ie.name, 32, true, error) || !str(e, "language", ie.language, 8, true, error) || !str(e, "version", version, 20, true, error) ||
            !str(e, "path", ie.path, 120, true, error) || !str(e, "description", ie.description, 200, false, error) ||
            !str(e, "manifestSha256", ie.manifestSha256, 64, true, error)) {
            error = "index.json: " + error;
            return false;
        }
        if (!validLibName(ie.name) || !validLanguage(ie.language) || !Version::parse(version, ie.version)) { error = "index.json: voce non valida: " + ie.name; return false; }
        if (!safeRelativePath(ie.path)) { error = "index.json: percorso non ammesso: " + ie.path; return false; }
        if (!crypto::validDigest(ie.manifestSha256)) { error = "index.json: impronta del manifest non valida per " + ie.name; return false; }
        std::string minFw;
        if (!str(e, "minFirmware", minFw, 20, false, error)) { error = "index.json: " + error; return false; }
        if (!minFw.empty() && !Version::parse(minFw, ie.minFirmware)) { error = "index.json: minFirmware non valido per " + ie.name; return false; }
        const json::Value* ms = e.get("manifestSize");
        if (ms) {
            if (!ms->isNumber() || ms->n < 0 || ms->n > static_cast<double>(MaxManifestBytes)) { error = "index.json: manifestSize non valido per " + ie.name; return false; }
            ie.manifestSize = static_cast<std::uint32_t>(ms->n);
        }
        if (!deps(e, ie.dependencies, error)) { error = "index.json (" + ie.name + "): " + error; return false; }
        for (const IndexEntry& o : out)
            if (o.name == ie.name && o.language == ie.language && o.version == ie.version) { error = "index.json: versione ripetuta: " + ie.name + " " + version; return false; }
        out.push_back(std::move(ie));
    }
    return true;
}

// ================================================================ repository sulla SD
namespace {
bool readSmall(files::FileSystem& fs, const std::string& path, std::string& out, std::size_t maxBytes, std::string& error);
}
bool localIndex(files::FileSystem& fs, const std::string& root, std::vector<IndexEntry>& out, std::string& error) {
    out.clear();
    files::Entry e;
    if (fs.stat(root, e) != files::Status::Ok || !e.directory) { error = "cartella " + root + " assente"; return false; }
    std::string text;
    if (fs.stat(root + "/index.json", e) == files::Status::Ok) {
        if (!readSmall(fs, root + "/index.json", text, MaxIndexBytes, error) || !parseIndex(text, out, error)) return false;
    }
    for (const char* lang : {"c", "cpp", "cs", "java", "py"}) {
        std::vector<files::Entry> libs;
        if (fs.list(root + "/" + lang, libs) != files::Status::Ok) continue;
        for (const files::Entry& l : libs) {
            if (!l.directory || !validLibName(l.name)) continue;
            std::vector<files::Entry> versions;
            if (fs.list(root + "/" + lang + "/" + l.name, versions) != files::Status::Ok) continue;
            for (const files::Entry& v : versions) {
                Version ver;
                if (!v.directory || !Version::parse(v.name, ver)) continue;
                bool listed = false;
                for (const IndexEntry& x : out) if (x.name == l.name && x.language == lang && x.version == ver) listed = true;
                if (listed || out.size() >= 512) continue;
                const std::string rel = std::string(lang) + "/" + l.name + "/" + v.name;
                std::string mt, err;
                Manifest m;
                if (!readSmall(fs, root + "/" + rel + "/" + ManifestName, mt, MaxManifestBytes, err) || !parseManifest(mt, m, err)) continue;
                if (m.name != l.name || m.language != lang || !(m.version == ver)) continue;
                IndexEntry ie;
                ie.name = m.name;
                ie.language = m.language;
                ie.description = m.description;
                ie.version = m.version;
                ie.path = rel;
                ie.manifestSha256 = crypto::sha256Hex(mt);
                ie.manifestSize = static_cast<std::uint32_t>(mt.size());
                ie.dependencies = m.dependencies;
                ie.minFirmware = m.minFirmware;
                out.push_back(std::move(ie));
            }
        }
    }
    return true;
}

// ================================================================ librerie installate
namespace {
bool readSmall(files::FileSystem& fs, const std::string& path, std::string& out, std::size_t maxBytes, std::string& error) {
    std::vector<std::uint8_t> data;
    const files::Status st = files::readAll(fs, path, data, maxBytes);
    if (st != files::Status::Ok) { error = st == files::Status::TooLarge ? "file troppo grande" : files::statusMessage(st); return false; }
    out.assign(data.begin(), data.end());
    return true;
}
const char* const kLangs[] = {"c", "cpp", "cs", "java", "py"};
} // namespace
std::vector<Installed> listInstalled(files::FileSystem& fs, std::string_view language) {
    std::vector<Installed> out;
    for (const char* l : kLangs) {
        if (!language.empty() && language != l) continue;
        const std::string dir = std::string(LibRoot) + "/" + langFolder(l);
        std::vector<files::Entry> entries;
        if (fs.list(dir, entries) != files::Status::Ok) continue;
        std::sort(entries.begin(), entries.end(), [](const files::Entry& a, const files::Entry& b) { return a.name < b.name; });
        for (const auto& e : entries) {
            if (!e.directory || e.name.empty() || e.name[0] == '.') continue;
            Installed lib;
            lib.root = dir + "/" + e.name;
            std::string text, err;
            if (!readSmall(fs, lib.root + "/" + ManifestName, text, MaxManifestBytes, err)) lib.error = "library.json: " + err;
            else if (!parseManifest(text, lib.manifest, err)) lib.error = err;
            else if (lowerAscii(lib.manifest.name) != lowerAscii(e.name)) lib.error = "il nome nel manifest (" + lib.manifest.name + ") non e' quello della cartella";
            else if (lib.manifest.language != l) lib.error = "il linguaggio nel manifest non e' quello della cartella";
            else lib.manifestOk = true;
            if (!lib.manifestOk) { lib.manifest.name = lowerAscii(e.name); lib.manifest.language = l; }
            out.push_back(std::move(lib));
        }
    }
    return out;
}
prog::LibraryRef toRef(const Installed& lib) {
    prog::LibraryRef r;
    r.name = lib.manifest.name;
    r.language = lib.manifest.language;
    r.namespaceName = lib.manifest.ns;
    r.root = lib.root;
    r.dataFolder = dataRoot(lib.manifest.language, lib.manifest.name);
    for (const LibFile& f : lib.manifest.files) r.files.emplace_back(f.path, f.sha256);
    return r;
}
VerifyReport verify(files::FileSystem& fs, const Installed& lib, const std::function<void()>& poll) {
    VerifyReport rep;
    if (!lib.manifestOk) { rep.ok = false; rep.problems.push_back(lib.error.empty() ? "library.json non valido" : lib.error); return rep; }
    for (const LibFile& f : lib.manifest.files) {
        const std::string path = lib.root + "/" + f.path;
        std::unique_ptr<files::Reader> r;
        const files::Status st = fs.openRead(path, r);
        if (st != files::Status::Ok) { rep.ok = false; rep.problems.push_back((st == files::Status::NotFound ? "file mancante: " : std::string("file illeggibile: ")) + f.path); continue; }
        crypto::Sha256 h;
        std::uint64_t n = 0;
        char buf[1024];
        bool ioError = false;
        for (;;) {
            std::size_t got = 0;
            if (r->read(buf, sizeof buf, got) != files::Status::Ok) { ioError = true; break; }
            if (got == 0) break;
            h.update(buf, got);
            n += got;
            if (poll) poll();
        }
        if (ioError) { rep.ok = false; rep.problems.push_back("errore di lettura: " + f.path); continue; }
        if (n != f.size) { rep.ok = false; rep.problems.push_back("dimensione diversa: " + f.path + " (" + std::to_string(n) + " byte invece di " + std::to_string(f.size) + ")"); continue; }
        if (!crypto::sameDigest(h.hex(), f.sha256)) { rep.ok = false; rep.problems.push_back("file modificato (SHA-256 diverso): " + f.path); }
    }
    return rep;
}
std::vector<std::string> dependents(files::FileSystem& fs, std::string_view language, std::string_view name) {
    std::vector<std::string> out;
    for (const Installed& lib : listInstalled(fs, "")) {
        if (!lib.manifestOk) continue;
        const bool sameFamily = lib.manifest.language == language || (language == "c" && lib.manifest.language == "cpp");
        if (!sameFamily) continue;
        for (const Dependency& d : lib.manifest.dependencies) if (d.name == name) { out.push_back(lib.manifest.name); break; }
    }
    return out;
}
files::Status removeTree(files::FileSystem& fs, const std::string& path) {
    files::Entry e;
    files::Status st = fs.stat(path, e);
    if (st != files::Status::Ok) return st;
    if (e.directory) {
        std::vector<files::Entry> entries;
        st = fs.list(path, entries);
        if (st != files::Status::Ok) return st;
        for (const auto& c : entries) {
            st = removeTree(fs, path + "/" + c.name);
            if (st != files::Status::Ok) return st;
        }
    }
    return fs.remove(path);
}
files::Status removeLibrary(files::FileSystem& fs, std::string_view language, std::string_view name, std::string& error) {
    if (!validLanguage(language) || name.empty() || name.size() > 32) { error = "nome non valido"; return files::Status::InvalidName; }
    const std::string root = libraryRoot(language, name);
    files::Entry e;
    if (fs.stat(root, e) != files::Status::Ok) { error = "libreria non installata"; return files::Status::NotFound; }
    const files::Status st = removeTree(fs, root);
    if (st != files::Status::Ok) error = files::statusMessage(st);
    return st;
}

// ================================================================ piano
bool makePlan(const std::vector<IndexEntry>& index, const std::vector<Installed>& installed, std::string_view name, std::string_view language,
              const Version& firmware, std::vector<PlanStep>& out, std::string& error, std::string_view wantVersion) {
    out.clear();
    auto installedOf = [&](const std::string& n, const std::string& l) -> const Installed* {
        for (const Installed& i : installed) if (i.manifestOk && i.manifest.name == n && i.manifest.language == l) return &i;
        return nullptr;
    };
    // la versione piu' alta dell'indice che soddisfa l'intervallo
    std::string tooNew;                                  // versione scartata perche' vuole un firmware piu' nuovo
    auto pick = [&](const std::string& n, const std::string& l, const std::string& range) -> const IndexEntry* {
        const IndexEntry* best = nullptr;
        for (const IndexEntry& e : index) {
            if (e.name != n || e.language != l) continue;
            if (!range.empty() && !rangeMatches(range, e.version)) continue;
            if (firmware < e.minFirmware) {
                tooNew = e.name + " " + e.version.text() + " richiede il firmware " + e.minFirmware.text() + " o successivo (questo e' " + firmware.text() + ")";
                continue;
            }
            if (!best || best->version < e.version) best = &e;
        }
        return best;
    };
    struct Frame { const IndexEntry* e; };
    std::vector<std::string> path;                       // per i cicli
    std::map<std::string, const IndexEntry*> chosen;
    std::vector<const IndexEntry*> order;
    std::function<bool(const IndexEntry*)> visit = [&](const IndexEntry* e) -> bool {
        const std::string key = e->language + "/" + e->name;
        for (std::size_t i = 0; i < path.size(); ++i) if (path[i] == key) {
            std::string cyc;
            for (std::size_t k = i; k < path.size(); ++k) cyc += path[k].substr(path[k].find('/') + 1) + " -> ";
            error = "ciclo di dipendenze: " + cyc + e->name;
            return false;
        }
        auto it = chosen.find(key);
        if (it != chosen.end()) {
            if (!(it->second->version == e->version)) { error = "versioni incompatibili richieste per " + e->name + ": " + it->second->version.text() + " e " + e->version.text(); return false; }
            return true;
        }
        path.push_back(key);
        for (const Dependency& d : e->dependencies) {
            // dipendenza gia' installata e compatibile: resta
            const Installed* have = installedOf(d.name, e->language);
            if (!have && e->language == "cpp") have = installedOf(d.name, "c");
            if (have && rangeMatches(d.range, have->manifest.version) && !chosen.count(e->language + "/" + d.name)) continue;
            const IndexEntry* dep = pick(d.name, e->language, d.range);
            if (!dep && e->language == "cpp") dep = pick(d.name, "c", d.range);
            if (!dep) {
                if (!tooNew.empty()) { error = tooNew; return false; }
                const bool known = std::any_of(index.begin(), index.end(), [&](const IndexEntry& x) { return x.name == d.name; });
                error = known ? "nessuna versione di " + d.name + " soddisfa " + d.range + " (richiesta da " + e->name + ")"
                              : "dipendenza mancante: " + d.name + " " + d.range + " (richiesta da " + e->name + ")";
                return false;
            }
            if (!visit(dep)) return false;
        }
        path.pop_back();
        if (e->dependencies.size() > MaxDependencies) { error = "troppe dipendenze"; return false; }
        chosen[key] = e;
        order.push_back(e);
        if (order.size() > MaxPlan) { error = "troppe librerie da installare insieme"; return false; }
        return true;
    };
    const std::string n(name), l(language);
    const IndexEntry* root = nullptr;
    if (!wantVersion.empty()) {
        Version v;
        if (!Version::parse(wantVersion, v)) { error = "versione non valida: " + std::string(wantVersion); return false; }
        for (const IndexEntry& e : index) if (e.name == n && e.language == l && e.version == v) root = &e;
        if (!root) { error = "versione " + std::string(wantVersion) + " di " + n + " non presente nell'indice"; return false; }
        if (firmware < root->minFirmware) { error = root->name + " " + root->version.text() + " richiede il firmware " + root->minFirmware.text() + " o successivo (questo e' " + firmware.text() + ")"; return false; }
    } else root = pick(n, l, "");
    if (!root) { error = !tooNew.empty() ? tooNew : "libreria " + n + " (" + l + ") non presente nell'indice"; return false; }
    if (!visit(root)) return false;
    for (const IndexEntry* e : order) {
        PlanStep s;
        s.entry = *e;
        if (const Installed* have = installedOf(e->name, e->language)) {
            s.fromVersion = have->manifest.version.text();
            const int c = e->version.compare(have->manifest.version);
            s.downgrade = c < 0;
            s.upToDate = c == 0;
        }
        out.push_back(std::move(s));
    }
    return true;
}

// ================================================================ sorgenti
bool FolderSource::begin(const std::string& relPath, std::string& error) {
    reader_.reset();
    if (!safeRelativePath(relPath)) { error = "percorso non ammesso: " + relPath; return false; }
    const files::Status st = fs_.openRead(base_.empty() ? relPath : base_ + "/" + relPath, reader_);
    if (st != files::Status::Ok) { error = (st == files::Status::NotFound ? "file non trovato nel repository: " : std::string(files::statusMessage(st)) + ": ") + relPath; return false; }
    return true;
}
Source::Poll FolderSource::poll(std::string& chunk, std::string& error) {
    if (!reader_) { error = "nessun file aperto"; return Poll::Failed; }
    char buf[2048];
    std::size_t got = 0;
    if (reader_->read(buf, sizeof buf, got) != files::Status::Ok) { error = "errore di lettura"; reader_.reset(); return Poll::Failed; }
    if (got == 0) { reader_.reset(); return Poll::Done; }
    chunk.append(buf, got);
    return Poll::Data;
}

// ================================================================ installazione
struct Installer::Hash { crypto::Sha256 sha; };
namespace {
std::string tmpDir(const std::string& lang, const std::string& name) { return std::string(TmpRoot) + "/" + lang + "-" + name + ".new"; }
std::string oldDir(const std::string& lang, const std::string& name) { return std::string(TmpRoot) + "/" + lang + "-" + name + ".old"; }
std::string journalPath() { return std::string(TmpRoot) + "/journal"; }
files::Status writeText(files::FileSystem& fs, const std::string& path, const std::string& text) {
    return files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}
} // namespace
Installer::Installer(files::FileSystem& fs, Source& src) : fs_(fs), src_(src), hash_(std::make_unique<Hash>()) {}
Installer::~Installer() { if (state_ == State::Fetching || state_ == State::Committing) cancel(); }
void Installer::start(const std::vector<PlanStep>& plan, const Version& firmware) {
    firmware_ = firmware;
    plan_.clear();
    for (const PlanStep& s : plan) if (!s.upToDate) plan_.push_back(s);
    manifests_.assign(plan_.size(), Manifest());
    step_ = file_ = 0;
    inManifest_ = true;
    open_ = false;
    bytes_ = 0;
    error_.clear();
    writer_.reset();
    (void)removeTree(fs_, TmpRoot);
    const files::Status st = fs_.makeDirs(TmpRoot);
    if (st != files::Status::Ok) { state_ = State::Failed; error_ = st == files::Status::NoCard ? "Inserire la scheda SD" : st == files::Status::Full ? "SD piena" : files::statusMessage(st); return; }
    state_ = plan_.empty() ? State::Done : State::Fetching;
}
void Installer::fail(const std::string& why) {
    error_ = why;
    if (writer_) { (void)writer_->close(); writer_.reset(); }
    src_.cancel();
    open_ = false;
    (void)removeTree(fs_, TmpRoot);
    state_ = State::Failed;
}
void Installer::cancel() {
    if (state_ != State::Fetching && state_ != State::Committing) return;
    fail("installazione annullata");
    state_ = State::Cancelled;
}
std::string Installer::progress() const {
    if (step_ >= plan_.size()) return state_ == State::Done ? "completata" : std::string();
    const PlanStep& s = plan_[step_];
    std::string what = s.entry.name + " " + s.entry.version.text() + ": ";
    if (inManifest_) return what + ManifestName;
    const Manifest& m = manifests_[step_];
    if (file_ < m.files.size()) return what + m.files[file_].path + " " + std::to_string(file_ + 1) + "/" + std::to_string(m.files.size());
    return what + "fine";
}
bool Installer::openNext() {
    const PlanStep& s = plan_[step_];
    std::string err;
    chunk_.clear();
    got_ = 0;
    hash_->sha.reset();
    if (inManifest_) {
        manifestText_.clear();
        if (!src_.begin(s.entry.path + "/" + ManifestName, err)) { fail(err); return false; }
        open_ = true;
        return true;
    }
    const Manifest& m = manifests_[step_];
    const LibFile& f = m.files[file_];
    const std::string dest = tmpDir(m.language, m.name) + "/" + f.path;
    const std::size_t slash = dest.rfind('/');
    files::Status st = fs_.makeDirs(dest.substr(0, slash));
    if (st == files::Status::Ok) st = fs_.openWrite(dest, writer_);
    if (st != files::Status::Ok) { fail(st == files::Status::Full ? "SD piena" : st == files::Status::NoCard ? "Inserire la scheda SD" : std::string(files::statusMessage(st))); return false; }
    if (!src_.begin(s.entry.path + "/" + f.path, err)) { fail(err); return false; }
    open_ = true;
    return true;
}
bool Installer::tick(std::size_t budgetBytes) {
    if (state_ == State::Committing) { commit(); return state_ == State::Committing; }
    if (state_ != State::Fetching) return false;
    std::size_t budget = budgetBytes;
    while (budget > 0 && state_ == State::Fetching) {
        if (step_ >= plan_.size()) { state_ = State::Committing; break; }
        if (!open_ && !openNext()) return false;
        std::string err;
        const Source::Poll p = src_.poll(chunk_, err);
        if (p == Source::Poll::Busy) break;
        if (p == Source::Poll::Failed) { fail(err.empty() ? "download interrotto" : err); return false; }
        const PlanStep& s = plan_[step_];
        if (!chunk_.empty()) {
            const std::size_t n = chunk_.size();
            hash_->sha.update(chunk_);
            got_ += n;
            bytes_ += n;
            budget = budget > n ? budget - n : 0;
            if (inManifest_) {
                if (manifestText_.size() + n > MaxManifestBytes) { fail("library.json di " + s.entry.name + " troppo grande"); return false; }
                manifestText_ += chunk_;
            } else {
                const LibFile& f = manifests_[step_].files[file_];
                if (got_ > f.size) { fail("file piu' grande del dichiarato: " + s.entry.name + "/" + f.path); return false; }
                const files::Status st = writer_->write(chunk_.data(), n);
                if (st != files::Status::Ok) { fail(st == files::Status::Full ? "SD piena" : st == files::Status::NoCard ? "Inserire la scheda SD" : std::string("scrittura non riuscita: ") + files::statusMessage(st)); return false; }
            }
            chunk_.clear();
        }
        if (p != Source::Poll::Done) continue;
        open_ = false;
        const std::string digest = hash_->sha.hex();
        if (inManifest_) {
            if (!crypto::sameDigest(digest, s.entry.manifestSha256)) { fail("impronta di library.json errata per " + s.entry.name + " (manifest alterato o indice non aggiornato)"); return false; }
            Manifest m;
            std::string perr;
            if (!parseManifest(manifestText_, m, perr)) { fail(s.entry.name + ": " + perr); return false; }
            if (m.name != s.entry.name || m.language != s.entry.language || !(m.version == s.entry.version)) { fail("library.json di " + s.entry.name + " non coerente con l'indice"); return false; }
            if (firmware_ < m.minFirmware) { fail(m.name + " " + m.version.text() + " richiede il firmware " + m.minFirmware.text() + " o successivo (questo e' " + firmware_.text() + ")"); return false; }
            if (m.dependencies.size() != s.entry.dependencies.size()) { fail("dipendenze di " + s.entry.name + " diverse da quelle dell'indice"); return false; }
            for (std::size_t k = 0; k < m.dependencies.size(); ++k)
                if (m.dependencies[k].name != s.entry.dependencies[k].name || m.dependencies[k].range != s.entry.dependencies[k].range) { fail("dipendenze di " + s.entry.name + " diverse da quelle dell'indice"); return false; }
            manifests_[step_] = std::move(m);
            inManifest_ = false;
            file_ = 0;
            const files::Status st = fs_.makeDirs(tmpDir(manifests_[step_].language, manifests_[step_].name));
            if (st != files::Status::Ok) { fail(st == files::Status::Full ? "SD piena" : files::statusMessage(st)); return false; }
            continue;
        }
        const LibFile& f = manifests_[step_].files[file_];
        const files::Status cst = writer_->close();
        writer_.reset();
        if (cst != files::Status::Ok) { fail(cst == files::Status::Full ? "SD piena" : std::string("scrittura non riuscita: ") + files::statusMessage(cst)); return false; }
        if (got_ != f.size) { fail("dimensione errata: " + s.entry.name + "/" + f.path + " (" + std::to_string(got_) + " byte invece di " + std::to_string(f.size) + ")"); return false; }
        if (!crypto::sameDigest(digest, f.sha256)) { fail("SHA-256 errato: " + s.entry.name + "/" + f.path + " (file alterato o download corrotto)"); return false; }
        if (++file_ >= manifests_[step_].files.size()) {
            // manifest nella cartella nuova, poi la libreria seguente
            const Manifest& m = manifests_[step_];
            const files::Status mst = writeText(fs_, tmpDir(m.language, m.name) + "/" + ManifestName, manifestText_);
            if (mst != files::Status::Ok) { fail(mst == files::Status::Full ? "SD piena" : files::statusMessage(mst)); return false; }
            ++step_;
            inManifest_ = true;
            file_ = 0;
        }
    }
    if (state_ == State::Committing) commit();
    return state_ == State::Fetching || state_ == State::Committing;
}
bool Installer::commit() {
    // giornale: da qui in poi l'installazione si completa anche dopo un'interruzione
    std::string journal = "PENDING\n";
    for (const Manifest& m : manifests_) journal += m.language + " " + m.name + "\n";
    files::Status st = writeText(fs_, journalPath(), journal);
    if (st != files::Status::Ok) { fail(st == files::Status::Full ? "SD piena" : files::statusMessage(st)); return false; }
    std::string note;
    if (!recoverInstall(fs_, note)) { error_ = note; state_ = State::Failed; return false; }
    state_ = State::Done;
    return true;
}
bool recoverInstall(files::FileSystem& fs, std::string& note) {
    std::string journal, err;
    if (!readSmall(fs, journalPath(), journal, 4096, err)) {
        // nessuna installazione in corso: residui di un download interrotto
        files::Entry e;
        if (fs.stat(TmpRoot, e) == files::Status::Ok) {
            std::vector<files::Entry> entries;
            if (fs.list(TmpRoot, entries) == files::Status::Ok && !entries.empty()) { (void)removeTree(fs, TmpRoot); note = "download interrotto annullato"; }
        }
        return true;
    }
    if (journal.rfind("PENDING\n", 0) != 0) { (void)removeTree(fs, TmpRoot); note = "giornale non valido: installazione annullata"; return true; }
    std::size_t p = 8;
    while (p < journal.size()) {
        std::size_t e = journal.find('\n', p);
        if (e == std::string::npos) e = journal.size();
        const std::string line = journal.substr(p, e - p);
        p = e + 1;
        const std::size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        const std::string lang = line.substr(0, sp), name = line.substr(sp + 1);
        if (!validLanguage(lang) || !validLibName(name)) continue;
        const std::string fin = libraryRoot(lang, name), nw = tmpDir(lang, name), old = oldDir(lang, name);
        files::Entry x;
        const bool haveNew = fs.stat(nw, x) == files::Status::Ok;
        if (!haveNew) continue;                                    // gia' spostata
        (void)fs.makeDirs(std::string(LibRoot) + "/" + langFolder(lang));
        if (fs.stat(fin, x) == files::Status::Ok) {
            (void)removeTree(fs, old);
            const files::Status st = fs.rename(fin, old);
            if (st != files::Status::Ok) { note = "installazione non completata (" + std::string(files::statusMessage(st)) + "): riprova"; return false; }
        }
        const files::Status st = fs.rename(nw, fin);
        if (st != files::Status::Ok) { note = "installazione non completata (" + std::string(files::statusMessage(st)) + "): riprova"; return false; }
    }
    (void)removeTree(fs, TmpRoot);
    note = "installazione completata";
    return true;
}

} // namespace fx::libs
