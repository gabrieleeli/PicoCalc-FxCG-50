// Lines are clipped to the 52-character screen width on purpose.
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "system_info.hpp"

#include "board_config.hpp"

#include <cstdarg>
#include <cstdio>

namespace fx::sysinfo {
namespace {
FramebufferPlacement g_framebuffer{};

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

void setFramebuffer(const FramebufferPlacement& placement) noexcept { g_framebuffer = placement; }

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

Report collect() noexcept {
    Report r;
    r.board = board::config::kBoardName;
    r.version = board::config::kFirmwareVersion;
    r.sram = mem::stats(mem::Pool::SRAM);
    r.psram = mem::stats(mem::Pool::PSRAM_BOARD);
    r.psramPicocalc = mem::stats(mem::Pool::PSRAM_PICOCALC);
    r.outOfMemoryEvents = mem::outOfMemoryEvents();
    r.core0 = runtime::core0Stack();
    r.core1 = runtime::core1Stack();
    r.flash = storage::layout();
    r.fs = storage::stats();
    r.framebuffer = g_framebuffer;
    r.psramInfo = psram::info();
    return r;
}

std::size_t formatMemory(const Report& r, char (*lines)[kLineChars], std::size_t maxLines) noexcept {
    std::size_t n = 0;
    char a[16], b[16], c[16], d[16];
    line(lines, maxLines, n, "%s  v%s", r.board ? r.board : "?", r.version ? r.version : "?");

    formatSize(a, sizeof a, r.sram.used);
    formatSize(b, sizeof b, r.sram.total);
    formatSize(c, sizeof c, r.sram.free);
    line(lines, maxLines, n, "SRAM   usata %s/%s  libera %s", a, b, c);
    formatSize(a, sizeof a, r.sram.largestFree);
    formatSize(b, sizeof b, r.sram.peakUsed);
    line(lines, maxLines, n, "       blocco max %s  picco %s  err %u", a, b, static_cast<unsigned>(r.sram.failures));

    if (r.psram.available) {
        formatSize(a, sizeof a, r.psram.used);
        formatSize(b, sizeof b, r.psram.total);
        formatSize(c, sizeof c, r.psram.free);
        line(lines, maxLines, n, "PSRAM  usata %s/%s  libera %s", a, b, c);
        formatSize(a, sizeof a, r.psram.largestFree);
        formatSize(b, sizeof b, r.psram.peakUsed);
        line(lines, maxLines, n, "       blocco max %s  picco %s  err %u", a, b, static_cast<unsigned>(r.psram.failures));
        line(lines, maxLines, n, "       SCK %lu MHz  test avvio %s", static_cast<unsigned long>(r.psramInfo.sckHz / 1000000u),
             r.psramInfo.bootSelfTest.passed ? "OK" : "-");
    } else if (board::config::kHasPsram) {
        line(lines, maxLines, n, "PSRAM  DISABILITATA: %s", r.psramInfo.error ? r.psramInfo.error : "errore");
    } else {
        line(lines, maxLines, n, "PSRAM  non presente su questa scheda");
    }
    line(lines, maxLines, n, "PSRAM PicoCalc: interfaccia pronta, nessun driver");

    formatSize(a, sizeof a, r.flash.firmwareUsed);
    formatSize(b, sizeof b, r.flash.firmwarePartition);
    formatSize(c, sizeof c, r.flash.flashSize);
    line(lines, maxLines, n, "FLASH  firmware %s/%s  totale %s", a, b, c);
    if (r.fs.mounted) {
        formatSize(a, sizeof a, r.fs.usedBytes);
        formatSize(b, sizeof b, r.fs.usedBytes + r.fs.freeBytes);
        formatSize(c, sizeof c, r.fs.freeBytes);
        formatSize(d, sizeof d, r.fs.peakUsedBytes);
        line(lines, maxLines, n, "       LittleFS %s/%s libera %s", a, b, c);
        line(lines, maxLines, n, "       picco %s  avvii %lu%s", d, static_cast<unsigned long>(r.fs.bootCount),
             r.fs.formattedAtBoot ? "  (formattato)" : "");
    } else {
        line(lines, maxLines, n, "       LittleFS non montato (err %d)", r.fs.lastError);
    }

    formatSize(a, sizeof a, r.core0.highWaterBytes);
    formatSize(b, sizeof b, r.core0.sizeBytes);
    formatSize(c, sizeof c, r.core1.highWaterBytes);
    formatSize(d, sizeof d, r.core1.sizeBytes);
    line(lines, maxLines, n, "STACK  core0 %s/%s  core1 %s/%s", a, b, c, d);
    line(lines, maxLines, n, "       guardia MSPLIM %s / %s", r.core0.guarded ? "attiva" : "-", r.core1.guarded ? "attiva" : "-");

    formatSize(a, sizeof a, r.framebuffer.bytes);
    line(lines, maxLines, n, "FRAMEBUFFER %s in %s%s", a, r.framebuffer.inPsram ? "PSRAM" : "SRAM",
         r.framebuffer.requestedPsram && !r.framebuffer.inPsram ? " (fallback)" : "");
    line(lines, maxLines, n, "Allocazioni fallite (OOM): %u", static_cast<unsigned>(r.outOfMemoryEvents));
    return n;
}

} // namespace fx::sysinfo
