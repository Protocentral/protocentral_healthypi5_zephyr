#!/usr/bin/env bash

# Delegate to make_ili9488.sh (canonical build script) with st7796 variant.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/make_ili9488.sh" st7796