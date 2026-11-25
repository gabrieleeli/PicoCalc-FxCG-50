// polinomi 1.0.0 - vedi polinomi.hpp
#include "polinomi.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

Polinomio::Polinomio(std::vector<double> c) : c_(c) {
    while (c_.size() > 1 && c_.back() == 0) c_.pop_back();
    if (c_.empty()) c_.push_back(0);
}

int Polinomio::grado() const { return static_cast<int>(c_.size()) - 1; }

double Polinomio::valuta(double x) const {
    double s = 0;
    for (std::size_t i = c_.size(); i-- > 0;) s = s * x + c_[i];
    return s;
}

Polinomio Polinomio::derivata() const {
    std::vector<double> d;
    for (std::size_t i = 1; i < c_.size(); i++) d.push_back(c_[i] * static_cast<double>(i));
    return Polinomio(d);
}

Polinomio Polinomio::operator+(const Polinomio& o) const {
    std::vector<double> r(std::max(c_.size(), o.c_.size()), 0.0);
    for (std::size_t i = 0; i < c_.size(); i++) r[i] += c_[i];
    for (std::size_t i = 0; i < o.c_.size(); i++) r[i] += o.c_[i];
    return Polinomio(r);
}

Polinomio Polinomio::operator*(const Polinomio& o) const {
    std::vector<double> r(c_.size() + o.c_.size() - 1, 0.0);
    for (std::size_t i = 0; i < c_.size(); i++)
        for (std::size_t j = 0; j < o.c_.size(); j++) r[i + j] += c_[i] * o.c_[j];
    return Polinomio(r);
}

std::string Polinomio::testo() const {
    std::ostringstream os;
    bool primo = true;
    for (std::size_t k = c_.size(); k-- > 0;) {
        const double a = c_[k];
        if (a == 0 && !(primo && k == 0)) continue;
        const double m = std::fabs(a);
        if (primo) os << (a < 0 ? "-" : "");
        else os << (a < 0 ? " - " : " + ");
        if (m != 1 || k == 0) os << m;
        if (k >= 1) os << "x";
        if (k >= 2) os << "^" << k;
        primo = false;
    }
    return os.str();
}

std::vector<double> Polinomio::radici() const {
    std::vector<double> r;
    const int g = grado();
    if (g == 0) return r;
    if (g == 1) { r.push_back(-c_[0] / c_[1]); return r; }
    if (g == 2) {
        const double a = c_[2], b = c_[1], c = c_[0];
        const double d = b * b - 4 * a * c;
        if (d < 0) return r;
        if (d == 0) { r.push_back(-b / (2 * a)); return r; }
        const double s = std::sqrt(d);
        r.push_back((-b - s) / (2 * a));
        r.push_back((-b + s) / (2 * a));
        std::sort(r.begin(), r.end());
        return r;
    }
    // grado >= 3: tra due punti critici consecutivi c'e' al piu' una radice
    std::vector<double> punti = derivata().radici();
    double limite = 1;
    for (std::size_t i = 0; i + 1 < c_.size(); i++) limite = std::max(limite, std::fabs(c_[i] / c_.back()) + 1);   // limite di Cauchy
    punti.insert(punti.begin(), -limite);
    punti.push_back(limite);
    for (std::size_t i = 0; i + 1 < punti.size(); i++) {
        double a = punti[i], b = punti[i + 1];
        double fa = valuta(a), fb = valuta(b);
        if (std::fabs(fa) < 1e-12) { if (r.empty() || std::fabs(r.back() - a) > 1e-9) r.push_back(a); continue; }
        if (fa * fb > 0) continue;
        for (int k = 0; k < 200 && b - a > 1e-13; k++) {
            const double m = (a + b) / 2, fm = valuta(m);
            if ((fa < 0) == (fm < 0)) { a = m; fa = fm; } else b = m;
        }
        const double x = (a + b) / 2;
        if (r.empty() || std::fabs(r.back() - x) > 1e-9) r.push_back(x);
    }
    return r;
}
