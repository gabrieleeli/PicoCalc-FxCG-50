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
  # The UF2 must have been built by scripts/build.sh from the firmware sources
  # of this commit: build.sh records the commit and the UF2 hash. Commits that
  # only change other files (documentation) do not require a rebuild.
  state="$ROOT/build/$board/fxai_source_state"
  if [[ ! -f "$state" ]]; then
    echo "ERROR: $state not found; rebuild with scripts/build.sh --board $board." >&2
    exit 2
  fi
  read -r built_commit built_hash < "$state"
  if [[ "$built_hash" != "$(sha256sum "$uf2" | cut -d' ' -f1)" ]]; then
    echo "ERROR: $uf2 changed after scripts/build.sh recorded it; rebuild with scripts/build.sh --board $board." >&2
    exit 2
  fi
  if [[ "$built_commit" == "dirty" ]]; then
    echo "ERROR: $uf2 was built from uncommitted firmware sources; commit and rebuild with scripts/build.sh --board $board." >&2
    exit 2
  fi
  if ! git -C "$ROOT" cat-file -e "${built_commit}^{commit}" 2>/dev/null ||
     ! git -C "$ROOT" diff --quiet "$built_commit" HEAD -- src include cmake CMakeLists.txt certs VERSION third_party; then
    echo "ERROR: firmware sources changed since $uf2 was built (commit ${built_commit:0:7}); rebuild with scripts/build.sh --board $board." >&2
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
