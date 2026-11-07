// AI chat: request/response protocol and the chat app driven through a mock transport.
#include "ai_protocol.hpp"
#include "apps.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "firmware.hpp"
#include "net.hpp"
#include "test_util.hpp"
#include <string>

using namespace fx;

namespace {
// Scripted transport: replays a list of states, then Done with a canned response.
class MockTransport final : public net::ChatTransport {
public:
    bool connected{true};
    std::vector<net::ChatState> script;
    int status{200};
    std::string body{"Risposta"}, err{"Timeout"};
    net::ChatRequest last;
    int begins{0}, cancels{0}, wifiConfigs{0};
    bool configureWifi(const std::string&, const std::string&) override { ++wifiConfigs; return true; }
    bool wifiConnected() override { return connected; }
    const char* wifiStatus() override { return connected ? "connesso" : "connessione..."; }
    bool begin(const net::ChatRequest& r) override { if (active_) return false; last = r; active_ = true; step_ = 0; ++begins; return true; }
    net::ChatState poll() override {
        if (!active_) return net::ChatState::Idle;
        if (step_ < script.size()) return script[step_++];
        return net::ChatState::Done;
    }
    int httpStatus() const override { return status; }
    const std::string& response() const override { return body; }
    const std::string& error() const override { return err; }
    void cancel() override { active_ = false; ++cancels; }
private:
    bool active_{false};
    std::size_t step_{0};
};
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
}

