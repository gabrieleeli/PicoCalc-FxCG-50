// matrici double[,]: prodotto, determinante con Gauss, trasformazioni del piano
using System;

class Program
{
    static double[,] Multiply(double[,] a, double[,] b)
    {
        int n = a.GetLength(0), m = b.GetLength(1), k = a.GetLength(1);
        var r = new double[n, m];
        for (int i = 0; i < n; i++)
            for (int j = 0; j < m; j++)
                for (int t = 0; t < k; t++) r[i, j] += a[i, t] * b[t, j];
        return r;
    }

    static double Determinant(double[,] m)
    {
        int n = m.GetLength(0);
        var a = (double[,])m.Clone();
        double det = 1;
        for (int c = 0; c < n; c++)
        {
            int p = c;
            for (int r = c + 1; r < n; r++) if (Math.Abs(a[r, c]) > Math.Abs(a[p, c])) p = r;
            if (Math.Abs(a[p, c]) < 1e-12) return 0;
            if (p != c)
            {
                for (int k = 0; k < n; k++) { double t = a[c, k]; a[c, k] = a[p, k]; a[p, k] = t; }
                det = -det;
            }
            det *= a[c, c];
            for (int r = c + 1; r < n; r++)
            {
                double f = a[r, c] / a[c, c];
                for (int k = c; k < n; k++) a[r, k] -= f * a[c, k];
            }
        }
        return det;
    }

    static void Print(double[,] m)
    {
        for (int i = 0; i < m.GetLength(0); i++)
        {
            for (int j = 0; j < m.GetLength(1); j++) Console.Write("{0,8:F3}", Math.Abs(m[i, j]) < 1e-12 ? 0.0 : m[i, j]);
            Console.WriteLine();
        }
    }

    static void Main()
    {
        double[,] a = { { 1, 2, 3 }, { 0, 1, 4 }, { 5, 6, 0 } };
        double[,] b = { { 2, 0 }, { 1, 1 }, { 0, 3 } };
        Print(Multiply(a, b));
        Console.WriteLine("det = " + Determinant(a).ToString("F4"));
        double t = Math.PI / 2;
        double[,] rot = { { Math.Cos(t), -Math.Sin(t) }, { Math.Sin(t), Math.Cos(t) } };
        double[,] pts = { { 1, 0, -1 }, { 0, 1, 0 } };
        Print(Multiply(rot, pts));
        Print(Multiply(new double[,] { { 2, 0 }, { 0, 0.5 } }, pts));
    }
}
