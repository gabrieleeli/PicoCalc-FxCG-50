// conti correnti: classi, map, eccezioni, operatori di confronto e stampa
#include <iostream>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

class Account {
    std::string owner_;
    double balance_ = 0;
    std::vector<std::string> log_;
public:
    Account() = default;
    explicit Account(const std::string& owner) : owner_(owner) {}
    const std::string& owner() const { return owner_; }
    double balance() const { return balance_; }
    void deposit(double v) {
        if (v <= 0) throw std::invalid_argument("importo non valido");
        balance_ += v;
        log_.push_back("+" + std::to_string(static_cast<int>(v)));
    }
    void withdraw(double v) {
        if (v > balance_) throw std::runtime_error(owner_ + ": fondi insufficienti");
        balance_ -= v;
        log_.push_back("-" + std::to_string(static_cast<int>(v)));
    }
    std::size_t operations() const { return log_.size(); }
    bool operator<(const Account& o) const { return balance_ < o.balance_; }
    friend std::ostream& operator<<(std::ostream& os, const Account& a) {
        return os << std::left << std::setw(8) << a.owner_ << std::right << std::setw(10) << std::fixed << std::setprecision(2) << a.balance_;
    }
};

class Bank {
    std::map<std::string, Account> accounts_;
public:
    Account& open(const std::string& name) {
        if (accounts_.count(name)) throw std::logic_error("conto gia' aperto: " + name);
        accounts_[name] = Account(name);
        return accounts_[name];
    }
    Account& get(const std::string& name) {
        auto it = accounts_.find(name);
        if (it == accounts_.end()) throw std::out_of_range("conto inesistente: " + name);
        return it->second;
    }
    void transfer(const std::string& from, const std::string& to, double v) {
        Account& a = get(from);
        Account& b = get(to);
        a.withdraw(v);
        b.deposit(v);
    }
    std::vector<Account> ranking() const {
        std::vector<Account> v;
        for (const auto& p : accounts_) v.push_back(p.second);
        std::sort(v.begin(), v.end());
        std::reverse(v.begin(), v.end());
        return v;
    }
};

int main() {
    Bank bank;
    bank.open("Anna").deposit(500);
    bank.open("Bruno").deposit(120);
    bank.open("Carla");
    const char* ops[][3] = {{"Anna", "Carla", "200"}, {"Bruno", "Anna", "300"}, {"Carla", "Dario", "10"}, {"Carla", "Bruno", "50"}};
    for (const auto& op : ops) {
        try {
            bank.transfer(op[0], op[1], std::stod(op[2]));
            std::cout << "ok " << op[0] << " -> " << op[1] << '\n';
        } catch (const std::runtime_error& e) {
            std::cout << "rifiutato: " << e.what() << '\n';
        } catch (const std::exception& e) {
            std::cout << "errore: " << e.what() << '\n';
        }
    }
    try {
        bank.open("Anna");
    } catch (const std::logic_error& e) {
        std::cout << e.what() << '\n';
    }
    for (const Account& a : bank.ranking()) std::cout << a << " (" << a.operations() << ")\n";
    return 0;
}
