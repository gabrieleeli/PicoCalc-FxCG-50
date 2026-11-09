// Lines are clipped to the 52-character screen width on purpose.
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "system_info.hpp"

#include "board_config.hpp"
#include "limits.hpp"

#include <cstdarg>
#include <cstdio>

#ifndef FXAI_VERSION_STRING
#define FXAI_VERSION_STRING "dev"
#endif

namespace fx::sysinfo {
namespace {
__attribute__((format(printf, 4, 5)))
void line(char (*lines)[kLineChars], std::size_t maxLines, std::size_t& n, const char* fmt, ...) noexcept {
    if (n >= maxLines) return;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(lines[n], kLineChars, fmt, ap);
    va_end(ap);
    ++n;
}
} // namespace

void formatSize(char* out, std::size_t capacity, std::size_t bytes) noexcept {
    if (bytes >= 1024u * 1024u)
        std::snprintf(out, capacity, "%.1fM", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 10u * 1024u)
        std::snprintf(out, capacity, "%uK", static_cast<unsigned>(bytes / 1024u));
    else if (bytes >= 1024u)
        std::snprintf(out, capacity, "%.1fK", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(out, capacity, "%u", static_cast<unsigned>(bytes));
}

Report collect(const Hardware& hw) noexcept {
    Report r;
    r.board = board::config::kBoardName;
    r.version = FXAI_VERSION_STRING;
    r.sram = mem::stats(mem::Pool::SRAM);
    r.psram = mem::stats(mem::Pool::PSRAM_BOARD);
    r.outOfMemoryEvents = mem::outOfMemoryEvents();
    r.invalidFrees = mem::invalidFrees();
    r.core0 = runtime::core0Stack();
    r.core1 = runtime::core1Stack();
    r.psramInfo = psram::info();
    r.hw = hw;
    return r;
}

std::size_t formatMemory(const Report& r, char (*lines)[kLineChars], std::size_t maxLines) noexcept {
    namespace cfg = board::config;
    std::size_t n = 0;
    char a[16], b[16], c[16], d[16];
    line(lines, maxLines, n, "%s  v%s", r.board ? r.board : "?", r.version ? r.version : "?");

    formatSize(a, sizeof a, r.sram.used);
    formatSize(b, sizeof b, r.sram.total);
    formatSize(c, sizeof c, r.sram.free);
    if (r.sram.total)
        line(lines, maxLines, n, "SRAM heap usata %s/%s  libera %s", a, b, c);
    else
        line(lines, maxLines, n, "SRAM heap usata %s (host: illimitata)", a);
    if (r.sram.total) {   // the host heap has no meaningful largest block
        formatSize(a, sizeof a, r.sram.largestFree);
        formatSize(b, sizeof b, r.sram.peakUsed);
        line(lines, maxLines, n, "  blocco max %s  picco %s  err %u", a, b, static_cast<unsigned>(r.sram.failures));
    }

    if (r.psram.available) {
        formatSize(a, sizeof a, r.psram.used);
        formatSize(b, sizeof b, r.psram.total);
        formatSize(c, sizeof c, r.psram.free);
        line(lines, maxLines, n, "PSRAM usata %s/%s  libera %s", a, b, c);
        formatSize(a, sizeof a, r.psram.largestFree);
        formatSize(b, sizeof b, r.psram.peakUsed);
        line(lines, maxLines, n, "  blocco max %s  picco %s  err %u", a, b, static_cast<unsigned>(r.psram.failures));
        if (r.psramInfo.sckHz)
            line(lines, maxLines, n, "  SCK %lu MHz  self-test avvio %s", static_cast<unsigned long>(r.psramInfo.sckHz / 1000000u),
                 r.psramInfo.bootSelfTest.passed ? "OK" : "-");
    } else if (cfg::kHasPsram) {
        line(lines, maxLines, n, "PSRAM DISABILITATA: %s", r.psramInfo.error ? r.psramInfo.error : "errore");
    } else {
        line(lines, maxLines, n, "PSRAM non presente su questa scheda");
    }
    line(lines, maxLines, n, "Limiti RAM: %s", limits::ram().name);

    formatSize(a, sizeof a, r.hw.firmwareBytes);
    formatSize(b, sizeof b, cfg::kFirmwarePartitionBytes);
    formatSize(c, sizeof c, cfg::kFlashSize);
    line(lines, maxLines, n, "FLASH firmware %s/%s  totale %s", a, b, c);
    formatSize(a, sizeof a, cfg::kFlashFsSize);
    formatSize(b, sizeof b, cfg::kFlashRecordSize);
    line(lines, maxLines, n, "  LittleFS %s  workspace %s (A/B)", a, b);
    line(lines, maxLines, n, "  scritture %lu  errori %lu", static_cast<unsigned long>(r.hw.flashJobs),
         static_cast<unsigned long>(r.hw.flashFailures));

    formatSize(a, sizeof a, r.core0.highWaterBytes);
    formatSize(b, sizeof b, r.core0.sizeBytes);
    formatSize(c, sizeof c, r.core1.highWaterBytes);
    formatSize(d, sizeof d, r.core1.sizeBytes);
    if (r.core0.sizeBytes)
        line(lines, maxLines, n, "STACK core0 %s/%s  core1 %s/%s", a, b, c, d);
    else
        line(lines, maxLines, n, "STACK n/d sull'host");
    line(lines, maxLines, n, "  guardia MSPLIM %s / %s", r.core0.guarded ? "attiva" : "-", r.core1.guarded ? "attiva" : "-");

    formatSize(a, sizeof a, r.hw.framebufferBytes);
    line(lines, maxLines, n, "FRAMEBUFFER %s in %s%s", a, r.hw.framebufferInPsram ? "PSRAM" : "SRAM",
         r.hw.framebufferRequestedPsram && !r.hw.framebufferInPsram ? " (ripiego)" : "");
    if (r.hw.lastPresentUs)
        line(lines, maxLines, n, "  ultimo aggiornamento LCD %lu us", static_cast<unsigned long>(r.hw.lastPresentUs));
    line(lines, maxLines, n, "Allocazioni fallite %u  free non validi %u", static_cast<unsigned>(r.outOfMemoryEvents),
         static_cast<unsigned>(r.invalidFrees));
    return n;
}

} // namespace fx::sysinfo
