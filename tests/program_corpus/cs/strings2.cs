// testo: StringBuilder, cifrario di Cesare, inversione, conteggi con array di char
using System;
using System.Text;

class Program
{
    static string Caesar(string s, int k)
    {
        var sb = new StringBuilder(s.Length);
        foreach (char c in s)
        {
            if (char.IsUpper(c)) sb.Append((char)('A' + (c - 'A' + k + 26) % 26));
            else if (char.IsLower(c)) sb.Append((char)('a' + (c - 'a' + k + 26) % 26));
            else sb.Append(c);
        }
        return sb.ToString();
    }

    static string Reverse(string s)
    {
        char[] a = s.ToCharArray();
        Array.Reverse(a);
        return new string(a);
    }

    static void Main()
    {
        string msg = "Ciao, PicoCalc!";
        string enc = Caesar(msg, 3);
        Console.WriteLine(enc + " -> " + Caesar(enc, -3));
        Console.WriteLine(Reverse(msg) + " " + new string('=', 5));
        int[] counts = new int[26];
        foreach (char c in msg.ToLower()) if (c >= 'a' && c <= 'z') counts[c - 'a']++;
        var sb = new StringBuilder();
        for (int i = 0; i < 26; i++) if (counts[i] > 1) sb.AppendFormat("{0}:{1} ", (char)('a' + i), counts[i]);
        Console.WriteLine(sb.ToString().TrimEnd());
        string words = "  uno   due tre  ";
        string[] parts = words.Trim().Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        Console.WriteLine(parts.Length + " [" + string.Join("|", parts) + "]");
        var b = new StringBuilder("abcdef");
        b.Remove(1, 2).Insert(0, "X").Replace('e', 'E');
        b[0] = 'Y';
        Console.WriteLine(b + " " + b.Length + " " + b.ToString().IndexOf('E'));
        Console.WriteLine(string.Concat("a", 1, 2.5, true) + " " + "x".PadLeft(3, '0') + " " + "Mississippi".Replace("ss", "SS").LastIndexOf("SS"));
        Console.WriteLine("Hello".CompareTo("help") < 0 ? "minore" : "maggiore");
        Console.WriteLine(string.Join(",", "a,b;c".Split(',', ';')) + " " + "abc".Contains("bc") + " " + "ABC".ToLower().StartsWith("ab"));
    }
}
