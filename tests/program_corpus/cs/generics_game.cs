// generici dell'utente, switch su stringhe, gioco a turni con un generatore proprio (LS-11, LS-15)
using System;
using System.Collections.Generic;

class Box<T>
{
    private readonly List<T> items = new List<T>();
    public void Put(T item) { items.Add(item); }
    public T Get(int i) { return items[i]; }
    public int Count { get { return items.Count; } }
}

class Rng
{
    private ulong state;
    public Rng(ulong seed) { state = seed; }
    public int Next(int n)
    {
        state = state * 6364136223846793005UL + 1442695040888963407UL;
        return (int)((state >> 33) % (ulong)n);
    }
}

class Program
{
    static T Max<T>(T a, T b) where T : IComparable<T> { return a.CompareTo(b) >= 0 ? a : b; }

    static string Describe(string cmd)
    {
        switch (cmd)
        {
            case "a": return "attacco";
            case "c":
            case "cura": return "cura";
            default: return "?";
        }
    }

    static void Main()
    {
        var box = new Box<string>();
        box.Put("uno");
        box.Put("due");
        Console.WriteLine(box.Count + " " + box.Get(1) + " " + Max(3, 9) + " " + Max("pera", "mela") + " " + Max(2.5, -1.0));
        var rng = new Rng(2024);
        int hero = 30, orc = 25, turn = 1;
        string move;
        while (hero > 0 && orc > 0 && (move = Console.ReadLine()) != null)
        {
            string what = Describe(move);
            if (what == "?")
            {
                Console.WriteLine("mossa sconosciuta: " + move);
                continue;
            }
            if (what == "attacco")
            {
                int dmg = 6 + rng.Next(4);
                orc -= dmg;
                Console.WriteLine($"turno {turn}: colpo da {dmg} (orco {orc})");
            }
            else
            {
                hero += 4;
                Console.WriteLine($"turno {turn}: cura (eroe {hero})");
            }
            if (orc > 0)
            {
                int back = 5 + rng.Next(3);
                hero -= back;
                Console.WriteLine($"         l'orco risponde con {back} (eroe {hero})");
            }
            turn++;
        }
        Console.WriteLine(orc <= 0 ? "vince l'eroe" : hero <= 0 ? "vince l'orco" : "pareggio");
    }
}
