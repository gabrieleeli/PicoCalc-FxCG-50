#!/usr/bin/env bash
# Builds the release package: source zip (committed tree, via git archive),
# the firmware UF2 of each board and SHA256SUMS. Usage: scripts/make_release.sh [out-dir]
# The UF2 files must already be built from the same commit:
#   scripts/build.sh --board pico_plus_2w && scripts/build.sh --board pico2w
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/dist}"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
NAME="PicoCalc-FxCG50-AI-v${VERSION}"

if [[ -n "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ]]; then
  echo "ERROR: uncommitted changes; commit them first so the zip matches the UF2 files." >&2
  exit 2
fi
UF2S=()
for board in pico_plus_2w pico2w; do
  uf2="$ROOT/build/$board/picocalc_fxai_${board}.uf2"
  if [[ ! -f "$uf2" ]]; then
    echo "ERROR: $uf2 not found; run scripts/build.sh --board $board first." >&2
    exit 2
  fi
  # Refuse a UF2 that is older than a tracked firmware source file.
  NEWER="$(cd "$ROOT" && git ls-files -z -- src include cmake CMakeLists.txt certs VERSION third_party | xargs -0 -I{} find {} -newer "$uf2" -print | head -1 || true)"
  if [[ -n "$NEWER" ]]; then
    echo "ERROR: $NEWER is newer than $uf2; rebuild with scripts/build.sh --board $board." >&2
    exit 2
  fi
  UF2S+=("$uf2")
done

mkdir -p "$OUT"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD)"
git -C "$ROOT" archive --format=zip --prefix="${NAME}/" -o "$OUT/${NAME}-src.zip" HEAD
FILES=("${NAME}-src.zip")
for uf2 in "${UF2S[@]}"; do
  base="$(basename "$uf2" .uf2)"
  cp "$uf2" "$OUT/${base}-v${VERSION}.uf2"
  FILES+=("${base}-v${VERSION}.uf2")
done
(
  cd "$OUT"
  sha256sum "${FILES[@]}" > SHA256SUMS
)
echo "Release ${VERSION} (commit ${COMMIT}) in ${OUT}:"
ls -l "$OUT"
