/* Uso di librerie installate: trasformazioni (che a sua volta usa matrici).
 * Installare prima la libreria con Program > LIB (F2 INST dall'indice o
 * F1 LOCAL da LIB/IMPORT): le dipendenze si installano da sole.
 * Un quadrato ruotato, ingrandito e traslato con le matrici omogenee. */
#include <stdio.h>
#include <trasformazioni.h>

int main(void) {
    double x[4] = {0, 2, 2, 0}, y[4] = {0, 0, 2, 2};
    Matrice t = tr_componi(tr_componi(tr_rotazione(30), tr_scala(1.5, 1.5)), tr_traslazione(1, -2));
    printf("vertice      prima          dopo\n");
    for (int i = 0; i < 4; i++) {
        double px = x[i], py = y[i];
        tr_applica(t, &px, &py);
        printf("%d      (%5.2f, %5.2f)  (%6.3f, %6.3f)\n", i + 1, x[i], y[i], px, py);
    }
    double sx = 3, sy = 1;
    tr_applica(tr_simmetria_x(), &sx, &sy);
    printf("simmetria di (3, 1) rispetto all'asse x: (%.0f, %.0f)\n", sx, sy);
    return 0;
}
