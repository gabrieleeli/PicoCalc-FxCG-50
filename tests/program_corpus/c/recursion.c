/* ricorsione: torre di Hanoi, permutazioni, potenza veloce, struct per valore */
#include <stdio.h>

typedef struct {
    double re, im;
} Complex;

Complex mul(Complex a, Complex b) {
    Complex r = {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
    return r;
}
int moves = 0;
void hanoi(int n, char from, char to, char via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);
    moves++;
    if (n == 3) printf("disco 3: %c -> %c\n", from, to);
    hanoi(n - 1, via, to, from);
}
void permute(char* s, int k, int n) {
    if (k == n) {
        printf("%s ", s);
        return;
    }
    for (int i = k; i < n; i++) {
        char t = s[k]; s[k] = s[i]; s[i] = t;
        permute(s, k + 1, n);
        t = s[k]; s[k] = s[i]; s[i] = t;
    }
}
long long power(long long b, int e) {
    if (e == 0) return 1;
    long long h = power(b, e / 2);
    return e % 2 ? h * h * b : h * h;
}
int ackermann(int m, int n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

int main(void) {
    hanoi(4, 'A', 'C', 'B');
    printf("%d mosse\n", moves);
    char s[] = "abc";
    permute(s, 0, 3);
    printf("\n%lld %lld\n", power(3, 13), power(2, 62));
    printf("%d\n", ackermann(2, 3));
    Complex z = {1, 1}, w = z;
    for (int i = 0; i < 3; i++) w = mul(w, z);
    printf("%.1f%+.1fi  z=%.1f%+.1fi\n", w.re, w.im, z.re, z.im);
    return 0;
}
