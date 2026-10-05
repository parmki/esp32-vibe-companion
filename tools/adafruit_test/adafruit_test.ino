/*
 * adafruit_test -- drive the panel with Adafruit_ST7789 instead of TFT_eSPI.
 *
 * Rationale: there is a report of a TTGO T-Display where TFT_eSPI produced a
 * blank screen while Adafruit_ST7789 worked on the same unit, and both TFT_eSPI
 * AND our own library-free SPI driver produced no pixels here. Adafruit is a
 * third, independent SPI implementation, so it is a clean tie-breaker between
 * "our SPI usage is wrong" and "the panel needs something else".
 *
 * Pin setup matters: we call SPI.begin(sck, -1, mosi, ss) with the board's real
 * pins BEFORE tft.init(). Adafruit_SPITFT::begin() then calls _spi->begin() with
 * no arguments, and the ESP32 core's SPIClass::begin() returns early when the
 * bus is already started -- so our pin mapping survives instead of being
 * replaced by the core's defaults.
 *
 * init(135, 240) computes the CGRAM offsets itself (colstart 53/52, rowstart 40).
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define PIN_MOSI 19
#define PIN_SCLK 18
#define TFT_CS    5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL    4

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== ADAFRUIT_ST7789 TEST ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("backlight HIGH");

  // Must precede init(): the core's SPIClass::begin() no-ops if the bus is up,
  // which is what keeps our custom pins instead of the ESP32 defaults.
  SPI.begin(PIN_SCLK, -1, PIN_MOSI, TFT_CS);
  Serial.println("SPI.begin(18, -1, 19, 5) ok");

  tft.init(135, 240);
  Serial.println("tft.init(135,240) ok");

  tft.setSPISpeed(10000000);
  tft.setRotation(0);

  Serial.print("reported size: ");
  Serial.print(tft.width());
  Serial.print(" x ");
  Serial.println(tft.height());

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("ADAFRUIT OK");
  delay(1500);
  Serial.println("--- long-dwell colour cycle ---");
}

void loop() {
  Serial.println(">> RED   (8s)"); tft.fillScreen(ST77XX_RED);   delay(8000);
  Serial.println(">> GREEN (8s)"); tft.fillScreen(ST77XX_GREEN); delay(8000);
  Serial.println(">> BLUE  (8s)"); tft.fillScreen(ST77XX_BLUE);  delay(8000);
  Serial.println(">> WHITE (8s)"); tft.fillScreen(ST77XX_WHITE); delay(8000);
  Serial.println(">> BLACK (8s)"); tft.fillScreen(ST77XX_BLACK); delay(8000);
  Serial.println(">> backlight OFF (4s)"); digitalWrite(TFT_BL, LOW);  delay(4000);
  Serial.println(">> backlight ON  (4s)"); digitalWrite(TFT_BL, HIGH); delay(4000);
}
