// Download HTTPS (GET) del gestore delle librerie (v1.20.0, solo Pimoroni
// Pico Plus 2 W): lwIP altcp_tls su mbedTLS, come il trasporto della CHAT AI
// (net_pico.cpp, che resta com'era e accende il Wi-Fi). Il certificato del
// server si verifica con certs/library_ca.pem (FXAI_LIBRARY_CA_FILE) e il nome
// dell'host con SNI e mbedtls_ssl_set_hostname(). Ogni passo e' asincrono e
// avanza da poll(): l'interfaccia non si blocca e AC annulla. I byte del
// corpo arrivano a pezzi e vanno subito sulla SD (lib_manager.cpp).
// Da provare sul dispositivo fisico (docs/reports/HARDWARE_TEST_v1.20.0.md).
#include "lib_fetch.hpp"
#include "library_ca.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "mbedtls/ssl.h"
#include <algorithm>
#include <string>

namespace {
using fx::net::Fetcher;
constexpr std::uint32_t TotalTimeoutMs = 180000, IdleTimeoutMs = 30000, WifiTimeoutMs = 30000;
constexpr std::size_t MaxPending = 16384;            // byte ricevuti in attesa di poll()

std::uint32_t nowMs() { return to_ms_since_boot(get_absolute_time()); }
int linkStatus() { return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); }

