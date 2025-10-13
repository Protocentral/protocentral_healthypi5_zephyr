#!/usr/bin/env bash

# Canonical build wrapper. Sources the project's Zephyr Python venv if present
# and builds the requested display variant. By default builds the ILI9488
# display variant. Call with `st7796` to build the ST7796 variant.

set -euo pipefail

# Source user's Zephyr venv if it exists
if [ -f "$HOME/zephyrproject/.venv/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$HOME/zephyrproject/.venv/bin/activate"
fi

DISPLAY_VARIANT=${1:-ili9488}

if [ "$DISPLAY_VARIANT" = "st7796" ]; then
	EXTRA_CONF="overlay-display-st7796.conf;overlay-logger-sd.conf"
	DTC_OVERLAY="healthypi5_rp2040_display_st7796.overlay"
else
	EXTRA_CONF="overlay-display-ili9488.conf;overlay-logger-sd.conf"
	DTC_OVERLAY="healthypi5_rp2040_display_ili9488.overlay"
fi

west build -p auto -b healthypi5 app \
  -DEXTRA_CONF_FILE="$EXTRA_CONF" \
  -DEXTRA_DTC_OVERLAY_FILE="$DTC_OVERLAY"