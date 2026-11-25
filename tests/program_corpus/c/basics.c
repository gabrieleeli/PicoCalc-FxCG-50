/* C: tipi, printf con i formati, aritmetica, cicli, funzioni (LC-01..LC-07, LC-16) */
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>

static int square(int x) { return x * x; }
double average(const int* a, int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    return n ? (double)sum / n : 0.0;
}

int main(void) {
    int a[] = {3, 7, 1, 9, 4};
    int n = sizeof a / sizeof a[0];
    printf("n=%d media=%.3f quadrato=%d\n", n, average(a, n), square(12));
    printf("[%5d] [%-5d] [%05d] [%+d] [%x] [%X] [%o] [%#x]\n", 42, 42, 42, 42, 255, 255, 8, 255);
    printf("[%8.3f] [%-8.2f] [%e] [%g] [%g] [%g]\n", 3.14159, 2.5, 12345.678, 0.0001, 1e-5, 123456789.0);
    printf("[%c] [%s] [%10s] [%-6s|] [%.3s] [%%]\n", 'A', "ciao", "destra", "sx", "abcdef");
    printf("%ld %lld %u %lu %zu\n", 123456789L, 1LL << 40, 4000000000u, 5UL, sizeof(double));
    printf("%d %d %d %d\n", 7 / 2, -7 / 2, 7 % 3, -7 % 3);
    printf("%d %d\n", INT_MAX, INT_MIN);
    unsigned char uc = 250;
    uc += 10;
    char c = 'a' + 2;
    printf("%d %c %d\n", uc, c, c);
    bool ok = 3 > 2;
    printf("%d %d\n", ok, !ok);
    int i = 0, total = 0;
    while (1) {
        if (++i > 10) break;
        if (i % 2) continue;
        total += i;
    }
    printf("%d\n", total);
    for (int r = 1; r <= 3; r++) {
        for (int k = 1; k <= r; k++) printf("*");
        printf("\n");
    }
    double x = 10;
    x /= 4;
    printf("%.2f %d %.1f\n", x, (int)x, (double)(int)(x * 10) / 10);
    printf("%*d|%-*d|%.*f\n", 6, 7, 4, 8, 2, 3.14159);
    return 0;
}
