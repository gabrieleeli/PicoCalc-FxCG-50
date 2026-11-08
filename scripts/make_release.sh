#!/usr/bin/env bash
# Builds the release package: source zip (committed tree, via git archive),
# the firmware UF2 and SHA256SUMS. Usage: scripts/make_release.sh [out-dir]
# The UF2 must already be built (scripts/build_pico2w.sh) from the same commit.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/dist}"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
UF2="${PICO_BUILD_DIR:-$ROOT/build/pico2w}/picocalc_fxai.uf2"
NAME="PicoCalc-FxCG50-AI-v${VERSION}"

if [[ -n "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ]]; then
  echo "ERROR: uncommitted changes; commit them first so the zip matches the UF2." >&2
  exit 2
fi
if [[ ! -f "$UF2" ]]; then
  echo "ERROR: $UF2 not found; run scripts/build_pico2w.sh first." >&2
  exit 2
fi
# Refuse a UF2 that is older than a tracked firmware source file.
NEWER="$(cd "$ROOT" && git ls-files -z -- src include cmake CMakeLists.txt certs VERSION | xargs -0 -I{} find {} -newer "$UF2" -print | head -1 || true)"
if [[ -n "$NEWER" ]]; then
  echo "ERROR: $NEWER is newer than the UF2; rebuild with scripts/build_pico2w.sh." >&2
  exit 2
fi

mkdir -p "$OUT"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD)"
git -C "$ROOT" archive --format=zip --prefix="${NAME}/" -o "$OUT/${NAME}-src.zip" HEAD
cp "$UF2" "$OUT/picocalc_fxai-v${VERSION}.uf2"
(
  cd "$OUT"
  sha256sum "${NAME}-src.zip" "picocalc_fxai-v${VERSION}.uf2" > SHA256SUMS
)
echo "Release ${VERSION} (commit ${COMMIT}) in ${OUT}:"
ls -l "$OUT"
