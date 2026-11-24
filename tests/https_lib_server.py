#!/usr/bin/env python3
"""Server HTTPS locale per le prove del gestore delle librerie (v1.20.0).

Uso: https_lib_server.py <cartella-repository> <cert.pem> <key.pem> <file-porta>

Serve i file del repository (index.json e le cartelle delle librerie) con
comportamenti scelti dal primo componente del percorso:
  /ok/<file>       risposta normale con Content-Length;
  /chunked/<file>  Transfer-Encoding: chunked a pezzi di 777 byte;
  /cut/<file>      i file delle librerie si interrompono a meta' (Content-Length
                   intero, poi la connessione si chiude): download interrotto;
  /slow/<file>     chunked con una pausa tra i pezzi (AC durante il download).
I file che non esistono danno 404. La porta scelta dal sistema va nel file
<file-porta>. Nessun percorso esce dalla cartella del repository.
"""
import http.server
import os
import ssl
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    root = "."

    def log_message(self, fmt, *args):  # niente rumore nei log dei test
        pass

    def do_GET(self):
        parts = self.path.lstrip("/").split("/", 1)
        mode = parts[0]
        rel = parts[1] if len(parts) > 1 else ""
        full = os.path.realpath(os.path.join(self.root, rel))
        if mode not in ("ok", "chunked", "cut", "slow") or not full.startswith(self.root + os.sep) or not os.path.isfile(full):
            body = b"non trovato\n"
            self.send_response(404)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        with open(full, "rb") as f:
            data = f.read()
        library_file = not rel.endswith("index.json") and not rel.endswith("library.json")
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Connection", "close")
        if mode in ("chunked", "slow"):
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for i in range(0, len(data), 777):
                piece = data[i:i + 777]
                self.wfile.write(b"%x\r\n" % len(piece) + piece + b"\r\n")
                self.wfile.flush()
                if mode == "slow":
                    time.sleep(0.05)
            self.wfile.write(b"0\r\n\r\n")
            return
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if mode == "cut" and library_file and len(data) > 1:
            self.wfile.write(data[: len(data) // 2])
            self.wfile.flush()
            self.close_connection = True
            return
        self.wfile.write(data)


def main():
    root, cert, key, port_file = sys.argv[1:5]
    Handler.root = os.path.realpath(root)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    with open(port_file + ".tmp", "w") as f:
        f.write(str(server.server_address[1]))
    os.replace(port_file + ".tmp", port_file)
    server.serve_forever()


if __name__ == "__main__":
    main()
