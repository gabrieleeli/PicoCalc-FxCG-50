/* trasformazioni 1.0.0 - vedi trasformazioni.h */
#include <math.h>
#include "trasformazioni.h"

Matrice tr_traslazione(double dx, double dy) {
    Matrice m = mat_identita(3);
    m.v[0][2] = dx;
    m.v[1][2] = dy;
    return m;
}

Matrice tr_rotazione(double gradi) {
    double a = gradi * M_PI / 180.0;
    Matrice m = mat_identita(3);
    m.v[0][0] = cos(a); m.v[0][1] = -sin(a);
    m.v[1][0] = sin(a); m.v[1][1] = cos(a);
    return m;
}

Matrice tr_scala(double sx, double sy) {
    Matrice m = mat_identita(3);
    m.v[0][0] = sx;
    m.v[1][1] = sy;
    return m;
}

Matrice tr_simmetria_x(void) {
    return tr_scala(1.0, -1.0);
}

Matrice tr_componi(Matrice prima, Matrice poi) {
    return mat_prodotto(poi, prima);
}

void tr_applica(Matrice t, double* x, double* y) {
    double nx = t.v[0][0] * *x + t.v[0][1] * *y + t.v[0][2];
    double ny = t.v[1][0] * *x + t.v[1][1] * *y + t.v[1][2];
    *x = nx;
    *y = ny;
}
