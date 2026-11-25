// metodi numerici con Func<double, double>: bisezione, Newton, Simpson
using System;

class Program
{
    static double Bisection(Func<double, double> f, double a, double b, double eps, out int steps)
    {
        steps = 0;
        while (b - a > eps)
        {
            double m = (a + b) / 2;
            if (f(a) * f(m) <= 0) b = m; else a = m;
            steps++;
        }
        return (a + b) / 2;
    }

    static double Newton(Func<double, double> f, Func<double, double> df, double x, int iterations)
    {
        for (int i = 0; i < iterations; i++) x -= f(x) / df(x);
        return x;
    }

    static double Simpson(Func<double, double> f, double a, double b, int n)
    {
        double h = (b - a) / n, s = f(a) + f(b);
        for (int i = 1; i < n; i++) s += f(a + i * h) * (i % 2 == 1 ? 4 : 2);
        return s * h / 3;
    }

    static double G(double x) { return x * x * x - x - 2; }

    static void Main()
    {
        int steps;
        double r = Bisection(G, 1, 2, 1e-9, out steps);
        Console.WriteLine("bisezione {0:F8} in {1} passi", r, steps);
        double s2 = Newton(x => x * x - 2, x => 2 * x, 1.0, 6);
        Console.WriteLine("sqrt(2) {0:F8} errore {1:E2}", s2, Math.Abs(s2 - Math.Sqrt(2)));
        Console.WriteLine("integrale di sin su [0, pi] {0:F8}", Simpson(Math.Sin, 0, Math.PI, 100));
        Console.WriteLine("integrale di x^2 su [0, 3] {0:F8}", Simpson(x => x * x, 0, 3, 10));
        double e = 0, term = 1;
        for (int k = 0; k < 20; k++)
        {
            e += term;
            term /= k + 1;
        }
        Console.WriteLine("e {0:F8} {1:F8}", e, Math.Exp(1));
    }
}
