// Host implementation of the board interface: RAM framebuffer, simulated
// clock, scripted key queue and a RAM flash model. Used by the simulator and
// the integration tests; it is not evidence that the physical LCD works.
#include "board_host.hpp"
#include "board_config.hpp"
#include "board_port.hpp"
#include "file_store.hpp"
#include "flash_fs.hpp"
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

namespace board {
namespace {
constexpr int W = 320, H = 320;
std::uint16_t fb[W * H];
gfx::Canvas canvas(fb, W, H);
gfx::TileDiff diff;
std::deque<std::uint16_t> keys;
std::uint32_t nowMs = 1000;
int battery = 87; bool isCharging = false;
// Same geometry as the device: record store (with the Pico 2 W reserved tail),
// LittleFS partition (capped at 2 MB to keep the host RAM use small) and the
// diagnostic sector.
storage::RamFlash flash(board::config::kFlashRecordSize);
storage::RamFlash fsFlash(std::min<std::size_t>(board::config::kFlashFsSize, 2u * 1024u * 1024u));
storage::RamFlash diagFlash(board::config::kFlashSectorBytes);
storage::FlashDevice* legacyFlash = nullptr;
[[maybe_unused]] bool flashConfigured = [] {
    if (board::config::kFlashRecordOffset + board::config::kFlashRecordSize >
        board::config::kFlashSize - board::config::kFlashTailGuardBytes)
        flash.setReservedTail(board::config::kFlashTailGuardBytes);
    return true;
}();
std::size_t flushes = 0, lastBytes = 0;
fx::net::ChatTransport* transport = nullptr;
fx::files::RamFileSystem card(16u * 1024u * 1024u);   // simulated microSD card
fx::files::FileSystem* files = &card;

// Minimal PNG writer (stored deflate blocks, no external dependency).
std::uint32_t crcTable[256];
void initCrc() { for (std::uint32_t n = 0; n < 256; ++n) { std::uint32_t c = n; for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcTable[n] = c; } }
std::uint32_t crc(const std::uint8_t* p, std::size_t n, std::uint32_t c = 0xFFFFFFFFu) { for (std::size_t i = 0; i < n; ++i) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8); return c; }
void be32(std::vector<std::uint8_t>& v, std::uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
void chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    be32(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> td(type, type + 4); td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    be32(out, crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
}
} // namespace

void init() { initCrc(); canvas.clear(0); diff.invalidate(); }
gfx::Canvas& graphics() noexcept { return canvas; }
void present() {
    gfx::Rect rects[32];
    const std::size_t n = diff.diff(fb, W, H, rects, 32);
    lastBytes = 0;
    for (std::size_t i = 0; i < n; ++i) lastBytes += static_cast<std::size_t>(rects[i].w * rects[i].h) * 3;
    if (n) ++flushes;
    (void)canvas.consumeDirty();
}
void invalidate() { diff.invalidate(); }
bool pollKey(std::uint16_t& raw) { if (keys.empty()) return false; raw = keys.front(); keys.pop_front(); return true; }
void service() {}
void idle() { nowMs += 10; }
std::uint32_t millis() { return nowMs; }
int batteryPercent() { return battery; }
bool charging() { return isCharging; }
void setBacklight(std::uint8_t, std::uint8_t) {}
std::size_t freeHeap() { return 256 * 1024; }
storage::FlashDevice& storageFlash() { return flash; }
storage::FlashDevice* legacyStorageFlash() { return legacyFlash; }
storage::FlashDevice* internalFlash() { return &fsFlash; }
fx::files::FileSystem* internalFileSystem() {
    static fx::files::LittleFsFileSystem fs(fsFlash);
    return &fs;
}
storage::FlashDevice* diagnosticFlash() { return &diagFlash; }
FramebufferInfo framebufferInfo() noexcept { return {fb, sizeof(fb), false, false}; }
std::uint32_t lastPresentUs() noexcept { return 0; }
std::uint32_t benchmarkFullPresent() { diff.invalidate(); present(); return 0; }
FlashStats flashStats() noexcept { return {0, 0}; }
std::size_t firmwareBytes() noexcept { return 0; }
fx::net::ChatTransport* chatTransport() { return transport; }
fx::files::FileSystem* fileSystem() { return files; }
const char* name() { return "host"; }
std::size_t lastFlushBytes() { return lastBytes; }

namespace host {
void pushKey(std::uint16_t raw) { keys.push_back(raw); }
void advance(std::uint32_t ms) { nowMs += ms; }
std::size_t flushCount() { return flushes; }
void setBattery(int p, bool c) { battery = p; isCharging = c; }
void setChatTransport(fx::net::ChatTransport* t) { transport = t; }
void setFileSystem(fx::files::FileSystem* fs) { files = fs; }
void setLegacyStorage(storage::FlashDevice* device) { legacyFlash = device; }
fx::files::FileSystem& ramCard() { return card; }
bool savePng(const std::string& path, int scale) {
    if (scale < 1) scale = 1;
    const int w = W * scale, h = H * scale;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (1 + w * 3));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < w; ++x) {
            const auto c = gfx::rgb565To888(fb[(y / scale) * W + x / scale]);
            raw.push_back(c.r); raw.push_back(c.g); raw.push_back(c.b);
        }
    }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::size_t pos = 0;
    while (pos < raw.size()) {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        z.push_back(pos + len == raw.size() ? 1 : 0);
        z.push_back(len & 0xFF); z.push_back(len >> 8); z.push_back(~len & 0xFF); z.push_back((~len >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    }
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    be32(z, (b << 16) | a);
    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr; be32(ihdr, static_cast<std::uint32_t>(w)); be32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", ihdr); chunk(png, "IDAT", z); chunk(png, "IEND", {});
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    std::fclose(f);
    return ok;
}
} // namespace host
} // namespace board
