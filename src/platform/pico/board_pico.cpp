// PicoCalc (ClockworkPi mainboard v2.0) board port for the Pimoroni Pico Plus 2 W
// (RP2350B, 8 MB PSRAM, 16 MB flash) and the Raspberry Pi Pico 2 W (RP2350A,
// 4 MB flash). The PicoCalc wiring is the same for both modules (pins from
// board_config.hpp); only the memory resources differ.
//
// Hardware facts used here come from the official ClockworkPi sources
// (github.com/clockworkpi/PicoCalc, Code/picocalc_helloworld/lcdspi and i2ckbd,
// Code/picocalc_keyboard) and the ST7365P datasheet shipped in that repository:
//   LCD  : ST7365P 320x320, hardware SPI1, SCK GP10, MOSI GP11, MISO GP12,
//          CS GP13, DC GP14, RST GP15, 25 MHz, MADCTL 0x48, INVON,
//          COLMOD 0x66 (18-bit RGB666, 3 bytes per pixel) as in the official driver.
//   Keys : STM32 keyboard controller on I2C1 (SDA GP6, SCL GP7) at 10 kHz,
//          address 0x1F. Register read = write [reg] + STOP, wait, read 2 bytes.
//          REG_ID_FIF (0x09) returns [state, key]; REG_ID_BAT (0x0B) returns
//          [reg, percent | 0x80 when charging]; writes use reg | 0x80.
// Everything in this file still requires validation on a physical PicoCalc.
#include "board_port.hpp"
#include "board_config.hpp"
#include "flash_fs.hpp"
#include "mem_pools.hpp"
#include "net.hpp"
#include "psram.hpp"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <cstdio>

#ifndef FXAI_LCD_SPI_HZ
#define FXAI_LCD_SPI_HZ 25000000
#endif
#ifndef FXAI_LCD_RGB565
#define FXAI_LCD_RGB565 0          // 0 = official 18-bit mode (3 bytes/pixel)
#endif
#ifndef FXAI_KBD_REPLY_DELAY_MS
#define FXAI_KBD_REPLY_DELAY_MS 16 // official i2ckbd driver waits 16 ms between write and read
#endif
extern "C" {
extern char __flash_binary_end;
}

