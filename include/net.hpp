#pragma once
#include <cstdint>
#include <string>

namespace fx::net {

enum class ChatState : std::uint8_t { Idle, Connecting, Sending, Waiting, Done, Failed };

struct ChatRequest {
    std::string host;
    std::uint16_t port{443};
    std::string path;
    std::string token;        // device token for the relay (never a provider API key)
    std::string body;         // JSON request body
};

// Asynchronous HTTPS POST to the AI relay. Implementations must never block
// the UI loop: begin() starts the request and poll() advances it.
class ChatTransport {
public:
    virtual ~ChatTransport() = default;
    virtual bool configureWifi(const std::string& ssid, const std::string& password) = 0;
    virtual bool wifiConnected() = 0;
    virtual const char* wifiStatus() = 0;
    virtual bool begin(const ChatRequest& request) = 0;
    virtual ChatState poll() = 0;
    virtual int httpStatus() const = 0;
    virtual const std::string& response() const = 0;
    virtual const std::string& error() const = 0;
    virtual void cancel() = 0;
};

} // namespace fx::net
