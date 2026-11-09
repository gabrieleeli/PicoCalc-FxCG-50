#include "ai_protocol.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace fx::ai {
namespace {
// Length of a valid UTF-8 sequence starting at s[i], 0 if invalid.
std::size_t utf8Seq(const std::string& s, std::size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    std::size_t n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (n == 0 || (n == 2 && c < 0xC2) || i + n > s.size()) return 0;
    for (std::size_t k = 1; k < n; ++k) if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 0;
    return n;
}
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}
} // namespace

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                else out += static_cast<char>(c);
            }
            ++i;
            continue;
        }
        const std::size_t n = utf8Seq(s, i);
        if (n == 0) { out += "\xEF\xBF\xBD"; ++i; continue; }
        out.append(s, i, n);
        i += n;
    }
    return out;
}

std::string buildChatBody(const std::vector<ChatMessage>& history, std::uint8_t provider, const std::string& model,
                          unsigned maxReplyChars, std::size_t maxBytes) {
    if (history.empty() || !history.back().fromUser) throw std::invalid_argument("L'ultimo messaggio deve essere dell'utente");
    const std::string head = std::string("{\"provider\":\"") + (provider == 1 ? "chatgpt" : "claude") + "\"" +
                             (model.empty() ? "" : ",\"model\":\"" + jsonEscape(model) + "\"") +
                             ",\"max_reply_chars\":" + std::to_string(maxReplyChars) + ",\"messages\":[";
    // Walk backwards, keeping as many complete messages as fit; start with a user message.
    std::vector<std::string> items;
    std::size_t used = head.size() + 2;
    for (std::size_t k = history.size(); k-- > 0;) {
        const auto& m = history[k];
        std::string item = std::string("{\"role\":\"") + (m.fromUser ? "user" : "assistant") + "\",\"content\":\"" + jsonEscape(m.text) + "\"}";
        if (used + item.size() + 1 > maxBytes) break;
        used += item.size() + 1;
        items.push_back(std::move(item));
    }
    if (items.empty()) throw std::length_error("Messaggio troppo lungo per il relay");
    // The oldest kept message must be from the user (drop a leading assistant reply).
    while (items.size() > 1 && items.back().rfind("{\"role\":\"assistant\"", 0) == 0) items.pop_back();
    std::string body = head;
    for (std::size_t k = items.size(); k-- > 0;) { body += items[k]; if (k) body += ","; }
    body += "]}";
    return body;
}

std::string buildHttpRequest(const std::string& host, const std::string& path, const std::string& token, const std::string& body) {
    std::string r = "POST " + (path.empty() ? std::string("/v1/chat") : path) + " HTTP/1.1\r\n";
    r += "Host: " + host + "\r\n";
    r += "User-Agent: PicoCalcFX/" FXAI_VERSION_STRING "\r\n";
    r += "Authorization: Bearer " + token + "\r\n";
    r += "Content-Type: application/json\r\n";
    r += "Accept: text/plain\r\n";
    r += "Connection: close\r\n";
    r += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    r += body;
    return r;
}

bool HttpResponseParser::parseHead() {
    // Status line.
    const auto eol = head_.find("\r\n");
    const std::string statusLine = head_.substr(0, eol);
    if (statusLine.rfind("HTTP/1.", 0) != 0 || statusLine.size() < 12) return fail("Risposta HTTP non valida");
    status_ = std::atoi(statusLine.c_str() + 9);
    if (status_ < 100 || status_ > 599) return fail("Codice HTTP non valido");
    std::size_t pos = eol + 2;
    while (pos < head_.size()) {
        const auto next = head_.find("\r\n", pos);
        const std::string line = head_.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        pos = next == std::string::npos ? head_.size() : next + 2;
        if (line.empty()) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) return fail("Header HTTP non valido");
        headers_.emplace_back(lower(trim(line.substr(0, colon))), trim(line.substr(colon + 1)));
    }
    chunked_ = lower(header("transfer-encoding")).find("chunked") != std::string::npos;
    const std::string len = header("content-length");
    if (!chunked_ && !len.empty()) {
        char* end = nullptr;
        const unsigned long long v = std::strtoull(len.c_str(), &end, 10);
        if (!end || *end != '\0') return fail("Content-Length non valido");
        if (v > maxBody_) return fail("Risposta troppo grande");
        hasLength_ = true; remaining_ = static_cast<std::size_t>(v);
    }
    stage_ = chunked_ ? Stage::ChunkSize : Stage::Body;
    if (!chunked_ && hasLength_ && remaining_ == 0) { stage_ = Stage::Done; complete_ = true; }
    return true;
}

