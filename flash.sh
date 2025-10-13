#!/usr/bin/env bash

# Flash helper: sources the user's Zephyr venv and runs the appropriate flash
# command. By default runs `west flash`. If invoked with `merged`, it will
# delegate to `./flash_merged.sh` which handles merged UF2 flashing.

set -euo pipefail

# Source user's Zephyr venv if present
if [ -f "$HOME/zephyrproject/.venv/bin/activate" ]; then
    # shellcheck source=/dev/null
    source "$HOME/zephyrproject/.venv/bin/activate"
fi

MODE=${1:-default}

if [ "$MODE" = "merged" ]; then
    if [ -x ./flash_merged.sh ]; then
        ./flash_merged.sh
    else
        echo "flash_merged.sh not found or not executable"
        exit 1
    fi
else
    west flash
fi
