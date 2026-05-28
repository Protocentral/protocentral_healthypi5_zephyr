#!/usr/bin/env bash

# Build (if needed) and flash the no-LVGL diagnostic image.
#
# This is the companion to make_nolvgl.sh / flash.sh — it ensures the build
# directory holds the no-LVGL variant before flashing, so you don't acci-
# dentally flash the regular display build that may also be sitting in
# build/ from a prior make_ili9488.sh run.
#
# Usage:
#   ./flash_nolvgl.sh           # build (auto-pristine) then west flash
#   ./flash_nolvgl.sh --no-build  # flash whatever is already in build/
#
# Flashing on RP2040 uses the UF2 runner by default (see board.cmake). Put
# the board into BOOTSEL mode (hold the BOOTSEL button while plugging in
# USB) so it enumerates as the RPI-RP2 mass-storage device. `west flash`
# then drops zephyr.uf2 onto that volume and the device reboots.

set -euo pipefail

if [ -f "$HOME/zephyrproject/.venv/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$HOME/zephyrproject/.venv/bin/activate"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

if [ "${1:-}" != "--no-build" ]; then
	"$SCRIPT_DIR/make_nolvgl.sh"
fi

# Sanity check: bail if the artifact obviously isn't the no-LVGL build.
# The no-LVGL image is ~260 KB of flash; the display build is ~820 KB.
# If the .config in the active build dir has HEALTHYPI_DISPLAY_ENABLED=y,
# refuse to flash so we don't push the wrong binary to a test device.
CFG="build/zephyr/.config"
if [ -f "$CFG" ] && grep -q '^CONFIG_HEALTHYPI_DISPLAY_ENABLED=y' "$CFG"; then
	echo "ERROR: build/ holds a display-enabled build, not the no-LVGL variant."
	echo "       Run 'scripts/flash_nolvgl.sh' (without --no-build) to rebuild."
	exit 1
fi

west flash
