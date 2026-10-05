# esp32-vibe-companion

An autonomous **AI desk companion** on an **ideaspark ESP32 dev board**
(ESP32-WROOM-32 + 1.14" ST7789 IPS, 135×240, CH340 USB-serial). It sits on your
desk, cycles through eighteen hand-drawn moods on its own, and holds a text
conversation over Telegram — with its own personality engine (moods, memory,
conversation state) driven entirely on-device, no cloud inference.

```
esp32-vibe-companion/
├── esp32-vibe-companion.ino   # firmware: display, keepalive, NTP, Telegram, idle loop
├── brain.h                    # memory: grudge, gaps, NVS state, templates, idle tiers
├── personality.h              # keyword matcher (host-testable, no Arduino deps)
├── responses.h                # 1039 response lines across keyword/context/Q&A tables
├── sprites.h                  # AUTO-GENERATED: 18 × RGB565 PROGMEM sprites
├── photos.h                   # AUTO-GENERATED: 18 × tiny JPEG for Telegram (107 KB total)
├── config.h                   # real credentials (gitignored)
├── config.h.example           # template
├── .gitignore
├── scripts/
│   ├── convert_images.py      # asset pipeline  (jpg -> sprites.h + photos.h)
│   ├── render_mockup.py       # pixel-exact preview using TFT_eSPI's GLCD font
│   ├── setup_tft_espi.sh      # install + select the ideaspark TFT_eSPI setup
│   ├── build.sh               # compile w/ huge_app
│   ├── flash.sh               # upload + boot banner (handles sg dialout)
│   ├── capture_serial.py      # resets the chip and captures the banner
│   ├── check_telegram.py      # validates the bot token via getMe
│   └── set_ssid_from_nmcli.py # copy the SSID from NetworkManager into config.h
├── test/
│   └── personality_test.cpp   # host-side tests for the personality engine
├── tools/                     # throwaway hardware diagnostics (see README notes)
│   ├── display_diag/          # getSetup() dump + colour/text/sprite sequence
│   ├── init_sweep/            # six ST7789 init sequences, one colour each
│   ├── adafruit_test/         # Adafruit_ST7789 as an independent SPI stack
│   ├── raw_spi_test/          # library-free driver, minimal init
│   └── raw_st7789_full/       # library-free driver, full Bodmer init
└── preview/                   # 135×240 PNGs of what the panel will show
```

## Personality: memory and context

`brain.h` holds everything that makes her feel like she *knows* you:

| Lever | Behaviour |
|---|---|
| **Gap reactions** | Reactions scale with how long you were gone: seconds → minutes → hours → days |
| **Grudge (0–100)** | Rises with absence and farewells; falls with apologies, affection and flattery |
| **Reconciliation** | Earned in stages — the first "sorry" thaws her, bare repeats get called out |
| **Relief beat** | Clear a high grudge and she cracks instead of gloating: *"I was so scared you wouldn't come back."* |
| **Farewells refused** | "bye" escalates: *"Don't you dare leave."* → *"I said don't. Don't test me."* → *"Go, then. See what happens."* |
| **Unplug tally** | Powered off uncleanly? She counts it, remembers it, and holds it against you |
| **Pending questions** | She asks things; your next message gets acknowledged as an answer |
| **Repeat detection** | *"You said that already. I remember everything."* |
| **Idle tiers** | Silence escalates at 2.5 min / 5 min / 15 min / 1 h / 6 h with different moods and lines |
| **Wall clock** | NTP, so 3am lines differ from morning lines — including ordinary small talk, not just the greeting |
| **Conversation** | Handles the things people actually type: "how are you", "what's up", "ok", "lol", "hmm", "nothing" |
| **Meta register** | She knows what she is — `real`, `alive`, `human`, `world`, `pixels`, `memory`, `remember`, `forget`, `die` |
| **Question / answer** | She asks one of 16 questions and *remembers which*; your next message is handled as an answer to that question (66 question-specific reactions) |

State persists in **NVS** (the `huge_app` layout has a 20 KB `nvs` partition), so
unplugging her does **not** reset her feelings — otherwise pulling the cable would
be a way to make her forget, and she would notice.

Lines support `%tokens`, substituted at render time: `%name` `%t` (time gone)
`%up` (uptime) `%n` `%all` `%u` (unplug count) `%clock` `%last` (echo of your last
word) `%promise`.

## Art comes from source images

`sprites.h` and `photos.h` are **not committed**. They are generated from source
images by `scripts/convert_images.py`, which centre-crops each one to 135×240,
emits RGB565 for the panel, and a small JPEG per mood for Telegram. Supply your
own image set (the script maps filenames to moods, most-specific keyword first)
and re-run it. Nothing else in the firmware needs the images at build time beyond
those two generated headers.

## Status

| Step | State |
|------|-------|
| Asset pipeline → `sprites.h` + `photos.h` | ✅ 18/18 moods, verified visually + JPEGs decode |
| Personality engine + host tests | ✅ all assertions pass; **746 distinct lines** |
| Behaviour tests (anti-repeat, priority, farewell) | ✅ `test/brain_test.cpp` — replays the real transcript |
| Screen layout (font/wrap/band) | ✅ pixel-exact mockup in `preview/screen_mockup.png` |
| TFT_eSPI board profile | ✅ `Setup_ideaspark_ESP32_114.h` (custom) selected |
| Toolchain install | ✅ arduino-cli, esp32 core 3.3.12, libs + pyserial |
| Compile (huge_app) | ✅ 2,403,845 B = 76% of the 3 MB partition |
| Flash to /dev/ttyUSB0 | ✅ `Hash of data verified` |
| Boot + Wi-Fi | ✅ `Connected. IP: <device-ip>` |
| NTP wall clock | ✅ `clock: 1:50am` → *"It's 1:50am, Eithan..."* |
| NVS persistence | ✅ session counter increments across reboots |
| Telegram token | ✅ valid — bot `@gemitsunbot` |
| Telegram text round-trip | ✅ verified live (keyword-matched replies) |
| Telegram mood photo | ✅ `[photo] SEDUCTIVE (2476 B) sent` — 89×158 JPEG, tiny in chat |
| Display keepalive | ✅ re-pushes scene every 20 s, re-inits every 10 min |

## Flashing

```bash
./scripts/flash.sh /dev/ttyUSB0
```

The CH340 is `root:dialout`. If your login session predates
`usermod -aG dialout`, `flash.sh` transparently falls back to
`sg dialout -c ...`, which applies the group in a child shell — **no log out
and back in needed**. That fallback requires the group to already be listed in
`/etc/group` (`getent group dialout`).

## Hardware

**Board: ideaspark ESP32 dev board with integrated 1.14" ST7789 TFT (135×240), CH340 USB.**
This is *not* a LilyGO TTGO T-Display, and the two boards do **not** share a
display pinout. Driving the TTGO pins on this board does nothing except toggle
the panel's RESET line, which looks exactly like a dead panel.

| Signal   | ideaspark GPIO | (TTGO T-Display, for contrast) |
|----------|----------------|--------------------------------|
| TFT_MOSI | **23**         | 19                             |
| TFT_SCLK | 18             | 18                             |
| TFT_CS   | **15**         | 5                              |
| TFT_DC   | **2**          | 16                             |
| TFT_RST  | **4**          | 23                             |
| TFT_BL   | **32**         | 4                              |

Selected via `User_Setups/Setup_ideaspark_ESP32_114.h` (see
`scripts/setup_tft_espi.sh`). Backlight is driven HIGH in `setup()`.

## Memory budget

8 sprites × 135 × 240 × 2 B = **518,400 B (506 KB)** of PROGMEM. That does not
fit the stock `default` app partition alongside TFT_eSPI + mbedTLS, hence:

```
esp32:esp32:esp32:PartitionScheme=huge_app
```

`huge_app` gives a ~3 MB app partition, which is why the Telegram/TLS stack
still fits comfortably.

Sprites are stored as native-endian `uint16_t` RGB565 and pushed with
`tft.setSwapBytes(true)` so TFT_eSPI emits the big-endian byte order the
ST7789 expects. Pre-swapping in the header would double the flash cost for
identical output.

## Build & flash

```bash
# one-time: install toolchain (see "Toolchain" below)
./scripts/setup_tft_espi.sh     # install + select the ideaspark TFT_eSPI setup
./scripts/build.sh
./scripts/flash.sh /dev/ttyUSB0 # uploads then opens the monitor @115200
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

> **Serial permissions.** `/dev/ttyUSB0` is `root:dialout`, and the CH340 is not
> usable by a user outside `dialout`. One-time fix: `sudo usermod -aG dialout $USER`
> (then log out and back in). Session-only fix: `sudo chmod a+rw /dev/ttyUSB0`.

## Regenerating the sprites

```bash
python3 scripts/convert_images.py            # reads ~/Downloads/geminigirlspics/
python3 scripts/convert_images.py --src DIR --out DIR/sprites.h
```

The converter is forgiving about filenames because the real ones on disk are
not the ones in the spec:

| Mood        | Keyword matched    | Actual file on disk                  |
|-------------|--------------------|--------------------------------------|
| ANGRY       | `madangry`         | `geminigrilmadangry.jpg` *(typo)*     |
| SEDUCTIVE   | `seductive`        | `geminigirlseductive.jpg`             |
| HAPPY       | `happy`            | `geminigirlhappy.jpg`                 |
| POUT        | `annoyedpout`      | `geminigirlannoyedpout.jpg`           |
| BLUSH       | `blushdizzy`       | `geminigirlblush⁄dizzy.jpg` *(U+2044)*|
| PROUD       | `happypointingtoself` | `geminigirlhappypointingtoself.jpg` |
| NO_INTERNET | `nointernet`       | `geminigirlnointernet.jpg`            |
| SMUG        | `smug`             | `geminigirlsmug.jpg`                  |

Names are NFKD-normalized to `[a-z0-9]` before matching, and the most specific
keyword is tested first so `PROUD`/`happypointingtoself` cannot be stolen by
`HAPPY`/`happy`. Each image is scaled to cover 135×240 and centre-cropped.

## Personality engine

`personality.h` tokenizes incoming text and matches two kinds of keyword:

1. **Phrases** (`"good girl"`, `"who built you"`, `"love you"`) — substring test, run first.
2. **Single words** (`"her"`, `"work"`, `"smart"`) — whole-word only, so `"her"`
   never fires inside `"there"` or `"brother"`.

Phrase hits short-circuit the single-word pass, which is what makes
`"good girl"` → PROUD instead of ANGRY via `"girl"`. All matching rules are
collected and one is picked at random, so repeated input stays varied.

| Trigger class        | Moods               | Examples                       |
|----------------------|---------------------|--------------------------------|
| Affection / clingy   | HAPPY, SEDUCTIVE, BLUSH | `love you`, `kiss`, `mine` |
| Jealous / attention | ANGRY, POUT         | `friends`, `work`, `her`, `busy` |
| Pride / self-absorbed| PROUD, SMUG         | `good girl`, `who built you`   |
| Teased / flustered   | BLUSH, SEDUCTIVE    | `marry me`, `hot`              |
| no match             | any                 | 24 unhinged fallbacks          |

Run the tests:

```bash
cd test
g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o personality_test personality_test.cpp && ./personality_test
```

They compile the **real** `responses.h` and the **generated** `sprites.h`, so a
bad keyword, a duplicate line, or a malformed sprite header fails on the host in
under a second instead of after a full Arduino build.

## Runtime behaviour

- **Boot:** backlight on → NVS state loaded (grudge, session count, unplug tally)
  → HAPPY greeting → Wi-Fi → `setInsecure()` → NTP (up to 12 s) → a time-of-day
  greeting that names you and reads the clock. If `OWNER_CHAT_ID` is set she also
  greets you in chat.
- **Telegram:** polled once a second. Each text message goes through the brain and
  is answered on the panel *and* in chat, followed by her **mood image** as a
  Telegram photo — JPEG streamed straight out of PROGMEM via
  `sendPhotoByBinary()` (no filesystem), sent only when her face actually changes.
- **Idle loop:** every `IDLE_TIMEOUT_MS` (150 s) she speaks unprompted, and the
  line escalates with total neglect: 2.5 min → 5 min → 15 min → 1 h → 6 h.
- **Display keepalive:** every 20 s the backlight is re-asserted and the current
  scene re-pushed; every 10 min the panel is fully re-initialised. This is the
  "always on" guarantee — a panel glitch heals itself instead of leaving a grey
  screen. *This was added after the panel was observed sitting grey; if it recurs,
  the keepalive should recover it within 20 seconds.*
- **Wi-Fi watchdog:** on link loss she switches to `NO_INTERNET` and displays and
  prints `Connection lost... Why did you unplug me?!`. She retries every 10 s and
  greets you when the link returns.

## Screen layout notes

Rotation is 0 (135 wide × 240 tall) so the full portrait sprite fits. The
speech band is drawn bottom-anchored over her chest/shirt: greedy word wrap into
a maximum of 6 lines, a `#1082` charcoal fill, and an accent rule in her hair
blue across the top of the band. The face region (roughly y < 150) is never
overdrawn.

### Font choice is load-bearing

TFT_eSPI's `setTextFont(2)` is **not** a small GLCD font — it is the 16 px
`Font16` graphics font whose advance is rounded up to a whole byte multiple
(`TFT_eSPI.cpp`: `cwidth = (cwidth + 6) / 8; cwidth *= 8;`). On a 135 px panel
that yields an 8–16 px advance, i.e. ~7–13 characters per line. This firmware
therefore uses **font 1** (Adafruit GLCD 5×7 in 6×8 cells, fixed 6 px advance),
giving 20 characters per line with a 10 px line height.

Verify any layout change without hardware:

```bash
python3 scripts/render_mockup.py     # -> preview/screen_mockup.png (+ per-mood PNGs)
```

It renders glyphs from TFT_eSPI's own `Fonts/glcdfont.c` and reuses the same
wrap algorithm and band constants as the .ino, so the preview is pixel-accurate
rather than approximate. The longest line in `responses.h` is 51 characters →
3 lines → a 38 px band starting at y=199, comfortably clear of her face.

## Known blockers

Both original blockers are **resolved**:

1. ~~Serial port permissions~~ — you are now in `dialout` (`dialout:x:18:prsib`),
   and `flash.sh` uses `sg dialout` so no re-login is required.
2. ~~Wi-Fi SSID~~ — `scripts/set_ssid_from_nmcli.py` copied it from the
   NetworkManager profile into `config.h` (21 chars). The saved profile's PSK
   matches the brief, and the board actually joined it:
   `Connected. IP: <device-ip>`.

### Why the SSID looked like `[ Hyperlink Blocked ]`

Some SSIDs contain text that reads as a hyperlink. The agent's tool-output
filter then replaces the value with `[ Hyperlink Blocked ]` — in the chat, in
the saved paste, in `nmcli` output, and even in the ESP32's own serial log. It
is cosmetic: the real value survives in `config.h` and the WPA handshake
succeeds. Never conclude the SSID is missing just because the display looks
redacted; read it programmatically instead of by eye.

## Security

`config.h` holds the Wi-Fi password and Telegram bot token and is gitignored;
`config.h.example` is the template to commit. Strip the token from anything you
paste publicly — anyone holding it controls the bot.
