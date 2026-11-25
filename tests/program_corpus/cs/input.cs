// lettura con Console.ReadLine fino alla fine dell'input (LS-13)
using System;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        int n = int.Parse(Console.ReadLine());
        var values = new List<double>();
        while (values.Count < n)
        {
            foreach (var part in Console.ReadLine().Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries))
                values.Add(double.Parse(part));
        }
        double sum = 0;
        foreach (var v in values) sum += v;
        Console.WriteLine("media " + (sum / n).ToString("F2"));
        string name = Console.ReadLine();
        Console.WriteLine("Ciao " + name.Trim() + "!");
        string line;
        int count = 0;
        while ((line = Console.ReadLine()) != null)
        {
            count++;
            Console.WriteLine(count + ": " + line.ToUpper());
        }
        Console.WriteLine("fine dopo " + count + " righe");
    }
}
