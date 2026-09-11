/* ===========================================================================
   Pulsar — Waveshare ESP32-S3-LCD-1.54  (240 x 240, ST7789, CST816)
   ---------------------------------------------------------------------------
   Offline build. No Wi-Fi, no HTTP. The payload below is a JSON literal that
   matches docs/api.md exactly, so wiring a real GET /v1/gateway_health in
   later means replacing one string — nothing else changes.

   Screen layout mirrors lcd154/mockups/device-ui.html, six fixed bands:
       status 0..20 | health 20..60 | hero 60..116
       graph 116..180 | stats 180..210 | footer 210..240

   Libraries (Arduino Library Manager):
       GFX Library for Arduino   by Moon On Our Nation   (Arduino_GFX)
       SensorLib                 by lewisxhe             (CST816 touch)
       ArduinoJson               by Benoit Blanchon      (v7)

   Pin map below is taken from Waveshare's own demos for this board:
       Arduino-3.2.0/examples/04_gfx_helloworld, 08_lvgl_arduino_v8
       ESP-IDF-5.5.1/01_factory/components/esp_bsp/bsp_power_manager.c
   =========================================================================== */

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <ctype.h>
#include "TouchDrvCSTXXX.hpp"

#if ARDUINOJSON_VERSION_MAJOR < 7
#error "Install ArduinoJson 7.x - v6 uses a different document API"
#endif

/* ---------------------------------------------------------------- pin map */
#define PIN_LCD_DC    45
#define PIN_LCD_CS    21
#define PIN_LCD_SCK   38
#define PIN_LCD_MOSI  39
#define PIN_LCD_RST   40
#define PIN_LCD_BL    46

#define PIN_I2C_SDA   42
#define PIN_I2C_SCL   41
#define PIN_TP_RST    47
#define PIN_TP_INT    48

#define PIN_BAT_ADC    1     /* ADC1_CH0, through a 1/3 divider   */
#define PIN_BAT_EN     2     /* drive HIGH to power the divider   */
#define PIN_CHARGING   3     /* input pull-up, LOW = charging     */

#define PIN_BTN_BOOT   0
#define PIN_BTN_A      5     /* PLUS is one of these two — both are */
#define PIN_BTN_B      4     /* treated as "next panel" below.      */

/* Clock offset applied to measured_at. No RTC and no NTP in this build,
   so the time on screen is the time the payload says it was measured.     */
#define TZ_OFFSET_HOURS  0

/* ------------------------------------------------------------- the payload
   One entry per configured gateway — exactly what three GET responses would
   look like. Field names, ordering and invariants follow docs/api.md:
     sum(buckets) == aggregates.2xx_count,  len(buckets) == bucket_count     */
