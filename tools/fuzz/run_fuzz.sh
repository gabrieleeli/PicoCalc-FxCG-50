#!/usr/bin/env bash
# Esegue tutti gli harness (o quelli di FUZZ_ONLY) per N secondi ciascuno (predefinito 300), con un corpus
# iniziale dalle fixture e dai test. Uso: tools/fuzz/run_fuzz.sh <build> [secondi]
# I crash finiscono in <build>/artifacts/<harness>/ (crash-*, timeout-*).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="$(cd "${1:?cartella di build}" && pwd)"; T="${2:-300}"
C="$B/corpus"; mkdir -p "$C"/{expr,ws,csv,pcg,eq,ui,g3d,tf,te,prog,ea,sheet,link,modes,py,stats}
"$B/gen_seeds" "$C"
# Espressioni dalle stringhe dei test (primo byte: modo 0 = Comp).
grep -ohE '"[^"\\]{2,80}"' "$ROOT"/tests/*_test.cpp | sort -u | head -3000 | sed 's/^"//; s/"$//' |
  { i=0; while IFS= read -r l; do printf '\0%s' "$l" > "$C/expr/s$i"; printf '\2\0%s' "$l" > "$C/eq/q$i"; printf '\1%s' "$l" > "$C/g3d/e$i"; i=$((i+1)); done; }
printf '\0001,2,3\r\n4,5,6\r\n' > "$C/csv/a"; printf '\001"A";"B"\n1,5;2\n' > "$C/csv/b"
printf '\001\012\000\001\012' > "$C/ui/a"; printf '3\201\012' > "$C/ui/eact"
# FUZZ_ONLY="fuzz_a fuzz_b" esegue solo gli harness elencati (v1.15.0: quelli nuovi o toccati).
run() {  # harness, cartella del corpus, lunghezza massima
  if [[ -n "${FUZZ_ONLY:-}" && " ${FUZZ_ONLY} " != *" $1 "* ]]; then return 0; fi
  local log="${4:-$1}"
  mkdir -p "$B/artifacts/$log"
  "$B/$1" "$C/$2" -fork=1 -ignore_crashes=1 -ignore_timeouts=1 -max_len="$3" -timeout=20 -rss_limit_mb=3000 \
    -max_total_time="$T" -artifact_prefix="$B/artifacts/$log/" > "$B/$log.log" 2>&1 || true
  echo "$log: $(grep -E '^#[0-9]+:' "$B/$log.log" | tail -n 1)"
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
run fuzz_eactivity ea 6000                              # v1.16.0: documenti e controller
FXAI_FUZZ_EACT_DOCS=1 run fuzz_eactivity ea 30000 fuzz_eactivity_docs   # solo documenti .pceact
# v1.17.0 (solo Pico Plus 2 W): formule del foglio, protocollo di Link, decoder delle modalita' Base, MicroPython
printf '\000A1=2\nB1==A1*3\nC1==Sum(A1:B1)+$A$1\nD1==D1\n!x\001\002\n#=A1+1\n' > "$C/sheet/a"
printf '\000A1==B1\nB1==Mean(A1:A9)/0\nZ999==Z998\n' > "$C/sheet/b"
printf '\000log\176P\005\000\000\000\000' > "$C/link/a"; printf '\001\001\001\000\001\021\000PICOCALC/PY/HI.py\002\000\000\000hi' > "$C/link/b"
printf '\002\176P\002\000\000\012\000\005\000\000\000\000\000\000\000\001F' > "$C/link/c"
printf '\002\001\003\000\000\000\360?\000\000\000\000\000\000\360?' > "$C/modes/conic"; printf '\013// pcprog: C#\nclass P { /* x */ }\n' > "$C/modes/prog"
printf '\014\010\000\000\000\001\002\000AX\101\001\001\000\000\000\000\000\000\360?\000\000\000\000\000\000\024@\000\000\000\000\000\000\360?' > "$C/modes/dynafile"
printf '\015\005\000\000\000\001\000\000\000\000\011\0002rec_an+1\000\000\000\000\000\000\000\000\000\000\000\000\000\000$@\000\000\000\000\000\000\000\000\000\000\000\000\000\000\360?' > "$C/modes/recfile"
{ printf '\016\164\000\000\000\001\000'; head -c 170 /dev/zero; } > "$C/modes/finfile"
for f in "$ROOT"/tests/fixtures/fuzz/fuzz_modes_*; do if [ -f "$f" ]; then cp "$f" "$C/modes/"; fi; done   # crash gia' trovati (primo byte = caso)
printf 'print(1+2)\n' > "$C/py/a"; printf 'def f(n):\n  return f(n+1)\nf(0)\n' > "$C/py/b"; printf 'while True:\n  pass\n' > "$C/py/c"
printf 'import math, casioplot\ncasioplot.set_pixel(1,2,(1,2,3))\nprint(math.sqrt(2), input())\n' > "$C/py/d"
[ -x "$B/fuzz_sheet" ] && run fuzz_sheet sheet 4000
[ -x "$B/fuzz_link" ] && run fuzz_link link 8000
[ -x "$B/fuzz_modes" ] && run fuzz_modes modes 6000
[ -x "$B/fuzz_python" ] && run fuzz_python py 2000
# v1.18.0 (solo Pico Plus 2 W): nucleo di Statistics, distribuzioni, sezioni 0x0101 e 0x0117
printf '\002\012\001\002\003\004\005\006\007\010\011\012\012\002\003\005\007\013\015\021\023\027\031\001\011' > "$C/stats/reg"
printf '\003\001\001\010\001\002\003\004\005\006\007\010\005\012\002\003\004\005\006' > "$C/stats/test"
printf '\004\014\001\001\001\002\002\002\003\003\003\001\002\003' > "$C/stats/anova"
printf '\005\001\000\001\000\002\001\003' > "$C/stats/dist"; printf '\001\001\001\000\000\000\000\000\000\000\000\000\000\000\000\000' > "$C/stats/dmode"
[ -x "$B/fuzz_stats" ] && run fuzz_stats stats 2000
ls "$B"/artifacts/*/ 2>/dev/null
