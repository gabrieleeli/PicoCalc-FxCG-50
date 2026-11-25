// Duello a turni: l'eroe sceglie a (attacco), c (cura) o f (fuga) a ogni
// turno; l'orco risponde. Generatore pseudo-casuale proprio con un seme
// fisso: la partita si ripete uguale con le stesse mosse.
using System;

class Combattente
{
    public string Nome { get; }
    public int Vita { get; set; }
    public int Forza { get; }
    public Combattente(string nome, int vita, int forza) { Nome = nome; Vita = vita; Forza = forza; }
    public bool Vivo => Vita > 0;
    public override string ToString() => $"{Nome} ({Vita} PV)";
}

class Dado
{
    private ulong stato;
    public Dado(ulong seme) { stato = seme; }
    public int Lancia(int facce)
    {
        stato = stato * 6364136223846793005UL + 1442695040888963407UL;
        return 1 + (int)((stato >> 33) % (ulong)facce);
    }
}

class Duello
{
    static void Main()
    {
        var dado = new Dado(7);
        var eroe = new Combattente("Eroe", 30, 5);
        var orco = new Combattente("Orco", 26, 4);
        int turno = 1, cure = 2;
        Console.WriteLine($"{eroe} contro {orco}");
        while (eroe.Vivo && orco.Vivo)
        {
            Console.Write($"turno {turno}: mossa (a/c/f)? ");
            string mossa = Console.ReadLine();
            if (mossa == null) break;
            Console.WriteLine(mossa);
            switch (mossa.Trim())
            {
                case "a":
                    int colpo = eroe.Forza + dado.Lancia(6);
                    orco.Vita -= colpo;
                    Console.WriteLine($"  colpisci per {colpo}: {orco}");
                    break;
                case "c":
                    if (cure == 0) { Console.WriteLine("  niente pozioni"); continue; }
                    cure--;
                    eroe.Vita += 8;
                    Console.WriteLine($"  ti curi: {eroe}, pozioni {cure}");
                    break;
                case "f":
                    Console.WriteLine("  fuggi!");
                    return;
                default:
                    Console.WriteLine("  mossa non valida");
                    continue;
            }
            if (orco.Vivo)
            {
                int danno = orco.Forza + dado.Lancia(4);
                eroe.Vita -= danno;
                Console.WriteLine($"  l'orco colpisce per {danno}: {eroe}");
            }
            turno++;
        }
        Console.WriteLine(eroe.Vivo ? (orco.Vivo ? "partita interrotta" : "hai vinto!") : "hai perso...");
    }
}
