// v1.17.0: il lato "dispositivo" del protocollo di Link sullo stdin/stdout,
// con lo stesso codice del firmware (src/link_protocol.cpp). Lo usa
// tools/test_picocalc_link.py per le prove di andata e ritorno sul PC:
//   link_peer receive FILE     riceve un pacchetto e lo scrive in FILE
//   link_peer send FILE NOME   invia il pacchetto di FILE
// Esce con 0 se il trasferimento e' completo e il pacchetto e' valido.
#include "link_protocol.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
class StdioPort final : public fx::link::Port {
public:
    std::size_t read(std::uint8_t* buf, std::size_t max) override {
        pollfd p{0, POLLIN, 0};
        if (poll(&p, 1, 1) <= 0 || !(p.revents & (POLLIN | POLLHUP))) return 0;
        const ssize_t n = ::read(0, buf, max);
        return n > 0 ? static_cast<std::size_t>(n) : 0;
    }
    void write(const std::uint8_t* data, std::size_t size) override {
        while (size) {
            const ssize_t n = ::write(1, data, size);
            if (n <= 0) return;
            data += n;
            size -= static_cast<std::size_t>(n);
        }
    }
};
std::uint32_t nowMs() {
    using namespace std::chrono;
    return static_cast<std::uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "uso: link_peer receive FILE | send FILE NOME\n"); return 2; }
    StdioPort port;
    const std::string mode = argv[1];
    if (mode == "receive") {
        fx::link::Receiver rx(port, nowMs);
        while (rx.status() == fx::link::Status::Running) { fx::step::Budget b; (void)rx.step(b); }
        if (rx.status() != fx::link::Status::Done) { std::fprintf(stderr, "%s\n", rx.error().c_str()); return 1; }
        std::vector<fx::link::Entry> entries;
        if (!fx::link::decodePackage(rx.package().data(), rx.package().size(), entries)) { std::fprintf(stderr, "pacchetto non valido\n"); return 1; }
        std::ofstream(argv[2], std::ios::binary).write(reinterpret_cast<const char*>(rx.package().data()), static_cast<std::streamsize>(rx.package().size()));
        std::fprintf(stderr, "%s: %zu voci\n", rx.name().c_str(), entries.size());
        return 0;
    }
    if (mode == "send" && argc >= 4) {
        std::ifstream in(argv[2], std::ios::binary);
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        // un po' di rumore prima, come le righe di log del firmware sulla stessa porta
        const char log[] = "[DIAG] OK   link peer\n";
        port.write(reinterpret_cast<const std::uint8_t*>(log), sizeof log - 1);
        fx::link::Sender tx(port, nowMs, std::move(data), argv[3]);
        while (tx.status() == fx::link::Status::Running) { fx::step::Budget b; (void)tx.step(b); }
        if (tx.status() != fx::link::Status::Done) { std::fprintf(stderr, "%s\n", tx.error().c_str()); return 1; }
        return 0;
    }
    return 2;
}
