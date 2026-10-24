#!/usr/bin/env sh
set -eu
exec sh "$(dirname -- "$0")/tools/vehicle-editor/build.sh" "$@"
