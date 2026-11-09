// Lines are clipped to the 52-character screen width on purpose.
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "diagnostics.hpp"

#include "board_config.hpp"
#include "bt_support.hpp"
#include "flash_fs.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "sd_card.hpp"
#include "system_runtime.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#if FXAI_ON_DEVICE
#include "board_port.hpp"
#include "pico/time.h"
#endif

namespace fx::diag {
namespace {
std::uint32_t nowUs() noexcept {
#if FXAI_ON_DEVICE
    return time_us_32();
#else
    return 0;
#endif
}

__attribute__((format(printf, 2, 3)))
void add(Result& r, const char* fmt, ...) noexcept {
    if (r.lineCount >= 8) return;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(r.lines[r.lineCount], sizeof r.lines[0], fmt, ap);
    va_end(ap);
    ++r.lineCount;
}

std::uint32_t mix(std::uint32_t& s) noexcept {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

struct Slot {
    unsigned char* p;
    std::size_t n;
    unsigned char tag;
};

#if FXAI_ON_DEVICE
StressStats g_core1Stats{};
volatile bool g_core1Done = false;
void core1Stress() {
    g_core1Stats = allocatorStress(0xC0DE1234u, 20000, 4096);
    g_core1Done = true;
}
#endif
} // namespace

const char* name(Test test) noexcept {
    switch (test) {
    case Test::PsramInfo: return "PSRAM: ID e configurazione";
    case Test::PsramPattern: return "PSRAM: pattern (256 KiB)";
    case Test::AllocatorStress: return "Allocatore: stress";
    case Test::DualCoreStress: return "Allocatore: stress 2 core";
    case Test::FlashPsram: return "PSRAM dopo scrittura flash";
    case Test::Framebuffer: return "Framebuffer: SRAM vs PSRAM";
    case Test::WiFi: return "Wi-Fi (CYW43)";
    case Test::Storage: return "LittleFS interno";
    case Test::SdCard: return "Scheda SD";
    case Test::Count: break;
    }
    return "?";
}

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

Result run(Test test) noexcept {
    Result r;
    r.ran = true;
    const std::uint32_t start = nowUs();
    char a[16], b[16];
    switch (test) {
    case Test::PsramInfo: {
        const auto& i = psram::info();
        if (!board::config::kHasPsram) { add(r, "Scheda senza PSRAM (Pico 2 W)"); r.passed = true; break; }
        add(r, "Rilevata: %s  Abilitata: %s", i.detected ? "si" : "no", i.enabled ? "si" : "no");
        add(r, "MF 0x%02X  KGD 0x%02X  EID 0x%02X", i.manufacturer, i.kgd, i.eid);
        add(r, "Dimensione %u KiB  CS GPIO%d", static_cast<unsigned>(i.sizeBytes / 1024u), board::config::kPsramCsPin);
        add(r, "clk_sys %lu Hz  div %lu  SCK %lu Hz", static_cast<unsigned long>(i.sysClockHz),
            static_cast<unsigned long>(i.clockDivider), static_cast<unsigned long>(i.sckHz));
        add(r, "MAX_SELECT %lu  MIN_DESELECT %lu", static_cast<unsigned long>(i.maxSelect),
            static_cast<unsigned long>(i.minDeselect));
        add(r, "Self-test avvio: %s (%lu us)", i.bootSelfTest.passed ? "OK" : "FALLITO",
            static_cast<unsigned long>(i.bootSelfTest.elapsedUs));
        add(r, "Verifiche M1 %lu  ripristini %lu", static_cast<unsigned long>(i.configChecks),
            static_cast<unsigned long>(i.configRepairs));
        if (i.error) add(r, "%s", i.error);
        r.passed = i.enabled;
        break;
    }
    case Test::PsramPattern: {
        const auto t = psram::runDiagnostics(256u * 1024u, nowUs());
        add(r, "Bus dati: %s  Pattern: %s", t.dataBus ? "OK" : "-", t.randomPattern ? "OK" : "-");
        add(r, "Percorso cache/uncached: %s", t.cachedPath ? "OK" : "-");
        add(r, "Campioni %lu  tempo %lu us", static_cast<unsigned long>(t.samples), static_cast<unsigned long>(t.elapsedUs));
        if (!t.passed && t.failedStep) add(r, "Errore: %s", t.failedStep);
        r.passed = t.passed;
        break;
    }
    case Test::AllocatorStress: {
        const auto st = allocatorStress(nowUs(), 20000, 8192);
        const bool integrity = mem::checkIntegrity();
        add(r, "Operazioni %u  allocazioni %u", static_cast<unsigned>(st.operations), static_cast<unsigned>(st.allocations));
        add(r, "Rifiutate %u  corruzioni %u", static_cast<unsigned>(st.failures), static_cast<unsigned>(st.corruptions));
        add(r, "Blocchi vivi max %u  integrita TLSF %s", static_cast<unsigned>(st.peakLive), integrity ? "OK" : "ERRORE");
        r.passed = st.corruptions == 0 && integrity;
        break;
    }
    case Test::DualCoreStress: {
#if FXAI_ON_DEVICE
        g_core1Done = false;
        if (!runtime::launchCore1(core1Stress)) { add(r, "Core 1 non avviabile"); break; }
        const auto st0 = allocatorStress(0x5EED0001u, 20000, 4096);
        const absolute_time_t until = make_timeout_time_ms(10000);
        while (!g_core1Done && !time_reached(until)) tight_loop_contents();
        const bool done = g_core1Done;
        const auto stack1 = runtime::core1Stack();
        runtime::stopCore1();
        const bool integrity = mem::checkIntegrity();
        add(r, "Core0: %u op, %u corruzioni", static_cast<unsigned>(st0.operations), static_cast<unsigned>(st0.corruptions));
        add(r, "Core1: %s, %u corruzioni", done ? "completato" : "TIMEOUT", static_cast<unsigned>(g_core1Stats.corruptions));
        std::snprintf(a, sizeof a, "%u", static_cast<unsigned>(stack1.highWaterBytes));
        add(r, "Stack core1 usato %s B (guardia MSPLIM)", a);
        add(r, "Integrita pool: %s", integrity ? "OK" : "ERRORE");
        r.passed = done && st0.corruptions == 0 && g_core1Stats.corruptions == 0 && integrity;
#else
        add(r, "solo su dispositivo");
#endif
        break;
    }
    case Test::FlashPsram: {
        const auto t = storage::testPsramAfterFlashWrite();
        if (!t.ran) { add(r, "%s", t.error ? t.error : "non eseguito"); break; }
        add(r, "Erase %lu us  program %lu us", static_cast<unsigned long>(t.eraseUs), static_cast<unsigned long>(t.programUs));
        add(r, "Flash verificata: %s", t.flashVerified ? "OK" : "ERRORE");
        add(r, "PSRAM integra (cache/diretta): %s/%s", t.psramIntactCached ? "OK" : "NO", t.psramIntactUncached ? "OK" : "NO");
        add(r, "PSRAM scrivibile dopo: %s", t.psramWritableAfter ? "OK" : "NO");
        add(r, "Config QMI M1 %s", t.m1ConfigIntact ? "intatta" : "ripristinata");
        r.passed = t.flashVerified && t.psramIntactCached && t.psramIntactUncached && t.psramWritableAfter;
        break;
    }
    case Test::Framebuffer: {
#if FXAI_ON_DEVICE
        const auto fb = board::framebuffer();
        add(r, "Framebuffer attivo in %s", fb.inPsram ? "PSRAM" : "SRAM");
        // Fill + full present from the active framebuffer.
        std::uint32_t t0 = time_us_32();
        std::fill(fb.pixels, fb.pixels + fb.bytes / 2, static_cast<std::uint16_t>(0x0841));
        const std::uint32_t fillActive = time_us_32() - t0;
        const std::uint32_t presentActive = board::benchmarkPresentFrom(fb.pixels);
        add(r, "%s: fill %lu us  present %lu us", fb.inPsram ? "PSRAM" : "SRAM", static_cast<unsigned long>(fillActive),
            static_cast<unsigned long>(presentActive));
        // Same measurement with a temporary buffer in the other memory.
        const mem::Pool other = fb.inPsram ? mem::Pool::SRAM : mem::Pool::PSRAM_BOARD;
        auto* tmp = static_cast<std::uint16_t*>(mem::tryAlloc(other, fb.bytes, 32));
        if (tmp) {
            std::memcpy(tmp, fb.pixels, fb.bytes);
            t0 = time_us_32();
            std::fill(tmp, tmp + fb.bytes / 2, static_cast<std::uint16_t>(0x0841));
            if (other == mem::Pool::PSRAM_BOARD) psram::cleanCache();
            const std::uint32_t fillOther = time_us_32() - t0;
            const std::uint32_t presentOther = board::benchmarkPresentFrom(tmp);
            add(r, "%s: fill %lu us  present %lu us", other == mem::Pool::PSRAM_BOARD ? "PSRAM" : "SRAM",
                static_cast<unsigned long>(fillOther), static_cast<unsigned long>(presentOther));
            mem::free(tmp);
        } else {
            add(r, "Confronto non disponibile (%s)", other == mem::Pool::PSRAM_BOARD ? "PSRAM assente" : "SRAM piena");
        }
        add(r, "Ultimo present() %lu us", static_cast<unsigned long>(board::lastPresentUs()));
        r.passed = true;
#else
        add(r, "solo su dispositivo");
#endif
        break;
    }
    case Test::WiFi: {
#if FXAI_ON_DEVICE
        add(r, "CYW43: %s", board::wifiReady() ? "inizializzato" : "ERRORE");
        const int found = board::wifiScan(4000);
        if (found >= 0) add(r, "Scansione: %d risultati (radio OK)", found);
        else add(r, "Scansione fallita");
        add(r, "Stato: %s", board::wifiStatus());
        add(r, "Bluetooth: %s", fx::bt::state());
        add(r, "Pin dal board header SDK: ON %u DATA %u CS %u CLK %u", board::config::kCyw43WlOn,
            board::config::kCyw43Data, board::config::kCyw43Cs, board::config::kCyw43Clock);
        r.passed = board::wifiReady() && found >= 0;
#else
        add(r, "solo su dispositivo");
#endif
        break;
    }
    case Test::Storage: {
        const auto before = storage::stats();
        if (!before.mounted) { add(r, "LittleFS non montato (err %d)", before.lastError); break; }
        char payload[64];
        std::snprintf(payload, sizeof payload, "diagnostica %lu", static_cast<unsigned long>(nowUs()));
        const bool wrote = storage::writeFile("/diag.tmp", payload, std::strlen(payload));
        char back[64] = {};
        const long got = storage::readFile("/diag.tmp", back, sizeof back - 1);
        const bool same = got == static_cast<long>(std::strlen(payload)) && std::strcmp(back, payload) == 0;
        const bool removed = storage::removeFile("/diag.tmp");
        const auto after = storage::stats();
        std::snprintf(a, sizeof a, "%u", static_cast<unsigned>(after.usedBytes / 1024u));
        std::snprintf(b, sizeof b, "%u", static_cast<unsigned>((after.usedBytes + after.freeBytes) / 1024u));
        add(r, "Scrittura %s  lettura %s  cancellazione %s", wrote ? "OK" : "NO", same ? "OK" : "NO", removed ? "OK" : "NO");
        add(r, "Occupato %s KiB su %s KiB", a, b);
        add(r, "Blocchi %u x %u B  avvii %lu", static_cast<unsigned>(after.blockCount),
            static_cast<unsigned>(after.blockSize), static_cast<unsigned long>(after.bootCount));
        r.passed = wrote && same && removed;
        break;
    }
    case Test::SdCard: {
        const auto p = sd::probe();
        if (!p.enabled) { add(r, "%s", p.error ? p.error : "disabilitata"); break; }
        add(r, "Scheda: %s", p.cardDetected ? "inserita" : "assente");
        if (p.initialized) {
            add(r, "SD v%u %s", p.version, p.highCapacity ? "SDHC/SDXC" : "SDSC");
            add(r, "Capacita %lu MiB", static_cast<unsigned long>(p.capacityBytes / (1024u * 1024u)));
        }
        if (p.error) add(r, "%s", p.error);
        r.passed = p.initialized;
        break;
    }
    case Test::Count:
        r.ran = false;
        break;
    }
    r.elapsedUs = nowUs() - start;
    std::printf("[diag] %s: %s\n", name(test), r.passed ? "OK" : "FALLITO/INFO");
    for (std::uint8_t i = 0; i < r.lineCount; ++i) std::printf("[diag]   %s\n", r.lines[i]);
    return r;
}

} // namespace fx::diag
