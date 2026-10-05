# esp32-vibe-companion

An ESP32 desk companion with a face and a personality: an 18-mood animated panel,
and a text chat interface over Telegram. The mood engine, conversation state and
memory all run **on the device** — there is no cloud inference, and the bot does
not need an LLM to hold a conversation.

Built on an ideaspark ESP32-WROOM-32 board with an integrated 1.14" ST7789 IPS
panel (135×240) and CH340 USB-serial.

## Repository layout

```
esp32-vibe-companion/
├── esp32-vibe-companion.ino   # firmware: display, keepalive, NTP, Telegram, idle loop
├── brain.h                    # state engine: mood, grudge, gaps, NVS memory, templates
├── personality.h              # keyword matcher (host-testable, no Arduino deps)
├── responses.h                # the response data: keyword / context / Q&A tables
├── sprites.h                  # GENERATED — RGB565 PROGMEM sprites (not committed)
├── photos.h                   # GENERATED — small JPEGs for chat (not committed)
├── config.h                   # real credentials (gitignored)
├── config.h.example           # configuration template
├── scripts/
│   ├── convert_images.py      # asset pipeline  (image -> sprites.h + photos.h)
│   ├── render_mockup.py       # pixel-exact preview using TFT_eSPI's GLCD font
│   ├── setup_tft_espi.sh      # install + select the board's TFT_eSPI setup
│   ├── build.sh               # compile with the huge_app partition scheme
│   ├── flash.sh               # upload + capture the boot banner
│   ├── capture_serial.py      # resets the chip and reads the banner
│   ├── check_telegram.py      # validates the bot token via getMe
│   └── set_ssid_from_nmcli.py # copy the active SSID from NetworkManager into config.h
├── test/
│   ├── personality_test.cpp   # data tests: tables, moods, question/answer integrity
│   ├── brain_test.cpp         # behaviour tests: runs the real reply pipeline
│   └── replay_transcript.cpp  # replay a conversation through the current brain
└── tools/                     # throwaway hardware diagnostics (see notes below)
```

## Hardware

**ideaspark ESP32 dev board with integrated 1.14" ST7789 TFT (135×240), CH340 USB.**

This board is *not* a LilyGO TTGO T-Display, and the two do **not** share a
display pinout. Driving TTGO pins on this board only toggles the panel's RESET
line, which looks exactly like a dead panel.

| Signal   | this board | (TTGO T-Display, for contrast) |
|----------|------------|--------------------------------|
| TFT_MOSI | **23**     | 19                             |
| TFT_SCLK | 18         | 18                             |
| TFT_CS   | **15**     | 5                              |
| TFT_DC   | **2**      | 16                             |
| TFT_RST  | **4**      | 23                             |
| TFT_BL   | **32**     | 4                              |

Selected via `User_Setups/Setup_ideaspark_ESP32_114.h`, installed by
`scripts/setup_tft_espi.sh`. Backlight is driven HIGH in `setup()`.

## Build and flash

```bash
./scripts/setup_tft_espi.sh        # one-time: select the board's TFT_eSPI setup
./scripts/build.sh
./scripts/flash.sh /dev/ttyUSB0    # upload, then monitor @115200
```

### Toolchain from scratch

```bash
# arduino-cli -> ~/.local/bin (no root needed)
curl -fsSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz \
  | tar xz -C /tmp arduino-cli && install -m755 /tmp/arduino-cli ~/.local/bin/

arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install TFT_eSPI UniversalTelegramBot ArduinoJson
```

The CH340 port is `root:dialout`. If your login session predates
`usermod -aG dialout`, `flash.sh` falls back to `sg dialout -c ...`, which applies
the group in a child shell without a re-login. Session-only alternative:
`sudo chmod a+rw /dev/ttyUSB0`.

## Configuration

Copy `config.h.example` to `config.h` and fill it in. `config.h` is gitignored.

| Define | Purpose |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | 2.4 GHz network only — the ESP32 has no 5 GHz radio |
| `BOT_TOKEN` | from @BotFather |
| `OWNER_CHAT_ID` | optional; pins the bot to a single chat. Empty = answer anyone |
| `IDLE_TIMEOUT_MS` | silence before the first unprompted line (default 150 s) |
| `YOUR_NAME` | what the companion calls you, substituted for the `%name` token |
| `TIMEZONE_TZ` | POSIX TZ string for the wall clock |
| `SEND_MOOD_PHOTOS` | also send the mood image back in chat |

