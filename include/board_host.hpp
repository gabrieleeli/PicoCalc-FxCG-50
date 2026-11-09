#pragma once
#include <cstdint>
#include <string>
namespace fx::net { class ChatTransport; }
namespace fx::files { class FileSystem; }
namespace storage { class FlashDevice; }

// Host-only extensions of the board interface (simulator and tests).
namespace board::host {
void pushKey(std::uint16_t raw);              // queue a FIFO word (state<<8 | code)
void advance(std::uint32_t ms);               // advance the simulated clock
bool savePng(const std::string& path, int scale = 2);
std::size_t flushCount();                     // number of present() calls that sent pixels
void setBattery(int percent, bool charging);
void setChatTransport(fx::net::ChatTransport* transport);   // tests: inject a mock
void setFileSystem(fx::files::FileSystem* fs);              // tests: replace the RAM card (nullptr: none)
fx::files::FileSystem& ramCard();                            // the default RAM card
void setLegacyStorage(storage::FlashDevice* device);         // tests: v1.8.0 record to migrate
} // namespace board::host
