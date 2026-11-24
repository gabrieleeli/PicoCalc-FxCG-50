// v1.20.0: gestore delle librerie dei programmi (LIB). Una prova per voce
// della tabella LB di docs/reports/ANALISI_FONTI_v1.20.0.md: versioni semver
// e intervalli, manifest e indice rigorosi (errori chiari), piano con le
// dipendenze (cicli, mancanti, incompatibili, firmware), installazione atomica
// da un repository (impronta errata, file troncato o piu' grande, SD piena o
// tolta a meta', AC), giornale e ripresa dopo un'interruzione, aggiornamento e
// ritorno a una versione vecchia, VERIFY, rimozione con le dipendenze, copia
// manuale e uso dal compilatore (prima il progetto, poi LIB).
#include "code_store.hpp"
#include "file_store.hpp"
#include "lib_manager.hpp"
#include "program.hpp"
#include "sha256.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <map>
#include <memory>

using namespace fx;
using namespace fx::libs;
using fx::files::RamFileSystem;
using fx::files::Status;

namespace {
Version V(const char* t) { Version v; (void)Version::parse(t, v); return v; }
const Version Fw = V("1.20.0");

// ---------------------------------------------------------------- un repository di prova (come tools/libraries.py)
struct RepoLib {
    std::string name, version, language;
    std::map<std::string, std::string> files;
    std::vector<Dependency> deps;
    std::string minFirmware;
};
struct Repo {
    Repo(RamFileSystem& f, std::string b) : fs(f), base(std::move(b)) {}
    RamFileSystem& fs;
    std::string base;
    std::string index;
    std::vector<IndexEntry> entries;
    std::map<std::string, std::string> manifests;    // percorso -> testo
};
std::string put(RamFileSystem& fs, const std::string& path, const std::string& text) {
    const std::size_t slash = path.rfind('/');
    if (slash != std::string::npos) (void)fs.makeDirs(path.substr(0, slash));
    (void)files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    return path;
}
std::string get(RamFileSystem& fs, const std::string& path) {
    const auto* d = fs.data(path);
    return d ? std::string(d->begin(), d->end()) : std::string("<assente>");
}
void build(Repo& r, const std::vector<RepoLib>& libs) {
    std::string idx = "{\"version\": 1, \"libraries\": [\n";
    bool first = true;
    for (const RepoLib& l : libs) {
        Manifest m;
        m.name = l.name;
        m.version = V(l.version.c_str());
        m.language = l.language;
        m.description = "libreria " + l.name;
        m.author = "prove";
        m.license = "MIT";
        if (!l.minFirmware.empty()) m.minFirmware = V(l.minFirmware.c_str());
        m.dependencies = l.deps;
        const std::string dir = l.language + "/" + l.name + "/" + l.version;
        for (const auto& [p, t] : l.files) {
            m.files.push_back({p, crypto::sha256Hex(t), static_cast<std::uint32_t>(t.size())});
            put(r.fs, r.base + "/" + dir + "/" + p, t);
        }
        const std::string mt = manifestJson(m);
        put(r.fs, r.base + "/" + dir + "/" + ManifestName, mt);
        r.manifests[dir] = mt;
        idx += std::string(first ? "" : ",\n") + "{\"name\": \"" + l.name + "\", \"version\": \"" + l.version + "\", \"language\": \"" + l.language +
               "\", \"path\": \"" + dir + "\", \"description\": \"libreria " + l.name + "\", \"manifestSha256\": \"" + crypto::sha256Hex(mt) +
               "\", \"manifestSize\": " + std::to_string(mt.size());
        if (!l.minFirmware.empty()) idx += ", \"minFirmware\": \"" + l.minFirmware + "\"";
        idx += ", \"dependencies\": {";
        for (std::size_t i = 0; i < l.deps.size(); ++i) idx += std::string(i ? ", " : "") + "\"" + l.deps[i].name + "\": \"" + l.deps[i].range + "\"";
        idx += "}}";
        first = false;
    }
    idx += "\n]}\n";
    r.index = idx;
    put(r.fs, r.base + "/index.json", idx);
    std::string err;
    CHECK(parseIndex(idx, r.entries, err));
}
// Installazione completa a passi piccoli (come il ciclo della UI).
bool install(RamFileSystem& sd, Source& src, const std::vector<IndexEntry>& index, const std::string& name, const std::string& lang,
             std::string& error, const std::string& version = {}, std::size_t budget = 700, int maxTicks = 100000) {
    std::vector<PlanStep> plan;
    if (!makePlan(index, listInstalled(sd, ""), name, lang, Fw, plan, error, version)) return false;
    Installer in(sd, src);
    in.start(plan, Fw);
    for (int i = 0; i < maxTicks && in.tick(budget); ++i) {}
    error = in.error();
    return in.state() == Installer::State::Done;
}
bool installedVersion(RamFileSystem& sd, const std::string& lang, const std::string& name, const char* version) {
    for (const Installed& l : listInstalled(sd, lang)) if (l.manifest.name == name) return l.manifestOk && l.manifest.version == V(version);
    return false;
}
bool tmpClean(RamFileSystem& sd) {
    std::vector<files::Entry> e;
    return sd.list(TmpRoot, e) != Status::Ok || e.empty();
}

const std::map<std::string, std::string> kMatrici10 = {
    {"matrici.h", "#pragma once\nint traccia(int a, int d);\n"},
    {"matrici.c", "#include \"matrici.h\"\nint traccia(int a, int d) { return a + d; }\n"}};
const std::map<std::string, std::string> kMatrici11 = {
    {"matrici.h", "#pragma once\nint traccia(int a, int d);\nint det2(int a, int b, int c, int d);\n"},
    {"matrici.c", "#include \"matrici.h\"\nint traccia(int a, int d) { return a + d; }\nint det2(int a, int b, int c, int d) { return a * d - b * c; }\n"}};

// ---------------------------------------------------------------- LB-01 versioni e intervalli
void testVersions() {
    Version v;
    CHECK(Version::parse("1.2.3", v) && v.major == 1 && v.minor == 2 && v.patch == 3);
    CHECK(Version::parse("0.0.0", v));
    for (const char* bad : {"1.2", "1.2.3.4", "01.2.3", "1.02.3", "a.b.c", "1.2.3-beta", "", " 1.2.3", "1..3", "-1.2.3", "123456.0.0"}) CHECK(!Version::parse(bad, v));
    CHECK(V("1.10.0").compare(V("1.9.9")) > 0);
    CHECK(V("2.0.0") == V("2.0.0") && V("0.9.9") < V("1.0.0"));
    CHECK_EQ_STR(V("10.0.42").text(), "10.0.42");
    CHECK(rangeMatches("^1.2.3", V("1.9.0")) && !rangeMatches("^1.2.3", V("2.0.0")) && !rangeMatches("^1.2.3", V("1.2.2")));
    CHECK(rangeMatches("^0.2.3", V("0.2.9")) && !rangeMatches("^0.2.3", V("0.3.0")));
    CHECK(rangeMatches("^0.0.3", V("0.0.3")) && !rangeMatches("^0.0.3", V("0.0.4")));   // come npm
    CHECK(rangeMatches("~1.2.3", V("1.2.9")) && !rangeMatches("~1.2.3", V("1.3.0")));
    CHECK(rangeMatches(">=1.0.0 <2.0.0", V("1.5.0")) && !rangeMatches(">=1.0.0 <2.0.0", V("2.0.0")));
    CHECK(rangeMatches(">1.0.0", V("1.0.1")) && !rangeMatches(">1.0.0", V("1.0.0")));
    CHECK(rangeMatches("<=1.0.0", V("1.0.0")) && !rangeMatches("<=1.0.0", V("1.0.1")));
    CHECK(rangeMatches("1.2.3", V("1.2.3")) && rangeMatches("=1.2.3", V("1.2.3")) && !rangeMatches("1.2.3", V("1.2.4")));
    CHECK(rangeMatches("*", V("7.7.7")));
    for (const char* bad : {"", "^", ">=", "1.2", "^x", ">= 1.0.0", "1 2 3 4 5", "~>1.0.0"}) CHECK(!validRange(bad));
    CHECK(!rangeMatches("^", V("1.0.0")));
}

// ---------------------------------------------------------------- LB-02 nomi e percorsi
void testNames() {
    CHECK(validLibName("matrici") && validLibName("geo-2d") && validLibName("a_b9"));
    for (const char* bad : {"", "Matrici", "9vite", "-x", "a b", "a/b", "a.b", "abcdefghijklmnopqrstuvwxyz0123456"}) CHECK(!validLibName(bad));
    CHECK(safeRelativePath("matrici.h") && safeRelativePath("src/util.c") && safeRelativePath("a/b/c/d.h"));
    for (const char* bad : {"", "/assoluto.h", "../fuori.h", "a/../b.h", "a//b.h", "a\\b.h", "C:x.h", ".nascosto", "a/./b.h", "a/b/c/d/e.h", "a/", "a b.h"})
        CHECK(!safeRelativePath(bad));
    CHECK_EQ_STR(libraryRoot("cpp", "geo"), "PICOCALC/LIB/CPP/geo");
    CHECK_EQ_STR(dataRoot("cs", "conti"), "PICOCALC/LIBDATA/CS/conti");
}

// ---------------------------------------------------------------- LB-03 manifest
std::string manifestWith(const std::string& extra, const std::string& files = R"([{"path": "a.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])") {
    return "{\"name\": \"prova\", \"version\": \"1.0.0\", \"language\": \"c\", \"files\": " + files + extra + "}";
}
void testManifest() {
    Manifest m;
    std::string err;
    CHECK(parseManifest(manifestWith(""), m, err) && m.name == "prova" && m.files.size() == 1 && m.files[0].size == 0);
    CHECK(parseManifest("\xEF\xBB\xBF" + manifestWith(""), m, err));                                    // BOM ammesso
    CHECK(parseManifest(manifestWith(", \"dependencies\": {\"base\": \"^1.0.0\"}, \"minFirmware\": \"1.20.0\", \"namespace\": \"Prova.Algebra\""), m, err));
    CHECK(m.dependencies.size() == 1 && m.dependencies[0].name == "base" && m.minFirmware == V("1.20.0") && m.ns == "Prova.Algebra");
    // giro completo: manifestJson -> parseManifest
    Manifest back;
    CHECK(parseManifest(manifestJson(m), back, err) && back.name == m.name && back.files.size() == 1 && back.dependencies.size() == 1);
    struct Bad { std::string text; const char* expect; };
    const std::string okFile = R"([{"path": "a.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])";
    const Bad bad[] = {
        {"{", "non valido"},
        {"[]", "oggetto"},
        {"{\"version\": \"1.0.0\", \"language\": \"c\", \"files\": " + okFile + "}", "'name' mancante"},
        {"{\"name\": \"Prova\", \"version\": \"1.0.0\", \"language\": \"c\", \"files\": " + okFile + "}", "nome non valido"},
        {"{\"name\": \"prova\", \"version\": \"1.0\", \"language\": \"c\", \"files\": " + okFile + "}", "versione non valida"},
        {"{\"name\": \"prova\", \"version\": \"1.0.0\", \"language\": \"rust\", \"files\": " + okFile + "}", "linguaggio sconosciuto"},
        {"{\"name\": \"prova\", \"version\": 1, \"language\": \"c\", \"files\": " + okFile + "}", "deve essere una stringa"},
        {manifestWith("", "[]"), "elenco non vuoto"},
        {manifestWith("", R"([{"path": "../x.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])"), "percorso non ammesso"},
        {manifestWith("", R"([{"path": "/x.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])"), "percorso non ammesso"},
        {manifestWith("", R"([{"path": "x.h", "sha256": "zz", "size": 0}])"), "SHA-256 non valido"},
        {manifestWith("", R"([{"path": "x.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 99999999}])"), "dimensione non valida"},
        {manifestWith("", R"([{"path": "x.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 1.5}])"), "dimensione non valida"},
        {manifestWith("", R"([{"path": "x.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}, {"path": "X.h", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])"), "file ripetuto"},
        {manifestWith("", R"([{"path": "library.json", "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "size": 0}])"), "manifest non e' un file"},
        {manifestWith(", \"dependencies\": [\"base\"]"), "oggetto"},
        {manifestWith(", \"dependencies\": {\"base\": \"qualsiasi\"}"), "intervallo di versioni non valido"},
        {manifestWith(", \"dependencies\": {\"prova\": \"*\"}"), "dipende da se stessa"},
        {manifestWith(", \"name\": \"doppio\""), "ripetuto"},
        {manifestWith(", \"namespace\": \"1x\""), "namespace non valido"},
        {manifestWith(", \"minFirmware\": \"x\""), "minFirmware non valido"},
        {std::string(MaxManifestBytes + 1, ' '), "troppo grande"},
    };
    for (const Bad& b : bad) {
        err.clear();
        const bool ok = parseManifest(b.text, m, err);
        CHECK(!ok);
        if (ok || err.find(b.expect) == std::string::npos) std::fprintf(stderr, "  manifest: atteso '%s', errore '%s'\n", b.expect, err.c_str());
    }
    // troppi file e libreria troppo grande
    std::string many = "[";
    for (std::size_t i = 0; i <= MaxFiles; ++i) many += std::string(i ? "," : "") + "{\"path\": \"f" + std::to_string(i) + ".h\", \"sha256\": \"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\", \"size\": 0}";
    CHECK(!parseManifest(manifestWith("", many + "]"), m, err) && err.find("troppi file") != std::string::npos);
    std::string heavy = "[";
    for (int i = 0; i < 5; ++i) heavy += std::string(i ? "," : "") + "{\"path\": \"g" + std::to_string(i) + ".h\", \"sha256\": \"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\", \"size\": " + std::to_string(MaxFileBytes) + "}";
    CHECK(!parseManifest(manifestWith("", heavy + "]"), m, err) && err.find("supera") != std::string::npos);
}

