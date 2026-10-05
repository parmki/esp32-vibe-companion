/*
 * raw_st7789_full -- library-free ST7789 driver using Bodmer's FULL init.
 *
 * The previous raw test used a minimal sequence (SWRESET/SLPOUT/COLMOD/MADCTL/
 * INVON/NORON/DISPON) and skipped the power and gamma blocks. On an ST7789
 * those are not optional: VCOMS and PWCTRL1 set the panel drive voltages, and
 * without them the panel legitimately shows nothing. So that test could not
 * distinguish "hardware dead" from "my init was incomplete".
 *
 * This one copies TFT_eSPI's own TFT_Drivers/ST7789_Init.h sequence verbatim
 * (the non-INIT_SEQUENCE_3 branch, i.e. the JLX240 datasheet variant), plus the
 * rotation-0 CGRAM offsets for a 135-wide panel (colstart 52, rowstart 40).
 *
 * Colours are held for 8 s each so there is no chance of looking during the
 * wrong phase. Runs at a conservative 10 MHz to rule out signal integrity.
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
#define COLSTART 52
#define ROWSTART 40

// ST7789 command bytes
#define C_SWRESET 0x01
#define C_SLPOUT  0x11
#define C_NORON   0x13
#define C_INVON   0x21
#define C_DISPON  0x29
#define C_CASET   0x2A
#define C_RASET   0x2B
#define C_RAMWR   0x2C
#define C_MADCTL  0x36
#define C_COLMOD  0x3A
#define C_RAMCTRL 0xB0
#define C_PORCTRL 0xB2
#define C_GCTRL   0xB7
#define C_VCOMS   0xBB
#define C_LCMCTRL 0xC0
#define C_VDVVRHEN 0xC2
#define C_VRHS    0xC3
#define C_VDVSET  0xC4
#define C_FRCTR2  0xC6
#define C_PWCTRL1 0xD0
#define C_PVGAMCTRL 0xE0
#define C_NVGAMCTRL 0xE1

static const uint16_t RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F, WHITE = 0xFFFF, BLACK = 0x0000;

static void cmd(uint8_t c) {
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, LOW);
  SPI.transfer(c);
  digitalWrite(PIN_CS, HIGH);
}
static void dat(uint8_t d) {
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, HIGH);
  SPI.transfer(d);
  digitalWrite(PIN_CS, HIGH);
}
static void dat16(uint16_t v) { dat(v >> 8); dat(v & 0xFF); }

static void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  cmd(C_CASET); dat16(x0); dat16(x1);
  cmd(C_RASET); dat16(y0); dat16(y1);
  cmd(C_RAMWR);
}

static void fill(uint16_t c) {
  setWindow(COLSTART, ROWSTART, COLSTART + TFT_W - 1, ROWSTART + TFT_H - 1);
  uint8_t hi = c >> 8, lo = c & 0xFF;
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, HIGH);
  for (uint32_t i = 0; i < (uint32_t)TFT_W * TFT_H; i++) { SPI.transfer(hi); SPI.transfer(lo); }
  digitalWrite(PIN_CS, HIGH);
}

static void fullInit() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);  delay(50);
  digitalWrite(PIN_RST, HIGH); delay(150);

  cmd(C_SLPOUT); delay(120);            // sleep out

  cmd(C_NORON);                         // normal display mode on

  cmd(C_MADCTL); dat(0x00);             // TFT_MAD_COLOR_ORDER (RGB)

  cmd(0xB6); dat(0x0A); dat(0x82);      // display function control

  cmd(C_RAMCTRL); dat(0x00); dat(0xE0);

  cmd(C_COLMOD); dat(0x55); delay(10);  // 16 bit/pixel

  cmd(C_PORCTRL); dat(0x0c); dat(0x0c); dat(0x00); dat(0x33); dat(0x33);
  cmd(C_GCTRL);   dat(0x35);            // VGH / VGL
  cmd(C_VCOMS);   dat(0x28);            // <-- panel drive voltage
  cmd(C_LCMCTRL); dat(0x0C);
  cmd(C_VDVVRHEN); dat(0x01); dat(0xFF);
  cmd(C_VRHS);    dat(0x10);
  cmd(C_VDVSET);  dat(0x20);
  cmd(C_FRCTR2);  dat(0x0f);
  cmd(C_PWCTRL1); dat(0xa4); dat(0xa1); // <-- power control

  cmd(C_PVGAMCTRL);
  const uint8_t pgamma[] = {0xd0,0x00,0x02,0x07,0x0a,0x28,0x32,0x44,0x42,0x06,0x0e,0x12,0x14,0x17};
  for (uint8_t v : pgamma) dat(v);

  cmd(C_NVGAMCTRL);
  const uint8_t ngamma[] = {0xd0,0x00,0x02,0x07,0x0a,0x28,0x31,0x54,0x47,0x0e,0x1c,0x17,0x1b,0x1e};
  for (uint8_t v : ngamma) dat(v);

  cmd(C_INVON);

  cmd(C_CASET); dat(0x00); dat(0x00); dat(0x00); dat(0xEF);
  cmd(C_RASET); dat(0x00); dat(0x00); dat(0x01); dat(0x3F);

  delay(120);
  cmd(C_DISPON); delay(120);

  digitalWrite(PIN_BL, HIGH);           // backlight on, as TFT_eSPI does
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== RAW ST7789, FULL Bodmer INIT, no libraries ===");

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT); digitalWrite(PIN_DC, HIGH);

  SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  Serial.println("SPI 10 MHz, sck=18 mosi=19 cs=5 dc=16 rst=23 bl=4");

  // Start with the backlight OFF so the very first thing the user sees is the
  // backlight coming on -- proving the GPIO4 path independently of any pixels.
  digitalWrite(PIN_BL, LOW);
  Serial.println("backlight OFF for 4s -- panel should be PURE BLACK");
  delay(4000);

  digitalWrite(PIN_BL, HIGH);
  Serial.println("backlight ON (panel uninitialised) -- look for DARK GREY glow");
  delay(4000);

  fullInit();
  Serial.println("full init sent; backlight on");
  Serial.println("--- long-dwell colour cycle ---");
}

void loop() {
  Serial.println(">> RED    (8s)"); fill(RED);   delay(8000);
  Serial.println(">> GREEN  (8s)"); fill(GREEN); delay(8000);
  Serial.println(">> BLUE   (8s)"); fill(BLUE);  delay(8000);
  Serial.println(">> WHITE  (8s)"); fill(WHITE); delay(8000);
  Serial.println(">> BLACK  (8s)"); fill(BLACK); delay(8000);

  Serial.println(">> backlight OFF (4s) -- panel should go PURE BLACK");
  digitalWrite(PIN_BL, LOW);  delay(4000);
  Serial.println(">> backlight ON  (4s) -- panel should glow DARK GREY");
  digitalWrite(PIN_BL, HIGH); delay(4000);
}
