// ricorsione: N regine, sottoinsiemi, memoizzazione con map, torre di Hanoi
#include <iostream>
#include <map>
#include <string>
#include <vector>

int queens(int row, int n, std::vector<int>& cols) {
    if (row == n) return 1;
    int count = 0;
    for (int c = 0; c < n; ++c) {
        bool ok = true;
        for (int r = 0; r < row; ++r)
            if (cols[r] == c || cols[r] - c == r - row || cols[r] - c == row - r) ok = false;
        if (!ok) continue;
        cols[row] = c;
        count += queens(row + 1, n, cols);
    }
    return count;
}
void subsets(const std::string& s, std::size_t i, std::string cur, std::vector<std::string>& out) {
    if (i == s.size()) {
        out.push_back("{" + cur + "}");
        return;
    }
    subsets(s, i + 1, cur, out);
    subsets(s, i + 1, cur + s[i], out);
}
std::map<long long, long long> memo;
long long collatzLen(long long n) {
    if (n == 1) return 1;
    auto it = memo.find(n);
    if (it != memo.end()) return it->second;
    const long long r = 1 + collatzLen(n % 2 ? 3 * n + 1 : n / 2);
    memo[n] = r;
    return r;
}
int hanoi(int n, char a, char b, char c, std::string& first) {
    if (n == 0) return 0;
    int m = hanoi(n - 1, a, c, b, first);
    if (first.empty()) first = std::string(1, a) + "->" + b;
    return m + 1 + hanoi(n - 1, c, b, a, first);
}

int main() {
    for (int n = 4; n <= 7; ++n) {
        std::vector<int> cols(n);
        std::cout << n << " regine: " << queens(0, n, cols) << '\n';
    }
    std::vector<std::string> out;
    subsets("abc", 0, "", out);
    for (const auto& s : out) std::cout << s << ' ';
    std::cout << '\n';
    long long best = 0, arg = 0;
    for (long long n = 1; n < 3000; ++n) {
        const long long l = collatzLen(n);
        if (l > best) {
            best = l;
            arg = n;
        }
    }
    std::cout << "Collatz: " << arg << " con " << best << " termini\n";
    std::string first;
    std::cout << "Hanoi(10): " << hanoi(10, 'A', 'C', 'B', first) << " mosse, prima " << first << '\n';
    return 0;
}