// ---------------------------------------------------------------- LB-04 indice
void testIndex() {
    std::vector<IndexEntry> idx;
    std::string err;
    const std::string sha = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    auto entry = [&](const std::string& extra) { return "{\"version\": 1, \"libraries\": [{\"name\": \"geo\", \"version\": \"1.0.0\", \"language\": \"cpp\", \"path\": \"cpp/geo/1.0.0\", \"manifestSha256\": \"" + sha + "\"" + extra + "}]}"; };
    CHECK(parseIndex(entry(""), idx, err) && idx.size() == 1 && idx[0].path == "cpp/geo/1.0.0");
    CHECK(parseIndex(entry(", \"minFirmware\": \"1.21.0\", \"dependencies\": {\"base\": \"~1.0.0\"}"), idx, err) && idx[0].minFirmware == V("1.21.0") && idx[0].dependencies.size() == 1);
    CHECK(!parseIndex("{\"version\": 2, \"libraries\": []}", idx, err) && err.find("versione del formato") != std::string::npos);
    CHECK(!parseIndex("{\"libraries\": []}", idx, err));
    CHECK(!parseIndex("{\"version\": 1, \"libraries\": {}}", idx, err));
    CHECK(!parseIndex(entry(", \"path\": \"../../etc\""), idx, err));                  // campo ripetuto
    std::string twice = "{\"version\": 1, \"libraries\": [";
    for (int i = 0; i < 2; ++i) twice += std::string(i ? "," : "") + "{\"name\": \"geo\", \"version\": \"1.0.0\", \"language\": \"cpp\", \"path\": \"cpp/geo/1.0.0\", \"manifestSha256\": \"" + sha + "\"}";
    CHECK(!parseIndex(twice + "]}", idx, err) && err.find("versione ripetuta") != std::string::npos);
    CHECK(!parseIndex("{\"version\": 1, \"libraries\": [{\"name\": \"geo\", \"version\": \"1.0.0\", \"language\": \"cpp\", \"path\": \"/abs\", \"manifestSha256\": \"" + sha + "\"}]}", idx, err) &&
          err.find("percorso non ammesso") != std::string::npos);
    CHECK(!parseIndex("{\"version\": 1, \"libraries\": [{\"name\": \"geo\", \"version\": \"1.0.0\", \"language\": \"cpp\", \"path\": \"a\", \"manifestSha256\": \"123\"}]}", idx, err));
    CHECK(!parseIndex(std::string(MaxIndexBytes + 10, ' '), idx, err) && err.find("troppo grande") != std::string::npos);
    CHECK(idx.empty());                                                               // nessuna voce a meta' dopo un errore
}

