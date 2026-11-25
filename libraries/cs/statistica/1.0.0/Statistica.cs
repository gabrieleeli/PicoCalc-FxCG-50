// statistica 1.0.0 - statistica a una e due variabili per i programmi C# della PicoCalc.
using System;
using System.Collections.Generic;

namespace PicoCalc.Statistica
{
    public class Retta
    {
        public double A { get; }
        public double B { get; }
        public double R { get; }
        public Retta(double a, double b, double r) { A = a; B = b; R = r; }
        public double Valore(double x) => A + B * x;
        public override string ToString() => $"y = {A:F4} + {B:F4}x (r = {R:F4})";
    }

    public static class Descrittiva
    {
        public static double Media(IList<double> x)
        {
            if (x.Count == 0) throw new ArgumentException("nessun dato");
            double s = 0;
            foreach (double v in x) s += v;
            return s / x.Count;
        }

        // Deviazione standard della popolazione (sigma x della fx-CG50).
        public static double DevStdPop(IList<double> x) => Math.Sqrt(Scarti2(x) / x.Count);

        // Deviazione standard campionaria (s x della fx-CG50).
        public static double DevStdCamp(IList<double> x) => Math.Sqrt(Scarti2(x) / (x.Count - 1));

        public static double Mediana(IList<double> x)
        {
            var c = new List<double>(x);
            c.Sort();
            int n = c.Count;
            return n % 2 == 1 ? c[n / 2] : (c[n / 2 - 1] + c[n / 2]) / 2;
        }

        public static Retta Regressione(IList<double> x, IList<double> y)
        {
            if (x.Count != y.Count || x.Count < 2) throw new ArgumentException("servono almeno due coppie (x, y)");
            double mx = Media(x), my = Media(y), sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < x.Count; i++)
            {
                sxy += (x[i] - mx) * (y[i] - my);
                sxx += (x[i] - mx) * (x[i] - mx);
                syy += (y[i] - my) * (y[i] - my);
            }
            double b = sxy / sxx;
            return new Retta(my - b * mx, b, sxy / Math.Sqrt(sxx * syy));
        }

        static double Scarti2(IList<double> x)
        {
            double m = Media(x), s = 0;
            foreach (double v in x) s += (v - m) * (v - m);
            return s;
        }
    }
}
