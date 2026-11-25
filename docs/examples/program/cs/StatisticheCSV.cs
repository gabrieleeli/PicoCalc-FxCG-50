// Statistiche di un file CSV sulla microSD (nella cartella del programma).
// Se misure.csv non c'e' lo crea con dati d'esempio; poi legge le colonne,
// calcola media, deviazione standard, mediana e la retta di regressione
// (anche con la libreria Fx della calcolatrice) e mette le colonne nelle
// liste 1 e 2 per la modalita' Statistics.
using System;
using System.Collections.Generic;
using System.IO;

class StatisticheCSV
{
    static void Main()
    {
        const string nome = "misure.csv";
        if (!File.Exists(nome))
        {
            File.WriteAllLines(nome, new[] {
                "ora,temperatura", "8,12.5", "9,14.0", "10,15.5", "11,17.25", "12,18.0", "13,19.5", "14,19.0", "15,18.25"
            });
            Console.WriteLine("creato " + nome);
        }
        var ore = new List<double>();
        var temp = new List<double>();
        string[] righe = File.ReadAllLines(nome);
        for (int i = 1; i < righe.Length; i++)
        {
            if (righe[i].Trim().Length == 0) continue;
            string[] campi = righe[i].Split(',');
            ore.Add(double.Parse(campi[0]));
            temp.Add(double.Parse(campi[1]));
        }
        int n = temp.Count;
        double somma = 0;
        foreach (double t in temp) somma += t;
        double media = somma / n, scarti = 0;
        foreach (double t in temp) scarti += (t - media) * (t - media);
        var ordinate = new List<double>(temp);
        ordinate.Sort();
        double mediana = n % 2 == 1 ? ordinate[n / 2] : (ordinate[n / 2 - 1] + ordinate[n / 2]) / 2;
        Console.WriteLine($"{n} misure, media {media:F3}, s {Math.Sqrt(scarti / (n - 1)):F3}, mediana {mediana:F3}");
        Console.WriteLine($"minimo {ordinate[0]:F2}, massimo {ordinate[n - 1]:F2}");
        // le stesse statistiche con la libreria della calcolatrice
        var s = Fx.Stat1Var(temp.ToArray());
        Console.WriteLine($"Fx: media {s.Mean:F3}, sx {s.SampleSd:F3}, Q1 {s.Q1:F3}, Q3 {s.Q3:F3}");
        var r = Fx.Regression(0, ore.ToArray(), temp.ToArray());           // 0 = LinearReg(ax+b)
        Console.WriteLine($"retta: y = {r.A:F4}x + {r.B:F4}, r = {r.R:F4}");
        Console.WriteLine($"previsione alle 16: {r.A * 16 + r.B:F2}");
        for (int i = 0; i < n; i++)
        {
            Fx.SetList(1, i + 1, ore[i]);
            Fx.SetList(2, i + 1, temp[i]);
        }
        Console.WriteLine("List 1 e List 2 pronte per Statistics");
    }
}