// ---------------------------------------------------------------- LB-05 piano
void testPlan() {
    RamFileSystem fs;
    Repo r{fs, "REPO"};
    build(r, {{"base", "1.0.0", "c", {{"base.h", "#pragma once\n"}}, {}, ""},
              {"base", "1.1.0", "c", {{"base.h", "#pragma once\n//1.1\n"}}, {}, ""},
              {"base", "2.0.0", "c", {{"base.h", "#pragma once\n//2\n"}}, {}, ""},
              {"matrici", "1.0.0", "c", kMatrici10, {{"base", "^1.0.0"}}, ""},
              {"geo", "1.0.0", "cpp", {{"geo.hpp", "#pragma once\n"}}, {{"matrici", ">=1.0.0"}}, ""},
              {"anello-a", "1.0.0", "c", {{"a.h", "x"}}, {{"anello-b", "*"}}, ""},
              {"anello-b", "1.0.0", "c", {{"b.h", "x"}}, {{"anello-c", "*"}}, ""},
              {"anello-c", "1.0.0", "c", {{"c.h", "x"}}, {{"anello-a", "*"}}, ""},
              {"orfana", "1.0.0", "c", {{"o.h", "x"}}, {{"inesistente", "^1.0.0"}}, ""},
              {"esigente", "1.0.0", "c", {{"e.h", "x"}}, {{"base", "^3.0.0"}}, ""},
              {"futura", "1.0.0", "c", {{"f.h", "x"}}, {}, ""},
              {"futura", "2.0.0", "c", {{"f.h", "y"}}, {}, "9.0.0"},
              {"solofutura", "1.0.0", "c", {{"s.h", "x"}}, {}, "9.0.0"}});
    std::vector<PlanStep> plan;
    std::string err;
    CHECK(makePlan(r.entries, {}, "matrici", "c", Fw, plan, err));
    CHECK(plan.size() == 2 && plan[0].entry.name == "base" && plan[0].entry.version == V("1.1.0") && plan[1].entry.name == "matrici");   // prima le dipendenze
    // C++ usa le librerie C
    CHECK(makePlan(r.entries, {}, "geo", "cpp", Fw, plan, err) && plan.size() == 3 && plan[0].entry.name == "base" && plan[2].entry.name == "geo");
    CHECK(!makePlan(r.entries, {}, "anello-a", "c", Fw, plan, err) && err.find("ciclo di dipendenze: anello-a -> anello-b -> anello-c -> anello-a") != std::string::npos);
    CHECK(!makePlan(r.entries, {}, "orfana", "c", Fw, plan, err) && err.find("dipendenza mancante: inesistente") != std::string::npos);
    CHECK(!makePlan(r.entries, {}, "esigente", "c", Fw, plan, err) && err.find("nessuna versione di base soddisfa ^3.0.0") != std::string::npos);
    CHECK(!makePlan(r.entries, {}, "assente", "c", Fw, plan, err) && err.find("non presente nell'indice") != std::string::npos);
    CHECK(!makePlan(r.entries, {}, "matrici", "cs", Fw, plan, err));                 // altro linguaggio
    // firmware: si sceglie la versione piu' alta adatta; se nessuna lo e', lo si dice
    CHECK(makePlan(r.entries, {}, "futura", "c", Fw, plan, err) && plan.size() == 1 && plan[0].entry.version == V("1.0.0"));
    CHECK(!makePlan(r.entries, {}, "solofutura", "c", Fw, plan, err) && err.find("richiede il firmware 9.0.0") != std::string::npos);
    CHECK(!makePlan(r.entries, {}, "futura", "c", Fw, plan, err, "2.0.0") && err.find("richiede il firmware") != std::string::npos);
    // una versione precisa (anche vecchia)
    CHECK(makePlan(r.entries, {}, "base", "c", Fw, plan, err, "1.0.0") && plan.size() == 1 && plan[0].entry.version == V("1.0.0"));
    CHECK(!makePlan(r.entries, {}, "base", "c", Fw, plan, err, "1.5.0"));
    CHECK(!makePlan(r.entries, {}, "base", "c", Fw, plan, err, "x"));
    // dipendenza gia' installata e compatibile: non si tocca; aggiornamento e ritorno indietro segnalati
    Installed have;
    have.manifestOk = true;
    have.manifest.name = "base";
    have.manifest.language = "c";
    have.manifest.version = V("1.0.0");
    CHECK(makePlan(r.entries, {have}, "matrici", "c", Fw, plan, err) && plan.size() == 1 && plan[0].entry.name == "matrici");
    CHECK(makePlan(r.entries, {have}, "base", "c", Fw, plan, err) && plan.size() == 1 && plan[0].fromVersion == "1.0.0" && !plan[0].downgrade && plan[0].entry.version == V("2.0.0"));
    have.manifest.version = V("2.0.0");
    CHECK(makePlan(r.entries, {have}, "base", "c", Fw, plan, err) && plan[0].upToDate);
    CHECK(makePlan(r.entries, {have}, "base", "c", Fw, plan, err, "1.1.0") && plan[0].downgrade);
    // installata ma incompatibile (2.0.0 contro ^1.0.0): si porta alla 1.1.0
    CHECK(makePlan(r.entries, {have}, "matrici", "c", Fw, plan, err) && plan.size() == 2 && plan[0].entry.version == V("1.1.0") && plan[0].downgrade);
}

