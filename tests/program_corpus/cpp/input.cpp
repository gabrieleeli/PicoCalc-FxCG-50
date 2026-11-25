// lettura con cin: numeri, parole, getline fino alla fine dell'input (LP-12)
#include <iostream>
#include <string>
#include <vector>

int main() {
    int n;
    std::cin >> n;
    std::vector<double> v(n);
    for (int i = 0; i < n; ++i) std::cin >> v[i];
    std::string name;
    std::cin.ignore();
    std::getline(std::cin, name);
    double s = 0;
    for (double x : v) s += x;
    std::cout << "Ciao " << name << ", media " << s / n << '\n';
    std::string line;
    int lines = 0;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) ++lines;
        std::cout << '[' << line << ']';
    }
    std::cout << '\n' << lines << '\n';
    return 0;
}
