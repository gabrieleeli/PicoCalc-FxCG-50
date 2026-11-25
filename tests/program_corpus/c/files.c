/* file confinati nella cartella del programma (LC-17) */
#include <stdio.h>
#include <string.h>

int main(void) {
    FILE* f = fopen("misure.txt", "w");
    if (!f) {
        printf("errore\n");
        return 1;
    }
    for (int i = 1; i <= 5; i++) fprintf(f, "%d %.1f\n", i, i * 1.5);
    fclose(f);
    f = fopen("misure.txt", "r");
    int id;
    double v, sum = 0;
    int count = 0;
    while (fscanf(f, "%d %lf", &id, &v) == 2) {
        sum += v;
        count++;
    }
    fclose(f);
    printf("%d valori, somma %.2f\n", count, sum);
    f = fopen("misure.txt", "a");
    fputs("6 9.0\n", f);
    fclose(f);
    f = fopen("misure.txt", "r");
    char line[32];
    int lines = 0;
    while (fgets(line, sizeof line, f)) lines++;
    printf("%d righe, feof %d\n", lines, feof(f) != 0);
    fclose(f);
    FILE* g = fopen("nessuno.txt", "r");
    printf("%s\n", g == NULL ? "file assente" : "trovato");
    f = fopen("lettere.txt", "w");
    for (char c = 'a'; c <= 'e'; c++) fputc(c, f);
    fclose(f);
    f = fopen("lettere.txt", "r");
    int ch, n = 0;
    while ((ch = fgetc(f)) != EOF) n += ch - 'a';
    fclose(f);
    printf("%d\n", n);
    int first = remove("lettere.txt");
    int second = remove("lettere.txt") != 0;
    printf("%d %d\n", first, second);
    return 0;
}