static const char PAYLOAD[] PROGMEM = R"JSON(
[
 {
  "gateway": "Payouts",
  "measured_at": 1770000100,
  "health": { "level": "critical", "message": "5xx 2.14% max 0.50%" },
  "metrics": [
   { "name": "Overview", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 17862, "4xx_count": 169, "5xx_count": 389,
                     "avg_latency_ms": 48, "p95_latency_ms": 210 },
     "buckets": [656,662,652,658,664,654,660,650,656,660,656,662,652,658,664,
                 654,659,649,655,659,654,619,569,534,500,448,414,363,329,292] },
   { "name": "Init Payout", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 6421, "4xx_count": 61, "5xx_count": 44,
                     "avg_latency_ms": 39, "p95_latency_ms": 165 },
     "buckets": [236,233,234,231,238,230,237,228,236,232,236,232,234,230,238,
                 229,237,227,235,232,237,222,211,195,189,166,160,136,129,111] },
   { "name": "Disburse Payout", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 5288, "4xx_count": 74, "5xx_count": 338,
                     "avg_latency_ms": 74, "p95_latency_ms": 390 },
     "buckets": [200,202,199,201,203,200,201,198,200,201,200,202,199,201,203,
                 200,201,198,200,200,194,179,161,147,134,117,106,91,80,70] },
   { "name": "Acct Validations", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 6153, "4xx_count": 34, "5xx_count": 7,
                     "avg_latency_ms": 21, "p95_latency_ms": 62 },
     "buckets": [220,227,219,226,223,224,222,224,220,227,220,228,219,227,223,
                 225,221,224,220,227,223,218,197,192,177,165,148,136,120,111] }
  ]
 },
 {
  "gateway": "Fraud VAS",
  "measured_at": 1770000100,
  "health": { "level": "info", "message": "error budget healthy" },
  "metrics": [
   { "name": "Overview", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 11632, "4xx_count": 122, "5xx_count": 31,
                     "avg_latency_ms": 24, "p95_latency_ms": 88 },
     "buckets": [403,422,407,418,422,412,414,409,403,420,420,403,421,414,411,
                 399,402,402,399,419,405,417,421,415,402,415,418,290,171,58] },
   { "name": "Eval", "bucket_unit": "minute", "bucket_size": 1, "bucket_count": 30,
     "aggregates": { "2xx_count": 11632, "4xx_count": 122, "5xx_count": 31,
                     "avg_latency_ms": 24, "p95_latency_ms": 88 },
     "buckets": [403,422,407,418,422,412,414,409,403,420,420,403,421,414,411,
                 399,402,402,399,419,405,417,421,415,402,415,418,290,171,58] }
  ]
 }
]
)JSON";

/* ----------------------------------------------------------------- model */
#define MAX_GW      3
#define MAX_ROWS    5
#define MAX_BUCKETS 30

struct Row {
  char     name[18];
  char     unit[8];
  int      size, count;
  long     c2xx, c4xx, c5xx;
  int      avg_ms, p95_ms;
  int      buckets[MAX_BUCKETS];
  uint8_t  nbuckets;

  long  total()    const { return c2xx + c4xx + c5xx; }
  float pct4()     const { return total() ? c4xx * 100.0f / total() : 0; }
  float pct5()     const { return total() ? c5xx * 100.0f / total() : 0; }
  int   spanUnits()const { return size * count; }
  float rate()     const { return spanUnits() ? (float)c2xx / spanUnits() : 0; }
};

struct Gateway {
  char    name[16];
  long    measured_at;
  char    level[10];
  char    message[22];
  Row     rows[MAX_ROWS];
  uint8_t nrows;
};

static Gateway gw[MAX_GW];
static uint8_t gwCount = 0;

/* ---------------------------------------------------------------- palette
   Authored in hex, folded to RGB565 the same way the panel does it.       */
static constexpr uint16_t rgb(uint32_t h){
  return (uint16_t)((((h >> 19) & 0x1F) << 11) | (((h >> 10) & 0x3F) << 5) | ((h >> 3) & 0x1F));
}
static constexpr uint16_t C_BG    = rgb(0x05070c);
static constexpr uint16_t C_PANE  = rgb(0x0c1422);
static constexpr uint16_t C_LINE  = rgb(0x1c2740);
static constexpr uint16_t C_LINE2 = rgb(0x28344a);
static constexpr uint16_t C_TX    = rgb(0xe6edf7);
static constexpr uint16_t C_DIM   = rgb(0x7f8fa8);
static constexpr uint16_t C_DIM2  = rgb(0x4d5c76);
static constexpr uint16_t C_CY    = rgb(0x22d3ee);
static constexpr uint16_t C_GR    = rgb(0x22c55e);
static constexpr uint16_t C_AM    = rgb(0xf5a524);
static constexpr uint16_t C_RD    = rgb(0xff4d5e);
static constexpr uint16_t C_GY    = rgb(0x8aa0c0);

