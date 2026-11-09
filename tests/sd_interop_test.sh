#!/usr/bin/env bash
# microSD interoperability: the firmware's storage code (FatFs + file store,
# through tools/sd_image_tool) writes GRAPH projects and files into FAT16,
# FAT32 and exFAT images made by mkfs.fat / mkfs.exfat; fsck.fat / fsck.exfat
# check the volumes and mtools reads the files back (and writes files the
# firmware code must read). Usage: sd_interop_test.sh <sd_image_tool> <work-dir>
set -uo pipefail
TOOL=$1; W=$2
export PATH="$PATH:/usr/sbin:/sbin" MTOOLS_SKIP_CHECK=1
rm -rf "$W"; mkdir -p "$W"
checks=0; fails=0
check() { checks=$((checks + 1)); if ! eval "$1" >>"$W/log.txt" 2>&1; then echo "FAIL: $1"; fails=$((fails + 1)); fi; }

"$TOOL" - sample "$W/sample.pcgraph"
head -c 300000 /dev/urandom >"$W/big.bin"
printf '' >"$W/empty.bin"
for fmt in fat16 fat32 exfat; do
  img="$W/$fmt.img"
  case $fmt in
    fat16) mkfs.fat -C -F 16 -n PICOCALC "$img" 32768 >/dev/null ;;
    fat32) mkfs.fat -C -F 32 -n PICOCALC "$img" 65536 >/dev/null ;;
    exfat) command -v mkfs.exfat >/dev/null || continue
           truncate -s 64M "$img" && mkfs.exfat -L PICOCALC "$img" >/dev/null ;;
  esac
  # the firmware code writes...
  check "'$TOOL' '$img' project PICOCALC/GRAPH/PROJ1.pcgraph"
  check "'$TOOL' '$img' put PICOCALC/PICT/BIG.bin '$W/big.bin'"
  check "'$TOOL' '$img' put PICOCALC/PICT/EMPTY.bin '$W/empty.bin'"
  for i in $(seq 1 120); do
    "$TOOL" "$img" put "PICOCALC/GRAPH/Progetto_$i.pcgraph" "$W/sample.pcgraph" >>"$W/log.txt" 2>&1 || { echo "FAIL: put $i ($fmt)"; fails=$((fails + 1)); }
  done
  checks=$((checks + 1))
  check "'$TOOL' '$img' rm PICOCALC/GRAPH/Progetto_7.pcgraph"
  check "'$TOOL' '$img' check PICOCALC/GRAPH/PROJ1.pcgraph | grep -q 'equal to the sample'"
  check "'$TOOL' '$img' get PICOCALC/PICT/BIG.bin '$W/$fmt.big' && cmp -s '$W/big.bin' '$W/$fmt.big'"
  check "[ \$('$TOOL' '$img' ls PICOCALC/GRAPH | wc -l) -eq 120 ]"
  if [ "$fmt" = exfat ]; then
    check "fsck.exfat -n '$img'"
    continue
  fi
  # ...an independent FAT implementation checks the volume and the files...
  check "fsck.fat -n '$img'"
  check "mcopy -n -i '$img' ::/PICOCALC/GRAPH/PROJ1.pcgraph '$W/$fmt.proj' && cmp -s '$W/sample.pcgraph' '$W/$fmt.proj'"
  check "mcopy -n -i '$img' ::/PICOCALC/PICT/BIG.bin '$W/$fmt.big2' && cmp -s '$W/big.bin' '$W/$fmt.big2'"
  check "mcopy -n -i '$img' ::/PICOCALC/GRAPH/Progetto_120.pcgraph '$W/$fmt.p120' && cmp -s '$W/sample.pcgraph' '$W/$fmt.p120'"
  check "[ \$(mdir -i '$img' -b ::/PICOCALC/GRAPH | wc -l) -eq 120 ]"
  # ...and writes files (long names included) that the firmware code reads.
  check "mcopy -i '$img' '$W/sample.pcgraph' '::/PICOCALC/GRAPH/Scritto dal PC.pcgraph'"
  check "'$TOOL' '$img' check 'PICOCALC/GRAPH/Scritto dal PC.pcgraph' | grep -q 'equal to the sample'"
  check "'$TOOL' '$img' put PICOCALC/GRAPH/PROJ1.pcgraph '$W/big.bin' && fsck.fat -n '$img'"
done
echo "sd_interop_test: $checks checks, $fails failures"
[ "$fails" -eq 0 ]
