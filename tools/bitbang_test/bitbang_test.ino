/*
 * bitbang_test -- drive the ST7789 with NO SPI peripheral at all.
 *
 * Three separate SPI implementations produced no pixels on this panel:
 *   1. TFT_eSPI (its own low-level ESP-IDF SPI setup)
 *   2. a library-free driver over Arduino's SPIClass + GPIO matrix
 *   3. Adafruit_ST7789 (Adafruit_SPITFT over SPIClass)
 * Yet the factory firmware displayed colours, and the backlight (GPIO4) is
 * clearly working because the panel glows dark grey.
 *
 * Every one of those three routes data through the ESP32's SPI peripheral and
 * the GPIO matrix. This sketch removes both: MOSI/SCLK/DC/CS are plain GPIOs
 * toggled in software at ~250 kHz. Nothing about SPI host selection, DMA,
 * pin-muxing or clock dividers can matter here.
 *
 *   - If the panel shows colours now  -> the wiring and panel are fine and the
 *     fault is in how the SPI peripheral is being configured/routed.
 *   - If it is still blank            -> the panel is not electrically
 *     receiving data, i.e. the flex/data lines, not the software.
 *
 * Full Bodmer init sequence, then long-dwell colours.
 */

#include <Arduino.h>

#define PIN_MOSI 19
#define PIN_SCLK 18
#define PIN_CS    5
#define PIN_DC   16
#define PIN_RST  23
#define PIN_BL    4

#define TFT_W 135
#define TFT_H 240
#define COLSTART 52
#define ROWSTART 40

#define D 2   // half-bit delay in microseconds -> ~250 kHz

static inline void sclk_hi() { digitalWrite(PIN_SCLK, HIGH); }
static inline void sclk_lo() { digitalWrite(PIN_SCLK, LOW); }

// Mode 0: data is sampled on the rising edge, so set MOSI first, then clock.
static void bb_byte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(PIN_MOSI, (b >> i) & 1);
    delayMicroseconds(D);
    sclk_hi();
    delayMicroseconds(D);
    sclk_lo();
    delayMicroseconds(D);
  }
}

static void cmd(uint8_t c) {
  digitalWrite(PIN_DC, LOW);
  digitalWrite(PIN_CS, LOW);
  bb_byte(c);
  digitalWrite(PIN_CS, HIGH);
}

static void dat(uint8_t d) {
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CS, LOW);
  bb_byte(d);
  digitalWrite(PIN_CS, HIGH);
}

static void dat16(uint16_t v) { dat(v >> 8); dat(v & 0xFF); }

static void fill(uint16_t c) {
  cmd(0x2A); dat16(COLSTART); dat16(COLSTART + TFT_W - 1);
  cmd(0x2B); dat16(ROWSTART); dat16(ROWSTART + TFT_H - 1);
  cmd(0x2C);
  uint8_t hi = c >> 8, lo = c & 0xFF;
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_CS, LOW);           // hold CS low for the whole pixel stream
  for (uint32_t i = 0; i < (uint32_t)TFT_W * TFT_H; i++) { bb_byte(hi); bb_byte(lo); }
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, LOW);
}

static void initPanel() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(10);
  digitalWrite(PIN_RST, LOW);  delay(20);
  digitalWrite(PIN_RST, HIGH); delay(150);

  cmd(0x01); delay(150);            // SWRESET
  cmd(0x11); delay(120);            // SLPOUT
  cmd(0x36); dat(0x00);             // MADCTL
  cmd(0x3A); dat(0x55); delay(10);  // COLMOD 16bpp
  cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);  // PORCTRL
  cmd(0xB7); dat(0x35);             // GCTRL
  cmd(0xBB); dat(0x28);             // VCOMS
  cmd(0xC0); dat(0x0C);             // LCMCTRL
  cmd(0xC2); dat(0x01);             // VDVVRHEN
  cmd(0xC3); dat(0x10);             // VRHS
  cmd(0xC4); dat(0x20);             // VDVSET
  cmd(0xC6); dat(0x0F);             // FRCTR2
  cmd(0xD0); dat(0xA4); dat(0xA1);  // PWCTRL1
  cmd(0x21);                        // INVON
  cmd(0x13);                        // NORON
  delay(10);
  cmd(0x29); delay(120);            // DISPON
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== BIT-BANGED ST7789 (no SPI peripheral) ===");

  pinMode(PIN_BL, OUTPUT);   digitalWrite(PIN_BL, HIGH);
  pinMode(PIN_MOSI, OUTPUT); digitalWrite(PIN_MOSI, LOW);
  pinMode(PIN_SCLK, OUTPUT); digitalWrite(PIN_SCLK, LOW);
  pinMode(PIN_CS, OUTPUT);   digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT);   digitalWrite(PIN_DC, HIGH);
  Serial.println("pins are plain GPIOs: mosi=19 sclk=18 cs=5 dc=16 rst=23 bl=4");
  Serial.println("bit rate ~250 kHz");

  initPanel();
  Serial.println("init sequence sent by bit-banging");
  Serial.println("--- long-dwell colour cycle ---");
}

void loop() {
  Serial.println(">> RED   (10s)"); fill(0xF800); delay(10000);
  Serial.println(">> GREEN (10s)"); fill(0x07E0); delay(10000);
  Serial.println(">> BLUE  (10s)"); fill(0x001F); delay(10000);
  Serial.println(">> WHITE (10s)"); fill(0xFFFF); delay(10000);
  Serial.println(">> BLACK (10s)"); fill(0x0000); delay(10000);
}
