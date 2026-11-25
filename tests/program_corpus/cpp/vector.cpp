// std::vector e algoritmi (LP-13, LP-14)
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

int main() {
    std::vector<int> v = {5, 3, 8, 1, 9, 2};
    v.push_back(7);
    std::sort(v.begin(), v.end());
    for (int x : v) std::cout << x << ' ';
    std::cout << '\n';
    std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
    for (std::size_t i = 0; i < v.size(); ++i) std::cout << v[i] << (i + 1 < v.size() ? "," : "\n");
    std::cout << std::accumulate(v.begin(), v.end(), 0) << ' ' << *std::max_element(v.begin(), v.end()) << ' '
              << *std::min_element(v.begin(), v.end()) << '\n';
    v.erase(v.begin() + 1);
    v.insert(v.begin(), 100);
    std::cout << v.front() << ' ' << v.back() << ' ' << v.size() << '\n';
    auto it = std::find(v.begin(), v.end(), 3);
    std::cout << (it != v.end()) << ' ' << (it - v.begin()) << ' ' << std::count(v.begin(), v.end(), 5) << '\n';
    std::vector<std::vector<int>> m(3, std::vector<int>(4, 0));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j) m[i][j] = i * j;
    for (const auto& row : m) {
        for (int x : row) std::cout << x << ' ';
        std::cout << '\n';
    }
    std::vector<double> d(5, 1.5);
    std::fill(d.begin() + 2, d.end(), 0.25);
    double sum = 0;
    for (double x : d) sum += x;
    std::cout << sum << ' ' << std::accumulate(d.begin(), d.end(), 0.0) << '\n';
    v.pop_back();
    v.resize(3);
    std::cout << v.size() << ' ' << v[2] << ' ' << v.at(0) << '\n';
    std::reverse(v.begin(), v.end());
    for (auto x : v) std::cout << x << ' ';
    std::cout << '\n';
    std::vector<int> w(v);
    w.clear();
    std::cout << w.empty() << ' ' << v.empty() << '\n';
    std::vector<int> e;
    e.reserve(10);
    for (int i = 0; i < 5; ++i) e.emplace_back(i * i);
    std::swap(e[0], e[4]);
    std::cout << e[0] << e[4] << ' ' << std::min(3, 9) << std::max(3, 9) << '\n';
    try {
        std::cout << e.at(10) << '\n';
    } catch (const std::out_of_range&) {
        std::cout << "fuori dai limiti\n";
    }
    std::vector<int> s2 = {3, 1, 2, 1};
    std::stable_sort(s2.begin(), s2.end());
    for (auto x : s2) std::cout << x;
    std::cout << '\n';
    return 0;
}
