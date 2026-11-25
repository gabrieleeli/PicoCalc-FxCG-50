// v1.20.0: corpus di Program confrontato con i compilatori reali. Ogni
// programma di tests/program_corpus/{c,cpp,cs} (con l'input <nome>.in) gira
// nell'interprete su una SD in RAM vuota; l'uscita deve essere identica, byte
// per byte, a <nome>.out prodotta da gcc, g++ e mcs/mono con
// tools/program_corpus_ref.py. I programmi coprono l'uso comune delle tabelle
// LC/LP/LS di docs/reports/ANALISI_FONTI_v1.20.0.md e sono anche semi dei
// fuzzer.
#include "file_store.hpp"
#include "program.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace fx;
using namespace fx::prog;
namespace fs = std::filesystem;

namespace {
std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
std::string firstDifference(const std::string& got, const std::string& want) {
    std::size_t i = 0;
    while (i < got.size() && i < want.size() && got[i] == want[i]) ++i;
    const std::size_t line = static_cast<std::size_t>(std::count(want.begin(), want.begin() + static_cast<std::ptrdiff_t>(i), '\n')) + 1;
    const std::size_t from = want.rfind('\n', i == 0 ? 0 : i - 1);
    const std::size_t start = from == std::string::npos ? 0 : from + 1;
    auto cut = [&](const std::string& s) { const std::size_t e = s.find('\n', start); return s.substr(start, e == std::string::npos ? std::string::npos : e - start); };
    return "riga " + std::to_string(line) + ": atteso '" + cut(want) + "', ottenuto '" + (start <= got.size() ? cut(got) : std::string()) + "'";
}
void runOne(const fs::path& src, Dialect d) {
    files::RamFileSystem card(4u << 20);
    Workspace ws;
    WorkspaceHost host(ws);
    host.fs = &card;
    host.folder = "PICOCALC/CODE/CORPUS";
    (void)card.makeDirs(host.folder);
    fs::path in = src;
    in.replace_extension(".in");
    if (fs::exists(in)) {
        const std::string text = slurp(in);
        std::size_t p = 0;
        while (p < text.size()) {
            std::size_t e = text.find('\n', p);
            if (e == std::string::npos) e = text.size();
            host.input.push_back(text.substr(p, e - p));
            p = e + 1;
        }
    }
    host.endOfInput = true;
    fs::path outPath = src;
    outPath.replace_extension(".out");
    const std::string want = slurp(outPath);
    std::string error;
    try {
        Project pr;
        pr.dialect = d;
        pr.files.push_back({src.filename().string(), slurp(src), -1});
        pr.entry = src.filename().string();
        Machine m(compile(pr), host);
        m.runToEnd(200000000);
        if (m.state() != Machine::State::Done) error = "non finito";
        else if (m.exitCode() != 0) error = "codice d'uscita " + std::to_string(m.exitCode());
    } catch (const Error& e) {
        error = std::string(e.what()) + " @" + std::to_string(e.line) + ":" + std::to_string(e.column);
    } catch (const std::exception& e) {
        error = std::string("eccezione: ") + e.what();
    }
    const bool ok = error.empty() && host.output == want;
    ::testutil::report(ok, src.filename().string().c_str(), __FILE__, __LINE__,
                       error.empty() ? (ok ? std::string() : firstDifference(host.output, want)) : error);
}
} // namespace

int main() {
    const fs::path root(FXAI_CORPUS_DIR);
    const std::pair<const char*, Dialect> langs[] = {{"c", Dialect::C}, {"cpp", Dialect::Cpp}, {"cs", Dialect::CSharp}};
    int programs = 0;
    for (const auto& [dir, d] : langs) {
        std::vector<fs::path> srcs;
        for (const auto& e : fs::directory_iterator(root / dir))
            if (e.path().extension() == std::string(".") + dir) srcs.push_back(e.path());
        std::sort(srcs.begin(), srcs.end());
        for (const auto& s : srcs) { runOne(s, d); ++programs; }
    }
    CHECK(programs >= 30);
    return testutil::finish("program_corpus_test");
}
