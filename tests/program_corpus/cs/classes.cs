// classi, proprieta', ereditarieta', interfacce, struct, enum, operatori (LS-05..LS-10, LS-21)
using System;
using System.Collections.Generic;

interface IShape
{
    double Area();
    string Name { get; }
}

abstract class Shape : IShape
{
    public abstract double Area();
    public virtual string Name { get { return "forma"; } }
    public override string ToString() { return Name + " " + Area().ToString("F2"); }
}

class Rect : Shape
{
    public double W { get; private set; }
    public double H { get; private set; }
    public Rect(double w, double h) { W = w; H = h; }
    public override double Area() { return W * H; }
    public override string Name => "rettangolo";
}

class Square : Rect
{
    public Square(double s) : base(s, s) { }
    public override string Name => "quadrato";
}

class Circle : Shape
{
    private readonly double r;
    public Circle(double r) { this.r = r; }
    public override double Area() { return Math.PI * r * r; }
}

struct Point
{
    public int X, Y;
    public Point(int x, int y) { X = x; Y = y; }
    public static Point operator +(Point a, Point b) { return new Point(a.X + b.X, a.Y + b.Y); }
    public static bool operator ==(Point a, Point b) { return a.X == b.X && a.Y == b.Y; }
    public static bool operator !=(Point a, Point b) { return !(a == b); }
    public override bool Equals(object o) { return o is Point && this == (Point)o; }
    public override int GetHashCode() { return X * 31 + Y; }
    public override string ToString() { return "(" + X + ", " + Y + ")"; }
}

enum Level { Low = 1, Medium, High = 10 }

class Counter
{
    public static int Created { get; private set; }
    public int Id { get; }
    public Counter() { Id = ++Created; }
}

class Temperature : IComparable<Temperature>
{
    public double Celsius { get; set; }
    public double Fahrenheit
    {
        get { return Celsius * 9 / 5 + 32; }
        set { Celsius = (value - 32) * 5 / 9; }
    }
    public int CompareTo(Temperature other) { return Celsius.CompareTo(other.Celsius); }
}

class Program
{
    static void Swap(ref int a, ref int b) { int t = a; a = b; b = t; }
    static bool TryHalf(int n, out int half)
    {
        half = n / 2;
        return n % 2 == 0;
    }
    static int Sum(params int[] xs)
    {
        int s = 0;
        foreach (int x in xs) s += x;
        return s;
    }
    static string Greet(string name, string greeting = "Ciao") { return greeting + " " + name; }

    static void Main()
    {
        var shapes = new List<Shape> { new Rect(2, 3), new Square(2), new Circle(1) };
        foreach (var s in shapes) Console.WriteLine(s);
        IShape i = shapes[1];
        Console.WriteLine(i.Name + " " + (i is Rect) + " " + (i is Circle) + " " + (shapes[2] as Rect == null));
        if (shapes[0] is Rect r) Console.WriteLine(r.W + "x" + r.H);
        Point p = new Point(1, 2), q = p;
        q.X = 10;
        Console.WriteLine(p + " " + q + " " + (p + q) + " " + (p == new Point(1, 2)));
        Level l = Level.Medium;
        Console.WriteLine(l + " " + (int)l + " " + (Level)10 + " " + (l < Level.High));
        new Counter(); new Counter();
        var c3 = new Counter();
        Console.WriteLine(c3.Id + " " + Counter.Created);
        var temps = new List<Temperature> { new Temperature { Celsius = 30 }, new Temperature { Fahrenheit = 32 }, new Temperature { Celsius = 15 } };
        temps.Sort();
        foreach (var t in temps) Console.Write(t.Celsius.ToString("F1") + "/" + t.Fahrenheit.ToString("F1") + " ");
        Console.WriteLine();
        int a = 1, b = 2;
        Swap(ref a, ref b);
        int h;
        bool even = TryHalf(7, out h);
        Console.WriteLine(a + " " + b + " " + even + " " + h + " " + Sum() + " " + Sum(1, 2, 3) + " " + Greet("Ada") + " " + Greet("Bob", "Salve"));
    }
}
