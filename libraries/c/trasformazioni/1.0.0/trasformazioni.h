/* trasformazioni 1.0.0 - trasformazioni del piano con le matrici omogenee 3x3
 * (libreria matrici). Gli angoli sono in gradi, positivi in senso antiorario. */
#ifndef TRASFORMAZIONI_H
#define TRASFORMAZIONI_H

#include <matrici.h>

Matrice tr_traslazione(double dx, double dy);
Matrice tr_rotazione(double gradi);
Matrice tr_scala(double sx, double sy);
Matrice tr_simmetria_x(void);                       /* rispetto all'asse x */
Matrice tr_componi(Matrice prima, Matrice poi);     /* applica prima, poi poi */
void tr_applica(Matrice t, double* x, double* y);

#endif
