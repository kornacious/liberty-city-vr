#!/usr/bin/env bash
# Prepare the same pinned GTA III re3 pack for either runtime profile.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR=""; WORK_DIR="$HOME/LCVRBuild/modern-assets"; OUTPUT_DIR=""; PROFILE=modern
ARCHIVE=""; SOURCE=""; BUILD_ONLY=0; ACCEPT=0; NONINTERACTIVE=0; INSTALL_ARGS=()
LOG_PATH="${TMPDIR:-/tmp}/LibertyCityVR-Prepare-Modern-Models.log"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --game-dir|-GameDir) GAME_DIR="$2"; shift 2 ;;
    --work-dir|-WorkDir) WORK_DIR="$2"; shift 2 ;;
    --output-dir|-OutputDir) OUTPUT_DIR="$2"; shift 2 ;;
    --profile|-Profile) PROFILE="${2,,}"; shift 2 ;;
    --xbox-archive|-XboxArchive|--modern-archive|-ModernArchive|--hd-archive|-HdArchive) ARCHIVE="$2"; shift 2 ;;
    --xbox-source|-XboxSource) SOURCE="$2"; shift 2 ;;
    --mods-archive|-ModsArchive) echo 'The old two-pack workflow is retired. Supply the re3 pack with --xbox-archive.' >&2; exit 1 ;;
    --xbox-only|-XboxOnly) PROFILE=modern; shift ;;
    --modern-only|-ModernOnly|--skip-xbox|-SkipXbox) PROFILE=modern; shift ;;
    --android-sdk|-AndroidSdk|--serial|-Serial) INSTALL_ARGS+=("$1" "$2"); shift 2 ;;
    --build-only|-BuildOnly) BUILD_ONLY=1; shift ;;
    --accept-downloads|-AcceptDownloads) ACCEPT=1; shift ;;
    --non-interactive|-NonInteractive) NONINTERACTIVE=1; INSTALL_ARGS+=(--non-interactive); shift ;;
    --log-path|-LogPath) LOG_PATH="$2"; shift 2 ;;
    --help|-h) echo 'Prepare Xbox Version HD (re3): --game-dir DIR [--profile modern] [--xbox-archive ZIP] [--build-only]'; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done
case "$PROFILE" in xbox|modern|both) ;; *) echo 'Profile must be xbox, modern or both.' >&2; exit 1 ;; esac
[ "$PROFILE" = modern ] || echo 'Xbox/Both are legacy aliases for the single Modern (Xbox HD) profile.'
PROFILE=modern
if [ -z "$GAME_DIR" ] && [ "$NONINTERACTIVE" -eq 0 ]; then read -r -p 'Original GTA III PC folder: ' GAME_DIR; fi
[ -n "$GAME_DIR" ] && [ -f "$GAME_DIR/models/gta3.img" ] && [ -f "$GAME_DIR/models/gta3.dir" ] || { echo 'Select an original GTA III installation with --game-dir.' >&2; exit 1; }
mkdir -p "$(dirname "$LOG_PATH")"
exec > >(tee "$LOG_PATH") 2>&1
PROFILES=("$PROFILE")

OUTPUTS=()
for target in "${PROFILES[@]}"; do
  out="${OUTPUT_DIR:-$GAME_DIR/modelsets/$target}"
  ARGS=(prepare --profile "$target" --out "$out" --work-dir "$WORK_DIR")
  [ -z "$ARCHIVE" ] || ARGS+=(--archive "$ARCHIVE")
  [ -z "$SOURCE" ] || ARGS+=(--source "$SOURCE")
  [ "$ACCEPT" -eq 0 ] || ARGS+=(--accept-downloads)
  [ "$NONINTERACTIVE" -eq 0 ] || ARGS+=(--non-interactive)
  python3 "$SCRIPT_DIR/modelsets/xbox-modelset.py" "${ARGS[@]}"
  OUTPUTS+=("$out")
done
if [ "$BUILD_ONLY" -eq 1 ]; then
  echo 'Requested profiles built and verified; no device commands were sent.'
else
  for i in "${!PROFILES[@]}"; do
    bash "$SCRIPT_DIR/install-modern-models.sh" --profile "${PROFILES[$i]}" --modern-dir "${OUTPUTS[$i]}" "${INSTALL_ARGS[@]}"
  done
  echo 'Requested profiles installed. Select Classic or Modern in MODEL ASSETS, then fully restart. Saved selections were preserved.'
fi
