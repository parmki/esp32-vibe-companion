/*
 * init_sweep -- find the init sequence THIS panel accepts.
 *
 * Established facts:
 *   - The factory firmware displayed colours -> hardware and wiring are good.
 *   - The backlight (GPIO4) works: the panel shows a dark grey glow.
 *   - Bodmer's TFT_eSPI sequence AND the JLX240 variant both produce no pixels.
 *   - There is a report of a T-Display where TFT_eSPI blanked but
 *     Adafruit_ST7789 worked on the same board.
 *
 * So the panel wants a different init table than the one we are sending. Rather
 * than iterate one flash per guess, this tries six candidate sequences back to
 * back, each ending in a DIFFERENT solid colour:
 *
 *   variant 1 -> RED     Bodmer JLX240 (TFT_eSPI branch 1, INVON)
 *   variant 2 -> GREEN   Adafruit_ST7789 style (LCMCTRL 0x2C, VCOMS 0x19, INVON)
 *   variant 3 -> BLUE    Adafruit style, VCOMS 0x28
 *   variant 4 -> WHITE   Adafruit style, no INVON (panel not inverted)
 *   variant 5 -> YELLOW  Bodmer JLX240 power/gamma but LCMCTRL 0x2C
 *   variant 6 -> CYAN    Minimal: SWRESET/SLPOUT/COLMOD/MADCTL/DISPON, no inversion
 *
 * The colour the user sees names the winning sequence. 12 s per variant.
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

#define C_SWRESET 0x01
#define C_SLPOUT  0x11
#define C_NORON   0x13
#define C_INVOFF  0x20
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

static const uint16_t RED=0xF800, GREEN=0x07E0, BLUE=0x001F,
                      WHITE=0xFFFF, YELLOW=0xFFE0, CYAN=0x07FF;

static void cmd(uint8_t c) {
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, LOW);
  SPI.transfer(c); digitalWrite(PIN_CS, HIGH);
}
static void dat(uint8_t d) {
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, HIGH);
  SPI.transfer(d); digitalWrite(PIN_CS, HIGH);
}
static void dat16(uint16_t v) { dat(v >> 8); dat(v & 0xFF); }

static void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t xoff, uint8_t yoff) {
  cmd(C_CASET); dat16(x0 + xoff); dat16(x1 + xoff);
  cmd(C_RASET); dat16(y0 + yoff); dat16(y1 + yoff);
  cmd(C_RAMWR);
}

static void fill(uint16_t c, uint8_t xoff, uint8_t yoff) {
  setWindow(0, 0, TFT_W - 1, TFT_H - 1, xoff, yoff);
  uint8_t hi = c >> 8, lo = c & 0xFF;
  digitalWrite(PIN_CS, LOW); digitalWrite(PIN_DC, HIGH);
  for (uint32_t i = 0; i < (uint32_t)TFT_W * TFT_H; i++) { SPI.transfer(hi); SPI.transfer(lo); }
  digitalWrite(PIN_CS, HIGH);
}

static void hardReset() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH); delay(10);
  digitalWrite(PIN_RST, LOW);  delay(20);
  digitalWrite(PIN_RST, HIGH); delay(150);
}

static void commonTail(uint8_t lcmctrl, uint8_t vrHs, uint8_t vcoms, bool invon) {
  cmd(C_PORCTRL); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
  cmd(C_GCTRL);   dat(0x35);
  cmd(C_VCOMS);   dat(vcoms);
  cmd(C_LCMCTRL); dat(lcmctrl);
  cmd(C_VDVVRHEN); dat(0x01);
  cmd(C_VRHS);    dat(vrHs);
  cmd(C_VDVSET);  dat(0x20);
  cmd(C_FRCTR2);  dat(0x0F);
  cmd(C_PWCTRL1); dat(0xA4); dat(0xA1);
  if (invon) cmd(C_INVON); else cmd(C_INVOFF);
  cmd(C_NORON);
  cmd(C_DISPON); delay(120);
}

// ------------------------------------------------------------------ variants
static void v1_bodmer_jlx240() {           // -> RED
  hardReset();
  cmd(C_SLPOUT); delay(120);
  cmd(C_NORON);
  cmd(C_MADCTL); dat(0x00);
  cmd(0xB6); dat(0x0A); dat(0x82);
  cmd(C_RAMCTRL); dat(0x00); dat(0xE0);
  cmd(C_COLMOD); dat(0x55); delay(10);
  cmd(C_PORCTRL); dat(0x0c); dat(0x0c); dat(0x00); dat(0x33); dat(0x33);
  cmd(C_GCTRL);   dat(0x35);
  cmd(C_VCOMS);   dat(0x28);
  cmd(C_LCMCTRL); dat(0x0C);
  cmd(C_VDVVRHEN); dat(0x01); dat(0xFF);
  cmd(C_VRHS);    dat(0x10);
  cmd(C_VDVSET);  dat(0x20);
  cmd(C_FRCTR2);  dat(0x0f);
  cmd(C_PWCTRL1); dat(0xa4); dat(0xa1);
  const uint8_t pg[]={0xd0,0x00,0x02,0x07,0x0a,0x28,0x32,0x44,0x42,0x06,0x0e,0x12,0x14,0x17};
  cmd(C_PVGAMCTRL); for (uint8_t v : pg) dat(v);
  const uint8_t ng[]={0xd0,0x00,0x02,0x07,0x0a,0x28,0x31,0x54,0x47,0x0e,0x1c,0x17,0x1b,0x1e};
  cmd(C_NVGAMCTRL); for (uint8_t v : ng) dat(v);
  cmd(C_INVON);
  delay(120); cmd(C_DISPON); delay(120);
}

static void adafruit_style(uint8_t vcoms, uint8_t lcmctrl, bool invon) {
  hardReset();
  cmd(C_SWRESET); delay(150);
  cmd(C_SLPOUT);  delay(120);
  cmd(C_COLMOD); dat(0x55); delay(10);
  cmd(C_MADCTL); dat(0x00);
  cmd(C_PORCTRL); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
  cmd(C_GCTRL);   dat(0x35);
  cmd(C_VCOMS);   dat(vcoms);
  cmd(C_LCMCTRL); dat(lcmctrl);
  cmd(C_VDVVRHEN); dat(0x01);
  cmd(C_VRHS);    dat(0x12);
  cmd(C_VDVSET);  dat(0x20);
  cmd(C_FRCTR2);  dat(0x0F);
  cmd(C_PWCTRL1); dat(0xA4); dat(0xA1);
  const uint8_t pg[]={0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
  cmd(C_PVGAMCTRL); for (uint8_t v : pg) dat(v);
  const uint8_t ng[]={0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
  cmd(C_NVGAMCTRL); for (uint8_t v : ng) dat(v);
  if (invon) cmd(C_INVON); else cmd(C_INVOFF);
  cmd(C_NORON);
  delay(10); cmd(C_DISPON); delay(120);
}

static void v6_minimal() {                  // -> CYAN
  hardReset();
  cmd(C_SWRESET); delay(150);
  cmd(C_SLPOUT);  delay(120);
  cmd(C_COLMOD);  dat(0x55); delay(10);
  cmd(C_MADCTL);  dat(0x00);
  cmd(C_INVOFF);
  cmd(C_NORON);
  cmd(C_DISPON);  delay(120);
}

struct Variant { const char* name; const char* colour; uint16_t rgb; void (*init)(); uint8_t xoff, yoff; };

static void v2() { adafruit_style(0x19, 0x2C, true); }
static void v3() { adafruit_style(0x28, 0x2C, true); }
static void v4() { adafruit_style(0x19, 0x2C, false); }
static void v5() {
  hardReset();
  cmd(C_SLPOUT); delay(120);
  cmd(C_NORON);
  cmd(C_MADCTL); dat(0x00);
  cmd(C_COLMOD); dat(0x55); delay(10);
  commonTail(0x2C, 0x10, 0x28, true);
}

static const Variant VARIANTS[] = {
  { "1 Bodmer JLX240 (current)",        "RED",    RED,    v1_bodmer_jlx240, 52, 40 },
  { "2 Adafruit style, LCMCTRL 0x2C",   "GREEN",  GREEN,  v2,               52, 40 },
  { "3 Adafruit style, VCOMS 0x28",     "BLUE",   BLUE,   v3,               52, 40 },
  { "4 Adafruit style, no INVON",       "WHITE",  WHITE,  v4,               52, 40 },
  { "5 Bodmer power + LCMCTRL 0x2C",    "YELLOW", YELLOW, v5,               52, 40 },
  { "6 Minimal, no inversion",          "CYAN",   CYAN,   v6_minimal,       52, 40 },
};
static const int N_VARIANTS = sizeof(VARIANTS) / sizeof(VARIANTS[0]);

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== ST7789 INIT SWEEP ===");
  Serial.println("each variant ends in a solid colour for 12s:");
  for (int i = 0; i < N_VARIANTS; i++) {
    Serial.print("  variant ");
    Serial.print(VARIANTS[i].name);
    Serial.print("  ->  ");
    Serial.println(VARIANTS[i].colour);
  }

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT); digitalWrite(PIN_DC, HIGH);
  SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  Serial.println("SPI 10 MHz");
}

void loop() {
  for (int i = 0; i < N_VARIANTS; i++) {
    const Variant& v = VARIANTS[i];
    Serial.print(">> variant "); Serial.print(v.name);
    Serial.print("  (expect "); Serial.print(v.colour); Serial.println(")");
    v.init();
    delay(50);
    fill(v.rgb, v.xoff, v.yoff);
    delay(12000);
  }
  Serial.println(">> sweep complete, restarting");
}
