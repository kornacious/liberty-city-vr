#!/usr/bin/env bash
# Linux port of REPAIR_GAME_FILES.bat for Liberty City VR.
set -uo pipefail

if [ -z "${LCVR_KEEP_OPEN:-}" ]; then
  export LCVR_KEEP_OPEN=1
  exec bash -c "bash '$0' \"\$@\"; echo; echo 'This window will remain open. Press any key when you are done reading it.'; read -n 1" _ "$@"
fi

echo "================================================================"
echo " LIBERTY CITY VR - REPAIR MENU TEXT AND FRONTEND FILES"
echo "================================================================"
echo ""
echo "Run this if the OPTIONS menus show entries like \"FET_GFX missing\""
echo "instead of proper labels. It copies the port's own replacement"
echo "text and frontend files to an existing installation."
echo ""
echo "Saves, settings and models are not touched."
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALLER="$SCRIPT_DIR/tools/install-game-files.sh"

if [ ! -f "$INSTALLER" ]; then
    echo "ERROR: Required script is missing: $INSTALLER"
    exit 1
fi

bash "$INSTALLER" "$@"
RESULT=$?

echo ""
if [ "$RESULT" -ne 0 ]; then
    echo "FAILED. Read the error above; nothing was silently ignored."
else
    echo "COMPLETE. Fully restart Liberty City VR on the headset."
fi
echo ""
exit "$RESULT"
