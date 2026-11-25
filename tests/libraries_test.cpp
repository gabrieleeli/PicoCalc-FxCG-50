// v1.20.0: le librerie pubblicate nella cartella libraries/ del repository.
// Il repository vero (index.json, manifest con le impronte di
// tools/libraries.py) si copia su una SD di prova; ogni libreria si installa
// con LIB (impronte, dipendenze, giornale) e si usa da un programma d'esempio
// che si compila ed esegue con l'interprete. I risultati attesi sono quelli
// di gcc, g++ e mono per gli stessi programmi con gli stessi file.
#include "code_store.hpp"
#include "file_store.hpp"
#include "lib_manager.hpp"
#include "program.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace fx;
using namespace fx::libs;
using fx::files::RamFileSystem;

namespace {
const char* const kRepo = FXAI_LIBRARIES_DIR;

void put(RamFileSystem& fs, const std::string& path, const std::string& text) {
    const std::size_t slash = path.rfind('/');
    if (slash != std::string::npos) (void)fs.makeDirs(path.substr(0, slash));
    (void)files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}
int copyRepo(RamFileSystem& fs) {
    int n = 0;
    for (const auto& e : std::filesystem::recursive_directory_iterator(kRepo)) {
        if (!e.is_regular_file()) continue;
        std::ifstream in(e.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        put(fs, "REPO/" + std::filesystem::relative(e.path(), kRepo).generic_string(), text);
        ++n;
    }
    return n;
}
std::string run(code::Store& st, const std::string& path, const std::string& source) {
    put(*static_cast<RamFileSystem*>(st.fileSystem()), path, source);
    prog::Project p;
    std::unique_ptr<code::ProjectSources> src;
    std::string folder, err;
    if (!code::loadProject(st, path, p, src, folder, err)) return "LOAD[" + err + "]";
    Workspace ws;
    prog::WorkspaceHost host(ws);
    host.fs = st.fileSystem();
    host.folder = folder;
    try {
        auto prog = prog::compile(p);
        prog::Machine m(prog, host);
        m.runToEnd(20000000);
    } catch (const prog::Error& e) {
        return host.output + "ERR[" + e.what() + " @" + e.file + ":" + std::to_string(e.line) + "]";
    }
    return host.output;
}
// Esempi di docs/examples/program: il sorgente va nella cartella del suo
// linguaggio (PICOCALC/CODE/<LING>), con l'input <nome>.in; l'uscita deve
// essere <nome>.out. Ritorna gli esempi eseguiti.
int runExamples(code::Store& st, Workspace& ws) {
    namespace fsys = std::filesystem;
    const std::pair<const char*, const char*> langs[] = {{"c", "C"}, {"cpp", "CPP"}, {"cs", "CS"}};
    int n = 0;
    for (const auto& [dir, folder] : langs) {
        std::vector<fsys::path> srcs;
        for (const auto& e : fsys::directory_iterator(fsys::path(FXAI_EXAMPLES_DIR) / dir))
            if (e.path().extension() == std::string(".") + dir) srcs.push_back(e.path());
        std::sort(srcs.begin(), srcs.end());
        for (const auto& src : srcs) {
            auto slurp = [](const fsys::path& p) {
                std::ifstream in(p, std::ios::binary);
                return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            };
            const std::string path = std::string("PICOCALC/CODE/") + folder + "/" + src.filename().string();
            put(*static_cast<RamFileSystem*>(st.fileSystem()), path, slurp(src));
            prog::Project p;
            std::unique_ptr<code::ProjectSources> sources;
            std::string programFolder, err;
            std::string got;
            prog::WorkspaceHost host(ws);
            if (!code::loadProject(st, path, p, sources, programFolder, err)) got = "LOAD[" + err + "]";
            else {
                host.fs = st.fileSystem();
                host.folder = programFolder;
                fsys::path in = src;
                in.replace_extension(".in");
                if (fsys::exists(in)) {
                    const std::string text = slurp(in);
                    for (std::size_t a = 0; a < text.size();) {
                        std::size_t b = text.find('\n', a);
                        if (b == std::string::npos) b = text.size();
                        host.input.push_back(text.substr(a, b - a));
                        a = b + 1;
                    }
                }
                host.endOfInput = true;
                try {
                    prog::Machine m(prog::compile(p), host);
                    m.runToEnd(50000000);
                    got = host.output;
                } catch (const prog::Error& e) {
                    got = host.output + "ERR[" + e.what() + " @" + e.file + ":" + std::to_string(e.line) + "]";
                }
            }
            fsys::path outp = src;
            outp.replace_extension(".out");
            const std::string want = slurp(outp);
            ::testutil::report(got == want, src.filename().string().c_str(), __FILE__, __LINE__, got == want ? std::string() : "uscita:\n" + got);
            ++n;
        }
    }
    return n;
}
} // namespace

int main() {
    RamFileSystem sd;
    CHECK(copyRepo(sd) >= 20);
    std::vector<std::uint8_t> data;
    CHECK(files::readAll(sd, "REPO/index.json", data, MaxIndexBytes) == files::Status::Ok);
    std::vector<IndexEntry> index;
    std::string err;
    CHECK(parseIndex(std::string(data.begin(), data.end()), index, err));
    CHECK(index.size() == 9);
    Version fw;
    (void)Version::parse("1.20.0", fw);
    // ogni libreria si installa dal repository (impronte e dipendenze dell'indice vero)
    FolderSource source(sd, "REPO");
    for (const IndexEntry& e : index) {
        std::vector<PlanStep> plan;
        const bool planned = makePlan(index, listInstalled(sd, ""), e.name, e.language, fw, plan, err);
        CHECK(planned);
        if (!planned) { std::fprintf(stderr, "  %s: %s\n", e.name.c_str(), err.c_str()); continue; }
        Installer in(sd, source);
        in.start(plan, fw);
        while (in.tick(4096)) {}
        CHECK(in.state() == Installer::State::Done);
        if (in.state() != Installer::State::Done) std::fprintf(stderr, "  %s: %s\n", e.name.c_str(), in.error().c_str());
    }
    const auto installed = listInstalled(sd, "");
    CHECK(installed.size() == 9);
    for (const Installed& l : installed) {
        const VerifyReport r = verify(sd, l);
        CHECK(l.manifestOk && r.ok);
        if (!r.ok) for (const auto& p : r.problems) std::fprintf(stderr, "  %s: %s\n", l.manifest.name.c_str(), p.c_str());
    }
    code::Store st(&sd);
    CHECK(st.ready() == files::Status::Ok);

    // ---- C
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/C/mat.c", R"~(#include <stdio.h>
#include <matrici.h>
int main(void) {
    double d[] = {2, 1, 1, 3};
    Matrice a = mat_da_array(2, 2, d);
    Matrice inv;
    printf("%.1f ", mat_determinante(a));
    if (mat_inversa(a, &inv)) printf("%.1f %.1f ", inv.v[0][0], inv.v[1][1]);
    Matrice p = mat_prodotto(a, inv);
    printf("%.1f %.1f\n", p.v[0][0], p.v[1][1]);
    double s[] = {1, 2, 3, 4, 5, 6};
    mat_stampa(mat_trasposta(mat_da_array(2, 3, s)));
    double z[] = {1, 2, 2, 4};
    printf("%d %.1f\n", mat_inversa(mat_da_array(2, 2, z), &inv), mat_determinante(mat_identita(4)));
    return 0;
}
)~"), "5.0 0.6 0.4 1.0 1.0\n        1        4\n        2        5\n        3        6\n0 1.0\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/C/sta.c", R"~(#include <stdio.h>
#include <statistica.h>
int main(void) {
    double x[] = {1, 2, 3, 4, 5}, y[] = {2.1, 3.9, 6.2, 7.8, 10.1};
    double a, b, r;
    stat_regressione(x, y, 5, &a, &b, &r);
    printf("%.3f %.4f %.4f %.1f %.1f %.1f\n", stat_media(y, 5), stat_dev_pop(x, 5), stat_dev_camp(x, 5), stat_mediana(y, 5), stat_minimo(y, 5), stat_massimo(y, 5));
    printf("%.4f %.4f %.5f\n", a, b, r);
    return 0;
}
)~"), "6.020 1.4142 1.5811 6.2 2.1 10.1\n0.0500 1.9900 0.99865\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/C/tra.c", R"~(#include <stdio.h>
#include <trasformazioni.h>
int main(void) {
    Matrice t = tr_componi(tr_rotazione(90), tr_traslazione(1, 0));
    double x = 1, y = 0;
    tr_applica(t, &x, &y);
    printf("%.3f %.3f\n", x, y);
    double px[3] = {0, 2, 0}, py[3] = {0, 0, 1};
    Matrice s = tr_componi(tr_scala(2, 3), tr_simmetria_x());
    for (int i = 0; i < 3; i++) { tr_applica(s, &px[i], &py[i]); printf("(%.1f,%.1f)", px[i], py[i]); }
    printf("\n");
    return 0;
}
)~"), "1.000 1.000\n(0.0,0.0)(4.0,0.0)(0.0,-3.0)\n");

    // ---- C++
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CPP/fra.cpp", R"~(#include <iostream>
#include <frazioni.hpp>
int main() {
    Frazione a(1, 2), b(1, 3);
    std::cout << a + b << " " << a * b << " " << (a - b) / b << " " << Frazione(6, -4) << " " << (b < a) << " " << Frazione(4, 2) << " " << (a + b).valore() << "\n";
    try { Frazione z(1, 0); } catch (const std::invalid_argument& e) { std::cout << e.what() << "\n"; }
    return 0;
}
)~"), "5/6 1/6 1/2 -3/2 1 2 0.833333\nfrazione con denominatore zero\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CPP/geo.cpp", R"~(#include <iostream>
#include <iomanip>
#include <geometria2d.hpp>
int main() {
    std::vector<Punto> q = {{0, 0}, {4, 0}, {4, 3}, {0, 3}};
    Punto g = baricentro(q);
    Punto r = ruota({1, 0}, {0, 0}, 90);
    std::cout << std::fixed << std::setprecision(2) << areaPoligono(q) << " " << perimetro(q) << " " << g.x << "," << g.y << " " << distanza({0, 0}, {3, 4})
              << " " << r.x << "," << r.y << " " << dentro(q, {1, 1}) << dentro(q, {5, 1}) << "\n";
    return 0;
}
)~"), "12.00 14.00 2.00,1.50 5.00 0.00,1.00 10\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CPP/pol.cpp", R"~(#include <iostream>
#include <polinomi.hpp>
int main() {
    Polinomio p({-6, 11, -6, 1});
    std::cout << p.testo() << " | " << p.grado() << " " << p.valuta(4) << " | " << p.derivata().testo() << " |";
    for (double r : p.radici()) std::cout << " " << r;
    Polinomio q({1, 1});
    std::cout << " | " << (q * q).testo() << " | " << (q + Polinomio({0, 0, 2})).testo() << " | " << Polinomio({0}).testo() << " | " << Polinomio({-1, 0, 1}).radici().size() << "\n";
    Polinomio c({1, 0, 1});
    std::cout << c.radici().size() << " " << Polinomio({-2, 0, 0, 1}).radici()[0] << "\n";
    return 0;
}
)~"), "x^3 - 6x^2 + 11x - 6 | 3 6 | 3x^2 - 12x + 11 | 1 2 3 | x^2 + 2x + 1 | 2x^2 + x + 1 | 0 | 2\n0 1.25992\n");

    // ---- C#
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CS/fin.cs", R"~(using System;
using PicoCalc.Finanza;
class P {
    static void Main() {
        Console.WriteLine($"{Interessi.Montante(1000, 0.05, 10):F2} {Interessi.ValoreAttuale(1628.89, 0.05, 10):F2} {Interessi.Rata(10000, 0.01, 12):F2} {Interessi.Rata(1200, 0, 12):F2}");
        double[] f = { -1000, 300, 400, 500 };
        Console.WriteLine($"{Interessi.Van(0.05, f):F2} {Interessi.Tir(f) * 100:F3}%");
        try { Interessi.Rata(1, 0.1, 0); } catch (ArgumentOutOfRangeException e) { Console.WriteLine(e.ParamName); }
    }
}
)~"), "1628.89 1000.00 888.49 100.00\n80.44 8.896%\nperiodi\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CS/sta.cs", R"~(using System;
using System.Collections.Generic;
using PicoCalc.Statistica;
class P {
    static void Main() {
        var x = new List<double> { 1, 2, 3, 4, 5 };
        var y = new List<double> { 2.1, 3.9, 6.2, 7.8, 10.1 };
        Console.WriteLine($"{Descrittiva.Media(y):F2} {Descrittiva.Mediana(y)} {Descrittiva.DevStdPop(x):F4} {Descrittiva.DevStdCamp(x):F4} {Descrittiva.Mediana(new List<double> { 4, 1, 3, 2 })}");
        var r = Descrittiva.Regressione(x, y);
        Console.WriteLine(r);
        Console.WriteLine($"{r.Valore(6):F2}");
    }
}
)~"), "6.02 6.2 1.4142 1.5811 2.5\ny = 0.0500 + 1.9900x (r = 0.9987)\n11.99\n");
    CHECK_EQ_STR(run(st, "PICOCALC/CODE/CS/mat.cs", R"~(using System;
using PicoCalc.Matrici;
class P {
    static void Main() {
        var a = Matrice.Da(2, 2, 1, 2, 3, 4);
        var b = Matrice.Identita(2);
        b[0, 1] = 1;
        Console.WriteLine(a * b);
        Console.WriteLine($"{a.Determinante()} {a.Trasposta()[0, 1]} {Matrice.Da(3, 3, 2, 0, 0, 0, 3, 0, 0, 0, 4).Determinante()} {Matrice.Da(1, 3, 0.5, 1.25, 2).Trasposta().Righe}");
        try { var c = a * Matrice.Identita(3); } catch (ArgumentException e) { Console.WriteLine(e.Message); }
    }
}
)~"), "1 3\n3 7\n-2 3 24 3\ndimensioni incompatibili per il prodotto\n");
    // ---- esempi della documentazione (docs/examples/program), anche con le librerie installate
    {
        Workspace ws;
        CHECK(runExamples(st, ws) >= 5);
        CHECK(ws.memory.recall('X') == -3 && ws.memory.recall('Y') == 1);   // Equazioni.cpp: radici dell'ultima equazione reale
        CHECK(ws.lists[1].size() == 8);                                      // StatisticheCSV.cs: List 2
    }
    return testutil::finish("libraries_test");
}