bool HttpResponseParser::feed(const char* data, std::size_t n) {
    if (failed()) return false;
    std::size_t i = 0;
    while (i < n) {
        switch (stage_) {
        case Stage::Headers: {
            head_.push_back(data[i++]);
            if (head_.size() > 8192) return fail("Header HTTP troppo lunghi");
            if (head_.size() >= 4 && head_.compare(head_.size() - 4, 4, "\r\n\r\n") == 0) {
                head_.resize(head_.size() - 4);
                if (!parseHead()) return false;
            }
            break;
        }
        case Stage::Body: {
            std::size_t take = n - i;
            if (hasLength_) take = std::min(take, remaining_);
            if (body_.size() + take > maxBody_) return fail("Risposta troppo grande");
            body_.append(data + i, take); i += take;
            if (hasLength_) { remaining_ -= take; if (remaining_ == 0) { stage_ = Stage::Done; complete_ = true; } }
            break;
        }
        case Stage::ChunkSize: case Stage::ChunkDataEnd: case Stage::Trailers: {
            line_.push_back(data[i++]);
            if (line_.size() > 1024) return fail("Chunk HTTP non valido");
            if (line_.size() < 2 || line_.compare(line_.size() - 2, 2, "\r\n") != 0) break;
            const std::string line = line_.substr(0, line_.size() - 2);
            line_.clear();
            if (stage_ == Stage::ChunkDataEnd) { if (!line.empty()) return fail("Chunk HTTP non terminato"); stage_ = Stage::ChunkSize; break; }
            if (stage_ == Stage::Trailers) { if (line.empty()) { stage_ = Stage::Done; complete_ = true; } break; }
            char* end = nullptr;
            const unsigned long size = std::strtoul(line.c_str(), &end, 16);
            if (end == line.c_str() || (*end != '\0' && *end != ';')) return fail("Dimensione chunk non valida");
            if (size == 0) { stage_ = Stage::Trailers; break; }
            if (body_.size() + size > maxBody_) return fail("Risposta troppo grande");
            remaining_ = size; stage_ = Stage::ChunkData;
            break;
        }
        case Stage::ChunkData: {
            const std::size_t take = std::min(n - i, remaining_);
            body_.append(data + i, take); i += take; remaining_ -= take;
            if (remaining_ == 0) stage_ = Stage::ChunkDataEnd;
            break;
        }
        case Stage::Done: return true;   // ignore anything after a complete response
        }
    }
    return true;
}

bool HttpResponseParser::finish() {
    if (failed()) return false;
    if (complete_) return true;
    if (stage_ == Stage::Body && !hasLength_) { complete_ = true; stage_ = Stage::Done; return true; }   // close-delimited
    fail(stage_ == Stage::Headers ? "Connessione chiusa prima della risposta" : "Risposta incompleta");
    return false;
}

std::string HttpResponseParser::header(const std::string& name) const {
    const std::string key = lower(name);
    for (const auto& h : headers_) if (h.first == key) return h.second;
    return {};
}

std::string sanitizeText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\t') { out += "  "; ++i; continue; }
        if (c == '\r') { ++i; continue; }
        if (c < 0x20 && c != '\n') { ++i; continue; }
        const std::size_t n = utf8Seq(s, i);
        if (n == 0) { out += '?'; ++i; continue; }
        out.append(s, i, n); i += n;
    }
    return out;
}
} // namespace fx::ai
