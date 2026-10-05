/*
 * display_diag -- isolate a blank/black panel.
 *
 * Flashes a sequence that separates three failure modes:
 *   1. fillScreen() colours  -> does the panel accept SPI commands at all?
 *   2. drawString()          -> does text render?
 *   3. pushImage() from PROGMEM, with and without setSwapBytes -> does the
 *      sprite path (the thing the firmware actually uses) work?
 *
 * Every step is announced on serial, so the user's description of the panel can
 * be matched line-for-line against the log. It also dumps TFT_eSPI's
 * getSetup(), which reports the config the *compiler* baked in - not what we
 * think we configured.
 *
 * sprites.h is symlinked from the project root so this tests the real arrays.
 */

#include <TFT_eSPI.h>
#include "sprites.h"

TFT_eSPI tft = TFT_eSPI();
setup_t  user;

#ifndef TFT_BL
#define TFT_BL 4
#endif

static void step(const char* label) {
  Serial.print(">> STEP: ");
  Serial.println(label);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== TFT_eSPI DISPLAY DIAGNOSTIC ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.print("forced backlight pin ");
  Serial.print(TFT_BL);
  Serial.println(" HIGH");

  tft.init();
  tft.getSetup(user);

  Serial.println("--- compiled-in setup (from getSetup) ---");
  Serial.print("TFT_eSPI version : "); Serial.println(user.version);
  Serial.print("processor code   : 0x"); Serial.println(user.esp, HEX);
  Serial.print("interface        : "); Serial.println(user.serial == 1 ? "SPI" : "Parallel");
  Serial.print("driver code      : 0x"); Serial.println(user.tft_driver, HEX);
  Serial.print("declared size    : "); Serial.print(user.tft_width);
                                       Serial.print(" x "); Serial.println(user.tft_height);
  Serial.print("offsets r0       : "); Serial.print(user.r0_x_offset);
                                       Serial.print(","); Serial.println(user.r0_y_offset);
  Serial.print("MOSI             : "); Serial.println(user.pin_tft_mosi);
  Serial.print("MISO             : "); Serial.println(user.pin_tft_miso);
  Serial.print("SCLK             : "); Serial.println(user.pin_tft_clk);
  Serial.print("CS               : "); Serial.println(user.pin_tft_cs);
  Serial.print("DC               : "); Serial.println(user.pin_tft_dc);
  Serial.print("RST              : "); Serial.println(user.pin_tft_rst);
  Serial.print("BL               : "); Serial.println(user.pin_tft_led);
  Serial.print("BL active        : "); Serial.println(user.pin_tft_led_on == HIGH ? "HIGH" : "LOW");
  Serial.print("SPI frequency    : "); Serial.println(user.tft_spi_freq / 10.0);

  uint16_t fonts = tft.fontsLoaded();
  Serial.print("fonts loaded     : GLCD="); Serial.print((fonts & (1 << 1)) ? 1 : 0);
  Serial.print(" F2="); Serial.print((fonts & (1 << 2)) ? 1 : 0);
  Serial.println();

  // ---------------------------------------------------------------------
  // SPI readback probe. TFT_SDA_READ is defined in Setup25, so TFT_eSPI
  // drives the bidirectional SDA (MOSI) line half-duplex and we can actually
  // ask the controller who it is. ST7789 RDDID (0x04) returns
  // [dummy][manufacturer][version]. A live, correctly wired panel answers
  // with values that are neither 0x00 nor 0xFF; a dead/absent/miswired panel
  // usually answers 0x00 or 0xFF on every index.
  // ---------------------------------------------------------------------
  Serial.println("--- SPI readback probe (is the panel electrically answering?) ---");
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t id = tft.readcommand8(0x04 /* ST7789_RDDID */, i);
    Serial.print("RDDID[");
    Serial.print(i);
    Serial.print("] = 0x");
    if (id < 0x10) Serial.print("0");
    Serial.println(id, HEX);
    delay(10);
  }

  tft.setRotation(0);
  tft.setTextWrap(false);
  Serial.print("after setRotation(0) -> width=");
  Serial.print(tft.width());
  Serial.print(" height=");
  Serial.println(tft.height());
  Serial.println("--- beginning visual sequence ---");
}

void loop() {
  // ---------------------------------------------------------------------
  // Backlight phase. This separates the two failure modes that both look
  // like "blank screen":
  //   - backlight path dead  -> the panel never glows, at all
  //   - backlight fine, data path dead -> panel glows grey/white but shows
  //     no image
  // If the user sees no brightness change here, the problem is power/GPIO or
  // the panel is not what we think it is -- not SPI settings.
  // ---------------------------------------------------------------------
  step("backlight OFF  (expect screen to go DARK)");
  digitalWrite(TFT_BL, LOW);   delay(1800);

  step("backlight ON   (expect screen to GLOW brighter)");
  digitalWrite(TFT_BL, HIGH);  delay(1800);

  step("backlight OFF  (expect DARK again)");
  digitalWrite(TFT_BL, LOW);   delay(1800);

  step("backlight ON   (expect GLOW again)");
  digitalWrite(TFT_BL, HIGH);  delay(1800);

  step("backlight PWM ramp (expect a slow fade down then up)");
  for (int v = 255; v >= 0; v -= 5) { analogWrite(TFT_BL, v); delay(12); }
  for (int v = 0; v <= 255; v += 5) { analogWrite(TFT_BL, v); delay(12); }
  digitalWrite(TFT_BL, HIGH);
  delay(500);

  step("fillScreen RED      (expect SOLID RED)");
  tft.fillScreen(TFT_RED);      delay(2000);

  step("fillScreen GREEN    (expect SOLID GREEN)");
  tft.fillScreen(TFT_GREEN);    delay(2000);

  step("fillScreen BLUE     (expect SOLID BLUE)");
  tft.fillScreen(TFT_BLUE);     delay(2000);

  step("fillScreen WHITE    (expect SOLID WHITE)");
  tft.fillScreen(TFT_WHITE);    delay(2000);

  step("fillScreen BLACK + text 'DIAG OK' (expect BLACK bg, WHITE text)");
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, SPRITE_W, SPRITE_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("DIAG OK", 6, 6, 2);
  tft.drawString("135x240", 6, 30, 2);
  tft.drawString("if you can read", 6, 60, 1);
  tft.drawString("this, SPI works", 6, 72, 1);
  delay(3000);

  step("pushImage HAPPY sprite, setSwapBytes(true)  (expect correct colours)");
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, SPRITE_W, SPRITE_H, getSprite(MOOD_HAPPY));
  delay(3000);

  step("pushImage ANGRY sprite, setSwapBytes(FALSE) (expect WRONG colours)");
  tft.setSwapBytes(false);
  tft.pushImage(0, 0, SPRITE_W, SPRITE_H, getSprite(MOOD_ANGRY));
  delay(3000);
}
