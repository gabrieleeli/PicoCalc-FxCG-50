// finanza 1.0.0 - matematica finanziaria per i programmi C# della PicoCalc.
// Tassi come numeri (0.05 = 5%); i flussi di cassa partono dal periodo 0.
using System;

namespace PicoCalc.Finanza
{
    public static class Interessi
    {
        // Capitale dopo `periodi` periodi all'interesse composto.
        public static double Montante(double capitale, double tasso, int periodi) => capitale * Math.Pow(1 + tasso, periodi);

        // Capitale da investire oggi per avere `montante` dopo `periodi` periodi.
        public static double ValoreAttuale(double montante, double tasso, int periodi) => montante / Math.Pow(1 + tasso, periodi);

        // Rata costante (ammortamento francese) di un prestito.
        public static double Rata(double capitale, double tasso, int periodi)
        {
            if (periodi <= 0) throw new ArgumentOutOfRangeException(nameof(periodi), "servono periodi positivi");
            if (tasso == 0) return capitale / periodi;
            return capitale * tasso / (1 - Math.Pow(1 + tasso, -periodi));
        }

        // Valore attuale netto dei flussi (flussi[0] al tempo 0).
        public static double Van(double tasso, double[] flussi)
        {
            double s = 0;
            for (int t = 0; t < flussi.Length; t++) s += flussi[t] / Math.Pow(1 + tasso, t);
            return s;
        }

        // Tasso interno di rendimento per bisezione tra -99% e 1000%.
        public static double Tir(double[] flussi)
        {
            double a = -0.99, b = 10.0;
            double fa = Van(a, flussi), fb = Van(b, flussi);
            if (fa * fb > 0) throw new InvalidOperationException("il TIR non esiste in [-99%, 1000%]");
            for (int k = 0; k < 200 && b - a > 1e-12; k++)
            {
                double m = (a + b) / 2, fm = Van(m, flussi);
                if ((fa < 0) == (fm < 0)) { a = m; fa = fm; } else b = m;
            }
            return (a + b) / 2;
        }
    }
}
