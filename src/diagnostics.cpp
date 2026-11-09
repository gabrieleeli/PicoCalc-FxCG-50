#include "diagnostics.hpp"
#include "graph_engine.hpp"
#include "graph_file.hpp"
#include "graph_solve.hpp"
#include "workspace.hpp"
#include <cmath>
#include <cstdio>
#include <memory>

namespace fx::diag {
namespace {
std::string format(const char* fmt, double a = 0, double b = 0, double c = 0) {
    char buf[112];
    std::snprintf(buf, sizeof(buf), fmt, a, b, c);
    return buf;
}
void add(std::vector<Line>& out, bool ok, std::string text) {
    std::printf("[DIAG] %s %s\n", ok ? "OK  " : "FAIL", text.c_str());
    out.push_back({ok, std::move(text)});
}
double rate(std::size_t bytes, std::uint32_t ms) { return ms ? static_cast<double>(bytes) / 1024.0 / (ms / 1000.0) : 0.0; }
} // namespace

std::vector<Line> testStorage(files::FileSystem* fs, const Clock& now) {
    std::vector<Line> out;
    if (!fs) { add(out, false, "microSD non disponibile in questa build"); return out; }
    std::uint32_t t0 = now();
    files::Status st = fs->status();
    const std::uint32_t mountMs = now() - t0;
    if (st != files::Status::Ok) { add(out, false, std::string("scheda: ") + files::statusMessage(st)); return out; }
    add(out, true, "scheda: " + fs->describe());
    add(out, true, format("inizializzazione e montaggio: %.0f ms", mountMs));
    std::uint64_t freeBytes = 0;
    t0 = now();
    st = fs->freeSpace(freeBytes);
    add(out, st == files::Status::Ok, st == files::Status::Ok ? format("spazio libero: %.1f MB (%.0f ms)", static_cast<double>(freeBytes) / 1048576.0, now() - t0)
                                                              : std::string("spazio libero: ") + files::statusMessage(st));
    const std::string dir = "PICOCALC/DIAG";
    if ((st = fs->makeDirs(dir)) != files::Status::Ok) { add(out, false, std::string("cartella di prova: ") + files::statusMessage(st)); return out; }
    // 64 KB with a pattern that differs in every byte and sector.
    std::vector<std::uint8_t> data(64 * 1024);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>((i * 7 + i / 512) & 0xFF);
    const std::string path = dir + "/TEST.bin";
    t0 = now();
    st = files::writeAtomic(*fs, path, data.data(), data.size());
    const std::uint32_t writeMs = now() - t0;
    add(out, st == files::Status::Ok, st != files::Status::Ok ? std::string("scrittura: ") + files::statusMessage(st)
                                      : writeMs ? format("scrittura 64 KB (con verifica CRC): %.0f ms, %.1f KB/s", writeMs, rate(data.size(), writeMs))
                                                : std::string("scrittura 64 KB (con verifica CRC): <1 ms"));
    if (st == files::Status::Ok) {
        std::vector<std::uint8_t> back;
        t0 = now();
        st = files::readAll(*fs, path, back, data.size());
        const std::uint32_t readMs = now() - t0;
        const bool same = st == files::Status::Ok && back == data;
        add(out, same, st != files::Status::Ok ? std::string("lettura: ") + files::statusMessage(st)
                       : same ? (readMs ? format("lettura 64 KB: %.0f ms, %.1f KB/s, dati identici", readMs, rate(back.size(), readMs))
                                        : std::string("lettura 64 KB: <1 ms, dati identici"))
                              : std::string("lettura: DATI DIVERSI da quelli scritti"));
    }
    // A GRAPH project through the real codec.
    // Projects on the heap (about 2 KB each; the RP2350 stack is 32 KB).
    auto p = std::make_unique<graphfile::Project>(), q = std::make_unique<graphfile::Project>();
    p->functions[0].expr = "(x)^(3)-4x";
    p->functions[1] = {"3cos(t)", GraphType::Param, true, 3, LineStyle::Thick, "2sin(t)"};
    const std::string text = graphfile::encode(*p), project = dir + "/DIAG.pcgraph";
    st = files::writeAtomic(*fs, project, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    std::vector<std::uint8_t> raw;
    if (st == files::Status::Ok) st = files::readAll(*fs, project, raw, graphfile::MaxProjectBytes);
    const std::string err = st == files::Status::Ok ? graphfile::decode(std::string(raw.begin(), raw.end()), *q) : files::statusMessage(st);
    add(out, err.empty() && q->functions[1].expr2 == "2sin(t)", err.empty() ? "progetto .pcgraph: salvato e riletto" : "progetto .pcgraph: " + err);
    // Clean up: nothing of the test stays on the card.
    files::Status rm = fs->remove(path);
    if (rm == files::Status::Ok || rm == files::Status::NotFound) rm = fs->remove(project);
    if (rm == files::Status::Ok || rm == files::Status::NotFound) rm = fs->remove(dir);
    add(out, rm == files::Status::Ok, rm == files::Status::Ok ? "file di prova eliminati" : std::string("pulizia: ") + files::statusMessage(rm));
    return out;
}

std::vector<Line> testGraph(Workspace& ws, const Clock& now) {
    std::vector<Line> out;
    // A fixed set: polynomial, trigonometric, pole, exponential, parametric.
    auto functions = std::make_unique<std::array<GraphFunction, MaxGraphFunctions>>();
    for (std::size_t i = 0; i < functions->size(); ++i) (*functions)[i] = emptyGraphFunction(i);
    (*functions)[0].expr = "(x)^(3)-4x";
    (*functions)[1].expr = "2sin(x)+cos(3x)";
    (*functions)[2].expr = "tan(x)";
    (*functions)[3].expr = "e^(x)/5";
    (*functions)[4] = {"3cos(t)", GraphType::Param, true, 4, LineStyle::Normal, "2sin(3t)"};
    const AngleMode angle = ws.settings.angle;
    setAngleMode(AngleMode::Radians);
    try {
        auto holder = std::make_unique<graph::GraphEngine>();       // on the heap, like the projects
        graph::GraphEngine& engine = *holder;
        std::uint32_t t0 = now();
        engine.build({functions.get(), &ws.memory, &ws, &ws.lists});
        add(out, engine.plots().size() == 5, format("compilazione di 5 grafici: %.0f ms", now() - t0));
        const graph::Viewport vp(ViewWindow{});
        std::size_t evals = 0, points = 0;
        t0 = now();
        auto curve = std::make_unique<graph::Curve>();
        for (const auto& plot : engine.plots()) {
            graph::Curve& c = *curve;
            c = graph::Curve{};
            for (int guard = 0; !c.done && guard < 1000; ++guard) {
                int budget = 600;
                graph::sampleStep(engine, plot, vp, false, c, budget);
                evals += static_cast<std::size_t>(600 - std::min(budget, 600));
            }
            points += c.xy.size() / 2;
        }
        const std::uint32_t drawMs = now() - t0;
        add(out, points > 0, format("campionamento: %.0f ms, %.0f valutazioni, %.0f punti", drawMs, static_cast<double>(evals), static_cast<double>(points)));
        if (drawMs) add(out, true, format("velocita': %.0f valutazioni/s (%.1f ms per ciclo da 600)", evals * 1000.0 / drawMs, 600.0 * drawMs / std::max<std::size_t>(evals, 1)));
        const graph::Plot& cubic = engine.plots()[0];
        graph::RealFunction f = [&](double x, double& y) { return engine.eval(cubic, x, y); };
        t0 = now();
        const auto roots = graph::findRoots(f, -6.3, 6.3);
        const bool rootsOk = roots.size() == 3 && std::abs(roots[0] + 2) < 1e-9 && std::abs(roots[1]) < 1e-9 && std::abs(roots[2] - 2) < 1e-9;
        add(out, rootsOk, format("G-Solve ROOT di x^3-4x: %.0f radici in %.0f ms", static_cast<double>(roots.size()), now() - t0));
        t0 = now();
        const double integral = graph::integrate(f, -2, 0);
        add(out, std::abs(integral - 4) < 1e-9, format("G-Solve integrale da -2 a 0 = %.10g (atteso 4), %.0f ms", integral, now() - t0));
    } catch (const std::exception& e) {
        add(out, false, std::string("GRAPH: ") + e.what());
    }
    setAngleMode(angle);
    return out;
}
} // namespace fx::diag
