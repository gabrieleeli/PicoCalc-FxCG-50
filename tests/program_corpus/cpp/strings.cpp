// std::string: costruttori, ricerca, modifica, conversioni (LP-11)
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

int main() {
    std::string s("Hello, World");
    std::string t(3, '-');
    std::cout << t << s << t << '\n';
    std::cout << s.size() << ' ' << s.length() << ' ' << s.empty() << '\n';
    std::cout << s.find("World") << ' ' << (s.find("xyz") == std::string::npos) << '\n';
    std::cout << s.substr(7) << '|' << s.substr(0, 5) << '\n';
    s.replace(0, 5, "Ciao");
    std::cout << s << '\n';
    s.insert(4, "!!");
    s.erase(s.size() - 1);
    std::cout << s << '\n';
    std::string u = s;
    std::reverse(u.begin(), u.end());
    std::cout << u << '\n';
    for (char& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    std::cout << u << '\n';
    std::cout << (std::string("abc") < std::string("abd")) << (s == "Ciao!!, Worl") << '\n';
    std::cout << s.front() << s.back() << s[2] << s.at(3) << '\n';
    std::string num = std::to_string(42) + "/" + std::to_string(-7);
    std::cout << num << ' ' << std::stoi("123") + 1 << ' ' << std::stod("2.5") * 2 << '\n';
    for (char c : std::string("xyz")) std::cout << c << '.';
    std::cout << '\n';
    std::cout << s.rfind('o') << ' ' << s.find('o') << ' ' << s.compare("Ciao") << '\n';
    std::string a = "ab";
    a += 'c';
    a.append("de");
    a.push_back('f');
    a.pop_back();
    std::cout << a << ' ' << a.c_str() << ' ' << std::string(a, 2) << ' ' << std::string("hello", 3) << '\n';
    s.clear();
    std::cout << s.empty() << s.size() << '\n';
    std::string w = "  spazi  ";
    std::size_t b = w.find_first_not_of(' '), e = w.find_last_not_of(' ');
    std::cout << '[' << w.substr(b, e - b + 1) << ']' << '\n';
    return 0;
}
