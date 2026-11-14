#!/bin/sh
# Model fx-CG50 frame cost on the host.
#
#   profile.sh <build-dir> [baseline-costs.txt]
#
# Builds fxOGL and the fxGLMark scenes as 32-bit soft-float code whose float
# operations go through softfloat_shim.c, checks the frames against
# golden_frames.txt, runs callgrind on one frame of each scene and writes
# <build-dir>/costs.txt. With a baseline costs.txt it prints the change per
# scene. Requires gcc/g++ multilib (-m32) and valgrind.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
[ $# -ge 1 ] || { echo "usage: $0 <build-dir> [baseline-costs.txt]" >&2; exit 2; }
OUT=$1
BASELINE=${2:-}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

command -v valgrind >/dev/null || { echo "valgrind is required" >&2; exit 1; }
printf 'int main(void){return 0;}\n' > "$OUT/probe.c"
gcc -m32 "$OUT/probe.c" -o "$OUT/probe" 2>/dev/null ||
    { echo "32-bit toolchain missing: install gcc-multilib and g++-multilib" >&2; exit 1; }

SCENES="clear build:use-vbo=true build:use-vbo=false texture:texture-filter=nearest texture:texture-filter=linear shading:shading=gouraud"
INCLUDES="-I$ROOT/apps/fxGLMark/tests/host/stubs -I$ROOT/apps/fxGLMark/src -I$ROOT/lib/fxOGL/include -I$ROOT/lib/fxOGL/src"
# -Os matches the fx-CG50 build; -ffp-contract=off keeps results bit-exact.
FLAGS="-m32 -msoft-float -mno-80387 -Os -g -ffp-contract=off -std=c++20"

echo "Building soft-float profile binary in $OUT"
gcc -m32 -O2 -msse2 -mfpmath=sse -fno-builtin -c "$HERE/softfloat_shim.c" -o "$OUT/softfloat_shim.o"
for source in "$ROOT"/lib/fxOGL/src/gles/*.cpp "$ROOT"/lib/fxOGL/src/gfx/*.cpp \
              "$ROOT/apps/fxGLMark/src/glmark2/glmark2.cpp" "$HERE/scenes.cpp"; do
    g++ $FLAGS $INCLUDES -c "$source" -o "$OUT/$(basename "$source" .cpp).o" &
done
wait
g++ -m32 "$OUT"/*.o -o "$OUT/scenes" -ldl -lm

# Soft-float results must match the hardware-float reference bit for bit.
"$OUT/scenes" --check "$HERE/golden_frames.txt"

echo "Running callgrind on one frame per scene"
for scene in $SCENES; do
    name=$(echo "$scene" | tr ':=' '__')
    valgrind --tool=callgrind --callgrind-out-file="$OUT/callgrind.$name" \
        --toggle-collect='fxglmark2::render_scene_frame*' \
        "$OUT/scenes" "$scene" 1 >/dev/null 2>"$OUT/callgrind.$name.log" &
done
wait

: > "$OUT/costs.txt"
for scene in $SCENES; do
    name=$(echo "$scene" | tr ':=' '__')
    cost=$(grep -m1 '^summary:' "$OUT/callgrind.$name" | awk '{print $2}')
    echo "$scene $cost" >> "$OUT/costs.txt"
done

if [ -n "$BASELINE" ]; then
    echo "Modelled frame cost (instructions incl. soft-float weights), baseline -> now:"
    while read -r scene cost; do
        base=$(awk -v s="$scene" '$1 == s {print $2}' "$BASELINE")
        awk -v s="$scene" -v b="${base:-0}" -v c="$cost" \
            'BEGIN { if (b > 0) printf "  %-32s %12d -> %12d  %+6.1f%%\n", s, b, c, (c / b - 1) * 100;
                     else printf "  %-32s %12s -> %12d\n", s, "-", c }'
    done < "$OUT/costs.txt"
else
    echo "Modelled frame cost (instructions incl. soft-float weights):"
    while read -r scene cost; do printf '  %-32s %12d\n' "$scene" "$cost"; done < "$OUT/costs.txt"
fi
echo "Per-function detail: callgrind_annotate --inclusive=yes $OUT/callgrind.<scene>"
