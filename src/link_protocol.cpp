// Nucleo di Link (v1.17.0): vedi link_protocol.hpp.
#include "link_protocol.hpp"
#include "mode_codec.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cctype>

namespace fx::link {
namespace {
void put16(std::vector<std::uint8_t>& o, std::uint16_t v) { o.push_back(static_cast<std::uint8_t>(v)); o.push_back(static_cast<std::uint8_t>(v >> 8)); }
void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { put16(o, static_cast<std::uint16_t>(v)); put16(o, static_cast<std::uint16_t>(v >> 16)); }
std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t get32(const std::uint8_t* p) { return static_cast<std::uint32_t>(get16(p)) | (static_cast<std::uint32_t>(get16(p + 2)) << 16); }
bool knownType(std::uint8_t t) { return t >= static_cast<std::uint8_t>(Type::Begin) && t <= static_cast<std::uint8_t>(Type::Abort); }
} // namespace

std::vector<std::uint8_t> encodeFrame(const Frame& f) {
    std::vector<std::uint8_t> o;
    o.reserve(FrameOverhead + f.payload.size());
    o.push_back(Sync0); o.push_back(Sync1);
    o.push_back(static_cast<std::uint8_t>(f.type));
    put16(o, f.seq);
    put16(o, static_cast<std::uint16_t>(f.payload.size()));
    o.insert(o.end(), f.payload.begin(), f.payload.end());
    put32(o, storage::crc32(o.data() + 2, o.size() - 2));
    return o;
}

void Decoder::push(const std::uint8_t* data, std::size_t size) {
    // Il buffer non cresce oltre una trama piu' il rumore che la precede.
    buf_.insert(buf_.end(), data, data + size);
    if (buf_.size() > 4 * (MaxPayload + FrameOverhead)) {
        const std::size_t drop = buf_.size() - 2 * (MaxPayload + FrameOverhead);
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(drop));
        discarded_ += drop;
    }
}
bool Decoder::next(Frame& out) {
    for (;;) {
        // Cerca 0x7E 'P'.
        std::size_t s = 0;
        while (s + 1 < buf_.size() && !(buf_[s] == Sync0 && buf_[s + 1] == Sync1)) ++s;
        if (s + 1 >= buf_.size()) {                       // nessuna intestazione: tieni l'ultimo byte (puo' essere 0x7E)
            const std::size_t keep = !buf_.empty() && buf_.back() == Sync0 ? 1 : 0;
            discarded_ += buf_.size() - keep;
            buf_.erase(buf_.begin(), buf_.end() - static_cast<std::ptrdiff_t>(keep));
            return false;
        }
        if (s) { buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(s)); discarded_ += s; }
        if (buf_.size() < 7) return false;
        const std::size_t len = get16(&buf_[5]);
        if (!knownType(buf_[2]) || len > MaxPayload) { buf_.erase(buf_.begin()); ++discarded_; continue; }
        if (buf_.size() < FrameOverhead + len) return false;
        const std::uint32_t crc = get32(&buf_[7 + len]);
        if (storage::crc32(&buf_[2], 5 + len) != crc) { buf_.erase(buf_.begin()); ++discarded_; continue; }
        out.type = static_cast<Type>(buf_[2]);
        out.seq = get16(&buf_[3]);
        out.payload.assign(buf_.begin() + 7, buf_.begin() + 7 + static_cast<std::ptrdiff_t>(len));
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(FrameOverhead + len));
        return true;
    }
}

// ---------------------------------------------------------------- pacchetto
bool validPath(const std::string& path) noexcept {
    if (path.size() < 10 || path.size() > 96 || path.compare(0, 9, "PICOCALC/") != 0) return false;
    if (path.find("..") != std::string::npos || path.find("//") != std::string::npos || path.back() == '/') return false;
    for (char c : path) {
        const auto u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '/' || c == '.' || c == '_' || c == '-' || c == '{' || c == '}' || c == '\'' || c == '~')) return false;
    }
    return true;
}
std::vector<std::uint8_t> encodePackage(const std::vector<Entry>& entries) {
    std::vector<std::uint8_t> out;
    codec::Writer w{out};
    w.u8(1);
    w.u16(static_cast<std::uint16_t>(entries.size()));
    for (const Entry& e : entries) { w.u8(static_cast<std::uint8_t>(e.kind)); w.str(e.path); w.blob(e.data); }
    return out;
}
bool decodePackage(const std::uint8_t* data, std::size_t size, std::vector<Entry>& out) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    const std::size_t n = r.u16();
    if (n > 256) return false;
    std::vector<Entry> v;
    int workspaces = 0;
    for (std::size_t k = 0; k < n && r.ok; ++k) {
        Entry e;
        const std::uint8_t kind = r.u8();
        e.path = r.str(96);
        e.data = r.blob(MaxPackageBytes);
        if (!r.ok || kind > 1) return false;
        e.kind = static_cast<Entry::Kind>(kind);
        if (e.kind == Entry::Kind::Workspace ? (!e.path.empty() || ++workspaces > 1) : !validPath(e.path)) return false;
        v.push_back(std::move(e));
    }
    if (!r.end()) return false;
    out = std::move(v);
    return true;
}

