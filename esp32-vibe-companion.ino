/*
 * esp32-vibe-companion
 * --------------------
 * A small desk bot on an ideaspark ESP32 dev board (ESP32-WROOM-32 + 1.14"
 * ST7789 135x240 TFT, CH340 USB). It shows a face and answers short messages.
 *
 *   - Full-screen RGB565 sprites in PROGMEM (sprites.h) -> needs `huge_app`.
 *   - Matching JPEGs in PROGMEM (photos.h), sent back over Telegram so the
 *     chat matches the panel. No filesystem needed: sendPhotoByBinary() streams
 *     them from memory.
 *   - Replies: keyword rules + context tables (responses.h) evaluated by
 *     brain.h against live state -- uptime, idle time, message counts, whether
 *     power was cut, and any pending question.
 *   - Counters persist in NVS across reboots.
 *   - NTP wall clock, so the time replies are correct.
 *   - Autonomous idle loop that reports status when nothing is happening.
 *   - Display keepalive: re-asserts the backlight and re-pushes the scene so a
 *     panel glitch cannot leave it stuck on a grey screen.
 *   - Wi-Fi watchdog.
 *
 * Required libraries: TFT_eSPI, UniversalTelegramBot, ArduinoJson.
 * TFT_eSPI must be configured for this board -- run scripts/setup_tft_espi.sh,
 * which installs and selects User_Setups/Setup_ideaspark_ESP32_114.h. The LCD
 * pinout differs from the TTGO T-Display: MOSI 23, SCLK 18, CS 15, DC 2,
 * RST 4, BL 32.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <TFT_eSPI.h>
#include <esp_system.h>   // esp_random()

#include "config.h"
#include "sprites.h"
#include "photos.h"
#include "responses.h"
#include "personality.h"
#include "brain.h"

// The selected TFT_eSPI setup (User_Setups/Setup_ideaspark_ESP32_114.h) defines
// TFT_BL as GPIO32 for the ideaspark board. Fall back just in case.
#ifndef TFT_BL
#define TFT_BL 32
#endif

// ---------------------------------------------------------------- display cfg
static const uint32_t SERIAL_BAUD   = 115200;
static const uint16_t BAND_BG       = 0x1082;   // deep charcoal
static const uint16_t BAND_ACCENT   = 0x5D9F;   // accent blue
static const uint16_t TEXT_COLOR    = 0xFFFF;
static const int      FONT_ID       = 1;      // GLCD: 6px advance, 8px tall -> 20 chars/line
static const int      LINE_H        = 10;     // 8px glyph + 2px leading
static const int      TEXT_PAD_X    = 5;
static const int      TEXT_MAX_W    = SPRITE_W - (2 * TEXT_PAD_X);
static const int      BAND_BOTTOM   = 3;
static const int      MAX_LINES     = 6;

// Keepalive: re-push the scene every 20 s; fully re-init the panel every 10 min.
// This is the "always on" guarantee -- if the panel ever loses sync it heals
// itself instead of sitting there grey.
static const unsigned long KEEPALIVE_MS = 20000UL;
static const unsigned long REINIT_MS    = 600000UL;

TFT_eSPI tft = TFT_eSPI();

// ------------------------------------------------------------------- telegram
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
static const unsigned long BOT_POLL_MS = 1000;
unsigned long lastBotPoll = 0;

// ---------------------------------------------------------------------- state
static const char* WIFI_LOST_LINE = "Connection lost. Retrying every 10 seconds.";

Mood          currentMood    = MOOD_HAPPY;
String        currentText    = "";
unsigned long lastUserAt     = 0;   // last inbound message (drives idle tier)
unsigned long lastIdleAt     = 0;   // when it last spoke unprompted
bool          wifiWasUp      = false;
unsigned long lastWifiRetry  = 0;
unsigned long lastKeepalive  = 0;
unsigned long lastReinit     = 0;
static int    lastPhotoMood  = -1;

// ===========================================================================
//  Rendering
// ===========================================================================

// Greedy word wrap into `out` (each entry points into the mutated `src` copy).
static int wrapText(char* src, const char* out[], int maxLines) {
  int   count = 0;
  char* word  = strtok(src, " ");
  char  line[96];
  line[0] = '\0';

  while (word != nullptr) {
    char candidate[96];
    if (line[0] == '\0') {
      snprintf(candidate, sizeof(candidate), "%s", word);
    } else {
      snprintf(candidate, sizeof(candidate), "%s %s", line, word);
    }

    if (tft.textWidth(candidate, FONT_ID) <= TEXT_MAX_W) {
      snprintf(line, sizeof(line), "%s", candidate);
    } else {
      if (count < maxLines) out[count++] = strdup(line);
      snprintf(line, sizeof(line), "%s", word);
    }
    word = strtok(nullptr, " ");
  }

  if (line[0] != '\0' && count < maxLines) out[count++] = strdup(line);
  return count;
}

// Draw sprite + optional speech band across the lower part of the panel.
static void renderScene(Mood mood, const char* text) {
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, SPRITE_W, SPRITE_H, getSprite(mood));

  if (text == nullptr || text[0] == '\0') return;

  char buf[256];
  snprintf(buf, sizeof(buf), "%s", text);        // strtok mutates, keep caller's
  const char* lines[MAX_LINES];
  int n = wrapText(buf, lines, MAX_LINES);

  int bandH = (n * LINE_H) + 8;
  int bandY = SPRITE_H - bandH - BAND_BOTTOM;
  int textY = bandY + 4;

  tft.fillRect(0, bandY, SPRITE_W, bandH, BAND_BG);
  tft.drawFastHLine(0, bandY, SPRITE_W, BAND_ACCENT);

  tft.setTextColor(TEXT_COLOR, BAND_BG);
  tft.setTextDatum(TL_DATUM);

  for (int i = 0; i < n; i++) {
    tft.drawString(lines[i], TEXT_PAD_X, textY + (i * LINE_H), FONT_ID);
    free((void*)lines[i]);
  }
}

// Single funnel for everything it says: updates state, draws, logs.
static void speak(Mood mood, const String& text) {
  currentMood = mood;
  currentText = text;
  renderScene(mood, text.c_str());
  Serial.printf("[%s] %s\n", moodName(mood), text.c_str());
}

// ===========================================================================
//  Telegram photo: stream a mood JPEG straight out of PROGMEM
// ===========================================================================

static const uint8_t* g_photo     = nullptr;
static uint32_t       g_photoLen  = 0;
static uint32_t       g_photoOff  = 0;
static const uint32_t PHOTO_CHUNK = 1024;

static bool  photoMore()   { return g_photoOff < g_photoLen; }
static byte* photoNextBuf() { return (byte*)(g_photo + g_photoOff); }
static int   photoNextLen() {
  uint32_t n = g_photoLen - g_photoOff;
  if (n > PHOTO_CHUNK) n = PHOTO_CHUNK;
  g_photoOff += n;
  return (int)n;
}

static void sendMoodPhoto(const String& chat_id, Mood mood) {
  if (!SEND_MOOD_PHOTOS) return;
  if ((int)mood == lastPhotoMood) return;      // only when the face changes

  const MoodPhoto& p = MOOD_PHOTOS[(int)mood];
  g_photo    = p.data;
  g_photoLen = p.len;
  g_photoOff = 0;

  String res = bot.sendPhotoByBinary(chat_id, "image/jpeg", (int)p.len,
                                     photoMore, nullptr, photoNextBuf, photoNextLen);
  bool ok = res.indexOf("\"ok\":true") >= 0;
  Serial.printf("[photo] %s (%u B) %s\n", moodName(mood), (unsigned)p.len,
                ok ? "sent" : res.c_str());
  if (ok) lastPhotoMood = (int)mood;
}

// ===========================================================================
//  Wi-Fi watchdog
// ===========================================================================

static bool connectWifi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("Joining \"%s\" ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("Wi-Fi failed.");
  return false;
}

static void handleWifiLoss() {
  if (wifiWasUp) {
    Serial.println(WIFI_LOST_LINE);
    wifiWasUp = false;
    speak(MOOD_NO_INTERNET, WIFI_LOST_LINE);
  } else if (currentMood != MOOD_NO_INTERNET) {
    speak(MOOD_NO_INTERNET, WIFI_LOST_LINE);
  }
}

// The wall clock, so the time replies are correct.
static void startClock() {
  configTzTime(TIMEZONE_TZ, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
}

// ===========================================================================
//  Telegram
// ===========================================================================

static void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    if (bot.messages[i].type != "message") continue;

    const String chat_id = bot.messages[i].chat_id;
    const String text    = bot.messages[i].text;
    if (text.length() == 0) continue;

    if (OWNER_CHAT_ID[0] != '\0' && chat_id != String(OWNER_CHAT_ID)) {
      Serial.printf("Ignoring chat %s (not the owner)\n", chat_id.c_str());
      continue;
    }

    lastUserAt = millis();
    VibeReply rep = brainReply(text.c_str());

    speak(rep.mood, rep.text);
    bot.sendMessage(chat_id, rep.text, "");
    sendMoodPhoto(chat_id, rep.mood);

    lastUserAt = millis();
  }
}

static void pollTelegram() {
  if (millis() - lastBotPoll < BOT_POLL_MS) return;
  lastBotPoll = millis();

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// ===========================================================================
//  Autonomous idle loop -- reports status when nothing is happening
// ===========================================================================

static void idleTick() {
  if (millis() - lastIdleAt < IDLE_TIMEOUT_MS) return;
  lastIdleAt = millis();

  uint32_t neglect = (millis() - lastUserAt) / 1000UL;
  VibeIdle it = brainIdle(neglect);
  if (it.text.length() == 0) return;

  currentMood = it.mood;
  currentText = it.text;
  renderScene(it.mood, it.text.c_str());
  Serial.printf("[idle %lus/%s] %s\n", (unsigned long)neglect, moodName(it.mood),
                it.text.c_str());
}

// ===========================================================================
//  Display keepalive -- the "always on" guarantee
// ===========================================================================

static void displayKeepalive() {
  unsigned long now = millis();

  if (now - lastReinit >= REINIT_MS) {
    lastReinit = now;
    Serial.println("[display] periodic re-init");
    tft.init();
    tft.setRotation(0);
    tft.setTextWrap(false);
    digitalWrite(TFT_BL, HIGH);
  }

  if (now - lastKeepalive >= KEEPALIVE_MS) {
    lastKeepalive = now;
    digitalWrite(TFT_BL, HIGH);              // re-assert the backlight
    renderScene(currentMood, currentText.c_str());
  }
}

// ===========================================================================
//  Setup / loop
// ===========================================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println("\n== esp32-vibe-companion ==");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);            // enable backlight

  tft.init();
  tft.setRotation(0);                    // 135 wide x 240 tall
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(false);

  randomSeed(esp_random());
  brainBegin();
  Serial.printf("brain: msgsEver=%u unplugs=%u session=%u clock=%s\n",
                (unsigned)B.messagesEver, (unsigned)B.unplugCount,
                (unsigned)B.sessionCount, clockValid() ? "ok" : "not yet");

  speak(MOOD_HAPPY, "Booting up...");
  lastUserAt = lastIdleAt = lastKeepalive = lastReinit = millis();

  wifiWasUp = connectWifi(20000);
  if (wifiWasUp) {
    secured_client.setInsecure();
    startClock();

    // Give NTP a real chance so the first line can use the clock. 12 s is
    // generous: it normally lands in 1-5 s, but a cold DNS cache can push it
    // out, and a clock-less greeting reads worse than a slightly slower boot.
    unsigned long t0 = millis();
    while (!clockValid() && millis() - t0 < 12000UL) delay(250);
    Serial.printf("clock: %s\n", clockValid() ? clockString().c_str() : "unavailable (NTP slow)");

    VibeReply g = brainBootGreeting();
    speak(g.mood, g.text);

    // if the owner is pinned, greet them in chat too
    if (OWNER_CHAT_ID[0] != '\0') {
      bot.sendMessage(String(OWNER_CHAT_ID), g.text, "");
      sendMoodPhoto(String(OWNER_CHAT_ID), g.mood);
    }
  } else {
    handleWifiLoss();
  }

  lastIdleAt = millis();
}

void loop() {
  // --- Wi-Fi watchdog -----------------------------------------------------
  if (WiFi.status() != WL_CONNECTED) {
    handleWifiLoss();
    if (millis() - lastWifiRetry > 10000UL) {
      lastWifiRetry = millis();
      if (connectWifi(15000)) {
        wifiWasUp = true;
        lastPhotoMood = -1;
        startClock();
        speak(MOOD_HAPPY, "Reconnected.");
        lastUserAt = millis();
      }
    }
  } else {
    wifiWasUp = true;
    pollTelegram();
  }

  idleTick();
  displayKeepalive();
  delay(20);
}
