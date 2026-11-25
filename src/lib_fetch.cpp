// Parti portabili del download HTTPS delle librerie: vedi lib_fetch.hpp.
#include "lib_fetch.hpp"
#include <cctype>
#include <cstdlib>

namespace fx::net {

bool parseHttpsUrl(const std::string& url, Url& out, std::string& error) {
    out = Url();
    const std::string pre = "https://";
    if (url.compare(0, pre.size(), pre) != 0) { error = "l'indirizzo deve iniziare con https://"; return false; }
    if (url.size() > 300) { error = "indirizzo troppo lungo"; return false; }
    const std::size_t slash = url.find('/', pre.size());
    std::string hostPort = url.substr(pre.size(), slash == std::string::npos ? std::string::npos : slash - pre.size());
    out.path = slash == std::string::npos ? "/" : url.substr(slash);
    const std::size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        const std::string p = hostPort.substr(colon + 1);
        if (p.empty() || p.size() > 5) { error = "porta non valida"; return false; }
        for (char c : p) if (!std::isdigit(static_cast<unsigned char>(c))) { error = "porta non valida"; return false; }
        const long port = std::strtol(p.c_str(), nullptr, 10);
        if (port <= 0 || port > 65535) { error = "porta non valida"; return false; }
        out.port = static_cast<std::uint16_t>(port);
        hostPort = hostPort.substr(0, colon);
    }
    if (hostPort.empty() || hostPort.size() > 253) { error = "host non valido"; return false; }
    for (char c : hostPort) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-')) { error = "host non valido"; return false; }
    for (char c : out.path) if (static_cast<unsigned char>(c) <= 32 || static_cast<unsigned char>(c) >= 127) { error = "percorso non valido nell'indirizzo"; return false; }
    out.host = hostPort;
    return true;
}
std::string urlFolder(const std::string& url) {
    const std::size_t q = url.find_first_of("?#");
    const std::string u = url.substr(0, q);
    const std::size_t slash = u.rfind('/');
    if (slash == std::string::npos || slash < 8) return u;
    return u.substr(0, slash);
}
std::string buildGet(const Url& url) {
    std::string r = "GET " + url.path + " HTTP/1.1\r\nHost: " + url.host;
    if (url.port != 443) r += ":" + std::to_string(url.port);
    r += "\r\nUser-Agent: PicoCalc-LIB/1\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    return r;
}