struct Level { const char* word; uint16_t c, dark, idle; uint16_t blink_ms; };
static const Level LV_INFO  = { "INFO",     C_GR, rgb(0x0b2c19), rgb(0x071a10),   0 };
static const Level LV_WARN  = { "WARNING",  C_AM, rgb(0x3a2a06), rgb(0x221903), 700 };
static const Level LV_CRIT  = { "CRITICAL", C_RD, rgb(0x42131c), rgb(0x280b11), 220 };
static const Level LV_FAULT = { "OFFLINE",  C_GY, rgb(0x18202d), rgb(0x0e141d), 700 };

/* ------------------------------------------------------------------ bands */
#define BAND_STATUS_Y   0
#define BAND_STATUS_H  20
#define BAND_HEALTH_Y  20
#define BAND_HEALTH_H  40
#define BAND_HERO_Y    60
#define BAND_HERO_H    56
#define BAND_GRAPH_Y  116
#define BAND_GRAPH_H   64
#define BAND_STATS_Y  180
#define BAND_STATS_H   30
#define BAND_FOOT_Y   210
#define BAND_FOOT_H    30

/* ---------------------------------------------------------------- display */
Arduino_DataBus* bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS,
                                            PIN_LCD_SCK, PIN_LCD_MOSI, -1 /* MISO */);
Arduino_GFX* panel = new Arduino_ST7789(bus, PIN_LCD_RST, 0 /*rotation*/, true /*IPS*/,
                                        240, 240);
/* One off-screen frame so a redraw lands in a single push. 240*240*2 = 113 KB,
   which lives in PSRAM. Never draw straight to the panel inside loop().     */
Arduino_Canvas* cv = new Arduino_Canvas(240, 240, panel);

TouchDrvCSTXXX touch;
static bool touchOK = false;

/* ------------------------------------------------------------------ state */
static uint8_t  curGw = 0, curRow = 0;
static bool     setupScreen = false;
static uint32_t heroT0 = 0;
static float    heroFrom = 0;
static uint32_t sweepT0 = 0;

/* ------------------------------------------------------------- small utils */
static void compactNum(long n, char* out, size_t cap){
  if (n < 0) n = 0;
  if      (n >= 10000000L) snprintf(out, cap, "%ldM",  n / 1000000L);
  else if (n >= 1000000L)  snprintf(out, cap, "%.1fM", n / 1e6);
  else if (n >= 100000L)   snprintf(out, cap, "%ldk",  n / 1000L);
  else if (n >= 1000L)     snprintf(out, cap, "%.1fk", n / 1e3);
  else                     snprintf(out, cap, "%ld",   n);
}
static float easeOut(float t){ t = 1 - t; return 1 - t*t*t; }
static float clampf(float v, float a, float b){ return v < a ? a : v > b ? b : v; }

/* text: x,y is the TOP-LEFT of the 6x8 cell, matching the mockup's coords */
static int txt(const char* s, int x, int y, uint8_t size, uint16_t c,
               char align = 'l', bool bold = false){
  int w = (int)strlen(s) * 6 * size;
  if (align == 'c') x -= w / 2;
  if (align == 'r') x -= w;
  cv->setTextSize(size);
  cv->setTextColor(c);
  cv->setCursor(x, y);
  cv->print(s);
  if (bold){ cv->setCursor(x + 1, y); cv->print(s); }
  return w;
}

static const Level& levelOf(const Gateway& g){
  if (!strcmp(g.level, "critical")) return LV_CRIT;
  if (!strcmp(g.level, "warning"))  return LV_WARN;
  if (!strcmp(g.level, "info"))     return LV_INFO;
  return LV_FAULT;
}
static void unitLabel(const Row& r, char* out, size_t cap){
  const char* u = !strcmp(r.unit, "second") ? "SEC" : !strcmp(r.unit, "hour") ? "HR" : "MIN";
  snprintf(out, cap, "2XX / %s", u);
}
static void spanLabel(const Row& r, char* out, size_t cap){
  int n = r.spanUnits();
  if (!strcmp(r.unit, "second") && n >= 60) snprintf(out, cap, "LAST %d MIN", n / 60);
  else snprintf(out, cap, "LAST %d %s", n,
                !strcmp(r.unit, "second") ? "SEC" : !strcmp(r.unit, "hour") ? "HR" : "MIN");
}

