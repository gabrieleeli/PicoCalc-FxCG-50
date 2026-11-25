// gioco a turni deterministico: generatore pseudo-casuale proprio (LCG), classi, input
#include <iostream>
#include <string>

class Rng {
    unsigned long long state;
public:
    explicit Rng(unsigned long long seed) : state(seed) {}
    int next(int n) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state >> 33) % static_cast<unsigned long long>(n));
    }
};

struct Fighter {
    std::string name;
    int hp, attack;
    bool alive() const { return hp > 0; }
};

int main() {
    Rng rng(2024);
    Fighter hero{"Eroe", 30, 6};
    Fighter orc{"Orco", 25, 5};
    std::string move;
    int turn = 1;
    while (hero.alive() && orc.alive() && std::cin >> move) {
        int dmg = 0;
        if (move == "a") dmg = hero.attack + rng.next(4);
        else if (move == "c") {
            hero.hp += 4;
            std::cout << "turno " << turn << ": " << hero.name << " si cura (" << hero.hp << ")\n";
        } else {
            std::cout << "mossa sconosciuta: " << move << '\n';
            continue;
        }
        if (dmg) {
            orc.hp -= dmg;
            std::cout << "turno " << turn << ": " << hero.name << " colpisce per " << dmg << " (" << orc.name << " " << orc.hp << ")\n";
        }
        if (orc.alive()) {
            int back = orc.attack + rng.next(3);
            hero.hp -= back;
            std::cout << "         " << orc.name << " risponde per " << back << " (" << hero.name << " " << hero.hp << ")\n";
        }
        ++turn;
    }
    std::cout << (orc.alive() ? (hero.alive() ? "pareggio" : "vince l'orco") : "vince l'eroe") << '\n';
    return 0;
}