namespace board {
namespace {
// ------------------------------------------------------------------ LCD
namespace cfg = board::config;
constexpr uint LCD_SCK = cfg::kLcdSck, LCD_TX = cfg::kLcdMosi, LCD_RX = cfg::kLcdMiso, LCD_CS = cfg::kLcdCs,
               LCD_DC = cfg::kLcdDc, LCD_RST = cfg::kLcdReset;
spi_inst_t* const LCD_SPI = spi1;
constexpr int W = static_cast<int>(cfg::kLcdWidth), H = static_cast<int>(cfg::kLcdHeight);
constexpr std::size_t FrameBytes = static_cast<std::size_t>(W) * H * sizeof(std::uint16_t);
// The framebuffer stays in SRAM by default: drawing into PSRAM would go
// through the 16 KB XIP cache shared with the code running from flash, and
// present() is limited by the SPI bus anyway (docs/PICO_PLUS_2W.md).
// FXAI_FRAMEBUFFER_IN_PSRAM=ON moves it (200 KB more SRAM heap); if the PSRAM
// is not available it is allocated from the SRAM heap instead.
#if FXAI_FRAMEBUFFER_IN_PSRAM
std::uint16_t* framebuffer = nullptr;
#else
alignas(4) std::uint16_t staticFramebuffer[W * H];
std::uint16_t* framebuffer = staticFramebuffer;
#endif
bool framebufferInPsram = false;
gfx::Canvas canvas(framebuffer, W, H);
std::uint32_t presentUs = 0;
gfx::TileDiff tiles;
std::size_t lastBytes = 0;
#if FXAI_LCD_RGB565
constexpr int BytesPerPixel = 2;
#else
constexpr int BytesPerPixel = 3;
#endif
std::uint8_t lineBuffer[W * BytesPerPixel];

void lcdCommand(std::uint8_t c) {
    gpio_put(LCD_DC, 0); gpio_put(LCD_CS, 0);
    spi_write_blocking(LCD_SPI, &c, 1);
    gpio_put(LCD_CS, 1);
}
void lcdCommand(std::uint8_t c, std::initializer_list<std::uint8_t> data) {
    lcdCommand(c);
    if (data.size() == 0) return;
    gpio_put(LCD_DC, 1); gpio_put(LCD_CS, 0);
    spi_write_blocking(LCD_SPI, data.begin(), data.size());
    gpio_put(LCD_CS, 1);
}
void lcdReset() {
    gpio_put(LCD_RST, 1); sleep_ms(10);
    gpio_put(LCD_RST, 0); sleep_ms(10);
    gpio_put(LCD_RST, 1); sleep_ms(200);
}
void lcdInit() {
    gpio_init(LCD_CS); gpio_set_dir(LCD_CS, GPIO_OUT); gpio_put(LCD_CS, 1);
    gpio_init(LCD_DC); gpio_set_dir(LCD_DC, GPIO_OUT); gpio_put(LCD_DC, 1);
    gpio_init(LCD_RST); gpio_set_dir(LCD_RST, GPIO_OUT); gpio_put(LCD_RST, 1);
    spi_init(LCD_SPI, FXAI_LCD_SPI_HZ);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_TX, GPIO_FUNC_SPI);
    gpio_set_function(LCD_RX, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled(LCD_RX, true);
    lcdReset();
    // Same register sequence as ClockworkPi's pico_lcd_init() (ILI9488/ST7365P).
    lcdCommand(0xE0, {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F}); // positive gamma
    lcdCommand(0xE1, {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F}); // negative gamma
    lcdCommand(0xC0, {0x17, 0x15});          // power control 1
    lcdCommand(0xC1, {0x41});                // power control 2
    lcdCommand(0xC5, {0x00, 0x12, 0x80});    // VCOM
    lcdCommand(0x36, {0x48});                // MADCTL: MX | BGR
#if FXAI_LCD_RGB565
    lcdCommand(0x3A, {0x55});                // 16-bit RGB565 (supported by the ST7365P datasheet)
#else
    lcdCommand(0x3A, {0x66});                // 18-bit RGB666, as the official driver
#endif
    lcdCommand(0xB0, {0x00});                // interface mode
    lcdCommand(0xB1, {0xA0});                // frame rate
    lcdCommand(0x21);                        // display inversion on (the panel needs it)
    lcdCommand(0xB4, {0x02});                // inversion control
    lcdCommand(0xB6, {0x02, 0x02, 0x3B});    // display function control
    lcdCommand(0xB7, {0xC6});                // entry mode
    lcdCommand(0xE9, {0x00});
    lcdCommand(0xF7, {0xA9, 0x51, 0x2C, 0x82}); // adjust control 3
    lcdCommand(0x11); sleep_ms(120);         // sleep out
    lcdCommand(0x29); sleep_ms(120);         // display on
    lcdCommand(0x36, {0x48});
}
// Address window + RAMWR; CS stays low for the pixel data that follows.
void beginWindow(int x0, int y0, int x1, int y1) {
    const std::uint8_t cx[4] = {static_cast<std::uint8_t>(x0 >> 8), static_cast<std::uint8_t>(x0), static_cast<std::uint8_t>(x1 >> 8), static_cast<std::uint8_t>(x1)};
    const std::uint8_t cy[4] = {static_cast<std::uint8_t>(y0 >> 8), static_cast<std::uint8_t>(y0), static_cast<std::uint8_t>(y1 >> 8), static_cast<std::uint8_t>(y1)};
    std::uint8_t c;
    gpio_put(LCD_CS, 0);
    gpio_put(LCD_DC, 0); c = 0x2A; spi_write_blocking(LCD_SPI, &c, 1);
    gpio_put(LCD_DC, 1); spi_write_blocking(LCD_SPI, cx, 4);
    gpio_put(LCD_DC, 0); c = 0x2B; spi_write_blocking(LCD_SPI, &c, 1);
    gpio_put(LCD_DC, 1); spi_write_blocking(LCD_SPI, cy, 4);
    gpio_put(LCD_DC, 0); c = 0x2C; spi_write_blocking(LCD_SPI, &c, 1);
    gpio_put(LCD_DC, 1);
}
// The SPI bus (not the CPU) is the bottleneck: converting one row takes far
// less time than shifting it out, so rows are converted and sent with
// spi_write_blocking(); DMA would only overlap a few percent of the transfer.
void sendRect(const gfx::Rect& r) {
    beginWindow(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1);
    for (int y = r.y; y < r.y + r.h; ++y) {
        const std::uint16_t* src = framebuffer + static_cast<std::size_t>(y) * W + r.x;
        std::uint8_t* dst = lineBuffer;
        for (int i = 0; i < r.w; ++i) {
#if FXAI_LCD_RGB565
            *dst++ = static_cast<std::uint8_t>(src[i] >> 8);
            *dst++ = static_cast<std::uint8_t>(src[i]);
#else
            gfx::rgb565ToRgb666(src[i], dst);
            dst += 3;
#endif
        }
        spi_write_blocking(LCD_SPI, lineBuffer, static_cast<std::size_t>(r.w) * BytesPerPixel);
    }
    gpio_put(LCD_CS, 1);
    lastBytes += static_cast<std::size_t>(r.w) * r.h * BytesPerPixel;
}

// ------------------------------------------------------------------ keyboard (STM32 over I2C1)
constexpr uint KBD_SDA = cfg::kKeyboardSda, KBD_SCL = cfg::kKeyboardScl;
i2c_inst_t* const KBD_I2C = i2c1;
constexpr std::uint8_t KBD_ADDR = cfg::kKeyboardAddress;
constexpr std::uint8_t REG_BKL = 0x05, REG_FIF = 0x09, REG_BK2 = 0x0A, REG_BAT = 0x0B, WRITE = 0x80;
constexpr std::uint32_t KBD_POLL_MS = 10, BAT_POLL_MS = 10000, I2C_TIMEOUT_US = 20000;
enum class Phase { Idle, WaitReply };
Phase phase = Phase::Idle;
std::uint8_t pendingReg = 0;
std::uint32_t phaseStart = 0, lastPoll = 0, lastBattery = 0;
constexpr std::size_t KeyQueueSize = 32;
std::uint16_t keyQueue[KeyQueueSize];
std::size_t keyHead = 0, keyCount = 0;
int battery = -1; bool isCharging = false;
struct PendingWrite { std::uint8_t reg, value; bool used; };
PendingWrite writes[2] = {};
std::uint32_t i2cErrors = 0;

void pushKey(std::uint16_t raw) {
    if (keyCount == KeyQueueSize) return;   // drop on overflow (UI is far behind)
    keyQueue[(keyHead + keyCount) % KeyQueueSize] = raw; ++keyCount;
}
bool startRead(std::uint8_t reg, std::uint32_t now) {
    const int w = i2c_write_timeout_us(KBD_I2C, KBD_ADDR, &reg, 1, false, I2C_TIMEOUT_US);
    if (w != 1) { ++i2cErrors; return false; }
    pendingReg = reg; phase = Phase::WaitReply; phaseStart = now;
    return true;
}
void serviceKeyboard() {
    const std::uint32_t now = to_ms_since_boot(get_absolute_time());
    if (phase == Phase::WaitReply) {
        if (now - phaseStart < FXAI_KBD_REPLY_DELAY_MS) return;
        std::uint8_t reply[2] = {0, 0};
        const int r = i2c_read_timeout_us(KBD_I2C, KBD_ADDR, reply, 2, false, I2C_TIMEOUT_US);
        phase = Phase::Idle;
        if (r != 2) { ++i2cErrors; lastPoll = now; return; }
        if (pendingReg == REG_FIF) {
            if (reply[0] != 0 || reply[1] != 0) {
                pushKey(static_cast<std::uint16_t>((reply[0] << 8) | reply[1]));
                lastPoll = now - KBD_POLL_MS;   // drain the FIFO without waiting
            } else {
                lastPoll = now;
            }
        } else if (pendingReg == REG_BAT) {
            // 0 means "no battery" in the STM32 firmware (USB powered).
            const int pct = reply[1] & 0x7F;
            battery = pct == 0 ? -1 : std::min(pct, 100); isCharging = (reply[1] & 0x80) != 0;
            lastBattery = now;
        }
        return;
    }
    for (auto& w : writes) {
        if (!w.used) continue;
        const std::uint8_t msg[2] = {static_cast<std::uint8_t>(w.reg | WRITE), w.value};
        w.used = false;
        if (i2c_write_timeout_us(KBD_I2C, KBD_ADDR, msg, 2, false, I2C_TIMEOUT_US) == 2) {
            pendingReg = w.reg; phase = Phase::WaitReply; phaseStart = now;   // consume the reply
        } else ++i2cErrors;
        return;
    }
    const std::uint32_t batteryInterval = battery < 0 ? 1000 : BAT_POLL_MS;
    if (now - lastBattery >= batteryInterval) { lastBattery = now; if (startRead(REG_BAT, now)) return; }
    if (now - lastPoll >= KBD_POLL_MS) { lastPoll = now; (void)startRead(REG_FIF, now); }
}

// ------------------------------------------------------------------ internal flash regions
// Flash and PSRAM share the QMI: while a sector is erased or a page is
// programmed XIP is off for BOTH chips. Every erase/program therefore runs
// through flash_safe_execute() (other core locked out, IRQs masked on this
// one), the data to program is first copied into an SRAM bounce buffer (a
// source in PSRAM or in flash would be unreadable during the operation; on the
// Pico Plus 2 W operator new places large buffers such as the serialised
// workspace in PSRAM), and the QMI M1 (PSRAM) window configuration is verified
// and repaired before XIP resumes. Erases go sector by sector so that USB and
// the keyboard are serviced between them.
// 1 KB = 4 pages per flash_safe_execute(): each program job keeps the IRQs
// masked for ~2 ms instead of ~8 ms for a whole sector.
alignas(4) std::uint8_t bounce[4 * FLASH_PAGE_SIZE];
struct FlashJob { std::uint32_t offset; std::size_t len; bool erase; bool m1Intact; };
void __no_inline_not_in_flash_func(runFlashJob)(void* param) {
    auto* job = static_cast<FlashJob*>(param);
    if (job->erase) flash_range_erase(job->offset, job->len);
    else flash_range_program(job->offset, bounce, job->len);
    job->m1Intact = psram::verifyConfigurationFromSafeZone();
}
std::uint32_t flashJobs = 0, flashFailures = 0;

class FlashRegion final : public storage::FlashDevice {
public:
    FlashRegion(std::size_t offset, std::size_t size, bool writable, std::size_t reservedTail = 0)
        : offset_(offset), size_(size), writable_(writable), tail_(reservedTail) {
        // Never overlap the firmware image (a region inside the firmware
        // partition is only valid above the image, and only for reading).
        const auto binaryEnd = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(&__flash_binary_end) - XIP_BASE);
        usable_ = offset_ >= binaryEnd && offset_ + size_ <= cfg::kFlashSize;
    }
    std::size_t size() const override { return usable_ ? size_ : 0; }
    std::size_t eraseSize() const override { return FLASH_SECTOR_SIZE; }
    std::size_t programSize() const override { return FLASH_PAGE_SIZE; }
    std::size_t reservedTail() const override { return tail_; }
    bool read(std::size_t off, void* dst, std::size_t len) override {
        if (!usable_ || off + len > size_) return false;
        // flash_range_erase/program flush the XIP cache: cached reads are fresh.
        std::memcpy(dst, reinterpret_cast<const void*>(XIP_BASE + offset_ + off), len);
        return true;
    }
    bool erase(std::size_t off, std::size_t len) override {
        if (!allowed(off, len) || off % FLASH_SECTOR_SIZE || len % FLASH_SECTOR_SIZE) return false;
        for (std::size_t done = 0; done < len; done += FLASH_SECTOR_SIZE) {
            FlashJob job{static_cast<std::uint32_t>(offset_ + off + done), FLASH_SECTOR_SIZE, true, true};
            if (!execute(job)) return false;
        }
        return true;
    }
    bool program(std::size_t off, const void* src, std::size_t len) override {
        if (!allowed(off, len) || off % FLASH_PAGE_SIZE || len % FLASH_PAGE_SIZE) return false;
        const auto* in = static_cast<const std::uint8_t*>(src);
        while (len) {
            const std::size_t chunk = len < sizeof(bounce) ? len : sizeof(bounce);
            std::memcpy(bounce, in, chunk);   // copied out of PSRAM/flash while XIP still works
            FlashJob job{static_cast<std::uint32_t>(offset_ + off), chunk, false, true};
            if (!execute(job)) return false;
            off += chunk; in += chunk; len -= chunk;
        }
        return true;
    }
private:
    bool allowed(std::size_t off, std::size_t len) const {
        if (!usable_ || !writable_ || off + len > size_ || off + len < off) return false;
        // Defence in depth: nothing ever writes the reserved tail of the region
        // nor the last flash sector (RP2350-E10).
        if (tail_ && off + len > size_ - tail_) return false;
        return offset_ + off + len <= cfg::kFlashSize - cfg::kFlashTailGuardBytes;
    }
    static bool execute(FlashJob& job) {
        ++flashJobs;
        const int rc = flash_safe_execute(runFlashJob, &job, 1000);
        if (rc != PICO_OK) { ++flashFailures; std::printf("[flash] flash_safe_execute failed (%d)\n", rc); }
        return rc == PICO_OK;
    }
    std::size_t offset_, size_;
    bool writable_, usable_{};
    std::size_t tail_;
};
// Workspace record store: on the Pico 2 W it ends at the end of the flash
// (v1.8.0 location), so its last sector is the reserved tail.
constexpr std::size_t RecordTail =
    cfg::kFlashRecordOffset + cfg::kFlashRecordSize > cfg::kFlashSize - cfg::kFlashTailGuardBytes ? cfg::kFlashTailGuardBytes : 0;
FlashRegion* recordFlash = nullptr;
FlashRegion* fsFlash = nullptr;
FlashRegion* diagFlash = nullptr;
FlashRegion* legacyFlash = nullptr;
} // namespace