// ---------------------------------------------------------------- invio
Sender::Sender(Port& port, Clock clock, std::vector<std::uint8_t> package, std::string name)
    : port_(port), clock_(std::move(clock)), package_(std::move(package)), name_(std::move(name)) {
    const std::size_t chunks = (package_.size() + MaxPayload - 1) / MaxPayload;
    lastSeq_ = static_cast<std::uint16_t>(chunks + 1);
    if (package_.size() > MaxPackageBytes || chunks + 1 >= 0xFFFF) { status_ = Status::Failed; error_ = "Memory ERROR: pacchetto troppo grande"; return; }
    sendCurrent();
}
void Sender::sendCurrent() {
    Frame f;
    f.seq = seq_;
    if (seq_ == 0) {
        f.type = Type::Begin;
        put32(f.payload, static_cast<std::uint32_t>(package_.size()));
        put32(f.payload, storage::crc32(package_.data(), package_.size()));
        const std::string n = name_.substr(0, 64);
        f.payload.push_back(static_cast<std::uint8_t>(n.size()));
        f.payload.insert(f.payload.end(), n.begin(), n.end());
    } else if (seq_ < lastSeq_) {
        f.type = Type::Data;
        const std::size_t at = static_cast<std::size_t>(seq_ - 1) * MaxPayload;
        const std::size_t len = std::min(MaxPayload, package_.size() - at);
        f.payload.assign(package_.begin() + static_cast<std::ptrdiff_t>(at), package_.begin() + static_cast<std::ptrdiff_t>(at + len));
    } else {
        f.type = Type::End;
    }
    const auto bytes = encodeFrame(f);
    port_.write(bytes.data(), bytes.size());
    sentAt_ = clock_();
}
bool Sender::step(step::Budget& budget) {
    while (status_ == Status::Running) {
        std::uint8_t buf[256];
        std::size_t n;
        while ((n = port_.read(buf, sizeof buf)) > 0) dec_.push(buf, n);
        Frame f;
        bool progressed = false;
        while (dec_.next(f)) {
            if (f.type == Type::Abort) { status_ = Status::Failed; error_ = "Annullato dal PC"; return true; }
            if (f.type == Type::Ack && f.seq == seq_) {
                if (seq_ > 0 && seq_ < lastSeq_) sent_ = std::min(package_.size(), static_cast<std::size_t>(seq_) * MaxPayload);
                if (seq_ == lastSeq_) { status_ = Status::Done; sent_ = package_.size(); return true; }
                ++seq_;
                tries_ = 0;
                sendCurrent();
                progressed = true;
            } else if (f.type == Type::Nak && f.seq == seq_) {
                if (seq_ == lastSeq_) { status_ = Status::Failed; error_ = "Data ERROR: CRC del pacchetto rifiutato dal PC"; return true; }
                ++retries_;
                sendCurrent();
                progressed = true;
            }
        }
        if (!progressed) {
            if (clock_() - sentAt_ >= RetryMs) {
                if (++tries_ > MaxRetries) { status_ = Status::Failed; error_ = "Time Out: nessuna risposta dal PC"; return true; }
                ++retries_;
                sendCurrent();
            }
            return false;                                     // aspetta la prossima occasione (tick)
        }
        if (!budget.consume()) return false;
    }
    return true;
}
void Sender::cancelled() {
    if (status_ != Status::Running) return;
    Frame f; f.type = Type::Abort; f.seq = seq_;
    const std::string why = "Break";
    f.payload.assign(why.begin(), why.end());
    const auto bytes = encodeFrame(f);
    port_.write(bytes.data(), bytes.size());
    status_ = Status::Failed;
    error_ = "Break";
}