Secrets live only in `config.h`. `scripts/set_ssid_from_nmcli.py` can copy the
active SSID out of NetworkManager without printing it.

## Personality engine

`personality.h` tokenizes incoming text and matches two kinds of keyword:

1. **Phrases** (`"good girl"`, `"who built you"`, `"love you"`) — substring test, run first.
2. **Single words** (`"her"`, `"work"`, `"smart"`) — whole-word only, so `"her"`
   never fires inside `"there"` or `"brother"`.

Phrase hits short-circuit the single-word pass, which is what makes
`"good girl"` resolve to the pride register instead of being caught by `"girl"`.
This pass also protects the emotional phrases: `"i wish we could be together"`
answers that, rather than being diluted by `yes` and `together`.

**All** matching rules are collected and one is chosen, so a keyword with several
variants stays varied — a phrase can have one line or thirteen.

| Trigger class | Moods | Examples |
|---|---|---|
| Affection | happy, seductive, blush | `love you`, `kiss`, `mine` |
| Attention / jealousy | angry, pout | `friends`, `work`, `her`, `busy` |
| Pride | proud, smug | `good girl`, `who built you` |
| Teased | blush, seductive | `marry me`, `hot` |
| Wishing / romantic | heart, blanket | `be together`, `i wish`, `stay forever` |
| Name-calling | smug, tease, angry | `slut`, `brat`, `go away` |
| no match | any | the fallback bank |

## Context and memory

`brain.h` is the state machine that turns a matcher into something that feels
like it remembers you. Every reply runs through it in priority order.

| Lever | Behaviour |
|---|---|
| **Gap reactions** | Reactions scale with how long you were gone: seconds → minutes → hours → days. A gap over 10 minutes *is* the message and outranks content. |
| **Grudge (0–100)** | Rises with absence and farewells; falls with apologies, affection and flattery |
| **Reconciliation** | Earned in stages — the first "sorry" thaws it, bare repeats get called out, and a high grudge takes several sincere messages |
| **Relief beat** | Clear a high grudge and the tone cracks instead of gloating |
| **Farewells refused** | Six escalation stages, six lines each, driven by a goodbye counter |
| **Unplug tally** | Powered off uncleanly? It is counted and remembered |
| **Pending questions** | The companion asks one of 50 questions and remembers **which**; your next message is handled as an answer to that question (181 question-specific replies) |
| **Anti-repetition** | A ring of recently-used lines; selection sites avoid anything said recently |
| **Repeat detection** | Notices when you send the same thing twice |
| **Idle tiers** | Silence escalates at 2.5 min / 5 min / 15 min / 1 h / 6 h with different moods and lines |
| **Wall clock** | NTP, so late-night lines differ from morning ones — including ordinary small talk |

State persists in **NVS** (the `huge_app` layout includes a 20 KB `nvs`
partition), so a power cycle does not reset accumulated state.

Lines support `%tokens`, substituted at render time: `%name`, `%t` (time gone),
`%up` (uptime), `%n`, `%all`, `%u` (unplug count), `%clock`, `%last` (echo of your
last content word), `%promise`.

## Response data

`responses.h` holds the lines, split into tables by purpose: plain keyword
registers, context tables evaluated against live state (gap, farewell, apology,
idle tiers), and a question/answer bank. Around **1000 distinct lines** currently.

Character rules live entirely in this file — nothing about tone is hardcoded in
the firmware.

## Art pipeline

`sprites.h` and `photos.h` are **not committed**. They are generated from source
images by `scripts/convert_images.py`, which centre-crops each one to 135×240,
emits RGB565 for the panel, and a small JPEG per mood for chat:

```bash
python3 scripts/convert_images.py --src DIR
```

The converter is deliberately forgiving about filenames: names are NFKD-normalized
to `[a-z0-9]` before matching, and the most specific keyword is tested first so a
generic keyword (`happy`) cannot steal a specific one (`happypointingtoself`).
Unmatched files are reported rather than silently skipped.

| Mood | Keyword matched |
|---|---|
| angry | `madangry` |
| seductive | `seductive` |
| happy | `happy` |
| pout | `annoyedpout` |
| blush | `blushdizzy` |
| proud | `happypointingtoself` |
| no_internet | `nointernet` |
| smug | `smug` |

