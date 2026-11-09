#pragma once
#include "workspace.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace fx::ai {

// JSON string escaping (RFC 8259) for UTF-8 input; invalid bytes become U+FFFD.
std::string jsonEscape(const std::string& s);

// Body of POST /v1/chat for the PicoCalc relay. Uses the most recent messages
// (the last one must be from the user) within maxBytes, oldest dropped first.
std::string buildChatBody(const std::vector<ChatMessage>& history, std::uint8_t provider,
                          const std::string& model, unsigned maxReplyChars, std::size_t maxBytes = 12000);

// Complete HTTP/1.1 request (Connection: close).
std::string buildHttpRequest(const std::string& host, const std::string& path, const std::string& token,
                             const std::string& body);

// Incremental HTTP/1.1 response parser: Content-Length, chunked and
// close-delimited bodies. Bodies larger than maxBody are an error.
class HttpResponseParser {
public:
    explicit HttpResponseParser(std::size_t maxBody = 16384) : maxBody_(maxBody) {}
    void reset() { *this = HttpResponseParser(maxBody_); }
    // Returns false once a protocol error occurred.
    bool feed(const char* data, std::size_t length);
    // Call when the connection closed; returns true when a complete response was received.
    bool finish();
    bool complete() const noexcept { return complete_; }
    bool failed() const noexcept { return !error_.empty(); }
    const std::string& error() const noexcept { return error_; }
    int status() const noexcept { return status_; }
    const std::string& body() const noexcept { return body_; }
    std::string header(const std::string& name) const;   // case-insensitive, first value
private:
    enum class Stage { Headers, Body, ChunkSize, ChunkData, ChunkDataEnd, Trailers, Done };
    bool fail(const char* why) { error_ = why; return false; }
    bool parseHead();
    std::size_t maxBody_;
    Stage stage_{Stage::Headers};
    std::string head_, body_, line_, error_;
    std::vector<std::pair<std::string, std::string>> headers_;
    int status_{0};
    bool chunked_{false}, hasLength_{false}, complete_{false};
    std::size_t remaining_{0};
};

// Makes relay text safe for the device: valid UTF-8, no control characters
// except newlines, tabs expanded.
std::string sanitizeText(const std::string& s);
} // namespace fx::ai
