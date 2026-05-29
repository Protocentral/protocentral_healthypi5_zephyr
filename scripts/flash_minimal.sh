#!/usr/bin/env bash

# Build (if needed) and flash the minimal diagnostic image: no LVGL, no BLE.
# Companion to make_minimal.sh — sibling of flash_nolvgl.sh, kept separate
# so each isolation variant has its own build+flash pair.
#
# Usage:
#   ./flash_minimal.sh             # build (auto-pristine) then west flash
#   ./flash_minimal.sh --no-build  # flash whatever is already in build/
#
# Flashing on RP2040 uses the UF2 runner (see boards/protocentral/healthypi5/
# board.cmake). Drop the board into BOOTSEL mode before invoking.

set -euo pipefail

if [ -f "$HOME/zephyrproject/.venv/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$HOME/zephyrproject/.venv/bin/activate"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

if [ "${1:-}" != "--no-build" ]; then
	"$SCRIPT_DIR/make_minimal.sh"
fi

# Sanity check: refuse to flash if the build dir holds a different variant.
# The minimal image must have BOTH display AND BLE disabled.
CFG="build/zephyr/.config"
if [ -f "$CFG" ]; then
	if grep -q '^CONFIG_HEALTHYPI_DISPLAY_ENABLED=y' "$CFG"; then
		echo "ERROR: build/ has display enabled, not the minimal variant."
		echo "       Re-run 'scripts/flash_minimal.sh' (without --no-build) to rebuild."
		exit 1
	fi
	if grep -q '^CONFIG_BT=y' "$CFG"; then
		echo "ERROR: build/ has BLE enabled, not the minimal variant."
		echo "       Re-run 'scripts/flash_minimal.sh' (without --no-build) to rebuild."
		exit 1
	fi
fi

west flash
