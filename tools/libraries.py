#!/usr/bin/env python3
"""Repository delle librerie dei programmi (v1.20.0): impronte e indice.

La cartella del repository (libraries/ nel progetto) contiene

    index.json
    <ling>/<nome>/<versione>/library.json      (c, cpp, cs; java e py dalle versioni seguenti)
    <ling>/<nome>/<versione>/<file della libreria>

library.json si scrive a mano (nome, versione, linguaggio, descrizione,
autore, licenza, namespace, firmware minimo, dipendenze); l'elenco "files"
con SHA-256 e dimensione e l'indice li calcola questo strumento con le stesse
regole del firmware (src/lib_manager.cpp): nomi [a-z0-9_-], percorsi relativi
senza "..", al piu' 64 file, 256 KB per file, 1 MB per libreria.

Uso:
  libraries.py update [CARTELLA]   riscrive "files" di ogni library.json e index.json
  libraries.py check [CARTELLA]    controlla che manifest e indice siano aggiornati (1 se no)
  libraries.py manifest CARTELLA_LIBRERIA   stampa il manifest aggiornato di una libreria
"""
import hashlib
import json
import os
import re
import sys

LANGS = ("c", "cpp", "cs", "java", "py")
MAX_FILES, MAX_FILE, MAX_LIB, MAX_MANIFEST, MAX_INDEX = 64, 256 * 1024, 1024 * 1024, 16 * 1024, 64 * 1024
NAME = re.compile(r"^[a-z][a-z0-9_-]{0,31}$")
VERSION = re.compile(r"^(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$")
SEGMENT = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]{0,39}$")
FIELD_ORDER = ("name", "version", "language", "description", "author", "license", "namespace", "minFirmware", "dependencies", "files")
RANGE_TERM = re.compile(r"^(\*|[\^~]?(?:>=|<=|>|<|=)?(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4}))$")


class RepoError(Exception):
    pass


def safe_relative(path):
    if not path or len(path) > 120 or path.startswith("/") or path.endswith("/"):
        return False
    parts = path.split("/")
    return len(parts) <= 4 and all(SEGMENT.match(p) and p not in (".", "..") for p in parts)


def valid_range(r):
    terms = r.split()
    if not r or len(r) > 64 or not terms or len(terms) > 4:
        return False
    for t in terms:
        if not RANGE_TERM.match(t) or (t[0] in "^~" and t[1:2] in "<>="):
            return False
    return True


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def library_files(folder):
    """File della libreria (tutti tranne library.json), percorsi con '/' in ordine."""
    out = []
    for root, dirs, files in os.walk(folder):
        dirs[:] = sorted(d for d in dirs if not d.startswith("."))
        for f in sorted(files):
            if f.startswith("."):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, folder).replace(os.sep, "/")
            if rel.lower() == "library.json":
                continue
            out.append((rel, full))
    return out


def manifest_for(folder, expect=None):
    """Manifest aggiornato (dict) della libreria in `folder`; expect = (ling, nome, versione)."""
    path = os.path.join(folder, "library.json")
    try:
        with open(path, "rb") as f:
            raw = f.read()
        data = json.loads(raw.decode("utf-8-sig"))
    except (OSError, ValueError) as e:
        raise RepoError("%s: library.json illeggibile o non valido (%s)" % (folder, e))
    if not isinstance(data, dict):
        raise RepoError(path + ": deve essere un oggetto")
    for key in ("name", "version", "language"):
        if not isinstance(data.get(key), str):
            raise RepoError("%s: campo '%s' mancante o non stringa" % (path, key))
    if not NAME.match(data["name"]):
        raise RepoError(path + ": nome non valido: " + data["name"])
    if not VERSION.match(data["version"]):
        raise RepoError(path + ": versione non valida: " + data["version"])
    if data["language"] not in LANGS:
        raise RepoError(path + ": linguaggio sconosciuto: " + data["language"])
    if expect and (data["language"], data["name"], data["version"]) != expect:
        raise RepoError("%s: il manifest dice %s/%s/%s, la cartella %s/%s/%s" % ((path,) + (data["language"], data["name"], data["version"]) + expect))
    if "minFirmware" in data and not VERSION.match(str(data["minFirmware"])):
        raise RepoError(path + ": minFirmware non valido")
    deps = data.get("dependencies", {})
    if not isinstance(deps, dict) or len(deps) > 16:
        raise RepoError(path + ": 'dependencies' deve essere un oggetto (al piu' 16)")
    for n, r in deps.items():
        if not NAME.match(n) or not isinstance(r, str) or not valid_range(r):
            raise RepoError("%s: dipendenza non valida: %s %r" % (path, n, r))
        if n == data["name"]:
            raise RepoError(path + ": la libreria dipende da se stessa")
    files = []
    total = 0
    seen = set()
    for rel, full in library_files(folder):
        if not safe_relative(rel):
            raise RepoError("%s: percorso non ammesso: %s" % (folder, rel))
        if rel.lower() in seen:
            raise RepoError("%s: file ripetuto (maiuscole): %s" % (folder, rel))
        seen.add(rel.lower())
        with open(full, "rb") as f:
            content = f.read()
        if len(content) > MAX_FILE:
            raise RepoError("%s: %s supera 256 KB" % (folder, rel))
        total += len(content)
        files.append({"path": rel, "sha256": sha256(content), "size": len(content)})
    if not files:
        raise RepoError(folder + ": nessun file nella libreria")
    if len(files) > MAX_FILES:
        raise RepoError(folder + ": troppi file (massimo 64)")
    if total > MAX_LIB:
        raise RepoError(folder + ": la libreria supera 1 MB")
    out = {}
    for key in FIELD_ORDER:
        if key == "files":
            out["files"] = files
        elif key == "dependencies":
            out["dependencies"] = dict(deps)
        elif key in data:
            out[key] = data[key]
    for key in data:
        if key not in out:
            raise RepoError("%s: campo sconosciuto '%s'" % (path, key))
    return out


