#!/usr/bin/env bash
# Linux port of UPDATE.bat for Liberty City VR.
set -uo pipefail

if [ -z "${LCVR_KEEP_OPEN:-}" ]; then
  export LCVR_KEEP_OPEN=1
  exec bash -c "bash '$0' \"\$@\"; echo; echo 'This window will remain open. Press any key when you are done reading it.'; read -n 1" _ "$@"
fi

echo ""
echo "LIBERTY CITY VR - UPDATE FROM GIT AND INSTALL"
echo "Reuses downloaded tools. Updates only the APK, preserving game data."
echo "Close the game on your Quest before updating."
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LCVR_UPDATE_SCRIPT="$SCRIPT_DIR/tools/update-and-install.sh"

if [ ! -f "$LCVR_UPDATE_SCRIPT" ]; then
  echo "ERROR: Keep UPDATE.sh inside the complete Liberty City VR source kit."
  exit 1
fi

bash "$LCVR_UPDATE_SCRIPT" "$@"
LCVR_UPDATE_EXIT=$?

echo ""
if [ "$LCVR_UPDATE_EXIT" -ne 0 ]; then
  echo "UPDATE FAILED. Read the error above; the game was not launched."
fi
exit "$LCVR_UPDATE_EXIT"
