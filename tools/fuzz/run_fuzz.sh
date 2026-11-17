#!/usr/bin/env bash
# Esegue tutti gli harness per N secondi ciascuno (predefinito 300), con un corpus
# iniziale dalle fixture e dai test. Uso: tools/fuzz/run_fuzz.sh <build> [secondi]
# I crash finiscono in <build>/artifacts/<harness>/ (crash-*, timeout-*).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="$(cd "${1:?cartella di build}" && pwd)"; T="${2:-300}"
C="$B/corpus"; mkdir -p "$C"/{expr,ws,csv,pcg,eq,ui,conv}
"$B/gen_seeds" "$C"
# Espressioni dalle stringhe dei test (primo byte: modo 0 = Comp).
grep -ohE '"[^"\\]{2,80}"' "$ROOT"/tests/*_test.cpp | sort -u | head -3000 | sed 's/^"//; s/"$//' |
  { i=0; while IFS= read -r l; do printf '\0%s' "$l" > "$C/expr/s$i"; printf '\2\0%s' "$l" > "$C/eq/q$i"; i=$((i+1)); done; }
printf '\0001,2,3\r\n4,5,6\r\n' > "$C/csv/a"; printf '\001"A";"B"\n1,5;2\n' > "$C/csv/b"
printf '\001\012\000\001\012' > "$C/ui/a"
# v1.14.0: conversioni di formato (convertitore, da/a, parametro, testo) e simboli di unita'
i=0; for t in 'FF.A' '-101.1' '1994' 'MCMXCIV' '(IV)CCXXXIV' '3F800000' '2024-02-29' '2020-W53-1' '#E8601E' '120, 50, 50' \
  '0.1(6)' '2 1/3' '3+4i' '5<53' '45 27 51 N, 9 11 24 E' '12 30 15.5' '1:02:03' 'U+20AC' 'E2 82 AC' 'M' 'nm eV' 'kPa psi' 'TiB KiB'; do
  printf "\\$(printf %03o $i)\\001\\003%s" "$t" > "$C/conv/c$i"; i=$((i+1)); done
run() {  # harness, cartella del corpus, lunghezza massima
  mkdir -p "$B/artifacts/$1"
  "$B/$1" "$C/$2" -fork=1 -ignore_crashes=1 -ignore_timeouts=1 -max_len="$3" -timeout=20 -rss_limit_mb=3000 \
    -max_total_time="$T" -artifact_prefix="$B/artifacts/$1/" > "$B/$1.log" 2>&1 || true
  echo "$1: $(grep -E '^#[0-9]+:' "$B/$1.log" | tail -n 1)"
}
run fuzz_expr expr 300; run fuzz_exactdiff expr 200; run fuzz_workspace ws 10000; run fuzz_csv csv 30000
run fuzz_pcgraph pcg 8000; run fuzz_equation eq 400; run fuzz_ui ui 300; run fuzz_conversion conv 300
ls "$B"/artifacts/*/ 2>/dev/null
