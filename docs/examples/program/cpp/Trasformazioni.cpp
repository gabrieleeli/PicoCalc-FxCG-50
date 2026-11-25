// Trasformazioni del piano con le matrici 2x2 e le coordinate omogenee 3x3:
// rotazione, simmetria, scala, traslazione e composizione applicate a un
// triangolo. Le coordinate si stampano; F6 GRAPH mostra il triangolo
// originale e quello trasformato.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using Matrice = std::vector<std::vector<double>>;

Matrice prodotto(const Matrice& a, const Matrice& b) {
    Matrice r(a.size(), std::vector<double>(b[0].size(), 0.0));
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b[0].size(); ++j)
            for (std::size_t k = 0; k < b.size(); ++k) r[i][j] += a[i][k] * b[k][j];
    return r;
}
Matrice rotazione(double gradi) {
    const double t = gradi * std::acos(-1.0) / 180;
    return {{std::cos(t), -std::sin(t), 0}, {std::sin(t), std::cos(t), 0}, {0, 0, 1}};
}
Matrice traslazione(double dx, double dy) { return {{1, 0, dx}, {0, 1, dy}, {0, 0, 1}}; }
Matrice scala(double sx, double sy) { return {{sx, 0, 0}, {0, sy, 0}, {0, 0, 1}}; }
Matrice simmetriaY() { return {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}}; }

// i vertici sono le colonne (x, y, 1)
void stampa(const char* nome, const Matrice& p) {
    std::cout << std::left << std::setw(12) << nome << std::right;
    for (std::size_t j = 0; j < p[0].size(); ++j) {
        const double x = std::fabs(p[0][j]) < 1e-12 ? 0 : p[0][j], y = std::fabs(p[1][j]) < 1e-12 ? 0 : p[1][j];
        std::cout << " (" << std::setw(6) << x << "," << std::setw(6) << y << ")";
    }
    std::cout << '\n';
}
void disegna(const Matrice& p) {
    for (std::size_t j = 0; j < p[0].size(); ++j) {
        const std::size_t k = (j + 1) % p[0].size();
        fx::line(p[0][j], p[1][j], p[0][k], p[1][k]);
    }
}

int main() {
    std::cout << std::fixed << std::setprecision(2);
    const Matrice triangolo = {{1, 4, 1}, {1, 1, 3}, {1, 1, 1}};
    stampa("originale", triangolo);
    stampa("rot 90", prodotto(rotazione(90), triangolo));
    stampa("simm. y", prodotto(simmetriaY(), triangolo));
    stampa("scala 2;0.5", prodotto(scala(2, 0.5), triangolo));
    // prima ruota di 45 gradi, poi trasla di (2, -1): la matrice composta e' T*R
    const Matrice composta = prodotto(traslazione(2, -1), rotazione(45));
    const Matrice risultato = prodotto(composta, triangolo);
    stampa("rot+trasl", risultato);
    std::cout << "det(R45) = " << rotazione(45)[0][0] * rotazione(45)[1][1] - rotazione(45)[0][1] * rotazione(45)[1][0] << '\n';
    fx::viewWindow(-6, 6, 1, -4, 6, 1);
    disegna(triangolo);
    disegna(risultato);
    return 0;
}
