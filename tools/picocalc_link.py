#!/usr/bin/env python3
"""PicoCalc FxCG-50 Link (v1.17.0): trasferimenti con il PC sulla USB seriale.

Lo stesso protocollo di src/link_protocol.cpp (formato PROPRIO del progetto,
non il protocollo Link Casio): trame con CRC-32, confermate una per una.

    0x7E 'P' | tipo u8 | seq u16 | lunghezza u16 (<= 1024) | dati | CRC-32 u32

BEGIN (seq 0: totale u32, CRC u32 del pacchetto, lunghezza del nome u8, nome)
-> ACK 0; DATA seq 1..n -> ACK; END -> ACK (CRC del pacchetto verificato) o NAK.
Il pacchetto (.pclink) contiene il workspace e/o file dell'utente:

    u8 versione (1) | u16 voci | per voce: u8 tipo (0 workspace, 1 file),
    u16 + percorso ("PICOCALC/PY/HI.py"), u32 + dati

Uso:
  picocalc_link.py receive --port /dev/ttyACM0 -o ricevuto.pclink [--extract CARTELLA]
      (sul dispositivo: Link > SEND)
  picocalc_link.py send --port /dev/ttyACM0 FILE[:PERCORSO] ... [--package x.pclink]
      (sul dispositivo: Link > RECEIVE; PERCORSO predefinito: dalla estensione)
  picocalc_link.py list x.pclink
  picocalc_link.py send-library --port /dev/ttyACM0 CARTELLA_LIBRERIA
      (v1.20.0; sul dispositivo: Link > RECEIVE, poi Program > LIB > LOCAL per
      installarla con il controllo delle impronte)

--command "programma argomenti" usa lo stdin/stdout di un processo invece della
porta seriale (test di andata e ritorno con tests/link_peer).
"""
import argparse
import os
import select
import shlex
import struct
import subprocess
import sys
import time
import zlib

SYNC0, SYNC1 = 0x7E, ord("P")
MAX_PAYLOAD = 1024
BEGIN, DATA, END, ACK, NAK, ABORT = 2, 3, 4, 5, 6, 7
RETRY_S = 1.0
MAX_RETRIES = 5
IDLE_S = 15.0
FILE_MAGIC = b"PCLK"
# Cartelle dei tipi di file (src/typed_files.cpp) per le estensioni note.
FOLDERS = {".pceact": "EACT", ".pcprog": "PROG", ".c": "PROG", ".cpp": "PROG", ".cs": "PROG", ".pcsheet": "SHEET",
           ".py": "PY", ".pcgeo": "GEOM", ".pcplot": "PPLOT", ".pcdyna": "DYNA", ".pcrecur": "RECUR",
           ".pcconic": "CONIC", ".pcfin": "FIN", ".pcpict": "PICT", ".csv": "LIST"}


class LinkError(Exception):
    pass


def crc32(data, seed=0):
    return zlib.crc32(data, seed) & 0xFFFFFFFF


def encode_frame(ftype, seq, payload=b""):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload oltre 1024 byte")
    body = struct.pack("<BHH", ftype, seq, len(payload)) + payload
    return bytes([SYNC0, SYNC1]) + body + struct.pack("<I", crc32(body))


class Decoder:
    """Decodificatore a flusso: scarta il rumore e le trame con il CRC sbagliato."""

    def __init__(self):
        self.buf = bytearray()
        self.discarded = 0

    def push(self, data):
        self.buf.extend(data)

    def frames(self):
        out = []
        while True:
            i = 0
            while i + 1 < len(self.buf) and not (self.buf[i] == SYNC0 and self.buf[i + 1] == SYNC1):
                i += 1
            if i + 1 >= len(self.buf):
                keep = 1 if self.buf and self.buf[-1] == SYNC0 else 0
                self.discarded += len(self.buf) - keep
                del self.buf[:len(self.buf) - keep]
                return out
            if i:
                self.discarded += i
                del self.buf[:i]
            if len(self.buf) < 7:
                return out
            ftype, seq, length = struct.unpack_from("<BHH", self.buf, 2)
            if not BEGIN <= ftype <= ABORT or length > MAX_PAYLOAD:
                del self.buf[0]
                self.discarded += 1
                continue
            if len(self.buf) < 11 + length:
                return out
            (crc,) = struct.unpack_from("<I", self.buf, 7 + length)
            if crc32(bytes(self.buf[2:7 + length])) != crc:
                del self.buf[0]
                self.discarded += 1
                continue
            out.append((ftype, seq, bytes(self.buf[7:7 + length])))
            del self.buf[:11 + length]


