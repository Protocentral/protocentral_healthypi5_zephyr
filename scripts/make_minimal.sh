#!/usr/bin/env bash

# Diagnostic build: HealthyPi 5 firmware with BOTH LVGL/display AND BLE
# disabled. The next step after the no-LVGL build (make_nolvgl.sh) when the
# SW WDT still trips: this narrows the freeze cause to USB stack, system
# workqueue, or sensor/data path by removing the BLE host+controller
# cooperative threads from the system entirely.
#
# What gets dropped vs. the regular display build:
#   - All LVGL framebuffer, fonts, screens, display_module.c
#   - The display_screens_thread (priority 5)
#   - The Bluetooth host + controller stacks and their cooperative threads
#   - ble_module.c (the zbus->bt_gatt_notify bridge)
#   - The bt_*_lis zbus listeners (gated via HPI_OBSERVERS in
#     hpi_zbus_channels.c)
#
# Companion scripts (kept separate so each isolation step is independent):
#   make_ili9488.sh / make_st7796.sh -- full build with display + BLE
#   make_nolvgl.sh                   -- BLE on, display off
#   make_minimal.sh                  -- both off (this script)

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
  -DEXTRA_CONF_FILE="overlay-logger-sd.conf;overlay-no-ble.conf"
