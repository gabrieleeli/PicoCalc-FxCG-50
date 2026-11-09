#include "diagnostics.hpp"
#include "app.hpp"
#include "board_config.hpp"
#include "graph_engine.hpp"
#include "graph_file.hpp"
#include "graph_solve.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "storage.hpp"
#include "system_info.hpp"
#include "system_runtime.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#if FXAI_ON_DEVICE
#include "pico/time.h"
#endif

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
    if (!fs) { add(out, false, "archivio non disponibile in questa build"); return out; }
    std::uint32_t t0 = now();
    files::Status st = fs->status();
    const std::uint32_t mountMs = now() - t0;
    if (st != files::Status::Ok) { add(out, false, std::string(fs->name()) + ": " + files::statusMessage(st)); return out; }
    add(out, true, std::string(fs->name()) + ": " + fs->describe());
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

// ---------------------------------------------------------------- memory and hardware (v1.9.0)
namespace {
std::uint32_t mix(std::uint32_t& s) noexcept {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
std::string sizeText(std::size_t bytes) {
    char b[16];
    sysinfo::formatSize(b, sizeof b, bytes);
    return b;
}
struct Slot { unsigned char* p; std::size_t n; unsigned char tag; };

#if FXAI_ON_DEVICE
StressStats g_core1Stats{};
volatile bool g_core1Done = false;
void core1Stress() {
    g_core1Stats = allocatorStress(0xC0DE1234u, 20000, 4096);
    g_core1Done = true;
}
#endif
} // namespace

StressStats allocatorStress(std::uint32_t seed, std::size_t operations, std::size_t maxBlock) noexcept {
    StressStats st;
    Slot live[128];
    std::size_t count = 0;
    std::uint32_t s = seed | 1u;
    const bool psram = mem::available(mem::Pool::PSRAM_BOARD);
    for (std::size_t op = 0; op < operations; ++op) {
        ++st.operations;
        if (count == 0 || (count < 128 && (mix(s) % 100u) < 55u)) {
            const std::size_t n = 1 + mix(s) % (maxBlock ? maxBlock : 1);
            const unsigned pick = mix(s) % (psram ? 4u : 2u);
            const mem::Policy policy = pick == 0 ? mem::Policy::PREFER_SRAM
                                     : pick == 1 ? mem::Policy::SRAM_ONLY
                                     : pick == 2 ? mem::Policy::PREFER_PSRAM : mem::Policy::PSRAM_ONLY;
            auto* p = static_cast<unsigned char*>(mem::tryAlloc(policy, n));
            if (!p) { ++st.failures; continue; }
            ++st.allocations;
            const auto tag = static_cast<unsigned char>(mix(s));
            std::memset(p, tag, n);
            live[count++] = {p, n, tag};
            st.peakLive = std::max(st.peakLive, count);
        } else {
            const std::size_t i = mix(s) % count;
            for (std::size_t k = 0; k < live[i].n; ++k)
                if (live[i].p[k] != live[i].tag) { ++st.corruptions; break; }
            mem::free(live[i].p);
            live[i] = live[--count];
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t k = 0; k < live[i].n; ++k)
            if (live[i].p[k] != live[i].tag) { ++st.corruptions; break; }
        mem::free(live[i].p);
    }
    return st;
}

std::vector<Line> testMemory(app::Platform& platform) {
    std::vector<Line> out;
    namespace cfg = board::config;
    const auto& i = psram::info();
    add(out, true, std::string("scheda: ") + cfg::kBoardName + ", flash " + sizeText(cfg::kFlashSize) +
                       (cfg::kHasPsram ? ", PSRAM attesa " + sizeText(cfg::kPsramExpectedBytes) : std::string(", senza PSRAM")));
    if (cfg::kHasPsram) {
        char buf[112];
        if (FXAI_ON_DEVICE)
            std::snprintf(buf, sizeof buf, "PSRAM: rilevata %s, abilitata %s, MF 0x%02X KGD 0x%02X EID 0x%02X, %u KB",
                          i.detected ? "si" : "no", i.enabled ? "si" : "no", i.manufacturer, i.kgd, i.eid,
                          static_cast<unsigned>(i.sizeBytes / 1024u));
        else
            std::snprintf(buf, sizeof buf, "PSRAM (simulata sull'host): abilitata %s, %u KB", i.enabled ? "si" : "no",
                          static_cast<unsigned>(i.sizeBytes / 1024u));
        add(out, i.enabled, buf);
        if (i.detected && i.sysClockHz) {          // timing exists only on the device
            std::snprintf(buf, sizeof buf, "PSRAM: clk_sys %lu MHz, div %lu, SCK %lu MHz, MAX_SELECT %lu, MIN_DESELECT %lu",
                          static_cast<unsigned long>(i.sysClockHz / 1000000u), static_cast<unsigned long>(i.clockDivider),
                          static_cast<unsigned long>(i.sckHz / 1000000u), static_cast<unsigned long>(i.maxSelect),
                          static_cast<unsigned long>(i.minDeselect));
            add(out, true, buf);
            std::snprintf(buf, sizeof buf, "PSRAM: self-test di avvio %s in %lu us, verifiche M1 %lu, ripristini %lu",
                          i.bootSelfTest.passed ? "OK" : "FALLITO", static_cast<unsigned long>(i.bootSelfTest.elapsedUs),
                          static_cast<unsigned long>(i.configChecks), static_cast<unsigned long>(i.configRepairs));
            add(out, i.bootSelfTest.passed, buf);
        }
        if (i.error) add(out, false, std::string("PSRAM: ") + i.error);
    }
    char lines[24][sysinfo::kLineChars];
    const std::size_t n = sysinfo::formatMemory(sysinfo::collect(platform.hardware()), lines, 24);
    for (std::size_t k = 1; k < n; ++k) add(out, true, lines[k]);   // line 0 repeats the board name
    add(out, mem::checkIntegrity(), mem::checkIntegrity() ? "integrita' dei pool TLSF: OK" : "integrita' dei pool TLSF: ERRORE");
    return out;
}

std::vector<Line> testPsram(const Clock& now) {
    std::vector<Line> out;
    if (!board::config::kHasPsram) { add(out, true, "PSRAM: non presente su questa scheda (test saltato)"); return out; }
    if (!mem::available(mem::Pool::PSRAM_BOARD)) { add(out, false, "PSRAM: non disponibile (avvio solo in SRAM)"); return out; }
    const std::uint32_t t0 = now();
    const auto t = psram::runDiagnostics(256u * 1024u, t0 * 2654435761u + 1u);
    char buf[112];
    std::snprintf(buf, sizeof buf, "PSRAM 256 KB: bus dati %s, pattern %s, cache/uncached %s, %lu campioni, %lu us",
                  t.dataBus ? "OK" : "-", t.randomPattern ? "OK" : "-", t.cachedPath ? "OK" : "-",
                  static_cast<unsigned long>(t.samples), static_cast<unsigned long>(t.elapsedUs));
    add(out, t.passed, buf);
    if (!t.passed && t.failedStep) {
        std::snprintf(buf, sizeof buf, "PSRAM: errore in %s, parola %u: atteso 0x%08lX letto 0x%08lX", t.failedStep,
                      static_cast<unsigned>(t.failWordIndex), static_cast<unsigned long>(t.expected),
                      static_cast<unsigned long>(t.actual));
        add(out, false, buf);
    }
    return out;
}

std::vector<Line> testAllocator(const Clock& now, bool dualCore) {
    std::vector<Line> out;
    std::uint32_t t0 = now();
    const auto st = allocatorStress(t0 * 2654435761u + 7u, 20000, 8192);
    const bool integrity = mem::checkIntegrity();
    char buf[112];
    std::snprintf(buf, sizeof buf, "allocatore: %u operazioni, %u allocazioni, %u rifiutate, %u corruzioni, %u ms",
                  static_cast<unsigned>(st.operations), static_cast<unsigned>(st.allocations),
                  static_cast<unsigned>(st.failures), static_cast<unsigned>(st.corruptions),
                  static_cast<unsigned>(now() - t0));
    add(out, st.corruptions == 0, buf);
    add(out, integrity, integrity ? "integrita' TLSF dopo lo stress: OK" : "integrita' TLSF dopo lo stress: ERRORE");
    if (!dualCore) return out;
#if FXAI_ON_DEVICE
    g_core1Done = false;
    if (!runtime::launchCore1(core1Stress)) { add(out, false, "2 core: core 1 non avviabile"); return out; }
    const auto st0 = allocatorStress(0x5EED0001u, 20000, 4096);
    const absolute_time_t until = make_timeout_time_ms(10000);
    while (!g_core1Done && !time_reached(until)) tight_loop_contents();
    const bool done = g_core1Done;
    const auto stack1 = runtime::core1Stack();
    runtime::stopCore1();
    const bool integrity2 = mem::checkIntegrity();
    std::snprintf(buf, sizeof buf, "2 core: core0 %u corruzioni, core1 %s con %u corruzioni, stack core1 %u B",
                  static_cast<unsigned>(st0.corruptions), done ? "completato" : "TIMEOUT",
                  static_cast<unsigned>(g_core1Stats.corruptions), static_cast<unsigned>(stack1.highWaterBytes));
    add(out, done && st0.corruptions == 0 && g_core1Stats.corruptions == 0, buf);
    add(out, integrity2, integrity2 ? "2 core: integrita' dei pool OK" : "2 core: integrita' dei pool ERRORE");
#else
    add(out, true, "2 core: solo sul dispositivo");
#endif
    return out;
}

std::vector<Line> testFlashPsram(storage::FlashDevice* sector, const Clock& now) {
    std::vector<Line> out;
    if (!sector || sector->size() < sector->eraseSize()) { add(out, false, "flash: settore diagnostico non disponibile"); return out; }
    // 64 KB of PSRAM: more than the 16 KB XIP cache, so dirty lines must have
    // been written back by the flash routines.
    constexpr std::size_t Words = 16384;
    std::uint32_t* buffer = nullptr;
    const bool havePsram = mem::available(mem::Pool::PSRAM_BOARD);
    if (havePsram) {
        buffer = static_cast<std::uint32_t*>(mem::tryAlloc(mem::Pool::PSRAM_BOARD, Words * 4, 64));
        if (!buffer) { add(out, false, "flash/PSRAM: PSRAM insufficiente per il test"); return out; }
        std::uint32_t s = 0x1234567u;
        for (std::size_t i = 0; i < Words; ++i) buffer[i] = mix(s);
    }
    const unsigned long repairsBefore = psram::info().configRepairs;
    std::uint32_t t0 = now();
    const bool erased = sector->erase(0, sector->eraseSize());
    const std::uint32_t eraseMs = now() - t0;
    std::uint8_t page[256];
    for (std::size_t i = 0; i < sizeof page; ++i) page[i] = static_cast<std::uint8_t>(i * 7u + 3u);
    t0 = now();
    const bool programmed = erased && sector->program(0, page, sizeof page);
    const std::uint32_t programMs = now() - t0;
    std::uint8_t back[256] = {};
    const bool verified = programmed && sector->read(0, back, sizeof back) && std::memcmp(back, page, sizeof page) == 0;
    char buf[112];
    std::snprintf(buf, sizeof buf, "flash: cancellazione %u ms, programmazione %u ms, verifica %s",
                  static_cast<unsigned>(eraseMs), static_cast<unsigned>(programMs), verified ? "OK" : "ERRATA");
    add(out, verified, buf);
    if (havePsram) {
        bool okCached = true, okUncached = true;
        std::uint32_t s = 0x1234567u;
#if FXAI_ON_DEVICE
        const auto uncached = reinterpret_cast<volatile std::uint32_t*>(
            reinterpret_cast<std::uintptr_t>(buffer) - board::config::kPsramCachedBase + board::config::kPsramUncachedBase);
#else
        const volatile std::uint32_t* uncached = buffer;
#endif
        for (std::size_t i = 0; i < Words; ++i) {
            const std::uint32_t v = mix(s);
            if (uncached[i] != v) okUncached = false;
            if (buffer[i] != v) okCached = false;
        }
        // The window must still accept writes (QPI mode + WRITABLE_M1 intact).
        for (std::size_t i = 0; i < 256; ++i) buffer[i] = ~static_cast<std::uint32_t>(i);
        psram::cleanCache();
        bool writable = true;
        for (std::size_t i = 0; i < 256; ++i) if (uncached[i] != ~static_cast<std::uint32_t>(i)) writable = false;
        mem::free(buffer);
        std::snprintf(buf, sizeof buf, "PSRAM dopo la scrittura flash: con cache %s, diretta %s, ancora scrivibile %s",
                      okCached ? "OK" : "ERRATA", okUncached ? "OK" : "ERRATA", writable ? "si" : "NO");
        add(out, okCached && okUncached && writable, buf);
        const unsigned long repairs = psram::info().configRepairs - repairsBefore;
        add(out, true, repairs ? "configurazione QMI M1 ripristinata dopo la scrittura (" + std::to_string(repairs) + " volte)"
                               : std::string("configurazione QMI M1 intatta dopo la scrittura"));
    } else {
        add(out, true, "PSRAM: non disponibile, verificata solo la flash");
    }
    return out;
}

std::vector<Line> testDisplay(app::Platform& platform) {
    std::vector<Line> out;
    const auto hw = platform.hardware();
    add(out, true, "framebuffer " + sizeText(hw.framebufferBytes) + " in " + (hw.framebufferInPsram ? "PSRAM" : "SRAM") +
                       (hw.framebufferRequestedPsram && !hw.framebufferInPsram ? " (PSRAM richiesta ma non disponibile)" : ""));
    const std::uint32_t us = platform.benchmarkPresent();
    if (us) add(out, true, "aggiornamento completo del LCD: " + std::to_string(us / 1000) + "." + std::to_string(us % 1000 / 100) + " ms");
    else add(out, true, "aggiornamento LCD: tempo misurato solo sul dispositivo");
#if FXAI_ON_DEVICE
    // GRAPH layer cache: copy of the 320x282 plot area from PSRAM into the
    // framebuffer, as done for every TRACE step (the screen is redrawn after).
    if (mem::available(mem::Pool::PSRAM_BOARD) && hw.framebufferBytes) {
        constexpr std::size_t Bytes = 320u * 282u * 2u;
        auto* layer = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Pool::PSRAM_BOARD, Bytes, 64));
        if (layer) {
            for (std::size_t i = 0; i < Bytes / 2; ++i) layer[i] = static_cast<std::uint16_t>(i * 2654435761u >> 16);
            psram::cleanCache();
            const std::uint32_t t0 = time_us_32();
            std::uint16_t* fb = reinterpret_cast<std::uint16_t*>(hw.framebufferPixels);
            if (fb) std::memcpy(fb + 18 * 320, layer, Bytes);
            const std::uint32_t copyUs = time_us_32() - t0;
            mem::free(layer);
            add(out, true, "copia del layer GRAPH (176 KB) dalla PSRAM: " + std::to_string(copyUs / 1000) + "." +
                               std::to_string(copyUs % 1000 / 100) + " ms");
        }
    }
#endif
    return out;
}
} // namespace fx::diag
