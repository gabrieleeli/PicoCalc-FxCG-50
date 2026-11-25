// map, set, unordered_map, pair e stringstream (LP-13, LP-16)
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <utility>

int main() {
    std::string text = "il gatto e il cane e il topo";
    std::istringstream in(text);
    std::map<std::string, int> freq;
    std::string w;
    while (in >> w) freq[w]++;
    for (const auto& p : freq) std::cout << p.first << '=' << p.second << '\n';
    std::cout << freq.size() << ' ' << freq.count("il") << ' ' << freq.count("cavallo") << '\n';
    auto it = freq.find("cane");
    if (it != freq.end()) std::cout << "trovato " << it->second << '\n';
    freq.erase("e");
    for (const auto& [k, v] : freq) std::cout << k << ':' << v << ' ';
    std::cout << '\n';
    std::set<int> s = {5, 1, 3, 1, 5};
    s.insert(2);
    for (int x : s) std::cout << x;
    std::cout << ' ' << s.size() << ' ' << s.count(3) << '\n';
    s.erase(3);
    std::cout << s.count(3) << ' ' << *s.begin() << '\n';
    std::map<int, std::string> nomi;
    nomi[3] = "tre";
    nomi[1] = "uno";
    nomi.insert({2, "due"});
    nomi.insert(std::make_pair(0, "zero"));
    for (auto it2 = nomi.begin(); it2 != nomi.end(); ++it2) std::cout << it2->first << it2->second << ' ';
    std::cout << '\n' << nomi.at(2) << '\n';
    std::pair<std::string, double> p("pi", 3.25);
    std::cout << p.first << ' ' << p.second << '\n';
    return 0;
}
