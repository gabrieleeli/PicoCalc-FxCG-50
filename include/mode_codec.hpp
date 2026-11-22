#pragma once
// Codifica binaria comune delle sezioni delle modalita' Base (v1.17.0, solo
// Pimoroni Pico Plus 2 W): interi little endian, double a 64 bit, stringhe con
// lunghezza u16. Reader non lancia: `ok` diventa false al primo errore
// (lunghezza oltre i dati, stringa oltre il limite) e i valori letti dopo sono 0.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fx::codec {
struct Writer {
    std::vector<std::uint8_t>& out;
    void u8(std::uint8_t v) { out.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void f64(double d) { std::uint64_t b; std::memcpy(&b, &d, 8); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void str(const std::string& s) { u16(static_cast<std::uint16_t>(s.size())); out.insert(out.end(), s.begin(), s.end()); }
    void blob(const std::vector<std::uint8_t>& v) { u32(static_cast<std::uint32_t>(v.size())); out.insert(out.end(), v.begin(), v.end()); }
};
struct Reader {
    const std::uint8_t* p;
    std::size_t n;
    std::size_t i{0};
    bool ok{true};
    bool need(std::size_t k) { if (!ok || k > n - i) ok = false; return ok; }
    std::uint8_t u8() { return need(1) ? p[i++] : 0; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    double f64() { const std::uint64_t lo = u32(), hi = u32(); const std::uint64_t b = lo | (hi << 32); double d; std::memcpy(&d, &b, 8); return d; }
    std::string str(std::size_t max) {
        const std::size_t k = u16();
        if (k > max || !need(k)) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(p + i), k);
        i += k;
        return s;
    }
    std::vector<std::uint8_t> blob(std::size_t max) {
        const std::size_t k = u32();
        if (k > max || !need(k)) { ok = false; return {}; }
        std::vector<std::uint8_t> v(p + i, p + i + k);
        i += k;
        return v;
    }
    bool end() const noexcept { return ok && i == n; }
};
} // namespace fx::codec