// ---------------------------------------------------------------- ricezione
Receiver::Receiver(Port& port, Clock clock, std::size_t maxBytes) : port_(port), clock_(std::move(clock)), max_(maxBytes) { lastAt_ = clock_(); }
void Receiver::reply(Type t, std::uint16_t seq) {
    Frame f; f.type = t; f.seq = seq;
    const auto bytes = encodeFrame(f);
    port_.write(bytes.data(), bytes.size());
}
bool Receiver::step(step::Budget& budget) {
    while (status_ == Status::Running) {
        std::uint8_t buf[256];
        std::size_t n;
        while ((n = port_.read(buf, sizeof buf)) > 0) dec_.push(buf, n);
        Frame f;
        if (!dec_.next(f)) {
            if (started_ && clock_() - lastAt_ >= IdleMs) { status_ = Status::Failed; error_ = "Time Out: trasferimento interrotto"; return true; }
            return false;
        }
        lastAt_ = clock_();
        switch (f.type) {
        case Type::Begin: {
            if (f.payload.size() < 9 || f.payload.size() != 9u + f.payload[8]) { reply(Type::Nak, f.seq); break; }
            const std::size_t total = get32(f.payload.data());
            if (started_ && next_ > 1) { reply(Type::Ack, 0); break; }   // BEGIN ripetuto: la conferma si e' persa
            if (total > max_) {
                Frame a; a.type = Type::Abort; const std::string m = "Memory ERROR"; a.payload.assign(m.begin(), m.end());
                const auto bytes = encodeFrame(a); port_.write(bytes.data(), bytes.size());
                status_ = Status::Failed; error_ = "Memory ERROR: pacchetto troppo grande per la memoria"; return true;
            }
            started_ = true;
            expected_ = total;
            crc_ = get32(f.payload.data() + 4);
            name_.assign(reinterpret_cast<const char*>(f.payload.data() + 9), f.payload[8]);
            data_.clear();
            data_.reserve(total);
            next_ = 1;
            reply(Type::Ack, 0);
            break;
        }
        case Type::Data:
            if (!started_) { reply(Type::Nak, f.seq); break; }
            if (f.seq < next_) { reply(Type::Ack, f.seq); break; }                     // doppione: conferma di nuovo
            if (f.seq != next_ || data_.size() + f.payload.size() > expected_) { reply(Type::Nak, f.seq); break; }
            data_.insert(data_.end(), f.payload.begin(), f.payload.end());
            reply(Type::Ack, f.seq);
            ++next_;
            break;
        case Type::End:
            if (!started_) { reply(Type::Nak, f.seq); break; }
            if (data_.size() != expected_ || storage::crc32(data_.data(), data_.size()) != crc_) {
                reply(Type::Nak, f.seq);
                status_ = Status::Failed; error_ = "Data ERROR: CRC del pacchetto errato"; return true;
            }
            reply(Type::Ack, f.seq);
            status_ = Status::Done;
            return true;
        case Type::Abort:
            status_ = Status::Failed; error_ = "Annullato dal PC"; return true;
        default: break;
        }
        if (!budget.consume()) return false;
    }
    return true;
}
void Receiver::cancelled() {
    if (status_ != Status::Running) return;
    if (started_) {
        Frame f; f.type = Type::Abort; f.seq = next_;
        const std::string why = "Break"; f.payload.assign(why.begin(), why.end());
        const auto bytes = encodeFrame(f); port_.write(bytes.data(), bytes.size());
    }
    status_ = Status::Failed;
    error_ = "Break";
}

// ---------------------------------------------------------------- stato e porte di prova
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1); w.u32(s.sent); w.u32(s.received); w.u32(s.lastBytes); w.str(s.lastName); w.str(s.lastError);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    t.sent = r.u32(); t.received = r.u32(); t.lastBytes = r.u32(); t.lastName = r.str(64); t.lastError = r.str(160);
    if (!r.end()) return false;
    s = std::move(t);
    return true;
}
std::size_t LoopPort::read(std::uint8_t* buf, std::size_t max) {
    const std::size_t n = std::min(max, inbox.size());
    std::copy(inbox.begin(), inbox.begin() + static_cast<std::ptrdiff_t>(n), buf);
    inbox.erase(inbox.begin(), inbox.begin() + static_cast<std::ptrdiff_t>(n));
    return n;
}
void LoopPort::write(const std::uint8_t* data, std::size_t size) {
    if (!peer) return;
    std::vector<std::uint8_t> bytes(data, data + size);
    if (corrupt) corrupt(bytes);
    peer->inbox.insert(peer->inbox.end(), bytes.begin(), bytes.end());
}
void connect(LoopPort& a, LoopPort& b) noexcept { a.peer = &b; b.peer = &a; }
} // namespace fx::link
