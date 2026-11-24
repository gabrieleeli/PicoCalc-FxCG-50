// matrici 1.0.0 - matrici di double per i programmi C# della PicoCalc.
using System;
using System.Text;

namespace PicoCalc.Matrici
{
    public class Matrice
    {
        readonly double[,] v;
        public int Righe { get; }
        public int Colonne { get; }

        public Matrice(int righe, int colonne)
        {
            if (righe <= 0 || colonne <= 0) throw new ArgumentException("dimensioni non valide");
            Righe = righe;
            Colonne = colonne;
            v = new double[righe, colonne];
        }

        public static Matrice Da(int righe, int colonne, params double[] valori)
        {
            if (valori.Length != righe * colonne) throw new ArgumentException("servono " + (righe * colonne) + " valori");
            var m = new Matrice(righe, colonne);
            for (int r = 0; r < righe; r++)
                for (int c = 0; c < colonne; c++) m[r, c] = valori[r * colonne + c];
            return m;
        }

        public static Matrice Identita(int n)
        {
            var m = new Matrice(n, n);
            for (int i = 0; i < n; i++) m[i, i] = 1;
            return m;
        }

        public double this[int r, int c]
        {
            get => v[r, c];
            set => v[r, c] = value;
        }

        public static Matrice operator *(Matrice a, Matrice b)
        {
            if (a.Colonne != b.Righe) throw new ArgumentException("dimensioni incompatibili per il prodotto");
            var m = new Matrice(a.Righe, b.Colonne);
            for (int r = 0; r < a.Righe; r++)
                for (int c = 0; c < b.Colonne; c++)
                {
                    double s = 0;
                    for (int k = 0; k < a.Colonne; k++) s += a[r, k] * b[k, c];
                    m[r, c] = s;
                }
            return m;
        }

        public Matrice Trasposta()
        {
            var m = new Matrice(Colonne, Righe);
            for (int r = 0; r < Righe; r++)
                for (int c = 0; c < Colonne; c++) m[c, r] = this[r, c];
            return m;
        }

        // Eliminazione di Gauss con pivot parziale.
        public double Determinante()
        {
            if (Righe != Colonne) throw new InvalidOperationException("matrice non quadrata");
            int n = Righe;
            var a = new double[n, n];
            for (int r = 0; r < n; r++)
                for (int c = 0; c < n; c++) a[r, c] = this[r, c];
            double det = 1;
            for (int k = 0; k < n; k++)
            {
                int p = k;
                for (int r = k + 1; r < n; r++) if (Math.Abs(a[r, k]) > Math.Abs(a[p, k])) p = r;
                if (Math.Abs(a[p, k]) < 1e-12) return 0;
                if (p != k)
                {
                    for (int c = 0; c < n; c++) { double t = a[k, c]; a[k, c] = a[p, c]; a[p, c] = t; }
                    det = -det;
                }
                det *= a[k, k];
                for (int r = k + 1; r < n; r++)
                {
                    double f = a[r, k] / a[k, k];
                    for (int c = k; c < n; c++) a[r, c] -= f * a[k, c];
                }
            }
            return det;
        }

        public override string ToString()
        {
            var sb = new StringBuilder();
            for (int r = 0; r < Righe; r++)
            {
                for (int c = 0; c < Colonne; c++) sb.Append(c == 0 ? "" : " ").Append(this[r, c].ToString("0.###"));
                if (r + 1 < Righe) sb.Append('\n');
            }
            return sb.ToString();
        }
    }
}