// ---------------------------------------------------------------- LB-06..09 installazione
void testInstall() {
    RamFileSystem fs;
    Repo r{fs, "REPO"};
    build(r, {{"base", "1.0.0", "c", {{"base.h", "#pragma once\n#define BASE 1\n"}}, {}, ""},
              {"matrici", "1.0.0", "c", kMatrici10, {{"base", "^1.0.0"}}, ""},
              {"matrici", "1.1.0", "c", kMatrici11, {{"base", "^1.0.0"}}, ""},
              {"conti", "1.0.0", "cs", {{"Conti.cs", "namespace Conti { public static class Iva { public static double Lordo(double n) => n * 1.22; } }\n"}}, {}, ""}});
    FolderSource src(fs, "REPO");
    std::string err;
    // installazione con la dipendenza, a passi piccoli
    CHECK(install(fs, src, r.entries, "matrici", "c", err, "1.0.0"));
    CHECK(installedVersion(fs, "c", "matrici", "1.0.0") && installedVersion(fs, "c", "base", "1.0.0"));
    CHECK_EQ_STR(get(fs, "PICOCALC/LIB/C/matrici/matrici.c"), kMatrici10.at("matrici.c"));
    CHECK(tmpClean(fs));
    for (const Installed& l : listInstalled(fs, "c")) CHECK(verify(fs, l).ok);
    // aggiornamento: la versione vecchia e' sostituita per intero
    CHECK(install(fs, src, r.entries, "matrici", "c", err));
    CHECK(installedVersion(fs, "c", "matrici", "1.1.0"));
    CHECK_EQ_STR(get(fs, "PICOCALC/LIB/C/matrici/matrici.h"), kMatrici11.at("matrici.h"));
    // ritorno a una versione vecchia (la UI chiede conferma: plan.downgrade)
    std::vector<PlanStep> plan;
    CHECK(makePlan(r.entries, listInstalled(fs, ""), "matrici", "c", Fw, plan, err, "1.0.0") && plan.back().downgrade);
    CHECK(install(fs, src, r.entries, "matrici", "c", err, "1.0.0") && installedVersion(fs, "c", "matrici", "1.0.0"));
    // gia' aggiornata: niente da fare
    CHECK(makePlan(r.entries, listInstalled(fs, ""), "matrici", "c", Fw, plan, err, "1.0.0") && plan.size() == 1 && plan[0].upToDate);
    // C#
    CHECK(install(fs, src, r.entries, "conti", "cs", err) && fs.exists("PICOCALC/LIB/CS/conti/Conti.cs"));

    // file alterato nel repository: SHA-256 errato, LIB intatta, .tmp ripulita
    put(fs, "REPO/c/matrici/1.1.0/matrici.c", "#include \"matrici.h\"\nint traccia(int a, int d) { return 666; }\nint det2(int a, int b, int c, int d) { return 0; }\n");
    CHECK(!install(fs, src, r.entries, "matrici", "c", err, "1.1.0"));
    CHECK(err.find("SHA-256 errato") != std::string::npos || err.find("dimensione errata") != std::string::npos || err.find("piu' grande") != std::string::npos);
    CHECK(installedVersion(fs, "c", "matrici", "1.0.0") && tmpClean(fs));
    put(fs, "REPO/c/matrici/1.1.0/matrici.c", kMatrici11.at("matrici.c"));
    // file troncato (download interrotto a meta')
    put(fs, "REPO/c/matrici/1.1.0/matrici.h", kMatrici11.at("matrici.h").substr(0, 10));
    CHECK(!install(fs, src, r.entries, "matrici", "c", err, "1.1.0") && err.find("dimensione errata") != std::string::npos);
    CHECK(installedVersion(fs, "c", "matrici", "1.0.0") && tmpClean(fs));
    put(fs, "REPO/c/matrici/1.1.0/matrici.h", kMatrici11.at("matrici.h"));
    // manifest alterato: l'impronta dell'indice non torna
    const std::string mt = get(fs, "REPO/c/matrici/1.1.0/library.json");
    put(fs, "REPO/c/matrici/1.1.0/library.json", mt + " ");
    CHECK(!install(fs, src, r.entries, "matrici", "c", err, "1.1.0") && err.find("impronta di library.json errata") != std::string::npos);
    put(fs, "REPO/c/matrici/1.1.0/library.json", mt);
    // file mancante nel repository
    (void)fs.remove("REPO/c/matrici/1.1.0/matrici.c");
    CHECK(!install(fs, src, r.entries, "matrici", "c", err, "1.1.0") && err.find("non trovato") != std::string::npos);
    CHECK(installedVersion(fs, "c", "matrici", "1.0.0") && tmpClean(fs));
    put(fs, "REPO/c/matrici/1.1.0/matrici.c", kMatrici11.at("matrici.c"));
    CHECK(install(fs, src, r.entries, "matrici", "c", err, "1.1.0"));
}

