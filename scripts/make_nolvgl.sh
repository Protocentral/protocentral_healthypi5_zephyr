#!/usr/bin/env bash

# Diagnostic build: HealthyPi 5 firmware WITHOUT LVGL / display module.
#
# Purpose: isolate whether the display thread (priority 5) is the source of
# the multi-thread freeze observed by the SW WDT. This build:
#   - omits the display overlay configs (no CONFIG_LVGL, no CONFIG_DISPLAY,
#     no CONFIG_HEALTHYPI_DISPLAY_ENABLED)
#   - skips compiling display_module.c, the UI directory, and screens
#   - removes the display_screens_thread from the system
#
# Pair with the regular make_ili9488.sh / make_st7796.sh on identical
# hardware (or sequential runs on the same device) to A/B test.

set -euo pipefail

# Run from repo root regardless of where the script was invoked from.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

if [ -f "$HOME/zephyrproject/.venv/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$HOME/zephyrproject/.venv/bin/activate"
fi

west build -p auto -b healthypi5 app \
  -DEXTRA_CONF_FILE="overlay-logger-sd.conf"
