#!/usr/bin/env bash
# Compile the sketch for the ideaspark ESP32 + 1.14" ST7789 board, huge_app.
#
# huge_app is mandatory: 8 sprites x 135x240x2 bytes = 506 KB of PROGMEM, which
# blows past the default 1.2 MB "default" app partition once you add TFT_eSPI
# and the TLS stack.
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FQBN="esp32:esp32:esp32:PartitionScheme=huge_app"

command -v arduino-cli >/dev/null 2>&1 || export PATH="$HOME/.local/bin:$PATH"

echo "Compiling $SKETCH_DIR"
echo "FQBN: $FQBN"
arduino-cli compile \
  --fqbn "$FQBN" \
  --warnings default \
  --output-dir "$SKETCH_DIR/build" \
  "$SKETCH_DIR" "$@"
