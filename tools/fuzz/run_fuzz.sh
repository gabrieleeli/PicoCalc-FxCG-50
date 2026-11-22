#!/usr/bin/env bash
# Esegue tutti gli harness (o quelli di FUZZ_ONLY) per N secondi ciascuno (predefinito 300), con un corpus
# iniziale dalle fixture e dai test. Uso: tools/fuzz/run_fuzz.sh <build> [secondi]
# I crash finiscono in <build>/artifacts/<harness>/ (crash-*, timeout-*).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="$(cd "${1:?cartella di build}" && pwd)"; T="${2:-300}"
C="$B/corpus"; mkdir -p "$C"/{expr,ws,csv,pcg,eq,ui,g3d,tf,te,prog,ea}
"$B/gen_seeds" "$C"
# Espressioni dalle stringhe dei test (primo byte: modo 0 = Comp).
grep -ohE '"[^"\\]{2,80}"' "$ROOT"/tests/*_test.cpp | sort -u | head -3000 | sed 's/^"//; s/"$//' |
  { i=0; while IFS= read -r l; do printf '\0%s' "$l" > "$C/expr/s$i"; printf '\2\0%s' "$l" > "$C/eq/q$i"; printf '\1%s' "$l" > "$C/g3d/e$i"; i=$((i+1)); done; }
printf '\0001,2,3\r\n4,5,6\r\n' > "$C/csv/a"; printf '\001"A";"B"\n1,5;2\n' > "$C/csv/b"
printf '\001\012\000\001\012' > "$C/ui/a"; printf '3\201\012' > "$C/ui/eact"
# FUZZ_ONLY="fuzz_a fuzz_b" esegue solo gli harness elencati (v1.15.0: quelli nuovi o toccati).
run() {  # harness, cartella del corpus, lunghezza massima
  if [[ -n "${FUZZ_ONLY:-}" && " ${FUZZ_ONLY} " != *" $1 "* ]]; then return 0; fi
  mkdir -p "$B/artifacts/$1"
  "$B/$1" "$C/$2" -fork=1 -ignore_crashes=1 -ignore_timeouts=1 -max_len="$3" -timeout=20 -rss_limit_mb=3000 \
    -max_total_time="$T" -artifact_prefix="$B/artifacts/$1/" > "$B/$1.log" 2>&1 || true
  echo "$1: $(grep -E '^#[0-9]+:' "$B/$1.log" | tail -n 1)"
}
# v1.15.0: celle e nomi legati (primo byte 0x40/0x80), file tipizzati, editor di testo, programmi C/C++/C#
printf '\100A1+Sum(A1:B3)' > "$C/expr/cells"; printf '\200raggio^2+x' > "$C/expr/bound"
printf '\000PCEA\001\000\000\000\004\000\000\000\000\000\000\000ciao' > "$C/tf/a"; printf '\001int main(){}' > "$C/tf/b"
printf 'ciao\001\002\004\005\012\016' > "$C/te/a"
for f in "$ROOT"/tests/fixtures/programs/*; do if [ -f "$f" ]; then cp "$f" "$C/prog/"; fi; done
printf '\000int main(){int a[3];for(int i=0;i<3;i++)a[i]=i;printf("%%d",a[2]);return 0;}' > "$C/prog/c1"
printf '\001#include <iostream>\nint main(){int n;std::cin>>n;std::cout<<n*2<<std::endl;}' > "$C/prog/cpp1"
printf '\002class P{static void Main(){var s=Console.ReadLine();Console.WriteLine($"{s}!");}}' > "$C/prog/cs1"
run fuzz_expr expr 300; run fuzz_exactdiff expr 200; run fuzz_workspace ws 10000; run fuzz_csv csv 30000
run fuzz_pcgraph pcg 8000; run fuzz_equation eq 400; run fuzz_ui ui 300; run fuzz_graph3d g3d 12000
run fuzz_typedfile tf 4000; run fuzz_texteditor te 600
[ -x "$B/fuzz_program" ] && run fuzz_program prog 2000
run fuzz_eactivity ea 6000                              # v1.16.0
ls "$B"/artifacts/*/ 2>/dev/null
