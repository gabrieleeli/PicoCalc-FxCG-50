/* matrici 1.0.0 - matrici piccole di double per i programmi C della PicoCalc.
 * Una Matrice ha al piu' MAT_MAX righe e colonne; gli elementi sono v[riga][colonna]. */
#ifndef MATRICI_H
#define MATRICI_H

#define MAT_MAX 6

typedef struct {
    int righe, colonne;
    double v[MAT_MAX][MAT_MAX];
} Matrice;

Matrice mat_nuova(int righe, int colonne);          /* tutta a zero */
Matrice mat_identita(int n);
Matrice mat_da_array(int righe, int colonne, const double* valori);   /* per righe */
Matrice mat_somma(Matrice a, Matrice b);
Matrice mat_prodotto(Matrice a, Matrice b);
Matrice mat_trasposta(Matrice a);
Matrice mat_per_scalare(Matrice a, double k);
double mat_determinante(Matrice a);                 /* eliminazione di Gauss con pivot */
int mat_inversa(Matrice a, Matrice* inversa);       /* 1 se invertibile, 0 se singolare */
void mat_stampa(Matrice a);

#endif
