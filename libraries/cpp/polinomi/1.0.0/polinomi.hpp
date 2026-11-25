// polinomi 1.0.0 - polinomi a coefficienti reali per i programmi C++ della PicoCalc.
// I coefficienti vanno dal grado 0: {c0, c1, c2} = c0 + c1 x + c2 x^2.
#pragma once
#include <string>
#include <vector>

class Polinomio {
public:
    explicit Polinomio(std::vector<double> coefficienti);
    int grado() const;
    double valuta(double x) const;                    // schema di Horner
    Polinomio derivata() const;
    Polinomio operator+(const Polinomio& o) const;
    Polinomio operator*(const Polinomio& o) const;
    std::string testo() const;                        // "2x^2 - 3x + 1"
    // Radici reali: formula per i gradi 1 e 2, bisezione sugli intervalli di
    // monotonia (radici della derivata) per i gradi piu' alti, in ordine crescente.
    std::vector<double> radici() const;
private:
    std::vector<double> c_;
};
