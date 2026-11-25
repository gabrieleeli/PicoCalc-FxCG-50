/* statistiche di una lista: media, varianza, mediana, istogramma */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int cmp(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

int main(void) {
    double v[] = {4.5, 7.0, 3.25, 9.5, 6.0, 7.0, 2.75, 8.25};
    int n = sizeof v / sizeof v[0];
    double sum = 0, sq = 0;
    for (int i = 0; i < n; i++) {
        sum += v[i];
        sq += v[i] * v[i];
    }
    double mean = sum / n;
    double var = sq / n - mean * mean;
    double s = 0;
    for (int i = 0; i < n; i++) s += (v[i] - mean) * (v[i] - mean);
    printf("n=%d media=%.4f varianza=%.4f sigma=%.4f s=%.4f\n", n, mean, var, sqrt(var), sqrt(s / (n - 1)));
    qsort(v, n, sizeof(double), cmp);
    double median = n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
    printf("min=%.2f max=%.2f mediana=%.3f\n", v[0], v[n - 1], median);
    int bins[4] = {0};
    for (int i = 0; i < n; i++) {
        int b = (int)((v[i] - 2.0) / 2.0);
        if (b > 3) b = 3;
        bins[b]++;
    }
    for (int b = 0; b < 4; b++) {
        printf("[%4.1f, %4.1f) ", 2.0 + 2 * b, 4.0 + 2 * b);
        for (int k = 0; k < bins[b]; k++) putchar('#');
        putchar('\n');
    }
    return 0;
}
