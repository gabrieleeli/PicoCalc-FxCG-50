/* statistica 1.0.0 - statistica a una e due variabili per i programmi C della PicoCalc.
 * Come la fx-CG50: deviazione standard della popolazione (sx con n-1 a parte). */
#ifndef STATISTICA_H
#define STATISTICA_H

double stat_media(const double* x, int n);
double stat_somma(const double* x, int n);
double stat_dev_pop(const double* x, int n);        /* sigma x (divide per n) */
double stat_dev_camp(const double* x, int n);       /* s x (divide per n - 1) */
double stat_mediana(const double* x, int n);        /* non modifica x */
double stat_minimo(const double* x, int n);
double stat_massimo(const double* x, int n);
/* Retta dei minimi quadrati y = a + b x e coefficiente di correlazione r. */
void stat_regressione(const double* x, const double* y, int n, double* a, double* b, double* r);

#endif
