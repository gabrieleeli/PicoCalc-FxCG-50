/* scanf fino alla fine dell'input, fgets, sscanf (LC-16) */
#include <stdio.h>
#include <string.h>

int main(void) {
    int n;
    if (scanf("%d", &n) != 1) return 1;
    double sum = 0, x, mx = -1e300;
    for (int i = 0; i < n && scanf("%lf", &x) == 1; i++) {
        sum += x;
        if (x > mx) mx = x;
    }
    printf("somma %.2f massimo %.2f\n", sum, mx);
    char name[32];
    int age;
    scanf("%31s %d", name, &age);
    printf("%s ha %d anni\n", name, age);
    char line[80];
    fgets(line, sizeof line, stdin);
    int lines = 0, total = 0;
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = '\0';
        int a, b;
        char op;
        if (sscanf(line, "%d %c %d", &a, &op, &b) == 3) {
            int r = op == '+' ? a + b : op == '-' ? a - b : op == '*' ? a * b : 0;
            printf("%s = %d\n", line, r);
            total += r;
        } else {
            printf("ignoro '%s'\n", line);
        }
        lines++;
    }
    printf("%d righe, totale %d\n", lines, total);
    return 0;
}
