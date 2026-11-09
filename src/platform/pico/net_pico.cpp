// Wi-Fi (CYW43, poll mode) + HTTPS client (lwIP altcp_tls over mbedTLS) used by
// AI CHAT to reach the relay. Every step is asynchronous and advanced from
// poll(), so the UI loop never blocks. The relay certificate is verified
// against the CA bundle embedded at build time (certs/relay_ca.pem by default)
// and the hostname is checked through mbedtls_ssl_set_hostname().
// Requires validation on a physical Pico 2 W.
#include "ai_protocol.hpp"
#include "net.hpp"
#include "relay_ca.h"
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
using fx::net::ChatState;
constexpr std::uint32_t TotalTimeoutMs = 120000, WifiTimeoutMs = 30000;
constexpr std::size_t WriteChunk = 1024;     // < MBEDTLS_SSL_OUT_CONTENT_LEN: avoids lwIP's partial-write assert

std::uint32_t nowMs() { return to_ms_since_boot(get_absolute_time()); }

class PicoTransport final : public fx::net::ChatTransport {
public:
    bool configureWifi(const std::string& ssid, const std::string& password) override {
        if (ssid.empty() || !ensureInit()) return false;
        const int link = linkStatus();
        if (ssid == ssid_ && (link == CYW43_LINK_UP || link == CYW43_LINK_JOIN || link == CYW43_LINK_NOIP)) return true;
        ssid_ = ssid; password_ = password;
        const int r = cyw43_arch_wifi_connect_async(ssid_.c_str(), password_.empty() ? nullptr : password_.c_str(),
                                                    password_.empty() ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK);
        return r == 0;
    }
    bool wifiConnected() override { return inited_ && linkStatus() == CYW43_LINK_UP; }
    const char* wifiStatus() override {
        if (!inited_) return "spento";
        switch (linkStatus()) {
        case CYW43_LINK_UP: return "connesso";
        case CYW43_LINK_JOIN: return "connessione...";
        case CYW43_LINK_NOIP: return "attesa indirizzo IP...";
        case CYW43_LINK_FAIL: return "errore di connessione";
        case CYW43_LINK_NONET: return "rete non trovata";
        case CYW43_LINK_BADAUTH: return "password errata";
        default: return "disconnesso";
        }
    }
    bool begin(const fx::net::ChatRequest& request) override {
        if (step_ != Step::Idle && step_ != Step::Done && step_ != Step::Failed) return false;
        if (!ensureInit()) return false;
        closePcb();
        req_ = request;
        wire_ = fx::ai::buildHttpRequest(request.host, request.path, request.token, request.body);
        written_ = 0;
        parser_.reset();
        error_.clear();
        started_ = nowMs();
        step_ = Step::WaitWifi;
        return true;
    }
    ChatState poll() override {
        service();
        // Connections are closed here, in the main loop, never inside lwIP callbacks.
        if ((step_ == Step::Done || step_ == Step::Failed) && pcb_) closePcb();
        if (step_ != Step::Idle && step_ != Step::Done && step_ != Step::Failed && nowMs() - started_ > TotalTimeoutMs)
            fail("timeout della richiesta");
        switch (step_) {
        case Step::WaitWifi: {
            const int link = linkStatus();
            if (link == CYW43_LINK_UP) startConnection();
            else if (link == CYW43_LINK_BADAUTH) fail("password Wi-Fi errata");
            else if (link == CYW43_LINK_NONET) fail("rete Wi-Fi non trovata");
            else if (link == CYW43_LINK_FAIL) fail("connessione Wi-Fi fallita");
            else if (nowMs() - started_ > WifiTimeoutMs) fail("Wi-Fi non connesso");
            return step_ == Step::Failed ? ChatState::Failed : ChatState::Connecting;
        }
        case Step::Dns: case Step::Connecting: return ChatState::Connecting;
        case Step::Sending: writeMore(); return step_ == Step::Failed ? ChatState::Failed : ChatState::Sending;
        case Step::Receiving: return ChatState::Waiting;
        case Step::Done: return ChatState::Done;
        case Step::Failed: return ChatState::Failed;
        case Step::Idle: return ChatState::Idle;
        }
        return ChatState::Idle;
    }
    int httpStatus() const override { return parser_.status(); }
    const std::string& response() const override { return parser_.body(); }
    const std::string& error() const override { return error_; }
    void cancel() override { closePcb(); step_ = Step::Idle; }
    void service() { if (inited_) cyw43_arch_poll(); }

private:
    enum class Step { Idle, WaitWifi, Dns, Connecting, Sending, Receiving, Done, Failed };
    bool ensureInit() {
        if (inited_) return true;
        if (cyw43_arch_init() != 0) { error_ = "inizializzazione Wi-Fi fallita"; return false; }
        cyw43_arch_enable_sta_mode();
        inited_ = true;
        return true;
    }
    int linkStatus() const { return inited_ ? cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) : CYW43_LINK_DOWN; }
    // Safe inside lwIP callbacks: the pcb is closed later by poll().
    void fail(const std::string& why) { error_ = why; step_ = Step::Failed; }
    void closePcb() {
        if (!pcb_) return;
        altcp_arg(pcb_, nullptr); altcp_recv(pcb_, nullptr); altcp_sent(pcb_, nullptr); altcp_err(pcb_, nullptr);
        if (altcp_close(pcb_) != ERR_OK) altcp_abort(pcb_);
        pcb_ = nullptr;
    }
    void startConnection() {
        if (!tls_) {
            tls_ = altcp_tls_create_config_client(reinterpret_cast<const u8_t*>(kRelayCaPem), sizeof(kRelayCaPem));
            if (!tls_) { fail("configurazione TLS (CA) non valida"); return; }
        }
        pcb_ = altcp_tls_new(tls_, IPADDR_TYPE_ANY);
        if (!pcb_) { fail("memoria insufficiente per TLS"); return; }
        altcp_arg(pcb_, this);
        altcp_recv(pcb_, &PicoTransport::onRecv);
        altcp_sent(pcb_, &PicoTransport::onSent);
        altcp_err(pcb_, &PicoTransport::onError);
        // SNI + certificate hostname check.
        if (mbedtls_ssl_set_hostname(static_cast<mbedtls_ssl_context*>(altcp_tls_context(pcb_)), req_.host.c_str()) != 0) {
            fail("hostname del relay non valido"); return;
        }
        step_ = Step::Dns;
        ip_addr_t addr;
        const err_t e = dns_gethostbyname(req_.host.c_str(), &addr, &PicoTransport::onDns, this);
        if (e == ERR_OK) connectTo(&addr);
        else if (e != ERR_INPROGRESS) fail("risoluzione DNS fallita");
    }
    void connectTo(const ip_addr_t* addr) {
        step_ = Step::Connecting;
        if (altcp_connect(pcb_, addr, req_.port, &PicoTransport::onConnected) != ERR_OK) fail("connessione al relay fallita");
    }
    void writeMore() {
        if (!pcb_ || step_ != Step::Sending) return;
        while (written_ < wire_.size()) {
            const std::size_t room = altcp_sndbuf(pcb_);
            const std::size_t n = std::min({WriteChunk, wire_.size() - written_, room});
            if (n == 0) break;
            const err_t e = altcp_write(pcb_, wire_.data() + written_, static_cast<u16_t>(n), TCP_WRITE_FLAG_COPY);
            if (e == ERR_MEM) break;                       // retried from onSent()/poll()
            if (e != ERR_OK) { fail("errore di invio"); return; }
            written_ += n;
        }
        altcp_output(pcb_);
        if (written_ >= wire_.size()) step_ = Step::Receiving;
    }
    static void onDns(const char*, const ip_addr_t* addr, void* arg) {
        auto* self = static_cast<PicoTransport*>(arg);
        if (self->step_ != Step::Dns || !self->pcb_) return;
        if (!addr) { self->fail("host del relay non trovato (DNS)"); return; }
        self->connectTo(addr);
    }
    static err_t onConnected(void* arg, altcp_pcb*, err_t err) {
        auto* self = static_cast<PicoTransport*>(arg);
        if (!self) return ERR_OK;
        if (err != ERR_OK) { self->fail("handshake TLS fallito"); return ERR_OK; }
        self->step_ = Step::Sending;
        self->writeMore();
        return ERR_OK;
    }
    static err_t onSent(void* arg, altcp_pcb*, u16_t) {
        auto* self = static_cast<PicoTransport*>(arg);
        if (self) self->writeMore();
        return ERR_OK;
    }
    static err_t onRecv(void* arg, altcp_pcb* pcb, pbuf* p, err_t err) {
        auto* self = static_cast<PicoTransport*>(arg);
        if (!self) { if (p) pbuf_free(p); return ERR_OK; }
        if (!p || err != ERR_OK) {                       // remote closed: response complete or truncated
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
            if (!self->parser_.feed(buf, n)) break;
            off = static_cast<u16_t>(off + n);
        }
        altcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        if (self->parser_.failed()) self->fail(self->parser_.error());
        else if (self->parser_.complete()) self->step_ = Step::Done;
        return ERR_OK;
    }
    static void onError(void* arg, err_t err) {
        auto* self = static_cast<PicoTransport*>(arg);
        if (!self) return;
        self->pcb_ = nullptr;                            // lwIP already freed the pcb
        if (self->step_ == Step::Receiving && self->parser_.finish()) { self->step_ = Step::Done; return; }
        self->error_ = err == ERR_ABRT ? "connessione interrotta" : err == ERR_RST ? "connessione chiusa dal relay" :
                       self->step_ == Step::Connecting ? "certificato o handshake TLS non valido" : "errore di rete";
        self->step_ = Step::Failed;
    }

    bool inited_{false};
    std::string ssid_, password_;
    altcp_tls_config* tls_{nullptr};
    altcp_pcb* pcb_{nullptr};
    Step step_{Step::Idle};
    fx::net::ChatRequest req_;
    std::string wire_;
    std::size_t written_{0};
    fx::ai::HttpResponseParser parser_{16384};
    std::string error_;
    std::uint32_t started_{0};
};

PicoTransport transport;
} // namespace

namespace board {
fx::net::ChatTransport* picoChatTransport() { return &transport; }
void picoNetService() { transport.service(); }
} // namespace board
