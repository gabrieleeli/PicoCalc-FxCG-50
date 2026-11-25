// metodi numerici: bisezione, Newton, Simpson, con puntatori a funzione e lambda
#include <iostream>
#include <iomanip>
#include <cmath>

double bisection(double (*f)(double), double a, double b, double eps, int& steps) {
    steps = 0;
    while (b - a > eps) {
        const double m = (a + b) / 2;
        if (f(a) * f(m) <= 0) b = m;
        else a = m;
        ++steps;
    }
    return (a + b) / 2;
}
template <class F, class D>
double newton(F f, D df, double x, int iterations) {
    for (int i = 0; i < iterations; ++i) x -= f(x) / df(x);
    return x;
}
template <class F>
double simpson(F f, double a, double b, int n) {
    const double h = (b - a) / n;
    double s = f(a) + f(b);
    for (int i = 1; i < n; ++i) s += f(a + i * h) * (i % 2 ? 4 : 2);
    return s * h / 3;
}
double g(double x) { return x * x * x - x - 2; }

int main() {
    std::cout << std::fixed << std::setprecision(8);
    int steps = 0;
    const double r = bisection(g, 1, 2, 1e-9, steps);
    std::cout << "bisezione " << r << " in " << steps << " passi\n";
    const double s2 = newton([](double x) { return x * x - 2; }, [](double x) { return 2 * x; }, 1.0, 6);
    std::cout << "sqrt(2) " << s2 << " errore " << std::scientific << std::setprecision(2) << std::fabs(s2 - std::sqrt(2.0)) << '\n';
    std::cout << std::fixed << std::setprecision(8);
    const double pi = std::acos(-1.0);
    std::cout << "integrale di sin su [0, pi] " << simpson([](double x) { return std::sin(x); }, 0.0, pi, 100) << '\n';
    std::cout << "integrale di x^2 su [0, 3] " << simpson([](double x) { return x * x; }, 0.0, 3.0, 10) << '\n';
    double e = 0, term = 1;
    for (int k = 0; k < 20; ++k) {
        e += term;
        term /= k + 1;
    }
    std::cout << "e " << e << ' ' << std::exp(1.0) << '\n';
    return 0;
}
