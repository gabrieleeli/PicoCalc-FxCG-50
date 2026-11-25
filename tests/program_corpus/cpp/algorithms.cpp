// ricorsione, memoizzazione, crivello, ricerca binaria, riferimenti, puntatori (LK-04, LP-07)
#include <iostream>
#include <vector>
#include <algorithm>

long long fib(int n, std::vector<long long>& memo) {
    if (n < 2) return n;
    if (memo[n]) return memo[n];
    return memo[n] = fib(n - 1, memo) + fib(n - 2, memo);
}
int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
void swapRef(int& a, int& b) {
    int t = a;
    a = b;
    b = t;
}
void fill(int* p, int n, int v) {
    for (int i = 0; i < n; ++i) *(p + i) = v + i;
}
int binarySearch(const std::vector<int>& a, int key) {
    int lo = 0, hi = static_cast<int>(a.size()) - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}
int& at(std::vector<int>& v, int i) { return v[i]; }

int main() {
    std::vector<long long> memo(91, 0);
    std::cout << fib(90, memo) << '\n';
    std::cout << gcd(84, 36) << ' ' << gcd(17, 5) << '\n';
    int x = 1, y = 2;
    swapRef(x, y);
    std::cout << x << y << '\n';
    int arr[5];
    fill(arr, 5, 10);
    for (int i = 0; i < 5; ++i) std::cout << arr[i] << ' ';
    std::cout << '\n';
    const int N = 50;
    std::vector<bool> prime(N + 1, true);
    prime[0] = prime[1] = false;
    for (int i = 2; i * i <= N; ++i)
        if (prime[i])
            for (int j = i * i; j <= N; j += i) prime[j] = false;
    std::vector<int> primes;
    for (int i = 0; i <= N; ++i)
        if (prime[i]) primes.push_back(i);
    for (int p : primes) std::cout << p << ' ';
    std::cout << '\n' << binarySearch(primes, 37) << ' ' << binarySearch(primes, 38) << '\n';
    at(primes, 0) = 99;
    std::cout << primes[0] << '\n';
    int* p = arr;
    p += 2;
    std::cout << *p << ' ' << p[1] << ' ' << (p - arr) << '\n';
    int total = 0;
    for (int i = 1; i <= 100; ++i) {
        if (i % 3 == 0) continue;
        if (i > 20) break;
        total += i;
    }
    std::cout << total << '\n';
    return 0;
}
