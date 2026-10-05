#!/usr/bin/env bash
# Configure the installed TFT_eSPI library for this project's board.
#
# Board: ideaspark ESP32 dev board with integrated 1.14" ST7789 TFT (135x240).
# This is NOT a LilyGO TTGO T-Display and the two use DIFFERENT LCD pins, so we
# cannot just enable one of the stock setups. Instead we drop in a custom setup
# file and select it. Driving the TTGO pins on this board only toggles the
# panel's RESET line, which is indistinguishable from a dead panel.
#
# Re-runnable: safe to run after a TFT_eSPI reinstall.
set -euo pipefail

LIBS_DIR="${ARDUINO_LIBS_DIR:-$HOME/Arduino/libraries}"
TFT_DIR="$LIBS_DIR/TFT_eSPI"
SELECT="$TFT_DIR/User_Setup_Select.h"
CUSTOM="$TFT_DIR/User_Setups/Setup_ideaspark_ESP32_114.h"

if [[ ! -f "$SELECT" ]]; then
  echo "TFT_eSPI not found at $TFT_DIR" >&2
  echo "Install it first:  arduino-cli lib install TFT_eSPI" >&2
  exit 1
fi

cp -n "$SELECT" "$SELECT.bak" 2>/dev/null || true

# --- 1. install the custom setup file ---------------------------------------
cat > "$CUSTOM" <<'EOF'
// Custom TFT_eSPI setup: ideaspark ESP32 dev board with integrated
// 1.14" ST7789 TFT (135x240).
//
// IMPORTANT: this board does NOT use the TTGO T-Display pin mapping.
// Per the vendor documentation the LCD is wired as:
//     MOSI D23/GPIO23   SCLK D18/GPIO18   CS D15/GPIO15
//     DC   D2/GPIO2     RST  D4/GPIO4     BLK D32/GPIO32
// Driving the TTGO pins here does nothing except toggle this panel's RESET
// line, which is why the screen stayed blank with three different drivers.

#define USER_SETUP_ID 250

#define ST7789_DRIVER

#define TFT_WIDTH  135
#define TFT_HEIGHT 240

#define CGRAM_OFFSET      // library adds the required 52/40 offsets

// --- LCD SPI pins (ideaspark mapping, NOT TTGO) -----------------------------
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_BL   32
#define TFT_BACKLIGHT_ON HIGH

// Only the fonts this project actually uses, to keep flash use down.
#define LOAD_GLCD     // font 1 - what the bot renders text with
#define LOAD_FONT2    // font 2 - used by the diagnostic sketches

// Conservative: 27 MHz is plenty for a 135x240 panel and tolerant of wiring.
#define SPI_FREQUENCY 27000000
EOF

# --- 2. disable any other active setup, enable ours -------------------------
# The library keeps these under User_Setups/, so a pattern missing that path
# prefix silently matches nothing -- exactly the kind of no-op that wasted a
# debugging cycle. Every step below is verified rather than assumed.
sed -i 's|^#include <User_Setup.h>|//#include <User_Setup.h>   // disabled by esp32-vibe-companion|' "$SELECT"
sed -i 's|^#include <User_Setups/Setup[0-9A-Za-z_]*\.h>|//&|' "$SELECT"
sed -i 's|^//#include <User_Setups/Setup_ideaspark_ESP32_114.h>|#include <User_Setups/Setup_ideaspark_ESP32_114.h>|' "$SELECT"
# if it was not present at all yet, append it
grep -q '^#include <User_Setups/Setup_ideaspark_ESP32_114.h>' "$SELECT" || \
  printf '\n#include <User_Setups/Setup_ideaspark_ESP32_114.h>\n' >> "$SELECT"

echo "== active TFT_eSPI setup include(s) =="
grep -nE '^[[:space:]]*#include <(User_Setup|User_Setups/)' "$SELECT" || true

if ! grep -q '^#include <User_Setups/Setup_ideaspark_ESP32_114.h>' "$SELECT"; then
  echo "FAILED: custom setup is not enabled in $SELECT" >&2
  exit 1
fi
if grep -qE '^#include <User_Setup.h>' "$SELECT"; then
  echo "FAILED: default User_Setup.h is still enabled (must be commented)" >&2
  exit 1
fi
if [[ "$(grep -cE '^#include <User_Setups/' "$SELECT")" != "1" ]]; then
  echo "FAILED: more than one User_Setups profile is active" >&2
  exit 1
fi

echo
echo "TFT_eSPI configured for the ideaspark ESP32 + 1.14in ST7789 (135x240)."
echo "  MOSI 23  SCLK 18  CS 15  DC 2  RST 4  BL 32"
