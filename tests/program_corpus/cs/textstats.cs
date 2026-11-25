// statistiche di un testo: Dictionary, ordinamento per valore, StringBuilder
using System;
using System.Collections.Generic;
using System.Text;

class Program
{
    static void Main()
    {
        var freq = new Dictionary<string, int>();
        int lines = 0, words = 0, letters = 0;
        string line;
        while ((line = Console.ReadLine()) != null)
        {
            lines++;
            var w = new StringBuilder();
            foreach (char ch in line + " ")
            {
                if (char.IsLetter(ch))
                {
                    w.Append(char.ToLower(ch));
                    letters++;
                }
                else if (w.Length > 0)
                {
                    string k = w.ToString();
                    freq[k] = freq.ContainsKey(k) ? freq[k] + 1 : 1;
                    words++;
                    w.Clear();
                }
            }
        }
        Console.WriteLine($"{lines} righe, {words} parole, {letters} lettere, {freq.Count} diverse");
        var list = new List<KeyValuePair<string, int>>(freq);
        list.Sort((a, b) => a.Value != b.Value ? b.Value.CompareTo(a.Value) : string.CompareOrdinal(a.Key, b.Key));
        for (int i = 0; i < 5 && i < list.Count; i++) Console.WriteLine($"{list[i].Key,8} {new string('*', list[i].Value)}");
        string longest = "";
        foreach (var kv in list) if (kv.Key.Length > longest.Length) longest = kv.Key;
        Console.WriteLine($"piu' lunga: {longest} ({longest.Length})");
        Console.WriteLine("media {0:F2} lettere", (double)letters / words);
    }
}
