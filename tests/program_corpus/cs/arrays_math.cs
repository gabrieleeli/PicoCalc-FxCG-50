// array 1D, 2D, frastagliati; Math, Convert, arrotondamenti, interi (LS-02, LS-04, LS-14)
using System;

class Program
{
    static double Mean(double[] v)
    {
        double s = 0;
        foreach (double x in v) s += x;
        return s / v.Length;
    }

    static void Main()
    {
        int[] a = { 5, 3, 8, 1 };
        Array.Sort(a);
        Console.WriteLine(string.Join(" ", a) + " " + a.Length + " " + Array.IndexOf(a, 8));
        int[,] m = new int[3, 4];
        for (int i = 0; i < m.GetLength(0); i++)
            for (int j = 0; j < m.GetLength(1); j++) m[i, j] = i * j;
        Console.WriteLine(m[2, 3] + " " + m.Length + " " + m.Rank + " " + m.GetLength(1));
        int[][] jag = new int[3][];
        for (int i = 0; i < 3; i++)
        {
            jag[i] = new int[i + 1];
            for (int j = 0; j <= i; j++) jag[i][j] = i + j;
        }
        foreach (var row in jag) Console.Write(string.Join(",", row) + ";");
        Console.WriteLine();
        double[] d = { 1.5, 2.5, 4.0 };
        Console.WriteLine(Mean(d).ToString("F4"));
        Array.Resize(ref a, 6);
        int[] copy = new int[6];
        Array.Copy(a, copy, 4);
        Console.WriteLine(a.Length + " " + a[5] + " " + copy[3]);
        Console.WriteLine(Math.Round(2.6) + " " + Math.Round(3.4) + " " + Math.Round(-2.7) + " " + Math.Round(2.375, 2) + " " + Math.Floor(-2.5) + " " + Math.Ceiling(2.1) + " " + Math.Truncate(-2.7));
        Console.WriteLine(Convert.ToInt32(2.5) + " " + Convert.ToInt32(3.5) + " " + (int)2.9 + " " + (int)-2.9 + " " + Convert.ToInt32("42"));
        Console.WriteLine(Math.Max(3, 7) + " " + Math.Min(-1, 2) + " " + Math.Abs(-4.5) + " " + Math.Pow(2, 10) + " " + Math.Sqrt(2).ToString("F5") + " " + Math.Sign(-3));
        Console.WriteLine(Math.Sin(Math.PI / 6).ToString("F3") + " " + Math.Log10(1000) + " " + Math.Exp(0) + " " + Math.Log(Math.E));
        Console.WriteLine(7 / 2 + " " + -7 / 2 + " " + 7 % 3 + " " + -7 % 3 + " " + 7.0 / 2 + " " + 7 / 2.0);
        int big = int.MaxValue;
        big++;
        long l = 1L << 40;
        Console.WriteLine(big + " " + l + " " + int.MinValue + " " + long.MaxValue);
        byte b = 250;
        b += 10;
        char c = 'a';
        c++;
        Console.WriteLine(b + " " + c + " " + (c + 1) + " " + (char)(c + 1));
        Console.WriteLine(double.Parse("2.5") * 2 + " " + int.Parse("-17") + " " + (0.1 + 0.2 == 0.3) + " " + (Math.Abs(0.1 + 0.2 - 0.3) < 1e-9));
        Console.WriteLine(1.0 / 0 + " " + (-1.0 / 0) + " " + double.IsNaN(0.0 / 0));
        uint u = 3;
        u -= 5;
        Console.WriteLine(u + " " + (5 & 3) + " " + (5 | 3) + " " + (5 ^ 3) + " " + (1 << 4) + " " + (~0));
    }
}
