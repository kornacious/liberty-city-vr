#!/usr/bin/env sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${1:-"$source_dir/../../out/vehicle-editor-linux-build"}
output_dir=${2:-"$source_dir/../../out/vehicle-editor-linux"}
cmake -S "$source_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target liberty-vehicle-editor placement-tests placement-runtime-tests placement-process-tests --parallel
ctest --test-dir "$build_dir" --output-on-failure
cmake --install "$build_dir" --prefix "$output_dir"
(cd "$build_dir" && cpack -G ZIP)
printf 'Editor: %s/liberty-vehicle-editor\n' "$output_dir"
