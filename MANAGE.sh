#!/bin/sh
set -eu

manager_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$manager_root"

if [ -x "$manager_root/out/manager-linux/LibertyManager" ]; then
    exec "$manager_root/out/manager-linux/LibertyManager" "$@"
fi

if [ -x "$manager_root/Setup/LibertyManager" ]; then
    exec "$manager_root/Setup/LibertyManager" "$@"
fi

if ! command -v dotnet >/dev/null 2>&1; then
    printf '%s\n' 'Install the .NET 10 SDK or publish the Linux manager to out/manager-linux.' >&2
    exit 127
fi

exec dotnet run --project "$manager_root/tools/manager/LibertyManager.csproj" -c Release -- "$@"