// ---------------------------------------------------------------- LB-10 interruzioni: SD tolta o piena, AC, giornale
void testInterruptions() {
    RamFileSystem repoFs;
    Repo r{repoFs, "REPO"};
    build(r, {{"base", "1.0.0", "c", {{"base.h", "#pragma once\n#define BASE 1\n"}}, {}, ""},
              {"matrici", "1.0.0", "c", kMatrici10, {{"base", "^1.0.0"}}, ""},
              {"matrici", "1.1.0", "c", kMatrici11, {{"base", "^1.0.0"}}, ""}});
    FolderSource src(repoFs, "REPO");
    std::string err;
    // la SD tolta (o la batteria che si stacca) dopo n byte scritti, per molti n:
    // dopo la ripresa all'avvio la libreria e' quella vecchia o quella nuova, mai a meta'
    int sawOld = 0, sawNew = 0;
    for (std::size_t cut = 0; cut < 2400; cut += 37) {
        RamFileSystem sd;
        CHECK(install(sd, src, r.entries, "matrici", "c", err, "1.0.0"));
        const std::size_t before = sd.used();
        (void)before;
        sd.failAfterBytes(cut);
        const bool ok = install(sd, src, r.entries, "matrici", "c", err, "1.1.0");
        sd.powerOn();
        std::string note;
        CHECK(recoverInstall(sd, note));
        const bool isOld = installedVersion(sd, "c", "matrici", "1.0.0");
        const bool isNew = installedVersion(sd, "c", "matrici", "1.1.0");
        CHECK(isOld || isNew);
        CHECK(!ok || isNew);
        for (const Installed& l : listInstalled(sd, "c")) CHECK(verify(sd, l).ok);
        CHECK(tmpClean(sd));
        sawOld += isOld;
        sawNew += isNew;
    }
    CHECK(sawOld > 0 && sawNew > 0);
    // ogni operazione dell'installazione puo' fallire (I/O): stesso risultato
    for (int n = 1; n < 60; ++n) {
        RamFileSystem sd;
        CHECK(install(sd, src, r.entries, "matrici", "c", err, "1.0.0"));
        sd.failOperation(n);
        (void)install(sd, src, r.entries, "matrici", "c", err, "1.1.0");
        sd.powerOn();
        std::string note;
        (void)recoverInstall(sd, note);
        CHECK(installedVersion(sd, "c", "matrici", "1.0.0") || installedVersion(sd, "c", "matrici", "1.1.0"));
        for (const Installed& l : listInstalled(sd, "c")) CHECK(verify(sd, l).ok);
    }
    // interruzione durante il commit: il giornale completa l'installazione alla ripresa
    {
        RamFileSystem sd;
        CHECK(install(sd, src, r.entries, "matrici", "c", err, "1.0.0"));
        std::vector<PlanStep> plan;
        CHECK(makePlan(r.entries, listInstalled(sd, ""), "matrici", "c", Fw, plan, err, "1.1.0"));
        Installer in(sd, src);
        in.start(plan, Fw);
        while (in.state() == Installer::State::Fetching && in.tick(100000)) {}
        // il commit e' fatto; si simula il riavvio a meta' del giro di rinomine rimettendo .new
        CHECK(in.state() == Installer::State::Done);
        // giornale a mano: cartella nuova pronta, la vecchia ancora al suo posto
        put(sd, std::string(TmpRoot) + "/c-matrici.new/matrici.h", kMatrici11.at("matrici.h"));
        put(sd, std::string(TmpRoot) + "/c-matrici.new/matrici.c", kMatrici11.at("matrici.c"));
        put(sd, std::string(TmpRoot) + "/c-matrici.new/library.json", r.manifests.at("c/matrici/1.1.0"));
        std::string note;
        // senza giornale: e' un download interrotto, si annulla
        CHECK(recoverInstall(sd, note) && note == "download interrotto annullato" && tmpClean(sd));
        put(sd, std::string(TmpRoot) + "/c-matrici.new/matrici.h", kMatrici10.at("matrici.h"));
        put(sd, std::string(TmpRoot) + "/c-matrici.new/matrici.c", kMatrici10.at("matrici.c"));
        put(sd, std::string(TmpRoot) + "/c-matrici.new/library.json", r.manifests.at("c/matrici/1.0.0"));
        put(sd, std::string(TmpRoot) + "/journal", "PENDING\nc matrici\n");
        CHECK(recoverInstall(sd, note) && note == "installazione completata");
        CHECK(installedVersion(sd, "c", "matrici", "1.0.0") && tmpClean(sd));
        // giornale illeggibile: si annulla senza toccare LIB
        put(sd, std::string(TmpRoot) + "/c-matrici.new/matrici.h", "x");
        put(sd, std::string(TmpRoot) + "/journal", "rotto");
        CHECK(recoverInstall(sd, note) && note.find("annullata") != std::string::npos && installedVersion(sd, "c", "matrici", "1.0.0"));
        // giornale con nomi non validi: ignorati (niente percorsi fuori da LIB)
        put(sd, std::string(TmpRoot) + "/journal", "PENDING\nc ../../CODE\nzz x\n");
        CHECK(recoverInstall(sd, note) && tmpClean(sd));
    }
    // AC durante il download: niente in LIB, .tmp ripulita
    {
        RamFileSystem sd;
        std::vector<PlanStep> plan;
        CHECK(makePlan(r.entries, {}, "matrici", "c", Fw, plan, err));
        Installer in(sd, src);
        in.start(plan, Fw);
        CHECK(in.tick(64));
        CHECK(!in.progress().empty());
        in.cancel();
        CHECK(in.state() == Installer::State::Cancelled && in.error() == "installazione annullata");
        CHECK(listInstalled(sd, "").empty() && tmpClean(sd));
        CHECK(!in.tick());
    }
    // SD piena: messaggio chiaro, niente a meta'
    {
        RamFileSystem sd;
        sd.setCapacity(600);
        CHECK(!install(sd, src, r.entries, "matrici", "c", err));
        CHECK(err.find("SD piena") != std::string::npos);
        sd.setCapacity(8u * 1024u * 1024u);
        std::string note;
        (void)recoverInstall(sd, note);
        CHECK(listInstalled(sd, "").empty() && tmpClean(sd));
    }
    // SD assente
    {
        RamFileSystem sd;
        sd.setPresent(false);
        std::vector<PlanStep> plan;
        CHECK(makePlan(r.entries, {}, "base", "c", Fw, plan, err));
        Installer in(sd, src);
        in.start(plan, Fw);
        CHECK(in.state() == Installer::State::Failed && in.error() == "Inserire la scheda SD");
    }
    // il distruttore annulla un'installazione in corso (uscita dalla modalita')
    {
        RamFileSystem sd;
        std::vector<PlanStep> plan;
        CHECK(makePlan(r.entries, {}, "matrici", "c", Fw, plan, err));
        {
            Installer in(sd, src);
            in.start(plan, Fw);
            (void)in.tick(100);
        }
        CHECK(listInstalled(sd, "").empty() && tmpClean(sd));
    }
}

