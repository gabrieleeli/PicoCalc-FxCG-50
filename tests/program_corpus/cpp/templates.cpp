// template di funzione e di classe (LP-09)
#include <iostream>
#include <string>

template <typename T>
T maxOf(T a, T b) { return a > b ? a : b; }

template <typename T, int N>
struct Stack {
    T data[N];
    int top = 0;
    bool push(const T& v) {
        if (top == N) return false;
        data[top++] = v;
        return true;
    }
    T pop() { return data[--top]; }
    bool empty() const { return top == 0; }
};

template <typename T>
class Pair {
public:
    T a, b;
    Pair(T a, T b) : a(a), b(b) {}
    T sum() const { return a + b; }
};

template <class T>
void printAll(const T* items, int n) {
    for (int i = 0; i < n; ++i) std::cout << items[i] << (i + 1 < n ? " " : "\n");
}

int main() {
    std::cout << maxOf(3, 7) << ' ' << maxOf(2.5, 1.5) << ' ' << maxOf<std::string>("pera", "mela") << '\n';
    Stack<int, 3> s;
    for (int i = 1; i <= 4; ++i) std::cout << s.push(i * 10);
    std::cout << '\n';
    while (!s.empty()) std::cout << s.pop() << ' ';
    std::cout << '\n';
    Pair<double> p(1.25, 2.5);
    Pair<std::string> q("ab", "cd");
    std::cout << p.sum() << ' ' << q.sum() << '\n';
    int nums[] = {4, 5, 6};
    printAll(nums, 3);
    std::string words[] = {"a", "bb"};
    printAll(words, 2);
    return 0;
}
