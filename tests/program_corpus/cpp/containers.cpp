// stack, queue, deque, list, unordered_map (LP-13)
#include <iostream>
#include <stack>
#include <queue>
#include <deque>
#include <list>
#include <string>

bool balanced(const std::string& s) {
    std::stack<char> st;
    for (char c : s) {
        if (c == '(' || c == '[') st.push(c);
        else if (c == ')' || c == ']') {
            if (st.empty()) return false;
            char o = st.top();
            st.pop();
            if ((c == ')' && o != '(') || (c == ']' && o != '[')) return false;
        }
    }
    return st.empty();
}

int main() {
    std::cout << balanced("(a[b]c)") << balanced("(]") << balanced("((") << '\n';
    std::queue<int> q;
    for (int i = 1; i <= 3; ++i) q.push(i * 2);
    std::cout << q.front() << q.back() << q.size() << ' ';
    q.pop();
    std::cout << q.front() << '\n';
    std::deque<int> d;
    d.push_back(1);
    d.push_front(0);
    d.push_back(2);
    std::cout << d.front() << d.back() << d[1] << d.size() << ' ';
    d.pop_front();
    std::cout << d.front() << '\n';
    std::list<std::string> l = {"b", "c"};
    l.push_front("a");
    l.push_back("d");
    for (const auto& s : l) std::cout << s;
    std::cout << ' ' << l.size() << ' ' << l.front() << l.back() << '\n';
    std::stack<int> s2;
    for (int i = 0; i < 4; ++i) s2.push(i);
    while (!s2.empty()) {
        std::cout << s2.top();
        s2.pop();
    }
    std::cout << '\n';
    return 0;
}
