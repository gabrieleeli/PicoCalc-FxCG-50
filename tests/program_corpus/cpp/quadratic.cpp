// equazioni di secondo grado con una classe e l'input da cin
#include <iostream>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <string>

class Quadratic {
    double a, b, c;
public:
    Quadratic(double a, double b, double c) : a(a), b(b), c(c) {}
    double discriminant() const { return b * b - 4 * a * c; }
    std::string solve() const {
        if (a == 0) return b == 0 ? (c == 0 ? "infinite soluzioni" : "impossibile") : "x = " + fmt(-c / b);
        const double d = discriminant();
        if (d > 0) return "x1 = " + fmt((-b - std::sqrt(d)) / (2 * a)) + ", x2 = " + fmt((-b + std::sqrt(d)) / (2 * a));
        if (d == 0) return "x = " + fmt(-b / (2 * a)) + " (doppia)";
        return "x = " + fmt(-b / (2 * a)) + " +- " + fmt(std::sqrt(-d) / (2 * std::fabs(a))) + "i";
    }
    static std::string fmt(double v) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << v;
        return os.str();
    }
};

int main() {
    double a, b, c;
    int n = 0;
    while (std::cin >> a >> b >> c) {
        Quadratic q(a, b, c);
        std::cout << a << "x^2 + " << b << "x + " << c << ": " << q.solve() << '\n';
        ++n;
    }
    std::cout << n << " equazioni\n";
    return 0;
}
