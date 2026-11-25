// file nella cartella del programma: ofstream, ifstream, CSV con getline (LP-16)
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    {
        std::ofstream out("voti.csv");
        out << "nome,voto\n";
        out << "Anna,8\nBruno,6\nCarla,9\nDario,7\n";
    }
    std::ifstream in("voti.csv");
    if (!in.is_open()) {
        std::cout << "impossibile aprire\n";
        return 1;
    }
    std::string line;
    std::getline(in, line);
    std::vector<int> votes;
    std::string best;
    int top = -1;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string name, vote;
        std::getline(ss, name, ',');
        std::getline(ss, vote, ',');
        int v = std::stoi(vote);
        votes.push_back(v);
        if (v > top) {
            top = v;
            best = name;
        }
    }
    in.close();
    double sum = 0;
    for (int v : votes) sum += v;
    std::cout << votes.size() << " voti, media " << sum / votes.size() << ", migliore " << best << '\n';
    std::ofstream app("voti.csv", std::ios::app);
    app << "Elena,10\n";
    app.close();
    std::ifstream again("voti.csv");
    int count = 0;
    std::string word;
    while (again >> word) ++count;
    std::cout << count << " parole\n";
    std::ifstream missing("non_esiste.txt");
    std::cout << missing.is_open() << ' ' << missing.fail() << '\n';
    std::ofstream nout("numeri.txt");
    nout << 3 << ' ' << 4.5 << ' ' << -2 << '\n';
    nout.close();
    std::ifstream nin("numeri.txt");
    double a, b, c;
    nin >> a >> b >> c;
    std::cout << a + b + c << '\n';
    return 0;
}