/* ---------------------------------------------------------------- battery */
static void batteryBegin(){
  pinMode(PIN_BAT_EN, OUTPUT);
  digitalWrite(PIN_BAT_EN, HIGH);          // powers the divider
  pinMode(PIN_CHARGING, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}
static float batteryVolts(){
  /* analogReadMilliVolts applies the chip's factory ADC calibration, then the
     board's 1/3 divider is undone — same maths as Waveshare's BSP.          */
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  return (mv / 8.0f) / 1000.0f * 3.0f;
}
static int batteryPercent(){
  float v = batteryVolts();
  if (v < 3.52f) return 1;
  if (v < 3.64f) return 20;
  if (v < 3.76f) return 40;
  if (v < 3.88f) return 60;
  if (v < 4.00f) return 80;
  return 100;
}
static bool charging(){ return digitalRead(PIN_CHARGING) == LOW; }

/* ------------------------------------------------------------------ parse */
static bool loadPayload(){
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, PAYLOAD);
  if (err){ Serial.printf("payload parse failed: %s\n", err.c_str()); return false; }

  JsonArrayConst arr = doc.as<JsonArrayConst>();
  gwCount = 0;
  for (JsonObjectConst o : arr){
    if (gwCount >= MAX_GW) break;
    Gateway& g = gw[gwCount];
    strlcpy(g.name, o["gateway"] | "-", sizeof g.name);
    g.measured_at = o["measured_at"] | 0;
    strlcpy(g.level,   o["health"]["level"]   | "info", sizeof g.level);
    strlcpy(g.message, o["health"]["message"] | "",     sizeof g.message);

    g.nrows = 0;
    for (JsonObjectConst m : o["metrics"].as<JsonArrayConst>()){
      if (g.nrows >= MAX_ROWS) break;                 // api.md §5: cap is 5
      Row& r = g.rows[g.nrows];
      strlcpy(r.name, m["name"] | "-", sizeof r.name);
      strlcpy(r.unit, m["bucket_unit"] | "minute", sizeof r.unit);
      r.size   = m["bucket_size"]  | 1;
      r.count  = m["bucket_count"] | 30;
      JsonObjectConst a = m["aggregates"];
      r.c2xx   = a["2xx_count"] | 0;
      r.c4xx   = a["4xx_count"] | 0;
      r.c5xx   = a["5xx_count"] | 0;
      r.avg_ms = a["avg_latency_ms"] | 0;
      r.p95_ms = a["p95_latency_ms"] | 0;

      r.nbuckets = 0;
      for (JsonVariantConst v : m["buckets"].as<JsonArrayConst>()){
        if (r.nbuckets >= MAX_BUCKETS) break;
        int b = v.as<int>();
        r.buckets[r.nbuckets++] = b < 0 ? 0 : b;      // a gap is a zero
      }
      /* short array → left-pad with zeros, the missing history is the old end */
      if (r.nbuckets && r.nbuckets < r.count && r.count <= MAX_BUCKETS){
        int pad = r.count - r.nbuckets;
        for (int i = r.nbuckets - 1; i >= 0; i--) r.buckets[i + pad] = r.buckets[i];
        for (int i = 0; i < pad; i++) r.buckets[i] = 0;
        r.nbuckets = r.count;
      }
      long s = 0; for (int i = 0; i < r.nbuckets; i++) s += r.buckets[i];
      if (r.nbuckets && s != r.c2xx)
        Serial.printf("  ! %s/%s: sum(buckets)=%ld but 2xx_count=%ld\n",
                      g.name, r.name, s, r.c2xx);
      g.nrows++;
    }
    Serial.printf("gateway %-14s %u rows  level=%s\n", g.name, g.nrows, g.level);
    gwCount++;
  }
  return gwCount > 0;
}

/* =========================================================================
   Bands
   ========================================================================= */
