// frazioni 1.0.0 - frazioni esatte per i programmi C++ della PicoCalc.
// Sempre ridotte ai minimi termini, con il denominatore positivo.
#pragma once
#include <iostream>
#include <stdexcept>
#include <string>

class Frazione {
public:
    Frazione(long numeratore = 0, long denominatore = 1) : n_(numeratore), d_(denominatore) { riduci(); }
    long numeratore() const { return n_; }
    long denominatore() const { return d_; }
    double valore() const { return static_cast<double>(n_) / static_cast<double>(d_); }
    Frazione inversa() const { return Frazione(d_, n_); }
    Frazione operator+(const Frazione& o) const { return Frazione(n_ * o.d_ + o.n_ * d_, d_ * o.d_); }
    Frazione operator-(const Frazione& o) const { return Frazione(n_ * o.d_ - o.n_ * d_, d_ * o.d_); }
    Frazione operator*(const Frazione& o) const { return Frazione(n_ * o.n_, d_ * o.d_); }
    Frazione operator/(const Frazione& o) const { return Frazione(n_ * o.d_, d_ * o.n_); }
    bool operator==(const Frazione& o) const { return n_ == o.n_ && d_ == o.d_; }
    bool operator!=(const Frazione& o) const { return !(*this == o); }
    bool operator<(const Frazione& o) const { return n_ * o.d_ < o.n_ * d_; }
    std::string testo() const { return d_ == 1 ? std::to_string(n_) : std::to_string(n_) + "/" + std::to_string(d_); }

private:
    static long mcd(long a, long b) {
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        while (b != 0) { const long t = a % b; a = b; b = t; }
        return a;
    }
    void riduci() {
        if (d_ == 0) throw std::invalid_argument("frazione con denominatore zero");
        if (d_ < 0) { n_ = -n_; d_ = -d_; }
        const long g = mcd(n_, d_);
        if (g > 1) { n_ /= g; d_ /= g; }
    }
    long n_, d_;
};

inline std::ostream& operator<<(std::ostream& os, const Frazione& f) { return os << f.testo(); }