int main() {
    using ai::HttpResponseParser;
    // --- JSON escaping and request body
    CHECK_EQ_STR(ai::jsonEscape("a\"b\\c\n\t\x01"), "a\\\"b\\\\c\\n\\t\\u0001");
    CHECK_EQ_STR(ai::jsonEscape("\xCF\x80=3.14"), "\xCF\x80=3.14");       // valid UTF-8 kept
    CHECK_EQ_STR(ai::jsonEscape("x\xFFy"), "x\xEF\xBF\xBDy");            // invalid byte -> U+FFFD
    std::vector<ChatMessage> h = {{true, "Ciao"}, {false, "Salve!"}, {true, "Quanto fa 2+2?"}};
    const std::string body = ai::buildChatBody(h, 0, "", 1200);
    CHECK_EQ_STR(body, "{\"provider\":\"claude\",\"max_reply_chars\":1200,\"messages\":[{\"role\":\"user\",\"content\":\"Ciao\"},"
                       "{\"role\":\"assistant\",\"content\":\"Salve!\"},{\"role\":\"user\",\"content\":\"Quanto fa 2+2?\"}]}");
    CHECK(ai::buildChatBody(h, 1, "gpt-x", 500).find("\"provider\":\"chatgpt\",\"model\":\"gpt-x\"") != std::string::npos);
    // Size budget: old messages are dropped and the oldest kept one is from the user.
    std::vector<ChatMessage> long_h;
    for (int i = 0; i < 20; ++i) { long_h.push_back({true, std::string(400, 'q')}); long_h.push_back({false, std::string(400, 'a')}); }
    long_h.push_back({true, "ultima"});
    const std::string trimmed = ai::buildChatBody(long_h, 0, "", 1200, 2000);
    CHECK(trimmed.size() <= 2000);
    CHECK(trimmed.find("\"messages\":[{\"role\":\"user\"") != std::string::npos);
    CHECK(trimmed.find("ultima") != std::string::npos);
    CHECK_THROWS(ai::buildChatBody({{false, "x"}}, 0, "", 100));
    CHECK_THROWS(ai::buildChatBody({{true, std::string(5000, 'z')}}, 0, "", 100, 1000));
    const std::string req = ai::buildHttpRequest("relay.example.org", "/v1/chat", "tok", "{}");
    CHECK(req.rfind("POST /v1/chat HTTP/1.1\r\nHost: relay.example.org\r\n", 0) == 0);
    CHECK(req.find("Authorization: Bearer tok\r\n") != std::string::npos);
    CHECK(req.find("Content-Length: 2\r\n\r\n{}") != std::string::npos);
    // --- HTTP response parser
    {
        HttpResponseParser p;
        const std::string r = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 5\r\nX-Relay-Model: m\r\n\r\nCiao!";
        for (char ch : r) CHECK(p.feed(&ch, 1));                  // byte by byte
        CHECK(p.complete() && p.status() == 200 && p.body() == "Ciao!" && p.header("x-relay-model") == "m");
    }
    {
        HttpResponseParser p;
        const std::string r = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n6;ext=1\r\npedia!\r\n0\r\n\r\n";
        CHECK(p.feed(r.data(), r.size()) && p.complete() && p.body() == "Wikipedia!");
    }
    {
        HttpResponseParser p;
        const std::string r = "HTTP/1.0 429 Too Many Requests\r\nContent-Type: text/plain\r\n\r\nTroppe richieste";
        CHECK(p.feed(r.data(), r.size()) && !p.complete());
        CHECK(p.finish() && p.status() == 429 && p.body() == "Troppe richieste");   // close-delimited
    }
    {
        HttpResponseParser p(10);
        const std::string r = "HTTP/1.1 200 OK\r\nContent-Length: 50\r\n\r\n";
        CHECK(!p.feed(r.data(), r.size()) && p.failed());       // larger than the device limit
        HttpResponseParser q;
        const std::string bad = "garbage\r\n\r\n";
        CHECK(!q.feed(bad.data(), bad.size()));
        HttpResponseParser c;
        const std::string cut = "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc";
        CHECK(c.feed(cut.data(), cut.size()) && !c.finish());   // truncated body is an error
        HttpResponseParser e;
        CHECK(!e.finish());                                     // closed before any response
    }
    CHECK_EQ_STR(ai::sanitizeText("a\tb\r\nc\x07\xFF"), "a  b\nc?");
    // --- chat app through the firmware with a mock transport
    board::init();
    MockTransport mock;
    board::host::setChatTransport(&mock);
    BoardPlatform platform;
    Firmware fw(platform, board::storageFlash());
    fw.boot();
    auto step = [&] { board::host::advance(50); if (fw.step()) { fw.draw(board::graphics()); board::present(); } };
    auto key = [&](std::uint8_t code) { fw.rawKey(press(code)); step(); fw.rawKey(press(code, 3)); step(); };
    auto type = [&](const std::string& s) { for (char c : s) { fw.rawKey(press(static_cast<std::uint8_t>(c))); step(); } };
    Workspace& ws = fw.workspace();
    type("m");
    CHECK(fw.apps().activeId() == app::AppId::AiChat);
    auto* chat = static_cast<app::AiChatApp*>(fw.apps().active());
    CHECK(chat->status().find("Configura") != std::string::npos);   // not configured yet
    type("ciao"); key(pckey::Enter);
    CHECK(fw.apps().errorShown() && mock.begins == 0);
    key(pckey::Esc);
    ws.settings.wifiSsid = "rete"; ws.settings.relayHost = "relay.example.org"; ws.settings.relayToken = "device-token";
    mock.script = {net::ChatState::Connecting, net::ChatState::Sending, net::ChatState::Waiting};
    mock.body = "Ciao! Come posso aiutarti?";
    key(pckey::Enter);                                         // input "ciao" is still there
    CHECK(mock.begins == 1 && chat->busy());
    CHECK(mock.last.host == "relay.example.org" && mock.last.port == 443 && mock.last.token == "device-token");
    CHECK(mock.last.body.find("\"content\":\"ciao\"") != std::string::npos);
    for (int i = 0; i < 6; ++i) step();
    CHECK(!chat->busy());
    CHECK(ws.chat.size() == 2 && !ws.chat[1].fromUser && ws.chat[1].text == "Ciao! Come posso aiutarti?");
    // HTTP error from the relay is shown, not stored as an answer.
    mock.status = 429; mock.body = "Troppe richieste: attendi un minuto"; mock.script = {};
    type("ancora"); key(pckey::Enter); step(); step();
    CHECK(ws.chat.size() == 3 && chat->status().find("429") != std::string::npos);
    // Transport failure.
    mock.status = 200; mock.script = {net::ChatState::Failed};
    type("x"); key(pckey::Enter); step(); step();
    CHECK(chat->status().find("Timeout") != std::string::npos && !chat->busy());
    // Provider toggle and new conversation.
    key(pckey::F3); CHECK(ws.settings.aiProvider == 1);
    key(pckey::F2); CHECK(ws.chat.empty());
    // Cancel an in-flight request.
    mock.script = std::vector<net::ChatState>(50, net::ChatState::Waiting);
    type("lunga"); key(pckey::Enter); CHECK(chat->busy());
    key(pckey::F5); CHECK(!chat->busy() && mock.cancels >= 1);
    board::host::setChatTransport(nullptr);
    return testutil::finish("ai_test");
}