static void drawStatus(){
  const int y = BAND_STATUS_Y, h = BAND_STATUS_H;
  cv->fillRect(0, y, 240, h, C_BG);

  /* battery shell, filled by charge; cyan while charging */
  const int pc = batteryPercent();
  const int bx = 9, by = y + 7;
  cv->fillRect(bx, by, 13, 7, C_LINE2);
  cv->fillRect(bx + 1, by + 1, 11, 5, C_BG);
  cv->fillRect(bx + 1, by + 1, (11 * pc) / 100, 5, charging() ? C_CY : C_DIM);
  cv->fillRect(bx + 13, by + 2, 1, 3, C_LINE2);
  char pcs[8]; snprintf(pcs, sizeof pcs, "%d%%", pc);
  txt(pcs, bx + 18, y + 6, 1, C_DIM2);

  /* clock — measured_at, because this build has no RTC and no NTP */
  time_t t = (time_t)(gw[curGw].measured_at + TZ_OFFSET_HOURS * 3600L);
  struct tm tmv; gmtime_r(&t, &tmv);
  char clk[8]; snprintf(clk, sizeof clk, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  txt(clk, 120, y + 6, 1, C_DIM, 'c');

  /* no radio in this build — three flat bars, dimmed */
  for (int i = 0; i < 3; i++) cv->fillRect(232 - 8 + i * 4, y + 12 - i * 3, 2, 3 + i * 3, C_DIM2);
  cv->fillRect(0, y + h - 1, 240, 1, C_LINE);
}

static void drawHealth(){
  const int y = BAND_HEALTH_Y, h = BAND_HEALTH_H;
  const Gateway& g = gw[curGw];
  const Level& L = levelOf(g);

  bool on = L.blink_ms ? ((millis() / L.blink_ms) % 2 == 0) : true;
  cv->fillRect(0, y, 240, h, (L.blink_ms && on) ? L.dark : L.idle);
  cv->fillRect(0, y, 3, h, L.c);                       /* the level edge */

  txt(L.word, 12, y + 8, 2, L.c, 'l', true);
  char msg[22]; strlcpy(msg, g.message, sizeof msg);   /* api.md §5: <= 20 */
  txt(msg, 12, y + 27, 1, C_TX);
}

static void drawHero(){
  const int y = BAND_HERO_Y;
  const Row& r = gw[curGw].rows[curRow];
  cv->fillRect(0, y, 240, BAND_HERO_H, C_BG);

  float t = clampf((millis() - heroT0) / 520.0f, 0, 1);
  float v = heroFrom + (r.rate() - heroFrom) * easeOut(t);
  char big[12]; compactNum((long)(v + 0.5f), big, sizeof big);
  txt(big, 10, y + 6, 4, C_TX, 'l', true);

  char u[16]; unitLabel(r, u, sizeof u);
  txt(u, 11, y + 44, 1, C_DIM2);
}

static void drawGraph(){
  const int y = BAND_GRAPH_Y, h = BAND_GRAPH_H;
  const Row& r = gw[curGw].rows[curRow];
  const Level& L = levelOf(gw[curGw]);
  cv->fillRect(0, y, 240, h, C_BG);
  if (r.nbuckets < 2) return;

  const int X0 = 6, PW = 228, PH = 42, YB = y + 6 + PH;
  const uint16_t body = (&L == &LV_INFO) ? C_LINE : L.dark;
  const uint16_t edge = (&L == &LV_INFO) ? C_CY   : L.c;

  long hi = 1;
  for (int i = 0; i < r.nbuckets; i++) if (r.buckets[i] > hi) hi = r.buckets[i];
  hi = hi * 114 / 100;                                   /* 14 % headroom */

  float sweep = easeOut(clampf((millis() - sweepT0) / 300.0f, 0, 1));
  int cols = (int)(PW * sweep);

  for (int px = 0; px < cols; px++){
    int pos = px * (r.nbuckets - 1) * 256 / (PW - 1);
    int i = pos >> 8, fr = pos & 255;
    int a = r.buckets[i], b = r.buckets[min(i + 1, r.nbuckets - 1)];
    int val = a + ((b - a) * fr >> 8);
    int ph  = (int)((long)val * (PH - 1) / hi);
    cv->fillRect(X0 + px, YB - ph, 1, ph + 1, body);
    cv->drawPixel(X0 + px, YB - ph, edge);
  }
  cv->fillRect(X0, YB + 1, PW, 1, C_LINE);
  char sp[16]; spanLabel(r, sp, sizeof sp);
  txt(sp, 232, y + h - 9, 1, C_DIM2, 'r');
}

static void drawStats(){
  const int y = BAND_STATS_Y;
  const Row& r = gw[curGw].rows[curRow];
  cv->fillRect(0, y, 240, BAND_STATS_H, C_BG);

  const float p4 = r.pct4(), p5 = r.pct5();
  const uint16_t c4 = p4 >= 2.0f ? C_AM : C_TX;
  const uint16_t c5 = p5 >= 1.0f ? C_RD : p5 >= 0.5f ? C_AM : C_TX;

  const int col[3] = { 10, 88, 164 };
  const char* lab[3] = { "P95", "4XX", "5XX" };
  char v0[12], v1[12], v2[12], s1[10], s2[10];
  snprintf(v0, sizeof v0, "%dms", r.p95_ms);
  compactNum(r.c4xx, v1, sizeof v1);
  compactNum(r.c5xx, v2, sizeof v2);
  snprintf(s1, sizeof s1, "%.2f%%", p4);
  snprintf(s2, sizeof s2, "%.2f%%", p5);
  const char* val[3] = { v0, v1, v2 };
  const char* sub[3] = { "", s1, s2 };
  const uint16_t cc[3] = { C_TX, c4, c5 };

  for (int i = 0; i < 3; i++){
    txt(lab[i], col[i], y + 3, 1, C_DIM2);
    int w = txt(val[i], col[i], y + 15, 1, cc[i], 'l', true);
    if (sub[i][0]) txt(sub[i], col[i] + w + 5, y + 15, 1, C_DIM2);
  }
}

static void drawFoot(){
  const int y = BAND_FOOT_Y;
  const Gateway& g = gw[curGw];
  cv->fillRect(0, y, 240, BAND_FOOT_H, C_PANE);
  cv->fillRect(0, y, 240, 1, C_LINE);

  char up[16]; strlcpy(up, g.name, sizeof up);
  for (char* p = up; *p; p++) *p = toupper(*p);
  txt(up, 10, y + 4, 1, C_DIM2);

  txt(g.rows[curRow].name, 10, y + 14, 2, C_TX, 'l', true);

  /* one dot per configured gateway, coloured by that gateway's level */
  const int dx = 240 - 10 - (gwCount * 7 - 3);
  for (int i = 0; i < gwCount; i++){
    const Level& L = levelOf(gw[i]);
    cv->fillRect(dx + i * 7, y + 5, 4, 4, i == curGw ? L.c : L.dark);
  }
  char pos[10]; snprintf(pos, sizeof pos, "%d/%d", curRow + 1, g.nrows);
  txt(pos, 230, y + 18, 1, C_DIM2, 'r');
}

static void drawSetup(){
  cv->fillScreen(C_BG);
  txt("SETUP", 10, 10, 2, C_CY, 'l', true);
  cv->fillRect(10, 32, 220, 1, C_LINE);
  int y = 44;
  for (int i = 0; i < gwCount; i++){
    cv->fillRect(10, y, 4, 4, levelOf(gw[i]).c);
    txt(gw[i].name, 20, y - 1, 1, C_TX, 'l', true);
    char n[16]; snprintf(n, sizeof n, "%d panels", gw[i].nrows);
    txt(n, 230, y - 1, 1, C_DIM2, 'r');
    y += 16;
  }
  cv->fillRect(10, 176, 220, 1, C_LINE);
  char v[24]; snprintf(v, sizeof v, "BATTERY %.2f V", batteryVolts());
  txt(v, 120, 186, 1, C_DIM, 'c');
  txt(charging() ? "CHARGING" : "ON BATTERY", 120, 200, 1, C_DIM2, 'c');
  txt("OFFLINE BUILD - NO RADIO", 120, 218, 1, C_DIM2, 'c');
}

static void render(){
  if (setupScreen){ drawSetup(); }
  else {
    cv->fillScreen(C_BG);
    drawStatus(); drawHealth(); drawHero(); drawGraph(); drawStats(); drawFoot();
  }
  cv->flush();
}

/* =========================================================================
   Input
   ========================================================================= */
static void beginHero(){ heroFrom = gw[curGw].rows[curRow].rate(); heroT0 = millis(); }

static void nextPanel(){
  beginHero();
  if (curRow + 1 < gw[curGw].nrows) curRow++;
  else { curRow = 0; curGw = (curGw + 1) % gwCount; }
  sweepT0 = millis();
}
static void prevPanel(){
  beginHero();
  if (curRow > 0) curRow--;
  else { curGw = (curGw + gwCount - 1) % gwCount; curRow = gw[curGw].nrows - 1; }
  sweepT0 = millis();
}
static void nextGateway(){
  beginHero();
  curGw = (curGw + 1) % gwCount; curRow = 0;
  sweepT0 = millis();
}

static void handleTouch(){
  if (!touchOK) return;
  int16_t x[2], y[2];
  static uint32_t lastTap = 0;
  if (!touch.getPoint(x, y, 1)) return;
  if (millis() - lastTap < 220) return;                 /* debounce */
  lastTap = millis();

  if (setupScreen){ setupScreen = false; return; }
  if (y[0] < BAND_STATUS_H)       { setupScreen = true; return; }
  if (y[0] >= BAND_FOOT_Y)        { nextGateway();      return; }
  x[0] < 120 ? prevPanel() : nextPanel();
}

static void handleButtons(){
  static uint32_t last = 0;
  static bool wasDown = false;
  bool down = (digitalRead(PIN_BTN_A) == LOW) || (digitalRead(PIN_BTN_B) == LOW);
  if (down && !wasDown && millis() - last > 220){
    last = millis();
    if (setupScreen) setupScreen = false; else nextPanel();
  }
  wasDown = down;
}

/* =========================================================================
   Lifecycle
   ========================================================================= */
void setup(){
  Serial.begin(115200);
  delay(200);
  Serial.println("\nPulsar - ESP32-S3-LCD-1.54 - offline build");

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  batteryBegin();

  if (!panel->begin()){ Serial.println("panel begin failed"); }
  panel->setTextWrap(false);
  if (!cv->begin()){
    Serial.println("canvas alloc failed - is PSRAM enabled in Tools?");
    panel->fillScreen(RGB565_BLACK);
    panel->setCursor(8, 100); panel->setTextColor(RGB565_RED); panel->setTextSize(2);
    panel->println("PSRAM OFF");
    while (true) delay(1000);
  }

  cv->setTextWrap(false);          // clients truncate, never wrap - api.md §5

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  touch.setPins(PIN_TP_RST, PIN_TP_INT);
  touchOK = touch.begin(Wire, CST816_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("touch: %s\n", touchOK ? "CST816 ok" : "not found (non-touch SKU?)");
  Serial.printf("battery: %.2f V  %d%%  %s\n",
                batteryVolts(), batteryPercent(), charging() ? "charging" : "on battery");

  if (!loadPayload()){
    panel->fillScreen(RGB565_BLACK);
    panel->setCursor(8, 100); panel->setTextColor(RGB565_RED); panel->setTextSize(2);
    panel->println("BAD PAYLOAD");
    while (true) delay(1000);
  }
  beginHero();
  sweepT0 = millis();
}

void loop(){
  handleTouch();
  handleButtons();
  render();                 /* ~25 fps; the blink and the count-up need it */
  delay(28);
}
