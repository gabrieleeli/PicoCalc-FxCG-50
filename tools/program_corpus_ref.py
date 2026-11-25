#!/usr/bin/env python3
"""Uscite di riferimento del corpus di Program (v1.20.0).

Uso: tools/program_corpus_ref.py [nome ...]

Per ogni programma di tests/program_corpus/{c,cpp,cs} compila con i
compilatori reali (gcc -std=c11, g++ -std=c++17, mcs di Mono), esegue in una
cartella vuota con l'input <nome>.in (se c'e') e scrive l'uscita in
<nome>.out. tests/program_corpus_test esegue gli stessi programmi con
l'interprete di Program e confronta le uscite byte per byte. Le uscite si
rigenerano solo quando un programma cambia (richiede gcc, g++, mcs e mono).
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = os.path.join(ROOT, "tests", "program_corpus")
ENV = dict(os.environ, LANG="C", LC_ALL="C")


def build(lang, src, work):
    exe = os.path.join(work, "prog")
    if lang == "c":
        cmd = ["gcc", "-std=c11", "-O0", "-Wall", "-o", exe, src, "-lm"]
    elif lang == "cpp":
        cmd = ["g++", "-std=c++17", "-O0", "-Wall", "-o", exe, src]
    else:
        exe += ".exe"
        cmd = ["mcs", "-nologo", "-out:" + exe, src]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("compilazione fallita: %s\n%s%s" % (src, r.stdout, r.stderr))
    return ["mono", exe] if lang == "cs" else [exe]


def main(names):
    done = 0
    for lang in ("c", "cpp", "cs"):
        folder = os.path.join(CORPUS, lang)
        for f in sorted(os.listdir(folder)):
            base, ext = os.path.splitext(f)
            if ext.lstrip(".") != lang or (names and base not in names):
                continue
            src = os.path.join(folder, f)
            inp = os.path.join(folder, base + ".in")
            with tempfile.TemporaryDirectory() as work:
                cmd = build(lang, src, work)
                stdin = open(inp, "rb").read() if os.path.exists(inp) else b""
                r = subprocess.run(cmd, input=stdin, capture_output=True, cwd=work, env=ENV, timeout=60)
            with open(os.path.join(folder, base + ".out"), "wb") as out:
                out.write(r.stdout)
            print("%-4s %-16s exit %d, %d byte" % (lang, base, r.returncode, len(r.stdout)))
            done += 1
    return 0 if done else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
