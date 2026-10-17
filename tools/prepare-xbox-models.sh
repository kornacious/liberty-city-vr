#!/usr/bin/env bash
# Build complete Xbox profile for Liberty City VR; install after validation.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR=""; WORK_DIR="$HOME/LCVRBuild/modern-assets"; OUTPUT_DIR=""; XBOX_ARCHIVE=""; XBOX_SOURCE=""
BUILD_ONLY=0; ACCEPT=0; NONINTERACTIVE=0; INSTALL_ARGS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --game-dir|-GameDir) GAME_DIR="$2"; shift 2 ;;
    --work-dir|-WorkDir) WORK_DIR="$2"; shift 2 ;;
    --output-dir|-OutputDir) OUTPUT_DIR="$2"; shift 2 ;;
    --xbox-archive|-XboxArchive) XBOX_ARCHIVE="$2"; shift 2 ;;
    --xbox-source|-XboxSource) XBOX_SOURCE="$2"; shift 2 ;;
    --android-sdk|-AndroidSdk|--serial|-Serial) INSTALL_ARGS+=("$1" "$2"); shift 2 ;;
    --build-only|-BuildOnly) BUILD_ONLY=1; shift ;;
    --accept-downloads|-AcceptDownloads) ACCEPT=1; shift ;;
    --non-interactive|-NonInteractive) NONINTERACTIVE=1; INSTALL_ARGS+=(--non-interactive); shift ;;
    *) echo "Unknown Xbox preparation option: $1" >&2; exit 1 ;;
  esac
done
[ -n "$GAME_DIR" ] && [ -f "$GAME_DIR/models/gta3.img" ] && [ -f "$GAME_DIR/models/gta3.dir" ] || { echo 'Select a legal original GTA III game folder with --game-dir.' >&2; exit 1; }
[ -n "$OUTPUT_DIR" ] || OUTPUT_DIR="$GAME_DIR/modelsets/xbox"
ARGS=(prepare --out "$OUTPUT_DIR" --work-dir "$WORK_DIR")
[ -z "$XBOX_ARCHIVE" ] || ARGS+=(--archive "$XBOX_ARCHIVE")
[ -z "$XBOX_SOURCE" ] || ARGS+=(--source "$XBOX_SOURCE")
[ "$ACCEPT" -eq 0 ] || ARGS+=(--accept-downloads)
[ "$NONINTERACTIVE" -eq 0 ] || ARGS+=(--non-interactive)
python3 "$SCRIPT_DIR/modelsets/xbox-modelset.py" "${ARGS[@]}"
if [ "$BUILD_ONLY" -eq 1 ]; then echo 'Xbox profile ready; no device commands were sent.'; exit 0; fi
exec bash "$SCRIPT_DIR/install-modern-models.sh" --profile xbox --modern-dir "$OUTPUT_DIR" "${INSTALL_ARGS[@]}"
