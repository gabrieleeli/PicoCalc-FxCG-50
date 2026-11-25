// statistiche di un testo letto dall'input: parole, frequenze ordinate, lunghezze
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <utility>

int main() {
    std::string line;
    std::map<std::string, int> freq;
    int lines = 0, words = 0, letters = 0;
    while (std::getline(std::cin, line)) {
        ++lines;
        std::string w;
        for (std::size_t i = 0; i <= line.size(); ++i) {
            const char c = i < line.size() ? line[i] : ' ';
            if (std::isalpha(static_cast<unsigned char>(c))) {
                w += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                ++letters;
            } else if (!w.empty()) {
                ++freq[w];
                ++words;
                w.clear();
            }
        }
    }
    std::cout << lines << " righe, " << words << " parole, " << letters << " lettere, " << freq.size() << " diverse\n";
    std::vector<std::pair<std::string, int>> v(freq.begin(), freq.end());
    std::sort(v.begin(), v.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    for (std::size_t i = 0; i < v.size() && i < 5; ++i) std::cout << std::setw(8) << v[i].first << ' ' << std::string(v[i].second, '*') << '\n';
    std::string longest;
    for (const auto& p : v)
        if (p.first.size() > longest.size()) longest = p.first;
    std::cout << "piu' lunga: " << longest << " (" << longest.size() << ")\n";
    std::cout << std::fixed << std::setprecision(2) << "media " << static_cast<double>(letters) / words << " lettere\n";
    return 0;
}
