// classi, costruttori, operatori, membri statici (LP-02, LP-03, LP-06)
#include <iostream>
#include <cmath>

class Vec2 {
public:
    double x, y;
    Vec2() : x(0), y(0) {}
    Vec2(double x_, double y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator*(double k) const { return Vec2(x * k, y * k); }
    Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    double length() const { return std::sqrt(x * x + y * y); }
    friend std::ostream& operator<<(std::ostream& os, const Vec2& v) { return os << "(" << v.x << ", " << v.y << ")"; }
};

class Counter {
    static int created;
    int id;
public:
    Counter() : id(++created) {}
    int getId() const { return id; }
    static int total() { return created; }
};
int Counter::created = 0;

class Account {
    double balance;
public:
    explicit Account(double initial = 0) : balance(initial) {}
    bool withdraw(double amount);
    void deposit(double amount) { balance += amount; }
    double get() const { return balance; }
};
bool Account::withdraw(double amount) {
    if (amount > balance) return false;
    balance -= amount;
    return true;
}

int main() {
    Vec2 a(3, 4), b(1, 2);
    Vec2 c = a + b * 2;
    std::cout << c << ' ' << a.length() << '\n';
    c += a;
    std::cout << c << ' ' << (c == Vec2(8, 12)) << '\n';
    Counter k1, k2, k3;
    std::cout << k2.getId() << ' ' << Counter::total() << ' ' << k3.getId() - k1.getId() << '\n';
    Vec2 arr[3];
    arr[1] = Vec2(1, 1);
    std::cout << arr[0] << arr[1] << '\n';
    Account acc(100);
    acc.deposit(50);
    std::cout << acc.withdraw(200) << acc.withdraw(30) << ' ' << acc.get() << '\n';
    Account empty;
    std::cout << empty.get() << '\n';
    return 0;
}
