/* puntatori a funzione, enum, switch con fall-through, goto, static, macro (LC-07, LC-11, LC-13..LC-15) */
#include <stdio.h>
#include <math.h>

#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define N 5

typedef double (*Fn)(double);
typedef enum { ADD, SUB, MUL } Op;

double twice(double x) { return 2 * x; }
double half(double x) { return x / 2; }
int apply(Op op, int a, int b) {
    switch (op) {
    case ADD: return a + b;
    case SUB: return a - b;
    case MUL: return a * b;
    }
    return 0;
}
int counter(void) {
    static int calls = 0;
    return ++calls;
}
double integrate(Fn f, double a, double b, int steps) {
    double h = (b - a) / steps, s = 0;
    for (int i = 0; i < steps; i++) s += f(a + (i + 0.5) * h);
    return s * h;
}
double cube(double x) { return x * x * x; }

int main(void) {
    Fn table[] = {twice, half, sqrt};
    for (int i = 0; i < 3; i++) printf("%.3f ", table[i](16));
    printf("\n%d %d %d\n", apply(ADD, 2, 3), apply(SUB, 2, 3), apply(MUL, 2, 3));
    counter();
    counter();
    printf("%d\n", counter());
    printf("%d %d %d\n", SQUARE(3 + 1), MAX(4, 9), N * 2);
    printf("%.6f\n", integrate(cube, 0, 2, 1000));
    int month = 2, days = 0;
    switch (month) {
    case 1: days += 31;
    case 2: days += 28;
    case 3: days += 31; break;
    default: days = -1;
    }
    printf("%d\n", days);
    int i = 0;
again:
    i++;
    if (i < 4) goto again;
    printf("%d\n", i);
    double (*pf)(double) = &twice;
    printf("%.1f\n", (*pf)(1.25));
    printf("%.4f %.4f %.4f %.1f %.1f\n", pow(2, 0.5), fabs(-2.5), floor(-2.5), ceil(2.1), fmod(7.5, 2));
    return 0;
}
