// geometria2d 1.0.0 - punti e poligoni del piano per i programmi C++ della PicoCalc.
#pragma once
#include <vector>

struct Punto {
    double x, y;
};

double distanza(Punto a, Punto b);
double perimetro(const std::vector<Punto>& poligono);
double areaPoligono(const std::vector<Punto>& poligono);      // formula di Gauss (lacci), sempre positiva
Punto baricentro(const std::vector<Punto>& poligono);         // dei vertici
Punto ruota(Punto p, Punto centro, double gradi);             // antiorario
bool dentro(const std::vector<Punto>& poligono, Punto p);     // regola del raggio
