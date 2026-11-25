// ostringstream e istringstream: costruire e analizzare testo (LP-16)
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

bool palindrome(const std::string& s) {
    std::string t;
    for (char c : s)
        if (std::isalpha(static_cast<unsigned char>(c))) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (std::size_t i = 0; i < t.size() / 2; ++i)
        if (t[i] != t[t.size() - 1 - i]) return false;
    return true;
}

int main() {
    std::ostringstream os;
    os << "x=" << 3 << ", y=" << 2.5 << ", ok=" << true;
    std::string s = os.str();
    std::cout << s << " (" << s.size() << ")\n";
    std::istringstream in("10 20 30 quaranta 50");
    int n, sum = 0;
    while (in >> n) sum += n;
    std::cout << sum << '\n';
    std::istringstream csv("1.5;2.5;;4");
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(csv, field, ';')) fields.push_back(field);
    std::cout << fields.size() << ' ';
    for (const auto& f : fields) std::cout << '[' << f << ']';
    std::cout << '\n';
    std::cout << palindrome("I topi non avevano nipoti") << palindrome("Ciao") << '\n';
    std::stringstream ss;
    ss << 42 << ' ' << 3.5;
    int a;
    double b;
    ss >> a >> b;
    std::cout << a * 2 << ' ' << b * 2 << '\n';
    std::string sentence = "uno due  tre";
    std::istringstream words(sentence);
    std::string w;
    int count = 0;
    while (words >> w) ++count;
    std::cout << count << '\n';
    return 0;
}
