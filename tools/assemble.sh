#!/bin/sh
# Assembles a buildable source tree for the Liberty City VR Quest port.
#
#   tools/assemble.sh <path to re3 source> <output dir>
#
# The re3 source tree is not part of this repository and is not distributed
# with it. The tested patch base is commit b050e1cc63b1869e6b514ab2f1a00ef4969e0c84
# from mrxenginner/re3.
set -e
RE3="$1"
OUT="$2"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

[ -n "$RE3" ] && [ -n "$OUT" ] || { echo "usage: $0 <re3-source> <output-dir>"; exit 1; }
[ -f "$RE3/src/core/main.cpp" ] || { echo "'$RE3' does not look like a re3 source tree"; exit 1; }
[ ! -e "$OUT" ] || { echo "'$OUT' already exists; refusing to overwrite"; exit 1; }

echo "[1/4] Copying the re3 tree..."
mkdir -p "$OUT"
cp -r "$RE3/." "$OUT/"
rm -rf "$OUT/.git"

echo "[2/4] Applying the port patches..."
cd "$OUT"
git init -q

for patch in "$REPO"/patches/*.patch; do
    if [ -f "$patch" ]; then
        echo "Applying $(basename "$patch")..."
        git apply --whitespace=nowarn "$patch" || {
            echo "Patch $(basename "$patch") did not apply cleanly. The source must be a clean checkout of commit b050e1cc63b1869e6b514ab2f1a00ef4969e0c84."
            exit 1
        }
    fi
done
rm -rf "$OUT/.git"

echo "[3/4] Copying the port sources..."
cp -r "$REPO/overlay/." "$OUT/"

echo "[4/4] Copying librw with the Vulkan backend..."
mkdir -p "$OUT/vendor/librw"
cp -r "$REPO/librw/." "$OUT/vendor/librw/"

echo ""
echo "Done. Next steps:"
echo "  cd $OUT/android"
echo "  gradle wrapper   (first time only, or open in Android Studio)"
echo "  ./gradlew assembleDebug"
echo "See BUILDING.md for prerequisites, game data, VR hands and headset installation."
