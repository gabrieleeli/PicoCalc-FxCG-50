/* matrici 1.0.0 - vedi matrici.h */
#include <stdio.h>
#include <math.h>
#include "matrici.h"

Matrice mat_nuova(int righe, int colonne) {
    Matrice m;
    m.righe = righe;
    m.colonne = colonne;
    for (int r = 0; r < MAT_MAX; r++)
        for (int c = 0; c < MAT_MAX; c++) m.v[r][c] = 0.0;
    return m;
}

Matrice mat_identita(int n) {
    Matrice m = mat_nuova(n, n);
    for (int i = 0; i < n; i++) m.v[i][i] = 1.0;
    return m;
}

Matrice mat_da_array(int righe, int colonne, const double* valori) {
    Matrice m = mat_nuova(righe, colonne);
    for (int r = 0; r < righe; r++)
        for (int c = 0; c < colonne; c++) m.v[r][c] = valori[r * colonne + c];
    return m;
}

Matrice mat_somma(Matrice a, Matrice b) {
    Matrice m = mat_nuova(a.righe, a.colonne);
    for (int r = 0; r < a.righe; r++)
        for (int c = 0; c < a.colonne; c++) m.v[r][c] = a.v[r][c] + b.v[r][c];
    return m;
}

Matrice mat_prodotto(Matrice a, Matrice b) {
    Matrice m = mat_nuova(a.righe, b.colonne);
    for (int r = 0; r < a.righe; r++)
        for (int c = 0; c < b.colonne; c++) {
            double s = 0.0;
            for (int k = 0; k < a.colonne; k++) s += a.v[r][k] * b.v[k][c];
            m.v[r][c] = s;
        }
    return m;
}

Matrice mat_trasposta(Matrice a) {
    Matrice m = mat_nuova(a.colonne, a.righe);
    for (int r = 0; r < a.righe; r++)
        for (int c = 0; c < a.colonne; c++) m.v[c][r] = a.v[r][c];
    return m;
}

Matrice mat_per_scalare(Matrice a, double k) {
    for (int r = 0; r < a.righe; r++)
        for (int c = 0; c < a.colonne; c++) a.v[r][c] *= k;
    return a;
}

double mat_determinante(Matrice a) {
    int n = a.righe;
    double det = 1.0;
    for (int k = 0; k < n; k++) {
        int p = k;
        for (int r = k + 1; r < n; r++)
            if (fabs(a.v[r][k]) > fabs(a.v[p][k])) p = r;
        if (fabs(a.v[p][k]) < 1e-12) return 0.0;
        if (p != k) {
            for (int c = 0; c < n; c++) {
                double t = a.v[k][c];
                a.v[k][c] = a.v[p][c];
                a.v[p][c] = t;
            }
            det = -det;
        }
        det *= a.v[k][k];
        for (int r = k + 1; r < n; r++) {
            double f = a.v[r][k] / a.v[k][k];
            for (int c = k; c < n; c++) a.v[r][c] -= f * a.v[k][c];
        }
    }
    return det;
}

int mat_inversa(Matrice a, Matrice* inversa) {
    int n = a.righe;
    Matrice b = mat_identita(n);
    for (int k = 0; k < n; k++) {
        int p = k;
        for (int r = k + 1; r < n; r++)
            if (fabs(a.v[r][k]) > fabs(a.v[p][k])) p = r;
        if (fabs(a.v[p][k]) < 1e-12) return 0;
        for (int c = 0; c < n; c++) {
            double t = a.v[k][c]; a.v[k][c] = a.v[p][c]; a.v[p][c] = t;
            t = b.v[k][c]; b.v[k][c] = b.v[p][c]; b.v[p][c] = t;
        }
        double d = a.v[k][k];
        for (int c = 0; c < n; c++) { a.v[k][c] /= d; b.v[k][c] /= d; }
        for (int r = 0; r < n; r++) {
            if (r == k) continue;
            double f = a.v[r][k];
            for (int c = 0; c < n; c++) { a.v[r][c] -= f * a.v[k][c]; b.v[r][c] -= f * b.v[k][c]; }
        }
    }
    *inversa = b;
    return 1;
}

void mat_stampa(Matrice a) {
    for (int r = 0; r < a.righe; r++) {
        for (int c = 0; c < a.colonne; c++) printf("%9.4g", a.v[r][c]);
        printf("\n");
    }
}
