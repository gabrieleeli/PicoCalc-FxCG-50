#pragma once
#include <cstddef>
#include <memory_resource>
#include <string>
namespace fx {
enum class AIProvider { ChatGPT, Claude };
bool aiReady();
std::string aiChat(AIProvider provider,const std::string& prompt);

// Request/response buffers for the (future) HTTPS relay client. Capacities
// come from fx::limits (8/16 KiB in SRAM on the Pico 2 W, up to 256 KiB /
// 2 MiB in PSRAM on the Pico Plus 2 W); appending beyond the capacity or when
// the pool is exhausted fails cleanly instead of throwing.
class AiBuffer {
public:
    explicit AiBuffer(std::size_t capacity);
    bool append(const char* data, std::size_t length) noexcept;
    void clear() noexcept { text_.clear(); }
    const std::pmr::string& text() const noexcept { return text_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool truncated() const noexcept { return truncated_; }
private:
    std::pmr::string text_;
    std::size_t capacity_;
    bool truncated_{};
};
std::size_t aiRequestCapacity();
std::size_t aiResponseCapacity();
}
