#!/usr/bin/env bash
# Compatibility entry point for the shared Xbox Version HD (re3) wizard.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/prepare-modern-models.sh" --profile xbox "$@"
