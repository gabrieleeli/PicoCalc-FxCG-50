// file confinati: File, StreamReader, StreamWriter (LS-18)
using System;
using System.IO;

class Program
{
    static void Main()
    {
        File.WriteAllText("voti.csv", "nome,voto\nAnna,8\nBruno,6\n");
        File.AppendAllText("voti.csv", "Carla,9\n");
        string[] lines = File.ReadAllLines("voti.csv");
        int sum = 0, count = 0;
        for (int i = 1; i < lines.Length; i++)
        {
            string[] f = lines[i].Split(',');
            sum += int.Parse(f[1]);
            count++;
        }
        Console.WriteLine(lines.Length + " righe, media " + ((double)sum / count).ToString("F2"));
        using (var w = new StreamWriter("log.txt"))
        {
            for (int i = 1; i <= 3; i++) w.WriteLine("riga " + i);
            w.Write("ultima");
        }
        using (var r = new StreamReader("log.txt"))
        {
            string line;
            int n = 0;
            while ((line = r.ReadLine()) != null) n += line.Length;
            Console.WriteLine(n);
        }
        var reader = new StreamReader("log.txt");
        Console.WriteLine(reader.ReadLine() + " " + reader.EndOfStream);
        string rest = reader.ReadToEnd();
        reader.Close();
        Console.WriteLine(rest.Length + " " + File.Exists("log.txt") + " " + File.Exists("assente.txt"));
        File.WriteAllLines("lista.txt", new[] { "a", "b", "c" });
        Console.WriteLine(File.ReadAllText("lista.txt").Replace("\n", "|"));
        File.Delete("lista.txt");
        Console.WriteLine(File.Exists("lista.txt"));
        try
        {
            File.ReadAllText("non_esiste.txt");
        }
        catch (FileNotFoundException)
        {
            Console.WriteLine("file non trovato");
        }
        Console.WriteLine(Path.GetExtension("dati/misure.csv") + " " + Path.GetFileName("dati/misure.csv") + " " + Path.Combine("a", "b.txt"));
    }
}
