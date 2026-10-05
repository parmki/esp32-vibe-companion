/*
 * raw_spi_test -- does the panel work WITHOUT TFT_eSPI?
 *
 * TFT_eSPI 2.5.43 on ESP32 Arduino core 3.x (ESP-IDF 5) is the one component
 * left that could be silently failing: it reconfigures the SPI bus through the
 * GPIO matrix, and if that goes wrong the panel never sees a byte while the
 * sketch happily reports success.
 *
 * This sketch talks ST7789 directly over SPI with no library in the path:
 * explicit pins, explicit init sequence, explicit window + RAMWR fill. If the
 * panel shows solid colours here, the hardware is fine and TFT_eSPI is the
 * problem. If it is still blank, the fault is electrical.
 *
 * Runs at a deliberately conservative 10 MHz first.
 */

#include <SPI.h>

#define PIN_MOSI 19
#define PIN_SCLK 18
#define PIN_CS    5
#define PIN_DC   16
#define PIN_RST  23
#define PIN_BL    4

#define TFT_W 135
#define TFT_H 240

// ST7789 CGRAM window offsets for a 135x240 panel
#define X_OFF 52
#define Y_OFF 40

static uint32_t spiHz = 10000000;

static void log(const char* s) { Serial.println(s); }

static void wrCmd(uint8_t c) {
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  SPI.transfer(c);
  digitalWrite(PIN_CS, HIGH);
}

static void wrData(uint8_t d) {
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, HIGH);
  SPI.transfer(d);
  digitalWrite(PIN_CS, HIGH);
}

static void wrData16(uint16_t v) { wrData(v >> 8); wrData(v & 0xFF); }

static void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  wrCmd(0x2A); wrData16(x0); wrData16(x1);
  wrCmd(0x2B); wrData16(y0); wrData16(y1);
  wrCmd(0x2C);   // RAMWR
}

static void fill(uint16_t colour) {
  setWindow(X_OFF, Y_OFF, X_OFF + TFT_W - 1, Y_OFF + TFT_H - 1);
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, HIGH);
  const uint8_t hi = colour >> 8;
  const uint8_t lo = colour & 0xFF;
  for (uint32_t i = 0; i < (uint32_t)TFT_W * TFT_H; i++) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  digitalWrite(PIN_CS, HIGH);
}

static void initPanel() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(50);
  digitalWrite(PIN_RST, HIGH);
  delay(150);

  wrCmd(0x01); delay(150);         // SWRESET
  wrCmd(0x11); delay(150);         // SLPOUT
  wrCmd(0x3A); wrData(0x55);       // COLMOD: 16 bit/pixel
  wrCmd(0x36); wrData(0x00);       // MADCTL: normal orientation
  wrCmd(0x21);                     // INVON  (ST7789 panels are inverted)
  wrCmd(0x13); delay(10);          // NORON
  wrCmd(0x29); delay(120);         // DISPON
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  log("=== RAW ST7789 SPI TEST (no TFT_eSPI) ===");

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  log("backlight -> HIGH");

  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH);

  SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  Serial.print("SPI: sck="); Serial.print(PIN_SCLK);
  Serial.print(" mosi=");    Serial.print(PIN_MOSI);
  Serial.print(" cs=");      Serial.print(PIN_CS);
  Serial.print(" dc=");      Serial.print(PIN_DC);
  Serial.print(" rst=");     Serial.print(PIN_RST);
  Serial.print(" @ ");       Serial.print(spiHz / 1000000.0); Serial.println(" MHz");

  initPanel();
  log("ST7789 init sequence sent");
  log("--- starting colour cycle ---");
}

void loop() {
  log(">> fill RED");
  fill(0xF800); delay(2000);
  log(">> fill GREEN");
  fill(0x07E0); delay(2000);
  log(">> fill BLUE");
  fill(0x001F); delay(2000);
  log(">> fill WHITE");
  fill(0xFFFF); delay(2000);
  log(">> fill BLACK");
  fill(0x0000); delay(1000);
  log(">> backlight OFF");
  digitalWrite(PIN_BL, LOW);  delay(1500);
  log(">> backlight ON");
  digitalWrite(PIN_BL, HIGH); delay(1500);
}
