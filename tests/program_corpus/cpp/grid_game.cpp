// gioco su griglia: vector<string>, struct, movimenti da input, ricerca in ampiezza
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <utility>

struct Pos {
    int r, c;
};

int shortest(const std::vector<std::string>& g, Pos s, Pos t) {
    const int R = static_cast<int>(g.size()), C = static_cast<int>(g[0].size());
    std::vector<std::vector<int>> dist(R, std::vector<int>(C, -1));
    std::queue<std::pair<int, int>> q;
    q.push({s.r, s.c});
    dist[s.r][s.c] = 0;
    const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();
        if (r == t.r && c == t.c) return dist[r][c];
        for (int k = 0; k < 4; ++k) {
            const int nr = r + dr[k], nc = c + dc[k];
            if (nr < 0 || nc < 0 || nr >= R || nc >= C || g[nr][nc] == '#' || dist[nr][nc] >= 0) continue;
            dist[nr][nc] = dist[r][c] + 1;
            q.push({nr, nc});
        }
    }
    return -1;
}

int main() {
    std::vector<std::string> grid = {"S...#....", ".##.#.##.", ".#..#..#.", ".#.##.#..", "...#..#.T"};
    Pos start{0, 0}, target{4, 8}, me = start;
    std::cout << "percorso minimo: " << shortest(grid, start, target) << '\n';
    char m;
    int moves = 0, bumps = 0;
    while (std::cin >> m) {
        Pos n = me;
        if (m == 'w') --n.r;
        else if (m == 's') ++n.r;
        else if (m == 'a') --n.c;
        else if (m == 'd') ++n.c;
        else continue;
        ++moves;
        if (n.r < 0 || n.c < 0 || n.r >= static_cast<int>(grid.size()) || n.c >= static_cast<int>(grid[0].size()) || grid[n.r][n.c] == '#') {
            ++bumps;
            continue;
        }
        me = n;
        if (me.r == target.r && me.c == target.c) break;
    }
    grid[me.r][me.c] = '@';
    for (const auto& row : grid) std::cout << row << '\n';
    std::cout << moves << " mosse, " << bumps << " urti, " << (me.r == target.r && me.c == target.c ? "arrivato" : "non arrivato") << '\n';
    return 0;
}
