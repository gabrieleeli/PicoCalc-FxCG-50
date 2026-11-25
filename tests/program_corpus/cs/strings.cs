// stringhe, StringBuilder, formati (LS-12, LS-13, LS-14)
using System;
using System.Text;

class Program
{
    static void Main()
    {
        string s = "Hello, World";
        Console.WriteLine(s.Length + " " + s.IndexOf("World") + " " + s.IndexOf('x') + " " + s.Contains("lo,"));
        Console.WriteLine(s.Substring(7) + "|" + s.Substring(0, 5) + "|" + s.ToUpper() + "|" + s.ToLower());
        Console.WriteLine(s.Replace("World", "PicoCalc").Insert(0, ">> ").Remove(3, 2));
        Console.WriteLine("[" + "  spazi  ".Trim() + "][" + "xxabcxx".TrimStart('x') + "][" + "abc".PadLeft(6) + "][" + "abc".PadRight(6, '.') + "]");
        string[] parts = "rosso,verde,,blu".Split(',');
        Console.WriteLine(parts.Length + " " + string.Join("+", parts));
        Console.WriteLine(s.StartsWith("Hell") + " " + s.EndsWith("x") + " " + s.LastIndexOf('o') + " " + s[4]);
        char[] cs = "dcba".ToCharArray();
        Array.Sort(cs);
        Console.WriteLine(new string(cs) + " " + string.Concat("a", "b", "c") + " " + string.IsNullOrEmpty("") + " " + "b".CompareTo("a"));
        var sb = new StringBuilder();
        for (int i = 0; i < 5; i++) sb.Append(i).Append(i < 4 ? "," : "");
        sb.Insert(0, "[").Append(']');
        Console.WriteLine(sb.ToString() + " " + sb.Length);
        int n = 42;
        double x = 3.14159;
        Console.WriteLine($"n={n} x={x:F2} [{n,5}] [{n,-5}] {n:D4} {255:X} {1234567.891:N2}");
        Console.WriteLine(string.Format("{0} + {1} = {2}", 2, 3, 2 + 3));
        Console.WriteLine("{0,6:F1}|{1,-6}|", 2.25, "ab");
        Console.WriteLine(x.ToString("F3") + " " + (0.5).ToString("0.00") + " " + 7.ToString("D3"));
        string multi = @"C:\cartella\file.txt";
        Console.WriteLine(multi.Length + " " + multi.Substring(multi.LastIndexOf('\\') + 1));
        Console.WriteLine(char.IsDigit('7') + " " + char.IsLetter('7') + " " + char.ToUpper('q') + " " + (int)'A' + " " + (char)66);
        string word = "radar";
        bool pal = true;
        for (int i = 0; i < word.Length / 2; i++) if (word[i] != word[word.Length - 1 - i]) pal = false;
        Console.WriteLine(word + " " + pal);
        Console.WriteLine(string.Equals("a", "a") + " " + ("abc" == "ab" + "c"));
    }
}
