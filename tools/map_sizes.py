#!/usr/bin/env python3
"""Dimensioni per modulo dalla mappa del linker GNU ld (v1.15.0).

Uso:
  tools/map_sizes.py <immagine.elf.map> [--top N] [--group] [--forbid FILE ...]

Somma i byte delle sezioni di ingresso caricate in flash (XIP 0x10000000) e in
SRAM (0x20000000) per file oggetto. --group raggruppa per area (src/apps,
src, third_party, SDK, librerie). --forbid termina con errore se un oggetto il
cui nome contiene una delle stringhe date e' presente nell'immagine con
almeno un byte: e' il controllo «nessun simbolo di una modalita' esclusa
nell'immagine» usato dal build del Pico 2 W.
"""
import re
import sys
from collections import defaultdict

FLASH = (0x10000000, 0x11000000)
SRAM = (0x20000000, 0x20082000)
LINE = re.compile(r"^\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*)$")
SECTION = re.compile(r"^ (\.[^\s]+)(?:\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*))?$")


def parse(path):
    flash = defaultdict(int)
    sram = defaultdict(int)
    in_map = False
    pending = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not in_map:
                if line.startswith("Linker script and memory map"):
                    in_map = True
                continue
            m = SECTION.match(line)
            if m:
                name, addr, size, obj = m.groups()
                if addr is None:
                    pending = name
                    continue
                record(name, addr, size, obj, flash, sram)
                pending = None
                continue
            if pending:
                m = LINE.match(line)
                if m:
                    record(pending, *m.groups(), flash, sram)
                pending = None
    return flash, sram


def record(name, addr, size, obj, flash, sram):
    a, s = int(addr, 16), int(size, 16)
    if s == 0 or name.startswith((".debug", ".comment", ".ARM.attributes", ".group")):
        return
    obj = obj.strip()
    if FLASH[0] <= a < FLASH[1]:
        flash[obj] += s
    elif SRAM[0] <= a < SRAM[1]:
        sram[obj] += s


def short(obj):
    obj = obj.replace("CMakeFiles/picocalc_fxai.dir/", "")
    if "pico-sdk" in obj or "pico_sdk" in obj or obj.startswith("/opt/") or "/lib/" in obj and "arm-none-eabi" not in obj:
        return obj
    return obj


def group(obj):
    o = obj.replace("CMakeFiles/picocalc_fxai.dir/", "")
    if o.startswith("src/apps/"):
        return "src/apps (controller)"
    if o.startswith("src/platform/"):
        return "src/platform (porta Pico)"
    if o.startswith("src/"):
        return "src (nuclei, motore, servizi)"
    if o.startswith("third_party/"):
        return "third_party (FatFs, LittleFS)"
    if "arm-none-eabi" in o or o.startswith("/usr/"):
        return "libc/libstdc++/libgcc"
    return "Pico SDK (cyw43, lwIP, mbedTLS, ...)"


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    top = 40
    grouped = False
    forbid = []
    i = 2
    while i < len(argv):
        if argv[i] == "--top":
            top = int(argv[i + 1]); i += 2
        elif argv[i] == "--group":
            grouped = True; i += 1
        elif argv[i] == "--forbid":
            i += 1
            while i < len(argv) and not argv[i].startswith("--"):
                forbid.append(argv[i]); i += 1
        else:
            print("argomento sconosciuto: " + argv[i], file=sys.stderr)
            return 2
    flash, sram = parse(path)
    if forbid:
        bad = sorted(o for o in set(flash) | set(sram) if any(f in o for f in forbid))
        for o in bad:
            print("ERRORE: oggetto escluso presente nell'immagine: %s (%d B flash, %d B SRAM)"
                  % (o, flash.get(o, 0), sram.get(o, 0)), file=sys.stderr)
        if bad:
            return 1
        print("map_sizes: nessun oggetto escluso nell'immagine (%d controllati)" % len(forbid))
        return 0
    if grouped:
        g_flash = defaultdict(int)
        g_sram = defaultdict(int)
        for o, s in flash.items():
            g_flash[group(o)] += s
        for o, s in sram.items():
            g_sram[group(o)] += s
        print("| Area | Flash (B) | SRAM (B) |")
        print("|---|---:|---:|")
        for k in sorted(g_flash, key=lambda k: -g_flash[k]):
            print("| %s | %d | %d |" % (k, g_flash[k], g_sram.get(k, 0)))
        print("| **Totale** | %d | %d |" % (sum(flash.values()), sum(sram.values())))
        return 0
    print("| Modulo | Flash (B) | SRAM (B) |")
    print("|---|---:|---:|")
    for o in sorted(flash, key=lambda k: -flash[k])[:top]:
        print("| `%s` | %d | %d |" % (o.replace("CMakeFiles/picocalc_fxai.dir/", ""), flash[o], sram.get(o, 0)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
