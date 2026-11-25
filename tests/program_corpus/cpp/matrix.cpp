// matrici: prodotto, trasposta, determinante, trasformazioni geometriche
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using Matrix = std::vector<std::vector<double>>;

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix r(a.size(), std::vector<double>(b[0].size(), 0.0));
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b[0].size(); ++j)
            for (std::size_t k = 0; k < b.size(); ++k) r[i][j] += a[i][k] * b[k][j];
    return r;
}
double det(const Matrix& m) {
    const std::size_t n = m.size();
    if (n == 1) return m[0][0];
    if (n == 2) return m[0][0] * m[1][1] - m[0][1] * m[1][0];
    double d = 0;
    for (std::size_t c = 0; c < n; ++c) {
        Matrix sub;
        for (std::size_t i = 1; i < n; ++i) {
            std::vector<double> row;
            for (std::size_t j = 0; j < n; ++j)
                if (j != c) row.push_back(m[i][j]);
            sub.push_back(row);
        }
        d += (c % 2 == 0 ? 1 : -1) * m[0][c] * det(sub);
    }
    return d;
}
void print(const Matrix& m) {
    for (const auto& row : m) {
        for (double v : row) std::cout << std::setw(8) << std::fixed << std::setprecision(3) << v;
        std::cout << '\n';
    }
}

int main() {
    Matrix a = {{1, 2, 3}, {0, 1, 4}, {5, 6, 0}};
    Matrix b = {{2, 0}, {1, 1}, {0, 3}};
    print(multiply(a, b));
    std::cout << det(a) << '\n';
    const double pi = std::acos(-1.0);
    const double t = pi / 2;
    Matrix rot = {{std::cos(t), -std::sin(t)}, {std::sin(t), std::cos(t)}};
    Matrix pts = {{1, 0, -1}, {0, 1, 0}};
    Matrix r = multiply(rot, pts);
    for (auto& row : r)
        for (auto& v : row)
            if (std::fabs(v) < 1e-12) v = 0;
    print(r);
    Matrix scale = {{2, 0}, {0, 0.5}};
    print(multiply(scale, pts));
    return 0;
}