# ---------------------------------------------------------------- pacchetti
def valid_path(path):
    if len(path) < 10 or len(path) > 96 or not path.startswith("PICOCALC/"):
        return False
    if ".." in path or "//" in path or path.endswith("/"):
        return False
    return all(c.isalnum() or c in "/._-{}'~" for c in path) and path.isascii()


def encode_package(entries):
    out = bytearray(struct.pack("<BH", 1, len(entries)))
    for kind, path, data in entries:
        p = path.encode("ascii")
        out += struct.pack("<BH", kind, len(p)) + p + struct.pack("<I", len(data)) + data
    return bytes(out)


def decode_package(data):
    if len(data) < 3 or data[0] != 1:
        raise LinkError("pacchetto non valido")
    (count,) = struct.unpack_from("<H", data, 1)
    i, entries, workspaces = 3, [], 0
    for _ in range(count):
        if i + 3 > len(data):
            raise LinkError("pacchetto troncato")
        kind, plen = struct.unpack_from("<BH", data, i)
        i += 3
        path = data[i:i + plen].decode("ascii")
        i += plen
        (dlen,) = struct.unpack_from("<I", data, i)
        i += 4
        payload = data[i:i + dlen]
        if len(payload) != dlen or kind > 1:
            raise LinkError("pacchetto troncato")
        i += dlen
        if kind == 0:
            workspaces += 1
            if path or workspaces > 1:
                raise LinkError("voce workspace non valida")
        elif not valid_path(path):
            raise LinkError("percorso non valido: " + path)
        entries.append((kind, path, payload))
    if i != len(data):
        raise LinkError("byte in piu' dopo il pacchetto")
    return entries


def encode_file(package):
    """File .pclink: intestazione dei file tipizzati (magia, versione 1, flag, lunghezza, CRC)."""
    return FILE_MAGIC + struct.pack("<HHII", 1, 0, len(package), crc32(package)) + package


def decode_file(raw):
    if len(raw) < 16 or raw[:4] != FILE_MAGIC:
        raise LinkError("non e' un file .pclink")
    version, flags, length, crc = struct.unpack_from("<HHII", raw, 4)
    body = raw[16:]
    if version != 1 or flags != 0 or length != len(body) or crc32(body) != crc:
        raise LinkError("file .pclink danneggiato")
    return body


# ---------------------------------------------------------------- canali
class SerialPort:
    """Porta seriale POSIX in modalita' raw (senza dipendenze); pyserial se c'e'."""

    def __init__(self, path):
        try:
            import serial  # type: ignore
            self.ser = serial.Serial(path, 115200, timeout=0)
            self.fd = None
        except ImportError:
            import termios
            import tty
            self.ser = None
            self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            tty.setraw(self.fd)
            attrs = termios.tcgetattr(self.fd)
            attrs[3] &= ~termios.ECHO
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def read(self, timeout):
        if self.ser:
            self.ser.timeout = timeout
            return self.ser.read(4096)
        r, _, _ = select.select([self.fd], [], [], timeout)
        return os.read(self.fd, 4096) if r else b""

    def write(self, data):
        if self.ser:
            self.ser.write(data)
            self.ser.flush()
        else:
            os.write(self.fd, data)

    def close(self):
        if self.ser:
            self.ser.close()
        else:
            os.close(self.fd)


class ProcessPort:
    """Lo stdin/stdout di un processo (tests/link_peer) al posto della USB."""

    def __init__(self, command):
        self.proc = subprocess.Popen(shlex.split(command), stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)

    def read(self, timeout):
        r, _, _ = select.select([self.proc.stdout], [], [], timeout)
        return os.read(self.proc.stdout.fileno(), 4096) if r else b""

    def write(self, data):
        self.proc.stdin.write(data)
        self.proc.stdin.flush()

    def close(self):
        try:
            self.proc.stdin.close()
        except OSError:
            pass
        self.proc.wait(timeout=10)


