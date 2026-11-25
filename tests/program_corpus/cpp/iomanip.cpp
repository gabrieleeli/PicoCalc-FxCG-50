// formattazione dei numeri con iostream e iomanip (LP-12)
#include <iostream>
#include <iomanip>
#include <cmath>

int main() {
    double pi = std::acos(-1.0);
    std::cout << pi << ' ' << 1.0 / 3 << ' ' << 100.0 << ' ' << 1e21 << ' ' << 2.5e-7 << '\n';
    std::cout << std::setprecision(3) << pi << ' ' << 1234.5678 << '\n';
    std::cout << std::fixed << std::setprecision(2) << pi << ' ' << 2.0 << ' ' << -0.005 << '\n';
    std::cout << std::setw(8) << 3.14159 << '|' << std::left << std::setw(6) << 42 << '|' << '\n';
    std::cout << std::right << std::setfill('0') << std::setw(5) << 42 << '\n';
    std::cout << std::setfill(' ') << std::scientific << 12345.678 << '\n';
    std::cout << std::hex << 255 << ' ' << std::dec << 255 << ' ' << std::oct << 8 << std::dec << '\n';
    std::cout << std::boolalpha << true << ' ' << (1 > 2) << std::noboolalpha << ' ' << true << '\n';
    for (int i = 1; i <= 3; ++i) std::cout << std::setw(4) << i * i;
    std::cout << '\n';
    std::cout << std::fixed << std::setprecision(1);
    for (double x = 0; x <= 1.0; x += 0.25) std::cout << x << ' ';
    std::cout << std::endl;
    return 0;
}
