/* equazioni di secondo grado ax^2 + bx + c = 0 lette dall'input (anche complesse) */
#include <stdio.h>
#include <math.h>

void solve(double a, double b, double c) {
    printf("%gx^2 %+gx %+g = 0: ", a, b, c);
    if (a == 0) {
        if (b == 0) printf(c == 0 ? "infinite soluzioni\n" : "impossibile\n");
        else printf("lineare, x = %.4f\n", -c / b);
        return;
    }
    double d = b * b - 4 * a * c;
    if (d > 0) {
        double s = sqrt(d);
        printf("x1 = %.4f, x2 = %.4f\n", (-b - s) / (2 * a), (-b + s) / (2 * a));
    } else if (d == 0) {
        printf("x = %.4f (doppia)\n", -b / (2 * a));
    } else {
        double re = -b / (2 * a), im = sqrt(-d) / (2 * fabs(a));
        printf("x = %.4f +- %.4fi\n", re, im);
    }
}

int main(void) {
    double a, b, c;
    int n = 0;
    while (scanf("%lf %lf %lf", &a, &b, &c) == 3) {
        solve(a, b, c);
        n++;
    }
    printf("%d equazioni\n", n);
    return 0;
}