# ---------------------------------------------------------------- trasferimenti
def send(port, package, name, progress=None, noise=b""):
    chunks = [package[i:i + MAX_PAYLOAD] for i in range(0, len(package), MAX_PAYLOAD)]
    last = len(chunks) + 1
    dec = Decoder()
    begin = struct.pack("<II", len(package), crc32(package)) + bytes([len(name[:64])]) + name[:64].encode("ascii", "replace")
    frames = [encode_frame(BEGIN, 0, begin)] + [encode_frame(DATA, k + 1, c) for k, c in enumerate(chunks)] + [encode_frame(END, last)]
    seq = 0
    while seq <= last:
        for attempt in range(MAX_RETRIES + 1):
            port.write(noise + frames[seq])
            deadline = time.monotonic() + RETRY_S
            answer = None
            while time.monotonic() < deadline and answer is None:
                dec.push(port.read(0.05))
                for ftype, fseq, payload in dec.frames():
                    if ftype == ABORT:
                        raise LinkError("annullato dal dispositivo: " + payload.decode("ascii", "replace"))
                    if fseq == seq and ftype in (ACK, NAK):
                        answer = ftype
            if answer == ACK:
                break
            if answer == NAK and seq == last:
                raise LinkError("il dispositivo ha rifiutato il CRC del pacchetto")
        else:
            raise LinkError("nessuna risposta dal dispositivo (Time Out)")
        if progress:
            progress(min(len(package), seq * MAX_PAYLOAD), len(package))
        seq += 1


def receive(port, max_bytes=8 * 1024 * 1024, progress=None, wait_s=None):
    dec = Decoder()
    started, expected, crc, nxt, name, data = False, 0, 0, 1, "", bytearray()
    last = time.monotonic()
    start = last

    def reply(t, s):
        port.write(encode_frame(t, s))

    while True:
        chunk = port.read(0.05)
        dec.push(chunk)
        frames = dec.frames()
        now = time.monotonic()
        if not frames:
            if started and now - last > IDLE_S:
                raise LinkError("trasferimento interrotto (Time Out)")
            if not started and wait_s is not None and now - start > wait_s:
                raise LinkError("nessun invio dal dispositivo")
            continue
        last = now
        for ftype, seq, payload in frames:
            if ftype == BEGIN:
                if len(payload) < 9 or len(payload) != 9 + payload[8]:
                    reply(NAK, seq)
                    continue
                if started and nxt > 1:
                    reply(ACK, 0)
                    continue
                expected, crc = struct.unpack_from("<II", payload)
                if expected > max_bytes:
                    port.write(encode_frame(ABORT, 0, b"Memory ERROR"))
                    raise LinkError("pacchetto troppo grande")
                name = payload[9:].decode("ascii", "replace")
                started, nxt, data = True, 1, bytearray()
                reply(ACK, 0)
            elif ftype == DATA:
                if not started:
                    reply(NAK, seq)
                elif seq < nxt:
                    reply(ACK, seq)
                elif seq != nxt or len(data) + len(payload) > expected:
                    reply(NAK, seq)
                else:
                    data += payload
                    reply(ACK, seq)
                    nxt += 1
                    if progress:
                        progress(len(data), expected)
            elif ftype == END:
                if not started:
                    reply(NAK, seq)
                    continue
                if len(data) != expected or crc32(bytes(data)) != crc:
                    reply(NAK, seq)
                    raise LinkError("CRC del pacchetto errato")
                reply(ACK, seq)
                return name, bytes(data)
            elif ftype == ABORT:
                raise LinkError("annullato dal dispositivo: " + payload.decode("ascii", "replace"))


# ---------------------------------------------------------------- comandi
def device_path(local, explicit):
    if explicit:
        return explicit
    base = os.path.basename(local)
    ext = os.path.splitext(base)[1].lower()
    if ext not in FOLDERS:
        raise LinkError("estensione sconosciuta, indicare il percorso: FILE:PICOCALC/<CARTELLA>/" + base)
    return "PICOCALC/" + FOLDERS[ext] + "/" + base


IMPORT_ROOT = "PICOCALC/LIB/IMPORT"


def library_entries(folder):
    """Voci del pacchetto per una libreria (v1.20.0): i file e il manifest con le
    impronte calcolate (tools/libraries.py) in PICOCALC/LIB/IMPORT/<ling>/<nome>/<versione>/.
    Sul dispositivo LIB > LOCAL le trova e le installa verificando dimensioni e SHA-256."""
    import libraries  # noqa: E402 (stessa cartella)
    try:
        manifest = libraries.manifest_for(folder)
        text = libraries.manifest_text(manifest)
    except libraries.RepoError as e:
        raise LinkError(str(e))
    base = "%s/%s/%s/%s" % (IMPORT_ROOT, manifest["language"], manifest["name"], manifest["version"])
    entries = []
    for f in manifest["files"]:
        path = base + "/" + f["path"]
        if not valid_path(path):
            raise LinkError("percorso troppo lungo o non valido sul dispositivo: " + path)
        with open(os.path.join(folder, *f["path"].split("/")), "rb") as fh:
            data = fh.read()
        if libraries.sha256(data) != f["sha256"]:
            raise LinkError("file cambiato durante la preparazione: " + f["path"])
        entries.append((1, path, data))
    entries.append((1, base + "/library.json", text.encode("utf-8")))
    return manifest, entries


