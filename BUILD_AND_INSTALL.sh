#!/usr/bin/env bash
# Linux port of BUILD_AND_INSTALL.bat for Liberty City VR.
set -uo pipefail

if [ -z "${LCVR_KEEP_OPEN:-}" ]; then
  export LCVR_KEEP_OPEN=1
  exec bash -c "bash '$0' \"\$@\"; echo; echo 'This window will remain open. Press any key when you are done reading it.'; read -n 1" _ "$@"
fi

echo ""
echo "============================================================"
echo "  LIBERTY CITY VR - PERSONAL QUEST BUILD AND INSTALL WIZARD"
echo "============================================================"
echo ""
echo "This builds a personal APK from source. No APK or GTA data is"
echo "included in this repository."
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LCVR_SCRIPT="$SCRIPT_DIR/tools/build-and-install.sh"
LCVR_LOG="${TMPDIR:-/tmp}/LibertyCityVR-Build-And-Install.log"

if [ ! -f "$LCVR_SCRIPT" ]; then
  echo "ERROR: Required script is missing: $LCVR_SCRIPT"
  exit 1
fi

bash "$LCVR_SCRIPT" --log-path "$LCVR_LOG" "$@"
LCVR_EXIT=$?

echo ""
if [ "$LCVR_EXIT" -ne 0 ]; then
  echo "FAILED. Read the error above; nothing was silently ignored."
else
  echo "SUCCESS. The wizard completed all requested steps."
fi
echo ""
echo "Diagnostic log:"
echo "  $LCVR_LOG"
echo ""
exit "$LCVR_EXIT"
