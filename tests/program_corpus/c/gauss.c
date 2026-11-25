/* sistema lineare con l'eliminazione di Gauss (pivot parziale), matrici come parametri */
#include <stdio.h>
#include <math.h>

#define N 3

int solve(double a[N][N + 1], double x[N]) {
    for (int c = 0; c < N; c++) {
        int p = c;
        for (int r = c + 1; r < N; r++)
            if (fabs(a[r][c]) > fabs(a[p][c])) p = r;
        if (fabs(a[p][c]) < 1e-12) return 0;
        if (p != c)
            for (int k = 0; k <= N; k++) {
                double t = a[c][k];
                a[c][k] = a[p][k];
                a[p][k] = t;
            }
        for (int r = c + 1; r < N; r++) {
            double f = a[r][c] / a[c][c];
            for (int k = c; k <= N; k++) a[r][k] -= f * a[c][k];
        }
    }
    for (int r = N - 1; r >= 0; r--) {
        double s = a[r][N];
        for (int k = r + 1; k < N; k++) s -= a[r][k] * x[k];
        x[r] = s / a[r][r];
    }
    return 1;
}

void print(double a[][N + 1], int rows) {
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c <= N; c++) printf("%8.3f", a[r][c]);
        printf("\n");
    }
}

int main(void) {
    double a[N][N + 1] = {{2, 1, -1, 8}, {-3, -1, 2, -11}, {-2, 1, 2, -3}};
    double x[N];
    print(a, N);
    if (solve(a, x)) printf("x = %.4f, y = %.4f, z = %.4f\n", x[0], x[1], x[2]);
    double s[N][N + 1] = {{1, 2, 3, 1}, {2, 4, 6, 2}, {1, 1, 1, 0}};
    printf("%s\n", solve(s, x) ? "soluzione unica" : "sistema singolare");
    return 0;
}
