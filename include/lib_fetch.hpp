#pragma once
// Download HTTPS (GET) per il gestore delle librerie (v1.20.0, solo
// Pimoroni Pico Plus 2 W). Asincrono come il trasporto della CHAT AI: begin()
// avvia la richiesta, poll() la fa avanzare e restituisce i byte del corpo a
// pezzi (il chiamante li scrive subito sulla SD: niente file interi in RAM).
// Il certificato del server si verifica con certs/library_ca.pem e il nome
// dell'host con SNI; AC = cancel().
//  * dispositivo: src/platform/pico/fetch_pico.cpp (lwIP altcp_tls + mbedTLS);
//  * PC: tests/https_fetch_host.cpp (OpenSSL) per i test con il server HTTPS locale.
#include "lib_manager.hpp"
#include <cstdint>
#include <string>

namespace fx::net {

class Fetcher {
public:
    enum class State : std::uint8_t { Idle, Busy, Done, Failed };
    virtual ~Fetcher() = default;
    // URL "https://host[:porta]/percorso"; false se non valido o occupato.
    virtual bool begin(const std::string& url) = 0;
    // Avanza; i nuovi byte del corpo vanno in `chunk` (aggiunti).
    virtual State poll(std::string& chunk) = 0;
    virtual int httpStatus() const = 0;
    virtual const std::string& error() const = 0;
    virtual void cancel() = 0;
};

// Parti di un URL https.
struct Url {
    std::string host, path;
    std::uint16_t port{443};
};
bool parseHttpsUrl(const std::string& url, Url& out, std::string& error);
// Cartella di un URL ("https://h/a/b/index.json" -> "https://h/a/b").
std::string urlFolder(const std::string& url);
// Risposta HTTP/1.1 a pezzi: intestazioni, corpo con Content-Length, chunked
// o fino alla chiusura; codice 200 obbligatorio, limite di byte del corpo.
class HttpBodyParser {
public:
    explicit HttpBodyParser(std::uint64_t maxBody) : max_(maxBody) {}
    // Aggiunge byte della connessione; i byte del corpo vanno in `body`.
    bool feed(const char* data, std::size_t size, std::string& body);
    bool finish();                                      // connessione chiusa: corpo completo?
    bool complete() const noexcept { return complete_; }
    bool failed() const noexcept { return failed_; }
    int status() const noexcept { return status_; }
    const std::string& error() const noexcept { return error_; }
private:
    bool fail(const std::string& why) { failed_ = true; error_ = why; return false; }
    std::uint64_t max_;
    std::string head_;
    bool inBody_{false}, chunked_{false}, complete_{false}, failed_{false};
    std::int64_t contentLength_{-1};
    std::uint64_t received_{0};
    int status_{0};
    std::string error_;
    // chunked
    std::string line_;
    std::uint64_t chunkLeft_{0};
    int chunkState_{0};                                 // 0 dimensione, 1 dati, 2 CRLF dopo i dati, 3 trailer
};
// Richiesta GET minima.
std::string buildGet(const Url& url);

// Sorgente dei file di un repository via HTTPS (libs::Source sopra un Fetcher):
// il file relativo `p` e' scaricato da `base/p`.
class HttpsSource final : public libs::Source {
public:
    HttpsSource(Fetcher& f, std::string baseUrl) : f_(f), base_(std::move(baseUrl)) {}
    bool begin(const std::string& relPath, std::string& error) override;
    Poll poll(std::string& chunk, std::string& error) override;
    void cancel() override { f_.cancel(); }
private:
    Fetcher& f_;
    std::string base_;
};

} // namespace fx::net
