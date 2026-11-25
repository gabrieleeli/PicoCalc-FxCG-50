// eccezioni: standard, dell'utente, finally, rilancio, when assente (LS-17)
using System;
using System.Collections.Generic;

class InsufficientFundsException : Exception
{
    public double Missing { get; }
    public InsufficientFundsException(double missing) : base("mancano " + missing.ToString("F2")) { Missing = missing; }
}

class Account
{
    public double Balance { get; private set; }
    public Account(double initial) { Balance = initial; }
    public void Withdraw(double amount)
    {
        if (amount <= 0) throw new ArgumentOutOfRangeException("amount");
        if (amount > Balance) throw new InsufficientFundsException(amount - Balance);
        Balance -= amount;
    }
}

class Program
{
    static int Parse(string s)
    {
        try
        {
            return int.Parse(s);
        }
        catch (FormatException)
        {
            Console.WriteLine("non e' un numero: " + s);
            return -1;
        }
        finally
        {
            Console.WriteLine("finally per " + s);
        }
    }

    static void Main()
    {
        Console.WriteLine(Parse("12") + Parse("x"));
        var acc = new Account(50);
        try
        {
            acc.Withdraw(20);
            acc.Withdraw(100);
        }
        catch (InsufficientFundsException e)
        {
            Console.WriteLine(e.Message + " " + e.Missing + " " + acc.Balance);
        }
        try
        {
            acc.Withdraw(-1);
        }
        catch (ArgumentException e)
        {
            Console.WriteLine(e.GetType().Name);
        }
        int[] a = new int[3];
        try
        {
            a[5] = 1;
        }
        catch (IndexOutOfRangeException)
        {
            Console.WriteLine("indice fuori dai limiti");
        }
        try
        {
            int zero = 0;
            Console.WriteLine(10 / zero);
        }
        catch (DivideByZeroException)
        {
            Console.WriteLine("divisione per zero");
        }
        try
        {
            string s = null;
            Console.WriteLine(s.Length);
        }
        catch (NullReferenceException)
        {
            Console.WriteLine("riferimento null");
        }
        try
        {
            try
            {
                throw new InvalidOperationException("prima");
            }
            catch (InvalidOperationException e)
            {
                Console.WriteLine("presa: " + e.Message);
                throw;
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("rilanciata: " + e.Message);
        }
        var stack = new Stack<int>();
        try
        {
            stack.Pop();
        }
        catch (InvalidOperationException)
        {
            Console.WriteLine("pila vuota");
        }
        try
        {
            object o = "testo";
            int n = (int)o;
            Console.WriteLine(n);
        }
        catch (InvalidCastException)
        {
            Console.WriteLine("cast non valido");
        }
        int r;
        Console.WriteLine(int.TryParse("123", out r) + " " + r + " " + int.TryParse("12a", out r) + " " + r);
    }
}
