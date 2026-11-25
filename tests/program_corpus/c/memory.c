/* malloc, realloc, free, puntatori, struct, qsort (LC-10, LC-12, LC-18) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[16];
    int age;
} Person;

typedef struct Node {
    int value;
    struct Node* next;
} Node;

int cmpInt(const void* a, const void* b) {
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}
int cmpAge(const void* a, const void* b) {
    const Person* p = a;
    const Person* q = b;
    return p->age - q->age;
}
Node* push(Node* head, int v) {
    Node* n = malloc(sizeof(Node));
    n->value = v;
    n->next = head;
    return n;
}

int main(void) {
    int cap = 2, n = 0;
    int* a = malloc(cap * sizeof(int));
    for (int i = 0; i < 7; i++) {
        if (n == cap) {
            cap *= 2;
            a = realloc(a, cap * sizeof(int));
        }
        a[n++] = (i * 37) % 11;
    }
    qsort(a, n, sizeof(int), cmpInt);
    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("(cap %d)\n", cap);
    free(a);
    Person people[3] = {{"Anna", 31}, {"Bruno", 25}, {"Carla", 40}};
    qsort(people, 3, sizeof(Person), cmpAge);
    for (int i = 0; i < 3; i++) printf("%s:%d ", people[i].name, people[i].age);
    printf("\n");
    Person* p = &people[1];
    p->age++;
    strcpy(p->name, "Annina");
    printf("%s %d\n", people[1].name, (*p).age);
    Node* list = NULL;
    for (int i = 1; i <= 4; i++) list = push(list, i * i);
    int sum = 0;
    for (Node* it = list; it; it = it->next) sum += it->value;
    printf("somma %d, testa %d\n", sum, list->value);
    while (list) {
        Node* next = list->next;
        free(list);
        list = next;
    }
    int* zeros = calloc(4, sizeof(int));
    printf("%d %d\n", zeros[0], zeros[3]);
    free(zeros);
    int m[3][4];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) m[r][c] = r * 10 + c;
    int* row = m[1];
    printf("%d %d %d\n", m[2][3], *(row + 2), (int)(sizeof m / sizeof m[0]));
    int x = 5;
    int* px = &x;
    int** ppx = &px;
    **ppx = 9;
    printf("%d\n", x);
    return 0;
}
