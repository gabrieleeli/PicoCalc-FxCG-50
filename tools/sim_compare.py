#!/usr/bin/env python3
"""Confronto pixel per pixel degli screenshot del simulatore (v1.15.0).

Uso:
  tools/sim_compare.py <picocalc_sim> <cartella_temporanea> [script.txt ...]
  tools/sim_compare.py --compress <png_nuovo> <png_destinazione>

Senza script esegue tutti i tests/sim/*.txt. Ogni screenshot prodotto viene
confrontato con quello omonimo in docs/screenshots: devono avere le stesse
dimensioni e gli stessi pixel RGB. Gli screenshot che non esistono ancora in
docs/screenshots sono elencati come NUOVI (vanno compressi con --compress,
che scrive un PNG a palette con compressione 9 e verifica che i pixel restino
identici). Richiede Pillow.
"""
import os
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, "docs", "screenshots")


def pixels(path):
    with Image.open(path) as im:
        rgb = im.convert("RGB")
        return rgb.size, rgb.tobytes()


def compress(src, dst):
    with Image.open(src) as im:
        rgb = im.convert("RGB")
        colors = rgb.getcolors(256)
        if colors is not None:
            pal = rgb.quantize(colors=len(colors), method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
            out = pal if pal.convert("RGB").tobytes() == rgb.tobytes() else rgb
        else:
            out = rgb
        out.save(dst, optimize=True, compress_level=9)
    if pixels(src) != pixels(dst):
        print("ERRORE: compressione con perdita per " + dst, file=sys.stderr)
        return 1
    return 0


def main(argv):
    if len(argv) == 4 and argv[1] == "--compress":
        return compress(argv[2], argv[3])
    if len(argv) < 3:
        print(__doc__)
        return 2
    sim, out = argv[1], argv[2]
    scripts = argv[3:] or sorted(
        os.path.join(ROOT, "tests", "sim", f) for f in os.listdir(os.path.join(ROOT, "tests", "sim")) if f.endswith(".txt"))
    os.makedirs(out, exist_ok=True)
    same = differ = new = 0
    for script in scripts:
        shots = [l.split()[1] for l in open(script, encoding="utf-8") if l.startswith("shot ")]
        r = subprocess.run([sim, out, script], capture_output=True, text=True)
        if r.returncode != 0:
            print("ERRORE: il simulatore e' terminato con %d su %s\n%s" % (r.returncode, script, r.stderr), file=sys.stderr)
            return 1
        for shot in shots:
            produced = os.path.join(out, shot)
            reference = os.path.join(SHOTS, shot)
            if not os.path.exists(produced):
                print("MANCANTE %s (%s)" % (shot, os.path.basename(script)))
                differ += 1
            elif not os.path.exists(reference):
                print("NUOVO    %s (%s)" % (shot, os.path.basename(script)))
                new += 1
            elif pixels(produced) != pixels(reference):
                print("DIVERSO  %s (%s)" % (shot, os.path.basename(script)))
                differ += 1
            else:
                same += 1
    print("sim_compare: %d identici, %d nuovi, %d diversi" % (same, new, differ))
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