bool HttpBodyParser::feed(const char* data, std::size_t size, std::string& body) {
    if (failed_) return false;
    std::size_t i = 0;
    if (!inBody_) {
        for (; i < size; ++i) {
            head_ += data[i];
            if (head_.size() > 16384) return fail("intestazioni HTTP troppo lunghe");
            if (head_.size() >= 4 && head_.compare(head_.size() - 4, 4, "\r\n\r\n") == 0) { ++i; break; }
        }
        if (head_.size() < 4 || head_.compare(head_.size() - 4, 4, "\r\n\r\n") != 0) return true;
        // riga di stato
        if (head_.compare(0, 5, "HTTP/") != 0) return fail("risposta non HTTP");
        const std::size_t sp = head_.find(' ');
        if (sp == std::string::npos) return fail("riga di stato non valida");
        status_ = std::atoi(head_.c_str() + sp + 1);
        if (status_ != 200) return fail("il server ha risposto " + std::to_string(status_) + (status_ == 404 ? " (file non trovato)" : ""));
        // intestazioni
        std::size_t p = head_.find("\r\n") + 2;
        while (p < head_.size()) {
            const std::size_t e = head_.find("\r\n", p);
            if (e == std::string::npos || e == p) break;
            std::string line = head_.substr(p, e - p);
            p = e + 2;
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string k = line.substr(0, colon), v = line.substr(colon + 1);
            for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            while (!v.empty() && v[0] == ' ') v.erase(0, 1);
            if (k == "content-length") {
                for (char c : v) if (!std::isdigit(static_cast<unsigned char>(c))) return fail("Content-Length non valido");
                contentLength_ = std::strtoll(v.c_str(), nullptr, 10);
                if (contentLength_ < 0 || static_cast<std::uint64_t>(contentLength_) > max_) return fail("file troppo grande");
            } else if (k == "transfer-encoding") {
                for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (v.find("chunked") != std::string::npos) chunked_ = true;
            }
        }
        inBody_ = true;
        if (!chunked_ && contentLength_ == 0) { complete_ = true; return true; }
    }
    for (; i < size && !complete_; ++i) {
        const char c = data[i];
        if (!chunked_) {
            const std::size_t n = contentLength_ >= 0 ? static_cast<std::size_t>(std::min<std::uint64_t>(size - i, static_cast<std::uint64_t>(contentLength_) - received_)) : size - i;
            if (received_ + n > max_) return fail("file troppo grande");   // mai piu' byte del limite nel corpo
            body.append(data + i, n);
            received_ += n;
            i += n - 1;
            if (contentLength_ >= 0 && received_ >= static_cast<std::uint64_t>(contentLength_)) complete_ = true;
            continue;
        }
        switch (chunkState_) {
        case 0:
            if (c == '\n') {
                while (!line_.empty() && (line_.back() == '\r' || line_.back() == ' ')) line_.pop_back();
                const std::size_t semi = line_.find(';');
                const std::string hex = line_.substr(0, semi);
                if (hex.empty() || hex.size() > 8) return fail("blocco chunked non valido");
                for (char h : hex) if (!std::isxdigit(static_cast<unsigned char>(h))) return fail("blocco chunked non valido");
                chunkLeft_ = std::strtoull(hex.c_str(), nullptr, 16);
                line_.clear();
                chunkState_ = chunkLeft_ == 0 ? 3 : 1;
            } else {
                line_ += c;
                if (line_.size() > 64) return fail("blocco chunked non valido");
            }
            break;
        case 1: {
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(size - i, chunkLeft_));
            if (received_ + n > max_) return fail("file troppo grande");
            body.append(data + i, n);
            received_ += n;
            chunkLeft_ -= n;
            i += n - 1;
            if (chunkLeft_ == 0) chunkState_ = 2;
            break;
        }
        case 2:
            if (c == '\n') chunkState_ = 0;
            else if (c != '\r') return fail("blocco chunked non valido");
            break;
        default:
            // trailer: fino alla riga vuota
            if (c == '\n') {
                while (!line_.empty() && line_.back() == '\r') line_.pop_back();
                if (line_.empty()) complete_ = true;
                line_.clear();
            } else line_ += c;
            break;
        }
    }
    return true;
}
bool HttpBodyParser::finish() {
    if (failed_) return false;
    if (complete_) return true;
    if (!inBody_) return fail("connessione chiusa prima della risposta");
    if (chunked_) return fail("download interrotto (connessione chiusa a meta')");
    if (contentLength_ >= 0) return fail("download interrotto (" + std::to_string(received_) + " byte su " + std::to_string(contentLength_) + ")");
    complete_ = true;                                    // senza lunghezza: fino alla chiusura
    return true;
}

bool HttpsSource::begin(const std::string& relPath, std::string& error) {
    if (!libs::safeRelativePath(relPath)) { error = "percorso non ammesso: " + relPath; return false; }
    if (!f_.begin(base_ + "/" + relPath)) { error = f_.error().empty() ? "download non avviato" : f_.error(); return false; }
    return true;
}
libs::Source::Poll HttpsSource::poll(std::string& chunk, std::string& error) {
    const std::size_t before = chunk.size();
    const Fetcher::State s = f_.poll(chunk);
    if (s == Fetcher::State::Failed) { error = f_.error().empty() ? "download non riuscito" : f_.error(); return Poll::Failed; }
    if (s == Fetcher::State::Done) return Poll::Done;
    return chunk.size() > before ? Poll::Data : Poll::Busy;
}

} // namespace fx::net
