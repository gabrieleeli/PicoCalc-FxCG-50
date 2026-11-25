// equazioni di secondo grado lette da Console.ReadLine
using System;

class Program
{
    static string Solve(double a, double b, double c)
    {
        if (a == 0) return b == 0 ? (c == 0 ? "infinite soluzioni" : "impossibile") : "x = " + (-c / b).ToString("F4");
        double d = b * b - 4 * a * c;
        if (d > 0)
        {
            double s = Math.Sqrt(d);
            return string.Format("x1 = {0:F4}, x2 = {1:F4}", (-b - s) / (2 * a), (-b + s) / (2 * a));
        }
        if (d == 0) return $"x = {-b / (2 * a):F4} (doppia)";
        return $"x = {-b / (2 * a):F4} +- {Math.Sqrt(-d) / (2 * Math.Abs(a)):F4}i";
    }

    static void Main()
    {
        string line;
        int n = 0;
        while ((line = Console.ReadLine()) != null)
        {
            string[] p = line.Split(' ');
            if (p.Length != 3) continue;
            double a = double.Parse(p[0]), b = double.Parse(p[1]), c = double.Parse(p[2]);
            Console.WriteLine($"{a}x^2 + {b}x + {c}: {Solve(a, b, c)}");
            n++;
        }
        Console.WriteLine(n + " equazioni");
    }
}
