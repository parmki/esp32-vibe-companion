#!/usr/bin/env bash
# Flash to the board and capture the boot banner.
#
#   ./scripts/flash.sh [/dev/ttyUSB0]
#
# The CH340 port is root:dialout. If the current login session predates the
# `usermod -aG dialout` (so `id` still lacks the group), this falls back to
# `sg dialout -c ...`, which applies the group in a child shell WITHOUT needing
# a log out and back in. The group must already be in /etc/group for that to
# work -- check with:  getent group dialout
set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"
FQBN="esp32:esp32:esp32:PartitionScheme=huge_app"
SKETCH_DIR="$(cd "$(dirname "$0")/.." && pwd)"

CLI="$(command -v arduino-cli || true)"
[[ -n "$CLI" ]] || CLI="$HOME/.local/bin/arduino-cli"
[[ -x "$CLI" ]] || { echo "arduino-cli not found" >&2; exit 1; }

if [[ ! -e "$PORT" ]]; then
  echo "Port $PORT does not exist. Is the board plugged in?" >&2
  ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
  exit 1
fi

UPLOAD="$CLI upload -p $PORT --fqbn $FQBN \"$SKETCH_DIR\""
MONITOR="python3 \"$SKETCH_DIR/scripts/capture_serial.py\" --port $PORT --seconds 25"

run() {
  if [[ -w "$PORT" ]]; then
    eval "$1"
  elif getent group dialout | awk -F: '{print $4}' | tr ',' '\n' | grep -qx "$USER"; then
    echo ">> port not directly writable; using 'sg dialout' (no re-login needed)"
    sg dialout -c "$1"
  else
    echo "No write access to $PORT and $USER is not in dialout." >&2
    ls -l "$PORT"
    echo "Fix:  sudo usermod -aG dialout \$USER   (then log out/in)" >&2
    echo "      or:  sudo chmod a+rw $PORT" >&2
    exit 1
  fi
}

echo ">> uploading to $PORT"
run "$UPLOAD"

echo
echo ">> boot banner ($PORT @115200)"
run "$MONITOR"
