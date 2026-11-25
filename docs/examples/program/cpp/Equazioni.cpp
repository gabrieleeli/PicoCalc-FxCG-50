// Equazioni di secondo grado a*x^2 + b*x + c = 0.
// Scrivere i tre coefficienti su una riga (es. "1 -3 2"), una equazione per
// riga; Ctrl+D (fine dell'input) termina. Le radici reali dell'ultima
// equazione finiscono nelle variabili X e Y della calcolatrice.
#include <cmath>
#include <iomanip>
#include <iostream>

int main() {
    double a, b, c;
    int n = 0;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "a b c? (Ctrl+D per finire)\n";
    while (std::cin >> a >> b >> c) {
        ++n;
        std::cout << "#" << n << ": ";
        if (a == 0) {
            if (b == 0) std::cout << (c == 0 ? "infinite soluzioni" : "impossibile") << '\n';
            else std::cout << "lineare, x = " << -c / b << '\n';
            continue;
        }
        const double d = b * b - 4 * a * c;
        if (d > 0) {
            const double x1 = (-b - std::sqrt(d)) / (2 * a), x2 = (-b + std::sqrt(d)) / (2 * a);
            std::cout << "x1 = " << x1 << "  x2 = " << x2 << '\n';
            fx::setVar('X', x1);
            fx::setVar('Y', x2);
        } else if (d == 0) {
            const double x = -b / (2 * a);
            std::cout << "x = " << x << " (doppia)\n";
            fx::setVar('X', x);
            fx::setVar('Y', x);
        } else {
            const double re = b == 0 ? 0.0 : -b / (2 * a);       // niente "-0.0000"
            std::cout << "x = " << re << " +- " << std::sqrt(-d) / (2 * std::fabs(a)) << "i\n";
        }
    }
    std::cout << n << " equazioni\n";
    return 0;
}
