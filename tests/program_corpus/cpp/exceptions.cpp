// eccezioni: tipi standard, classe dell'utente, rilancio (LP-15)
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class InsufficientFunds : public std::runtime_error {
public:
    explicit InsufficientFunds(const std::string& m) : std::runtime_error(m) {}
};

double divide(double a, double b) {
    if (b == 0) throw std::invalid_argument("divisione per zero");
    return a / b;
}
void withdraw(double& balance, double amount) {
    if (amount > balance) throw InsufficientFunds("mancano " + std::to_string(static_cast<int>(amount - balance)));
    balance -= amount;
}

int main() {
    try {
        std::cout << divide(10, 4) << '\n';
        std::cout << divide(1, 0) << '\n';
    } catch (const std::invalid_argument& e) {
        std::cout << "errore: " << e.what() << '\n';
    }
    double balance = 50;
    try {
        withdraw(balance, 20);
        withdraw(balance, 100);
    } catch (const std::runtime_error& e) {
        std::cout << "runtime: " << e.what() << ' ' << balance << '\n';
    }
    std::vector<int> v(3);
    try {
        v.at(5) = 1;
    } catch (const std::exception& e) {
        std::cout << "eccezione standard\n";
    }
    try {
        std::stoi("abc");
    } catch (const std::invalid_argument&) {
        std::cout << "stoi: non e' un numero\n";
    }
    try {
        try {
            throw 42;
        } catch (int x) {
            std::cout << "intero " << x << '\n';
            throw;
        }
    } catch (...) {
        std::cout << "rilanciata\n";
    }
    try {
        throw std::string("stringa");
    } catch (const std::string& s) {
        std::cout << s << '\n';
    }
    return 0;
}
