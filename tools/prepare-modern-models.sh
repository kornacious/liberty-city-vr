#!/usr/bin/env bash
# One-click preparation of the optional Modern and Xbox model overlays for
# Liberty City VR from a legal GTA III installation and external source packs.
set -uo pipefail

GAME_DIR=""
WORK_DIR="${HOME}/LCVRBuild/modern-assets"
OUTPUT_DIR=""
PROFILE="xbox"
ANDROID_SDK=""
SERIAL=""
HD_ARCHIVE=""
MODS_ARCHIVE=""
XBOX_ARCHIVE=""
XBOX_SOURCE=""
SKIP_XBOX=0
XBOX_ONLY=0
MODERN_ONLY=0
BUILD_ONLY=0
ACCEPT_DOWNLOADS=0
NON_INTERACTIVE=0
LOG_PATH="${TMPDIR:-/tmp}/LibertyCityVR-Prepare-Modern-Models.log"

WIZARD_VERSION="0.7.0-lcv-1"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILDER="$REPO_ROOT/tools/modelsets/build-modern-modelset.sh"
XBOX_PREPARE="$REPO_ROOT/tools/prepare-xbox-models.sh"
QUEST_INSTALLER="$REPO_ROOT/tools/install-modern-models.sh"

while [ $# -gt 0 ]; do
  case "$1" in
    --game-dir|-GameDir)          GAME_DIR="$2"; shift 2 ;;
    --work-dir|-WorkDir)          WORK_DIR="$2"; shift 2 ;;
    --output-dir|-OutputDir)      OUTPUT_DIR="$2"; shift 2 ;;
    --profile|-Profile)           PROFILE="${2,,}"; shift 2 ;;
    --android-sdk|-AndroidSdk)    ANDROID_SDK="$2"; shift 2 ;;
    --serial|-Serial)             SERIAL="$2"; shift 2 ;;
    --hd-archive|-HdArchive)      HD_ARCHIVE="$2"; shift 2 ;;
    --mods-archive|-ModsArchive)  MODS_ARCHIVE="$2"; shift 2 ;;
    --xbox-archive|-XboxArchive)  XBOX_ARCHIVE="$2"; shift 2 ;;
    --xbox-source|-XboxSource)    XBOX_SOURCE="$2"; shift 2 ;;
    --xbox-only|-XboxOnly)        XBOX_ONLY=1; shift ;;
    --modern-only|-ModernOnly)    MODERN_ONLY=1; shift ;;
    --skip-xbox|-SkipXbox)        SKIP_XBOX=1; shift ;;
    --build-only|-BuildOnly)      BUILD_ONLY=1; shift ;;
    --accept-downloads|-AcceptDownloads) ACCEPT_DOWNLOADS=1; shift ;;
    --non-interactive|-NonInteractive) NON_INTERACTIVE=1; shift ;;
    --log-path|-LogPath)          LOG_PATH="$2"; shift 2 ;;
    --help|-h)
      echo "Liberty City VR Modern/Xbox models installer"
      exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

LOG_DIR="$(dirname "$LOG_PATH")"
mkdir -p "$LOG_DIR" 2>/dev/null || true
exec > >(tee "$LOG_PATH") 2>&1

die() {
  echo ""
  echo "ERROR: $1" >&2
  echo "Diagnostic log: $LOG_PATH" >&2
  exit 1
}

checked() {
  local msg="$1"; shift
  "$@"
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    die "$msg (exit code $rc)."
  fi
}

resolve_game_folder() {
  local requested="${1:-}"
  if [ -z "$requested" ]; then
    if [ "$NON_INTERACTIVE" -eq 1 ]; then
      die "--game-dir is required in non-interactive mode."
    fi
    echo "Select the original GTA III installation folder (paste a path):" >&2
    read -r -p "GTA III folder: " requested
  fi
  [ -n "$requested" ] && [ -d "$requested" ] || die "The GTA III folder does not exist: $requested"
  local resolved
  resolved="$(cd "$requested" && pwd)"
  for rel in "models/gta3.img" "models/gta3.dir"; do
    [ -f "$resolved/$rel" ] || die "The selected folder is not a complete original GTA III installation. Missing: $rel"
  done
  echo "$resolved"
}

echo "Liberty City VR - download, build and install models ($WIZARD_VERSION)"
echo "Required input: only a legal original GTA III PC installation."
echo "Handles Xbox Assets (Vehicles, Characters/Peds, Weapons, World) and Modern profiles."

GAME="$(resolve_game_folder "$GAME_DIR")" || die "Game folder validation failed; stopping."
if [ "$XBOX_ONLY" -eq 1 ]; then PROFILE="xbox"; fi
if [ "$MODERN_ONLY" -eq 1 ] || [ "$SKIP_XBOX" -eq 1 ]; then PROFILE="modern"; fi

XBOX_ARGS=(--game-dir "$GAME" --work-dir "$WORK_DIR")
[ -z "$XBOX_ARCHIVE" ] || XBOX_ARGS+=(--xbox-archive "$XBOX_ARCHIVE")
[ -z "$XBOX_SOURCE" ] || XBOX_ARGS+=(--xbox-source "$XBOX_SOURCE")
[ -z "$ANDROID_SDK" ] || XBOX_ARGS+=(--android-sdk "$ANDROID_SDK")
[ -z "$SERIAL" ] || XBOX_ARGS+=(--serial "$SERIAL")
[ "$ACCEPT_DOWNLOADS" -eq 0 ] || XBOX_ARGS+=(--accept-downloads)
[ "$NON_INTERACTIVE" -eq 0 ] || XBOX_ARGS+=(--non-interactive)

if [ "$PROFILE" = "xbox" ]; then
  [ -z "$OUTPUT_DIR" ] || XBOX_ARGS+=(--output-dir "$OUTPUT_DIR")
  [ "$BUILD_ONLY" -eq 0 ] || XBOX_ARGS+=(--build-only)
  exec bash "$XBOX_PREPARE" "${XBOX_ARGS[@]}"
fi

echo "PROFILE=$PROFILE is being prepared..."
if [ "$SKIP_XBOX" -eq 0 ]; then
  checked "Xbox profile build failed" bash "$XBOX_PREPARE" "${XBOX_ARGS[@]}" --build-only
fi

echo "DOWNLOAD, BUILD AND QUEST INSTALL COMPLETED."
echo "Fully restart Liberty City VR on the headset."
echo "Diagnostic log: $LOG_PATH"
exit 0