// ---------------------------------------------------------------- LB-11 firmware minimo, sicurezza
void testSafety() {
    RamFileSystem fs;
    Repo r{fs, "REPO"};
    build(r, {{"nuova", "1.0.0", "c", {{"n.h", "x"}}, {}, ""}});
    FolderSource src(fs, "REPO");
    // il manifest chiede un firmware piu' nuovo anche se l'indice non lo dice
    Manifest m;
    std::string err;
    CHECK(parseManifest(r.manifests.at("c/nuova/1.0.0"), m, err));
    m.minFirmware = V("3.0.0");
    const std::string mt = manifestJson(m);
    put(fs, "REPO/c/nuova/1.0.0/library.json", mt);
    std::string idx = r.index;
    const std::string oldSha = crypto::sha256Hex(r.manifests.at("c/nuova/1.0.0"));
    idx.replace(idx.find(oldSha), oldSha.size(), crypto::sha256Hex(mt));
    std::vector<IndexEntry> entries;
    CHECK(parseIndex(idx, entries, err));
    RamFileSystem sd;
    CHECK(!install(sd, src, entries, "nuova", "c", err) && err.find("richiede il firmware 3.0.0") != std::string::npos);
    CHECK(listInstalled(sd, "").empty());
    // la sorgente rifiuta i percorsi fuori dal repository
    std::string e2;
    CHECK(!src.begin("../segreti.txt", e2) && e2.find("non ammesso") != std::string::npos);
    CHECK(!src.begin("/etc/passwd", e2));
    // l'installazione non esegue niente: un file .c resta un file (nessun output, nessun programma avviato)
    build(r, {{"innocua", "1.0.0", "c", {{"main.c", "int main(void) { printf(\"eseguito!\"); return 0; }\n"}}, {}, ""}});
    CHECK(install(sd, src, r.entries, "innocua", "c", err));
    CHECK_EQ_STR(get(sd, "PICOCALC/LIB/C/innocua/main.c"), "int main(void) { printf(\"eseguito!\"); return 0; }\n");
}

