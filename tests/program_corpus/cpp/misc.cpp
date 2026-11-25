// enum class, switch, do-while, costanti, interi e caratteri, operatori sui bit (LK-02, LK-03, LK-05)
#include <iostream>
#include <climits>
#include <cstdint>

enum class Color { Red, Green = 5, Blue };
enum Day { Mon = 1, Tue, Wed };

const char* name(Color c) {
    switch (c) {
    case Color::Red: return "rosso";
    case Color::Green: return "verde";
    default: return "blu";
    }
}

int main() {
    std::cout << name(Color::Blue) << ' ' << static_cast<int>(Color::Blue) << ' ' << Wed << '\n';
    int i = 0;
    do {
        i += 3;
    } while (i < 10);
    std::cout << i << '\n';
    std::cout << 7 / 2 << ' ' << -7 / 2 << ' ' << 7 % 3 << ' ' << -7 % 3 << ' ' << 7.0 / 2 << '\n';
    unsigned int u = 3;
    std::cout << u - 5 << ' ' << (5u > 3) << '\n';
    std::cout << (6 & 3) << (6 | 3) << (6 ^ 3) << (1 << 4) << (256 >> 2) << (~0) << '\n';
    char c = 'A';
    c += 2;
    std::cout << c << ' ' << static_cast<int>(c) << ' ' << char(c + 1) << ' ' << ('z' - 'a') << '\n';
    long long big = 1LL << 40;
    std::cout << big << ' ' << INT_MAX << ' ' << INT_MIN << ' ' << sizeof(long long) << sizeof(int) << sizeof(char) << '\n';
    std::uint8_t byte = 250;
    byte += 10;
    std::cout << static_cast<int>(byte) << '\n';
    int grade = 7;
    switch (grade) {
    case 10:
    case 9: std::cout << "ottimo"; break;
    case 8:
    case 7: std::cout << "buono"; break;
    default: std::cout << "da migliorare";
    }
    std::cout << '\n';
    constexpr int N = 4;
    int sq[N];
    for (int k = 0; k < N; ++k) sq[k] = k * k;
    auto total = 0;
    for (int k : sq) total += k;
    std::cout << total << ' ' << (total > 10 ? "grande" : "piccolo") << '\n';
    double d = 7.9;
    std::cout << static_cast<int>(d) << ' ' << static_cast<int>(-d) << ' ' << int(2.5 + 0.5) << '\n';
    bool flag = !true || (3 > 2 && 2 > 1);
    std::cout << flag << ' ' << (flag ? 1 : 0) + 1 << '\n';
    int a = 5;
    int b = a++;
    b += ++a;
    std::cout << a << ' ' << b << '\n';
    return 0;
}