fx::net::ChatTransport* picoChatTransport();   // net_pico.cpp
void picoNetService();                          // net_pico.cpp: cyw43_arch_poll() once Wi-Fi is on

void init() {
    stdio_init_all();
    // Memory first: the PSRAM pool must exist before anything large is
    // allocated (operator new sends large blocks to PSRAM_BOARD when present).
    mem::init();
    (void)psram::init();          // on failure the firmware runs from SRAM only
#if FXAI_FRAMEBUFFER_IN_PSRAM
    framebuffer = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Pool::PSRAM_BOARD, FrameBytes, 64));
    framebufferInPsram = framebuffer != nullptr;
    if (!framebuffer) framebuffer = static_cast<std::uint16_t*>(mem::alloc(mem::Pool::SRAM, FrameBytes, 64));
    canvas.attach(framebuffer, W, H);
#endif
    gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
    gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
    i2c_init(KBD_I2C, 10000);
    gpio_pull_up(KBD_SCL);
    gpio_pull_up(KBD_SDA);
    lcdInit();
    canvas.clear(0xFFFF);
    tiles.invalidate();
    static FlashRegion record(cfg::kFlashRecordOffset, cfg::kFlashRecordSize, true, RecordTail);
    static FlashRegion fs(cfg::kFlashFsOffset, cfg::kFlashFsSize, true);
    static FlashRegion diag(cfg::kFlashDiagOffset, cfg::kFlashSectorBytes, true);
    recordFlash = &record; fsFlash = &fs; diagFlash = &diag;
    if constexpr (cfg::kLegacyRecordOffset + cfg::kLegacyRecordSize <= cfg::kFirmwarePartitionBytes) {
        // Pico Plus 2 W: a workspace saved by a v1.8.0 (pico2_w) firmware on
        // this module, read once to migrate it (never written).
        static FlashRegion legacy(cfg::kLegacyRecordOffset, cfg::kLegacyRecordSize, false);
        if (legacy.size()) legacyFlash = &legacy;
    }
}
gfx::Canvas& graphics() noexcept { return canvas; }
void present() {
    const std::uint32_t t0 = time_us_32();
    gfx::Rect rects[32];
    const std::size_t n = tiles.diff(framebuffer, W, H, rects, 32);
    lastBytes = 0;
    for (std::size_t i = 0; i < n; ++i) sendRect(rects[i]);
    (void)canvas.consumeDirty();
    presentUs = time_us_32() - t0;
}
void invalidate() { tiles.invalidate(); }
bool pollKey(std::uint16_t& raw) {
    if (keyCount == 0) return false;
    raw = keyQueue[keyHead]; keyHead = (keyHead + 1) % KeyQueueSize; --keyCount;
    return true;
}
void service() { serviceKeyboard(); picoNetService(); }
void idle() { sleep_ms(2); }
std::uint32_t millis() { return to_ms_since_boot(get_absolute_time()); }
int batteryPercent() { return battery; }
bool charging() { return isCharging; }
void setBacklight(std::uint8_t lcd, std::uint8_t keyboard) {
    writes[0] = {REG_BKL, lcd, true};
    writes[1] = {REG_BK2, keyboard, true};
}
std::size_t freeHeap() { return mem::stats(mem::Pool::SRAM).free; }
storage::FlashDevice& storageFlash() { return *recordFlash; }
storage::FlashDevice* legacyStorageFlash() { return legacyFlash; }
storage::FlashDevice* internalFlash() { return fsFlash; }
fx::files::FileSystem* internalFileSystem() {
    if (!fsFlash || fsFlash->size() == 0) return nullptr;
    static fx::files::LittleFsFileSystem fs(*fsFlash);   // mounted on first use
    return &fs;
}
storage::FlashDevice* diagnosticFlash() { return diagFlash; }
FramebufferInfo framebufferInfo() noexcept { return {framebuffer, FrameBytes, framebufferInPsram, FXAI_FRAMEBUFFER_IN_PSRAM != 0}; }
std::uint32_t lastPresentUs() noexcept { return presentUs; }
std::uint32_t benchmarkFullPresent() {
    tiles.invalidate();
    present();
    return presentUs;
}
FlashStats flashStats() noexcept { return {flashJobs, flashFailures}; }
std::size_t firmwareBytes() noexcept {
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(&__flash_binary_end) - XIP_BASE);
}
fx::net::ChatTransport* chatTransport() { return picoChatTransport(); }
const char* name() { return board::config::kIsPicoPlus2W ? "PicoCalc / Pico Plus 2 W" : "PicoCalc / Pico 2 W"; }
std::size_t lastFlushBytes() { return lastBytes; }
} // namespace board
