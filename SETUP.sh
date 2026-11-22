#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for setup in "$root/Setup/LibertySetup" "$root/out/setup-linux-x64/LibertySetup"; do
    if [ -x "$setup" ]; then
        exec "$setup"
    fi
done
if ! command -v dotnet >/dev/null 2>&1; then
    printf '%s\n' 'Download the Linux x64 Setup kit from the Liberty City VR releases page.' >&2
    exit 1
fi
exec dotnet run --project "$root/tools/setup" -c Release --artifacts-path "$root/out/setup-local-build"
