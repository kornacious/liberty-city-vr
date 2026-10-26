#!/usr/bin/env bash
# Linux port of INSTALL_MODERN_MODELS.bat for Liberty City VR.
set -uo pipefail

goto_finish() {
    echo ""
    if [ "$RESULT" -ne 0 ]; then
        echo "FAILED. Read the error above; inspect the diagnostic log before retrying."
    fi
    if [ -t 0 ]; then
        echo "Press any key to close."
        read -r -n 1 || true
    fi
    exit "$RESULT"
}

title "Liberty City VR - Download Build and Install Modern / Xbox Models" 2>/dev/null || true
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

echo "================================================================"
echo " LIBERTY CITY VR - DOWNLOAD, BUILD AND INSTALL MODERN / XBOX MODELS"
echo "================================================================"
echo ""

INSTALLER="$PWD/tools/prepare-modern-models.sh"
if [ ! -f "$INSTALLER" ]; then
    echo "ERROR: Required file is missing:"
    echo "  $INSTALLER"
    echo ""
    echo "Extract the complete source-kit ZIP before running this file."
    RESULT=1
    goto_finish
fi

bash "$INSTALLER" "$@"
RESULT=$?

goto_finish
