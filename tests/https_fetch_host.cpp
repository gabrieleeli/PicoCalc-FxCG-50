// v1.20.0: download HTTPS delle librerie, prova di andata e ritorno con un
// server HTTPS locale (tests/https_lib_server.py, certificati fatti al volo
// da tests/https_lib_test.sh). Sul PC il trasporto e' OpenSSL al posto di
// lwIP + mbedTLS del dispositivo (src/platform/pico/fetch_pico.cpp); tutto il
// resto e' il codice del firmware: URL, richiesta GET, parser HTTP
// (Content-Length, chunked, chiusura a meta'), HttpsSource, piano e
// installatore atomico. Verifica del certificato con la CA indicata, SNI e
// controllo del nome dell'host come sul dispositivo.
//
//   https_fetch_host --make-repo <cartella>
//   https_fetch_host --run <porta> <ca.pem> <altra-ca.pem> <cartella-repository>
#include "file_store.hpp"
#include "lib_fetch.hpp"
#include "lib_manager.hpp"
#include "sha256.hpp"
#include "test_util.hpp"
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

using namespace fx;
using namespace fx::libs;
using fx::files::RamFileSystem;

namespace {
// ---------------------------------------------------------------- Fetcher con OpenSSL (un thread per richiesta)
class SslFetcher final : public net::Fetcher {
public:
    explicit SslFetcher(std::string caFile) : ca_(std::move(caFile)) {}
    ~SslFetcher() override { cancel(); }
    bool begin(const std::string& url) override {
        if (thread_.joinable()) {
            if (state_ == State::Busy) { error_ = "download gia' in corso"; return false; }
            thread_.join();
        }
        net::Url u;
        std::string err;
        if (!net::parseHttpsUrl(url, u, err)) { error_ = err; return false; }
        std::lock_guard<std::mutex> g(m_);
        pending_.clear();
        error_.clear();
        status_ = 0;
        stop_ = false;
        state_ = State::Busy;
        thread_ = std::thread([this, u] { run(u); });
        return true;
    }
    State poll(std::string& chunk) override {
        std::lock_guard<std::mutex> g(m_);
        chunk += pending_;
        pending_.clear();
        return state_;
    }
    int httpStatus() const override { return status_; }
    const std::string& error() const override { return error_; }
    void cancel() override {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
        std::lock_guard<std::mutex> g(m_);
        pending_.clear();
        state_ = State::Idle;
    }
private:
    void fail(const std::string& why) {
        std::lock_guard<std::mutex> g(m_);
        if (state_ == State::Busy) { error_ = why; state_ = State::Failed; }
    }
    void run(net::Url u) {
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { fail("configurazione TLS non valida"); return; }
        struct Guard { SSL_CTX* c; BIO* b{nullptr}; ~Guard() { if (b) BIO_free_all(b); SSL_CTX_free(c); } } guard{ctx};
        if (SSL_CTX_load_verify_locations(ctx, ca_.c_str(), nullptr) != 1) { fail("configurazione TLS (certificati) non valida"); return; }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        BIO* b = BIO_new_ssl_connect(ctx);
        guard.b = b;
        SSL* ssl = nullptr;
        BIO_get_ssl(b, &ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
        SSL_set_tlsext_host_name(ssl, u.host.c_str());          // SNI
        SSL_set1_host(ssl, u.host.c_str());                     // nome dell'host nel certificato
        const std::string target = u.host + ":" + std::to_string(u.port);
        BIO_set_conn_hostname(b, target.c_str());
        if (BIO_do_connect(b) <= 0) { fail(SSL_get_verify_result(ssl) != X509_V_OK ? "certificato o handshake TLS non valido" : "connessione al server fallita"); return; }
        if (BIO_do_handshake(b) <= 0 || SSL_get_verify_result(ssl) != X509_V_OK) { fail("certificato o handshake TLS non valido"); return; }
        int fd = -1;
        BIO_get_fd(b, &fd);
        if (fd >= 0) {
            timeval tv{};
            tv.tv_sec = 0;
            tv.tv_usec = 200000;                             // per vedere stop_ (AC) anche durante una lettura
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        }
        const std::string req = net::buildGet(u);
        if (BIO_write(b, req.data(), static_cast<int>(req.size())) != static_cast<int>(req.size())) { fail("errore di invio"); return; }
        (void)BIO_flush(b);
        net::HttpBodyParser parser(libs::MaxFileBytes + 4096);
        const auto started = std::chrono::steady_clock::now();
        char buf[1500];
        for (;;) {
            if (stop_) return;
            if (std::chrono::steady_clock::now() - started > std::chrono::seconds(30)) { fail("timeout del download"); return; }
            const int n = BIO_read(b, buf, sizeof buf);
            if (n > 0) {
                std::string body;
                const bool ok = parser.feed(buf, static_cast<std::size_t>(n), body);
                status_ = parser.status();
                {
                    std::lock_guard<std::mutex> g(m_);
                    pending_ += body;
                }
                if (!ok) { fail(parser.error()); return; }
                if (parser.complete()) break;
                continue;
            }
            if (BIO_should_retry(b)) continue;                 // timeout della lettura: si riprova
            // connessione chiusa (anche senza close_notify, come lwIP)
            if (!parser.finish()) { fail(parser.error()); return; }
            break;
        }
        std::lock_guard<std::mutex> g(m_);
        if (state_ == State::Busy) state_ = State::Done;
    }
    std::string ca_;
    std::thread thread_;
    std::mutex m_;
    std::string pending_, error_;
    std::atomic<State> state_{State::Idle};
    std::atomic<int> status_{0};
    std::atomic<bool> stop_{false};
};

// ---------------------------------------------------------------- repository sul disco
void writeFile(const std::filesystem::path& p, const std::string& text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f << text;
}
struct Lib {
    std::string name, version, language;
    std::map<std::string, std::string> files;
    std::vector<Dependency> deps;
    std::map<std::string, std::string> served;         // file serviti diversi da quelli del manifest
};
int makeRepo(const std::filesystem::path& dir) {
    std::string big;
    for (int i = 0; i < 4500; ++i) big += "/* riga " + std::to_string(i) + " della libreria grande */\n";   // ~200 KB (limite 256 KB per file)
    const std::vector<Lib> libs = {
        {"base", "1.0.0", "c", {{"base.h", "#pragma once\n#define BASE 1\n"}}, {}, {}},
        {"matrici", "1.0.0", "c", {{"matrici.h", "#pragma once\nint traccia(int a, int d);\n"}, {"matrici.c", "#include \"matrici.h\"\nint traccia(int a, int d) { return a + d; }\n"}}, {{"base", "^1.0.0"}}, {}},
        {"geo", "1.0.0", "cpp", {{"geo.hpp", "#pragma once\n#include <matrici.h>\n"}}, {{"matrici", ">=1.0.0"}}, {}},
        {"grande", "1.0.0", "c", {{"grande.h", big}}, {}, {}},
        {"alterata", "1.0.0", "c", {{"a.h", "#define A 1\n"}}, {}, {{"a.h", "#define A 7\n"}}},   // stessa lunghezza, contenuto diverso
        {"anello-a", "1.0.0", "c", {{"a.h", "x"}}, {{"anello-b", "*"}}, {}},
        {"anello-b", "1.0.0", "c", {{"b.h", "x"}}, {{"anello-a", "*"}}, {}},
        {"orfana", "1.0.0", "c", {{"o.h", "x"}}, {{"inesistente", "^1.0.0"}}, {}},
    };
    std::string idx = "{\"version\": 1, \"libraries\": [\n";
    for (std::size_t k = 0; k < libs.size(); ++k) {
        const Lib& l = libs[k];
        Manifest m;
        m.name = l.name;
        (void)Version::parse(l.version, m.version);
        m.language = l.language;
        m.description = "libreria di prova " + l.name;
        m.license = "MIT";
        m.dependencies = l.deps;
        const std::string rel = l.language + "/" + l.name + "/" + l.version;
        for (const auto& [p, t] : l.files) {
            m.files.push_back({p, crypto::sha256Hex(t), static_cast<std::uint32_t>(t.size())});
            const auto s = l.served.find(p);
            writeFile(dir / rel / p, s == l.served.end() ? t : s->second);
        }
        const std::string mt = manifestJson(m);
        writeFile(dir / rel / ManifestName, mt);
        idx += std::string(k ? ",\n" : "") + "{\"name\": \"" + l.name + "\", \"version\": \"" + l.version + "\", \"language\": \"" + l.language + "\", \"path\": \"" + rel +
               "\", \"description\": \"" + m.description + "\", \"manifestSha256\": \"" + crypto::sha256Hex(mt) + "\", \"manifestSize\": " + std::to_string(mt.size()) + ", \"dependencies\": {";
        for (std::size_t i = 0; i < l.deps.size(); ++i) idx += std::string(i ? ", " : "") + "\"" + l.deps[i].name + "\": \"" + l.deps[i].range + "\"";
        idx += "}}";
    }
    writeFile(dir / "index.json", idx + "\n]}\n");
    return 0;
}

// ---------------------------------------------------------------- prove
bool fetchAll(net::Fetcher& f, const std::string& url, std::string& body, std::string& error) {
    body.clear();
    if (!f.begin(url)) { error = f.error(); return false; }
    for (int i = 0; i < 6000; ++i) {
        const net::Fetcher::State s = f.poll(body);
        if (s == net::Fetcher::State::Done) return true;
        if (s == net::Fetcher::State::Failed) { error = f.error(); return false; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    error = "attesa troppo lunga";
    return false;
}
Version fw() { Version v; (void)Version::parse("1.20.0", v); return v; }
bool install(RamFileSystem& sd, net::Fetcher& f, const std::string& base, const std::vector<IndexEntry>& index, const std::string& name, const std::string& lang, std::string& error) {
    std::vector<PlanStep> plan;
    if (!makePlan(index, listInstalled(sd, ""), name, lang, fw(), plan, error)) return false;
    net::HttpsSource src(f, base);
    Installer in(sd, src);
    in.start(plan, fw());
    for (int i = 0; i < 200000 && in.tick(4096); ++i) std::this_thread::sleep_for(std::chrono::microseconds(200));
    error = in.error();
    return in.state() == Installer::State::Done;
}
bool tmpClean(RamFileSystem& sd) { std::vector<files::Entry> e; return sd.list(TmpRoot, e) != files::Status::Ok || e.empty(); }
bool has(RamFileSystem& sd, const std::string& lang, const std::string& name) {
    for (const Installed& l : listInstalled(sd, lang)) if (l.manifest.name == name && l.manifestOk && verify(sd, l).ok) return true;
    return false;
}

int runTests(const std::string& port, const std::string& ca, const std::string& otherCa, const std::filesystem::path& repo) {
    const std::string host = "https://localhost:" + port;
    SslFetcher f(ca);
    std::string body, err;
    // indice dal server (Content-Length) e a pezzi (chunked): lo stesso testo del repository
    std::ifstream in(repo / "index.json", std::ios::binary);
    const std::string onDisk((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(fetchAll(f, host + "/ok/index.json", body, err) && body == onDisk && f.httpStatus() == 200);
    CHECK(fetchAll(f, host + "/chunked/index.json", body, err) && body == onDisk);
    std::vector<IndexEntry> index;
    CHECK(parseIndex(body, index, err) && index.size() == 8);
    CHECK_EQ_STR(net::urlFolder(host + "/ok/index.json"), host + "/ok");
    // 404
    CHECK(!fetchAll(f, host + "/ok/manca.json", body, err) && err.find("404") != std::string::npos && f.httpStatus() == 404);
    // certificato di un'altra CA: rifiutato
    SslFetcher wrong(otherCa);
    CHECK(!fetchAll(wrong, host + "/ok/index.json", body, err) && err.find("certificato") != std::string::npos);
    // nome dell'host diverso da quello del certificato (127.0.0.1 invece di localhost): rifiutato
    CHECK(!fetchAll(f, "https://127.0.0.1:" + port + "/ok/index.json", body, err) && err.find("certificato") != std::string::npos);
    // installazione con le dipendenze (geo -> matrici -> base), file verificati
    RamFileSystem sd;
    CHECK(install(sd, f, host + "/ok", index, "geo", "cpp", err));
    CHECK(has(sd, "cpp", "geo") && has(sd, "c", "matrici") && has(sd, "c", "base") && tmpClean(sd));
    // file grande a pezzi (chunked) e lenti
    CHECK(install(sd, f, host + "/chunked", index, "grande", "c", err) && has(sd, "c", "grande"));
    if (!err.empty()) std::fprintf(stderr, "  grande: %s\n", err.c_str());
    // impronta errata: il file servito non e' quello del manifest
    CHECK(!install(sd, f, host + "/ok", index, "alterata", "c", err) && err.find("SHA-256 errato") != std::string::npos);
    CHECK(!has(sd, "c", "alterata") && tmpClean(sd));
    // download interrotto a meta': niente libreria a meta', LIB come prima
    RamFileSystem sd2;
    CHECK(!install(sd2, f, host + "/cut", index, "matrici", "c", err) && err.find("interrotto") != std::string::npos);
    CHECK(listInstalled(sd2, "").empty() && tmpClean(sd2));
    // dipendenze mancanti e cicli: errori chiari prima di scaricare
    std::vector<PlanStep> plan;
    CHECK(!makePlan(index, {}, "orfana", "c", fw(), plan, err) && err.find("dipendenza mancante: inesistente") != std::string::npos);
    CHECK(!makePlan(index, {}, "anello-a", "c", fw(), plan, err) && err.find("ciclo di dipendenze") != std::string::npos);
    // AC durante un download lento: annullato, niente in LIB
    {
        RamFileSystem sd3;
        CHECK(makePlan(index, {}, "grande", "c", fw(), plan, err));
        net::HttpsSource src(f, host + "/slow");
        Installer inst(sd3, src);
        inst.start(plan, fw());
        for (int i = 0; i < 40 && inst.tick(4096); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(inst.state() == Installer::State::Fetching);
        inst.cancel();
        CHECK(inst.state() == Installer::State::Cancelled && listInstalled(sd3, "").empty() && tmpClean(sd3));
    }
    // dopo l'AC il fetcher si riusa
    CHECK(fetchAll(f, host + "/ok/index.json", body, err) && body == onDisk);
    return testutil::finish("https_lib_test");
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--make-repo") return makeRepo(argv[2]);
    if (argc == 6 && std::string(argv[1]) == "--run") return runTests(argv[2], argv[3], argv[4], argv[5]);
    std::fprintf(stderr, "uso: %s --make-repo DIR | --run PORTA CA ALTRA_CA DIR\n", argv[0]);
    return 2;
}
