#!/usr/bin/env bash
# v1.20.0: installazione delle librerie via HTTPS contro un server locale.
# Crea una CA di prova e il certificato del server (SAN DNS:localhost), una
# seconda CA estranea, il repository di prova, avvia tests/https_lib_server.py
# e lancia le prove di https_fetch_host (OpenSSL come trasporto del PC).
# Uso: https_lib_test.sh <https_fetch_host> <cartella-di-lavoro>
set -euo pipefail
BIN=$1; W=$2
HERE=$(cd "$(dirname "$0")" && pwd)
rm -rf "$W"; mkdir -p "$W"
cd "$W"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=PicoCalc test CA" -keyout ca.key -out ca.pem >/dev/null 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=Altra CA" -keyout other.key -out other.pem >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -subj "/CN=localhost" -keyout server.key -out server.csr >/dev/null 2>&1
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\n' > ext.cnf
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 2 -extfile ext.cnf -out server.pem >/dev/null 2>&1
"$BIN" --make-repo "$W/repo"
python3 "$HERE/https_lib_server.py" "$W/repo" server.pem server.key "$W/port" 2>"$W/server.log" &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT
for _ in $(seq 1 100); do [ -s "$W/port" ] && break; sleep 0.05; done
[ -s "$W/port" ] || { echo "server HTTPS non avviato"; cat "$W/server.log"; exit 1; }
"$BIN" --run "$(cat "$W/port")" "$W/ca.pem" "$W/other.pem" "$W/repo"