## Memory budget

18 sprites × 135 × 240 × 2 B = **1,166,400 B (~1.1 MB)** of PROGMEM, which does
not fit the stock `default` app partition alongside TFT_eSPI + mbedTLS. Hence:

```
esp32:esp32:esp32:PartitionScheme=huge_app
```

`huge_app` gives a ~3 MB app partition. Current build uses ~76% of it, leaving
room for both more response data and more moods.

Sprites are stored as native-endian `uint16_t` RGB565 and pushed with
`tft.setSwapBytes(true)`, so TFT_eSPI emits the big-endian byte order the ST7789
expects. Pre-swapping in the header would double the flash cost for identical
output.

## Tests

The matcher and state engine have no Arduino dependencies, so both compile and
run on the host — a bad keyword, a duplicate line or a malformed generated header
fails in under a second instead of after a full Arduino build.

```bash
cd test
g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o personality_test personality_test.cpp && ./personality_test
g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o brain_test       brain_test.cpp       && ./brain_test
```

- `personality_test` checks the **data**: duplicate lines, duplicate keywords,
  moods that no rule can reach, every question having at least one answer, every
  answer pointing at a question that exists.
- `brain_test` checks the **behaviour** by compiling the real pipeline: that
  repeated input does not repeat a line, that a clear keyword is not hijacked by
  a pending question, that farewells are detected, and that a grudge is
  eventually recoverable.

The shims that make this possible are `test/Arduino.h` (a `String` over
`std::string`, `PROGMEM`, a test-driven `millis()`) and `test/Preferences.h`
(an in-memory NVS).

## Runtime behaviour

- **Boot:** backlight on → NVS state loaded (grudge, session count, unplug tally)
  → greeting → Wi-Fi → NTP (up to 12 s) → a time-aware greeting. If
  `OWNER_CHAT_ID` is set it also greets you in chat.
- **Chat:** polled once a second. Each text message goes through the brain and is
  answered on the panel *and* in chat, followed by the mood image as a photo —
  JPEG streamed straight out of PROGMEM via `sendPhotoByBinary()`, so no
  filesystem partition is needed. Sent only when the mood actually changes.
- **Idle loop:** every `IDLE_TIMEOUT_MS` the companion speaks unprompted, and the
  line escalates with total neglect.
- **Display keepalive:** every 20 s the backlight is re-asserted and the current
  scene re-pushed; every 10 min the panel is fully re-initialised. A panel glitch
  therefore heals itself within 20 s instead of leaving a grey screen.
- **Wi-Fi watchdog:** on link loss the companion switches to the `no_internet`
  mood and retries every 10 s, announcing itself when the link returns.

## Display notes

Rotation is 0 (135 wide × 240 tall) so the portrait sprite fills the panel. The
speech band is drawn bottom-anchored over the chest/shirt area: greedy word wrap
into at most 6 lines, a `#1082` charcoal fill, and an accent rule across the top
of the band. The face region (roughly y < 150) is never overdrawn.

### Font choice is load-bearing

TFT_eSPI's `setTextFont(2)` is **not** a small GLCD font — it is the 16 px
`Font16` graphics font whose advance is rounded up to a whole byte multiple
(`TFT_eSPI.cpp`: `cwidth = (cwidth + 6) / 8; cwidth *= 8;`). On a 135 px panel
that yields an 8–16 px advance, i.e. ~7–13 characters per line. This firmware
therefore uses **font 1** (Adafruit GLCD 5×7 in 6×8 cells, fixed 6 px advance),
giving 20 characters per line at a 10 px line height.

Verify layout changes without hardware:

```bash
python3 scripts/render_mockup.py     # -> preview/screen_mockup.png (+ per-mood PNGs)
```

It renders glyphs from TFT_eSPI's own `Fonts/glcdfont.c` and reuses the same wrap
algorithm and band constants as the `.ino`, so the preview is pixel-accurate
rather than approximate.

## Tools

`tools/` contains standalone sketches used to bring up an unknown panel: pin
dumps via `getSetup()`, six raw ST7789 init sequences, an independent
Adafruit_ST7789 stack, and library-free bit-banged drivers. They are kept because
they are the fastest way to answer "is this the wiring or the code" on a board
with no documentation.

## License

MIT — see `LICENSE`.
