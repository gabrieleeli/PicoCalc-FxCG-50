// gerarchia di classi: abstract, interface, virtual/override, base, static, as/is, ToString
using System;
using System.Collections.Generic;

interface IPayable
{
    double Pay();
}

abstract class Employee : IPayable
{
    private static int count;
    public string Name { get; }
    public int Id { get; }
    protected Employee(string name) { Name = name; Id = ++count; }
    public abstract double Pay();
    public virtual string Role => "dipendente";
    public override string ToString() => $"#{Id} {Name,-6} {Role,-12} {Pay(),9:F2}";
    public static int Count => count;
}

class Salaried : Employee
{
    private readonly double monthly;
    public Salaried(string name, double monthly) : base(name) { this.monthly = monthly; }
    public override double Pay() => monthly;
    public override string Role => "impiegato";
}

class Hourly : Employee
{
    public double Rate { get; set; }
    public int Hours { get; set; }
    public Hourly(string name, double rate, int hours) : base(name) { Rate = rate; Hours = hours; }
    public override double Pay()
    {
        int extra = Math.Max(0, Hours - 160);
        return Rate * (Hours - extra) + Rate * 1.5 * extra;
    }
    public override string Role => "a ore";
}

class Manager : Salaried
{
    private readonly List<Employee> team = new List<Employee>();
    public Manager(string name, double monthly) : base(name, monthly) { }
    public void Add(Employee e) { team.Add(e); }
    public override double Pay() => base.Pay() + 100 * team.Count;
    public override string Role => "responsabile";
}

class Program
{
    static void Main()
    {
        var m = new Manager("Ada", 3000);
        var staff = new List<Employee> { m, new Salaried("Bob", 2000), new Hourly("Cleo", 15.5, 172), new Hourly("Dan", 20, 100) };
        m.Add(staff[1]);
        m.Add(staff[2]);
        double total = 0;
        foreach (Employee e in staff)
        {
            Console.WriteLine(e);
            total += e.Pay();
        }
        Console.WriteLine($"totale {total:F2} per {Employee.Count} persone");
        IPayable p = staff[2];
        Hourly h = p as Hourly;
        Console.WriteLine((p is Employee) + " " + (h != null) + " " + (staff[1] as Hourly == null) + " " + h.Hours);
        staff.Sort((x, y) => y.Pay().CompareTo(x.Pay()));
        var names = new List<string>();
        foreach (var x in staff) names.Add(x.Name);
        Console.WriteLine(string.Join(", ", names));
    }
}