def open_port(args):
    if args.command:
        return ProcessPort(args.command)
    if not args.port:
        raise LinkError("--port o --command")
    return SerialPort(args.port)


def show(entries):
    for kind, path, data in entries:
        print(("workspace" if kind == 0 else path) + "  " + str(len(data)) + " B")


def main(argv=None):
    ap = argparse.ArgumentParser(description="PicoCalc FxCG-50 Link (USB seriale, formato proprio con CRC-32)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("send", "receive"):
        p = sub.add_parser(name)
        p.add_argument("--port")
        p.add_argument("--command")
        p.add_argument("--quiet", action="store_true")
    s = sub.choices["send"]
    s.add_argument("files", nargs="*", help="FILE[:PERCORSO sul dispositivo]")
    s.add_argument("--workspace", help="record del workspace da inviare")
    s.add_argument("--package", help="file .pclink da inviare cosi' com'e'")
    s.add_argument("--name", default="PC")
    s.add_argument("--noise", action="store_true", help="test: rumore prima di ogni trama")
    r = sub.choices["receive"]
    r.add_argument("-o", "--output", required=True, help="file .pclink scritto")
    r.add_argument("--extract", help="cartella in cui scrivere i file del pacchetto")
    r.add_argument("--wait", type=float, default=None, help="secondi di attesa dell'invio")
    lp = sub.add_parser("list")
    lp.add_argument("package")
    sl = sub.add_parser("send-library", help="invia una libreria (cartella con library.json) in PICOCALC/LIB/IMPORT")
    sl.add_argument("folder")
    sl.add_argument("--port")
    sl.add_argument("--command")
    sl.add_argument("--quiet", action="store_true")
    sl.add_argument("--name", default="LIB")
    args = ap.parse_args(argv)
    try:
        if args.cmd == "list":
            with open(args.package, "rb") as f:
                show(decode_package(decode_file(f.read())))
            return 0
        if args.cmd == "send-library":
            manifest, entries = library_entries(args.folder)
            progress = None if args.quiet else (lambda done, total: print("\r%3d%%" % (100 * done // max(1, total)), end="", file=sys.stderr))
            port = open_port(args)
            try:
                send(port, encode_package(entries), args.name, progress, b"")
            finally:
                port.close()
            if progress:
                print("", file=sys.stderr)
            print("inviata %s %s (%s, %d file): sul dispositivo Program > LIB > LOCAL" % (manifest["name"], manifest["version"], manifest["language"], len(entries) - 1))
            return 0
        progress = None if args.quiet else (lambda done, total: print("\r%3d%%" % (100 * done // max(1, total)), end="", file=sys.stderr))
        port = open_port(args)
        try:
            if args.cmd == "send":
                if args.package:
                    with open(args.package, "rb") as f:
                        package = decode_file(f.read())
                    decode_package(package)
                else:
                    entries = []
                    if args.workspace:
                        with open(args.workspace, "rb") as f:
                            entries.append((0, "", f.read()))
                    for spec in args.files:
                        local, _, target = spec.partition(":")
                        path = device_path(local, target)
                        if not valid_path(path):
                            raise LinkError("percorso non valido: " + path)
                        with open(local, "rb") as f:
                            entries.append((1, path, f.read()))
                    if not entries:
                        raise LinkError("niente da inviare")
                    package = encode_package(entries)
                send(port, package, args.name, progress, b"log [DIAG] rumore\n" if args.noise else b"")
                if progress:
                    print("", file=sys.stderr)
            else:
                name, package = receive(port, progress=progress, wait_s=args.wait)
                entries = decode_package(package)
                with open(args.output, "wb") as f:
                    f.write(encode_file(package))
                if progress:
                    print("", file=sys.stderr)
                print("ricevuto " + name)
                show(entries)
                if args.extract:
                    for kind, path, data in entries:
                        target = os.path.join(args.extract, "workspace.bin" if kind == 0 else path)
                        os.makedirs(os.path.dirname(target), exist_ok=True)
                        with open(target, "wb") as f:
                            f.write(data)
        finally:
            port.close()
        return 0
    except LinkError as e:
        print("ERRORE: " + str(e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
