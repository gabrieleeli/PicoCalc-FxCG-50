#!/usr/bin/env python3
# v1.18.0: manifesto del congelamento del Pico 2 W per tests/architecture_test.cpp.
# Uso: tools/freeze_manifest.py <repo> build/host-pico2w/build.ninja <commit> tests/fixtures/pico2w_frozen_v1_17_0.txt
# Generates tests/fixtures/pico2w_frozen_v1_17_0.txt: FNV-1a 64 of every file the
# Raspberry Pi Pico 2 W compiled at v1.17.0 (sources + include/*.hpp), from git.
import subprocess, sys, re
root, ninja, rev, out = sys.argv[1:5]
text = open(ninja, encoding='utf-8').read()
m = re.search(r'FXAI_COMPILED_SOURCES=\\"([^\\]*)', text)
srcs = [s for s in m.group(1).split(',') if s]
def exists(path):
    return subprocess.run(["git", "-C", root, "cat-file", "-e", f"{rev}:{path}"], capture_output=True).returncode == 0
srcs = [s for s in srcs if exists(s)]       # files added after <commit> are not frozen
heads = subprocess.run(["git", "-C", root, "ls-tree", "--name-only", rev, "include/"], capture_output=True, text=True, check=True).stdout.split()
files = sorted(set(srcs) | set(h for h in heads if h.endswith(".hpp")))
def fnv(b):
    h = 1469598103934665603
    for x in b:
        h ^= x; h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h
lines = ["# v1.18.0: congelamento del Raspberry Pi Pico 2 W (tests/architecture_test.cpp).",
         "# FNV-1a a 64 bit dei byte di ogni file compilato sul Pico 2 W alla v1.17.0",
         "# (aa2923c): sorgenti di FXAI_COMPILED_SOURCES e include/*.hpp. Un file del",
         "# manifesto che cambia fa fallire il test, salvo l'eccezione esplicita di",
         "# Graph e Table della v1.18.0 (kGraphTableV118). Una correzione condivisa",
         "# futura aggiorna la riga del file con il motivo nel rapporto della versione."]
for f in files:
    data = subprocess.run(["git", "-C", root, "show", f"{rev}:{f}"], capture_output=True, check=True).stdout
    lines.append(f"{f} {fnv(data):016x}")
open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print(len(files), "files")
