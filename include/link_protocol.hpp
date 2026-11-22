#pragma once
// Nucleo di Link (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
//
// Formato PROPRIO (il protocollo Link Casio tra calcolatrici non e' usato):
// un trasferimento porta un PACCHETTO (.pclink: il workspace e/o file
// dell'utente) diviso in trame con CRC-32, ognuna confermata (stop-and-wait).
//
// Trama (little endian):
//   0x7E 'P' | tipo u8 | seq u16 | lunghezza u16 (<= MaxPayload) | dati | CRC-32 u32
// Il CRC copre tipo, seq, lunghezza e dati (polinomio 0xEDB88320, come il
// record). Il decodificatore si risincronizza da solo: i byte che non formano
// una trama valida (righe di log sulla stessa USB seriale, rumore) vengono
// scartati. Sequenza: BEGIN (seq 0: totale u32, CRC u32 del pacchetto, nome)
// -> ACK 0; DATA seq 1..n -> ACK; END -> ACK (CRC del pacchetto verificato) o
// NAK. Ritrasmissione dopo un timeout, al piu' MaxRetries volte; ABORT annulla.
// Lo stesso protocollo e' scritto in Python in tools/picocalc_link.py.
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fx::link {

constexpr std::uint8_t Sync0 = 0x7E, Sync1 = 'P';
constexpr std::size_t MaxPayload = 1024;
constexpr std::size_t FrameOverhead = 11;                 // sync 2, tipo 1, seq 2, lunghezza 2, CRC 4
constexpr std::uint32_t RetryMs = 1000;                   // attesa di una conferma
constexpr int MaxRetries = 5;
constexpr std::uint32_t IdleMs = 15000;                   // ricezione ferma a meta': Time Out
constexpr std::size_t MaxPackageBytes = 8u * 1024u * 1024u;

enum class Type : std::uint8_t { Begin = 2, Data = 3, End = 4, Ack = 5, Nak = 6, Abort = 7 };
struct Frame {
    Type type{Type::Ack};
    std::uint16_t seq{0};
    std::vector<std::uint8_t> payload;
};
std::vector<std::uint8_t> encodeFrame(const Frame& f);

// Decodificatore a flusso: push() di byte qualsiasi, next() restituisce le trame valide.
class Decoder {
public:
    void push(const std::uint8_t* data, std::size_t size);
    bool next(Frame& out);
    std::size_t discarded() const noexcept { return discarded_; }   // byte scartati (rumore, CRC errati)
private:
    std::vector<std::uint8_t> buf_;
    std::size_t discarded_{0};
};

// Canale seriale non bloccante (USB CDC sul dispositivo, tubo nei test).
class Port {
public:
    virtual ~Port() = default;
    virtual std::size_t read(std::uint8_t* buf, std::size_t max) = 0;   // byte disponibili ora (0 = nessuno)
    virtual void write(const std::uint8_t* data, std::size_t size) = 0;
};

// ---- pacchetto .pclink
struct Entry {
    enum class Kind : std::uint8_t { Workspace = 0, File = 1 };
    Kind kind{Kind::File};
    std::string path;                                     // "PICOCALC/SHEET/CONTI.pcsheet" ("" per il workspace)
    std::vector<std::uint8_t> data;
    bool operator==(const Entry& o) const noexcept { return kind == o.kind && path == o.path && data == o.data; }
};
std::vector<std::uint8_t> encodePackage(const std::vector<Entry>& entries);
// false se i dati non sono validi (percorsi fuori da PICOCALC/, "..", nomi vuoti).
bool decodePackage(const std::uint8_t* data, std::size_t size, std::vector<Entry>& out);
bool validPath(const std::string& path) noexcept;

// ---- trasferimento (un'unita' di lavoro = una trama inviata o ricevuta)
enum class Status : std::uint8_t { Running, Done, Failed };
using Clock = std::function<std::uint32_t()>;
class Sender final : public step::Task {
public:
    Sender(Port& port, Clock clock, std::vector<std::uint8_t> package, std::string name);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return package_.size(); }
    std::size_t done() const override { return sent_; }
    void cancelled() override;                            // AC: ABORT al PC
    Status status() const noexcept { return status_; }
    const std::string& error() const noexcept { return error_; }
    std::size_t retries() const noexcept { return retries_; }
private:
    void sendCurrent();
    Port& port_;
    Clock clock_;
    std::vector<std::uint8_t> package_;
    std::string name_;
    Decoder dec_;
    std::uint16_t seq_{0};                                // trama in attesa di conferma
    std::uint16_t lastSeq_{0};                            // seq della trama END
    std::size_t sent_{0};
    std::uint32_t sentAt_{0};
    int tries_{0};
    std::size_t retries_{0};
    Status status_{Status::Running};
    std::string error_;
};
class Receiver final : public step::Task {
public:
    Receiver(Port& port, Clock clock, std::size_t maxBytes = MaxPackageBytes);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return expected_; }
    std::size_t done() const override { return data_.size(); }
    void cancelled() override;
    Status status() const noexcept { return status_; }
    bool started() const noexcept { return started_; }
    const std::string& error() const noexcept { return error_; }
    const std::string& name() const noexcept { return name_; }
    std::vector<std::uint8_t>& package() noexcept { return data_; }
private:
    void reply(Type t, std::uint16_t seq);
    Port& port_;
    Clock clock_;
    std::size_t max_;
    Decoder dec_;
    bool started_{false};
    std::size_t expected_{0};
    std::uint32_t crc_{0};
    std::uint16_t next_{1};
    std::uint32_t lastAt_{0};
    std::string name_;
    std::vector<std::uint8_t> data_;
    Status status_{Status::Running};
    std::string error_;
};

// Stato della modalita' nel workspace (sezione 0x010D): ultimo trasferimento.
struct State {
    std::uint32_t sent{0}, received{0};                   // pacchetti inviati e ricevuti
    std::uint32_t lastBytes{0};
    std::string lastName;
    std::string lastError;
    bool operator==(const State& o) const noexcept { return sent == o.sent && received == o.received && lastBytes == o.lastBytes && lastName == o.lastName && lastError == o.lastError; }
};
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);

// Coppia di porte collegate in memoria (simulatore, test): cio' che si scrive
// su una si legge dall'altra.
class LoopPort final : public Port {
public:
    LoopPort* peer{nullptr};
    std::vector<std::uint8_t> inbox;
    std::size_t read(std::uint8_t* buf, std::size_t max) override;
    void write(const std::uint8_t* data, std::size_t size) override;
    std::function<void(std::vector<std::uint8_t>&)> corrupt;   // test: altera i byte in transito
};
void connect(LoopPort& a, LoopPort& b) noexcept;

} // namespace fx::link