def manifest_text(manifest):
    text = json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    if len(text.encode("utf-8")) > MAX_MANIFEST:
        raise RepoError(manifest["name"] + ": library.json supera 16 KB")
    return text


def scan(repo):
    """[(ling, nome, versione, cartella)] in ordine."""
    out = []
    for lang in LANGS:
        ld = os.path.join(repo, lang)
        if not os.path.isdir(ld):
            continue
        for name in sorted(os.listdir(ld)):
            nd = os.path.join(ld, name)
            if name.startswith(".") or not os.path.isdir(nd):
                continue
            for ver in sorted(os.listdir(nd), key=lambda v: [int(x) if x.isdigit() else -1 for x in v.split(".")]):
                vd = os.path.join(nd, ver)
                if os.path.isdir(vd) and not ver.startswith("."):
                    out.append((lang, name, ver, vd))
    return out


def build(repo):
    """{percorso: testo} di tutti i manifest e dell'indice aggiornati."""
    texts = {}
    entries = []
    libs = scan(repo)
    known = {(l, n) for l, n, _, _ in libs}
    for lang, name, ver, folder in libs:
        m = manifest_for(folder, (lang, name, ver))
        for dep in m["dependencies"]:
            if (lang, dep) not in known and not (lang == "cpp" and ("c", dep) in known):
                raise RepoError("%s/%s %s: dipendenza %s assente dal repository" % (lang, name, ver, dep))
        text = manifest_text(m)
        texts[os.path.join(folder, "library.json")] = text
        e = {"name": name, "version": ver, "language": lang, "path": "%s/%s/%s" % (lang, name, ver)}
        if m.get("description"):
            e["description"] = m["description"]
        e["manifestSha256"] = sha256(text.encode("utf-8"))
        e["manifestSize"] = len(text.encode("utf-8"))
        if m.get("minFirmware"):
            e["minFirmware"] = m["minFirmware"]
        e["dependencies"] = m["dependencies"]
        entries.append(e)
    index = json.dumps({"version": 1, "libraries": entries}, indent=2, ensure_ascii=False) + "\n"
    if len(index.encode("utf-8")) > MAX_INDEX:
        raise RepoError("index.json supera 64 KB")
    texts[os.path.join(repo, "index.json")] = index
    return texts


def main(argv):
    if len(argv) < 2 or argv[1] not in ("update", "check", "manifest"):
        print(__doc__)
        return 2
    try:
        if argv[1] == "manifest":
            folder = argv[2]
            sys.stdout.write(manifest_text(manifest_for(folder)))
            return 0
        repo = argv[2] if len(argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "libraries")
        texts = build(repo)
        stale = []
        for path, text in sorted(texts.items()):
            try:
                with open(path, "r", encoding="utf-8") as f:
                    have = f.read()
            except OSError:
                have = None
            if have != text:
                stale.append(path)
                if argv[1] == "update":
                    with open(path + ".tmp", "w", encoding="utf-8", newline="\n") as f:
                        f.write(text)
                    os.replace(path + ".tmp", path)
        n = len(texts) - 1
        if argv[1] == "check":
            for p in stale:
                print("non aggiornato: " + os.path.relpath(p, repo))
            print("%d librerie, %s" % (n, "da aggiornare (tools/libraries.py update)" if stale else "manifest e indice aggiornati"))
            return 1 if stale else 0
        print("%d librerie, %d file riscritti" % (n, len(stale)))
        return 0
    except RepoError as e:
        print("ERRORE: " + str(e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
