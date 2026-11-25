/* struct annidate, array di struct, puntatori a struct, enum */
#include <stdio.h>
#include <string.h>

typedef enum { FOOD, TOOL, BOOK } Category;
typedef struct {
    int day, month, year;
} Date;
typedef struct {
    char name[20];
    Category cat;
    double price;
    int qty;
    Date added;
} Item;

const char* catName(Category c) {
    switch (c) {
    case FOOD: return "cibo";
    case TOOL: return "attrezzo";
    default: return "libro";
    }
}
void restock(Item* it, int n) { it->qty += n; }
double total(const Item* items, int n) {
    double t = 0;
    for (const Item* p = items; p < items + n; p++) t += p->price * p->qty;
    return t;
}
Item* cheapest(Item items[], int n) {
    Item* best = &items[0];
    for (int i = 1; i < n; i++)
        if (items[i].price < best->price) best = &items[i];
    return best;
}

int main(void) {
    Item shop[] = {
        {"mele", FOOD, 0.45, 100, {3, 5, 2024}},
        {"martello", TOOL, 12.9, 4, {12, 1, 2023}},
        {"manuale", BOOK, 25.0, 2, {28, 2, 2024}},
    };
    int n = sizeof shop / sizeof shop[0];
    restock(&shop[1], 6);
    for (int i = 0; i < n; i++)
        printf("%-10s %-9s %6.2f x %3d  %02d/%02d/%d\n", shop[i].name, catName(shop[i].cat), shop[i].price, shop[i].qty,
               shop[i].added.day, shop[i].added.month, shop[i].added.year);
    printf("valore %.2f\n", total(shop, n));
    Item* c = cheapest(shop, n);
    printf("piu' economico: %s\n", c->name);
    Item copy = shop[2];
    strcpy(copy.name, "copia");
    copy.added.year++;
    printf("%s %d / %s %d\n", shop[2].name, shop[2].added.year, copy.name, copy.added.year);
    printf("%zu\n", sizeof(Date));
    return 0;
}
