/* statistica 1.0.0 - vedi statistica.h */
#include <math.h>
#include <stdlib.h>
#include "statistica.h"

double stat_somma(const double* x, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++) s += x[i];
    return s;
}

double stat_media(const double* x, int n) {
    return n > 0 ? stat_somma(x, n) / n : 0.0;
}

static double scarti2(const double* x, int n) {
    double m = stat_media(x, n), s = 0.0;
    for (int i = 0; i < n; i++) s += (x[i] - m) * (x[i] - m);
    return s;
}

double stat_dev_pop(const double* x, int n) {
    return n > 0 ? sqrt(scarti2(x, n) / n) : 0.0;
}

double stat_dev_camp(const double* x, int n) {
    return n > 1 ? sqrt(scarti2(x, n) / (n - 1)) : 0.0;
}

static int confronta(const void* a, const void* b) {
    double d = *(const double*)a - *(const double*)b;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

double stat_mediana(const double* x, int n) {
    if (n <= 0) return 0.0;
    double* c = malloc(n * sizeof(double));
    if (!c) return 0.0;
    for (int i = 0; i < n; i++) c[i] = x[i];
    qsort(c, n, sizeof(double), confronta);
    double m = n % 2 ? c[n / 2] : (c[n / 2 - 1] + c[n / 2]) / 2.0;
    free(c);
    return m;
}

double stat_minimo(const double* x, int n) {
    double m = x[0];
    for (int i = 1; i < n; i++) if (x[i] < m) m = x[i];
    return m;
}

double stat_massimo(const double* x, int n) {
    double m = x[0];
    for (int i = 1; i < n; i++) if (x[i] > m) m = x[i];
    return m;
}

void stat_regressione(const double* x, const double* y, int n, double* a, double* b, double* r) {
    double mx = stat_media(x, n), my = stat_media(y, n);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (int i = 0; i < n; i++) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    *b = sxx != 0.0 ? sxy / sxx : 0.0;
    *a = my - *b * mx;
    *r = (sxx > 0.0 && syy > 0.0) ? sxy / sqrt(sxx * syy) : 0.0;
}