class PicoFetcher final : public Fetcher {
public:
    bool begin(const std::string& url) override {
        if (step_ != Step::Idle && step_ != Step::Done && step_ != Step::Failed) { error_ = "download gia' in corso"; return false; }
        closePcb();
        error_.clear();
        if (!fx::net::parseHttpsUrl(url, url_, error_)) return false;
        wire_ = fx::net::buildGet(url_);
        written_ = 0;
        parser_ = fx::net::HttpBodyParser(fx::libs::MaxFileBytes + 4096);
        pending_.clear();
        deferred_ = 0;
        started_ = lastData_ = nowMs();
        step_ = Step::WaitWifi;
        return true;
    }
    State poll(std::string& chunk) override {
        if ((step_ == Step::Done || step_ == Step::Failed) && pcb_) closePcb();
        if (!pending_.empty()) { chunk += pending_; pending_.clear(); lastData_ = nowMs(); }
        // finestra TCP riaperta solo quando i byte sono stati consegnati (la SD tiene il passo)
        while (pcb_ && deferred_ > 0) {
            const u16_t n = static_cast<u16_t>(std::min<std::size_t>(deferred_, 0xFFFF));
            altcp_recved(pcb_, n);
            deferred_ -= n;
        }
        const std::uint32_t now = nowMs();
        if (step_ != Step::Idle && step_ != Step::Done && step_ != Step::Failed) {
            if (now - started_ > TotalTimeoutMs) fail("timeout del download");
            else if (step_ == Step::Receiving && now - lastData_ > IdleTimeoutMs) fail("il server non risponde (download interrotto)");
        }
        switch (step_) {
        case Step::WaitWifi: {
            const int link = linkStatus();
            if (link == CYW43_LINK_UP) startConnection();
            else if (link == CYW43_LINK_BADAUTH) fail("password Wi-Fi errata");
            else if (link == CYW43_LINK_NONET) fail("rete Wi-Fi non trovata");
            else if (link == CYW43_LINK_FAIL) fail("connessione Wi-Fi fallita");
            else if (now - started_ > WifiTimeoutMs) fail("Wi-Fi non connesso (SYSTEM > Wi-Fi)");
            return step_ == Step::Failed ? State::Failed : State::Busy;
        }
        case Step::Sending: writeMore(); return step_ == Step::Failed ? State::Failed : State::Busy;
        case Step::Done: return State::Done;
        case Step::Failed: return State::Failed;
        case Step::Idle: return State::Idle;
        default: return State::Busy;
        }
    }
    int httpStatus() const override { return parser_.status(); }
    const std::string& error() const override { return error_; }
    void cancel() override { closePcb(); pending_.clear(); deferred_ = 0; step_ = Step::Idle; }

private:
    enum class Step { Idle, WaitWifi, Dns, Connecting, Sending, Receiving, Done, Failed };
    void fail(const std::string& why) { if (step_ != Step::Failed) error_ = why; step_ = Step::Failed; }
    void closePcb() {
        deferred_ = 0;
        if (!pcb_) return;
        altcp_arg(pcb_, nullptr); altcp_recv(pcb_, nullptr); altcp_sent(pcb_, nullptr); altcp_err(pcb_, nullptr);
        if (altcp_close(pcb_) != ERR_OK) altcp_abort(pcb_);
        pcb_ = nullptr;
    }
    void startConnection() {
        if (!tls_) {
            tls_ = altcp_tls_create_config_client(reinterpret_cast<const u8_t*>(kLibraryCaPem), sizeof(kLibraryCaPem));
            if (!tls_) { fail("configurazione TLS (certificati) non valida"); return; }
        }
        pcb_ = altcp_tls_new(tls_, IPADDR_TYPE_ANY);
        if (!pcb_) { fail("memoria insufficiente per TLS"); return; }
        altcp_arg(pcb_, this);
        altcp_recv(pcb_, &PicoFetcher::onRecv);
        altcp_sent(pcb_, &PicoFetcher::onSent);
        altcp_err(pcb_, &PicoFetcher::onError);
        if (mbedtls_ssl_set_hostname(static_cast<mbedtls_ssl_context*>(altcp_tls_context(pcb_)), url_.host.c_str()) != 0) { fail("nome del server non valido"); return; }
        step_ = Step::Dns;
        ip_addr_t addr;
        const err_t e = dns_gethostbyname(url_.host.c_str(), &addr, &PicoFetcher::onDns, this);
        if (e == ERR_OK) connectTo(&addr);
        else if (e != ERR_INPROGRESS) fail("risoluzione DNS fallita");
    }
    void connectTo(const ip_addr_t* addr) {
        step_ = Step::Connecting;
        if (altcp_connect(pcb_, addr, url_.port, &PicoFetcher::onConnected) != ERR_OK) fail("connessione al server fallita");
    }
    void writeMore() {
        if (!pcb_ || step_ != Step::Sending) return;
        while (written_ < wire_.size()) {
            const std::size_t room = altcp_sndbuf(pcb_);
            const std::size_t n = std::min({std::size_t(1024), wire_.size() - written_, room});
            if (n == 0) break;
            const err_t e = altcp_write(pcb_, wire_.data() + written_, static_cast<u16_t>(n), TCP_WRITE_FLAG_COPY);
            if (e == ERR_MEM) break;
            if (e != ERR_OK) { fail("errore di invio"); return; }
            written_ += n;
        }
        altcp_output(pcb_);
        if (written_ >= wire_.size()) step_ = Step::Receiving;
    }
    static void onDns(const char*, const ip_addr_t* addr, void* arg) {
        auto* self = static_cast<PicoFetcher*>(arg);
        if (self->step_ != Step::Dns || !self->pcb_) return;
        if (!addr) { self->fail("server non trovato (DNS)"); return; }
        self->connectTo(addr);
    }
    static err_t onConnected(void* arg, altcp_pcb*, err_t err) {
        auto* self = static_cast<PicoFetcher*>(arg);
        if (!self) return ERR_OK;
        if (err != ERR_OK) { self->fail("handshake TLS fallito"); return ERR_OK; }
        self->step_ = Step::Sending;
        self->writeMore();
        return ERR_OK;
    }
    static err_t onSent(void* arg, altcp_pcb*, u16_t) {
        auto* self = static_cast<PicoFetcher*>(arg);
        if (self) self->writeMore();
        return ERR_OK;
    }
    static err_t onRecv(void* arg, altcp_pcb* pcb, pbuf* p, err_t err) {
        auto* self = static_cast<PicoFetcher*>(arg);
        if (!self) { if (p) pbuf_free(p); return ERR_OK; }
        if (!p || err != ERR_OK) {
            if (p) pbuf_free(p);
            if (self->step_ == Step::Done || self->step_ == Step::Failed) return ERR_OK;
            if (self->parser_.finish()) self->step_ = Step::Done;
            else self->fail(self->parser_.error());
            return ERR_OK;
        }
        char buf[512];
        for (u16_t off = 0; off < p->tot_len;) {
            const u16_t n = pbuf_copy_partial(p, buf, static_cast<u16_t>(std::min<std::size_t>(sizeof(buf), p->tot_len - off)), off);
            if (n == 0) break;
            if (!self->parser_.feed(buf, n, self->pending_)) break;
            off = static_cast<u16_t>(off + n);
        }
        // la finestra TCP si riapre subito o quando poll() consegna i byte (controllo di flusso verso la SD)
        if (self->pending_.size() < MaxPending) altcp_recved(pcb, p->tot_len);
        else self->deferred_ += p->tot_len;
        pbuf_free(p);
        if (self->parser_.failed()) self->fail(self->parser_.error());
        else if (self->parser_.complete()) self->step_ = Step::Done;
        return ERR_OK;
    }
    static void onError(void* arg, err_t err) {
        auto* self = static_cast<PicoFetcher*>(arg);
        if (!self) return;
        self->pcb_ = nullptr;
        if (self->step_ == Step::Receiving && self->parser_.finish()) { self->step_ = Step::Done; return; }
        self->error_ = err == ERR_ABRT ? "connessione interrotta" : err == ERR_RST ? "connessione chiusa dal server" :
                       self->step_ == Step::Connecting ? "certificato o handshake TLS non valido" : "errore di rete";
        self->step_ = Step::Failed;
    }

    altcp_tls_config* tls_{nullptr};
    altcp_pcb* pcb_{nullptr};
    Step step_{Step::Idle};
    fx::net::Url url_;
    std::string wire_;
    std::size_t written_{0};
    fx::net::HttpBodyParser parser_{fx::libs::MaxFileBytes + 4096};
    std::string pending_;
    std::size_t deferred_{0};
    std::string error_;
    std::uint32_t started_{0}, lastData_{0};
};

PicoFetcher fetcher;
} // namespace

namespace board {
fx::net::Fetcher* fetcher() { return &::fetcher; }
} // namespace board
