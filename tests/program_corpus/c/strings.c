/* stringhe del C: string.h, ctype.h, sprintf, strtok (LC-09, LC-19, LC-20) */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void upper(char* s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}
int countVowels(const char* s) {
    int n = 0;
    for (int i = 0; s[i] != '\0'; i++)
        if (strchr("aeiouAEIOU", s[i])) n++;
    return n;
}

int main(void) {
    char buf[64];
    strcpy(buf, "Hello");
    strcat(buf, ", World");
    printf("%s %zu %d\n", buf, strlen(buf), countVowels(buf));
    upper(buf);
    printf("%s\n", buf);
    printf("%d %d %d\n", strcmp("abc", "abd") < 0, strcmp("b", "a") > 0, strncmp("abcx", "abcy", 3));
    char* p = strstr(buf, "WORLD");
    printf("%s %ld\n", p, (long)(p - buf));
    char line[] = "rosso,verde,,blu";
    int k = 0;
    for (char* tok = strtok(line, ","); tok; tok = strtok(NULL, ",")) printf("%d:%s ", k++, tok);
    printf("\n");
    char out[40];
    sprintf(out, "%s-%03d-%.1f", "ID", 7, 2.25);
    printf("%s\n", out);
    snprintf(out, 6, "%s", "troncata");
    printf("%s\n", out);
    int v = atoi("123abc");
    double d = atof("2.5");
    long l = strtol("ff", NULL, 16);
    printf("%d %.1f %ld\n", v, d, l);
    char word[] = "radar";
    int pal = 1;
    for (size_t i = 0, j = strlen(word) - 1; i < j; i++, j--)
        if (word[i] != word[j]) pal = 0;
    printf("%s palindromo: %d\n", word, pal);
    printf("%d%d%d%d\n", isdigit('5') != 0, isalpha('5') != 0, isspace(' ') != 0, tolower('Q') == 'q');
    char copy[10];
    memset(copy, 'z', 3);
    copy[3] = '\0';
    printf("%s %zu\n", copy, strlen(copy));
    const char* last = strrchr("a/b/c.txt", '/');
    printf("%s\n", last + 1);
    return 0;
}