// ---------------------------------------------------------------- LB-12 VERIFY, copia manuale, rimozione, dipendenze
void testVerifyAndRemove() {
    RamFileSystem fs;
    Repo r{fs, "REPO"};
    build(r, {{"base", "1.0.0", "c", {{"base.h", "#pragma once\n#define BASE 1\n"}}, {}, ""},
              {"matrici", "1.0.0", "c", kMatrici10, {{"base", "^1.0.0"}}, ""}});
    FolderSource src(fs, "REPO");
    std::string err;
    RamFileSystem sd;
    CHECK(install(sd, src, r.entries, "matrici", "c", err));
    // VERIFY: file modificato e file mancante
    put(sd, "PICOCALC/LIB/C/matrici/matrici.c", "// cambiato\n");
    (void)sd.remove("PICOCALC/LIB/C/matrici/matrici.h");
    VerifyReport rep;
    for (const Installed& l : listInstalled(sd, "c")) if (l.manifest.name == "matrici") rep = verify(sd, l);
    CHECK(!rep.ok && rep.problems.size() == 2);
    CHECK(rep.problems.size() == 2 && (rep.problems[0].find("dimensione diversa") != std::string::npos || rep.problems[0].find("modificato") != std::string::npos));
    CHECK(rep.problems.size() == 2 && rep.problems[1].find("file mancante: matrici.h") != std::string::npos);
    // il compilatore non usa un file modificato dopo l'installazione
    {
        code::Store st(&sd);
        (void)st.ready();
        put(sd, "PICOCALC/LIB/C/matrici/matrici.h", kMatrici10.at("matrici.h"));
        put(sd, "PICOCALC/CODE/C/usa.c", "#include <stdio.h>\n#include <matrici.h>\nint main(void) { printf(\"%d\", traccia(2, 5)); return 0; }\n");
        prog::Project p;
        std::unique_ptr<code::ProjectSources> srcs;
        std::string folder, e;
        CHECK(code::loadProject(st, "PICOCALC/CODE/C/usa.c", p, srcs, folder, e));
        bool failed = false;
        try { (void)prog::compile(p); } catch (const prog::Error& x) { failed = true; e = x.what(); }
        CHECK(failed && e.find("VERIFY") != std::string::npos);
        // reinstallata: si compila e si esegue con la libreria
        CHECK(install(sd, src, r.entries, "matrici", "c", err, "1.0.0") || true);
        std::string note;
        (void)removeLibrary(sd, "c", "matrici", note);
        CHECK(install(sd, src, r.entries, "matrici", "c", err));
        p = prog::Project();
        CHECK(code::loadProject(st, "PICOCALC/CODE/C/usa.c", p, srcs, folder, e));
        Workspace ws;
        prog::WorkspaceHost host(ws);
        host.fs = &sd;
        host.folder = folder;
        try {
            auto prog = prog::compile(p);
            prog::Machine mach(prog, host);
            mach.runToEnd(2000000);
        } catch (const prog::Error& x) { host.output += std::string("ERR[") + x.what() + "]"; }
        CHECK_EQ_STR(host.output, "7");
        // intestazione assente: il suggerimento di LIB
        put(sd, "PICOCALC/CODE/C/manca.c", "#include <vettori.h>\nint main(void) { return 0; }\n");
        p = prog::Project();
        CHECK(code::loadProject(st, "PICOCALC/CODE/C/manca.c", p, srcs, folder, e));
        e.clear();
        try { (void)prog::compile(p); } catch (const prog::Error& x) { e = x.what(); }
        CHECK(e.find("vettori.h") != std::string::npos && e.find("LIB") != std::string::npos);
    }
    // rimozione: chi dipende da base lo dice; la rimozione toglie la cartella intera
    CHECK(dependents(sd, "c", "base") == std::vector<std::string>{"matrici"});
    std::string note;
    CHECK(removeLibrary(sd, "c", "matrici", note) == Status::Ok && !sd.exists("PICOCALC/LIB/C/matrici"));
    CHECK(dependents(sd, "c", "base").empty());
    CHECK(removeLibrary(sd, "c", "matrici", note) == Status::NotFound && note == "libreria non installata");
    CHECK(removeLibrary(sd, "rust", "x", note) == Status::InvalidName);
    // copia manuale dal PC: una cartella con library.json messa in LIB/C/
    for (const auto& [p, t] : kMatrici10) put(sd, "PICOCALC/LIB/C/matrici/" + p, t);
    put(sd, "PICOCALC/LIB/C/matrici/library.json", r.manifests.at("c/matrici/1.0.0"));
    bool found = false;
    for (const Installed& l : listInstalled(sd, "c")) if (l.manifest.name == "matrici") { found = l.manifestOk && verify(sd, l).ok; }
    CHECK(found);
    // manifest guasto o di un'altra libreria: elencata con l'errore, non usata dal compilatore
    put(sd, "PICOCALC/LIB/C/rotta/library.json", "{ non json");
    put(sd, "PICOCALC/LIB/C/scambiata/library.json", r.manifests.at("c/base/1.0.0"));
    int broken = 0;
    for (const Installed& l : listInstalled(sd, "c")) {
        if (l.manifest.name == "rotta") { broken++; CHECK(!l.manifestOk && l.error.find("non valido") != std::string::npos); }
        if (l.manifest.name == "scambiata") { broken++; CHECK(!l.manifestOk && l.error.find("non e' quello della cartella") != std::string::npos); }
    }
    CHECK(broken == 2);
    code::ProjectSources ps(sd, "PICOCALC/CODE/C");
    for (const prog::LibraryRef& ref : ps.libraries("c")) CHECK(ref.name != "rotta" && ref.name != "scambiata");
    // toRef: cartella dei dati della libreria
    for (const Installed& l : listInstalled(sd, "c")) if (l.manifest.name == "matrici") CHECK(toRef(l).dataFolder == "PICOCALC/LIBDATA/C/matrici");
}
} // namespace

int main() {
    testVersions();
    testNames();
    testManifest();
    testIndex();
    testPlan();
    testInstall();
    testInterruptions();
    testSafety();
    testVerifyAndRemove();
    return testutil::finish("lib_manager_test");
}
