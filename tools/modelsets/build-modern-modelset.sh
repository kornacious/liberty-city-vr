#!/usr/bin/env bash
# Modern alias of the supported GTA III Xbox Version HD (re3) builder.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARGS=(build --profile modern)
while [ "$#" -gt 0 ]; do
  case "$1" in
    --source|-Source) ARGS+=(--source "$2"); shift 2 ;;
    --out|-Out) ARGS+=(--out "$2"); shift 2 ;;
    --force|-Force) ARGS+=(--force); shift ;;
    *) echo "Unsupported argument: $1. Use --source for the extracted Xbox Version HD (re3) folder." >&2; exit 1 ;;
  esac
done
exec python3 "$SCRIPT_DIR/xbox-modelset.py" "${ARGS[@]}"
