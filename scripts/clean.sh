#!/usr/bin/env bash
set -euo pipefail

# Run from repo root so `west build` finds app/ and west.yml.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

west build -t clean -b healthypi5_rp2040 app