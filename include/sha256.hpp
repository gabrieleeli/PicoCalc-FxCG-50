#pragma once
// SHA-256 (FIPS 180-4) incrementale (v1.20.0, solo Pimoroni Pico Plus 2 W):
// impronte dei file delle librerie (library.json, index.json, VERIFY) e delle
// password dei programmi. Nessuna dipendenza esterna: lo stesso codice gira
// sul dispositivo e nei test sul PC.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace fx::crypto {

class Sha256 {
public:
    Sha256() noexcept { reset(); }
    void reset() noexcept;
    void update(const void* data, std::size_t size) noexcept;
    void update(std::string_view s) noexcept { update(s.data(), s.size()); }
    std::array<std::uint8_t, 32> finish() noexcept;      // poi reset() per un nuovo calcolo
    std::string hex() noexcept;                          // finish() in 64 cifre esadecimali minuscole
private:
    void block(const std::uint8_t* p) noexcept;
    std::uint32_t h_[8];
    std::uint8_t buf_[64];
    std::size_t used_{0};
    std::uint64_t bits_{0};
};

std::string sha256Hex(std::string_view data);
// Confronto di impronte (64 cifre esadecimali, maiuscole o minuscole).
bool sameDigest(std::string_view a, std::string_view b) noexcept;
bool validDigest(std::string_view hex) noexcept;

} // namespace fx::crypto
