// struct, vettore di struct ordinato con piu' chiavi, tabella allineata
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

struct Student {
    std::string name;
    int grade;
    double avg;
};

bool better(const Student& a, const Student& b) {
    if (a.grade != b.grade) return a.grade > b.grade;
    return a.name < b.name;
}

int main() {
    std::vector<Student> cls = {
        {"Rossi", 8, 7.5}, {"Bianchi", 9, 8.25}, {"Verdi", 8, 6.0}, {"Neri", 9, 9.0}, {"Ancona", 7, 7.0}};
    std::sort(cls.begin(), cls.end(), better);
    std::cout << std::left << std::setw(10) << "Nome" << std::right << std::setw(6) << "Voto" << std::setw(8) << "Media" << '\n';
    for (const Student& s : cls)
        std::cout << std::left << std::setw(10) << s.name << std::right << std::setw(6) << s.grade << std::setw(8)
                  << std::fixed << std::setprecision(2) << s.avg << '\n';
    Student best = cls.front();
    best.name = "Copia";
    std::cout << cls.front().name << ' ' << best.name << '\n';
    double sum = 0;
    for (const auto& s : cls) sum += s.avg;
    std::cout << "media della classe " << sum / cls.size() << '\n';
    auto it = std::max_element(cls.begin(), cls.end(), [](const Student& a, const Student& b) { return a.avg < b.avg; });
    std::cout << it->name << '\n';
    return 0;
}
