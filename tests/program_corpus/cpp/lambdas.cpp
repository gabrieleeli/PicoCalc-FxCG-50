// lambda con catture, comparatori, contatori (LP-10)
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

int apply(int (*f)(int), int x) { return f(x); }
int twice(int x) { return 2 * x; }

int main() {
    int base = 10;
    auto add = [base](int x) { return x + base; };
    auto inc = [&base]() { ++base; };
    inc();
    inc();
    std::cout << add(1) << ' ' << base << '\n';
    int counter = 0;
    auto next = [&]() { return ++counter; };
    next();
    next();
    std::cout << next() << '\n';
    std::vector<std::string> words = {"banana", "kiwi", "mela", "ananas", "fico"};
    std::sort(words.begin(), words.end(), [](const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return a.size() < b.size();
        return a < b;
    });
    for (const auto& w : words) std::cout << w << ' ';
    std::cout << '\n';
    auto square = [](double v) -> double { return v * v; };
    std::cout << square(1.5) << ' ' << apply(twice, 21) << '\n';
    int threshold = 4;
    int longWords = 0;
    for (const auto& w : words)
        if ([threshold](const std::string& s) { return static_cast<int>(s.size()) > threshold; }(w)) ++longWords;
    std::cout << longWords << '\n';
    std::vector<int> nums = {1, 2, 3, 4};
    int total = 0;
    std::for_each(nums.begin(), nums.end(), [&total](int n) { total += n * n; });
    std::cout << total << '\n';
    return 0;
}
