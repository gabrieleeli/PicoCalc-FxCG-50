// List, Dictionary, HashSet, Queue, Stack, lambda (LS-15, LS-16)
using System;
using System.Collections.Generic;

class Program
{
    static void Main()
    {
        var nums = new List<int> { 5, 3, 8, 1, 9, 2 };
        nums.Add(7);
        nums.Sort();
        Console.WriteLine(string.Join(" ", nums));
        nums.Sort((a, b) => b.CompareTo(a));
        Console.WriteLine(string.Join(",", nums) + " " + nums.Count + " " + nums[0]);
        nums.Remove(8);
        nums.RemoveAt(0);
        nums.Insert(1, 100);
        Console.WriteLine(string.Join(" ", nums) + " " + nums.Contains(100) + " " + nums.IndexOf(3));
        Console.WriteLine(nums.Find(x => x < 3) + " " + nums.FindIndex(x => x > 50) + " " + nums.Exists(x => x == 42));
        nums.RemoveAll(x => x % 2 == 0);
        Console.WriteLine(string.Join(" ", nums));
        int total = 0;
        nums.ForEach(x => total += x);
        Console.WriteLine(total);
        var words = "il gatto e il cane e il topo".Split(' ');
        var freq = new Dictionary<string, int>();
        foreach (var w in words)
        {
            if (freq.ContainsKey(w)) freq[w]++;
            else freq[w] = 1;
        }
        foreach (var kv in freq) Console.Write(kv.Key + "=" + kv.Value + " ");
        Console.WriteLine();
        int v;
        Console.WriteLine(freq.TryGetValue("cane", out v) + " " + v + " " + freq.TryGetValue("x", out v) + " " + v);
        try
        {
            freq.Add("il", 9);
        }
        catch (ArgumentException)
        {
            Console.WriteLine("chiave gia' presente");
        }
        try
        {
            Console.WriteLine(freq["cavallo"]);
        }
        catch (KeyNotFoundException)
        {
            Console.WriteLine("chiave assente");
        }
        freq.Remove("e");
        Console.WriteLine(freq.Count + " " + string.Join(",", freq.Keys) + " " + freq.ContainsValue(3));
        var set = new HashSet<int> { 5, 1, 3, 1, 5 };
        set.Add(2);
        Console.WriteLine(set.Count + " " + set.Contains(3) + " " + set.Add(3));
        var q = new Queue<string>();
        q.Enqueue("a"); q.Enqueue("b"); q.Enqueue("c");
        Console.WriteLine(q.Dequeue() + q.Peek() + q.Count);
        var st = new Stack<int>();
        for (int i = 0; i < 4; i++) st.Push(i * i);
        Console.Write(st.Peek() + ":");
        while (st.Count > 0) Console.Write(" " + st.Pop());
        Console.WriteLine();
        int[] arr = nums.ToArray();
        Array.Reverse(arr);
        Console.WriteLine(arr.Length + " " + arr[0] + " " + Array.IndexOf(arr, 9));
        Func<int, int, int> add = (a, b) => a + b;
        Predicate<string> longWord = s => s.Length > 3;
        Action<string> show = s => Console.Write("<" + s + ">");
        show(add(2, 3).ToString());
        Console.WriteLine(longWord("gatto") + " " + longWord("il"));
    }
}
